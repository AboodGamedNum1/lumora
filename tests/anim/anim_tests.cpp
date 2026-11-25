// Tests for core/anim. Local expectations are hand-computed (see the comments); nothing here reads a file or the
// network. Tolerance for functions that use libm (slerp: acos and sin) is 1e-6, see docs/animation.md.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "animation.h"

using Catch::Approx;
using namespace lumora::anim;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr double kTol = 1e-6;

Track make(int comps, Interp interp, std::vector<float> times, std::vector<float> values, bool rot = false) {
  Track t;
  t.target = "layer/test/position";
  t.components = comps;
  t.is_rotation = rot;
  t.interp = interp;
  t.times = std::move(times);
  t.values = std::move(values);
  return t;
}

std::vector<float> sample(const Track& t, float time) {
  std::vector<float> out(static_cast<std::size_t>(t.components) + 1, -777.0f);  // guard slot behind the value
  REQUIRE(sample_track(t, time, out.data()));
  CHECK(out.back() == -777.0f);  // nothing written past components
  out.pop_back();
  return out;
}

float s1(const Track& t, float time) { return sample(t, time)[0]; }

double len4(const std::vector<float>& q) {
  return std::sqrt(static_cast<double>(q[0]) * q[0] + static_cast<double>(q[1]) * q[1] +
                   static_cast<double>(q[2]) * q[2] + static_cast<double>(q[3]) * q[3]);
}

// Same rotation up to sign (q and -q), within tolerance.
bool same_rotation(const std::vector<float>& a, const std::vector<float>& b, double tol) {
  double plus = 0, minus = 0;
  for (int i = 0; i < 4; ++i) {
    plus = std::fmax(plus, std::fabs(static_cast<double>(a[i]) - b[i]));
    minus = std::fmax(minus, std::fabs(static_cast<double>(a[i]) + b[i]));
  }
  return plus <= tol || minus <= tol;
}

// Tiny deterministic generator (xorshift64*), so the random tests do not depend on a library distribution.
struct Rng {
  std::uint64_t s;
  std::uint64_t next() {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 2685821657736338717ull;
  }
  double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }  // [0, 1)
  float range(double lo, double hi) { return static_cast<float>(lo + (hi - lo) * unit()); }
  int pick(int n) { return static_cast<int>(next() % static_cast<std::uint64_t>(n)); }
};

const float kS = 0.70710678118654752f;  // sqrt(0.5)

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("every TrackStatus is reachable", "[anim][status]") {
  const Track good = make(1, Interp::Linear, {0, 1}, {0, 1});
  CHECK(validate_track(good) == TrackStatus::Ok);

  CHECK(validate_track(make(1, Interp::Linear, {}, {})) == TrackStatus::NoKeys);
  CHECK(validate_track(Track{}) == TrackStatus::NoKeys);

  Track many = make(1, Interp::Linear, std::vector<float>(kMaxKeysPerTrack + 1, 0.0f), {});
  CHECK(validate_track(many) == TrackStatus::TooManyKeys);  // checked before the value count is compared

  CHECK(validate_track(make(0, Interp::Linear, {0}, {})) == TrackStatus::BadComponents);
  CHECK(validate_track(make(5, Interp::Linear, {0}, {1, 2, 3, 4, 5})) == TrackStatus::BadComponents);
  CHECK(validate_track(make(-1, Interp::Linear, {0}, {1})) == TrackStatus::BadComponents);
  CHECK(validate_track(make(3, Interp::Linear, {0}, {0, 0, 1}, true)) == TrackStatus::BadComponents);
  CHECK(validate_track(make(std::numeric_limits<int>::max(), Interp::Linear, {0}, {1})) == TrackStatus::BadComponents);

  CHECK(validate_track(make(2, Interp::Linear, {0, 1}, {1, 2, 3})) == TrackStatus::BadValueCount);
  CHECK(validate_track(make(1, Interp::CubicSpline, {0, 1}, {0, 0, 0, 1, 1, 1, 9})) == TrackStatus::BadValueCount);
  CHECK(validate_track(make(1, Interp::Step, {0, 1}, {1})) == TrackStatus::BadValueCount);

  CHECK(validate_track(make(1, Interp::Linear, {0, 0}, {1, 2})) == TrackStatus::TimesNotIncreasing);
  CHECK(validate_track(make(1, Interp::Linear, {1, 0}, {1, 2})) == TrackStatus::TimesNotIncreasing);
  CHECK(validate_track(make(1, Interp::Linear, {-1, 0}, {1, 2})) == TrackStatus::TimesNotIncreasing);
  CHECK(validate_track(make(1, Interp::Linear, {-0.0f, 1}, {1, 2})) == TrackStatus::Ok);  // negative zero is zero
  CHECK(validate_track(make(1, Interp::Linear, {0, kNaN}, {1, 2})) == TrackStatus::TimesNotIncreasing);
  CHECK(validate_track(make(1, Interp::Linear, {0, kInf}, {1, 2})) == TrackStatus::TimesNotIncreasing);
  CHECK(validate_track(make(1, Interp::Linear, {kNaN}, {1})) == TrackStatus::TimesNotIncreasing);

  CHECK(validate_track(make(1, Interp::Linear, {0, 1}, {1, kNaN})) == TrackStatus::NonFinite);
  CHECK(validate_track(make(1, Interp::Linear, {0, 1}, {kInf, 1})) == TrackStatus::NonFinite);
  CHECK(validate_track(make(1, Interp::CubicSpline, {0}, {kNaN, 0, 0})) == TrackStatus::NonFinite);  // a tangent

  CHECK(validate_track(make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, 0, 0}, true)) == TrackStatus::ZeroQuaternion);
  // Cubic: only the value element of a key must be non-zero, tangents may be zero.
  CHECK(validate_track(make(4, Interp::CubicSpline, {0}, {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}, true)) == TrackStatus::Ok);
  CHECK(validate_track(make(4, Interp::CubicSpline, {0}, {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1}, true)) ==
        TrackStatus::ZeroQuaternion);

  Track bad = good;
  bad.target = "";
  CHECK(validate_track(bad) == TrackStatus::BadTarget);
  bad.target = std::string(129, 'a');
  CHECK(validate_track(bad) == TrackStatus::BadTarget);
  bad.target = std::string(128, 'a');
  CHECK(validate_track(bad) == TrackStatus::Ok);
  bad.target = "tab\there";
  CHECK(validate_track(bad) == TrackStatus::BadTarget);
  bad.target = std::string("nul\0byte", 8);
  CHECK(validate_track(bad) == TrackStatus::BadTarget);
  bad.target = "del\x7f";
  CHECK(validate_track(bad) == TrackStatus::BadTarget);
}

TEST_CASE("target names must be clean UTF-8", "[anim][status]") {
  Track t = make(1, Interp::Linear, {0}, {1});
  const char* good[] = {"a", "caf\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80", "sp ace/x-y_z"};
  for (const char* g : good) {
    t.target = g;
    INFO(g);
    CHECK(validate_track(t) == TrackStatus::Ok);
  }
  const char* bad[] = {"\xC3",          // truncated
                       "\xC0\x80",      // overlong
                       "\xE0\x80\x80",  // overlong
                       "\xED\xA0\x80",  // surrogate
                       "\xF4\x90\x80\x80",  // above U+10FFFF
                       "\xF8\x88\x80\x80\x80", "\x80", "\xFF", "a\xE2\x82", "a\xC3(" };
  for (const char* b : bad) {
    t.target = b;
    INFO(b);
    CHECK(validate_track(t) == TrackStatus::BadTarget);
  }
  // 128 bytes made of 2-byte characters is fine, 129 is not.
  t.target.clear();
  for (int i = 0; i < 64; ++i) t.target += "\xC3\xA9";
  CHECK(validate_track(t) == TrackStatus::Ok);
  t.target += "a";
  CHECK(validate_track(t) == TrackStatus::BadTarget);
}

TEST_CASE("status texts are fixed and distinct", "[anim][status]") {
  const TrackStatus all[] = {TrackStatus::Ok,           TrackStatus::NoKeys,       TrackStatus::TooManyKeys,
                             TrackStatus::BadComponents, TrackStatus::BadValueCount, TrackStatus::TimesNotIncreasing,
                             TrackStatus::NonFinite,     TrackStatus::ZeroQuaternion, TrackStatus::BadTarget};
  std::vector<std::string> seen;
  for (TrackStatus s : all) {
    const char* text = track_status_text(s);
    REQUIRE(text != nullptr);
    CHECK(text[0] != '\0');
    for (const std::string& o : seen) CHECK(o != text);
    seen.push_back(text);
  }
  CHECK(track_status_text(static_cast<TrackStatus>(1234)) != nullptr);
}

TEST_CASE("durations", "[anim][duration]") {
  CHECK(track_duration(make(1, Interp::Linear, {0, 1.5f, 4}, {0, 1, 2})) == 4.0f);
  CHECK(track_duration(Track{}) == 0.0f);
  CHECK(track_duration(make(1, Interp::Linear, {0, 2}, {0})) == 0.0f);  // invalid track
  Clip c;
  CHECK(clip_duration(c) == 0.0f);
  c.tracks.push_back(make(1, Interp::Linear, {0, 3}, {0, 1}));
  c.tracks.push_back(make(1, Interp::Linear, {0, 7}, {0, 1}));
  c.tracks.push_back(make(1, Interp::Linear, {0, 9}, {0}));  // invalid, ignored
  c.tracks.push_back(make(1, Interp::Step, {5}, {1}));
  CHECK(clip_duration(c) == 7.0f);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("sample_track refuses invalid tracks and leaves out untouched", "[anim][sample]") {
  float out[4] = {9, 9, 9, 9};
  CHECK_FALSE(sample_track(Track{}, 0.0f, out));
  CHECK_FALSE(sample_track(make(1, Interp::Linear, {0, 1}, {0, kNaN}), 0.5f, out));
  CHECK_FALSE(sample_track(make(1, Interp::Linear, {0, 1}, {0, 1}), 0.5f, nullptr));
  for (float v : out) CHECK(v == 9.0f);
}

TEST_CASE("Step holds the left key", "[anim][sample]") {
  const Track t = make(1, Interp::Step, {0, 1, 2}, {5, 10, 20});
  CHECK(s1(t, -3.0f) == 5.0f);
  CHECK(s1(t, 0.0f) == 5.0f);
  CHECK(s1(t, 0.25f) == 5.0f);
  CHECK(s1(t, 0.5f) == 5.0f);
  CHECK(s1(t, 0.999999f) == 5.0f);
  CHECK(s1(t, 1.0f) == 10.0f);
  CHECK(s1(t, 1.5f) == 10.0f);
  CHECK(s1(t, 2.0f) == 20.0f);
  CHECK(s1(t, 100.0f) == 20.0f);
}

TEST_CASE("Linear: key, quarter and mid times", "[anim][sample]") {
  // value(t) = 10 * t on [0, 1], then 10 + 20 * (t - 1) on [1, 2]
  const Track t = make(1, Interp::Linear, {0, 1, 2}, {0, 10, 30});
  CHECK(s1(t, 0.0f) == 0.0f);
  CHECK(s1(t, 0.25f) == 2.5f);
  CHECK(s1(t, 0.5f) == 5.0f);
  CHECK(s1(t, 0.75f) == 7.5f);
  CHECK(s1(t, 1.0f) == 10.0f);
  CHECK(s1(t, 1.25f) == 15.0f);
  CHECK(s1(t, 1.5f) == 20.0f);
  CHECK(s1(t, 2.0f) == 30.0f);
  CHECK(s1(t, -1.0f) == 0.0f);
  CHECK(s1(t, 9.0f) == 30.0f);
  // Unequal intervals: keys at 0, 4: quarter is t = 1.
  const Track u = make(1, Interp::Linear, {0, 4}, {-2, 6});
  CHECK(s1(u, 1.0f) == 0.0f);
  CHECK(s1(u, 2.0f) == 2.0f);
  CHECK(s1(u, 3.0f) == 4.0f);
}

TEST_CASE("Linear and Step with 2, 3 and 4 components", "[anim][sample]") {
  const Track t2 = make(2, Interp::Linear, {0, 2}, {0, 10, 4, 20});
  auto v = sample(t2, 0.5f);
  CHECK(v[0] == 1.0f);
  CHECK(v[1] == 12.5f);
  const Track t3 = make(3, Interp::Linear, {1, 3}, {1, 2, 3, 3, 2, 1});
  v = sample(t3, 2.0f);
  CHECK(v[0] == 2.0f);
  CHECK(v[1] == 2.0f);
  CHECK(v[2] == 2.0f);
  const Track t4 = make(4, Interp::Step, {0, 1}, {1, 2, 3, 4, 5, 6, 7, 8});
  v = sample(t4, 0.5f);
  CHECK(v == std::vector<float>({1, 2, 3, 4}));
  v = sample(t4, 1.0f);
  CHECK(v == std::vector<float>({5, 6, 7, 8}));
  // Out of range holds, exact.
  v = sample(t3, 0.0f);
  CHECK(v == std::vector<float>({1, 2, 3}));
  v = sample(t3, 1e30f);
  CHECK(v == std::vector<float>({3, 2, 1}));
}

TEST_CASE("a single key holds everywhere", "[anim][sample]") {
  const Track t = make(2, Interp::Linear, {3}, {7, 8});
  for (float time : {-5.0f, 0.0f, 3.0f, 100.0f}) {
    const auto v = sample(t, time);
    CHECK(v[0] == 7.0f);
    CHECK(v[1] == 8.0f);
  }
}

TEST_CASE("non-finite sample times", "[anim][sample]") {
  const Track t = make(1, Interp::Linear, {1, 2}, {10, 20});
  CHECK(s1(t, kNaN) == 10.0f);    // NaN holds the first key
  CHECK(s1(t, kInf) == 20.0f);    // +inf holds the last key
  CHECK(s1(t, -kInf) == 10.0f);   // -inf holds the first key
}

TEST_CASE("Linear never overflows to infinity", "[anim][sample]") {
  const float big = std::numeric_limits<float>::max();
  const Track t = make(1, Interp::Linear, {0, 1}, {-big, big});
  CHECK(s1(t, 0.5f) == 0.0f);
  const float q = s1(t, 0.75f);
  CHECK(std::isfinite(q));
  CHECK(q == Approx(big * 0.5).epsilon(1e-6));
}

TEST_CASE("CubicSpline uses the glTF Hermite formula", "[anim][sample][cubic]") {
  // Per key: in-tangent, value, out-tangent. Key 0: (0, 0, 1). Key 1: (2, 1, 0).
  // With dt = 1: p(u) = h10 * 1 + h01 * 1 + h11 * 2, h10 = u^3-2u^2+u, h01 = -2u^3+3u^2, h11 = u^3-u^2.
  const Track t = make(1, Interp::CubicSpline, {0, 1}, {0, 0, 1, 2, 1, 0});
  CHECK(s1(t, 0.0f) == 0.0f);
  CHECK(s1(t, 0.25f) == Approx(0.203125).margin(1e-7));  // .140625 + .15625 - .09375
  CHECK(s1(t, 0.5f) == Approx(0.375).margin(1e-7));      // .125 + .5 - .25
  CHECK(s1(t, 0.75f) == Approx(0.609375).margin(1e-7));  // .046875 + .84375 - .28125
  CHECK(s1(t, 1.0f) == 1.0f);
  CHECK(s1(t, -2.0f) == 0.0f);  // hold the key value, not a tangent
  CHECK(s1(t, 2.0f) == 1.0f);

  // Tangents are scaled by the interval length: same keys over [0, 2].
  // u = 0.5: h10 * dt * 1 + h01 * 1 + h11 * dt * 2 = .125*2 + .5 + (-.125)*2*2 = .25 + .5 - .5 = .25
  const Track w = make(1, Interp::CubicSpline, {0, 2}, {0, 0, 1, 2, 1, 0});
  CHECK(s1(w, 1.0f) == Approx(0.25).margin(1e-7));
  // Zero tangents give smoothstep: 3u^2 - 2u^3 between 0 and 8.
  const Track z = make(1, Interp::CubicSpline, {0, 1}, {0, 0, 0, 0, 8, 0});
  CHECK(s1(z, 0.25f) == Approx(8 * 0.15625).margin(1e-6));
  CHECK(s1(z, 0.5f) == Approx(4.0).margin(1e-6));
  // Two components and three keys: the second interval uses keys 1 and 2.
  const Track m = make(2, Interp::CubicSpline, {0, 1, 3},
                       {0, 0, 0, 0, 0, 0,   // key 0: in, value, out
                        0, 0, 5, 5, 0, 0,   // key 1: in = (0,0), value = (5,5), out = (0,0)
                        0, 0, 9, 1, 0, 0}); // key 2: value = (9,1)
  const auto a = sample(m, 1.0f);
  CHECK(a[0] == 5.0f);
  CHECK(a[1] == 5.0f);
  const auto b = sample(m, 2.0f);  // u = 0.5 of dt = 2, zero tangents: halfway
  CHECK(b[0] == Approx(7.0).margin(1e-6));
  CHECK(b[1] == Approx(3.0).margin(1e-6));
}

TEST_CASE("CubicSpline in-tangent of the next key is used", "[anim][sample][cubic]") {
  // Only the in-tangent of key 1 is non-zero: p(u) = h01 * 1 + h11 * dt * m1 with value 0 -> 1.
  const Track t = make(1, Interp::CubicSpline, {0, 1}, {0, 0, 0, 4, 1, 0});
  const double u = 0.5;
  const double expect = (-2 * u * u * u + 3 * u * u) * 1.0 + (u * u * u - u * u) * 4.0;  // 0.5 - 0.5
  CHECK(s1(t, 0.5f) == Approx(expect).margin(1e-7));
  const double u2 = 0.25;
  const double e2 = (-2 * u2 * u2 * u2 + 3 * u2 * u2) + (u2 * u2 * u2 - u2 * u2) * 4.0;
  CHECK(s1(t, 0.25f) == Approx(e2).margin(1e-7));
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("rotation Linear: slerp endpoints, midpoint and quarter", "[anim][sample][rotation]") {
  // Identity to 90 degrees about z: (0, 0, sin 45, cos 45).
  const Track t = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, kS, kS}, true);
  auto q = sample(t, 0.0f);
  CHECK(q == std::vector<float>({0, 0, 0, 1}));
  q = sample(t, 1.0f);
  CHECK(q[2] == Approx(kS).margin(1e-7));
  CHECK(q[3] == Approx(kS).margin(1e-7));
  q = sample(t, 0.5f);  // 45 degrees: sin 22.5 = 0.38268343236509, cos 22.5 = 0.92387953251129
  CHECK(q[0] == Approx(0.0).margin(kTol));
  CHECK(q[1] == Approx(0.0).margin(kTol));
  CHECK(q[2] == Approx(0.38268343236509).margin(kTol));
  CHECK(q[3] == Approx(0.92387953251129).margin(kTol));
  q = sample(t, 0.25f);  // 22.5 degrees: sin 11.25 = 0.19509032201613, cos 11.25 = 0.98078528040323
  CHECK(q[2] == Approx(0.19509032201613).margin(kTol));
  CHECK(q[3] == Approx(0.98078528040323).margin(kTol));
  q = sample(t, 0.75f);  // 67.5 degrees: sin 33.75 = 0.55557023301960, cos 33.75 = 0.83146961230255
  CHECK(q[2] == Approx(0.55557023301960).margin(kTol));
  CHECK(q[3] == Approx(0.83146961230255).margin(kTol));
}

TEST_CASE("rotation: keys are normalized and sampled unit length", "[anim][sample][rotation]") {
  const Track t = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 5, 0, 0, 3, 3}, true);  // lengths 5 and 4.24
  CHECK(len4(sample(t, 0.0f)) == Approx(1.0).margin(kTol));
  CHECK(len4(sample(t, 1.0f)) == Approx(1.0).margin(kTol));
  for (int i = 1; i < 20; ++i) CHECK(len4(sample(t, static_cast<float>(i) / 20.0f)) == Approx(1.0).margin(kTol));
  const Track step = make(4, Interp::Step, {0, 1}, {0, 0, 0, 5, 0, 0, 3, 3}, true);
  const auto q = sample(step, 0.5f);
  CHECK(q[3] == Approx(1.0).margin(kTol));
  CHECK(len4(q) == Approx(1.0).margin(kTol));
  const Track one = make(4, Interp::Linear, {2}, {0, 0, 0, 2}, true);
  CHECK(sample(one, 0.0f)[3] == Approx(1.0).margin(kTol));
}

TEST_CASE("rotation: shortest path flips the sign", "[anim][sample][rotation]") {
  // Second key is the negated 90 degree rotation: the same orientation, so the result is the 45 degree one
  // (or its negation) and never the long way round.
  const Track t = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, -kS, -kS}, true);
  const auto q = sample(t, 0.5f);
  CHECK(same_rotation(q, {0, 0, 0.38268343f, 0.92387953f}, 1e-6));
  // The result must be closer to the first key than to its negation: positive dot with identity.
  CHECK(q[3] > 0.9f);
  // Long way would pass through w < 0: a 270 degree rotation sampled at 1/3 must be 30 degrees the short way.
  // 270 degrees about z = (0, 0, sin 135, cos 135) = (0, 0, kS, -kS), the short way is -90 degrees.
  const Track l = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, kS, -kS}, true);
  const auto h = sample(l, 0.5f);  // half of -90 degrees = -45 degrees: z = -sin 22.5 (or the negation)
  CHECK(same_rotation(h, {0, 0, -0.38268343f, 0.92387953f}, 1e-6));
}

TEST_CASE("rotation: 180 degrees apart and antipodal keys", "[anim][sample][rotation]") {
  // Identity to 180 degrees about z: the midpoint is 90 degrees, (0, 0, kS, kS).
  const Track t = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, 1, 0}, true);
  const auto q = sample(t, 0.5f);
  CHECK(same_rotation(q, {0, 0, kS, kS}, 1e-6));
  CHECK(len4(q) == Approx(1.0).margin(kTol));
  // q and -q are the same orientation: any sample is that orientation, unit length, no NaN.
  const Track a = make(4, Interp::Linear, {0, 1}, {0, 0, kS, kS, 0, 0, -kS, -kS}, true);
  for (int i = 0; i <= 8; ++i) {
    const auto r = sample(a, static_cast<float>(i) / 8.0f);
    CHECK(same_rotation(r, {0, 0, kS, kS}, 1e-6));
    CHECK(len4(r) == Approx(1.0).margin(kTol));
  }
  // 180 degrees about x vs about y (dot = 0): the midpoint has unit length and equal angle to both ends.
  const Track b = make(4, Interp::Linear, {0, 1}, {1, 0, 0, 0, 0, 1, 0, 0}, true);
  const auto m = sample(b, 0.5f);
  CHECK(len4(m) == Approx(1.0).margin(kTol));
  CHECK(m[0] == Approx(kS).margin(kTol));
  CHECK(m[1] == Approx(kS).margin(kTol));
}

TEST_CASE("rotation: nearly parallel keys use normalized lerp", "[anim][sample][rotation]") {
  const float e = 1e-5f;
  const Track t = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, e, 1}, true);
  for (int i = 0; i <= 10; ++i) {
    const auto q = sample(t, static_cast<float>(i) / 10.0f);
    for (float v : q) CHECK(std::isfinite(v));
    CHECK(len4(q) == Approx(1.0).margin(kTol));
    CHECK(q[2] == Approx(static_cast<double>(e) * i / 10.0).margin(1e-7));
  }
  // Identical keys: constant result, no division by a zero sine.
  const Track same = make(4, Interp::Linear, {0, 1}, {0, 0, kS, kS, 0, 0, kS, kS}, true);
  const auto q = sample(same, 0.5f);
  CHECK(q[2] == Approx(kS).margin(1e-7));
  CHECK(q[3] == Approx(kS).margin(1e-7));
  // The switch point (dot just above and just below 0.9995) gives continuous results.
  const double theta = 2.0 * std::acos(0.9995);  // angle between the quaternions: dot = cos(theta / 2)
  const float below = static_cast<float>(std::sin(theta / 2 * 1.02)), above = static_cast<float>(std::sin(theta / 2 * 0.98));
  const float cb = std::cos(theta / 2 * 1.02), ca = std::cos(theta / 2 * 0.98);
  const Track lo = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, below, cb}, true);
  const Track hi = make(4, Interp::Linear, {0, 1}, {0, 0, 0, 1, 0, 0, above, ca}, true);
  const auto a = sample(lo, 0.5f);
  const auto b = sample(hi, 0.5f);
  CHECK(a[2] == Approx(std::sin(theta / 2 * 1.02 / 2)).margin(1e-5));  // half the angle
  CHECK(b[2] == Approx(std::sin(theta / 2 * 0.98 / 2)).margin(1e-5));
}

TEST_CASE("rotation Step and CubicSpline", "[anim][sample][rotation]") {
  const Track s = make(4, Interp::Step, {0, 1}, {0, 0, 0, 1, 0, 0, kS, kS}, true);
  CHECK(sample(s, 0.99f) == std::vector<float>({0, 0, 0, 1}));
  CHECK(sample(s, 1.0f)[2] == Approx(kS).margin(1e-7));
  // Cubic with zero tangents between identity and 90 degrees: u = 0.5 is the plain average, normalized.
  // average = (0, 0, kS/2, (1 + kS)/2) normalized: z = 0.35355/0.92388 = 0.38268, w = 0.85355/0.92388 = 0.92388.
  const Track c = make(4, Interp::CubicSpline, {0, 1},
                       {0, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,
                        0, 0, 0, 0,  0, 0, kS, kS, 0, 0, 0, 0}, true);
  const auto q = sample(c, 0.5f);
  CHECK(q[2] == Approx(0.38268343236509).margin(kTol));
  CHECK(q[3] == Approx(0.92387953251129).margin(kTol));
  CHECK(len4(q) == Approx(1.0).margin(kTol));
  CHECK(sample(c, 0.0f) == std::vector<float>({0, 0, 0, 1}));
  // Tangents that cancel the value out give a zero vector: the left key is returned instead of NaN.
  const Track z = make(4, Interp::CubicSpline, {0, 1},
                       {0, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,
                        0, 0, 0, 0,  0, 0, 0, -1, 0, 0, 0, 0}, true);
  const auto r = sample(z, 0.5f);  // average of (0,0,0,1) and (0,0,0,-1) is zero
  for (float v : r) CHECK(std::isfinite(v));
  CHECK(len4(r) == Approx(1.0).margin(kTol));
}

TEST_CASE("sampling picks the right interval in a long track", "[anim][sample]") {
  std::vector<float> times, values;
  for (int i = 0; i < 1000; ++i) {
    times.push_back(static_cast<float>(i) * 0.5f);
    values.push_back(static_cast<float>(i) * 2.0f);  // value = 4 * time
  }
  const Track t = make(1, Interp::Linear, times, values);
  for (int i = 0; i < 999; ++i) {
    CHECK(s1(t, static_cast<float>(i) * 0.5f) == static_cast<float>(i) * 2.0f);
    CHECK(s1(t, static_cast<float>(i) * 0.5f + 0.25f) == static_cast<float>(i) * 2.0f + 1.0f);
  }
  CHECK(s1(t, 1e9f) == 1998.0f);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("apply_loop table", "[anim][loop]") {
  struct Row { float time, duration; float clamp, repeat, pingpong; };
  const Row rows[] = {
      // time, duration, Clamp, Repeat, PingPong
      {0.0f, 2.0f, 0.0f, 0.0f, 0.0f},
      {0.5f, 2.0f, 0.5f, 0.5f, 0.5f},
      {1.0f, 2.0f, 1.0f, 1.0f, 1.0f},
      {1.5f, 2.0f, 1.5f, 1.5f, 1.5f},
      {2.0f, 2.0f, 2.0f, 0.0f, 2.0f},   // exactly the duration
      {2.5f, 2.0f, 2.0f, 0.5f, 1.5f},
      {3.0f, 2.0f, 2.0f, 1.0f, 1.0f},
      {4.0f, 2.0f, 2.0f, 0.0f, 0.0f},   // two durations: pingpong is back at the start
      {4.5f, 2.0f, 2.0f, 0.5f, 0.5f},
      {5.0f, 2.0f, 2.0f, 1.0f, 1.0f},
      {6.0f, 2.0f, 2.0f, 0.0f, 2.0f},
      {7.5f, 2.0f, 2.0f, 1.5f, 0.5f},
      {8.0f, 2.0f, 2.0f, 0.0f, 0.0f},
      {0.25f, 1.0f, 0.25f, 0.25f, 0.25f},
      {1.25f, 1.0f, 1.0f, 0.25f, 0.75f},
  };
  for (const Row& r : rows) {
    INFO("time " << r.time << " duration " << r.duration);
    CHECK(apply_loop(r.time, r.duration, Loop::Clamp) == r.clamp);
    CHECK(apply_loop(r.time, r.duration, Loop::Repeat) == r.repeat);
    CHECK(apply_loop(r.time, r.duration, Loop::PingPong) == r.pingpong);
  }
}

TEST_CASE("apply_loop: negative, zero, huge and non-finite inputs", "[anim][loop]") {
  const Loop modes[] = {Loop::Clamp, Loop::Repeat, Loop::PingPong};
  const float inf = kInf;
  const float max = std::numeric_limits<float>::max();
  for (Loop m : modes) {
    CHECK(apply_loop(-1.0f, 2.0f, m) == 0.0f);
    CHECK(apply_loop(-0.0f, 2.0f, m) == 0.0f);
    CHECK(apply_loop(-1e30f, 2.0f, m) == 0.0f);
    CHECK(apply_loop(-inf, 2.0f, m) == 0.0f);
    CHECK(apply_loop(inf, 2.0f, m) == 0.0f);
    CHECK(apply_loop(kNaN, 2.0f, m) == 0.0f);
    CHECK(apply_loop(1.0f, 0.0f, m) == 0.0f);
    CHECK(apply_loop(1.0f, -2.0f, m) == 0.0f);
    CHECK(apply_loop(1.0f, kNaN, m) == 0.0f);
    CHECK(apply_loop(1.0f, inf, m) == 0.0f);
    CHECK(apply_loop(1.0f, -inf, m) == 0.0f);
    CHECK(apply_loop(0.0f, 0.0f, m) == 0.0f);
    CHECK(apply_loop(kNaN, kNaN, m) == 0.0f);
    for (float time : {1e30f, 1e20f, max, 123456.789f, 3.0e7f}) {
      for (float d : {1.0f, 2.0f, 0.1f, 1e-30f, 1e30f, max}) {
        const float r = apply_loop(time, d, m);
        INFO("time " << time << " duration " << d);
        CHECK(std::isfinite(r));
        CHECK(r >= 0.0f);
        CHECK(r <= d);
      }
    }
  }
  CHECK(apply_loop(1e30f, 2.0f, Loop::Clamp) == 2.0f);
  CHECK(apply_loop(max, max, Loop::Clamp) == max);
  CHECK(apply_loop(max, max, Loop::Repeat) == 0.0f);
  CHECK(apply_loop(max, max, Loop::PingPong) == max);  // 2 * max does not overflow in double
  CHECK(apply_loop(1e30f, 1.0f, Loop::Repeat) == 0.0f);  // 1e30f is an integer multiple of 1
  CHECK(apply_loop(1e30f, 1.0f, Loop::PingPong) == 0.0f);
  CHECK(apply_loop(1e30f, 3.0f, Loop::Repeat) == Approx(std::fmod(static_cast<double>(1e30f), 3.0)));
  CHECK(apply_loop(1.0f, 1e30f, Loop::Repeat) == 1.0f);
  CHECK(apply_loop(0.5f, 1e-30f, Loop::Clamp) == 1e-30f);
  CHECK(apply_loop(1.0f, 2.0f, static_cast<Loop>(77)) == 0.0f);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("random tracks: finite output, continuity and unit rotations (fixed seed)", "[anim][random]") {
  Rng rng{0x9E3779B97F4A7C15ull};
  int checked = 0;
  for (int n = 0; n < 240; ++n) {
    const bool rot = rng.pick(4) == 0;
    const int comps = rot ? 4 : 1 + rng.pick(4);
    const Interp interp = static_cast<Interp>(rng.pick(3));
    const int keys = 1 + rng.pick(12);
    std::vector<float> times;
    float time = rng.range(0, 2);
    for (int k = 0; k < keys; ++k) {
      times.push_back(time);
      time += rng.range(0.05, 3);
    }
    const std::size_t per = static_cast<std::size_t>(comps) * (interp == Interp::CubicSpline ? 3 : 1);
    std::vector<float> values(static_cast<std::size_t>(keys) * per);
    for (float& v : values) v = rng.range(-100, 100);
    if (rot) {  // keep every key away from zero length
      for (int k = 0; k < keys; ++k) {
        const std::size_t off = (interp == Interp::CubicSpline ? static_cast<std::size_t>(k) * 12 + 4 : static_cast<std::size_t>(k) * 4);
        values[off + 3] = rng.range(1, 100);
      }
    }
    const Track t = make(comps, interp, times, values, rot);
    REQUIRE(validate_track(t) == TrackStatus::Ok);
    ++checked;

    const float end = times.back() + 1.0f;
    for (int i = 0; i <= 400; ++i) {
      const float at = -0.5f + (end + 0.5f) * static_cast<float>(i) / 400.0f;
      const auto v = sample(t, at);
      for (float x : v) REQUIRE(std::isfinite(x));
      if (rot) REQUIRE(len4(v) == Approx(1.0).margin(kTol));
    }
    // Keys are hit exactly (non-rotation) or to tolerance (rotation).
    for (int k = 0; k < keys; ++k) {
      const std::size_t off = interp == Interp::CubicSpline ? (static_cast<std::size_t>(k) * 3 + 1) * comps : static_cast<std::size_t>(k) * comps;
      const auto v = sample(t, times[static_cast<std::size_t>(k)]);
      if (!rot) {
        for (int c = 0; c < comps; ++c) REQUIRE(v[static_cast<std::size_t>(c)] == values[off + static_cast<std::size_t>(c)]);
      } else {
        std::vector<float> key(values.begin() + static_cast<long>(off), values.begin() + static_cast<long>(off) + 4);
        const double l = len4(key);
        for (float& x : key) x = static_cast<float>(x / l);
        REQUIRE(same_rotation(v, key, 1e-6));
      }
    }
    // Linear continuity: the value just left of a key equals the key value up to the slope times one ulp.
    if (interp == Interp::Linear && !rot) {
      for (int k = 1; k < keys; ++k) {
        const float tk = times[static_cast<std::size_t>(k)];
        const float left = std::nextafter(tk, 0.0f);
        const double ulp = static_cast<double>(tk) - static_cast<double>(left);
        const auto a = sample(t, left);
        const auto b = sample(t, tk);
        const double dt = static_cast<double>(tk) - times[static_cast<std::size_t>(k) - 1];
        for (int c = 0; c < comps; ++c) {
          const double slope = std::fabs(static_cast<double>(values[static_cast<std::size_t>(k) * comps + c]) -
                                         values[static_cast<std::size_t>(k - 1) * comps + c]) / dt;
          REQUIRE(std::fabs(static_cast<double>(a[static_cast<std::size_t>(c)]) - b[static_cast<std::size_t>(c)]) <=
                  slope * ulp * 2.0 + 1e-4);
        }
      }
    }
    // Midpoint of a Linear interval is the average.
    if (interp == Interp::Linear && !rot && keys >= 2) {
      const float mid = 0.5f * (times[0] + times[1]);
      const auto v = sample(t, mid);
      for (int c = 0; c < comps; ++c) {
        const double avg = 0.5 * (static_cast<double>(values[static_cast<std::size_t>(c)]) + values[static_cast<std::size_t>(comps + c)]);
        REQUIRE(v[static_cast<std::size_t>(c)] == Approx(avg).margin(1e-3));
      }
    }
  }
  CHECK(checked >= 200);
}

TEST_CASE("sampling is deterministic and independent of addresses", "[anim][random]") {
  Rng rng{12345};
  std::vector<float> times, values;
  for (int k = 0; k < 50; ++k) {
    times.push_back(static_cast<float>(k) * 0.37f);
    for (int c = 0; c < 4; ++c) values.push_back(rng.range(-5, 5));
    values[values.size() - 1] = 1.0f + rng.range(0, 1);
  }
  const Track a = make(4, Interp::Linear, times, values, true);
  const Track b = a;  // a copy lives at different addresses
  for (int i = 0; i < 300; ++i) {
    const float at = static_cast<float>(i) * 0.061f;
    const auto va = sample(a, at);
    const auto vb = sample(b, at);
    CHECK(va == vb);
    CHECK(va == sample(a, at));
  }
}

// ---------------------------------------------------------------------------------------------------------------
namespace {
const char* kGoodClip = R"({
  "name": "intro",
  "tracks": [
    {"target": "layer/hills/position", "components": 2, "interpolation": "linear",
     "times": [0, 1, 2], "values": [0, 0, 10, 5, 20, 0]},
    {"target": "layer/hills/rotation", "components": 4, "rotation": true, "interpolation": "step",
     "times": [0, 1], "values": [0, 0, 0, 1, 0, 0, 0.7071067811865476, 0.7071067811865476], "future": [1, 2, {"x": null}]},
    {"target": "opacity", "components": 1, "times": [0, 2], "values": [0, 1]}
  ],
  "extra": "ignored"
})";
}

TEST_CASE("parse_clip_json: a good clip", "[anim][json]") {
  const ParseClipResult r = parse_clip_json(kGoodClip);
  REQUIRE(r.ok);
  CHECK(r.message == "ok");
  CHECK(r.clip.name == "intro");
  REQUIRE(r.clip.tracks.size() == 3);
  CHECK(r.clip.tracks[0].target == "layer/hills/position");
  CHECK(r.clip.tracks[0].components == 2);
  CHECK(r.clip.tracks[0].interp == Interp::Linear);
  CHECK(r.clip.tracks[1].is_rotation);
  CHECK(r.clip.tracks[1].interp == Interp::Step);
  CHECK(r.clip.tracks[2].interp == Interp::Linear);  // default
  CHECK_FALSE(r.clip.tracks[2].is_rotation);          // default
  CHECK(r.clip.tracks[0].values[2] == 10.0f);
  CHECK(clip_duration(r.clip) == 2.0f);
  for (const Track& t : r.clip.tracks) CHECK(validate_track(t) == TrackStatus::Ok);
  // Minimal clip: no name, no tracks.
  const ParseClipResult e = parse_clip_json(R"({"tracks": []})");
  CHECK(e.ok);
  CHECK(e.clip.name.empty());
  CHECK(e.clip.tracks.empty());
  // CubicSpline values.
  const ParseClipResult c = parse_clip_json(
      R"({"tracks":[{"target":"x","components":1,"interpolation":"cubicspline","times":[0,1],"values":[0,0,1,2,1,0]}]})");
  REQUIRE(c.ok);
  CHECK(c.clip.tracks[0].interp == Interp::CubicSpline);
}

TEST_CASE("parse_clip_json: errors are short, fixed, carry a path and leave the clip empty", "[anim][json]") {
  struct Case { const char* text; const char* expect; };
  const Case cases[] = {
      {"", "$: invalid JSON, too deep or duplicate key"},
      {"not json", "$: invalid JSON, too deep or duplicate key"},
      {"[]", "$: must be an object"},
      {"null", "$: must be an object"},
      {"42", "$: must be an object"},
      {"{}", "tracks: must be an array"},
      {R"({"tracks": {}})", "tracks: must be an array"},
      {R"({"tracks": null})", "tracks: must be an array"},
      {R"({"name": 5, "tracks": []})", "name: must be a string"},
      {R"({"name": "a\u0001b", "tracks": []})", "name: at most 128 bytes of valid UTF-8 without control characters"},
      {R"({"tracks": [1]})", "tracks[0]: must be an object"},
      {R"({"tracks": [{"components":1,"times":[0],"values":[0]}]})", "tracks[0].target: must be a string"},
      {R"({"tracks": [{"target":5,"components":1,"times":[0],"values":[0]}]})", "tracks[0].target: must be a string"},
      {R"({"tracks": [{"target":"a","times":[0],"values":[0]}]})", "tracks[0].components: must be a whole number from 1 to 4"},
      {R"({"tracks": [{"target":"a","components":"1","times":[0],"values":[0]}]})", "tracks[0].components: must be a whole number from 1 to 4"},
      {R"({"tracks": [{"target":"a","components":1.5,"times":[0],"values":[0]}]})", "tracks[0].components: must be a whole number from 1 to 4"},
      {R"({"tracks": [{"target":"a","components":5,"times":[0],"values":[0]}]})", "tracks[0].components: must be a whole number from 1 to 4"},
      {R"({"tracks": [{"target":"a","components":0,"times":[0],"values":[0]}]})", "tracks[0].components: must be a whole number from 1 to 4"},
      {R"({"tracks": [{"target":"a","components":-1,"times":[0],"values":[0]}]})", "tracks[0].components: must be a whole number from 1 to 4"},
      {R"({"tracks": [{"target":"a","components":1,"rotation":1,"times":[0],"values":[0]}]})", "tracks[0].rotation: must be true or false"},
      {R"({"tracks": [{"target":"a","components":1,"interpolation":"cubic","times":[0],"values":[0]}]})", "tracks[0].interpolation: must be step, linear or cubicspline"},
      {R"({"tracks": [{"target":"a","components":1,"interpolation":7,"times":[0],"values":[0]}]})", "tracks[0].interpolation: must be step, linear or cubicspline"},
      {R"({"tracks": [{"target":"a","components":1,"values":[0]}]})", "tracks[0].times: missing"},
      {R"({"tracks": [{"target":"a","components":1,"times":[0]}]})", "tracks[0].values: missing"},
      {R"({"tracks": [{"target":"a","components":1,"times":"0","values":[0]}]})", "tracks[0].times: must be an array of numbers"},
      {R"({"tracks": [{"target":"a","components":1,"times":[0,"1"],"values":[0,1]}]})", "tracks[0].times: must contain only numbers"},
      {R"({"tracks": [{"target":"a","components":1,"times":[0],"values":[null]}]})", "tracks[0].values: must contain only numbers"},
      {R"({"tracks": [{"target":"a","components":1,"times":[0],"values":[1e39]}]})", "tracks[0].values: number outside the float range"},
      {R"({"tracks": [{"target":"a","components":1,"times":[],"values":[]}]})", "tracks[0]: track has no keys"},
      {R"({"tracks": [{"target":"a","components":1,"times":[1,1],"values":[0,0]}]})", "tracks[0]: times must be finite, not negative and strictly increasing"},
      {R"({"tracks": [{"target":"a","components":1,"times":[0],"values":[0,1]}]})", "tracks[0]: wrong number of values"},
      {R"({"tracks": [{"target":"a","components":3,"rotation":true,"times":[0],"values":[0,0,1]}]})", "tracks[0]: bad component count"},
      {R"({"tracks": [{"target":"a","components":4,"rotation":true,"times":[0],"values":[0,0,0,0]}]})", "tracks[0]: a rotation key has length zero"},
      {R"({"tracks": [{"target":"","components":1,"times":[0],"values":[0]}]})", "tracks[0]: bad target name"},
      {R"({"tracks": [{"target":"ok","components":1,"times":[0],"values":[0]},{"target":"a\nb","components":1,"times":[0],"values":[0]}]})", "tracks[1]: bad target name"},
      {R"({"tracks": [], "tracks": []})", "$: invalid JSON, too deep or duplicate key"},
  };
  for (const Case& c : cases) {
    INFO(c.text);
    const ParseClipResult r = parse_clip_json(c.text);
    CHECK_FALSE(r.ok);
    CHECK(r.message == c.expect);
    CHECK(r.clip.name.empty());
    CHECK(r.clip.tracks.empty());
  }
}

TEST_CASE("parse_clip_json: a failure after good tracks leaves the clip empty", "[anim][json]") {
  const ParseClipResult r = parse_clip_json(
      R"({"name": "x", "tracks": [{"target":"a","components":1,"times":[0],"values":[0]}, {"target":"b"}]})");
  CHECK_FALSE(r.ok);
  CHECK(r.clip.name.empty());
  CHECK(r.clip.tracks.empty());
}

TEST_CASE("parse_clip_json: messages never contain input bytes", "[anim][json]") {
  const std::string marker = "SECRET_MARKER_123";
  const std::string inputs[] = {
      "{\"tracks\": [{\"target\": \"" + marker + "\\u0001\", \"components\": 1, \"times\": [0], \"values\": [0]}]}",
      "{\"" + marker + "\": 1, \"tracks\": 5}",
      "{\"tracks\": [{\"target\": \"a\", \"components\": \"" + marker + "\"}]}",
      "{\"tracks\": [{\"target\": \"a\", \"components\": 1, \"interpolation\": \"" + marker + "\"}]}",
      "{\"name\": \"" + marker + std::string(200, 'x') + "\", \"tracks\": []}",
      marker,
      "{\"" + marker + "\": 1, \"" + marker + "\": 2}",
  };
  for (const std::string& in : inputs) {
    const ParseClipResult r = parse_clip_json(in);
    CHECK_FALSE(r.ok);
    CHECK(r.message.find("SECRET") == std::string::npos);
    CHECK(r.message.size() < 100);
  }
}

TEST_CASE("parse_clip_json: hostile JSON", "[anim][json][hostile]") {
  // Deep nesting, far past the depth limit.
  CHECK_FALSE(parse_clip_json(std::string(100000, '[')).ok);
  CHECK_FALSE(parse_clip_json(std::string(5000, '[') + std::string(5000, ']')).ok);
  std::string deep_obj;
  for (int i = 0; i < 3000; ++i) deep_obj += "{\"a\":";
  deep_obj += "1";
  deep_obj += std::string(3000, '}');
  CHECK_FALSE(parse_clip_json(deep_obj).ok);
  // Nesting inside an ignored unknown field is still bounded by the depth limit.
  std::string nested = R"({"tracks": [], "junk": )" + std::string(200, '[') + std::string(200, ']') + "}";
  CHECK_FALSE(parse_clip_json(nested).ok);
  // Unknown fields with modest nesting are fine.
  CHECK(parse_clip_json(R"({"tracks": [], "junk": [[1], {"a": [2]}]})").ok);
  // Wrong types everywhere.
  const char* wrong[] = {
      R"({"tracks": [[]]})",
      R"({"tracks": [{"target": [], "components": {}, "times": 1, "values": 2}]})",
      R"({"tracks": [{"target": "a", "components": true, "times": [], "values": []}]})",
      R"({"tracks": "x", "name": []})",
      R"({"tracks": [{"target": "a", "components": 1, "times": [[0]], "values": [[0]]}]})",
      R"({"tracks": [{"target": "a", "components": 1, "times": [{}], "values": [0]}]})",
      "{\"tracks\": [{\"target\": \"a\", \"components\": 1, \"times\": [0], \"values\": [0]}]",  // truncated
      "{\"tracks\": [] } trailing",
      "\xEF\xBB\xBF{\"tracks\": []}",
      "{\"tracks\": [{\"target\": \"a\xC3\", \"components\": 1, \"times\": [0], \"values\": [0]}]}",
      R"({"tracks": [{"target":"a","components":1,"times":[1e999],"values":[0]}]})",
      R"({"tracks": [{"target":"a","components":1,"times":[0],"values":[-1e999]}]})",
      R"({"tracks": [{"target":"a","components":1e308,"times":[0],"values":[0]}]})",
      R"({"tracks": [{"target":"a","components":4294967297,"times":[0],"values":[0]}]})",
      R"({"tracks": [{"target":"a","components":1,"times":[0, -1],"values":[0, 0]}]})",
  };
  for (const char* w : wrong) {
    INFO(w);
    const ParseClipResult r = parse_clip_json(w);
    CHECK_FALSE(r.ok);
    CHECK(r.clip.tracks.empty());
    CHECK_FALSE(r.message.empty());
  }
  // Counts: too many tracks.
  std::string many = R"({"tracks": [)";
  for (std::size_t i = 0; i <= kMaxTracksPerClip; ++i) many += (i ? ",{}" : "{}");
  many += "]}";
  const ParseClipResult mt = parse_clip_json(many);
  CHECK_FALSE(mt.ok);
  CHECK(mt.message == "tracks: too many tracks");
  // Too many numbers in total across tracks (each track is small, the sum is not).
  std::string big = R"({"tracks": [)";
  std::string arr = "[";
  for (int i = 0; i < 60000; ++i) arr += (i ? ",0" : "0");
  arr += "]";
  // times would not be increasing, but the budget check comes first on the 9th track.
  for (int t = 0; t < 12; ++t) {
    big += std::string(t ? "," : "") + R"({"target":"a","components":1,"times":)" + arr + R"(,"values":)" + arr + "}";
  }
  big += "]}";
  const ParseClipResult bt = parse_clip_json(big);
  CHECK_FALSE(bt.ok);
  CHECK(bt.clip.tracks.empty());
  // Oversized text.
  const ParseClipResult huge = parse_clip_json(std::string(5u << 20, ' '));
  CHECK_FALSE(huge.ok);
  CHECK(huge.message == "$: text too large");
  // A long key and a long string do not matter.
  const ParseClipResult longname = parse_clip_json("{\"name\": \"" + std::string(100000, 'a') + "\", \"tracks\": []}");
  CHECK_FALSE(longname.ok);
}

TEST_CASE("parse_clip_json: a large valid clip", "[anim][json]") {
  std::string text = R"({"name":"big","tracks":[{"target":"a","components":4,"times":[)";
  const int keys = 20000;
  for (int i = 0; i < keys; ++i) text += (i ? "," : "") + std::to_string(i);
  text += R"(],"values":[)";
  for (int i = 0; i < keys * 4; ++i) text += (i ? "," : "") + std::to_string(i % 7);
  text += "]}]}";
  const ParseClipResult r = parse_clip_json(text);
  REQUIRE(r.ok);
  CHECK(r.clip.tracks[0].times.size() == static_cast<std::size_t>(keys));
  CHECK(clip_duration(r.clip) == static_cast<float>(keys - 1));
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("a 100000-key track: one validation, then repeated sampling under a time cap", "[anim][perf]") {
  const std::size_t n = 100000;
  std::vector<float> times(n), values(n);
  for (std::size_t i = 0; i < n; ++i) {
    times[i] = static_cast<float>(i) * 0.25f;
    values[i] = static_cast<float>(i) * 0.5f;  // value = 2 * time
  }
  const Track t = make(1, Interp::Linear, times, values);

  const auto start = std::chrono::steady_clock::now();
  REQUIRE(validate_track(t) == TrackStatus::Ok);
  const double one = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("one validation took " << one << " s");

  // sample_track validates the whole track on every call (documented O(n) cost), so only a thousand samples.
  const int samples = 1000;
  Rng rng{777};
  float out = 0;
  for (int i = 0; i < samples; ++i) {
    const float at = rng.range(0, static_cast<double>(n - 1) * 0.25);
    REQUIRE(sample_track(t, at, &out));
    REQUIRE(out == Approx(2.0 * at).epsilon(1e-5).margin(1e-3));
  }
  const double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("total " << total << " s for " << samples << " samples");
  CHECK(total < 30.0);
}
