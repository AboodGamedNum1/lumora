// PNG decoder and encoder. Interface and rules: core/image/png.h and docs/png.md.
// The input is untrusted. Every size is checked in 64-bit arithmetic against the limits before any allocation, every
// read is bounds checked, and every loop consumes input or is bounded by checked image dimensions.
#include "png.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "deflate.h"

namespace lumora::image {
namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::array<std::uint8_t, 8> kSignature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::uint32_t kMaxChunkLength = 0x7FFFFFFFu;

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* p, std::size_t n) {
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      t[i] = c;
    }
    return t;
  }();
  for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
  return crc;
}

std::uint32_t adler32(const std::uint8_t* p, std::size_t n) {
  std::uint32_t a = 1, b = 0;
  while (n > 0) {
    const std::size_t chunk = std::min<std::size_t>(n, 5552);
    for (std::size_t i = 0; i < chunk; ++i) {
      a += p[i];
      b += a;
    }
    a %= 65521u;
    b %= 65521u;
    p += chunk;
    n -= chunk;
  }
  return (b << 16) | a;
}

std::uint32_t be32(const std::uint8_t* p) {
  return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | std::uint32_t{p[3]};
}

PngResult fail(PngStatus status, const char* message) {
  PngResult r;
  r.status = status;
  r.message = message;
  return r;
}

bool is_letter(std::uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

constexpr std::uint32_t tag(char a, char b, char c, char d) {
  return (std::uint32_t{static_cast<std::uint8_t>(a)} << 24) | (std::uint32_t{static_cast<std::uint8_t>(b)} << 16) |
         (std::uint32_t{static_cast<std::uint8_t>(c)} << 8) | std::uint32_t{static_cast<std::uint8_t>(d)};
}
constexpr std::uint32_t kIHDR = tag('I', 'H', 'D', 'R');
constexpr std::uint32_t kPLTE = tag('P', 'L', 'T', 'E');
constexpr std::uint32_t kIDAT = tag('I', 'D', 'A', 'T');
constexpr std::uint32_t kIEND = tag('I', 'E', 'N', 'D');
constexpr std::uint32_t kTRNS = tag('t', 'R', 'N', 'S');

struct Header {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  int depth = 0;
  int color_type = 0;
  bool interlaced = false;
  int channels = 1;
  int bits_per_pixel = 0;
};

bool valid_depth(int color_type, int depth) {
  switch (color_type) {
    case 0: return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
    case 2: return depth == 8 || depth == 16;
    case 3: return depth == 1 || depth == 2 || depth == 4 || depth == 8;
    case 4: return depth == 8 || depth == 16;
    case 6: return depth == 8 || depth == 16;
    default: return false;
  }
}

struct Pass {
  std::uint32_t x0, y0, dx, dy;
};
constexpr Pass kAdam7[7] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};

std::uint32_t pass_extent(std::uint32_t total, std::uint32_t start, std::uint32_t step) {
  return total > start ? (total - start + step - 1) / step : 0;
}

std::uint64_t row_bytes(std::uint32_t pixels, int bits_per_pixel) {
  return (std::uint64_t{pixels} * static_cast<std::uint64_t>(bits_per_pixel) + 7) / 8;
}

// Size of the inflated data: one filter byte plus the packed row, for every row of every non-empty pass.
std::uint64_t raw_size(const Header& h) {
  if (!h.interlaced) return std::uint64_t{h.height} * (1 + row_bytes(h.width, h.bits_per_pixel));
  std::uint64_t total = 0;
  for (const Pass& p : kAdam7) {
    const std::uint32_t pw = pass_extent(h.width, p.x0, p.dx);
    const std::uint32_t ph = pass_extent(h.height, p.y0, p.dy);
    if (pw == 0 || ph == 0) continue;
    total += std::uint64_t{ph} * (1 + row_bytes(pw, h.bits_per_pixel));
  }
  return total;
}

int paeth(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

// Reverses the row filter in place. `prev` points to the previous unfiltered row (all zero for the first row).
bool unfilter_row(std::uint8_t filter, std::uint8_t* row, const std::uint8_t* prev, std::size_t n, std::size_t bpp) {
  switch (filter) {
    case 0: return true;
    case 1:
      for (std::size_t i = bpp; i < n; ++i) row[i] = static_cast<std::uint8_t>(row[i] + row[i - bpp]);
      return true;
    case 2:
      for (std::size_t i = 0; i < n; ++i) row[i] = static_cast<std::uint8_t>(row[i] + prev[i]);
      return true;
    case 3:
      for (std::size_t i = 0; i < n; ++i) {
        const int left = i >= bpp ? row[i - bpp] : 0;
        row[i] = static_cast<std::uint8_t>(row[i] + ((left + prev[i]) >> 1));
      }
      return true;
    case 4:
      for (std::size_t i = 0; i < n; ++i) {
        const int left = i >= bpp ? row[i - bpp] : 0;
        const int up_left = i >= bpp ? prev[i - bpp] : 0;
        row[i] = static_cast<std::uint8_t>(row[i] + paeth(left, prev[i], up_left));
      }
      return true;
    default: return false;
  }
}

struct Palette {
  std::size_t count = 0;                 // entries
  std::array<std::uint8_t, 768> rgb{};   // 3 bytes per entry
  std::array<std::uint8_t, 256> alpha{};  // filled with 255, then overwritten by tRNS
};

struct Transparency {
  bool present = false;  // tRNS seen (types 0 and 2: a colour key)
  std::uint16_t key[3] = {0, 0, 0};
};

// Converts one unfiltered row of a (sub)image into RGBA pixels at out[(y*W + x0 + i*dx)*4].
// Returns false on a palette index beyond the palette.
bool expand_row(const Header& h, const Palette& pal, const Transparency& tr, const std::uint8_t* row, std::uint32_t count,
                std::uint8_t* out_row, std::uint32_t dx) {
  const std::size_t step = std::size_t{dx} * 4;
  const int depth = h.depth;
  switch (h.color_type) {
    case 0: {
      for (std::uint32_t i = 0; i < count; ++i, out_row += step) {
        std::uint32_t sample;
        std::uint8_t g;
        if (depth == 16) {
          sample = (std::uint32_t{row[i * 2]} << 8) | row[i * 2 + 1];
          g = row[i * 2];
        } else if (depth == 8) {
          sample = row[i];
          g = row[i];
        } else {
          const std::uint32_t bit = i * static_cast<std::uint32_t>(depth);
          const std::uint32_t mask = (1u << depth) - 1u;
          sample = (row[bit >> 3] >> (8 - depth - static_cast<int>(bit & 7))) & mask;
          g = static_cast<std::uint8_t>(sample * 255u / mask);
        }
        out_row[0] = out_row[1] = out_row[2] = g;
        out_row[3] = static_cast<std::uint8_t>((tr.present && sample == tr.key[0]) ? 0 : 255);
      }
      return true;
    }
    case 2: {
      for (std::uint32_t i = 0; i < count; ++i, out_row += step) {
        if (depth == 8) {
          const std::uint8_t* p = row + std::size_t{i} * 3;
          out_row[0] = p[0];
          out_row[1] = p[1];
          out_row[2] = p[2];
          out_row[3] = static_cast<std::uint8_t>((tr.present && p[0] == tr.key[0] && p[1] == tr.key[1] && p[2] == tr.key[2]) ? 0 : 255);
        } else {
          const std::uint8_t* p = row + std::size_t{i} * 6;
          const std::uint16_t r = static_cast<std::uint16_t>((p[0] << 8) | p[1]);
          const std::uint16_t g = static_cast<std::uint16_t>((p[2] << 8) | p[3]);
          const std::uint16_t b = static_cast<std::uint16_t>((p[4] << 8) | p[5]);
          out_row[0] = p[0];
          out_row[1] = p[2];
          out_row[2] = p[4];
          out_row[3] = static_cast<std::uint8_t>((tr.present && r == tr.key[0] && g == tr.key[1] && b == tr.key[2]) ? 0 : 255);
        }
      }
      return true;
    }
    case 3: {
      for (std::uint32_t i = 0; i < count; ++i, out_row += step) {
        std::uint32_t idx;
        if (depth == 8) {
          idx = row[i];
        } else {
          const std::uint32_t bit = i * static_cast<std::uint32_t>(depth);
          const std::uint32_t mask = (1u << depth) - 1u;
          idx = (row[bit >> 3] >> (8 - depth - static_cast<int>(bit & 7))) & mask;
        }
        if (idx >= pal.count) return false;
        out_row[0] = pal.rgb[idx * 3];
        out_row[1] = pal.rgb[idx * 3 + 1];
        out_row[2] = pal.rgb[idx * 3 + 2];
        out_row[3] = pal.alpha[idx];
      }
      return true;
    }
    case 4: {
      const std::size_t px = depth == 16 ? 4 : 2;
      const std::size_t second = depth == 16 ? 2 : 1;
      for (std::uint32_t i = 0; i < count; ++i, out_row += step) {
        const std::uint8_t* p = row + std::size_t{i} * px;
        out_row[0] = out_row[1] = out_row[2] = p[0];
        out_row[3] = p[second];
      }
      return true;
    }
    default: {  // 6
      const std::size_t px = depth == 16 ? 8 : 4;
      const std::size_t inc = depth == 16 ? 2 : 1;
      for (std::uint32_t i = 0; i < count; ++i, out_row += step) {
        const std::uint8_t* p = row + std::size_t{i} * px;
        out_row[0] = p[0];
        out_row[1] = p[inc];
        out_row[2] = p[inc * 2];
        out_row[3] = p[inc * 3];
      }
      return true;
    }
  }
}

}  // namespace

PngResult decode_png(const std::uint8_t* data, std::size_t size, const PngLimits& limits) {
  if (data == nullptr || size == 0) return fail(PngStatus::NotPng, "empty input");
  {
    const std::size_t n = std::min<std::size_t>(size, kSignature.size());
    if (std::memcmp(data, kSignature.data(), n) != 0) return fail(PngStatus::NotPng, "bad signature");
    if (size < kSignature.size()) return fail(PngStatus::Truncated, "data ends inside the signature");
  }

  Header hdr;
  Palette pal;
  pal.alpha.fill(255);
  Transparency trns;
  bool have_ihdr = false, have_plte = false, have_trns = false, seen_idat = false, idat_closed = false, have_iend = false;
  Bytes idat;
  std::size_t pos = kSignature.size();

  while (!have_iend) {
    if (size - pos < 8) return fail(PngStatus::Truncated, "data ends inside a chunk header");
    const std::uint32_t length = be32(data + pos);
    const std::uint32_t type = be32(data + pos + 4);
    const bool ancillary = (data[pos + 4] & 0x20) != 0;
    if (!is_letter(data[pos + 4]) || !is_letter(data[pos + 5]) || !is_letter(data[pos + 6]) || !is_letter(data[pos + 7]))
      return fail(PngStatus::BadChunk, "chunk type is not four letters");
    if (length > kMaxChunkLength) return fail(PngStatus::BadChunk, "chunk length above 2^31-1");
    if (size - pos - 8 < std::uint64_t{length} + 4) return fail(PngStatus::Truncated, "data ends inside a chunk");
    const std::uint8_t* body = data + pos + 8;
    const std::uint32_t crc_stored = be32(body + length);
    const std::uint32_t crc = crc32_update(crc32_update(0xFFFFFFFFu, data + pos + 4, 4), body, length) ^ 0xFFFFFFFFu;
    if (crc != crc_stored) return fail(PngStatus::BadCrc, "chunk CRC-32 mismatch");
    pos += std::size_t{length} + 12;

    if (!have_ihdr && type != kIHDR) return fail(PngStatus::BadChunk, "IHDR is not the first chunk");
    if (seen_idat && type != kIDAT) idat_closed = true;

    if (type == kIHDR) {
      if (have_ihdr) return fail(PngStatus::BadChunk, "duplicate IHDR");
      if (length != 13) return fail(PngStatus::BadChunk, "IHDR length is not 13");
      have_ihdr = true;
      hdr.width = be32(body);
      hdr.height = be32(body + 4);
      hdr.depth = body[8];
      hdr.color_type = body[9];
      if (hdr.width == 0 || hdr.height == 0 || hdr.width > kMaxChunkLength || hdr.height > kMaxChunkLength)
        return fail(PngStatus::BadHeader, "zero or invalid image size");
      if (!valid_depth(hdr.color_type, hdr.depth)) return fail(PngStatus::BadHeader, "bad colour type or bit depth");
      if (body[10] != 0) return fail(PngStatus::BadHeader, "bad compression method");
      if (body[11] != 0) return fail(PngStatus::BadHeader, "bad filter method");
      if (body[12] > 1) return fail(PngStatus::BadHeader, "bad interlace method");
      hdr.interlaced = body[12] == 1;
      hdr.channels = hdr.color_type == 2 ? 3 : hdr.color_type == 4 ? 2 : hdr.color_type == 6 ? 4 : 1;
      hdr.bits_per_pixel = hdr.channels * hdr.depth;
      // Limits first, from the header values only, before anything is allocated.
      const std::uint64_t pixels = std::uint64_t{hdr.width} * hdr.height;
      if (hdr.width > limits.max_dim || hdr.height > limits.max_dim || pixels > limits.max_pixels)
        return fail(PngStatus::TooLarge, "image size above the limits");
      if (pixels > (std::numeric_limits<std::size_t>::max() / 2) / 16)
        return fail(PngStatus::TooLarge, "image size above what this platform can address");
    } else if (type == kPLTE) {
      if (have_plte) return fail(PngStatus::BadChunk, "duplicate PLTE");
      if (seen_idat) return fail(PngStatus::BadChunk, "PLTE after IDAT");
      if (have_trns) return fail(PngStatus::BadChunk, "PLTE after tRNS");
      if (hdr.color_type == 0 || hdr.color_type == 4) return fail(PngStatus::BadChunk, "PLTE in a greyscale image");
      if (length == 0 || length % 3 != 0 || length > 768) return fail(PngStatus::BadPalette, "bad PLTE length");
      const std::size_t entries = length / 3;
      if (hdr.color_type == 3 && entries > (std::size_t{1} << hdr.depth))
        return fail(PngStatus::BadPalette, "more PLTE entries than the bit depth allows");
      have_plte = true;
      pal.count = entries;
      std::memcpy(pal.rgb.data(), body, length);
    } else if (type == kTRNS) {
      if (have_trns) return fail(PngStatus::BadChunk, "duplicate tRNS");
      if (seen_idat) return fail(PngStatus::BadChunk, "tRNS after IDAT");
      if (hdr.color_type == 4 || hdr.color_type == 6) return fail(PngStatus::BadChunk, "tRNS with an alpha channel");
      have_trns = true;
      if (hdr.color_type == 0) {
        if (length != 2) return fail(PngStatus::BadPalette, "bad tRNS length");
        trns.present = true;
        trns.key[0] = static_cast<std::uint16_t>((body[0] << 8) | body[1]);
      } else if (hdr.color_type == 2) {
        if (length != 6) return fail(PngStatus::BadPalette, "bad tRNS length");
        trns.present = true;
        for (int i = 0; i < 3; ++i) trns.key[i] = static_cast<std::uint16_t>((body[i * 2] << 8) | body[i * 2 + 1]);
      } else {
        if (!have_plte) return fail(PngStatus::BadChunk, "tRNS before PLTE");
        if (length > pal.count) return fail(PngStatus::BadPalette, "bad tRNS length");
        for (std::uint32_t i = 0; i < length; ++i) pal.alpha[i] = body[i];
      }
    } else if (type == kIDAT) {
      if (idat_closed) return fail(PngStatus::BadChunk, "IDAT chunks are not contiguous");
      if (!seen_idat && hdr.color_type == 3 && !have_plte) return fail(PngStatus::BadPalette, "missing PLTE");
      seen_idat = true;
      if (length > 0) {
        if (idat.size() + length > size) return fail(PngStatus::BadChunk, "IDAT data above the file size");
        idat.insert(idat.end(), body, body + length);
      }
    } else if (type == kIEND) {
      if (length != 0) return fail(PngStatus::BadChunk, "IEND length is not zero");
      if (!seen_idat) return fail(PngStatus::BadChunk, "no IDAT before IEND");
      have_iend = true;
    } else if (!ancillary) {
      return fail(PngStatus::BadChunk, "unknown critical chunk");
    }
    // Other ancillary chunks (gAMA, iCCP, sRGB, cHRM, acTL, fcTL, fdAT, text...) are CRC-checked and ignored.
  }
  if (pos != size) return fail(PngStatus::BadChunk, "bytes after IEND");

  // zlib framing: CMF, FLG, DEFLATE stream, Adler-32 (big endian) at the very end of the IDAT data.
  if (idat.size() < 2 + 4) return fail(PngStatus::BadZlib, "zlib stream too short");
  const unsigned cmf = idat[0], flg = idat[1];
  if ((cmf & 0x0Fu) != 8) return fail(PngStatus::BadZlib, "zlib compression method is not deflate");
  if ((cmf >> 4) > 7) return fail(PngStatus::BadZlib, "zlib window above 32 KB");
  if (((cmf << 8) | flg) % 31 != 0) return fail(PngStatus::BadZlib, "zlib header check bits");
  if (flg & 0x20u) return fail(PngStatus::BadZlib, "zlib preset dictionary");

  const std::uint64_t expected = raw_size(hdr);
  auto inflated = package::inflate_raw(idat.data() + 2, idat.size() - 2, expected);
  if (inflated.status == package::InflateStatus::OutputLimit)
    return fail(PngStatus::BadFilter, "inflated data is larger than the image needs");
  if (inflated.status != package::InflateStatus::Ok) return fail(PngStatus::BadZlib, "malformed DEFLATE stream");
  if (inflated.consumed != idat.size() - 2 - 4) return fail(PngStatus::BadZlib, "DEFLATE stream does not end at the Adler-32");
  if (adler32(inflated.data.data(), inflated.data.size()) != be32(idat.data() + idat.size() - 4))
    return fail(PngStatus::BadZlib, "Adler-32 mismatch");
  if (inflated.data.size() != expected) return fail(PngStatus::BadFilter, "inflated size does not match the image");
  idat.clear();
  idat.shrink_to_fit();

  PngResult result;
  result.image.width = hdr.width;
  result.image.height = hdr.height;
  result.image.rgba.assign(std::size_t{hdr.width} * hdr.height * 4, 0);
  std::uint8_t* out = result.image.rgba.data();
  std::uint8_t* raw = inflated.data.data();
  const std::size_t bpp = static_cast<std::size_t>(std::max(1, hdr.bits_per_pixel / 8));

  std::size_t offset = 0;
  const int pass_count = hdr.interlaced ? 7 : 1;
  for (int pi = 0; pi < pass_count; ++pi) {
    const Pass pass = hdr.interlaced ? kAdam7[pi] : Pass{0, 0, 1, 1};
    const std::uint32_t pw = pass_extent(hdr.width, pass.x0, pass.dx);
    const std::uint32_t ph = pass_extent(hdr.height, pass.y0, pass.dy);
    if (pw == 0 || ph == 0) continue;
    const std::size_t rb = static_cast<std::size_t>(row_bytes(pw, hdr.bits_per_pixel));
    const Bytes zero(rb, 0);
    const std::uint8_t* prev = zero.data();
    for (std::uint32_t y = 0; y < ph; ++y) {
      std::uint8_t* line = raw + offset;
      std::uint8_t* row = line + 1;
      if (!unfilter_row(line[0], row, prev, rb, bpp)) return fail(PngStatus::BadFilter, "filter type above 4");
      const std::size_t oy = std::size_t{pass.y0} + std::size_t{y} * pass.dy;
      std::uint8_t* out_row = out + (oy * hdr.width + pass.x0) * 4;
      if (!expand_row(hdr, pal, trns, row, pw, out_row, pass.dx)) return fail(PngStatus::BadPalette, "palette index out of range");
      prev = row;
      offset += 1 + rb;
    }
  }
  result.status = PngStatus::Ok;
  result.message = "ok";
  return result;
}

namespace {

void put32(Bytes& b, std::uint32_t v) {
  b.push_back(static_cast<std::uint8_t>(v >> 24));
  b.push_back(static_cast<std::uint8_t>(v >> 16));
  b.push_back(static_cast<std::uint8_t>(v >> 8));
  b.push_back(static_cast<std::uint8_t>(v));
}

void put_chunk(Bytes& out, const char* type, const std::uint8_t* body, std::size_t n) {
  put32(out, static_cast<std::uint32_t>(n));
  const std::size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  if (n > 0) out.insert(out.end(), body, body + n);
  const std::uint32_t crc = crc32_update(0xFFFFFFFFu, out.data() + start, out.size() - start) ^ 0xFFFFFFFFu;
  put32(out, crc);
}

}  // namespace

std::vector<std::uint8_t> encode_png(const Rgba8& image, int level) {
  if (!image.valid() || image.width > kMaxChunkLength || image.height > kMaxChunkLength) return {};
  const std::size_t w = image.width, h = image.height;
  const std::size_t stride = w * 4;
  if (h > (std::numeric_limits<std::size_t>::max() / 2) / (stride + 1)) return {};
  level = std::max(0, std::min(9, level));

  Bytes raw(h * (stride + 1));
  const Bytes zero(stride, 0);
  std::array<Bytes, 5> candidate;
  for (auto& c : candidate) c.resize(stride);
  for (std::size_t y = 0; y < h; ++y) {
    const std::uint8_t* cur = image.rgba.data() + y * stride;
    const std::uint8_t* up = y > 0 ? cur - stride : zero.data();
    std::array<std::uint64_t, 5> cost{};
    for (std::size_t i = 0; i < stride; ++i) {
      const int a = i >= 4 ? cur[i - 4] : 0;
      const int b = up[i];
      const int c = i >= 4 ? up[i - 4] : 0;
      const int x = cur[i];
      const std::uint8_t f[5] = {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(x - a),
                                 static_cast<std::uint8_t>(x - b), static_cast<std::uint8_t>(x - ((a + b) >> 1)),
                                 static_cast<std::uint8_t>(x - paeth(a, b, c))};
      for (int k = 0; k < 5; ++k) {
        candidate[static_cast<std::size_t>(k)][i] = f[k];
        const int s = static_cast<std::int8_t>(f[k]);
        cost[static_cast<std::size_t>(k)] += static_cast<std::uint64_t>(s < 0 ? -s : s);
      }
    }
    std::size_t best = 0;
    for (std::size_t k = 1; k < 5; ++k)
      if (cost[k] < cost[best]) best = k;  // ties keep the lowest filter number
    std::uint8_t* line = raw.data() + y * (stride + 1);
    line[0] = static_cast<std::uint8_t>(best);
    std::memcpy(line + 1, candidate[best].data(), stride);
  }

  const Bytes packed = package::deflate_raw(raw.data(), raw.size(), level);
  Bytes z;
  z.reserve(packed.size() + 6);
  const unsigned flevel = level < 2 ? 0u : level < 6 ? 1u : level == 6 ? 2u : 3u;
  const unsigned cmf = 0x78;
  unsigned flg = flevel << 6;
  flg += 31 - ((cmf << 8) + flg) % 31;
  z.push_back(static_cast<std::uint8_t>(cmf));
  z.push_back(static_cast<std::uint8_t>(flg));
  z.insert(z.end(), packed.begin(), packed.end());
  put32(z, adler32(raw.data(), raw.size()));

  Bytes out(kSignature.begin(), kSignature.end());
  Bytes ihdr;
  put32(ihdr, image.width);
  put32(ihdr, image.height);
  ihdr.push_back(8);  // depth
  ihdr.push_back(6);  // RGBA
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  put_chunk(out, "IHDR", ihdr.data(), ihdr.size());
  put_chunk(out, "IDAT", z.data(), z.size());
  put_chunk(out, "IEND", nullptr, 0);
  return out;
}

}  // namespace lumora::image
