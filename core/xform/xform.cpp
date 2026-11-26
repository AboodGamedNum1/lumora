// 2D transforms and orthographic camera. Conventions, sin/cos error, rounding rules and golden digests: docs/xform.md.
// Determinism: only + - * / and comparisons on floats and doubles; no libm function is called. Float*float products
// are exact in double, so every entry is one double expression converted to float once. Compilers are asked not to
// fuse multiply-add; the project must not enable fast-math.
#include "xform.h"

#include <cstring>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::xform {
namespace {

constexpr double kMaxRadians = 1e4;
constexpr float kFloatMax = 3.4028234663852886e38f;

inline bool is_finite(float v) { return v - v == 0.0f; }  // false for NaN and infinity
inline bool is_finite(double v) { return v - v == 0.0; }
inline bool is_nan(double v) { return v != v; }
inline bool all_finite(const Affine& m) {
  return is_finite(m.a) && is_finite(m.b) && is_finite(m.c) && is_finite(m.d) && is_finite(m.tx) && is_finite(m.ty);
}

// Conversion to float, once. Adding 0.0 turns -0 into +0, so results never carry a negative zero.
inline float to_float(double v) { return static_cast<float>(v + 0.0); }

inline std::uint32_t bits_of(float f) {
  std::uint32_t u;
  std::memcpy(&u, &f, sizeof u);
  return u;
}
inline float float_of(std::uint32_t u) {
  float f;
  std::memcpy(&f, &u, sizeof f);
  return f;
}
// Neighbouring floats for finite f (and +-infinity handled by the callers).
inline float next_up(float f) {
  std::uint32_t u = bits_of(f);
  if ((u & 0x7fffffffu) == 0) return float_of(1u);  // +-0 -> smallest positive
  return float_of((u >> 31) ? u - 1 : u + 1);
}
inline float next_down(float f) {
  std::uint32_t u = bits_of(f);
  if ((u & 0x7fffffffu) == 0) return float_of(0x80000001u);
  return float_of((u >> 31) ? u + 1 : u - 1);
}

// ---------------------------------------------------------------------------------------------------------------
// Exact sums (Shewchuk). two_sum is exact unless it overflows; the values here stay far below 2^270.
inline void two_sum(double a, double b, double& x, double& y) {
  x = a + b;
  const double bv = x - a;
  const double av = x - bv;
  const double br = b - bv;
  const double ar = a - av;
  y = ar + br;
}

struct Expansion {  // nonoverlapping, increasing magnitude, no zero components; at most 8 components here
  double c[8];
  int n = 0;
  void add(double b) {
    if (b == 0.0) return;
    int out = 0;
    double q = b;
    for (int i = 0; i < n; ++i) {
      double nq, h;
      two_sum(q, c[i], nq, h);
      if (h != 0.0) c[out++] = h;
      q = nq;
    }
    n = out;
    if (q != 0.0) c[n++] = q;
  }
  int sign() const { return n == 0 ? 0 : (c[n - 1] > 0.0 ? 1 : -1); }  // the largest component decides
};

// Outward rounding of the exact value p + q + t to float: lo <= exact <= hi, both floats, tightest possible.
// p and q are exact products of floats, t is a float, so everything is exact in double until the final comparison.
void bounds3(double p, double q, double t, float& lo, float& hi) {
  double s1, e1, s2, e2;
  two_sum(p, q, s1, e1);
  two_sum(s1, t, s2, e2);
  (void)e1;
  (void)e2;
  const float f = static_cast<float>(s2);  // nearest float of the leading part
  if (is_nan(s2)) {
    lo = hi = f;
    return;
  }
  if (!is_finite(f)) {  // beyond the float range
    if (f > 0) {
      lo = kFloatMax;
      hi = f;
    } else {
      lo = f;
      hi = -kFloatMax;
    }
    return;
  }
  Expansion e;  // exact value minus f
  e.add(-static_cast<double>(f));
  e.add(p);
  e.add(q);
  e.add(t);
  const int sg = e.sign();
  lo = sg >= 0 ? f : next_down(f);
  hi = sg <= 0 ? f : next_up(f);
  lo = lo + 0.0f;
  hi = hi + 0.0f;
}

// ---------------------------------------------------------------------------------------------------------------
// sin and cos from + - * /. Reduction x = k*(pi/2) + r with pi/2 split in three parts: P1 and P2 have 33 significant
// bits, so k*P1 and k*P2 are exact for |k| < 2^20 (here |k| <= 6367); P3 is the rest (error below 2^-122 relative).
// Then Taylor polynomials on |r| <= pi/4 + tiny, evaluated by Horner.
constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;
constexpr double kPio2Hi = 0x1.921fb54400000p+0;
constexpr double kPio2Mid = 0x1.0b4611a600000p-34;
constexpr double kPio2Lo = 0x1.3198a2e037073p-69;

double sin_poly(double r) {
  const double r2 = r * r;
  double p = -0x1.2f49b46814157p-57;  // -1/19!
  p = p * r2 + 0x1.952c77030ad4ap-49;       // +1/17!
  p = p * r2 + -0x1.ae7f3e733b81fp-41;
  p = p * r2 + 0x1.6124613a86d09p-33;
  p = p * r2 + -0x1.ae64567f544e4p-26;
  p = p * r2 + 0x1.71de3a556c734p-19;
  p = p * r2 + -0x1.a01a01a01a01ap-13;
  p = p * r2 + 0x1.1111111111111p-7;
  p = p * r2 + -0x1.5555555555555p-3;
  return r + r * (r2 * p);
}

double cos_poly(double r) {
  const double r2 = r * r;
  double p = 0x1.e542ba4020225p-62;  // +1/20!
  p = p * r2 + -0x1.6827863b97d97p-53;
  p = p * r2 + 0x1.ae7f3e733b81fp-45;
  p = p * r2 + -0x1.93974a8c07c9dp-37;
  p = p * r2 + 0x1.1eed8eff8d898p-29;
  p = p * r2 + -0x1.27e4fb7789f5cp-22;
  p = p * r2 + 0x1.a01a01a01a01ap-16;
  p = p * r2 + -0x1.6c16c16c16c17p-10;
  p = p * r2 + 0x1.5555555555555p-5;
  p = p * r2 + -0x1.0000000000000p-1;
  return 1.0 + r2 * p;
}

// Valid for finite |x| <= 1e4. Results are doubles (before the final float rounding).
void sincos_double(double x, double& s, double& c) {
  const double q = x * kTwoOverPi;
  const long long k = static_cast<long long>(q >= 0 ? q + 0.5 : q - 0.5);
  const double kd = static_cast<double>(k);
  const double r = ((x - kd * kPio2Hi) - kd * kPio2Mid) - kd * kPio2Lo;
  const double sr = sin_poly(r), cr = cos_poly(r);
  switch (static_cast<int>(((k % 4) + 4) % 4)) {
    case 0: s = sr; c = cr; break;
    case 1: s = cr; c = -sr; break;
    case 2: s = -sr; c = -cr; break;
    default: s = -cr; c = sr; break;
  }
}

// Float sine and cosine of a validated angle (finite, |radians| <= 1e4).
void sincos_float(float radians, float& s, float& c) {
  double sd, cd;
  sincos_double(static_cast<double>(radians), sd, cd);
  s = to_float(sd);
  c = to_float(cd);
}

bool good_angle(float r) { return is_finite(r) && r <= 1e4f && r >= -1e4f; }

}  // namespace

Affine identity() { return Affine{}; }

Affine translation(float tx, float ty) {
  Affine m;
  m.tx = tx;
  m.ty = ty;
  return m;
}

Affine scaling(float sx, float sy) {
  Affine m;
  m.a = sx;
  m.d = sy;
  return m;
}

Affine rotation(float radians) {
  if (!good_angle(radians)) return Affine{};
  float s, c;
  sincos_float(radians, s, c);
  Affine m;
  m.a = c;
  m.b = s;
  m.c = 0.0f - s;
  m.d = c;
  return m;
}

Affine multiply(const Affine& l, const Affine& r) {
  const double la = l.a, lb = l.b, lc = l.c, ld = l.d, ltx = l.tx, lty = l.ty;
  const double ra = r.a, rb = r.b, rc = r.c, rd = r.d, rtx = r.tx, rty = r.ty;
  Affine m;
  m.a = to_float(la * ra + lc * rb);
  m.b = to_float(lb * ra + ld * rb);
  m.c = to_float(la * rc + lc * rd);
  m.d = to_float(lb * rc + ld * rd);
  m.tx = to_float(la * rtx + lc * rty + ltx);
  m.ty = to_float(lb * rtx + ld * rty + lty);
  return m;
}

Vec2 apply(const Affine& m, Vec2 p) {
  const double x = p.x, y = p.y;
  return Vec2{to_float(static_cast<double>(m.a) * x + static_cast<double>(m.c) * y + static_cast<double>(m.tx)),
              to_float(static_cast<double>(m.b) * x + static_cast<double>(m.d) * y + static_cast<double>(m.ty))};
}

Vec2 apply_vector(const Affine& m, Vec2 v) {
  const double x = v.x, y = v.y;
  return Vec2{to_float(static_cast<double>(m.a) * x + static_cast<double>(m.c) * y),
              to_float(static_cast<double>(m.b) * x + static_cast<double>(m.d) * y)};
}

double determinant(const Affine& m) {
  // Both products are exact in double (48 bits), so one subtraction rounds the exact determinant once.
  return static_cast<double>(m.a) * static_cast<double>(m.d) - static_cast<double>(m.b) * static_cast<double>(m.c);
}

bool invert(const Affine& m, Affine& out) {
  if (!all_finite(m)) return false;
  const double det = determinant(m);
  if (det == 0.0) return false;  // exact: a double difference of exact products is zero only if the exact value is
  const double a = m.a, b = m.b, c = m.c, d = m.d, tx = m.tx, ty = m.ty;
  Affine r;
  r.a = to_float(d / det);
  r.b = to_float(-b / det);
  r.c = to_float(-c / det);
  r.d = to_float(a / det);
  r.tx = to_float((c * ty - d * tx) / det);
  r.ty = to_float((b * tx - a * ty) / det);
  if (!all_finite(r)) return false;
  out = r;
  return true;
}

Affine compose_trs(Vec2 t, float radians, Vec2 s) {
  float sn = 0.0f, cs = 1.0f;
  if (good_angle(radians)) sincos_float(radians, sn, cs);
  Affine m;
  m.a = to_float(static_cast<double>(cs) * static_cast<double>(s.x));
  m.b = to_float(static_cast<double>(sn) * static_cast<double>(s.x));
  m.c = to_float(-static_cast<double>(sn) * static_cast<double>(s.y));
  m.d = to_float(static_cast<double>(cs) * static_cast<double>(s.y));
  m.tx = t.x;
  m.ty = t.y;
  return m;
}

const char* camera_status_text(CameraStatus s) {
  switch (s) {
    case CameraStatus::Ok: return "ok";
    case CameraStatus::BadViewport: return "bad viewport";
    case CameraStatus::BadZoom: return "bad zoom";
    case CameraStatus::BadPosition: return "bad position";
    case CameraStatus::BadRotation: return "bad rotation";
  }
  return "unknown";
}

CameraStatus validate_camera(const Camera& c) {
  auto bad_size = [](float v) { return !is_finite(v) || v <= 0.0f || v > 1e6f; };
  if (bad_size(c.width) || bad_size(c.height)) return CameraStatus::BadViewport;
  if (bad_size(c.zoom) || c.zoom < 1e-6f) return CameraStatus::BadZoom;
  auto bad_pos = [](float v) { return !is_finite(v) || v > 1e9f || v < -1e9f; };
  if (bad_pos(c.position.x) || bad_pos(c.position.y)) return CameraStatus::BadPosition;
  if (!good_angle(c.rotation)) return CameraStatus::BadRotation;
  return CameraStatus::Ok;
}

bool camera_matrix(const Camera& cam, Affine& out) {
  if (validate_camera(cam) != CameraStatus::Ok) return false;
  float sn, cs;
  sincos_float(cam.rotation, sn, cs);
  const double z = cam.zoom, c = cs, s = sn;
  const double a = z * c, b = z * s, cc = z * s, d = -(z * c);
  const double px = cam.position.x, py = cam.position.y;
  Affine m;
  m.a = to_float(a);
  m.b = to_float(b);
  m.c = to_float(cc);
  m.d = to_float(d);
  m.tx = to_float(static_cast<double>(cam.width) * 0.5 - (a * px + cc * py));
  m.ty = to_float(static_cast<double>(cam.height) * 0.5 - (b * px + d * py));
  out = m;
  return true;
}

bool world_to_screen(const Camera& cam, Vec2 world, Vec2& out) {
  Affine m;
  if (!camera_matrix(cam, m)) return false;
  out = apply(m, world);
  return true;
}

bool screen_to_world(const Camera& cam, Vec2 screen, Vec2& out) {
  Affine m, inv;
  if (!camera_matrix(cam, m) || !invert(m, inv)) return false;
  out = apply(inv, screen);
  return true;
}

bool visible_world_bounds(const Camera& cam, Rect& out) {
  Affine m, inv;
  if (!camera_matrix(cam, m) || !invert(m, inv)) return false;
  out = transform_rect(inv, Rect{0.0f, 0.0f, cam.width, cam.height});
  return true;
}

Rect transform_rect(const Affine& m, const Rect& r) {
  const bool ok = all_finite(m) && is_finite(r.min_x) && is_finite(r.min_y) && is_finite(r.max_x) && is_finite(r.max_y);
  if (!ok) {
    const float q = float_of(0x7fc00000u);  // quiet NaN from its bit pattern
    return Rect{q, q, q, q};
  }
  const float xs[4] = {r.min_x, r.max_x, r.min_x, r.max_x}, ys[4] = {r.min_y, r.min_y, r.max_y, r.max_y};
  Rect o{0, 0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    const double x = xs[i], y = ys[i];
    float xl, xh, yl, yh;
    bounds3(static_cast<double>(m.a) * x, static_cast<double>(m.c) * y, m.tx, xl, xh);
    bounds3(static_cast<double>(m.b) * x, static_cast<double>(m.d) * y, m.ty, yl, yh);
    if (i == 0 || xl < o.min_x) o.min_x = xl;
    if (i == 0 || xh > o.max_x) o.max_x = xh;
    if (i == 0 || yl < o.min_y) o.min_y = yl;
    if (i == 0 || yh > o.max_y) o.max_y = yh;
  }
  return o;
}

std::uint64_t digest_floats(const float* v, std::size_t n) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint32_t u = bits_of(v[i]);
    for (int k = 0; k < 4; ++k) {
      h ^= (u >> (8 * k)) & 0xffu;
      h *= 0x100000001b3ull;
    }
  }
  return h;
}

}  // namespace lumora::xform
