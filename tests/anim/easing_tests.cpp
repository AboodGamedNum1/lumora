// Tests for core/anim/easing.cpp: easing kinds, names, CSS cubic Bezier and tween_value. Pure CPU.
// Reference values in the table come from a separate Python implementation of the published formulas in
// docs/easing.md. The default tolerance is 1e-6 relative (plus 1e-6 absolute near zero).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "easing.h"

using Catch::Approx;
using namespace lumora::anim;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr double kTol = 1e-6;

const Ease kAll[] = {
    Ease::Linear,
    Ease::QuadIn, Ease::QuadOut, Ease::QuadInOut,
    Ease::CubicIn, Ease::CubicOut, Ease::CubicInOut,
    Ease::SineIn, Ease::SineOut, Ease::SineInOut,
    Ease::ExpoIn, Ease::ExpoOut, Ease::ExpoInOut,
    Ease::BackIn, Ease::BackOut, Ease::BackInOut,
    Ease::ElasticIn, Ease::ElasticOut, Ease::ElasticInOut,
    Ease::BounceIn, Ease::BounceOut, Ease::BounceInOut,
};

const char* const kNames[] = {
    "linear",
    "quad-in", "quad-out", "quad-in-out",
    "cubic-in", "cubic-out", "cubic-in-out",
    "sine-in", "sine-out", "sine-in-out",
    "expo-in", "expo-out", "expo-in-out",
    "back-in", "back-out", "back-in-out",
    "elastic-in", "elastic-out", "elastic-in-out",
    "bounce-in", "bounce-out", "bounce-in-out",
};

bool near(double got, double want, double tol = kTol) { return std::fabs(got - want) <= tol * std::fabs(want) + tol; }

struct Rng {
  std::uint64_t s;
  std::uint64_t next() {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 2685821657736338717ull;
  }
  double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
};

bool has_overshoot(Ease k) {
  switch (k) {
    case Ease::BackIn: case Ease::BackOut: case Ease::BackInOut:
    case Ease::ElasticIn: case Ease::ElasticOut: case Ease::ElasticInOut:
    case Ease::BounceIn: case Ease::BounceOut: case Ease::BounceInOut:
      return true;
    default:
      return false;
  }
}

}  // namespace

TEST_CASE("there are 22 kinds with distinct names", "[anim][easing]") {
  static_assert(sizeof(kAll) / sizeof(kAll[0]) == 22, "22 standard kinds");
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == 22, "22 names");
  for (std::size_t i = 0; i < 22; ++i) {
    CHECK(std::string(ease_name(kAll[i])) == kNames[i]);
    for (std::size_t j = i + 1; j < 22; ++j) CHECK(std::string(kNames[i]) != kNames[j]);
  }
}

TEST_CASE("names round-trip and near misses are rejected", "[anim][easing][names]") {
  for (std::size_t i = 0; i < 22; ++i) {
    Ease e = Ease::Linear;
    Ease before = Ease::BounceOut;
    e = before;
    REQUIRE(ease_from_name(kNames[i], e));
    CHECK(e == kAll[i]);
    // Every near miss leaves `out` untouched.
    const std::string good = kNames[i];
    std::vector<std::string> bad;
    std::string upper = good;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    bad.push_back(upper);
    std::string title = good;
    title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    bad.push_back(title);
    bad.push_back(" " + good);
    bad.push_back(good + " ");
    bad.push_back(good + "\n");
    bad.push_back(good + std::string(1, '\0'));
    bad.push_back(std::string(1, '\0') + good);
    bad.push_back(good + "x");
    bad.push_back(good.substr(0, good.size() - 1));
    bad.push_back(good + good);
    std::string underscore = good;
    for (char& c : underscore) {
      if (c == '-') c = '_';
    }
    if (underscore != good) bad.push_back(underscore);
    std::string spaced = good;
    for (char& c : spaced) {
      if (c == '-') c = ' ';
    }
    if (spaced != good) bad.push_back(spaced);
    for (const std::string& b : bad) {
      Ease untouched = Ease::ElasticInOut;
      INFO("name of " << good << ", variant size " << b.size());
      CHECK_FALSE(ease_from_name(b, untouched));
      CHECK(untouched == Ease::ElasticInOut);
    }
  }
  Ease untouched = Ease::BackIn;
  for (const std::string& b : {std::string(), std::string(" "), std::string("\0", 1), std::string("nope"),
                               std::string(100000, 'a'), std::string("linear\0quad-in", 14), std::string("quad-in-out-in"),
                               std::string("easeInQuad"), std::string("ease")}) {
    CHECK_FALSE(ease_from_name(b, untouched));
    CHECK(untouched == Ease::BackIn);
  }
  CHECK(std::string(ease_name(static_cast<Ease>(999))) == "unknown");
  CHECK(std::string(ease_name(static_cast<Ease>(-1))) == "unknown");
}

TEST_CASE("exact endpoints, clamping and non-finite inputs for every kind", "[anim][easing]") {
  for (Ease k : kAll) {
    INFO(ease_name(k));
    CHECK(ease(k, 0.0f) == 0.0f);
    CHECK(ease(k, 1.0f) == 1.0f);
    CHECK(ease(k, -0.0f) == 0.0f);
    CHECK(ease(k, -1.0f) == 0.0f);
    CHECK(ease(k, 2.0f) == 1.0f);
    CHECK(ease(k, 1e30f) == 1.0f);
    CHECK(ease(k, -1e30f) == 0.0f);
    CHECK(ease(k, kNaN) == 0.0f);
    CHECK(ease(k, -kInf) == 0.0f);
    CHECK(ease(k, kInf) == 1.0f);
    CHECK(ease(k, std::numeric_limits<float>::denorm_min()) == ease(k, std::numeric_limits<float>::denorm_min()));
    CHECK(std::isfinite(ease(k, std::numeric_limits<float>::denorm_min())));
    CHECK(std::isfinite(ease(k, std::nextafterf(1.0f, 0.0f))));
    CHECK(std::isfinite(ease(k, std::nextafterf(0.0f, 1.0f))));
    CHECK(std::isfinite(ease(k, 0.5f)));
  }
  CHECK(ease(static_cast<Ease>(99), 0.25f) == 0.25f);  // unknown value behaves like Linear
}

TEST_CASE("reference values at t = 0.1, 0.25, 0.5, 0.75, 0.9", "[anim][easing]") {
  struct Row { const char* name; double v[5]; };
  // from a separate Python implementation of the formulas in docs/easing.md
  const Row rows[] = {
      {"linear", {0.1, 0.25, 0.5, 0.75, 0.9}},
      {"quad-in", {0.01, 0.0625, 0.25, 0.5625, 0.81}},
      {"quad-out", {0.19, 0.4375, 0.75, 0.9375, 0.99}},
      {"quad-in-out", {0.02, 0.125, 0.5, 0.875, 0.98}},
      {"cubic-in", {0.001, 0.015625, 0.125, 0.421875, 0.729}},
      {"cubic-out", {0.271, 0.578125, 0.875, 0.984375, 0.999}},
      {"cubic-in-out", {0.004, 0.0625, 0.5, 0.9375, 0.996}},
      {"sine-in", {0.0123116594049, 0.0761204674887, 0.292893218813, 0.617316567635, 0.84356553496}},
      {"sine-out", {0.15643446504, 0.382683432365, 0.707106781187, 0.923879532511, 0.987688340595}},
      {"sine-in-out", {0.0244717418524, 0.146446609407, 0.5, 0.853553390593, 0.975528258148}},
      {"expo-in", {0.001953125, 0.00552427172802, 0.03125, 0.176776695297, 0.5}},
      {"expo-out", {0.5, 0.823223304703, 0.96875, 0.994475728272, 0.998046875}},
      {"expo-in-out", {0.001953125, 0.015625, 0.5, 0.984375, 0.998046875}},
      {"back-in", {-0.01431422, -0.0641365625, -0.0876975, 0.1825903125, 0.59117202}},
      {"back-out", {0.40882798, 0.8174096875, 1.0876975, 1.0641365625, 1.01431422}},
      {"back-in-out", {-0.037518552, -0.09968184375, 0.5, 1.09968184375, 1.037518552}},
      {"elastic-in", {0.001953125, -0.00552427172802, -0.015625, 0.0883883476483, -0.25}},
      {"elastic-out", {1.25, 0.911611652352, 1.015625, 1.00552427173, 0.998046875}},
      {"elastic-in-out", {0.000339156597006, 0.0119694444237, 0.5, 0.988030555576, 0.999660843403}},
      {"bounce-in", {0.011875, 0.02734375, 0.234375, 0.52734375, 0.924375}},
      {"bounce-out", {0.075625, 0.47265625, 0.765625, 0.97265625, 0.988125}},
      {"bounce-in-out", {0.03, 0.1171875, 0.5, 0.8828125, 0.97}},
  };
  const float ts[5] = {0.1f, 0.25f, 0.5f, 0.75f, 0.9f};
  for (const Row& r : rows) {
    Ease e;
    REQUIRE(ease_from_name(r.name, e));
    for (int i = 0; i < 5; ++i) {
      INFO(r.name << " at " << ts[i]);
      // 0.1f and 0.9f are not exactly 0.1 and 0.9: compare against the float values, with the tolerance scaled up
      // for the steepest curves (slope times the float rounding of t, about 1e-8).
      CHECK(near(ease(e, ts[i]), r.v[i], 1e-5));
    }
  }
  // Exact values at dyadic times need no tolerance beyond float rounding.
  CHECK(ease(Ease::QuadIn, 0.5f) == 0.25f);
  CHECK(ease(Ease::QuadOut, 0.5f) == 0.75f);
  CHECK(ease(Ease::CubicIn, 0.5f) == 0.125f);
  CHECK(ease(Ease::ExpoIn, 0.5f) == 0.03125f);
  CHECK(ease(Ease::BounceOut, 0.25f) == 0.47265625f);
  CHECK(ease(Ease::Linear, 0.3f) == 0.3f);
}

TEST_CASE("documented constants appear in the curves", "[anim][easing]") {
  // Back: ease-in at t = 1 / 3 uses c1 = 1.70158 and c3 = c1 + 1.
  const double t = 0.5;
  CHECK(near(ease(Ease::BackIn, 0.5f), (1.70158 + 1.0) * t * t * t - 1.70158 * t * t, 1e-7));
  // Back in-out uses c2 = c1 * 1.525 = 2.5949095.
  const double c2 = 1.70158 * 1.525;
  CHECK(c2 == Approx(2.5949095).epsilon(1e-9));
  const double a = 0.5;  // 2t at t = 0.25
  CHECK(near(ease(Ease::BackInOut, 0.25f), (a * a * ((c2 + 1) * a - c2)) / 2, 1e-7));
  // Elastic periods: c4 = 2 pi / 3 (in, out), c5 = 2 pi / 4.5 (in-out). Zeros of the sine term:
  // elastic-out is 1 where (10 t - 0.75) * c4 is a multiple of pi, that is 10 t - 0.75 = 1.5 k.
  CHECK(near(ease(Ease::ElasticOut, 0.075f), 1.0, 1e-6));   // 10 * 0.075 = 0.75 -> sin(0) = 0
  CHECK(near(ease(Ease::ElasticOut, 0.225f), 1.0, 1e-5));   // 2.25 - 0.75 = 1.5 -> sin(pi) = 0
  CHECK(near(ease(Ease::ElasticOut, 0.375f), 1.0, 1e-5));   // 3.75 - 0.75 = 3.0 -> sin(2 pi) = 0
  // Elastic in-out zeros of the sine term: (20 t - 11.125) * c5 = k pi, that is 20 t = 11.125 + 2.25 k. For k = -4
  // (t = 0.10625) the first half is 0, and for k = 3 (t = 0.89375) the second half is 1.
  CHECK(near(ease(Ease::ElasticInOut, 0.89375f), 1.0, 1e-5));
  CHECK(near(ease(Ease::ElasticInOut, 0.10625f), 0.0, 1e-5));
  // Bounce segment boundaries 1/2.75, 2/2.75, 2.5/2.75 are continuous.
  for (double b : {1.0 / 2.75, 2.0 / 2.75, 2.5 / 2.75}) {
    const float lo = std::nextafterf(static_cast<float>(b), 0.0f);
    const float hi = std::nextafterf(static_cast<float>(b), 1.0f);
    CHECK(std::fabs(ease(Ease::BounceOut, lo) - ease(Ease::BounceOut, hi)) < 1e-5);
  }
  // Bounce touches 1 at the segment ends: out is 0.75, 0.9375, 0.984375 at the lowest points of each bounce.
  CHECK(near(ease(Ease::BounceOut, static_cast<float>(1.5 / 2.75)), 0.75, 1e-6));
  CHECK(near(ease(Ease::BounceOut, static_cast<float>(2.25 / 2.75)), 0.9375, 1e-6));
  CHECK(near(ease(Ease::BounceOut, static_cast<float>(2.625 / 2.75)), 0.984375, 1e-6));
}

TEST_CASE("Out(t) = 1 - In(1 - t) and In-Out symmetry", "[anim][easing][symmetry]") {
  struct Family { Ease in, out, inout; };
  const Family fam[] = {
      {Ease::QuadIn, Ease::QuadOut, Ease::QuadInOut},         {Ease::CubicIn, Ease::CubicOut, Ease::CubicInOut},
      {Ease::SineIn, Ease::SineOut, Ease::SineInOut},         {Ease::ExpoIn, Ease::ExpoOut, Ease::ExpoInOut},
      {Ease::BackIn, Ease::BackOut, Ease::BackInOut},         {Ease::ElasticIn, Ease::ElasticOut, Ease::ElasticInOut},
      {Ease::BounceIn, Ease::BounceOut, Ease::BounceInOut},
  };
  for (const Family& f : fam) {
    INFO(ease_name(f.in));
    for (int i = 0; i <= 2000; ++i) {
      const float t = static_cast<float>(i) / 2000.0f;
      const float mirror = 1.0f - t;  // exact for these dyadic-free steps up to float rounding
      CHECK(std::fabs(static_cast<double>(ease(f.out, t)) - (1.0 - ease(f.in, mirror))) < 2e-6);
      // In-Out: f(t) + f(1 - t) = 1
      CHECK(std::fabs(static_cast<double>(ease(f.inout, t)) + ease(f.inout, mirror) - 1.0) < 2e-6);
    }
    CHECK(ease(f.inout, 0.5f) == Approx(0.5).margin(1e-6));
  }
  // Linear is its own mirror.
  for (int i = 0; i <= 100; ++i) {
    const float t = static_cast<float>(i) / 100.0f;
    CHECK(std::fabs(ease(Ease::Linear, t) - t) == 0.0f);
  }
}

TEST_CASE("fixed-seed sweep of 100000 values per kind: finite, bounded and monotonic where required", "[anim][easing][sweep]") {
  for (Ease k : kAll) {
    INFO(ease_name(k));
    Rng rng{0xC0FFEE1234567ull + static_cast<std::uint64_t>(k) * 7919};
    std::vector<float> ts(100000);
    for (float& t : ts) t = static_cast<float>(rng.unit());
    ts.push_back(0.0f);
    ts.push_back(1.0f);
    std::sort(ts.begin(), ts.end());
    float prev = -1.0f;
    for (float t : ts) {
      const float v = ease(k, t);
      REQUIRE(std::isfinite(v));
      REQUIRE(std::fabs(v) < 10.0f);  // overshoot of every kind is far below this
      if (!has_overshoot(k)) {
        REQUIRE(v >= 0.0f);
        REQUIRE(v <= 1.0f);
        REQUIRE(v >= prev);  // monotonic non-decreasing
      }
      prev = v;
    }
  }
}

TEST_CASE("overshoot kinds stay within their known bounds", "[anim][easing]") {
  float lo = 0, hi = 0;
  for (int i = 0; i <= 10000; ++i) {
    const float t = static_cast<float>(i) / 10000.0f;
    lo = std::min(lo, ease(Ease::BackIn, t));
    hi = std::max(hi, ease(Ease::BackOut, t));
  }
  CHECK(lo < -0.09f);   // back-in dips below 0 by about 0.1
  CHECK(lo > -0.15f);
  CHECK(hi > 1.09f);
  CHECK(hi < 1.15f);
  float emax = 0;
  for (int i = 0; i <= 10000; ++i) emax = std::max(emax, ease(Ease::ElasticOut, static_cast<float>(i) / 10000.0f));
  CHECK(emax > 1.35f);  // the first overshoot of elastic-out peaks near 1.37
  CHECK(emax < 1.40f);
  for (int i = 0; i <= 10000; ++i) {  // bounce never leaves [0, 1]
    const float v = ease(Ease::BounceOut, static_cast<float>(i) / 10000.0f);
    CHECK(v >= 0.0f);
    CHECK(v <= 1.0f);
  }
}

// ---------------------------------------------------------------------------------------------------------------
namespace {
double bez(double p1, double p2, double s) {
  const double u = 1 - s;
  return 3 * u * u * s * p1 + 3 * u * s * s * p2 + s * s * s;
}
double bez_d(double p1, double p2, double s) {
  const double u = 1 - s;
  return 3 * u * u * p1 + 6 * u * s * (p2 - p1) + 3 * s * s * (1 - p2);
}
// Independent reference: plain bisection to full double precision.
double solve_s(double x1, double x2, double t) {
  double lo = 0, hi = 1;
  for (int i = 0; i < 200; ++i) {
    const double m = 0.5 * (lo + hi);
    if (bez(x1, x2, m) < t) lo = m; else hi = m;
  }
  return 0.5 * (lo + hi);
}
float bezier(float x1, float y1, float x2, float y2, float t) {
  float out = -777.0f;
  REQUIRE(cubic_bezier(x1, y1, x2, y2, t, out));
  return out;
}
}  // namespace

TEST_CASE("cubic_bezier: CSS keyword curves against reference values", "[anim][bezier]") {
  struct Css { const char* name; float c[4]; double v[5]; };
  // reference values from a separate Python bisection solver
  const Css css[] = {
      {"ease", {0.25f, 0.1f, 0.25f, 1.0f}, {0.094796305716, 0.408510591355, 0.802403387585, 0.960458978349, 0.994316477485}},
      {"ease-in", {0.42f, 0.0f, 1.0f, 1.0f}, {0.0170266096516, 0.0934646507188, 0.315356812573, 0.621861869175, 0.839427845762}},
      {"ease-out", {0.0f, 0.0f, 0.58f, 1.0f}, {0.160572154238, 0.378138130825, 0.684643187427, 0.906535349281, 0.982973390348}},
      {"ease-in-out", {0.42f, 0.0f, 0.58f, 1.0f}, {0.0197224535483, 0.129161931047, 0.5, 0.870838068953, 0.980277546452}},
  };
  const float ts[5] = {0.1f, 0.25f, 0.5f, 0.75f, 0.9f};
  for (const Css& c : css) {
    for (int i = 0; i < 5; ++i) {
      INFO(c.name << " at " << ts[i]);
      CHECK(near(bezier(c.c[0], c.c[1], c.c[2], c.c[3], ts[i]), c.v[i], 2e-6));
    }
    CHECK(bezier(c.c[0], c.c[1], c.c[2], c.c[3], 0.0f) == 0.0f);
    CHECK(bezier(c.c[0], c.c[1], c.c[2], c.c[3], 1.0f) == 1.0f);
  }
  // The linear curve (1/3, 1/3, 2/3, 2/3) is the identity.
  for (int i = 0; i <= 20; ++i) {
    const float t = static_cast<float>(i) / 20.0f;
    CHECK(bezier(1.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, t) == Approx(t).margin(2e-6));
  }
  // ease-in-out is point symmetric.
  for (int i = 1; i < 20; ++i) {
    const float t = static_cast<float>(i) / 20.0f;
    CHECK(bezier(0.42f, 0, 0.58f, 1, t) + bezier(0.42f, 0, 0.58f, 1, 1.0f - t) == Approx(1.0).margin(4e-6));
  }
}

TEST_CASE("cubic_bezier: invalid controls leave out untouched", "[anim][bezier]") {
  const float bad[][4] = {
      {-0.01f, 0, 0.5f, 1}, {1.01f, 0, 0.5f, 1}, {0.5f, 0, -0.01f, 1}, {0.5f, 0, 1.01f, 1},
      {kNaN, 0, 0.5f, 1},   {0.5f, kNaN, 0.5f, 1}, {0.5f, 0, kNaN, 1},  {0.5f, 0, 0.5f, kNaN},
      {kInf, 0, 0.5f, 1},   {0.5f, kInf, 0.5f, 1}, {0.5f, 0, -kInf, 1}, {0.5f, 0, 0.5f, -kInf},
      {2.0f, 0, 0.5f, 1},   {0.5f, 0, -3.0f, 1},
  };
  for (const auto& c : bad) {
    float out = 42.0f;
    CHECK_FALSE(cubic_bezier(c[0], c[1], c[2], c[3], 0.5f, out));
    CHECK(out == 42.0f);
    CHECK_FALSE(cubic_bezier(c[0], c[1], c[2], c[3], 0.0f, out));
    CHECK_FALSE(cubic_bezier(c[0], c[1], c[2], c[3], kNaN, out));
    CHECK(out == 42.0f);
  }
  // y may be anything finite, x may sit on the edge of [0, 1].
  float out = 0;
  CHECK(cubic_bezier(0.0f, -5.0f, 1.0f, 7.0f, 0.5f, out));
  CHECK(std::isfinite(out));
  CHECK(cubic_bezier(0.5f, std::numeric_limits<float>::max(), 0.5f, -std::numeric_limits<float>::max(), 0.3f, out));
  CHECK(std::isfinite(out));
}

TEST_CASE("cubic_bezier: endpoints, clamping and non-finite t", "[anim][bezier]") {
  for (const auto& c : {std::vector<float>{0.25f, 0.1f, 0.25f, 1.0f}, std::vector<float>{0.0f, 5.0f, 1.0f, -5.0f},
                        std::vector<float>{0.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 1.0f, 1.0f, 1.0f}}) {
    CHECK(bezier(c[0], c[1], c[2], c[3], 0.0f) == 0.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], -0.0f) == 0.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], 1.0f) == 1.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], -3.0f) == 0.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], 3.0f) == 1.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], kNaN) == 0.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], -kInf) == 0.0f);
    CHECK(bezier(c[0], c[1], c[2], c[3], kInf) == 1.0f);
    CHECK(std::isfinite(bezier(c[0], c[1], c[2], c[3], 0.5f)));
    CHECK(std::isfinite(bezier(c[0], c[1], c[2], c[3], std::nextafterf(0.0f, 1.0f))));
    CHECK(std::isfinite(bezier(c[0], c[1], c[2], c[3], std::nextafterf(1.0f, 0.0f))));
  }
}

TEST_CASE("cubic_bezier: 400 fixed-seed control sets keep x accurate, finite and monotonic", "[anim][bezier][random]") {
  Rng rng{0xBEE5ull};
  int checked = 0;
  for (int n = 0; n < 400; ++n) {
    // The first 100 sets use the interior of [0, 1], the rest include the edges 0 and 1 and flat (x1 = x2) cases.
    float x1 = static_cast<float>(0.02 + 0.96 * rng.unit());
    float x2 = static_cast<float>(0.02 + 0.96 * rng.unit());
    if (n >= 100) {
      const int mode = static_cast<int>(rng.next() % 6);
      if (mode == 0) x1 = 0.0f;
      if (mode == 1) x1 = 1.0f;
      if (mode == 2) x2 = 0.0f;
      if (mode == 3) x2 = 1.0f;
      if (mode == 4) x2 = x1;
    }
    const float y1 = static_cast<float>(-2.0 + 5.0 * rng.unit());
    const float y2 = static_cast<float>(-2.0 + 5.0 * rng.unit());
    float prev_y = -1e30f;
    const bool monotone_y = n % 4 == 0;  // for those, y controls are ordered 0 <= y1 <= y2 <= 1
    const float my1 = static_cast<float>(0.5 * rng.unit());
    const float my2 = static_cast<float>(0.5 + 0.5 * rng.unit());
    for (int i = 0; i <= 50; ++i) {
      const float t = static_cast<float>(i) / 50.0f;
      float out = 0;
      const float c1 = monotone_y ? my1 : y1, c2 = monotone_y ? my2 : y2;
      REQUIRE(cubic_bezier(x1, c1, x2, c2, t, out));
      REQUIRE(std::isfinite(out));
      // Reference: solve x(s) = t independently, then y(s). The allowed error in y is the x accuracy of 1e-6 times the
      // local slope dy/dx (a flat or steep x leaves the slope large), plus float rounding.
      const double s = solve_s(x1, x2, t);
      const double ref = bez(c1, c2, s);
      const double dx = bez_d(x1, x2, s);
      const double dy = bez_d(c1, c2, s);
      const double slope = dx > 1e-9 ? std::fabs(dy / dx) : 1e3;
      const bool interior = t > 0.0f && t < 1.0f;
      if (interior && dx > 1e-3) {
        REQUIRE(std::fabs(out - ref) <= 2e-6 * slope + 2e-6);
        ++checked;
      } else if (!interior) {
        REQUIRE(out == (t == 0.0f ? 0.0f : 1.0f));
      }
      if (monotone_y) {
        REQUIRE(out >= prev_y - 1e-6f);  // monotonic x(s) and monotonic y(s) give a monotonic curve
        REQUIRE(out >= -1e-6f);
        REQUIRE(out <= 1.0f + 1e-6f);
        prev_y = out;
      }
    }
  }
  CHECK(checked > 15000);
}

// ---------------------------------------------------------------------------------------------------------------
namespace {
Tween make_tween(float from, float to, float duration, float delay, Ease e, Loop l) {
  Tween t;
  t.from = from;
  t.to = to;
  t.duration = duration;
  t.delay = delay;
  t.easing = e;
  t.loop = l;
  return t;
}
float tv(const Tween& t, float time) {
  float out = -777.0f;
  REQUIRE(tween_value(t, time, out));
  return out;
}
}  // namespace

TEST_CASE("tween_value: delay, endpoints and easing", "[anim][tween]") {
  const Tween t = make_tween(10, 20, 2, 1, Ease::Linear, Loop::Clamp);
  CHECK(tv(t, -5.0f) == 10.0f);
  CHECK(tv(t, 0.0f) == 10.0f);
  CHECK(tv(t, 0.999f) == 10.0f);
  CHECK(tv(t, 1.0f) == 10.0f);   // the tween starts at the delay
  CHECK(tv(t, 1.5f) == 12.5f);
  CHECK(tv(t, 2.0f) == 15.0f);
  CHECK(tv(t, 2.5f) == 17.5f);
  CHECK(tv(t, 3.0f) == 20.0f);   // exactly at the end
  CHECK(tv(t, 100.0f) == 20.0f);
  CHECK(tv(t, 1e30f) == 20.0f);
  // Eased: quad-in at half time is 0.25 of the span.
  const Tween q = make_tween(0, 100, 4, 0, Ease::QuadIn, Loop::Clamp);
  CHECK(tv(q, 2.0f) == 25.0f);
  CHECK(tv(q, 1.0f) == Approx(6.25).margin(1e-5));
  // Reverse tween (to < from).
  const Tween r = make_tween(5, -5, 1, 0, Ease::Linear, Loop::Clamp);
  CHECK(tv(r, 0.5f) == 0.0f);
  CHECK(tv(r, 1.0f) == -5.0f);
  // from == to is constant. Overshooting kinds go beyond the endpoints.
  const Tween c = make_tween(3, 3, 1, 0, Ease::ElasticOut, Loop::Clamp);
  CHECK(tv(c, 0.1f) == 3.0f);
  const Tween b = make_tween(0, 10, 1, 0, Ease::BackOut, Loop::Clamp);
  CHECK(tv(b, 0.5f) == Approx(10.876975).margin(1e-4));
}

TEST_CASE("tween_value: loop modes", "[anim][tween]") {
  // duration 2, delay 0.5, linear 0 -> 8: value = 4 * (loop time)
  struct Row { float time, clamp, repeat, pingpong; };
  const Row rows[] = {
      {0.0f, 0, 0, 0},       // before the delay
      {0.5f, 0, 0, 0},       // at the delay
      {1.0f, 2, 2, 2},       // loop time 0.5
      {2.5f, 8, 0, 8},       // loop time 2: the end under Clamp and PingPong, wraps to the start under Repeat
      {3.0f, 8, 2, 6},       // loop time 2.5
      {4.5f, 8, 0, 0},       // loop time 4: two durations
      {5.0f, 8, 2, 2},       // loop time 4.5
      {-1.0f, 0, 0, 0},      // before the delay
  };
  for (const Row& r : rows) {
    INFO("time " << r.time);
    CHECK(tv(make_tween(0, 8, 2, 0.5f, Ease::Linear, Loop::Clamp), r.time) == r.clamp);
    CHECK(tv(make_tween(0, 8, 2, 0.5f, Ease::Linear, Loop::Repeat), r.time) == r.repeat);
    CHECK(tv(make_tween(0, 8, 2, 0.5f, Ease::Linear, Loop::PingPong), r.time) == r.pingpong);
  }
  // Easing is applied to the looped progress: PingPong with quad-in mirrors the eased curve.
  const Tween p = make_tween(0, 16, 2, 0, Ease::QuadIn, Loop::PingPong);
  CHECK(tv(p, 1.0f) == 4.0f);   // progress 0.5 -> 0.25 * 16
  CHECK(tv(p, 3.0f) == 4.0f);   // mirrored
  CHECK(tv(p, 2.0f) == 16.0f);
  CHECK(tv(p, 4.0f) == 0.0f);
}

TEST_CASE("tween_value: invalid tweens, zero duration, negative delay and non-finite numbers", "[anim][tween]") {
  const Tween good = make_tween(0, 1, 1, 0, Ease::Linear, Loop::Clamp);
  float out = 42.0f;
  CHECK(tween_value(good, 0.5f, out));
  CHECK(out == 0.5f);

  const Tween bad[] = {
      make_tween(0, 1, 0, 0, Ease::Linear, Loop::Clamp),          // zero duration
      make_tween(0, 1, -1, 0, Ease::Linear, Loop::Clamp),         // negative duration
      make_tween(0, 1, 1, -0.5f, Ease::Linear, Loop::Clamp),      // negative delay
      make_tween(0, 1, 1, -kInf, Ease::Linear, Loop::Clamp),
      make_tween(kNaN, 1, 1, 0, Ease::Linear, Loop::Clamp),
      make_tween(0, kNaN, 1, 0, Ease::Linear, Loop::Clamp),
      make_tween(kInf, 1, 1, 0, Ease::Linear, Loop::Clamp),
      make_tween(0, -kInf, 1, 0, Ease::Linear, Loop::Clamp),
      make_tween(0, 1, kNaN, 0, Ease::Linear, Loop::Clamp),
      make_tween(0, 1, kInf, 0, Ease::Linear, Loop::Clamp),
      make_tween(0, 1, 1, kNaN, Ease::Linear, Loop::Clamp),
      make_tween(0, 1, 1, kInf, Ease::Linear, Loop::Clamp),
      make_tween(0, 1, -0.0f, 0, Ease::Linear, Loop::Clamp),      // negative zero duration is not > 0
  };
  for (const Tween& t : bad) {
    for (float time : {0.0f, 0.5f, 5.0f, kNaN}) {
      out = 42.0f;
      CHECK_FALSE(tween_value(t, time, out));
      CHECK(out == 42.0f);
    }
  }
  CHECK(tween_value(make_tween(0, 1, 1, -0.0f, Ease::Linear, Loop::Clamp), 0.5f, out));  // negative zero delay is fine
  CHECK(out == 0.5f);
  for (Loop l : {Loop::Clamp, Loop::Repeat, Loop::PingPong}) {  // and equal to a delay of +0 for every time
    for (float time : {-1.0f, 0.0f, 0.3f, 1.0f, 1.7f, 5.0f, kNaN, kInf, -kInf}) {
      const float a = tv(make_tween(2, 9, 1.5f, -0.0f, Ease::CubicInOut, l), time);
      const float b = tv(make_tween(2, 9, 1.5f, 0.0f, Ease::CubicInOut, l), time);
      CHECK(std::memcmp(&a, &b, sizeof a) == 0);
    }
  }
  CHECK(tween_value(make_tween(0, 1, std::numeric_limits<float>::denorm_min(), 0, Ease::Linear, Loop::Clamp), 1.0f, out));
  CHECK(out == 1.0f);
}

TEST_CASE("tween_value: non-finite and extreme times", "[anim][tween]") {
  for (Loop l : {Loop::Clamp, Loop::Repeat, Loop::PingPong}) {
    const Tween t = make_tween(10, 20, 2, 1, Ease::QuadOut, l);
    // NaN time and +infinity time (after the delay) map to loop time 0 through apply_loop: the start value.
    CHECK(tv(t, kNaN) == 10.0f);
    CHECK(tv(t, kInf) == 10.0f);
    CHECK(tv(t, -kInf) == 10.0f);
    CHECK(std::isfinite(tv(t, std::numeric_limits<float>::max())));
    CHECK(std::isfinite(tv(t, 1e30f)));
    CHECK(tv(t, -1e30f) == 10.0f);
  }
  CHECK(tv(make_tween(10, 20, 2, 1, Ease::Linear, Loop::Clamp), 1e30f) == 20.0f);
  // A huge delay is fine: before it the value is `from`.
  const float big = std::numeric_limits<float>::max();
  CHECK(tv(make_tween(1, 2, 1, big, Ease::Linear, Loop::Clamp), 1e30f) == 1.0f);
  CHECK(tv(make_tween(1, 2, 1, big, Ease::Linear, Loop::Clamp), big) == 1.0f);   // loop time 0 at the delay
}

TEST_CASE("tween_value: huge endpoints stay finite", "[anim][tween]") {
  const Tween t = make_tween(-3e38f, 3e38f, 1, 0, Ease::Linear, Loop::Clamp);
  CHECK(tv(t, 0.0f) == -3e38f);
  CHECK(tv(t, 1.0f) == 3e38f);
  CHECK(tv(t, 0.5f) == Approx(0.0).margin(1e32));
  CHECK(tv(t, 0.25f) == Approx(-1.5e38).epsilon(1e-6));
  CHECK(tv(t, 0.75f) == Approx(1.5e38).epsilon(1e-6));
  // Overshooting kinds push past the float range: clamped to the largest finite float.
  const float max = std::numeric_limits<float>::max();
  for (Ease e : {Ease::BackOut, Ease::BackIn, Ease::ElasticOut, Ease::ElasticIn, Ease::BackInOut, Ease::ElasticInOut}) {
    for (int i = 0; i <= 200; ++i) {
      const float v = tv(make_tween(-max, max, 1, 0, e, Loop::Clamp), static_cast<float>(i) / 200.0f);
      CHECK(std::isfinite(v));
      CHECK(v >= -max);
      CHECK(v <= max);
    }
  }
  CHECK(tv(make_tween(max, -max, 1, 0, Ease::BackOut, Loop::Clamp), 0.5f) == -max);  // 1.0877 * span overshoots below -max
  CHECK(tv(make_tween(-max, max, 1, 0, Ease::BackOut, Loop::Clamp), 0.5f) == max);
}

TEST_CASE("tween_value: determinism for every kind", "[anim][tween]") {
  for (Ease e : kAll) {
    const Tween t = make_tween(-2, 6, 3, 0.25f, e, Loop::PingPong);
    for (int i = -4; i < 60; ++i) {
      const float time = static_cast<float>(i) * 0.37f;
      const float a = tv(t, time);
      const float b = tv(t, time);
      CHECK(std::memcmp(&a, &b, sizeof a) == 0);
      CHECK(std::isfinite(a));
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("ten million ease calls under a time cap", "[anim][easing][perf]") {
  const auto start = std::chrono::steady_clock::now();
  double sum = 0;
  const int per_kind = 10000000 / 22 + 1;  // 10000010 calls in total
  for (Ease k : kAll) {
    for (int i = 0; i < per_kind; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(per_kind);
      sum += ease(k, t);
    }
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("10 million calls took " << secs << " s");
  CHECK(std::isfinite(sum));
  CHECK(secs < 30.0);
}
