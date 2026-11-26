// CPU compositing and blur (scope P). Integer arithmetic only. The exact rules are in docs/composite.md.
#include "composite.h"

namespace lumora::composite {

namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

inline u32 d255(u32 v) { return div255(v); }
inline u32 clamp255(u32 v) { return v > 255u ? 255u : v; }

bool mode_ok(BlendMode m) { return static_cast<unsigned>(m) <= static_cast<unsigned>(BlendMode::Screen); }

// One channel of the mode formula. s and d are the (scaled) source and the destination channel value, sa and da the alpha
// values. Every product of two byte values gets exactly one div255, terms are summed, the result is clamped to 0..255.
// The same function is used for the alpha channel with s = Sa and d = Da, except for Multiply and Screen (see pd_alpha).
u32 channel(BlendMode m, u32 s, u32 d, u32 sa, u32 da) {
  switch (m) {
    case BlendMode::Src: return s;
    case BlendMode::SrcOver: return clamp255(s + d255(d * (255u - sa)));
    case BlendMode::DstOver: return clamp255(d + d255(s * (255u - da)));
    case BlendMode::SrcIn: return d255(s * da);
    case BlendMode::DstIn: return d255(d * sa);
    case BlendMode::SrcOut: return d255(s * (255u - da));
    case BlendMode::DstOut: return d255(d * (255u - sa));
    case BlendMode::SrcAtop: return clamp255(d255(s * da) + d255(d * (255u - sa)));
    case BlendMode::DstAtop: return clamp255(d255(d * sa) + d255(s * (255u - da)));
    case BlendMode::Xor: return clamp255(d255(s * (255u - da)) + d255(d * (255u - sa)));
    case BlendMode::Plus: return clamp255(s + d);
    case BlendMode::Multiply: return clamp255(d255(s * (255u - da)) + d255(d * (255u - sa)) + d255(s * d));
    case BlendMode::Screen: {
      const u32 p = d255(s * d);
      const u32 sum = s + d;
      return sum > p ? clamp255(sum - p) : 0u;
    }
  }
  return 0;
}

// Alpha channel result. Multiply and Screen use the SrcOver alpha; the other modes use the channel formula with S = Sa and D = Da.
u32 pd_alpha(BlendMode m, u32 sa, u32 da) {
  if (m == BlendMode::Multiply || m == BlendMode::Screen) return channel(BlendMode::SrcOver, sa, da, sa, da);
  return channel(m, sa, da, sa, da);
}

// s: scaled source (4 values). d: destination pixel, updated in place.
void apply_pixel(BlendMode m, const u32 s[4], u8* d) {
  const u32 sa = s[3], da = d[3];
  const u32 r0 = channel(m, s[0], d[0], sa, da);
  const u32 r1 = channel(m, s[1], d[1], sa, da);
  const u32 r2 = channel(m, s[2], d[2], sa, da);
  const u32 r3 = pd_alpha(m, sa, da);
  // Last step: every colour channel is clamped to the result alpha, so the output satisfies R, G, B <= A.
  d[0] = static_cast<u8>(r0 > r3 ? r3 : r0);
  d[1] = static_cast<u8>(r1 > r3 ? r3 : r1);
  d[2] = static_cast<u8>(r2 > r3 ? r3 : r2);
  d[3] = static_cast<u8>(r3);
}

bool size_ok(u32 w, u32 h) { return w != 0 && h != 0 && w <= kMaxSize && h <= kMaxSize; }

// Do the byte ranges of any touched row of A intersect the byte range of any touched row of B? Row i of A covers
// [a + i * a_stride, +a_len), row j of B covers [b + j * b_stride, +b_len), for i, j in [0, rows). Addresses are only
// compared as integers here, for this status. They never influence any output byte. Addresses of user space fit 63 bits.
bool rows_overlap(const void* a, u64 a_stride, u64 a_len, const void* b, u64 b_stride, u64 b_len, u64 rows) {
  const i64 pa = static_cast<i64>(reinterpret_cast<std::uintptr_t>(a));
  const i64 pb = static_cast<i64>(reinterpret_cast<std::uintptr_t>(b));
  const i64 delta = pa - pb;  // A relative to B
  const i64 bs = static_cast<i64>(b_stride);
  for (u64 i = 0; i < rows; ++i) {
    const i64 t = delta + static_cast<i64>(i) * static_cast<i64>(a_stride);  // start of A row i relative to B base
    // B row j overlaps iff j * bs > t - b_len and j * bs < t + a_len.
    const i64 low = t - static_cast<i64>(b_len);
    i64 j = 0;
    if (low >= 0) j = low / bs + 1;
    if (j < static_cast<i64>(rows) && j * bs < t + static_cast<i64>(a_len)) return true;
  }
  return false;
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadSize: return "bad size";
    case Status::BadMode: return "bad mode";
    case Status::BadRadius: return "bad radius";
    case Status::Overlap: return "overlap";
  }
  return "unknown composite status";
}

std::uint32_t div255(std::uint32_t v) {
  const u64 t = static_cast<u64>(v) + 128u;
  return static_cast<u32>((t + (t >> 8)) >> 8);
}

void premultiply(const std::uint8_t in[4], std::uint8_t out[4]) {
  if (in == nullptr || out == nullptr) return;
  const u32 a = in[3];
  const u32 r = d255(in[0] * a), g = d255(in[1] * a), b = d255(in[2] * a);
  out[0] = static_cast<u8>(r);
  out[1] = static_cast<u8>(g);
  out[2] = static_cast<u8>(b);
  out[3] = static_cast<u8>(a);
}

void unpremultiply(const std::uint8_t in[4], std::uint8_t out[4]) {
  if (in == nullptr || out == nullptr) return;
  const u32 a = in[3];
  if (a == 0) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }
  u32 v[3];
  for (int i = 0; i < 3; ++i) {
    const u32 q = (static_cast<u32>(in[i]) * 255u + a / 2u) / a;
    v[i] = q > 255u ? 255u : q;
  }
  out[0] = static_cast<u8>(v[0]);
  out[1] = static_cast<u8>(v[1]);
  out[2] = static_cast<u8>(v[2]);
  out[3] = static_cast<u8>(a);
}

Status blend(const Surface& dst, const ConstSurface& src, std::int32_t dx, std::int32_t dy, BlendMode mode, std::uint8_t opacity) {
  if (dst.data == nullptr || src.data == nullptr) return Status::NullBuffer;
  if (!size_ok(dst.width, dst.height) || !size_ok(src.width, src.height) || static_cast<u64>(dst.stride) < static_cast<u64>(dst.width) * 4u ||
      static_cast<u64>(src.stride) < static_cast<u64>(src.width) * 4u)
    return Status::BadSize;
  if (!mode_ok(mode)) return Status::BadMode;
  const i64 x0 = dx > 0 ? dx : 0, y0 = dy > 0 ? dy : 0;
  const i64 ex = static_cast<i64>(dx) + src.width, ey = static_cast<i64>(dy) + src.height;
  const i64 x1 = ex < static_cast<i64>(dst.width) ? ex : static_cast<i64>(dst.width);
  const i64 y1 = ey < static_cast<i64>(dst.height) ? ey : static_cast<i64>(dst.height);
  if (x0 >= x1 || y0 >= y1) return Status::Ok;  // nothing touched
  const u64 w = static_cast<u64>(x1 - x0), h = static_cast<u64>(y1 - y0);
  const u8* sp0 = src.data + static_cast<std::size_t>(y0 - dy) * src.stride + static_cast<std::size_t>(x0 - dx) * 4u;
  u8* dp0 = dst.data + static_cast<std::size_t>(y0) * dst.stride + static_cast<std::size_t>(x0) * 4u;
  if (rows_overlap(dp0, dst.stride, w * 4u, sp0, src.stride, w * 4u, h)) return Status::Overlap;
  const u32 op = opacity;
  for (u64 j = 0; j < h; ++j) {
    const u8* sp = sp0 + j * src.stride;
    u8* dp = dp0 + j * dst.stride;
    for (u64 i = 0; i < w; ++i, sp += 4, dp += 4) {
      const u32 s[4] = {d255(sp[0] * op), d255(sp[1] * op), d255(sp[2] * op), d255(sp[3] * op)};
      apply_pixel(mode, s, dp);
    }
  }
  return Status::Ok;
}

Status fill_mask(const Surface& dst, const Mask& mask, std::int32_t dx, std::int32_t dy, const std::uint8_t color[4], BlendMode mode) {
  if (dst.data == nullptr || mask.data == nullptr || color == nullptr) return Status::NullBuffer;
  if (!size_ok(dst.width, dst.height) || !size_ok(mask.width, mask.height) || static_cast<u64>(dst.stride) < static_cast<u64>(dst.width) * 4u ||
      static_cast<u64>(mask.stride) < static_cast<u64>(mask.width))
    return Status::BadSize;
  if (!mode_ok(mode)) return Status::BadMode;
  const i64 x0 = dx > 0 ? dx : 0, y0 = dy > 0 ? dy : 0;
  const i64 ex = static_cast<i64>(dx) + mask.width, ey = static_cast<i64>(dy) + mask.height;
  const i64 x1 = ex < static_cast<i64>(dst.width) ? ex : static_cast<i64>(dst.width);
  const i64 y1 = ey < static_cast<i64>(dst.height) ? ey : static_cast<i64>(dst.height);
  if (x0 >= x1 || y0 >= y1) return Status::Ok;
  const u64 w = static_cast<u64>(x1 - x0), h = static_cast<u64>(y1 - y0);
  const u8* mp0 = mask.data + static_cast<std::size_t>(y0 - dy) * mask.stride + static_cast<std::size_t>(x0 - dx);
  u8* dp0 = dst.data + static_cast<std::size_t>(y0) * dst.stride + static_cast<std::size_t>(x0) * 4u;
  if (rows_overlap(dp0, dst.stride, w * 4u, mp0, mask.stride, w, h)) return Status::Overlap;
  const u32 c0 = color[0], c1 = color[1], c2 = color[2], c3 = color[3];
  for (u64 j = 0; j < h; ++j) {
    const u8* mp = mp0 + j * mask.stride;
    u8* dp = dp0 + j * dst.stride;
    for (u64 i = 0; i < w; ++i, dp += 4) {
      const u32 m = mp[i];
      const u32 s[4] = {d255(c0 * m), d255(c1 * m), d255(c2 * m), d255(c3 * m)};
      apply_pixel(mode, s, dp);
    }
  }
  return Status::Ok;
}

namespace {

// Horizontal pass of one box blur, in place, row by row. The row is copied to a fixed local array first, so the window
// always reads the values before this pass (the result equals a full intermediate image).
void blur_h(const Surface& s, u32 r) {
  u8 line[kMaxSize * 4];
  const i64 w = s.width;
  const std::size_t rowb = static_cast<std::size_t>(s.width) * 4u;
  const u32 window = 2u * r + 1u;
  const i64 ri = static_cast<i64>(r);
  for (u32 y = 0; y < s.height; ++y) {
    u8* row = s.data + static_cast<std::size_t>(y) * s.stride;
    for (std::size_t i = 0; i < rowb; ++i) line[i] = row[i];
    for (int c = 0; c < 4; ++c) {
      u32 sum = 0;
      for (i64 i = -ri; i <= ri; ++i) {
        const i64 k = i < 0 ? 0 : (i >= w ? w - 1 : i);
        sum += line[k * 4 + c];
      }
      for (i64 x = 0; x < w; ++x) {
        row[x * 4 + c] = static_cast<u8>((sum + r) / window);
        const i64 add = x + ri + 1 >= w ? w - 1 : x + ri + 1;
        const i64 sub = x - ri < 0 ? 0 : x - ri;
        sum = sum + line[add * 4 + c] - line[sub * 4 + c];
      }
    }
  }
}

// Vertical pass, in place, column by column (the column is copied to a fixed local array: 4 channels of at most 8192 rows).
void blur_v(const Surface& s, u32 r) {
  u8 col[kMaxSize * 4];
  const i64 h = s.height;
  const u32 window = 2u * r + 1u;
  const i64 ri = static_cast<i64>(r);
  for (u32 x = 0; x < s.width; ++x) {
    u8* base = s.data + static_cast<std::size_t>(x) * 4u;
    for (i64 y = 0; y < h; ++y) {
      const u8* px = base + static_cast<std::size_t>(y) * s.stride;
      for (int c = 0; c < 4; ++c) col[y * 4 + c] = px[c];
    }
    for (int c = 0; c < 4; ++c) {
      u32 sum = 0;
      for (i64 i = -ri; i <= ri; ++i) {
        const i64 k = i < 0 ? 0 : (i >= h ? h - 1 : i);
        sum += col[k * 4 + c];
      }
      for (i64 y = 0; y < h; ++y) {
        base[static_cast<std::size_t>(y) * s.stride + c] = static_cast<u8>((sum + r) / window);
        const i64 add = y + ri + 1 >= h ? h - 1 : y + ri + 1;
        const i64 sub = y - ri < 0 ? 0 : y - ri;
        sum = sum + col[add * 4 + c] - col[sub * 4 + c];
      }
    }
  }
}

Status blur_checked(const Surface& s, u32 rx, u32 ry, int passes) {
  if (s.data == nullptr) return Status::NullBuffer;
  if (!size_ok(s.width, s.height) || static_cast<u64>(s.stride) < static_cast<u64>(s.width) * 4u) return Status::BadSize;
  if (rx > kMaxRadius || ry > kMaxRadius) return Status::BadRadius;
  for (int i = 0; i < passes; ++i) {
    if (rx > 0) blur_h(s, rx);
    if (ry > 0) blur_v(s, ry);
  }
  return Status::Ok;
}

}  // namespace

Status box_blur(const Surface& s, std::uint32_t radius_x, std::uint32_t radius_y, std::uint8_t* scratch) {
  (void)scratch;  // reserved, never read or written
  return blur_checked(s, radius_x, radius_y, 1);
}

Status blur3(const Surface& s, std::uint32_t radius_x, std::uint32_t radius_y, std::uint8_t* scratch) {
  (void)scratch;  // reserved, never read or written
  return blur_checked(s, radius_x, radius_y, 3);
}

std::uint64_t digest_surface(const ConstSurface& s) {
  u64 h = 0xcbf29ce484222325ull;
  if (s.data == nullptr || s.width == 0 || s.height == 0) return h;
  for (u32 y = 0; y < s.height; ++y) {
    const u8* row = s.data + static_cast<std::size_t>(y) * s.stride;
    for (std::size_t x = 0; x < static_cast<std::size_t>(s.width) * 4u; ++x) {
      h ^= row[x];
      h *= 0x100000001b3ull;
    }
  }
  return h;
}

}  // namespace lumora::composite
