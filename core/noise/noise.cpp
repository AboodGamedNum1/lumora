// Lumora deterministic integer noise (scope Q). Integer arithmetic only, no static mutable state.
// Every rule is defined in docs/noise.md; the tests compare against an independent reference of those rules.
#include "noise.h"

namespace lumora::noise {

namespace {

constexpr std::uint32_t rotl32(std::uint32_t v, unsigned n) { return (v << n) | (v >> (32u - n)); }

// Cell index: floor(p / 65536) without a signed right shift.
inline std::int32_t cell_of(Pos p) {
  const std::uint32_t u = static_cast<std::uint32_t>(p);
  const std::uint32_t c = (u >> 16) | ((u & 0x80000000u) != 0u ? 0xFFFF0000u : 0u);
  return static_cast<std::int32_t>(c);
}
inline std::int64_t frac_of(Pos p) { return static_cast<std::int64_t>(static_cast<std::uint32_t>(p) & 0xFFFFu); }

// Cell plus a small non-negative step; cells are -32768..32767 so this never overflows.
inline std::int32_t cadd(std::int32_t c, int d) { return c + d; }

// floor(v / 65536) for any int64 value of moderate size.
inline std::int64_t floor16(std::int64_t v) {
  return v >= 0 ? (v >> 16) : -((-v + 65535) >> 16);
}

// Linear interpolation with one rounding (half up): floor((a*(65536-w) + b*w + 32768) / 65536). Works for signed a, b.
inline std::int64_t lerp(std::int64_t a, std::int64_t b, std::int64_t w) {
  return floor16(a * (65536 - w) + b * w + 32768);
}

// 64x64 -> 128 unsigned multiply from 32-bit pieces (portable, no compiler intrinsics).
inline void mul128(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) {
  const std::uint64_t a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
  const std::uint64_t b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
  hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

inline std::int64_t isqrt64(std::uint64_t n) {
  std::uint64_t r = 0;
  std::uint64_t bit = 1ull << 62;
  while (bit > n) bit >>= 2;
  while (bit != 0) {
    if (n >= r + bit) {
      n -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<std::int64_t>(r);
}

constexpr std::int8_t kG2[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
constexpr std::int8_t kG3[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
                                    {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};

inline Q16 map_gradient(std::int64_t raw) {
  const std::int64_t v = 32768 + floor16((raw + 1) * 32768);  // round half up of raw / 2
  return static_cast<Q16>(v < 0 ? 0 : (v > 65535 ? 65535 : v));
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadSize: return "bad size";
    case Status::BadKind: return "bad kind";
    case Status::BadOctaves: return "bad octaves";
  }
  return "unknown";
}

std::uint32_t hash2(std::uint32_t seed, std::int32_t x, std::int32_t y) {
  const std::uint32_t a = static_cast<std::uint32_t>(x), b = static_cast<std::uint32_t>(y);
  std::uint32_t h = seed;
  h ^= a * 0x9E3779B1u;
  h = rotl32(h, 13) * 0x85EBCA6Bu;
  h ^= b * 0xC2B2AE35u;
  h = rotl32(h, 15) * 0x27D4EB2Fu;
  h ^= h >> 16;
  h *= 0x7FEB352Du;
  h ^= h >> 15;
  h *= 0x846CA68Bu;
  h ^= h >> 16;
  return h;
}

std::uint32_t hash3(std::uint32_t seed, std::int32_t x, std::int32_t y, std::int32_t z) {
  return hash2(seed, static_cast<std::int32_t>(hash2(seed, x, y)), z);
}

std::uint32_t fade(std::uint32_t t) {
  if (t > 65536u) t = 65536u;
  const std::uint64_t T = t;
  // inner = 6t^2 - 15*65536*t + 10*2^32, always positive on 0..65536 (at most 10*2^32).
  const std::uint64_t inner = 6ull * T * T + 10ull * 4294967296ull - 983040ull * T;
  std::uint64_t hi, lo;
  mul128(T * T * T, inner, hi, lo);
  const std::uint64_t half = 0x8000000000000000ull;
  if (lo >= ~half + 1ull) ++hi;  // lo + 2^63 carries out of the low word
  return static_cast<std::uint32_t>(hi);
}

Q16 value2(std::uint32_t seed, Pos x, Pos y) {
  const std::int32_t cx = cell_of(x), cy = cell_of(y);
  const std::int64_t u = fade(static_cast<std::uint32_t>(frac_of(x))), v = fade(static_cast<std::uint32_t>(frac_of(y)));
  auto c = [&](int i, int j) { return static_cast<std::int64_t>(hash2(seed, cadd(cx, i), cadd(cy, j)) >> 16); };
  const std::int64_t r = lerp(lerp(c(0, 0), c(1, 0), u), lerp(c(0, 1), c(1, 1), u), v);
  return static_cast<Q16>(r);
}

Q16 value3(std::uint32_t seed, Pos x, Pos y, Pos z) {
  const std::int32_t cx = cell_of(x), cy = cell_of(y), cz = cell_of(z);
  const std::int64_t u = fade(static_cast<std::uint32_t>(frac_of(x))), v = fade(static_cast<std::uint32_t>(frac_of(y))),
                     w = fade(static_cast<std::uint32_t>(frac_of(z)));
  auto c = [&](int i, int j, int k) {
    return static_cast<std::int64_t>(hash3(seed, cadd(cx, i), cadd(cy, j), cadd(cz, k)) >> 16);
  };
  auto plane = [&](int k) { return lerp(lerp(c(0, 0, k), c(1, 0, k), u), lerp(c(0, 1, k), c(1, 1, k), u), v); };
  return static_cast<Q16>(lerp(plane(0), plane(1), w));
}

Q16 gradient2(std::uint32_t seed, Pos x, Pos y) {
  const std::int32_t cx = cell_of(x), cy = cell_of(y);
  const std::int64_t fx = frac_of(x), fy = frac_of(y);
  const std::int64_t u = fade(static_cast<std::uint32_t>(fx)), v = fade(static_cast<std::uint32_t>(fy));
  auto d = [&](int i, int j) {
    const std::int8_t* g = kG2[(hash2(seed, cadd(cx, i), cadd(cy, j)) >> 16) & 7u];
    return g[0] * (fx - 65536 * i) + g[1] * (fy - 65536 * j);
  };
  return map_gradient(lerp(lerp(d(0, 0), d(1, 0), u), lerp(d(0, 1), d(1, 1), u), v));
}

Q16 gradient3(std::uint32_t seed, Pos x, Pos y, Pos z) {
  const std::int32_t cx = cell_of(x), cy = cell_of(y), cz = cell_of(z);
  const std::int64_t fx = frac_of(x), fy = frac_of(y), fz = frac_of(z);
  const std::int64_t u = fade(static_cast<std::uint32_t>(fx)), v = fade(static_cast<std::uint32_t>(fy)),
                     w = fade(static_cast<std::uint32_t>(fz));
  auto d = [&](int i, int j, int k) {
    const std::int8_t* g = kG3[(hash3(seed, cadd(cx, i), cadd(cy, j), cadd(cz, k)) >> 16) % 12u];
    return g[0] * (fx - 65536 * i) + g[1] * (fy - 65536 * j) + g[2] * (fz - 65536 * k);
  };
  auto plane = [&](int k) { return lerp(lerp(d(0, 0, k), d(1, 0, k), u), lerp(d(0, 1, k), d(1, 1, k), u), v); };
  return map_gradient(lerp(plane(0), plane(1), w));
}

Q16 cellular2(std::uint32_t seed, Pos x, Pos y) {
  const std::int32_t cx = cell_of(x), cy = cell_of(y);
  const std::int64_t fx = frac_of(x), fy = frac_of(y);
  std::uint64_t best = ~0ull;
  for (int j = -1; j <= 1; ++j) {
    for (int i = -1; i <= 1; ++i) {
      const std::uint32_t h = hash2(seed, static_cast<std::int32_t>(static_cast<std::uint32_t>(cx) + static_cast<std::uint32_t>(i)),
                                    static_cast<std::int32_t>(static_cast<std::uint32_t>(cy) + static_cast<std::uint32_t>(j)));
      const std::int64_t dx = i * 65536 + static_cast<std::int64_t>(h & 0xFFFFu) - fx;
      const std::int64_t dy = j * 65536 + static_cast<std::int64_t>(h >> 16) - fy;
      const std::uint64_t d2 = static_cast<std::uint64_t>(dx * dx + dy * dy);
      if (d2 < best) best = d2;
    }
  }
  const std::int64_t r = isqrt64(best);
  return static_cast<Q16>(r > 65535 ? 65535 : r);
}

Q16 fbm2(Kind kind, std::uint32_t seed, Pos x, Pos y, std::uint32_t octaves) {
  const std::uint8_t k = static_cast<std::uint8_t>(kind);
  if (k > 2u || octaves < 1u || octaves > 8u) return 0;
  std::uint64_t sum = 0, total = 0;
  for (std::uint32_t i = 0; i < octaves; ++i) {
    const std::uint64_t wgt = 128u >> i;
    const Pos px = static_cast<Pos>(static_cast<std::uint32_t>(x) << i);
    const Pos py = static_cast<Pos>(static_cast<std::uint32_t>(y) << i);
    const std::uint32_t s = seed + i;
    std::uint64_t v = 0;
    if (k == 0u) v = value2(s, px, py);
    else if (k == 1u) v = gradient2(s, px, py);
    else v = cellular2(s, px, py);
    sum += wgt * v;
    total += wgt;
  }
  return static_cast<Q16>((2 * sum + total) / (2 * total));
}

Status fill_image(std::uint8_t* out, std::uint32_t width, std::uint32_t height, std::uint32_t stride, Kind kind,
                  std::uint32_t seed, Pos x0, Pos y0, Pos step, std::uint32_t octaves) {
  if (out == nullptr) return Status::NullBuffer;
  if (width == 0u || height == 0u || width > kMaxSize || height > kMaxSize || stride < width) return Status::BadSize;
  if (static_cast<std::uint8_t>(kind) > 2u) return Status::BadKind;
  if (octaves < 1u || octaves > 8u) return Status::BadOctaves;
  const std::uint32_t ux = static_cast<std::uint32_t>(x0), uy = static_cast<std::uint32_t>(y0);
  const std::uint32_t us = static_cast<std::uint32_t>(step);
  for (std::uint32_t j = 0; j < height; ++j) {
    std::uint8_t* row = out + static_cast<std::size_t>(j) * stride;
    const Pos py = static_cast<Pos>(uy + j * us);
    for (std::uint32_t i = 0; i < width; ++i) {
      const Pos px = static_cast<Pos>(ux + i * us);
      row[i] = static_cast<std::uint8_t>(fbm2(kind, seed, px, py, octaves) >> 8);
    }
  }
  return Status::Ok;
}

std::uint64_t digest_image(const std::uint8_t* data, std::uint32_t width, std::uint32_t height, std::uint32_t stride) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  if (data == nullptr || width == 0u || height == 0u) return h;
  for (std::uint32_t j = 0; j < height; ++j) {
    const std::uint8_t* row = data + static_cast<std::size_t>(j) * stride;
    for (std::uint32_t i = 0; i < width; ++i) h = (h ^ row[i]) * 0x100000001b3ull;
  }
  return h;
}

}  // namespace lumora::noise
