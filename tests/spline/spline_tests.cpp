// Tests for core/spline. Pure CPU. Golden digests are also written in docs/spline.md and must match on Linux and Windows.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "spline.h"

using Catch::Approx;
using namespace lumora::spline;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr double kPi = 3.14159265358979323846;

Path make(Kind k, bool closed, const std::vector<Vec2>& pts) {
  Path p;
  p.kind = k;
  p.closed = closed;
  p.points = pts.data();
  p.count = pts.size();
  return p;
}

std::vector<Vec2> pts_n(std::size_t n) {
  std::vector<Vec2> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = {static_cast<float>(i), static_cast<float>(i % 7)};
  return v;
}

std::unique_ptr<LengthTable> table_of(const Path& p) {
  std::unique_ptr<LengthTable> t(new LengthTable());
  REQUIRE(build_length_table(p, *t));
  return t;
}

// Fixed point sets for the golden digests.
const std::vector<Vec2> kPoly = {{0, 0}, {3, 4}, {6, -2}, {9, 1}, {12, 5}, {15, 0}, {18, 3}};
const std::vector<Vec2> kBez = {{0, 0}, {1, 2}, {3, 3}, {4, 0}, {5, -3}, {7, -3}, {8, 1}, {9, 4}, {11, 5}, {12, 2}};

std::uint64_t golden_digest(Kind k, bool closed) {
  const std::vector<Vec2>& src = k == Kind::CubicBezier ? kBez : kPoly;
  std::vector<Vec2> pts = src;
  if (k == Kind::CubicBezier && closed) pts.pop_back();  // 9 points = 3 segments
  const Path p = make(k, closed, pts);
  std::vector<Vec2> out(100);
  REQUIRE(resample_even(p, 100, out.data()));
  return digest_points(out.data(), out.size());
}

constexpr std::uint64_t kGoldenLinear = 0x197a7ea2afb2ed2aull;
constexpr std::uint64_t kGoldenCrOpen = 0x6cdcbdbcf7df6bbeull;
constexpr std::uint64_t kGoldenCrClosed = 0x65020a83861edd11ull;
constexpr std::uint64_t kGoldenBezOpen = 0x3b8b259c3d2a9778ull;
constexpr std::uint64_t kGoldenBezClosed = 0x260d2de6ee281cb7ull;

}  // namespace

TEST_CASE("validate_path: every status and count rule", "[spline][validate]") {
  std::vector<Vec2> v = pts_n(4);
  Path p = make(Kind::Linear, false, v);
  CHECK(validate_path(p) == PathStatus::Ok);
  CHECK(std::string(path_status_text(PathStatus::Ok)) == "ok");

  p.kind = static_cast<Kind>(3);
  CHECK(validate_path(p) == PathStatus::BadKind);
  p.kind = static_cast<Kind>(200);
  CHECK(validate_path(p) == PathStatus::BadKind);
  CHECK(segment_count(p) == 0);

  // Too few points.
  for (Kind k : {Kind::Linear, Kind::CatmullRom}) {
    for (bool closed : {false, true}) {
      std::vector<Vec2> one = pts_n(1);
      CHECK(validate_path(make(k, closed, one)) == PathStatus::TooFewPoints);
      CHECK(validate_path(make(k, closed, std::vector<Vec2>())) == PathStatus::TooFewPoints);
      std::vector<Vec2> two = pts_n(2);
      CHECK(validate_path(make(k, closed, two)) == PathStatus::Ok);
    }
  }
  for (std::size_t n : {0u, 1u, 2u, 3u}) CHECK(validate_path(make(Kind::CubicBezier, false, pts_n(n))) == PathStatus::TooFewPoints);
  for (std::size_t n : {0u, 1u, 2u}) CHECK(validate_path(make(Kind::CubicBezier, true, pts_n(n))) == PathStatus::TooFewPoints);
  // Bezier counts.
  for (std::size_t n = 4; n <= 40; ++n) {
    const bool ok_open = (n - 1) % 3 == 0;
    CHECK(validate_path(make(Kind::CubicBezier, false, pts_n(n))) == (ok_open ? PathStatus::Ok : PathStatus::BadPointCount));
  }
  for (std::size_t n = 3; n <= 40; ++n) {
    const bool ok_closed = n % 3 == 0;
    CHECK(validate_path(make(Kind::CubicBezier, true, pts_n(n))) == (ok_closed ? PathStatus::Ok : PathStatus::BadPointCount));
  }
  // Too many points; limit values.
  CHECK(validate_path(make(Kind::Linear, false, pts_n(kMaxPoints))) == PathStatus::Ok);
  CHECK(validate_path(make(Kind::Linear, false, pts_n(kMaxPoints + 1))) == PathStatus::TooManyPoints);
  CHECK(validate_path(make(Kind::CubicBezier, false, pts_n(1024))) == PathStatus::Ok);  // 1 + 3 * 341
  CHECK(validate_path(make(Kind::CubicBezier, false, pts_n(1023))) == PathStatus::BadPointCount);
  CHECK(validate_path(make(Kind::CubicBezier, true, pts_n(1023))) == PathStatus::Ok);
  CHECK(validate_path(make(Kind::CubicBezier, false, pts_n(1026))) == PathStatus::TooManyPoints);  // size beats count shape
  Path huge = make(Kind::Linear, false, pts_n(4));
  huge.count = 1u << 30;
  CHECK(validate_path(huge) == PathStatus::TooManyPoints);

  // Bad points.
  for (float bad : {kNaN, kInf, -kInf, 1000000.5f, -1000001.0f, 3e38f}) {
    for (int comp = 0; comp < 2; ++comp) {
      v = pts_n(4);
      (comp == 0 ? v[2].x : v[2].y) = bad;
      CHECK(validate_path(make(Kind::Linear, false, v)) == PathStatus::BadPoint);
    }
  }
  v = pts_n(4);
  v[0] = {1e6f, -1e6f};
  CHECK(validate_path(make(Kind::Linear, false, v)) == PathStatus::Ok);  // limit is inclusive
  // Null points with a plausible count.
  Path np;
  np.count = 4;
  CHECK(validate_path(np) == PathStatus::BadPoint);
  // Order: kind, count, too many, shape, point.
  v = pts_n(5);
  v[1].x = kNaN;
  CHECK(validate_path(make(Kind::CubicBezier, false, v)) == PathStatus::BadPointCount);
  CHECK(validate_path(make(Kind::Linear, false, v)) == PathStatus::BadPoint);
  Path bk = make(Kind::Linear, false, v);
  bk.kind = static_cast<Kind>(9);
  CHECK(validate_path(bk) == PathStatus::BadKind);

  std::vector<std::string> texts;
  for (int s = 0; s <= 5; ++s) {
    const std::string t = path_status_text(static_cast<PathStatus>(s));
    CHECK_FALSE(t.empty());
    for (const std::string& o : texts) CHECK(o != t);
    texts.push_back(t);
  }
  CHECK(std::string(path_status_text(static_cast<PathStatus>(99))) == "unknown status");
}

TEST_CASE("segment counts: open versus closed", "[spline][segments]") {
  CHECK(segment_count(make(Kind::Linear, false, pts_n(5))) == 4);
  CHECK(segment_count(make(Kind::Linear, true, pts_n(5))) == 5);
  CHECK(segment_count(make(Kind::Linear, true, pts_n(2))) == 2);
  CHECK(segment_count(make(Kind::CatmullRom, false, pts_n(2))) == 1);
  CHECK(segment_count(make(Kind::CatmullRom, true, pts_n(6))) == 6);
  CHECK(segment_count(make(Kind::CubicBezier, false, pts_n(7))) == 2);
  CHECK(segment_count(make(Kind::CubicBezier, true, pts_n(6))) == 2);
  CHECK(segment_count(make(Kind::CubicBezier, true, pts_n(3))) == 1);
  CHECK(segment_count(make(Kind::CubicBezier, false, pts_n(5))) == 0);  // invalid
  CHECK(segment_count(Path{}) == 0);
}

TEST_CASE("linear evaluation and tangents", "[spline][linear]") {
  const std::vector<Vec2> v = {{0, 0}, {3, 4}, {3, 10}};
  const Path p = make(Kind::Linear, false, v);
  Vec2 a = eval_segment(p, 0, 0.5f);
  CHECK(a.x == 1.5f); CHECK(a.y == 2.0f);
  a = eval_segment(p, 1, 1.0f);
  CHECK(a.x == 3.0f); CHECK(a.y == 10.0f);
  a = tangent_segment(p, 0, 0.3f);
  CHECK(a.x == 3.0f); CHECK(a.y == 4.0f);
  a = tangent_segment(p, 1, 0.9f);
  CHECK(a.x == 0.0f); CHECK(a.y == 6.0f);
  // Closed: the last segment returns to the first point.
  const Path c = make(Kind::Linear, true, v);
  a = eval_segment(c, 2, 1.0f);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = eval_segment(c, 2, 0.5f);
  CHECK(a.x == 1.5f); CHECK(a.y == 5.0f);
  a = tangent_segment(c, 2, 0.0f);
  CHECK(a.x == -3.0f); CHECK(a.y == -10.0f);
}

TEST_CASE("evaluation edge rules: clamp, NaN and invalid input", "[spline][edge]") {
  const std::vector<Vec2> v = {{0, 0}, {10, 0}};
  const Path p = make(Kind::Linear, false, v);
  CHECK(eval_segment(p, 0, -3.0f).x == 0.0f);
  CHECK(eval_segment(p, 0, 7.0f).x == 10.0f);
  CHECK(eval_segment(p, 0, kNaN).x == 0.0f);
  CHECK(eval_segment(p, 0, kInf).x == 10.0f);
  CHECK(eval_segment(p, 0, -kInf).x == 0.0f);
  CHECK(tangent_segment(p, 0, kNaN).x == 10.0f);
  // Segment out of range or invalid path: {0, 0}.
  Vec2 z = eval_segment(p, 1, 0.5f);
  CHECK(z.x == 0.0f); CHECK(z.y == 0.0f);
  z = eval_segment(p, static_cast<std::size_t>(-1), 0.5f);
  CHECK(z.x == 0.0f); CHECK(z.y == 0.0f);
  z = tangent_segment(p, 5, 0.5f);
  CHECK(z.x == 0.0f); CHECK(z.y == 0.0f);
  const std::vector<Vec2> bad = {{0, 0}, {kNaN, 1}};
  z = eval_segment(make(Kind::Linear, false, bad), 0, 0.5f);
  CHECK(z.x == 0.0f); CHECK(z.y == 0.0f);
  z = tangent_segment(Path{}, 0, 0.5f);
  CHECK(z.x == 0.0f); CHECK(z.y == 0.0f);
}

TEST_CASE("Catmull-Rom: end points, tangents, open versus closed", "[spline][catmull]") {
  const std::vector<Vec2> line = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
  const Path o = make(Kind::CatmullRom, false, line);
  CHECK(segment_count(o) == 3);
  // Passes through every point.
  for (std::size_t s = 0; s < 3; ++s) {
    CHECK(eval_segment(o, s, 0.0f).x == static_cast<float>(s));
    CHECK(eval_segment(o, s, 1.0f).x == static_cast<float>(s + 1));
  }
  // Open ends duplicate the end points: tangent at the first point is half the first chord, and at the last point
  // half the last chord; in the middle it is half the distance between neighbours.
  CHECK(tangent_segment(o, 0, 0.0f).x == 0.5f);
  CHECK(tangent_segment(o, 0, 1.0f).x == 1.0f);
  CHECK(tangent_segment(o, 1, 0.0f).x == 1.0f);
  CHECK(tangent_segment(o, 2, 1.0f).x == 0.5f);
  CHECK(tangent_segment(o, 0, 0.0f).y == 0.0f);
  // Midpoint of a straight uniform run.
  CHECK(eval_segment(o, 1, 0.5f).x == 1.5f);

  // Closed unit square: wrapping tangents.
  const std::vector<Vec2> sq = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  const Path c = make(Kind::CatmullRom, true, sq);
  CHECK(segment_count(c) == 4);
  Vec2 t = tangent_segment(c, 0, 0.0f);  // 0.5 * (P1 - P3)
  CHECK(t.x == 0.5f); CHECK(t.y == -0.5f);
  t = tangent_segment(c, 3, 1.0f);  // end of the last segment equals start of the first: same tangent
  CHECK(t.x == 0.5f); CHECK(t.y == -0.5f);
  Vec2 e = eval_segment(c, 3, 1.0f);
  CHECK(e.x == 0.0f); CHECK(e.y == 0.0f);
  e = eval_segment(c, 0, 0.0f);
  CHECK(e.x == 0.0f); CHECK(e.y == 0.0f);
  // Segment 0 midpoint: 0.5 * (2 P1 ... ) hand computed with p0 = (0,1), p1 = (0,0), p2 = (1,0), p3 = (1,1) at u = 0.5.
  // x: 0.5 * (0 + 1 * 0.5 + (0 - 0 + 4 - 1) * 0.25 + (0 - 0 - 3 + 1) * 0.125) = 0.5 * (0.5 + 0.75 - 0.25) = 0.5
  // y: 0.5 * (0 + (0 - 1) * 0.5 + (2 - 0 + 0 - 1) * 0.25 + (0 - 1 - 0 + 1) * 0.125) = 0.5 * (-0.5 + 0.25) = -0.125
  e = eval_segment(c, 0, 0.5f);
  CHECK(e.x == 0.5f); CHECK(e.y == -0.125f);
  // The same points as an open path differ at the first tangent.
  const Path op = make(Kind::CatmullRom, false, sq);
  CHECK(segment_count(op) == 3);
  t = tangent_segment(op, 0, 0.0f);  // 0.5 * (P1 - P0) with the first point duplicated
  CHECK(t.x == 0.5f); CHECK(t.y == 0.0f);
  // Tangent matches a numeric derivative of the position.
  for (int s = 0; s < 4; ++s) {
    for (float u : {0.1f, 0.5f, 0.9f}) {
      const double h = 1e-3;
      const Vec2 a = eval_segment(c, static_cast<std::size_t>(s), static_cast<float>(u - h));
      const Vec2 b = eval_segment(c, static_cast<std::size_t>(s), static_cast<float>(u + h));
      const Vec2 d = tangent_segment(c, static_cast<std::size_t>(s), u);
      CHECK(d.x == Approx((b.x - a.x) / (2 * h)).margin(2e-3));
      CHECK(d.y == Approx((b.y - a.y) / (2 * h)).margin(2e-3));
    }
  }
}

TEST_CASE("cubic Bezier: end points, midpoint, tangents, wrapping", "[spline][bezier]") {
  const std::vector<Vec2> v = {{0, 0}, {1, 2}, {3, 2}, {4, 0}, {5, -2}, {6, -2}, {7, 0}};
  const Path p = make(Kind::CubicBezier, false, v);
  CHECK(segment_count(p) == 2);
  Vec2 a = eval_segment(p, 0, 0.0f);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = eval_segment(p, 0, 1.0f);
  CHECK(a.x == 4.0f); CHECK(a.y == 0.0f);
  a = eval_segment(p, 1, 0.0f);
  CHECK(a.x == 4.0f);
  a = eval_segment(p, 1, 1.0f);
  CHECK(a.x == 7.0f); CHECK(a.y == 0.0f);
  a = eval_segment(p, 0, 0.5f);  // (0 + 3*1 + 3*3 + 4) / 8 = 2, (0 + 6 + 6 + 0) / 8 = 1.5
  CHECK(a.x == 2.0f); CHECK(a.y == 1.5f);
  a = tangent_segment(p, 0, 0.0f);  // 3 (c0 - p0)
  CHECK(a.x == 3.0f); CHECK(a.y == 6.0f);
  a = tangent_segment(p, 0, 1.0f);  // 3 (p1 - c1)
  CHECK(a.x == 3.0f); CHECK(a.y == -6.0f);
  a = tangent_segment(p, 0, 0.5f);  // 0.75 (c0-p0) + 1.5 (c1-c0) + 0.75 (p1-c1) = (0.75+3+0.75, 1.5+0+(-1.5))
  CHECK(a.x == 4.5f); CHECK(a.y == 0.0f);
  // Closed: three points per segment, the last end point wraps to the first.
  const std::vector<Vec2> cv = {{0, 0}, {1, 1}, {1, -1}};
  const Path c = make(Kind::CubicBezier, true, cv);
  CHECK(segment_count(c) == 1);
  a = eval_segment(c, 0, 1.0f);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = tangent_segment(c, 0, 1.0f);  // 3 (p1 - c1) = 3 * ((0,0) - (1,-1))
  CHECK(a.x == -3.0f); CHECK(a.y == 3.0f);
  const std::vector<Vec2> two = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}};
  const Path c2 = make(Kind::CubicBezier, true, two);
  a = eval_segment(c2, 1, 1.0f);
  CHECK(a.x == 0.0f);  // second segment ends at point 0
  a = eval_segment(c2, 1, 0.0f);
  CHECK(a.x == 3.0f);
}

TEST_CASE("arc length: straight lines, quarter arc and monotonic tables", "[spline][length]") {
  // Straight lines are exact up to double rounding for every kind.
  const std::vector<Vec2> line = {{0, 0}, {3, 4}, {6, 8}};
  CHECK(table_of(make(Kind::Linear, false, line))->total == Approx(10.0).epsilon(1e-12));
  const std::vector<Vec2> bline = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
  CHECK(table_of(make(Kind::CubicBezier, false, bline))->total == Approx(3.0).epsilon(1e-12));
  const std::vector<Vec2> cline = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
  // Uniform Catmull-Rom on equally spaced collinear points is not uniform in speed at the open ends, but the length is
  // still exactly the distance covered.
  CHECK(table_of(make(Kind::CatmullRom, false, cline))->total == Approx(3.0).epsilon(1e-12));
  // Closed there-and-back polyline.
  const std::vector<Vec2> back = {{0, 0}, {5, 0}};
  CHECK(table_of(make(Kind::Linear, true, back))->total == Approx(10.0).epsilon(1e-12));

  // Unit quarter circle by the usual Bezier constant. True length is pi / 2. The 32 chord polyline lies inside the
  // curve (about -1e-4) and the Bezier itself lies outside the circle (about +2.7e-4 in radius), so the table is within
  // 1e-4 of pi / 2 (measured +6.2e-5).
  const float k = 0.5522847498f;
  const std::vector<Vec2> q = {{1, 0}, {1, k}, {k, 1}, {0, 1}};
  const Path qp = make(Kind::CubicBezier, false, q);
  const auto qt = table_of(qp);
  INFO("quarter arc length " << qt->total);
  CHECK(std::fabs(qt->total - kPi / 2.0) < 1e-4);
  CHECK(qt->chords == 32);
  CHECK(qt->cumulative[0] == 0.0);
  CHECK(qt->cumulative[32] == qt->total);

  // Cumulative lengths never decrease, for every kind and for closed paths.
  std::vector<Vec2> rnd(30);
  std::uint32_t s = 12345;
  for (auto& v : rnd) {
    s = s * 1664525u + 1013904223u;
    v.x = static_cast<float>(s >> 16) / 65536.0f * 100.0f - 50.0f;
    s = s * 1664525u + 1013904223u;
    v.y = static_cast<float>(s >> 16) / 65536.0f * 100.0f - 50.0f;
  }
  for (Kind kind : {Kind::Linear, Kind::CatmullRom, Kind::CubicBezier}) {
    for (bool closed : {false, true}) {
      std::vector<Vec2> pts(rnd.begin(), rnd.begin() + (kind == Kind::CubicBezier ? (closed ? 30 : 28) : 30));
      const Path p = make(kind, closed, pts);
      REQUIRE(validate_path(p) == PathStatus::Ok);
      const auto t = table_of(p);
      CHECK(t->chords == segment_count(p) * 32);
      for (std::size_t i = 1; i <= t->chords; ++i) REQUIRE(t->cumulative[i] >= t->cumulative[i - 1]);
      CHECK(t->total == t->cumulative[t->chords]);
      CHECK(t->total > 0.0);
    }
  }
  // Invalid paths leave the table untouched.
  std::unique_ptr<LengthTable> keep(new LengthTable());
  keep->total = 42.0;
  keep->chords = 7;
  CHECK_FALSE(build_length_table(Path{}, *keep));
  CHECK(keep->total == 42.0);
  CHECK(keep->chords == 7);
  // A zero-length path (all points equal) has total 0.
  const std::vector<Vec2> same = {{2, 2}, {2, 2}, {2, 2}};
  CHECK(table_of(make(Kind::Linear, false, same))->total == 0.0);
}

TEST_CASE("eval_at_length", "[spline][length]") {
  const std::vector<Vec2> v = {{0, 0}, {10, 0}, {10, 10}};
  const Path p = make(Kind::Linear, false, v);
  const auto t = table_of(p);
  CHECK(t->total == Approx(20.0).epsilon(1e-12));
  Vec2 a = eval_at_length(p, *t, 0.0);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = eval_at_length(p, *t, 5.0);
  CHECK(a.x == Approx(5.0).epsilon(1e-6)); CHECK(a.y == 0.0f);
  a = eval_at_length(p, *t, 15.0);
  CHECK(a.x == Approx(10.0).epsilon(1e-6)); CHECK(a.y == Approx(5.0).epsilon(1e-6));
  a = eval_at_length(p, *t, t->total);  // exact end point
  CHECK(a.x == 10.0f); CHECK(a.y == 10.0f);
  // Clamp and NaN.
  a = eval_at_length(p, *t, -4.0);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = eval_at_length(p, *t, 1e30);
  CHECK(a.x == 10.0f); CHECK(a.y == 10.0f);
  a = eval_at_length(p, *t, std::numeric_limits<double>::quiet_NaN());
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = eval_at_length(p, *t, std::numeric_limits<double>::infinity());
  CHECK(a.x == 10.0f); CHECK(a.y == 10.0f);
  a = eval_at_length(p, *t, -std::numeric_limits<double>::infinity());
  CHECK(a.x == 0.0f);
  // The corner at exactly half the length belongs to the second segment start.
  a = eval_at_length(p, *t, t->cumulative[32]);
  CHECK(a.x == 10.0f); CHECK(a.y == 0.0f);
  // Table that does not match the path, or an invalid path: {0, 0}.
  const std::vector<Vec2> w = {{0, 0}, {1, 1}};
  a = eval_at_length(make(Kind::Linear, false, w), *t, 3.0);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  a = eval_at_length(Path{}, *t, 3.0);
  CHECK(a.x == 0.0f); CHECK(a.y == 0.0f);
  // Zero-length path: always the point.
  const std::vector<Vec2> same = {{2, 3}, {2, 3}};
  const Path sp = make(Kind::Linear, false, same);
  a = eval_at_length(sp, *table_of(sp), 1.0);
  CHECK(a.x == 2.0f); CHECK(a.y == 3.0f);
  // Zero-length chords in the middle are skipped (duplicate vertex).
  const std::vector<Vec2> dup = {{0, 0}, {4, 0}, {4, 0}, {4, 4}};
  const Path dp = make(Kind::Linear, false, dup);
  const auto dt = table_of(dp);
  a = eval_at_length(dp, *dt, 4.0);
  CHECK(a.x == Approx(4.0).epsilon(1e-6)); CHECK(a.y == Approx(0.0).margin(1e-6));
  a = eval_at_length(dp, *dt, 6.0);
  CHECK(a.x == Approx(4.0).epsilon(1e-6)); CHECK(a.y == Approx(2.0).epsilon(1e-6));
}

TEST_CASE("resample_even: spacing, endpoints and failure cases", "[spline][resample]") {
  const std::vector<Vec2> v = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
  const Path p = make(Kind::Linear, false, v);
  std::vector<Vec2> out(41);
  REQUIRE(resample_even(p, 41, out.data()));
  CHECK(out[0].x == 0.0f); CHECK(out[0].y == 0.0f);
  CHECK(out[40].x == 0.0f); CHECK(out[40].y == 10.0f);  // exact end point
  // 30 units over 40 gaps: the arc spacing is 0.75. Euclidean gaps never exceed it, and are shorter only across a
  // corner (at most a factor of cos 45 degrees there).
  int corner_gaps = 0;
  for (int i = 1; i < 41; ++i) {
    const double d = std::hypot(static_cast<double>(out[i].x) - out[i - 1].x, static_cast<double>(out[i].y) - out[i - 1].y);
    CHECK(d <= 0.75 + 1e-4);
    if (d < 0.75 - 1e-4) {
      ++corner_gaps;
      CHECK(d >= 0.75 * 0.70);
    }
  }
  CHECK(corner_gaps <= 2);

  // Spacing bounds on curves: gaps are within the arc spacing, and the sum of gaps is close to the total length.
  for (Kind kind : {Kind::CatmullRom, Kind::CubicBezier}) {
    std::vector<Vec2> pts = kind == Kind::CubicBezier ? kBez : kPoly;
    const Path cp = make(kind, false, pts);
    const auto tbl = table_of(cp);
    const std::size_t n = 500;
    std::vector<Vec2> r(n);
    REQUIRE(resample_even(cp, n, r.data()));
    const double spacing = tbl->total / static_cast<double>(n - 1);
    double sum = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
      const double d = std::hypot(static_cast<double>(r[i].x) - r[i - 1].x, static_cast<double>(r[i].y) - r[i - 1].y);
      REQUIRE(d <= spacing * (1.0 + 1e-4) + 1e-4);
      sum += d;
    }
    CHECK(sum <= tbl->total + 1e-3);
    CHECK(sum >= tbl->total * 0.999);
  }
  // Closed paths end where they start.
  const Path cl = make(Kind::Linear, true, v);
  std::vector<Vec2> r(9);
  REQUIRE(resample_even(cl, 9, r.data()));
  CHECK(r[0].x == 0.0f); CHECK(r[0].y == 0.0f);
  CHECK(r[8].x == 0.0f); CHECK(r[8].y == 0.0f);
  CHECK(r[2].x == 10.0f); CHECK(r[2].y == 0.0f);  // 10 units along a 40 unit loop with 8 gaps of 5
  CHECK(r[4].x == 10.0f); CHECK(r[4].y == 10.0f);

  // n == 1 gives s = 0; n == 2 gives both ends.
  Vec2 one{9, 9};
  REQUIRE(resample_even(p, 1, &one));
  CHECK(one.x == 0.0f); CHECK(one.y == 0.0f);
  Vec2 two[2];
  REQUIRE(resample_even(p, 2, two));
  CHECK(two[0].x == 0.0f);
  CHECK(two[1].x == 0.0f); CHECK(two[1].y == 10.0f);
  // Failures leave the output untouched.
  Vec2 keep[2] = {{7, 7}, {7, 7}};
  CHECK_FALSE(resample_even(p, 0, keep));
  CHECK_FALSE(resample_even(p, 65537, keep));
  CHECK_FALSE(resample_even(Path{}, 2, keep));
  const std::vector<Vec2> bad = {{0, 0}, {kNaN, 0}};
  CHECK_FALSE(resample_even(make(Kind::Linear, false, bad), 2, keep));
  CHECK(keep[0].x == 7.0f); CHECK(keep[1].y == 7.0f);
  std::vector<Vec2> big(65536);
  CHECK(resample_even(p, 65536, big.data()));
  CHECK(big[65535].y == 10.0f);
}

TEST_CASE("digest_points: FNV-1a 64 over little-endian float bits", "[spline][digest]") {
  CHECK(digest_points(nullptr, 0) == 0xcbf29ce484222325ull);
  const Vec2 z{0.0f, 0.0f};
  CHECK(digest_points(&z, 1) == 0xa8c7f832281a39c5ull);  // eight zero bytes
  const Vec2 a{1.0f, -2.5f};
  CHECK(digest_points(&a, 1) == 0x09e629ee2dfdb3f8ull);
  const Vec2 two[2] = {{1.0f, 2.0f}, {3.0f, 4.0f}};
  const Vec2 swapped[2] = {{3.0f, 4.0f}, {1.0f, 2.0f}};
  const Vec2 flipped{2.0f, 1.0f};
  CHECK(digest_points(two, 2) != digest_points(swapped, 2));  // order matters
  CHECK(digest_points(two, 1) != digest_points(&flipped, 1));  // x then y
  const Vec2 nz{-0.0f, 0.0f};
  CHECK(digest_points(&nz, 1) != digest_points(&z, 1));  // bit patterns, so -0 differs
}

TEST_CASE("golden digests of resampled paths, one per kind", "[spline][digest]") {
  const std::uint64_t lin = golden_digest(Kind::Linear, false), cro = golden_digest(Kind::CatmullRom, false),
                      crc = golden_digest(Kind::CatmullRom, true), bzo = golden_digest(Kind::CubicBezier, false),
                      bzc = golden_digest(Kind::CubicBezier, true);
  INFO(std::hex << "linear 0x" << lin << " cr open 0x" << cro << " cr closed 0x" << crc << " bezier open 0x" << bzo
                << " bezier closed 0x" << bzc);
  CHECK(lin == kGoldenLinear);
  CHECK(cro == kGoldenCrOpen);
  CHECK(crc == kGoldenCrClosed);
  CHECK(bzo == kGoldenBezOpen);
  CHECK(bzc == kGoldenBezClosed);
  CHECK(lin != cro);
  CHECK(cro != crc);
  CHECK(bzo != bzc);
  CHECK(golden_digest(Kind::Linear, false) == lin);  // repeatable
}

TEST_CASE("performance: large paths", "[spline][perf]") {
  const auto start = std::chrono::steady_clock::now();
  std::vector<Vec2> pts(kMaxPoints);
  std::uint32_t s = 99;
  for (auto& v : pts) {
    s = s * 1664525u + 1013904223u;
    v.x = static_cast<float>(s >> 16) / 65536.0f * 1000.0f;
    s = s * 1664525u + 1013904223u;
    v.y = static_cast<float>(s >> 16) / 65536.0f * 1000.0f;
  }
  std::vector<Vec2> out(65536);
  std::uint64_t acc = 0;
  for (Kind k : {Kind::Linear, Kind::CatmullRom, Kind::CubicBezier}) {
    for (bool closed : {false, true}) {
      const std::size_t n = k == Kind::CubicBezier ? (closed ? 1023 : 1024) : kMaxPoints;
      Path p;
      p.kind = k;
      p.closed = closed;
      p.points = pts.data();
      p.count = n;
      REQUIRE(validate_path(p) == PathStatus::Ok);
      REQUIRE(resample_even(p, 32768, out.data()));
      acc ^= digest_points(out.data(), 32768);
    }
  }
  CHECK(acc != 0);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("took " << secs << " s");
  CHECK(secs < 10.0);
}
