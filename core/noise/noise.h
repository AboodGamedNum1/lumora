// Lumora deterministic integer noise (scope Q). Pure CPU, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/noise/noise.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no
// floats, no unordered containers, no static mutable state, no pointer values in any decision. Purpose: procedural textures and
// motion for wallpapers (clouds, aurora, water, grain), the CPU reference for later shader ports. Evidence label: software only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::noise {

// Output values are Q16: 0..65535 maps to 0.0..1.0 (1.0 is not representable; the maximum is 65535). Sample positions are
// Q16 fixed point as well: the integer part (x >> 16, arithmetic shift, floor for negatives) selects the lattice cell and the
// low 16 bits are the position inside it. Any int32 position is valid.
using Q16 = std::uint16_t;
using Pos = std::int32_t;

// Hash: the 32-bit mixer is defined exactly as
//   h = seed; h ^= a * 0x9E3779B1u; h = rotl(h, 13) * 0x85EBCA6Bu; h ^= b * 0xC2B2AE35u; h = rotl(h, 15) * 0x27D4EB2Fu;
//   h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
// with all arithmetic modulo 2^32 and rotl a 32-bit rotate left. a and b are the two lattice coordinates reinterpreted as
// uint32. The 3D hash is hash2(seed, hash2(seed, x, y), z). Exactly these definitions, so golden values are portable.
std::uint32_t hash2(std::uint32_t seed, std::int32_t x, std::int32_t y);
std::uint32_t hash3(std::uint32_t seed, std::int32_t x, std::int32_t y, std::int32_t z);

// Smoothstep-like fade used by value and gradient noise: fade(t) = t*t*t*(t*(t*6 - 15) + 10) on t in 0..65536 (Q16, with
// 65536 allowed as the end point), result in 0..65536. The value is the exact rational t^3 * (6 t^2 - 15 t 65536 + 10 * 65536^2) / 2^64 rounded ONCE, half up.
// The numerator reaches 2^80, so it needs a portable multi-word multiply (no compiler extensions); t above 65536 is treated
// as 65536. fade(0) = 0 and fade(65536) = 65536.
std::uint32_t fade(std::uint32_t t);

// Value noise: hash of the four (2D) or eight (3D) lattice corners, taken as hash >> 16 (0..65535), blended with fade() weights,
// each linear interpolation rounded once as documented in docs/noise.md. Returns 0..65535.
Q16 value2(std::uint32_t seed, Pos x, Pos y);
Q16 value3(std::uint32_t seed, Pos x, Pos y, Pos z);

// Gradient (Perlin style) noise with 8 fixed 2D gradients and 12 fixed 3D gradients chosen by the corner hash (tables are
// defined in docs/noise.md and mirrored by the tests). The raw result is mapped to 0..65535 with 32768 meaning zero; the
// mapping and clamp are documented in docs/noise.md. Returns 0..65535.
Q16 gradient2(std::uint32_t seed, Pos x, Pos y);
Q16 gradient3(std::uint32_t seed, Pos x, Pos y, Pos z);

// Cellular (Worley) noise F1, 2D: one feature point per cell at a hashed offset, nearest point over the 3 x 3 cells around
// the sample, squared distance in 64 bits, result = min(65535, isqrt(d2) rounded down in the Q16 scale) (the integer
// square root is exact and defined in docs/noise.md). Returns 0..65535. The search covers only the 3 x 3 cells, so this is by
// definition the nearest of those nine feature points; it can differ from the true nearest point in rare cases and the docs
// must say so.
Q16 cellular2(std::uint32_t seed, Pos x, Pos y);

// Fractal sum (fBm) of an octave function over `octaves` (1..8): octave i samples at position * 2^i (wrapping in int32 as
// unsigned arithmetic) with seed + i, weights halve each octave, the sum is normalized by the total weight with one rounding.
// kind selects value2, gradient2 or cellular2. Returns 0..65535.
enum class Kind : std::uint8_t { Value = 0, Gradient = 1, Cellular = 2 };
// An invalid kind or octaves value returns 0 (fill_image reports them as errors). Weights are 128 >> i for octave i; the result
// is (2 * sum + total_weight) / (2 * total_weight) with integer division, that is the weighted average rounded half up exactly.
Q16 fbm2(Kind kind, std::uint32_t seed, Pos x, Pos y, std::uint32_t octaves);

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,  // out is null
  BadSize,     // width or height is 0 or > kMaxSize, or stride < width
  BadKind,     // Kind outside the enum
  BadOctaves,  // octaves is 0 or > 8
};
// status_text gives the lower-case spaced enum name ("ok", "null buffer", "bad size", ...) and "unknown" for an unknown value.
const char* status_text(Status s);
constexpr std::uint32_t kMaxSize = 8192;

// Fills a width x height 8-bit image: pixel (i, j) = fbm2(kind, seed, x0 + i * step, y0 + j * step) >> 8, with step a
// Q16 increment (may be negative or zero); position arithmetic wraps in uint32. Checks in enum order. Nothing is written
// on error; stride padding is never written.
Status fill_image(std::uint8_t* out, std::uint32_t width, std::uint32_t height, std::uint32_t stride, Kind kind,
                  std::uint32_t seed, Pos x0, Pos y0, Pos step, std::uint32_t octaves);

// FNV-1a 64 over the bytes of an 8-bit image, rows top to bottom, width bytes per row; null or zero size hashes zero bytes.
std::uint64_t digest_image(const std::uint8_t* data, std::uint32_t width, std::uint32_t height, std::uint32_t stride);

}  // namespace lumora::noise
