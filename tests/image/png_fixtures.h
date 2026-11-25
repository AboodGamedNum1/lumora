// Test-only PNG writer and reference model for png_tests.cpp. Written for this repository: stored (uncompressed)
// DEFLATE, its own CRC-32 and Adler-32, its own forward row filters, and its own expected-RGBA computation, so the
// decoder is checked against code that shares nothing with it. No third-party PNG files are used.
#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace png_fixtures {

using Bytes = std::vector<std::uint8_t>;

inline std::uint32_t crc32(const std::uint8_t* p, std::size_t n) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    for (int k = 0; k < 8; ++k) crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
  }
  return ~crc;
}

inline std::uint32_t adler32(const Bytes& d) {
  std::uint32_t a = 1, b = 0;
  for (std::uint8_t v : d) {
    a = (a + v) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

inline void put32(Bytes& b, std::uint32_t v) {
  b.push_back(static_cast<std::uint8_t>(v >> 24));
  b.push_back(static_cast<std::uint8_t>(v >> 16));
  b.push_back(static_cast<std::uint8_t>(v >> 8));
  b.push_back(static_cast<std::uint8_t>(v));
}

inline Bytes signature() { return {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; }

// A chunk with a correct length and CRC.
inline Bytes chunk(const std::string& type, const Bytes& body = {}) {
  Bytes out;
  put32(out, static_cast<std::uint32_t>(body.size()));
  out.insert(out.end(), type.begin(), type.end());
  out.insert(out.end(), body.begin(), body.end());
  put32(out, crc32(out.data() + 4, out.size() - 4));
  return out;
}

inline void append(Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); }

// zlib stream with stored DEFLATE blocks.
inline Bytes zlib_stored(const Bytes& raw) {
  Bytes z = {0x78, 0x01};
  std::size_t pos = 0;
  do {
    const std::size_t n = std::min<std::size_t>(raw.size() - pos, 65535);
    const bool last = pos + n >= raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(static_cast<std::uint8_t>(n & 0xFF));
    z.push_back(static_cast<std::uint8_t>(n >> 8));
    z.push_back(static_cast<std::uint8_t>(~n & 0xFF));
    z.push_back(static_cast<std::uint8_t>((~n >> 8) & 0xFF));
    z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
    pos += n;
  } while (pos < raw.size());
  put32(z, adler32(raw));
  return z;
}

inline Bytes ihdr_body(std::uint32_t w, std::uint32_t h, int depth, int ctype, int comp = 0, int filter = 0, int interlace = 0) {
  Bytes b;
  put32(b, w);
  put32(b, h);
  b.push_back(static_cast<std::uint8_t>(depth));
  b.push_back(static_cast<std::uint8_t>(ctype));
  b.push_back(static_cast<std::uint8_t>(comp));
  b.push_back(static_cast<std::uint8_t>(filter));
  b.push_back(static_cast<std::uint8_t>(interlace));
  return b;
}

struct Pass {
  std::uint32_t x0, y0, dx, dy;
};
inline std::vector<Pass> passes(bool interlaced) {
  if (!interlaced) return {{0, 0, 1, 1}};
  return {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
}

struct Rgba {
  std::uint32_t w = 0, h = 0;
  Bytes px;
};

struct Spec {
  std::uint32_t w = 1, h = 1;
  int ctype = 6, depth = 8;
  bool interlaced = false;
  std::vector<std::uint16_t> samples;  // w*h*channels, row major, one value per channel (palette index for type 3)
  Bytes plte;                          // RGB triples (type 3, optionally 2 and 6)
  Bytes trns;                          // raw tRNS body, empty for none
  int filter = 5;                      // 0..4 fixed filter for every row, 5 = cycle through 0..4 by row number
};

inline int channels(int ctype) { return ctype == 2 ? 3 : ctype == 4 ? 2 : ctype == 6 ? 4 : 1; }

// Deterministic pseudo random samples that fit the depth (palette indices below `palette_entries` for type 3).
inline std::vector<std::uint16_t> make_samples(const Spec& s, std::uint32_t seed, std::size_t palette_entries = 0) {
  std::vector<std::uint16_t> v(std::size_t{s.w} * s.h * static_cast<std::size_t>(channels(s.ctype)));
  std::uint32_t x = seed * 2654435761u + 12345u;
  for (auto& e : v) {
    x = x * 1664525u + 1013904223u;
    const std::uint32_t r = x >> 8;
    if (s.ctype == 3) {
      e = static_cast<std::uint16_t>(r % palette_entries);
    } else {
      e = static_cast<std::uint16_t>(r & ((1u << s.depth) - 1u));
    }
  }
  return v;
}

inline Bytes make_palette(std::size_t entries, std::uint32_t seed) {
  Bytes p(entries * 3);
  std::uint32_t x = seed * 22695477u + 1u;
  for (auto& b : p) {
    x = x * 1664525u + 1013904223u;
    b = static_cast<std::uint8_t>(x >> 24);
  }
  return p;
}

inline Bytes pack_row(const Spec& s, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& pixels) {
  const int nch = channels(s.ctype);
  const std::size_t bits = pixels.size() * static_cast<std::size_t>(nch * s.depth);
  Bytes row((bits + 7) / 8, 0);
  std::size_t bitpos = 0;
  for (const auto& xy : pixels) {
    const std::size_t base = (std::size_t{xy.second} * s.w + xy.first) * static_cast<std::size_t>(nch);
    for (int c = 0; c < nch; ++c) {
      const std::uint16_t v = s.samples[base + static_cast<std::size_t>(c)];
      if (s.depth == 16) {
        row[bitpos / 8] = static_cast<std::uint8_t>(v >> 8);
        row[bitpos / 8 + 1] = static_cast<std::uint8_t>(v & 0xFF);
      } else if (s.depth == 8) {
        row[bitpos / 8] = static_cast<std::uint8_t>(v);
      } else {
        row[bitpos / 8] = static_cast<std::uint8_t>(row[bitpos / 8] | (v << (8 - s.depth - static_cast<int>(bitpos % 8))));
      }
      bitpos += static_cast<std::size_t>(s.depth);
    }
  }
  return row;
}

inline int paeth(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

inline Bytes filter_row(int type, const Bytes& cur, const Bytes& prev, std::size_t bpp) {
  Bytes out(cur.size() + 1);
  out[0] = static_cast<std::uint8_t>(type);
  for (std::size_t i = 0; i < cur.size(); ++i) {
    const int a = i >= bpp ? cur[i - bpp] : 0;
    const int b = prev[i];
    const int c = i >= bpp ? prev[i - bpp] : 0;
    int pred = 0;
    switch (type) {
      case 1: pred = a; break;
      case 2: pred = b; break;
      case 3: pred = (a + b) / 2; break;
      case 4: pred = paeth(a, b, c); break;
      default: break;
    }
    out[i + 1] = static_cast<std::uint8_t>(cur[i] - pred);
  }
  return out;
}

// Raw (inflated) image data: filtered rows of every pass.
inline Bytes raw_data(const Spec& s) {
  Bytes raw;
  const std::size_t bpp = static_cast<std::size_t>(std::max(1, channels(s.ctype) * s.depth / 8));
  int row_number = 0;
  for (const Pass& p : passes(s.interlaced)) {
    std::vector<std::uint32_t> xs, ys;
    for (std::uint32_t x = p.x0; x < s.w; x += p.dx) xs.push_back(x);
    for (std::uint32_t y = p.y0; y < s.h; y += p.dy) ys.push_back(y);
    if (xs.empty() || ys.empty()) continue;
    Bytes prev;
    for (std::uint32_t y : ys) {
      std::vector<std::pair<std::uint32_t, std::uint32_t>> pixels;
      for (std::uint32_t x : xs) pixels.emplace_back(x, y);
      const Bytes cur = pack_row(s, pixels);
      if (prev.empty()) prev.assign(cur.size(), 0);
      const int type = s.filter == 5 ? row_number % 5 : s.filter;
      append(raw, filter_row(type, cur, prev, bpp));
      prev = cur;
      ++row_number;
    }
  }
  return raw;
}

// A complete PNG: IHDR, optional PLTE, optional tRNS, one IDAT (stored DEFLATE), IEND.
inline Bytes build(const Spec& s) {
  Bytes out = signature();
  append(out, chunk("IHDR", ihdr_body(s.w, s.h, s.depth, s.ctype, 0, 0, s.interlaced ? 1 : 0)));
  if (!s.plte.empty()) append(out, chunk("PLTE", s.plte));
  if (!s.trns.empty()) append(out, chunk("tRNS", s.trns));
  append(out, chunk("IDAT", zlib_stored(raw_data(s))));
  append(out, chunk("IEND"));
  return out;
}

// Expected RGBA computed straight from the samples.
inline Bytes expected_rgba(const Spec& s) {
  const int nch = channels(s.ctype);
  const std::uint32_t maxv = (1u << s.depth) - 1u;
  Bytes out(std::size_t{s.w} * s.h * 4);
  auto narrow = [&](std::uint32_t v) -> std::uint8_t {
    return static_cast<std::uint8_t>(s.depth == 16 ? (v >> 8) : s.depth == 8 ? v : v * 255u / maxv);
  };
  for (std::size_t i = 0; i < std::size_t{s.w} * s.h; ++i) {
    const std::uint16_t* v = &s.samples[i * static_cast<std::size_t>(nch)];
    std::uint8_t* o = &out[i * 4];
    switch (s.ctype) {
      case 0: {
        o[0] = o[1] = o[2] = narrow(v[0]);
        const bool keyed = s.trns.size() == 2 && v[0] == ((s.trns[0] << 8) | s.trns[1]);
        o[3] = keyed ? 0 : 255;
        break;
      }
      case 2: {
        o[0] = narrow(v[0]);
        o[1] = narrow(v[1]);
        o[2] = narrow(v[2]);
        const bool keyed = s.trns.size() == 6 && v[0] == ((s.trns[0] << 8) | s.trns[1]) &&
                           v[1] == ((s.trns[2] << 8) | s.trns[3]) && v[2] == ((s.trns[4] << 8) | s.trns[5]);
        o[3] = keyed ? 0 : 255;
        break;
      }
      case 3:
        o[0] = s.plte[v[0] * 3];
        o[1] = s.plte[v[0] * 3 + 1];
        o[2] = s.plte[v[0] * 3 + 2];
        o[3] = v[0] < s.trns.size() ? s.trns[v[0]] : 255;
        break;
      case 4:
        o[0] = o[1] = o[2] = narrow(v[0]);
        o[3] = narrow(v[1]);
        break;
      default:
        o[0] = narrow(v[0]);
        o[1] = narrow(v[1]);
        o[2] = narrow(v[2]);
        o[3] = narrow(v[3]);
        break;
    }
  }
  return out;
}

// Byte offsets of the chunks of a well-formed file: {offset of the length field, total chunk size}.
inline std::vector<std::pair<std::size_t, std::size_t>> chunk_spans(const Bytes& png) {
  std::vector<std::pair<std::size_t, std::size_t>> v;
  std::size_t pos = 8;
  while (pos + 12 <= png.size()) {
    const std::size_t len = (std::size_t{png[pos]} << 24) | (std::size_t{png[pos + 1]} << 16) | (std::size_t{png[pos + 2]} << 8) | png[pos + 3];
    v.emplace_back(pos, len + 12);
    pos += len + 12;
  }
  return v;
}

}  // namespace png_fixtures
