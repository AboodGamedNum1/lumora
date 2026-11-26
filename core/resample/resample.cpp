// Lumora CPU image resampling and mip chains (scope R). Integer arithmetic only, no heap, no static mutable state.
// Every rule is defined in docs/resample.md; the tests compare against an independent reference of those rules.
#include "resample.h"

namespace lumora::resample {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr u64 kBasis = 0xcbf29ce484222325ull;

inline i64 clampi(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }

// floor(v / 2^k) for a signed value, without a shift of a negative number.
inline i64 floor_shift(i64 v, unsigned k) {
  if (v >= 0) return v >> k;
  const i64 m = (static_cast<i64>(1) << k) - 1;
  return -((-v + m) >> k);
}

// Overlap rule (same as core/composite, generalized to different row counts). See docs/resample.md.
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

// Source position in Q16 for destination index i: floor(((2i+1) * n * 65536) / (2N)) - 32768 (may be negative).
inline i64 src_pos(u64 i, u64 n, u64 N) { return static_cast<i64>(((2 * i + 1) * n * 65536u) / (2 * N)) - 32768; }

// x0 = floor(sx / 65536) and fx = sx - x0 * 65536 in 0..65535, without a shift of a negative value.
inline void split_pos(i64 sx, i64& x0, i64& fx) {
  if (sx >= 0) {
    x0 = sx >> 16;
    fx = sx & 0xFFFF;
  } else {
    x0 = -1;  // sx >= -32768
    fx = sx + 65536;
  }
}

// Catmull-Rom weights at fraction f (0..65535): exact polynomial numerators over 2^33, each rounded half up, then the
// remainder goes to the larger-magnitude centre tap so the four weights sum to exactly 65536.
void cr_weights(i64 f, i64 w[4]) {
  const i64 f2 = f * f, f3 = f2 * f;
  const i64 s16 = 65536, s32 = static_cast<i64>(1) << 32, s48 = static_cast<i64>(1) << 48;
  const i64 n0 = -f3 + 2 * f2 * s16 - f * s32;
  const i64 n1 = 3 * f3 - 5 * f2 * s16 + 2 * s48;
  const i64 n2 = -3 * f3 + 4 * f2 * s16 + f * s32;
  const i64 n3 = f3 - f2 * s16;
  const i64 half = static_cast<i64>(1) << 32;
  w[0] = floor_shift(n0 + half, 33);
  w[1] = floor_shift(n1 + half, 33);
  w[2] = floor_shift(n2 + half, 33);
  w[3] = floor_shift(n3 + half, 33);
  const int k = f <= 32768 ? 1 : 2;
  i64 others = 0;
  for (int i = 0; i < 4; ++i) {
    if (i != k) others += w[i];
  }
  w[k] = 65536 - others;
}

inline void finish_pixel(std::uint8_t* d, i64 r, i64 g, i64 b, i64 a) {
  if (r > a) r = a;
  if (g > a) g = a;
  if (b > a) b = a;
  d[0] = static_cast<std::uint8_t>(r);
  d[1] = static_cast<std::uint8_t>(g);
  d[2] = static_cast<std::uint8_t>(b);
  d[3] = static_cast<std::uint8_t>(a);
}

// Weighted sum of the taps (per channel) rounded once half up and clamped to 0..255.
inline i64 round_q32(i64 s) { return clampi(floor_shift(s + (static_cast<i64>(1) << 31), 32), 0, 255); }

void resize_unchecked(const ConstSurface& src, const Surface& dst, Filter filter) {
  const u64 w = src.width, h = src.height, W = dst.width, H = dst.height;
  for (u64 j = 0; j < H; ++j) {
    std::uint8_t* drow = dst.data + j * dst.stride;
    if (filter == Filter::Nearest) {
      const u64 y = ((2 * j + 1) * h) / (2 * H);
      const std::uint8_t* srow = src.data + y * src.stride;
      for (u64 i = 0; i < W; ++i) {
        const u64 x = ((2 * i + 1) * w) / (2 * W);
        const std::uint8_t* p = srow + x * 4;
        finish_pixel(drow + i * 4, p[0], p[1], p[2], p[3]);
      }
    } else if (filter == Filter::Bilinear || filter == Filter::CatmullRom) {
      const bool cr = filter == Filter::CatmullRom;
      i64 y0, fy;
      split_pos(src_pos(j, h, H), y0, fy);
      i64 wy[4], ty[4];
      int ny;
      if (cr) {
        cr_weights(fy, wy);
        for (int k = 0; k < 4; ++k) ty[k] = y0 - 1 + k;
        ny = 4;
      } else {
        wy[0] = 65536 - fy;
        wy[1] = fy;
        ty[0] = y0;
        ty[1] = y0 + 1;
        ny = 2;
      }
      for (u64 i = 0; i < W; ++i) {
        i64 x0, fx;
        split_pos(src_pos(i, w, W), x0, fx);
        i64 wx[4], tx[4];
        int nx;
        if (cr) {
          cr_weights(fx, wx);
          for (int k = 0; k < 4; ++k) tx[k] = x0 - 1 + k;
          nx = 4;
        } else {
          wx[0] = 65536 - fx;
          wx[1] = fx;
          tx[0] = x0;
          tx[1] = x0 + 1;
          nx = 2;
        }
        i64 acc[4] = {0, 0, 0, 0};
        for (int b = 0; b < ny; ++b) {
          const std::uint8_t* srow = src.data + static_cast<u64>(clampi(ty[b], 0, static_cast<i64>(h) - 1)) * src.stride;
          for (int a = 0; a < nx; ++a) {
            const std::uint8_t* p = srow + static_cast<u64>(clampi(tx[a], 0, static_cast<i64>(w) - 1)) * 4;
            const i64 wt = wy[b] * wx[a];
            acc[0] += wt * p[0];
            acc[1] += wt * p[1];
            acc[2] += wt * p[2];
            acc[3] += wt * p[3];
          }
        }
        finish_pixel(drow + i * 4, round_q32(acc[0]), round_q32(acc[1]), round_q32(acc[2]), round_q32(acc[3]));
      }
    } else {  // Box
      const u64 ylo = (j * h) / H, yhi = ((j + 1) * h + H - 1) / H;  // source rows ylo .. yhi - 1
      const u64 n = w * h;
      for (u64 i = 0; i < W; ++i) {
        const u64 xlo = (i * w) / W, xhi = ((i + 1) * w + W - 1) / W;
        u64 acc[4] = {0, 0, 0, 0};
        for (u64 y = ylo; y < yhi; ++y) {
          const u64 ya = (y * H > j * h) ? y * H : j * h;
          const u64 yb = ((y + 1) * H < (j + 1) * h) ? (y + 1) * H : (j + 1) * h;
          const u64 wy = yb - ya;
          const std::uint8_t* srow = src.data + y * src.stride;
          for (u64 x = xlo; x < xhi; ++x) {
            const u64 xa = (x * W > i * w) ? x * W : i * w;
            const u64 xb = ((x + 1) * W < (i + 1) * w) ? (x + 1) * W : (i + 1) * w;
            const u64 wt = (xb - xa) * wy;
            const std::uint8_t* p = srow + x * 4;
            acc[0] += wt * p[0];
            acc[1] += wt * p[1];
            acc[2] += wt * p[2];
            acc[3] += wt * p[3];
          }
        }
        u64 v[4];
        for (int c = 0; c < 4; ++c) v[c] = (2 * acc[c] + n) / (2 * n);
        finish_pixel(drow + i * 4, static_cast<i64>(v[0]), static_cast<i64>(v[1]), static_cast<i64>(v[2]), static_cast<i64>(v[3]));
      }
    }
  }
}

bool size_ok(u32 w, u32 h) { return w != 0u && h != 0u && w <= kMaxSize && h <= kMaxSize; }

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadSize: return "bad size";
    case Status::BadFilter: return "bad filter";
    case Status::Overlap: return "overlap";
    case Status::BufferTooSmall: return "buffer too small";
  }
  return "unknown";
}

Status resize(const ConstSurface& src, const Surface& dst, Filter filter) {
  if (src.data == nullptr || dst.data == nullptr) return Status::NullBuffer;
  if (!size_ok(src.width, src.height) || !size_ok(dst.width, dst.height) ||
      static_cast<u64>(src.stride) < static_cast<u64>(src.width) * 4u ||
      static_cast<u64>(dst.stride) < static_cast<u64>(dst.width) * 4u) {
    return Status::BadSize;
  }
  if (static_cast<std::uint8_t>(filter) > 3u) return Status::BadFilter;
  if (rows_overlap(dst.data, dst.stride, static_cast<u64>(dst.width) * 4u, dst.height, src.data, src.stride,
                   static_cast<u64>(src.width) * 4u, src.height)) {
    return Status::Overlap;
  }
  resize_unchecked(src, dst, filter);
  return Status::Ok;
}

std::uint32_t mip_count(std::uint32_t width, std::uint32_t height) {
  if (!size_ok(width, height)) return 0;
  u32 m = width > height ? width : height;
  u32 n = 1;
  while (m > 1u) {
    m >>= 1;
    ++n;
  }
  return n;
}

bool mip_level_size(std::uint32_t width, std::uint32_t height, std::uint32_t level, std::uint32_t* out_w,
                    std::uint32_t* out_h) {
  if (level >= mip_count(width, height)) return false;
  u32 w = width, h = height;
  for (u32 i = 0; i < level; ++i) {
    w = (w >> 1) > 1u ? (w >> 1) : 1u;
    h = (h >> 1) > 1u ? (h >> 1) : 1u;
  }
  if (out_w != nullptr) *out_w = w;
  if (out_h != nullptr) *out_h = h;
  return true;
}

std::uint64_t mip_chain_bytes(std::uint32_t width, std::uint32_t height) {
  const u32 n = mip_count(width, height);
  u64 total = 0;
  u32 w = width, h = height;
  for (u32 i = 0; i < n; ++i) {
    total += static_cast<u64>(w) * h * 4u;
    w = (w >> 1) > 1u ? (w >> 1) : 1u;
    h = (h >> 1) > 1u ? (h >> 1) : 1u;
  }
  return total;
}

Status build_mip_chain(const ConstSurface& src, std::uint8_t* out, std::size_t capacity) {
  if (src.data == nullptr || out == nullptr) return Status::NullBuffer;
  if (!size_ok(src.width, src.height) || static_cast<u64>(src.stride) < static_cast<u64>(src.width) * 4u) {
    return Status::BadSize;
  }
  const u64 bytes = mip_chain_bytes(src.width, src.height);
  if (rows_overlap(out, bytes, bytes, 1, src.data, src.stride, static_cast<u64>(src.width) * 4u, src.height)) {
    return Status::Overlap;
  }
  if (static_cast<u64>(capacity) < bytes) return Status::BufferTooSmall;
  u32 w = src.width, h = src.height;
  for (u32 j = 0; j < h; ++j) {
    const std::uint8_t* s = src.data + static_cast<u64>(j) * src.stride;
    std::uint8_t* d = out + static_cast<u64>(j) * w * 4u;
    for (u32 i = 0; i < w * 4u; ++i) d[i] = s[i];
  }
  const u32 n = mip_count(src.width, src.height);
  u64 off = 0;
  for (u32 lvl = 1; lvl < n; ++lvl) {
    const u32 nw = (w >> 1) > 1u ? (w >> 1) : 1u, nh = (h >> 1) > 1u ? (h >> 1) : 1u;
    const u64 next = off + static_cast<u64>(w) * h * 4u;
    const ConstSurface from{out + off, w, h, w * 4u};
    const Surface to{out + next, nw, nh, nw * 4u};
    resize_unchecked(from, to, Filter::Box);
    off = next;
    w = nw;
    h = nh;
  }
  return Status::Ok;
}

std::uint64_t digest_surface(const ConstSurface& s) {
  u64 h = kBasis;
  if (s.data == nullptr || s.width == 0u || s.height == 0u) return h;
  for (u32 j = 0; j < s.height; ++j) {
    const std::uint8_t* row = s.data + static_cast<u64>(j) * s.stride;
    for (u32 i = 0; i < s.width * 4u; ++i) h = (h ^ row[i]) * 0x100000001b3ull;
  }
  return h;
}

}  // namespace lumora::resample
