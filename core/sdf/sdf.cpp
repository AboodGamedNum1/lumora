// Lumora CPU signed distance fields (scope S). Integer arithmetic only, no heap, no static mutable state.
// Every rule is defined in docs/sdf.md; the tests compare against an independent brute-force reference.
#include "sdf.h"

namespace lumora::sdf {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr u64 kBasis = 0xcbf29ce484222325ull;
constexpr u32 kNone = kNoSet;  // internal "no set pixel seen" marker, same value as the public one

// Overlap rule shared with core/composite and core/resample. See docs/sdf.md.
bool rows_overlap(const void* a, u64 a_stride, u64 a_len, u64 a_rows, const void* b, u64 b_stride, u64 b_len, u64 b_rows) {
  const u64 kTagMask = 0x00FFFFFFFFFFFFFFull;
  const u64 ua = static_cast<u64>(reinterpret_cast<std::uintptr_t>(a)) & kTagMask;
  const u64 ub = static_cast<u64>(reinterpret_cast<std::uintptr_t>(b)) & kTagMask;
  const u64 diff = ua - ub;  // modulo 2^64
  const bool negative = diff >= (static_cast<u64>(1) << 63);
  const u64 absd = negative ? (static_cast<u64>(0) - diff) : diff;  // |delta| as unsigned, no signed negation
  if (absd >= (static_cast<u64>(1) << 47)) return false;
  const i64 delta = negative ? -static_cast<i64>(absd) : static_cast<i64>(absd);  // A relative to B
  const i64 bs = static_cast<i64>(b_stride);
  for (u64 i = 0; i < a_rows; ++i) {
    const i64 t = delta + static_cast<i64>(i) * static_cast<i64>(a_stride);  // start of A row i relative to B base
    // B row j overlaps iff j * bs > t - b_len and j * bs < t + a_len.
    const i64 low = t - static_cast<i64>(b_len);
    i64 j = 0;
    if (low >= 0) j = low / bs + 1;
    if (j < static_cast<i64>(b_rows) && j * bs < t + static_cast<i64>(a_len)) return true;
  }
  return false;
}

bool size_ok(u32 w, u32 h) { return w != 0u && h != 0u && w <= kMaxSize && h <= kMaxSize; }

// Exact squared distance transform. A pixel is set when (mask >= threshold) != invert. out holds width * height values,
// tightly packed. Returns nothing; kNoSet everywhere when no pixel is set.
void edt(const std::uint8_t* mask, u32 w, u32 h, u32 stride, std::uint8_t threshold, bool invert, u32* out) {
  // Column pass: exact distance (in pixels) to the nearest set pixel in the same column, kNone if the column has none.
  for (u32 x = 0; x < w; ++x) {
    u32 last = kNone;  // row of the last set pixel above (or at) the current row
    for (u32 y = 0; y < h; ++y) {
      const bool set = ((mask[static_cast<u64>(y) * stride + x] >= threshold) != invert);
      if (set) last = y;
      out[static_cast<u64>(y) * w + x] = (last == kNone) ? kNone : y - last;
    }
    u32 next = kNone;  // row of the nearest set pixel below (or at) the current row
    for (u32 y = h; y-- > 0;) {
      const bool set = ((mask[static_cast<u64>(y) * stride + x] >= threshold) != invert);
      if (set) next = y;
      if (next != kNone) {
        const u32 d = next - y;
        u32& o = out[static_cast<u64>(y) * w + x];
        if (o == kNone || d < o) o = d;
      }
    }
  }
  // Row pass: lower envelope of the parabolas f(x') + (x - x')^2 over the columns with a finite f.
  u32 f[kMaxSize];
  std::uint16_t v[kMaxSize];
  for (u32 y = 0; y < h; ++y) {
    u32* row = out + static_cast<u64>(y) * w;
    for (u32 x = 0; x < w; ++x) f[x] = row[x] == kNone ? kNone : row[x] * row[x];
    u32 n = 0;
    for (u32 q = 0; q < w; ++q) {
      if (f[q] == kNone) continue;
      while (n >= 2) {
        // Pop v[n-1] when the intersection of (v[n-1], q) is at or before the intersection of (v[n-2], v[n-1]).
        const i64 a = v[n - 2], b = v[n - 1], c = q;
        const i64 num1 = (static_cast<i64>(f[q]) + c * c) - (static_cast<i64>(f[b]) + b * b);
        const i64 den1 = 2 * (c - b);
        const i64 num2 = (static_cast<i64>(f[b]) + b * b) - (static_cast<i64>(f[a]) + a * a);
        const i64 den2 = 2 * (b - a);
        if (num1 * den2 <= num2 * den1) --n;
        else break;
      }
      v[n++] = static_cast<std::uint16_t>(q);
    }
    if (n == 0) {
      for (u32 x = 0; x < w; ++x) row[x] = kNoSet;
      continue;
    }
    u32 k = 0;
    for (u32 x = 0; x < w; ++x) {
      for (;;) {
        if (k + 1 >= n) break;
        const i64 d0 = static_cast<i64>(x) - v[k], d1 = static_cast<i64>(x) - v[k + 1];
        if (static_cast<i64>(f[v[k + 1]]) + d1 * d1 <= static_cast<i64>(f[v[k]]) + d0 * d0) ++k;
        else break;
      }
      const i64 d = static_cast<i64>(x) - v[k];
      row[x] = static_cast<u32>(static_cast<i64>(f[v[k]]) + d * d);
    }
  }
}

// Encoded byte for the signed distance s = sign * q (Q8) with spread * 256 = den.
inline std::uint8_t encode(u64 q, bool inside, u64 den) {
  const u64 r = (2u * q * 127u + den) / (2u * den);  // round(q * 127 / den), ties away from zero
  i64 v = 128;
  v += inside ? static_cast<i64>(r) : -static_cast<i64>(r);
  if (v < 1) v = 1;
  if (v > 255) v = 255;
  return static_cast<std::uint8_t>(v);
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadSize: return "bad size";
    case Status::BadSpread: return "bad spread";
    case Status::ScratchTooSmall: return "scratch too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

Status distance_squared(const std::uint8_t* mask, std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                        std::uint8_t threshold, std::uint32_t* out) {
  if (mask == nullptr || out == nullptr) return Status::NullBuffer;
  if (!size_ok(width, height) || stride < width) return Status::BadSize;
  // The output extent is the contiguous width * height * 4 bytes; the mask rows are compared against it.
  const u64 out_bytes = static_cast<u64>(width) * height * 4u;
  if (rows_overlap(out, out_bytes, out_bytes, 1, mask, stride, width, height)) return Status::Overlap;
  edt(mask, width, height, stride, threshold, false, out);
  return Status::Ok;
}

std::uint32_t isqrt(std::uint64_t v) {
  u64 r = 0;
  u64 bit = static_cast<u64>(1) << 62;
  while (bit > v) bit >>= 2;
  while (bit != 0) {
    if (v >= r + bit) {
      v -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<u32>(r);
}

Status make_sdf(const std::uint8_t* mask, std::uint32_t width, std::uint32_t height, std::uint32_t mask_stride,
                std::uint8_t* out, std::uint32_t out_stride, std::uint32_t spread, std::uint32_t* scratch,
                std::size_t scratch_count) {
  if (mask == nullptr || out == nullptr || scratch == nullptr) return Status::NullBuffer;
  if (!size_ok(width, height) || mask_stride < width || out_stride < width) return Status::BadSize;
  if (spread == 0u || spread > kMaxSpread) return Status::BadSpread;
  const u64 pixels = static_cast<u64>(width) * height;
  if (static_cast<u64>(scratch_count) < pixels) return Status::ScratchTooSmall;
  if (rows_overlap(out, out_stride, width, height, mask, mask_stride, width, height) ||
      rows_overlap(scratch, pixels * 4u, pixels * 4u, 1, mask, mask_stride, width, height) ||
      rows_overlap(scratch, pixels * 4u, pixels * 4u, 1, out, out_stride, width, height)) {
    return Status::Overlap;
  }
  const u64 den = static_cast<u64>(spread) * 256u;
  // Pass 1: distance to the nearest inside pixel, used for the outside pixels.
  edt(mask, width, height, mask_stride, 128, false, scratch);
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      if (mask[static_cast<u64>(y) * mask_stride + x] >= 128) continue;
      const u32 d2 = scratch[static_cast<u64>(y) * width + x];
      const u64 q = d2 == kNoSet ? den : isqrt(static_cast<u64>(d2) * 65536u);
      out[static_cast<u64>(y) * out_stride + x] = encode(q, false, den);
    }
  }
  // Pass 2: distance to the nearest outside pixel, used for the inside pixels.
  edt(mask, width, height, mask_stride, 128, true, scratch);
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      if (mask[static_cast<u64>(y) * mask_stride + x] < 128) continue;
      const u32 d2 = scratch[static_cast<u64>(y) * width + x];
      const u64 q = d2 == kNoSet ? den : isqrt(static_cast<u64>(d2) * 65536u);
      out[static_cast<u64>(y) * out_stride + x] = encode(q, true, den);
    }
  }
  return Status::Ok;
}

std::uint8_t edge_coverage(std::uint8_t v, std::uint32_t aa) {
  if (aa < 1u || aa > 127u) return v < 128u ? 0 : 255;
  const i64 t = (static_cast<i64>(v) - 128) * 255 + static_cast<i64>(aa) * 255;
  const i64 d = 2 * static_cast<i64>(aa);
  i64 c = t / d;
  if (t % d != 0 && t < 0) --c;  // floor division for negative t
  if (c < 0) c = 0;
  if (c > 255) c = 255;
  return static_cast<std::uint8_t>(c);
}

std::uint64_t digest_image(const std::uint8_t* data, std::uint32_t width, std::uint32_t height, std::uint32_t stride) {
  u64 h = kBasis;
  if (data == nullptr || width == 0u || height == 0u) return h;
  for (u32 j = 0; j < height; ++j) {
    const std::uint8_t* row = data + static_cast<u64>(j) * stride;
    for (u32 i = 0; i < width; ++i) h = (h ^ row[i]) * 0x100000001b3ull;
  }
  return h;
}

}  // namespace lumora::sdf
