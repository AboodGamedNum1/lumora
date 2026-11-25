// Color and gradient module. Rules, error bounds and golden digests: docs/color.md.
// Determinism: no libm function is called. pow, cbrt, exp and log are own code built from + - * / only (plus exact
// bit operations), computed in double. Compilers are asked not to fuse multiply-add; the project must not enable
// fast-math. Results are therefore bit-identical on every host with IEEE 754 double arithmetic.
#include "color.h"

#include <cstring>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::color {
namespace {

constexpr double kLn2 = 0.693147180559945309417232121458;
constexpr double kLn2Hi = 6.93147180369123816490e-01;  // low bits zero: k * kLn2Hi is exact for |k| < 2^20
constexpr double kLn2Lo = 1.90821492927058770002e-10;
constexpr double kSqrt2 = 1.41421356237309504880;

std::uint64_t bits_of(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof b);
  return b;
}
double from_bits(std::uint64_t b) {
  double v;
  std::memcpy(&v, &b, sizeof v);
  return v;
}

// Natural logarithm for finite x > 0. Exponent split by bit operations, then 2 atanh(s) with s = (m-1)/(m+1) for a
// mantissa m in [1/sqrt2, sqrt2], |s| <= 0.1716; the series is cut after s^33 (truncation below 1e-26).
double my_log(double x) {
  int e = 0;
  if (x < 2.2250738585072014e-308) {  // subnormal: scale by 2^54 (exact)
    x *= 18014398509481984.0;
    e = -54;
  }
  std::uint64_t b = bits_of(x);
  e += static_cast<int>((b >> 52) & 0x7FF) - 1023;
  double m = from_bits((b & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull);  // [1,2)
  if (m > kSqrt2) {
    m *= 0.5;
    e += 1;
  }
  const double s = (m - 1.0) / (m + 1.0);
  const double s2 = s * s;
  double p = 1.0 / 33.0;
  for (int k = 31; k >= 1; k -= 2) p = p * s2 + 1.0 / static_cast<double>(k);
  return static_cast<double>(e) * kLn2 + 2.0 * s * p;
}

// Exponential for |x| < 700: x = k ln2 + r, |r| <= ln2/2, Taylor series to r^22, then scaling by 2^k.
double my_exp(double x) {
  if (x > 700.0) x = 700.0;
  if (x < -700.0) x = -700.0;
  double kf = x / kLn2;
  kf = kf < 0.0 ? -static_cast<double>(static_cast<long long>(0.5 - kf)) : static_cast<double>(static_cast<long long>(kf + 0.5));
  const double r = (x - kf * kLn2Hi) - kf * kLn2Lo;
  double p = 1.0;
  for (int k = 22; k >= 1; --k) p = 1.0 + p * r / static_cast<double>(k);
  const int k = static_cast<int>(kf);
  return p * from_bits(static_cast<std::uint64_t>(k + 1023) << 52);
}

double my_pow(double x, double y) {  // x > 0 finite
  return my_exp(y * my_log(x));
}

// Cube root for finite x >= 0: exp(log(x) / 3) followed by one Newton step.
double my_cbrt(double x) {
  if (!(x > 0.0)) return 0.0;
  double y = my_exp(my_log(x) / 3.0);
  y = y - (y * y * y - x) / (3.0 * y * y);
  return y;
}

double clamp01d(double v) {
  if (!(v > 0.0)) return 0.0;  // also NaN
  return v > 1.0 ? 1.0 : v;
}

double srgb_to_linear_d(double v) {
  v = clamp01d(v);
  if (v <= 0.04045) return v / 12.92;
  return my_pow((v + 0.055) / 1.055, 2.4);
}

double linear_to_srgb_d(double v) {
  v = clamp01d(v);
  if (v <= 0.0031308) return v * 12.92;
  return 1.055 * my_pow(v, 1.0 / 2.4) - 0.055;
}

double signed_cbrt(double x) { return x < 0.0 ? -my_cbrt(-x) : my_cbrt(x); }

struct LabD {
  double l, a, b;
};

LabD linear_to_oklab_d(double r, double g, double b) {
  const double l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
  const double m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
  const double s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;
  const double l_ = signed_cbrt(l), m_ = signed_cbrt(m), s_ = signed_cbrt(s);
  return {0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
          1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
          0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_};
}

void oklab_to_linear_d(const LabD& lab, double out[3]) {
  const double l_ = lab.l + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
  const double m_ = lab.l - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
  const double s_ = lab.l - 0.0894841775 * lab.a - 1.2914855480 * lab.b;
  const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
  out[0] = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
  out[1] = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
  out[2] = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
}

LabD srgb_to_oklab_d(double r, double g, double b) {
  return linear_to_oklab_d(srgb_to_linear_d(r), srgb_to_linear_d(g), srgb_to_linear_d(b));
}

void oklab_to_srgb_d(const LabD& lab, double out[3]) {
  double lin[3];
  oklab_to_linear_d(lab, lin);
  for (int i = 0; i < 3; ++i) out[i] = linear_to_srgb_d(lin[i]);
}

bool finite_in_01(float v) { return v >= 0.0f && v <= 1.0f; }  // false for NaN

double clamp01f(float v) { return clamp01d(static_cast<double>(v)); }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
float srgb_to_linear(float v) { return static_cast<float>(srgb_to_linear_d(static_cast<double>(v))); }
float linear_to_srgb(float v) { return static_cast<float>(linear_to_srgb_d(static_cast<double>(v))); }

std::uint8_t to_u8(float v) {
  const double x = clamp01f(v) * 255.0;  // exact in double
  return static_cast<std::uint8_t>(static_cast<int>(x + 0.5));  // x >= 0: nearest, ties away from zero
}
float from_u8(std::uint8_t v) { return static_cast<float>(v) / 255.0f; }

Oklab srgb_to_oklab(float r, float g, float b) {
  const LabD l = srgb_to_oklab_d(r, g, b);
  return {static_cast<float>(l.l), static_cast<float>(l.a), static_cast<float>(l.b)};
}

void oklab_to_srgb(const Oklab& lab, float out_rgb[3]) {
  double o[3];
  oklab_to_srgb_d({static_cast<double>(lab.l), static_cast<double>(lab.a), static_cast<double>(lab.b)}, o);
  for (int i = 0; i < 3; ++i) out_rgb[i] = static_cast<float>(o[i]);
}

Rgba premultiply(const Rgba& c) { return {c.r * c.a, c.g * c.a, c.b * c.a, c.a}; }

Rgba unpremultiply(const Rgba& c) {
  if (c.a == 0.0f) return {0.0f, 0.0f, 0.0f, 0.0f};
  return {c.r / c.a, c.g / c.a, c.b / c.a, c.a};
}

Rgba over(const Rgba& src, const Rgba& dst) {
  const float inv = 1.0f - src.a;
  const float ao = src.a + dst.a * inv;
  if (!(ao > 0.0f)) return {0.0f, 0.0f, 0.0f, 0.0f};
  const float wd = dst.a * inv;
  return {(src.r * src.a + dst.r * wd) / ao, (src.g * src.a + dst.g * wd) / ao, (src.b * src.a + dst.b * wd) / ao, ao};
}

const char* gradient_status_text(GradientStatus s) {
  switch (s) {
    case GradientStatus::Ok: return "ok";
    case GradientStatus::TooFewStops: return "too few stops";
    case GradientStatus::TooManyStops: return "too many stops";
    case GradientStatus::BadPosition: return "bad stop position";
    case GradientStatus::BadColor: return "bad stop color";
    case GradientStatus::BadSpace: return "bad interpolation space";
  }
  return "unknown status";
}

GradientStatus validate_gradient(const Gradient& g) {
  if (g.count < 2) return GradientStatus::TooFewStops;
  if (g.count > kMaxStops) return GradientStatus::TooManyStops;
  float prev = 0.0f;
  for (std::size_t i = 0; i < g.count; ++i) {
    const float p = g.stops[i].position;
    if (!finite_in_01(p) || (i > 0 && p < prev)) return GradientStatus::BadPosition;
    prev = p;
  }
  for (std::size_t i = 0; i < g.count; ++i) {
    const Rgba& c = g.stops[i].color;
    if (!finite_in_01(c.r) || !finite_in_01(c.g) || !finite_in_01(c.b) || !finite_in_01(c.a)) {
      return GradientStatus::BadColor;
    }
  }
  const std::uint8_t sp = static_cast<std::uint8_t>(g.space);
  if (sp > static_cast<std::uint8_t>(Space::Oklab)) return GradientStatus::BadSpace;
  return GradientStatus::Ok;
}

namespace {

// Interpolation channels of a stop colour in the given space.
void to_space(Space space, const Rgba& c, double out[3]) {
  switch (space) {
    case Space::Srgb:
      out[0] = c.r; out[1] = c.g; out[2] = c.b;
      break;
    case Space::Linear:
      out[0] = srgb_to_linear_d(c.r); out[1] = srgb_to_linear_d(c.g); out[2] = srgb_to_linear_d(c.b);
      break;
    case Space::Oklab: {
      const LabD l = srgb_to_oklab_d(c.r, c.g, c.b);
      out[0] = l.l; out[1] = l.a; out[2] = l.b;
      break;
    }
  }
}

void from_space(Space space, const double in[3], double out[3]) {
  switch (space) {
    case Space::Srgb:
      for (int i = 0; i < 3; ++i) out[i] = clamp01d(in[i]);
      break;
    case Space::Linear:
      for (int i = 0; i < 3; ++i) out[i] = linear_to_srgb_d(in[i]);
      break;
    case Space::Oklab:
      oklab_to_srgb_d({in[0], in[1], in[2]}, out);
      break;
  }
}

}  // namespace

Rgba sample_gradient(const Gradient& g, float t) {
  if (validate_gradient(g) != GradientStatus::Ok) return {0.0f, 0.0f, 0.0f, 0.0f};
  if (!(t > 0.0f)) t = 0.0f;  // also NaN
  if (t > 1.0f) t = 1.0f;
  if (t < g.stops[0].position) return g.stops[0].color;
  // Last stop with position <= t: the later of equal positions wins.
  std::size_t i = 0;
  while (i + 1 < g.count && g.stops[i + 1].position <= t) ++i;
  if (i + 1 >= g.count) return g.stops[g.count - 1].color;
  const Stop& a = g.stops[i];
  const Stop& b = g.stops[i + 1];  // b.position > t >= a.position, so the span is positive
  const double span = static_cast<double>(b.position) - static_cast<double>(a.position);
  const double u = (static_cast<double>(t) - static_cast<double>(a.position)) / span;
  if (u <= 0.0) return a.color;  // exactly on a stop: its own colour, no conversion round trip
  double ca[3], cb[3], mix[3], out[3];
  to_space(g.space, a.color, ca);
  to_space(g.space, b.color, cb);
  for (int k = 0; k < 3; ++k) mix[k] = ca[k] + (cb[k] - ca[k]) * u;
  from_space(g.space, mix, out);
  const double alpha = static_cast<double>(a.color.a) + (static_cast<double>(b.color.a) - static_cast<double>(a.color.a)) * u;
  return {static_cast<float>(out[0]), static_cast<float>(out[1]), static_cast<float>(out[2]),
          static_cast<float>(clamp01d(alpha))};
}

bool bake_gradient_u8(const Gradient& g, std::size_t n, std::uint8_t* out_rgba) {
  if (validate_gradient(g) != GradientStatus::Ok || n == 0 || n > 65536 || out_rgba == nullptr) return false;
  for (std::size_t i = 0; i < n; ++i) {
    const float t = n == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1);
    const Rgba c = sample_gradient(g, t);
    out_rgba[i * 4 + 0] = to_u8(c.r);
    out_rgba[i * 4 + 1] = to_u8(c.g);
    out_rgba[i * 4 + 2] = to_u8(c.b);
    out_rgba[i * 4 + 3] = to_u8(c.a);
  }
  return true;
}

std::uint64_t digest_u8(const std::uint8_t* data, std::size_t n) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  for (std::size_t i = 0; i < n; ++i) {
    h ^= data[i];
    h *= 0x100000001b3ull;
  }
  return h;
}

}  // namespace lumora::color
