// Lumora color module (scope G). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/color/color.cpp only.
// All float math must give bit-identical results on Linux, Windows and Android:
// no libm transcendental calls (pow, exp, log, cbrt); use own polynomial or
// iterative code built from + - * / sqrt only, and disable FMA contraction.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::color {

struct Rgba {  // straight (non-premultiplied) float color, nominal range 0..1
  float r, g, b, a;
};

// sRGB transfer function (IEC 61966-2-1). Inputs outside 0..1 are clamped. NaN maps to 0.
float srgb_to_linear(float v);
float linear_to_srgb(float v);

// 8-bit helpers. Rounding: nearest, ties away from zero, clamp to 0..255.
std::uint8_t to_u8(float v);   // v in 0..1 (clamped, NaN -> 0)
float from_u8(std::uint8_t v); // v / 255

// sRGB 0..1 <-> OKLab. Alpha is untouched and handled by the callers.
struct Oklab {
  float l, a, b;
};
Oklab srgb_to_oklab(float r, float g, float b);
void oklab_to_srgb(const Oklab& lab, float out_rgb[3]);  // result clamped to 0..1

// Alpha handling.
Rgba premultiply(const Rgba& c);
Rgba unpremultiply(const Rgba& c);  // alpha 0 gives all zero
// Source-over of straight colors, result straight. If the result alpha is 0 all four channels are 0. Operates only
// by arithmetic on the passed values (no conversion inside).
// Sampling exactly at a stop position returns that stop color unchanged (no space round trip).
Rgba over(const Rgba& src, const Rgba& dst);

enum class Space : std::uint8_t { Srgb = 0, Linear = 1, Oklab = 2 };  // interpolation space

struct Stop {
  float position;  // 0..1
  Rgba color;      // sRGB straight
};

constexpr std::size_t kMaxStops = 16;

enum class GradientStatus : std::uint8_t {
  Ok = 0,
  TooFewStops,      // fewer than 2
  TooManyStops,     // more than kMaxStops
  BadPosition,      // NaN, outside 0..1, or decreasing order (equal is allowed: hard step)
  BadColor,         // NaN or outside 0..1 in any channel
  BadSpace,
};
const char* gradient_status_text(GradientStatus s);

struct Gradient {
  Stop stops[kMaxStops];
  std::size_t count = 0;
  Space space = Space::Srgb;
};

GradientStatus validate_gradient(const Gradient& g);
// Sample at t (clamped to 0..1; NaN -> 0). Returns sRGB straight color.
// Before the first stop and after the last stop the end colors hold.
// With equal positions the later stop wins for t >= position (hard step).
// Interpolation: color channels in `space`, alpha linear. Invalid gradient returns {0,0,0,0}.
Rgba sample_gradient(const Gradient& g, float t);
// Fill n evenly spaced samples (t = i / (n - 1), n == 1 gives t = 0) as packed RGBA8 sRGB, straight alpha.
// Returns false (out untouched) on invalid gradient, n == 0 or n > 65536.
bool bake_gradient_u8(const Gradient& g, std::size_t n, std::uint8_t* out_rgba);

// Standard FNV-1a 64 (offset 0xcbf29ce484222325, prime 0x100000001b3) over the n bytes in order, for golden tests.
std::uint64_t digest_u8(const std::uint8_t* data, std::size_t n);

}  // namespace lumora::color
