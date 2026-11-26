// Tests for core/xform. Pure CPU, deterministic (own LCG, no std distributions), no timing-sensitive bounds except the
// generous perf cap. Golden digests are also written in docs/xform.md and must match on Linux and Windows.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "xform.h"

using namespace lumora::xform;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kPi = 3.14159265358979f;

float from_bits(std::uint32_t b) {
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}
std::uint32_t to_bits(float f) {
  std::uint32_t b;
  std::memcpy(&b, &f, sizeof b);
  return b;
}
float up(float f) { return from_bits(to_bits(f) + 1); }  // next float above a positive finite f
float down(float f) { return from_bits(to_bits(f) - 1); }

float fabsf_(float v) { return v < 0 ? -v : v; }
double fabs_(double v) { return v < 0 ? -v : v; }
bool near(double a, double b, double tol) { return fabs_(a - b) <= tol; }
bool same_bits(const Affine& x, const Affine& y) {
  return to_bits(x.a) == to_bits(y.a) && to_bits(x.b) == to_bits(y.b) && to_bits(x.c) == to_bits(y.c) &&
         to_bits(x.d) == to_bits(y.d) && to_bits(x.tx) == to_bits(y.tx) && to_bits(x.ty) == to_bits(y.ty);
}

struct Lcg {
  std::uint32_t s;
  std::uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
  float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }          // [0, 1)
  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
};

struct SinCosRef {
  std::uint32_t bits;
  double sin_ref, cos_ref;  // correctly rounded doubles of the exact sin and cos of the float argument
};
// Computed with 90-digit decimal arithmetic outside the code under test.
const SinCosRef kRefs[] = {
    {0x3f000000u, 0.479425538604203, 0.8775825618903728},
    {0x3f800000u, 0.8414709848078965, 0.5403023058681398},
    {0x40000000u, 0.9092974268256817, -0.4161468365471424},
    {0x40490fdbu, -8.742278000372475e-08, -0.9999999999999962},
    {0xbf800000u, -0.8414709848078965, 0.5403023058681398},
    {0x3f490fdbu, 0.7071067966408575, 0.7071067657322372},
    {0x3fc90fdbu, 0.999999999999999, -4.371139000186241e-08},
    {0x40c90fdbu, 1.7484556000744883e-07, 0.9999999999999847},
    {0x42c80000u, -0.5063656411097588, 0.8623188722876839},
    {0x447a2000u, 0.9952739571052135, 0.09710690144438526},
    {0x461c3c00u, 0.6360869563962336, -0.7716173818043345},
    {0xc61c3e00u, -0.18828574191134848, -0.9821142903923622},
    {0x461c4000u, -0.30561438888825215, -0.9521553682590148},
    {0xc61c4000u, 0.30561438888825215, -0.9521553682590148},
    {0x3a83126fu, 0.0009999998808307693, 0.9999994999999942},
    {0x3727c5acu, 9.999999747212084e-06, 0.99999999995},
    {0x45f56e00u, -0.22956816453633294, 0.9732925859326265},
    {0x3f9e0651u, 0.9440056825826441, 0.32992919126939374},
    {0xc0200000u, -0.5984721441039565, -0.8011436155469337},
    {0x40e00000u, 0.6569865987187891, 0.7539022543433046},
    {0xc46e1a2cu, 0.48499854960744854, -0.8745149552058394},
    {0x44956e54u, 0.9974271290845431, -0.07168767094951685},
    {0x460490d9u, 0.9424364098019031, -0.3343854265360547},
    {0xc42bbfe9u, -0.8469355119859565, -0.5316956258397145},
    {0x431cd351u, -0.2514498728931939, 0.9678703226269502},
    {0x44da764au, 0.8254606519095501, 0.5644596638813625},
    {0xc5c51658u, 0.9989551751049712, 0.04570074540963428},
    {0x436e2c3bu, -0.5549196723900749, 0.8319039350757346},
    {0x45225a78u, 0.4284960742376539, -0.903543642754969},
    {0x45b71c4du, -0.45047625260732477, -0.8927884104516935},
    {0xc5fdac3fu, 0.3378721341228178, 0.9411920213130223},
    {0xc575bf99u, 0.9633301356834686, 0.2683189327723073},
    {0xc102fc45u, -0.9451941512397248, -0.3265088306037313},
    {0x40c62c29u, -0.09017201581768948, 0.995926205882431},
    {0x407799ecu, -0.6647633473090778, -0.7470540088068803},
    {0xc112992au, -0.25938407031194183, -0.9657742510899789},
    {0x411a4d49u, -0.21734194013977048, -0.9760955286529492},
    {0x4114b8f5u, 0.12925975487931718, -0.9916107682798422},
    {0x40450556u, 0.0631000252914238, -0.9980072077937221},
    {0x4013eb96u, 0.7381598613569939, -0.6746258363577722},
};

// Half a float ulp at the reference value, from bit operations.
double half_ulp(double ref) {
  float f = static_cast<float>(ref);
  if (f < 0) f = -f;
  return 0.5 * static_cast<double>(up(f) - f);
}

void push(std::vector<float>& v, const Affine& m) {
  v.push_back(m.a);
  v.push_back(m.b);
  v.push_back(m.c);
  v.push_back(m.d);
  v.push_back(m.tx);
  v.push_back(m.ty);
}

Camera cam(float px, float py, float zoom, float rot, float w, float h) {
  Camera c;
  c.position = {px, py};
  c.zoom = zoom;
  c.rotation = rot;
  c.width = w;
  c.height = h;
  return c;
}

constexpr std::uint64_t kGoldenMatrices = 0x4834887ccaefa160ull;
constexpr std::uint64_t kGoldenCameras = 0xed727f6d2b619bb9ull;

std::uint64_t matrix_digest() {
  std::vector<float> v;
  const Affine r1 = rotation(1.0f), r100 = rotation(100.0f), rm3 = rotation(-3.0f);
  push(v, r1);
  push(v, r100);
  push(v, rm3);
  const Affine trs = compose_trs({10, -20}, 0.5f, {2, 3});
  push(v, trs);
  push(v, multiply(trs, r100));
  Affine inv;
  REQUIRE(invert(trs, inv));
  push(v, inv);
  REQUIRE(invert(r1, inv));
  push(v, inv);
  const Vec2 p = apply(trs, {1.5f, -2.5f});
  v.push_back(p.x);
  v.push_back(p.y);
  return digest_floats(v.data(), v.size());
}

std::uint64_t camera_digest() {
  std::vector<float> v;
  const Camera cams[] = {cam(0, 0, 1, 0, 1280, 720), cam(100, -50, 2.5f, 0.75f, 800, 600),
                         cam(-1234.5f, 99.25f, 0.125f, -2.0f, 1920, 1080), cam(0, 0, 40, 3.0f, 64, 64)};
  for (const Camera& c : cams) {
    Affine m;
    REQUIRE(camera_matrix(c, m));
    push(v, m);
    Rect r;
    REQUIRE(visible_world_bounds(c, r));
    v.push_back(r.min_x);
    v.push_back(r.min_y);
    v.push_back(r.max_x);
    v.push_back(r.max_y);
    Vec2 s, w;
    REQUIRE(world_to_screen(c, {3.0f, 4.0f}, s));
    REQUIRE(screen_to_world(c, {10.0f, 20.0f}, w));
    v.push_back(s.x);
    v.push_back(s.y);
    v.push_back(w.x);
    v.push_back(w.y);
  }
  return digest_floats(v.data(), v.size());
}

}  // namespace

TEST_CASE("digest_floats is FNV-1a 64 over little-endian float bits", "[xform][digest]") {
  REQUIRE(digest_floats(nullptr, 0) == 0xcbf29ce484222325ull);
  const float z = 0.0f, o = 1.0f;
  REQUIRE(digest_floats(&z, 1) == 0x4d25767f9dce13f5ull);
  REQUIRE(digest_floats(&o, 1) == 0x4b72477f9c5c2f98ull);
  const float two[2] = {1.0f, -2.5f};
  REQUIRE(digest_floats(two, 2) == 0x09e629ee2dfdb3f8ull);
}

TEST_CASE("sin and cos agree with high precision references", "[xform][sincos]") {
  double worst = 0;  // largest error in units of half an ulp... measured as a fraction of one ulp
  for (const SinCosRef& r : kRefs) {
    const float x = from_bits(r.bits);
    const Affine m = rotation(x);
    const double es = fabs_(static_cast<double>(m.b) - r.sin_ref), ec = fabs_(static_cast<double>(m.a) - r.cos_ref);
    INFO("x bits " << r.bits);
    // Correctly rounded results are within half an ulp of the exact value; the extra 0.1% covers the double evaluation.
    REQUIRE(es <= half_ulp(r.sin_ref) * 1.001);
    REQUIRE(ec <= half_ulp(r.cos_ref) * 1.001);
    REQUIRE(to_bits(m.d) == to_bits(m.a));
    REQUIRE(m.c == 0.0f - m.b);
    const double ws = es / (2 * half_ulp(r.sin_ref)), wc = ec / (2 * half_ulp(r.cos_ref));
    if (ws > worst) worst = ws;
    if (wc > worst) worst = wc;
  }
  INFO("max error in ulps " << worst);
  REQUIRE(worst <= 0.5005);
}

TEST_CASE("rotation: identity outside the range, exact special angles, quarter turns", "[xform][sincos]") {
  const Affine id = identity();
  REQUIRE(same_bits(rotation(0.0f), id));
  REQUIRE(same_bits(rotation(-0.0f), id));
  REQUIRE(same_bits(rotation(kNaN), id));
  REQUIRE(same_bits(rotation(kInf), id));
  REQUIRE(same_bits(rotation(-kInf), id));
  REQUIRE(same_bits(rotation(10000.5f), id));
  REQUIRE(same_bits(rotation(-1e30f), id));
  REQUIRE(!same_bits(rotation(10000.0f), id));
  REQUIRE(!same_bits(rotation(-10000.0f), id));
  // Multiples of 90 degrees: the float nearest k*pi/2 is not exactly k*pi/2, so values are near 0 and +-1 within the
  // representation error of the argument (about |x| * 6e-8).
  for (int k = -8; k <= 8; ++k) {
    const float x = static_cast<float>(k) * 1.57079632679489662f;
    const Affine m = rotation(x);
    const double tol = 1.5e-7 * (fabsf_(x) + 1.0f);
    const double cs[4] = {1, 0, -1, 0}, sn[4] = {0, 1, 0, -1};
    const int q = ((k % 4) + 4) % 4;
    REQUIRE(near(m.a, cs[q], tol));
    REQUIRE(near(m.b, sn[q], tol));
  }
  // Exactly representable results: the exact 0/1 values appear at 0, and rotation entries obey the header layout.
  const Affine r = rotation(0.5f);
  REQUIRE(r.a == r.d);
  REQUIRE(r.b == 0.0f - r.c);
  REQUIRE(r.a > 0.87758f);
  REQUIRE(r.a < 0.87759f);
  REQUIRE(r.b > 0.47942f);
  REQUIRE(r.b < 0.47943f);
  // sin^2 + cos^2 = 1 and the symmetries sin(-x) = -sin(x), cos(-x) = cos(x) hold bit for bit.
  Lcg g{7};
  for (int i = 0; i < 2000; ++i) {
    const float x = g.range(-10000.0f, 10000.0f);
    const Affine p = rotation(x), n = rotation(-x);
    REQUIRE(to_bits(p.a) == to_bits(n.a));
    REQUIRE(to_bits(p.b) == to_bits(0.0f - n.b));
    REQUIRE(near(static_cast<double>(p.a) * p.a + static_cast<double>(p.b) * p.b, 1.0, 2e-7));
  }
}

TEST_CASE("exact small integer algebra", "[xform][matrix]") {
  const Affine t = translation(3, 4), s = scaling(2, 5), id = identity();
  REQUIRE(same_bits(multiply(id, t), t));
  REQUIRE(same_bits(multiply(t, id), t));
  const Affine ts = multiply(t, s);  // scale first, then translate
  REQUIRE(ts.a == 2);
  REQUIRE(ts.b == 0);
  REQUIRE(ts.c == 0);
  REQUIRE(ts.d == 5);
  REQUIRE(ts.tx == 3);
  REQUIRE(ts.ty == 4);
  const Affine st = multiply(s, t);  // translate first, then scale
  REQUIRE(st.tx == 6);
  REQUIRE(st.ty == 20);
  const Vec2 p = apply(ts, {7, -2});
  REQUIRE(p.x == 17);
  REQUIRE(p.y == -6);
  const Vec2 v = apply_vector(ts, {7, -2});
  REQUIRE(v.x == 14);
  REQUIRE(v.y == -10);
  const Affine a{1, 2, 3, 4, 5, 6}, b{7, 8, 9, 10, 11, 12}, c{-1, 0, 2, 3, -4, 1};
  const Affine ab = multiply(a, b);
  REQUIRE(ab.a == 1 * 7 + 3 * 8);
  REQUIRE(ab.b == 2 * 7 + 4 * 8);
  REQUIRE(ab.c == 1 * 9 + 3 * 10);
  REQUIRE(ab.d == 2 * 9 + 4 * 10);
  REQUIRE(ab.tx == 1 * 11 + 3 * 12 + 5);
  REQUIRE(ab.ty == 2 * 11 + 4 * 12 + 6);
  REQUIRE(same_bits(multiply(multiply(a, b), c), multiply(a, multiply(b, c))));
  // apply(multiply(a, b), p) = apply(a, apply(b, p)) for small integers.
  const Vec2 q = {3, -5};
  const Vec2 l = apply(multiply(a, b), q), r = apply(a, apply(b, q));
  REQUIRE(l.x == r.x);
  REQUIRE(l.y == r.y);
  REQUIRE(determinant(a) == -2.0);
  REQUIRE(determinant(id) == 1.0);
  REQUIRE(determinant(multiply(a, b)) == determinant(a) * determinant(b));
  // compose_trs with angle 0 is T * S exactly.
  REQUIRE(same_bits(compose_trs({3, 4}, 0.0f, {2, 5}), ts));
  // Order: scale first, rotate, translate last. A point (1, 0) scaled by 2 and rotated by pi/2 ends near (0, 2) + t.
  const Affine trs = compose_trs({10, 20}, 1.57079632679489662f, {2, 3});
  const Vec2 o = apply(trs, {1, 0});
  REQUIRE(near(o.x, 10, 1e-6));
  REQUIRE(near(o.y, 22, 1e-6));
  const Vec2 o2 = apply(trs, {0, 1});  // (0, 3) rotated: (-3, 0)
  REQUIRE(near(o2.x, 7, 1e-6));
  REQUIRE(near(o2.y, 20, 1e-6));
}

TEST_CASE("determinant is exact", "[xform][matrix]") {
  const float e = from_bits(0x34000000u);  // 2^-23
  Affine m;
  m.a = 1.0f + e;
  m.d = 1.0f + e;
  m.b = 1.0f + e;
  m.c = 1.0f + e;
  REQUIRE(determinant(m) == 0.0);
  Affine inv;
  const Affine keep{9, 9, 9, 9, 9, 9};
  inv = keep;
  REQUIRE(!invert(m, inv));
  REQUIRE(same_bits(inv, keep));
  // (1 + 2^-23)(1 - 2^-23) - 1 = -2^-46: float arithmetic would give 0, the exact value is kept.
  Affine n;
  n.a = 1.0f + e;
  n.d = 1.0f - e;
  n.b = 1.0f;
  n.c = 1.0f;
  const double det = determinant(n);
  REQUIRE(det == -1.0 / 70368744177664.0);  // -2^-46 is not a float-sized quantity near 0 of this form
  REQUIRE(det != 0.0);
  Affine r;
  REQUIRE(invert(n, r));
  // Large and tiny values stay exact: 2^100 * 2^-100 - 0 * 0 = 1.
  Affine h;
  h.a = from_bits(0x71800000u);  // 2^100
  h.d = from_bits(0x0d800000u);  // 2^-100
  REQUIRE(determinant(h) == 1.0);
}

TEST_CASE("invert: failure rules and round trips", "[xform][matrix]") {
  const Affine keep{9, 8, 7, 6, 5, 4};
  Affine out = keep;
  REQUIRE(!invert(Affine{1, 2, 2, 4, 0, 0}, out));
  REQUIRE(same_bits(out, keep));
  REQUIRE(!invert(Affine{0, 0, 0, 0, 0, 0}, out));
  REQUIRE(!invert(Affine{kNaN, 0, 0, 1, 0, 0}, out));
  REQUIRE(!invert(Affine{1, kInf, 0, 1, 0, 0}, out));
  REQUIRE(!invert(Affine{1, 0, 0, 1, kNaN, 0}, out));
  REQUIRE(!invert(Affine{1, 0, 0, 1, 0, -kInf}, out));
  REQUIRE(same_bits(out, keep));
  // The inverse would overflow: 1 / 1e-40 is beyond the float range.
  REQUIRE(!invert(Affine{1e-40f, 0, 0, 1, 0, 0}, out));
  REQUIRE(!invert(Affine{1, 0, 0, 1e-40f, 0, 0}, out));
  REQUIRE(same_bits(out, keep));
  // Small integer example is exact.
  REQUIRE(invert(Affine{2, 0, 0, 4, 6, 8}, out));
  REQUIRE(out.a == 0.5f);
  REQUIRE(out.d == 0.25f);
  REQUIRE(out.tx == -3.0f);
  REQUIRE(out.ty == -2.0f);
  // Round trip tolerance: |apply(inv, apply(m, p)) - p| <= 1e-5 * (1 + |p|) for well conditioned m (documented).
  Lcg g{99};
  for (int i = 0; i < 3000; ++i) {
    const Affine m = compose_trs({g.range(-500, 500), g.range(-500, 500)}, g.range(-10, 10),
                                 {g.range(0.25f, 4.0f), g.range(0.25f, 4.0f)});
    Affine inv;
    REQUIRE(invert(m, inv));
    const Vec2 p = {g.range(-1000, 1000), g.range(-1000, 1000)};
    const Vec2 q = apply(inv, apply(m, p));
    REQUIRE(near(q.x, p.x, 1e-5 * (1 + fabsf_(p.x) + fabsf_(p.y))));
    REQUIRE(near(q.y, p.y, 1e-5 * (1 + fabsf_(p.x) + fabsf_(p.y))));
  }
}

TEST_CASE("apply and multiply: non-finite values propagate", "[xform][matrix]") {
  const Vec2 a = apply(identity(), {kNaN, 1});
  REQUIRE(a.x != a.x);
  REQUIRE(a.y != a.y);  // 0 * NaN is NaN, so both outputs are NaN
  const Vec2 b = apply(scaling(kInf, 1), {0, 1});  // inf * 0 = NaN
  REQUIRE(b.x != b.x);
  const Affine m = multiply(translation(kInf, 0), identity());
  REQUIRE(m.tx == kInf);
  const Vec2 big = apply(scaling(1e30f, 1e30f), {1e30f, -1e30f});  // overflows the float range
  REQUIRE(big.x == kInf);
  REQUIRE(big.y == -kInf);
  // No negative zero comes out of the arithmetic.
  const Vec2 z = apply(scaling(-1, -1), {0, 0});
  REQUIRE(to_bits(z.x) == 0u);
  REQUIRE(to_bits(z.y) == 0u);
}

TEST_CASE("camera: every status and boundary", "[xform][camera]") {
  REQUIRE(validate_camera(Camera{}) == CameraStatus::Ok);
  const float big = 1e6f;
  REQUIRE(validate_camera(cam(0, 0, 1, 0, big, big)) == CameraStatus::Ok);
  REQUIRE(validate_camera(cam(0, 0, 1, 0, up(big), 10)) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(0, 0, 1, 0, 10, up(big))) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(0, 0, 1, 0, 0, 10)) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(0, 0, 1, 0, 10, -1)) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(0, 0, 1, 0, kNaN, 10)) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(0, 0, 1, 0, 10, kInf)) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(0, 0, 1e6f, 0, 10, 10)) == CameraStatus::Ok);
  REQUIRE(validate_camera(cam(0, 0, 1e-6f, 0, 10, 10)) == CameraStatus::Ok);
  REQUIRE(validate_camera(cam(0, 0, up(1e6f), 0, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(0, 0, down(1e-6f), 0, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(0, 0, 0, 0, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(0, 0, -1, 0, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(0, 0, kNaN, 0, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(0, 0, kInf, 0, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(1e9f, -1e9f, 1, 0, 10, 10)) == CameraStatus::Ok);
  REQUIRE(validate_camera(cam(up(1e9f), 0, 1, 0, 10, 10)) == CameraStatus::BadPosition);
  REQUIRE(validate_camera(cam(0, -up(1e9f), 1, 0, 10, 10)) == CameraStatus::BadPosition);
  REQUIRE(validate_camera(cam(kNaN, 0, 1, 0, 10, 10)) == CameraStatus::BadPosition);
  REQUIRE(validate_camera(cam(0, -kInf, 1, 0, 10, 10)) == CameraStatus::BadPosition);
  REQUIRE(validate_camera(cam(0, 0, 1, 10000.0f, 10, 10)) == CameraStatus::Ok);
  REQUIRE(validate_camera(cam(0, 0, 1, -10000.0f, 10, 10)) == CameraStatus::Ok);
  REQUIRE(validate_camera(cam(0, 0, 1, up(10000.0f), 10, 10)) == CameraStatus::BadRotation);
  REQUIRE(validate_camera(cam(0, 0, 1, kNaN, 10, 10)) == CameraStatus::BadRotation);
  REQUIRE(validate_camera(cam(0, 0, 1, -kInf, 10, 10)) == CameraStatus::BadRotation);
  // Checks run in enum order: viewport, zoom, position, rotation.
  REQUIRE(validate_camera(cam(kNaN, 0, 0, kNaN, 0, 10)) == CameraStatus::BadViewport);
  REQUIRE(validate_camera(cam(kNaN, 0, 0, kNaN, 10, 10)) == CameraStatus::BadZoom);
  REQUIRE(validate_camera(cam(kNaN, 0, 1, kNaN, 10, 10)) == CameraStatus::BadPosition);
  REQUIRE(validate_camera(cam(0, 0, 1, kNaN, 10, 10)) == CameraStatus::BadRotation);
  const CameraStatus all[] = {CameraStatus::Ok, CameraStatus::BadViewport, CameraStatus::BadZoom, CameraStatus::BadPosition,
                              CameraStatus::BadRotation};
  for (std::size_t i = 0; i < 5; ++i) {
    REQUIRE(std::strlen(camera_status_text(all[i])) > 0);
    for (std::size_t j = i + 1; j < 5; ++j) REQUIRE(std::strcmp(camera_status_text(all[i]), camera_status_text(all[j])) != 0);
  }
  // An invalid camera leaves outputs untouched and returns false.
  const Camera bad = cam(0, 0, 0, 0, 10, 10);
  Affine m = Affine{9, 9, 9, 9, 9, 9};
  Vec2 v = {5, 6}, w = {5, 6};
  Rect r = {1, 2, 3, 4};
  REQUIRE(!camera_matrix(bad, m));
  REQUIRE(!world_to_screen(bad, {0, 0}, v));
  REQUIRE(!screen_to_world(bad, {0, 0}, w));
  REQUIRE(!visible_world_bounds(bad, r));
  REQUIRE(m.a == 9);
  REQUIRE(v.x == 5);
  REQUIRE(w.y == 6);
  REQUIRE(r.max_y == 4);
}

TEST_CASE("camera: conventions and exact cases", "[xform][camera]") {
  // Zero rotation, zoom 2, viewport 100 x 50, position (10, 20): world y up, screen y down.
  const Camera c = cam(10, 20, 2, 0, 100, 50);
  Vec2 s;
  REQUIRE(world_to_screen(c, {10, 20}, s));  // the position is the viewport center
  REQUIRE(s.x == 50);
  REQUIRE(s.y == 25);
  REQUIRE(world_to_screen(c, {11, 20}, s));  // +x world is to the right
  REQUIRE(s.x == 52);
  REQUIRE(s.y == 25);
  REQUIRE(world_to_screen(c, {10, 21}, s));  // +y world is up on screen (smaller y)
  REQUIRE(s.x == 50);
  REQUIRE(s.y == 23);
  Affine m;
  REQUIRE(camera_matrix(c, m));
  REQUIRE(m.a == 2);
  REQUIRE(m.b == 0);
  REQUIRE(m.c == 0);
  REQUIRE(m.d == -2);
  REQUIRE(m.tx == 30);
  REQUIRE(m.ty == 65);
  Vec2 w;
  REQUIRE(screen_to_world(c, {0, 0}, w));
  REQUIRE(w.x == -15);
  REQUIRE(w.y == 32.5f);
  Rect r;
  REQUIRE(visible_world_bounds(c, r));
  REQUIRE(r.min_x == -15);
  REQUIRE(r.max_x == 35);
  REQUIRE(r.min_y == 7.5f);
  REQUIRE(r.max_y == 32.5f);
  // Rotation +90 degrees: the camera frame turns counter-clockwise, so world +y appears toward screen right.
  const Camera q = cam(0, 0, 10, 1.57079632679489662f, 200, 100);
  REQUIRE(world_to_screen(q, {0, 1}, s));
  REQUIRE(near(s.x, 110, 1e-4));
  REQUIRE(near(s.y, 50, 1e-4));
  REQUIRE(world_to_screen(q, {1, 0}, s));  // world +x appears toward screen down
  REQUIRE(near(s.x, 100, 1e-4));
  REQUIRE(near(s.y, 60, 1e-4));
  // 180 degrees: both axes flip.
  const Camera h = cam(0, 0, 1, kPi, 100, 100);
  REQUIRE(world_to_screen(h, {5, 7}, s));
  REQUIRE(near(s.x, 45, 1e-5));
  REQUIRE(near(s.y, 57, 1e-5));
}

TEST_CASE("camera: round trips across zooms and rotations", "[xform][camera]") {
  const float zooms[] = {1e-3f, 0.01f, 0.5f, 1.0f, 2.0f, 37.0f, 1000.0f};
  const float rots[] = {0.0f, 0.3f, 1.5707964f, 3.0f, -2.2f, 100.0f, -9999.0f, 10000.0f};
  Lcg g{2024};
  for (float z : zooms) {
    for (float rot : rots) {
      const Camera c = cam(g.range(-1000, 1000), g.range(-1000, 1000), z, rot, 1280, 720);
      for (int i = 0; i < 20; ++i) {
        const Vec2 p = {c.position.x + g.range(-300, 300) / z, c.position.y + g.range(-300, 300) / z};
        Vec2 s, back;
        REQUIRE(world_to_screen(c, p, s));
        REQUIRE(screen_to_world(c, s, back));
        // Float rounding of the matrix and of the screen position: relative to the world extent of the view.
        const double tol = 1e-5 * (1.0 + fabsf_(c.position.x) + fabsf_(c.position.y)) + 4e-6 * 1500.0 / z;
        INFO("zoom " << z << " rot " << rot);
        REQUIRE(near(back.x, p.x, tol));
        REQUIRE(near(back.y, p.y, tol));
      }
    }
  }
}

TEST_CASE("bounds enclose the transformed corners", "[xform][camera]") {
  Lcg g{31337};
  for (int i = 0; i < 4000; ++i) {
    const float z = g.range(0.01f, 50.0f), rot = g.range(-20.0f, 20.0f), w = g.range(1, 4000), h = g.range(1, 4000);
    const Camera c = cam(g.range(-5000, 5000), g.range(-5000, 5000), z, rot, w, h);
    Rect r;
    REQUIRE(visible_world_bounds(c, r));
    REQUIRE(r.min_x <= r.max_x);
    REQUIRE(r.min_y <= r.max_y);
    const Vec2 corners[4] = {{0, 0}, {w, 0}, {0, h}, {w, h}};
    for (const Vec2& s : corners) {
      Vec2 world;
      REQUIRE(screen_to_world(c, s, world));
      REQUIRE(world.x >= r.min_x);
      REQUIRE(world.x <= r.max_x);
      REQUIRE(world.y >= r.min_y);
      REQUIRE(world.y <= r.max_y);
    }
  }
}

TEST_CASE("transform_rect: exact values and outward rounding", "[xform][rect]") {
  const Rect r = {1, 2, 3, 5};
  const Rect s = transform_rect(scaling(2, 3), r);
  REQUIRE(s.min_x == 2);
  REQUIRE(s.min_y == 6);
  REQUIRE(s.max_x == 6);
  REQUIRE(s.max_y == 15);
  const Rect n = transform_rect(scaling(-1, 1), r);  // a mirror still gives ordered bounds
  REQUIRE(n.min_x == -3);
  REQUIRE(n.max_x == -1);
  const Rect t = transform_rect(translation(10, -10), r);
  REQUIRE(t.min_x == 11);
  REQUIRE(t.max_y == -5);
  const Rect id = transform_rect(identity(), r);
  REQUIRE(id.min_x == 1);
  REQUIRE(id.max_y == 5);
  // The exact result 1 + 1e-8 is between two floats: the bounds hold both neighbours, nearest rounding would not.
  const Rect o = transform_rect(translation(1e-8f, 0), Rect{1, 0, 1, 0});
  REQUIRE(o.min_x == 1.0f);
  REQUIRE(o.max_x == up(1.0f));
  const Rect o2 = transform_rect(translation(-1e-8f, 0), Rect{1, 0, 1, 0});
  REQUIRE(o2.max_x == 1.0f);
  REQUIRE(o2.min_x == down(1.0f));
  // A rotated rect encloses its four transformed corners (checked with the exact corner arithmetic of apply).
  Lcg g{5};
  for (int i = 0; i < 3000; ++i) {
    const Affine m = compose_trs({g.range(-100, 100), g.range(-100, 100)}, g.range(-8, 8), {g.range(0.1f, 5), g.range(0.1f, 5)});
    const Rect in = {g.range(-50, 0), g.range(-50, 0), g.range(0, 50), g.range(0, 50)};
    const Rect out = transform_rect(m, in);
    const Vec2 cs[4] = {{in.min_x, in.min_y}, {in.max_x, in.min_y}, {in.min_x, in.max_y}, {in.max_x, in.max_y}};
    for (const Vec2& p : cs) {
      const Vec2 q = apply(m, p);
      REQUIRE(q.x >= out.min_x);
      REQUIRE(q.x <= out.max_x);
      REQUIRE(q.y >= out.min_y);
      REQUIRE(q.y <= out.max_y);
    }
  }
  // Non-finite input: every bound is NaN.
  const Rect bad = transform_rect(identity(), Rect{kNaN, 0, 1, 1});
  REQUIRE(bad.min_x != bad.min_x);
  REQUIRE(bad.max_y != bad.max_y);
  const Rect bad2 = transform_rect(Affine{1, 0, 0, 1, kInf, 0}, Rect{0, 0, 1, 1});
  REQUIRE(bad2.min_x != bad2.min_x);
  REQUIRE(bad2.min_y != bad2.min_y);
  // Beyond the float range the bounds turn infinite on the outside only.
  const Rect huge = transform_rect(scaling(3e38f, 1), Rect{0, 0, 10, 1});
  REQUIRE(huge.min_x == 0.0f);
  REQUIRE(huge.max_x == kInf);
}

TEST_CASE("golden digests of matrices and cameras", "[xform][digest]") {
  const std::uint64_t m = matrix_digest(), c = camera_digest();
  INFO("matrices " << std::hex << m << " cameras " << c);
  REQUIRE(m == kGoldenMatrices);
  REQUIRE(c == kGoldenCameras);
}

TEST_CASE("performance: rotations, matrices and camera queries", "[xform][perf]") {
  const auto t0 = std::chrono::steady_clock::now();
  Lcg g{1};
  std::vector<float> acc;
  acc.reserve(1024);
  float sum = 0;
  for (int i = 0; i < 200000; ++i) {
    const Camera c = cam(g.range(-100, 100), g.range(-100, 100), g.range(0.1f, 10), g.range(-100, 100), 1280, 720);
    Vec2 s, w;
    Rect r;
    REQUIRE(world_to_screen(c, {g.range(-100, 100), g.range(-100, 100)}, s));
    REQUIRE(screen_to_world(c, s, w));
    REQUIRE(visible_world_bounds(c, r));
    sum += w.x + r.max_x;
  }
  REQUIRE(sum == sum);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  INFO("seconds " << secs);
  REQUIRE(secs < 10.0);
}
