// Tests for core/color. Pure CPU. Golden digests are also written in docs/color.md and must match on Linux and Windows.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "color.h"

using Catch::Approx;
using namespace lumora::color;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

Gradient two(Space s) {
  Gradient g;
  g.count = 2;
  g.space = s;
  g.stops[0] = {0.0f, {0.0f, 0.0f, 0.0f, 1.0f}};
  g.stops[1] = {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
  return g;
}

// Gradient used by the golden digests: five stops, one hard step, alpha changes.
Gradient golden(Space s) {
  Gradient g;
  g.count = 5;
  g.space = s;
  g.stops[0] = {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}};
  g.stops[1] = {0.3f, {1.0f, 1.0f, 0.0f, 0.5f}};
  g.stops[2] = {0.6f, {0.0f, 0.25f, 1.0f, 1.0f}};
  g.stops[3] = {0.6f, {0.0f, 0.75f, 0.25f, 0.0f}};
  g.stops[4] = {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
  return g;
}

std::uint64_t ramp_digest(Space s) {
  std::vector<std::uint8_t> buf(256 * 4);
  REQUIRE(bake_gradient_u8(golden(s), 256, buf.data()));
  return digest_u8(buf.data(), buf.size());
}

// Reference values from a 60 digit decimal implementation of the same formulas (exp/log based powers).
struct Ref1 { float x; double y; };
const Ref1 kS2L[] = {
    {0.0001f, 7.7399380804953562e-06}, {0.02f, 0.0015479876160990713}, {0.04045f, 0.0031308049535603713},
    {0.05f, 0.0039359395040889653},    {0.1f, 0.010022825574869033},   {0.2f, 0.033104766570885048},
    {0.3f, 0.073238955878405426},      {0.5f, 0.21404114048223244},    {0.7f, 0.44798841244188314},
    {0.9f, 0.78741228939561703},       {0.999f, 0.99772662769268217},  {1.0f, 1.0}};
const Ref1 kL2S[] = {
    {0.0001f, 0.001292},           {0.004f, 0.050708713977347976},
    {0.01f, 0.099852822734128335}, {0.05f, 0.24780052799263166},       {0.18f, 0.46135612950044164},
    {0.5f, 0.73535698305244945},   {0.8f, 0.90633175334405947},        {1.0f, 1.0}};
struct RefLab { float r, g, b; double l, a, bb; };
const RefLab kLab[] = {
    {1, 1, 1, 0.99999999347354607, 8.095307350269844e-11, 3.7273907607792462e-08},
    {0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0.62795536061455159, 0.2248630610659742, 0.12584629853073512},
    {0, 1, 0, 0.86643961153566951, -0.23388757418790787, 0.17949847989672996},
    {0, 0, 1, 0.45201371838534288, -0.032456984168763768, -0.31152814767837511},
    {0.5f, 0.25f, 0.75f, 0.51522365538496029, 0.10492907971146974, -0.16038022226881291},
    {0.2f, 0.8f, 0.4f, 0.74500145443412891, -0.16356375574574658, 0.096066667922576274},
    {0.9f, 0.9f, 0.1f, 0.89424219737222266, -0.064450246895570343, 0.17987933656097974}};

constexpr std::uint64_t kGoldenSrgb = 0xcc797225a83824b7ull;
constexpr std::uint64_t kGoldenLinear = 0x12d82711acf8f9e7ull;
constexpr std::uint64_t kGoldenOklab = 0x5a312dc44ca9558aull;

}  // namespace

TEST_CASE("gradient statuses in header order", "[color][gradient]") {
  Gradient g = two(Space::Srgb);
  CHECK(validate_gradient(g) == GradientStatus::Ok);
  CHECK(std::string(gradient_status_text(GradientStatus::Ok)) == "ok");
  g.count = 0; CHECK(validate_gradient(g) == GradientStatus::TooFewStops);
  g.count = 1; CHECK(validate_gradient(g) == GradientStatus::TooFewStops);
  g.count = kMaxStops + 1; CHECK(validate_gradient(g) == GradientStatus::TooManyStops);
  g.count = 100000; CHECK(validate_gradient(g) == GradientStatus::TooManyStops);
  g.count = kMaxStops;
  for (std::size_t i = 0; i < kMaxStops; ++i) g.stops[i] = {0.5f, {0, 0, 0, 1}};
  CHECK(validate_gradient(g) == GradientStatus::Ok);  // equal positions are allowed

  for (float bad : {kNaN, -0.0001f, 1.0001f, kInf, -kInf}) {
    g = two(Space::Srgb);
    g.stops[1].position = bad;
    CHECK(validate_gradient(g) == GradientStatus::BadPosition);
    g = two(Space::Srgb);
    g.stops[0].position = bad;
    CHECK(validate_gradient(g) == GradientStatus::BadPosition);
  }
  g = two(Space::Srgb);
  g.stops[0].position = 0.7f; g.stops[1].position = 0.6f;
  CHECK(validate_gradient(g) == GradientStatus::BadPosition);  // decreasing
  g.stops[1].position = 0.7f;
  CHECK(validate_gradient(g) == GradientStatus::Ok);  // equal is a hard step
  g.stops[0].position = -0.0f; g.stops[1].position = 0.0f;
  CHECK(validate_gradient(g) == GradientStatus::Ok);

  for (int ch = 0; ch < 4; ++ch) {
    for (float bad : {kNaN, -0.001f, 1.001f, kInf}) {
      g = two(Space::Srgb);
      float* c = &g.stops[1].color.r;
      c[ch] = bad;
      CHECK(validate_gradient(g) == GradientStatus::BadColor);
    }
  }
  g = two(Space::Linear);
  g.space = static_cast<Space>(3);
  CHECK(validate_gradient(g) == GradientStatus::BadSpace);
  g.space = static_cast<Space>(255);
  CHECK(validate_gradient(g) == GradientStatus::BadSpace);
  // Order: count, then positions, then colors, then space.
  g = two(Space::Srgb);
  g.space = static_cast<Space>(9);
  g.stops[0].color.g = 2.0f;
  CHECK(validate_gradient(g) == GradientStatus::BadColor);
  g.stops[1].position = kNaN;
  CHECK(validate_gradient(g) == GradientStatus::BadPosition);
  g.count = 1;
  CHECK(validate_gradient(g) == GradientStatus::TooFewStops);
  // A bad position in a later stop wins over a bad color in an earlier one.
  g = two(Space::Srgb);
  g.stops[0].color.r = kNaN;
  g.stops[1].position = 2.0f;
  CHECK(validate_gradient(g) == GradientStatus::BadPosition);

  std::vector<std::string> texts;
  for (int s = 0; s <= 5; ++s) {
    const std::string t = gradient_status_text(static_cast<GradientStatus>(s));
    CHECK_FALSE(t.empty());
    for (const std::string& o : texts) CHECK(o != t);
    texts.push_back(t);
  }
  CHECK(std::string(gradient_status_text(static_cast<GradientStatus>(77))) == "unknown status");
}

TEST_CASE("sRGB transfer functions match high precision references", "[color][srgb]") {
  for (const Ref1& r : kS2L) CHECK(srgb_to_linear(r.x) == Approx(r.y).epsilon(2e-7).margin(1e-12));
  for (const Ref1& r : kL2S) CHECK(linear_to_srgb(r.x) == Approx(r.y).epsilon(2e-7).margin(1e-12));
  CHECK(srgb_to_linear(0.0f) == 0.0f);
  CHECK(srgb_to_linear(1.0f) == 1.0f);
  CHECK(linear_to_srgb(0.0f) == 0.0f);
  CHECK(linear_to_srgb(1.0f) == 1.0f);
  // Clamping and NaN.
  for (float v : {-1.0f, -kInf, -0.0f, kNaN}) {
    CHECK(srgb_to_linear(v) == 0.0f);
    CHECK(linear_to_srgb(v) == 0.0f);
  }
  for (float v : {1.5f, kInf, 1e30f}) {
    CHECK(srgb_to_linear(v) == 1.0f);
    CHECK(linear_to_srgb(v) == 1.0f);
  }
  CHECK(srgb_to_linear(std::numeric_limits<float>::denorm_min()) >= 0.0f);
  CHECK(linear_to_srgb(std::numeric_limits<float>::denorm_min()) > 0.0f);
  // Monotonic ramps, results inside 0..1.
  float prev_a = -1.0f, prev_b = -1.0f;
  for (int i = 0; i <= 8192; ++i) {
    const float x = static_cast<float>(i) / 8192.0f;
    const float a = srgb_to_linear(x), b = linear_to_srgb(x);
    CHECK(a >= prev_a);
    CHECK(b >= prev_b);
    CHECK((a >= 0.0f && a <= 1.0f && b >= 0.0f && b <= 1.0f));
    prev_a = a; prev_b = b;
  }
  // Round trip in float over a fine grid.
  for (int i = 0; i <= 4096; ++i) {
    const float x = static_cast<float>(i) / 4096.0f;
    CHECK(std::fabs(linear_to_srgb(srgb_to_linear(x)) - x) < 3e-6f);
    CHECK(std::fabs(srgb_to_linear(linear_to_srgb(x)) - x) < 3e-6f);
  }
}

TEST_CASE("8-bit helpers", "[color][u8]") {
  CHECK(to_u8(0.0f) == 0);
  CHECK(to_u8(1.0f) == 255);
  CHECK(to_u8(0.5f) == 128);  // 127.5 exactly: ties away from zero
  CHECK(to_u8(-0.0f) == 0);
  CHECK(to_u8(-5.0f) == 0);
  CHECK(to_u8(5.0f) == 255);
  CHECK(to_u8(kNaN) == 0);
  CHECK(to_u8(kInf) == 255);
  CHECK(to_u8(-kInf) == 0);
  CHECK(to_u8(std::numeric_limits<float>::denorm_min()) == 0);
  // Around every rounding boundary (2k+1)/510.
  for (int k = 0; k < 255; ++k) {
    const float mid = (2.0f * static_cast<float>(k) + 1.0f) / 510.0f;
    const double exact = (2.0 * k + 1.0) / 510.0;
    const float below = std::nextafterf(mid, 0.0f), above = std::nextafterf(mid, 1.0f);
    if (static_cast<double>(mid) < exact) { CHECK(to_u8(mid) == k); }
    if (static_cast<double>(mid) > exact) { CHECK(to_u8(mid) == k + 1); }
    CHECK(to_u8(below) <= k + 1);
    CHECK(to_u8(above) >= k);
    CHECK(to_u8(below) <= to_u8(above));
  }
  CHECK(from_u8(0) == 0.0f);
  CHECK(from_u8(255) == 1.0f);
  CHECK(from_u8(51) == Approx(0.2).epsilon(1e-7));
  for (int i = 0; i < 256; ++i) {
    const std::uint8_t v = static_cast<std::uint8_t>(i);
    CHECK(to_u8(from_u8(v)) == v);
    // 8-bit sRGB to linear and back to 8-bit sRGB is exact for every value.
    CHECK(to_u8(linear_to_srgb(srgb_to_linear(from_u8(v)))) == v);
  }
  // The 8-bit to linear map is strictly increasing (255 distinct float values).
  for (int i = 1; i < 256; ++i) {
    CHECK(srgb_to_linear(from_u8(static_cast<std::uint8_t>(i))) > srgb_to_linear(from_u8(static_cast<std::uint8_t>(i - 1))));
  }
}

TEST_CASE("OKLab matches references and round trips", "[color][oklab]") {
  for (const RefLab& r : kLab) {
    const Oklab lab = srgb_to_oklab(r.r, r.g, r.b);
    CHECK(std::fabs(lab.l - r.l) < 2e-6);
    CHECK(std::fabs(lab.a - r.a) < 2e-6);
    CHECK(std::fabs(lab.b - r.bb) < 2e-6);
  }
  // Round trip over a grid of sRGB colours.
  for (int ri = 0; ri <= 16; ++ri) {
    for (int gi = 0; gi <= 16; ++gi) {
      for (int bi = 0; bi <= 16; ++bi) {
        const float r = static_cast<float>(ri) / 16.0f, g = static_cast<float>(gi) / 16.0f, b = static_cast<float>(bi) / 16.0f;
        float out[3];
        oklab_to_srgb(srgb_to_oklab(r, g, b), out);
        REQUIRE(std::fabs(out[0] - r) < 2e-5f);
        REQUIRE(std::fabs(out[1] - g) < 2e-5f);
        REQUIRE(std::fabs(out[2] - b) < 2e-5f);
      }
    }
  }
  // Out of gamut OKLab values are clamped to 0..1; NaN gives 0.
  float out[3];
  oklab_to_srgb({1.0f, 0.4f, 0.4f}, out);
  for (float v : out) CHECK((v >= 0.0f && v <= 1.0f));
  oklab_to_srgb({5.0f, 0.0f, 0.0f}, out);
  CHECK(out[0] == 1.0f); CHECK(out[1] == 1.0f); CHECK(out[2] == 1.0f);
  oklab_to_srgb({-1.0f, 0.0f, 0.0f}, out);
  CHECK(out[0] == 0.0f); CHECK(out[1] == 0.0f); CHECK(out[2] == 0.0f);
  oklab_to_srgb({kNaN, 0.0f, 0.0f}, out);
  for (float v : out) CHECK(v == 0.0f);
  // Inputs are clamped, NaN is zero; black maps to zero.
  const Oklab nan = srgb_to_oklab(kNaN, -3.0f, 0.0f);
  CHECK(nan.l == 0.0f); CHECK(nan.a == 0.0f); CHECK(nan.b == 0.0f);
  const Oklab hi = srgb_to_oklab(9.0f, 9.0f, 9.0f);
  CHECK(hi.l == Approx(1.0).epsilon(1e-6));
  // Lightness rises with grey level.
  float prev = -1.0f;
  for (int i = 0; i <= 255; ++i) {
    const float v = from_u8(static_cast<std::uint8_t>(i));
    const float l = srgb_to_oklab(v, v, v).l;
    CHECK(l > prev);
    CHECK(std::fabs(srgb_to_oklab(v, v, v).a) < 1e-5f);
    prev = l;
  }
}

TEST_CASE("alpha handling", "[color][alpha]") {
  const Rgba c{0.5f, 0.25f, 1.0f, 0.5f};
  const Rgba p = premultiply(c);
  CHECK(p.r == 0.25f); CHECK(p.g == 0.125f); CHECK(p.b == 0.5f); CHECK(p.a == 0.5f);
  const Rgba u = unpremultiply(p);
  CHECK(u.r == 0.5f); CHECK(u.g == 0.25f); CHECK(u.b == 1.0f); CHECK(u.a == 0.5f);
  const Rgba z = unpremultiply({0.3f, 0.2f, 0.1f, 0.0f});
  CHECK(z.r == 0.0f); CHECK(z.g == 0.0f); CHECK(z.b == 0.0f); CHECK(z.a == 0.0f);
  // over
  Rgba o = over({1, 0, 0, 1}, {0, 0, 1, 1});
  CHECK(o.r == 1.0f); CHECK(o.b == 0.0f); CHECK(o.a == 1.0f);
  o = over({1, 0, 0, 0}, {0, 0, 1, 1});
  CHECK(o.r == 0.0f); CHECK(o.b == 1.0f); CHECK(o.a == 1.0f);
  o = over({1, 0, 0, 0.5f}, {0, 0, 1, 1});
  CHECK(o.r == 0.5f); CHECK(o.b == 0.5f); CHECK(o.a == 1.0f);
  o = over({1, 0, 0, 0.5f}, {0, 0, 1, 0.5f});  // ao = 0.75, red 0.5/0.75, blue 0.25/0.75
  CHECK(o.a == 0.75f);
  CHECK(o.r == Approx(2.0 / 3.0).epsilon(1e-6));
  CHECK(o.b == Approx(1.0 / 3.0).epsilon(1e-6));
  o = over({0.7f, 0.8f, 0.9f, 0.0f}, {0.1f, 0.2f, 0.3f, 0.0f});  // result alpha zero: all four channels zero
  CHECK(o.r == 0.0f); CHECK(o.g == 0.0f); CHECK(o.b == 0.0f); CHECK(o.a == 0.0f);
  // Over with a transparent destination keeps the source colour.
  o = over({0.3f, 0.6f, 0.9f, 0.4f}, {1, 1, 1, 0});
  CHECK(o.r == Approx(0.3).epsilon(1e-6)); CHECK(o.g == Approx(0.6).epsilon(1e-6)); CHECK(o.b == Approx(0.9).epsilon(1e-6));
  CHECK(o.a == Approx(0.4).epsilon(1e-6));
}

TEST_CASE("gradient sampling: holds, hard steps, alpha, spaces", "[color][gradient]") {
  Gradient g = two(Space::Srgb);
  g.stops[0].position = 0.25f;
  g.stops[1].position = 0.75f;
  g.stops[0].color = {1, 0, 0, 0.0f};
  g.stops[1].color = {0, 0, 1, 1.0f};
  Rgba c = sample_gradient(g, 0.0f);  // before first stop: end colour holds
  CHECK(c.r == 1.0f); CHECK(c.a == 0.0f);
  c = sample_gradient(g, 1.0f);
  CHECK(c.b == 1.0f); CHECK(c.a == 1.0f);
  c = sample_gradient(g, 0.5f);  // middle: alpha linear, sRGB linear
  CHECK(c.r == Approx(0.5).epsilon(1e-6)); CHECK(c.b == Approx(0.5).epsilon(1e-6)); CHECK(c.a == Approx(0.5).epsilon(1e-6));
  c = sample_gradient(g, kNaN);
  CHECK(c.r == 1.0f);
  c = sample_gradient(g, -5.0f); CHECK(c.r == 1.0f);
  c = sample_gradient(g, 7.0f); CHECK(c.b == 1.0f);
  c = sample_gradient(g, kInf); CHECK(c.b == 1.0f);
  c = sample_gradient(g, 0.25f);  // exactly on a stop: its colour, unchanged
  CHECK(c.r == 1.0f); CHECK(c.g == 0.0f); CHECK(c.b == 0.0f); CHECK(c.a == 0.0f);
  c = sample_gradient(g, 0.75f);
  CHECK(c.b == 1.0f); CHECK(c.r == 0.0f); CHECK(c.a == 1.0f);

  // Hard step: later stop wins at t >= position.
  Gradient h = golden(Space::Oklab);
  c = sample_gradient(h, 0.6f);
  CHECK(c.r == 0.0f); CHECK(c.g == 0.75f); CHECK(c.b == 0.25f); CHECK(c.a == 0.0f);
  c = sample_gradient(h, std::nextafterf(0.6f, 0.0f));
  CHECK(c.b > 0.9f); CHECK(c.a > 0.99f);
  c = sample_gradient(h, std::nextafterf(0.6f, 1.0f));
  CHECK(c.g > 0.7f); CHECK(c.a < 0.01f);
  // Three stops at one position: the last wins.
  Gradient t = two(Space::Linear);
  t.count = 4;
  t.stops[0] = {0.5f, {1, 0, 0, 1}};
  t.stops[1] = {0.5f, {0, 1, 0, 1}};
  t.stops[2] = {0.5f, {0, 0, 1, 1}};
  t.stops[3] = {0.5f, {1, 1, 1, 1}};
  CHECK(sample_gradient(t, 0.5f).r == 1.0f);
  CHECK(sample_gradient(t, 0.5f).g == 1.0f);
  CHECK(sample_gradient(t, 0.4f).g == 0.0f);  // before: first stop holds
  CHECK(sample_gradient(t, 0.4f).r == 1.0f);
  CHECK(sample_gradient(t, 0.6f).b == 1.0f);
  // Hard step at the very end and at the very start.
  Gradient e = two(Space::Srgb);
  e.stops[0] = {0.0f, {1, 0, 0, 1}};
  e.stops[1] = {0.0f, {0, 1, 0, 1}};
  CHECK(sample_gradient(e, 0.0f).g == 1.0f);  // the later of equal stops wins
  CHECK(sample_gradient(e, 0.5f).g == 1.0f);
  // Midpoints differ by space: grey between black and white.
  const float mid_s = sample_gradient(two(Space::Srgb), 0.5f).r;
  const float mid_l = sample_gradient(two(Space::Linear), 0.5f).r;
  const float mid_o = sample_gradient(two(Space::Oklab), 0.5f).r;
  CHECK(mid_s == Approx(0.5).epsilon(1e-6));
  CHECK(mid_l == Approx(0.73535698305244945).epsilon(1e-6));  // linear 0.5 in sRGB
  CHECK(mid_o == Approx(0.38857285904633442).epsilon(2e-6));  // OKLab lightness 0.5 is linear 0.125
  CHECK(mid_o < mid_s);
  CHECK(mid_s < mid_l);
  // Invalid gradient returns transparent black.
  Gradient bad = two(Space::Srgb);
  bad.count = 1;
  c = sample_gradient(bad, 0.5f);
  CHECK(c.r == 0.0f); CHECK(c.g == 0.0f); CHECK(c.b == 0.0f); CHECK(c.a == 0.0f);
}

TEST_CASE("gradient ramps are monotonic in every space", "[color][gradient]") {
  for (Space s : {Space::Srgb, Space::Linear, Space::Oklab}) {
    const Gradient g = two(s);
    Rgba prev = sample_gradient(g, 0.0f);
    CHECK(prev.r == 0.0f);
    for (int i = 1; i <= 2048; ++i) {
      const Rgba c = sample_gradient(g, static_cast<float>(i) / 2048.0f);
      REQUIRE(c.r >= prev.r);
      REQUIRE(c.g >= prev.g);
      REQUIRE(c.b >= prev.b);
      REQUIRE((c.r >= 0.0f && c.r <= 1.0f && c.a == 1.0f));
      prev = c;
    }
    CHECK(prev.r == 1.0f);
  }
  // Baked ramp is monotonic too.
  std::vector<std::uint8_t> buf(256 * 4);
  for (Space s : {Space::Srgb, Space::Linear, Space::Oklab}) {
    REQUIRE(bake_gradient_u8(two(s), 256, buf.data()));
    for (int i = 1; i < 256; ++i) REQUIRE(buf[i * 4] >= buf[(i - 1) * 4]);
    CHECK(buf[0] == 0);
    CHECK(buf[255 * 4] == 255);
    CHECK(buf[255 * 4 + 3] == 255);
  }
}

TEST_CASE("bake_gradient_u8", "[color][bake]") {
  std::uint8_t out[8 * 4];
  for (auto& b : out) b = 0xAB;
  Gradient bad = two(Space::Srgb);
  bad.count = 1;
  CHECK_FALSE(bake_gradient_u8(bad, 8, out));
  CHECK_FALSE(bake_gradient_u8(two(Space::Srgb), 0, out));
  CHECK_FALSE(bake_gradient_u8(two(Space::Srgb), 65537, out));
  for (std::uint8_t b : out) CHECK(b == 0xAB);  // untouched
  CHECK(bake_gradient_u8(two(Space::Srgb), 1, out));
  CHECK(out[0] == 0); CHECK(out[3] == 255);  // n == 1 gives t = 0
  CHECK(bake_gradient_u8(two(Space::Srgb), 2, out));
  CHECK(out[0] == 0); CHECK(out[4] == 255);
  CHECK(bake_gradient_u8(two(Space::Srgb), 3, out));
  CHECK(out[4] == 128);  // t = 0.5
  // Largest size works.
  std::vector<std::uint8_t> big(65536 * 4);
  CHECK(bake_gradient_u8(two(Space::Linear), 65536, big.data()));
  CHECK(big[65535 * 4] == 255);
}

TEST_CASE("digest_u8 is FNV-1a 64 over the bytes in order", "[color][digest]") {
  CHECK(digest_u8(nullptr, 0) == 0xcbf29ce484222325ull);
  const std::uint8_t a[] = {'a'};
  CHECK(digest_u8(a, 1) == 0xaf63dc4c8601ec8cull);
  const std::uint8_t foobar[] = {'f', 'o', 'o', 'b', 'a', 'r'};
  CHECK(digest_u8(foobar, 6) == 0x85944171f73967e8ull);
  const std::uint8_t rev[] = {'r', 'a', 'b', 'o', 'o', 'f'};
  CHECK(digest_u8(rev, 6) != digest_u8(foobar, 6));  // order matters
}

TEST_CASE("golden digests of a baked 256 entry ramp", "[color][digest]") {
  const std::uint64_t s = ramp_digest(Space::Srgb), l = ramp_digest(Space::Linear), o = ramp_digest(Space::Oklab);
  INFO(std::hex << "srgb 0x" << s << " linear 0x" << l << " oklab 0x" << o);
  CHECK(s == kGoldenSrgb);
  CHECK(l == kGoldenLinear);
  CHECK(o == kGoldenOklab);
  CHECK(s != l);
  CHECK(l != o);
  CHECK(ramp_digest(Space::Srgb) == s);  // repeatable
}

TEST_CASE("performance: bake large ramps", "[color][perf]") {
  const auto start = std::chrono::steady_clock::now();
  std::vector<std::uint8_t> buf(65536 * 4);
  Gradient g;
  g.count = kMaxStops;
  for (std::size_t i = 0; i < kMaxStops; ++i) {
    const float p = static_cast<float>(i) / 15.0f;
    g.stops[i] = {p, {(i % 3) / 2.0f, (i % 5) / 4.0f, (i % 7) / 6.0f, 1.0f - static_cast<float>(i % 2) * 0.5f}};
  }
  std::uint64_t sum = 0;
  for (Space s : {Space::Srgb, Space::Linear, Space::Oklab}) {
    g.space = s;
    for (int rep = 0; rep < 3; ++rep) {
      REQUIRE(bake_gradient_u8(g, 65536, buf.data()));
      sum ^= digest_u8(buf.data(), buf.size());
    }
  }
  CHECK(sum != 0);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("took " << secs << " s");
  CHECK(secs < 10.0);
}
