// Tests for core/geom. Pure CPU. Golden digests are also written in docs/geom.md and must match on Linux and Windows.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "geom.h"

using namespace lumora::geom;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

float from_bits(std::uint32_t b) {
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}

std::vector<Pt> reversed(std::vector<Pt> v) {
  std::reverse(v.begin(), v.end());
  return v;
}

// Shapes with integer coordinates, counter-clockwise.
const std::vector<Pt> kStar = {{0, 100}, {-24, 32}, {-95, 31}, {-38, -12}, {-59, -81},
                               {0, -40}, {59, -81}, {38, -12}, {95, 31},   {24, 32}};
const std::vector<Pt> kComb = {{0, 0}, {10, 0}, {10, 8}, {8, 8}, {8, 2}, {6, 2}, {6, 8}, {4, 8}, {4, 2}, {2, 2}, {2, 8}, {0, 8}};
const std::vector<Pt> kSpiral = {{0, 1}, {13, 1}, {13, 13}, {3, 13}, {3, 5}, {9, 5}, {9, 9}, {6, 9},
                                 {6, 11}, {11, 11}, {11, 3}, {1, 3}, {1, 15}, {15, 15}, {15, -1}, {0, -1}};
const std::vector<Pt> kSquareCw = {{0, 0}, {0, 2}, {0, 4}, {2, 4}, {4, 4}, {4, 2}, {4, 0}, {2, 0}};

std::vector<std::uint32_t> triangulated(const std::vector<Pt>& poly, std::size_t* tris) {
  std::vector<std::uint32_t> idx(3 * poly.size());
  *tris = triangulate(poly.data(), poly.size(), idx.data());
  idx.resize(3 * *tris);
  return idx;
}

std::uint64_t golden(const std::vector<Pt>& poly) {
  std::size_t t = 0;
  const std::vector<std::uint32_t> idx = triangulated(poly, &t);
  REQUIRE(t > 0);
  return digest_indices(idx.data(), idx.size());
}

constexpr std::uint64_t kGoldenStar = 0x4c8185a3172a36a7ull;
constexpr std::uint64_t kGoldenComb = 0x8ae73f62f25b81bfull;
constexpr std::uint64_t kGoldenSpiral = 0x2bc354ceabfdba3dull;
constexpr std::uint64_t kGoldenSquareCw = 0xd9a50c642d2e1d37ull;

// Checks a triangulation: every triangle is counter-clockwise and the triangle areas add up exactly to the polygon
// area (integer coordinates make all areas exact in double). Returns the triangle count.
std::size_t check_triangulation(const std::vector<Pt>& poly) {
  std::size_t t = 0;
  const std::vector<std::uint32_t> idx = triangulated(poly, &t);
  REQUIRE(t > 0);
  double sum = 0.0;
  for (std::size_t i = 0; i < t; ++i) {
    const Pt tri[3] = {poly[idx[3 * i]], poly[idx[3 * i + 1]], poly[idx[3 * i + 2]]};
    REQUIRE(idx[3 * i] < poly.size());
    REQUIRE(idx[3 * i + 1] < poly.size());
    REQUIRE(idx[3 * i + 2] < poly.size());
    REQUIRE(orient(tri[0], tri[1], tri[2]) == 1);
    sum += signed_area2(tri, 3);
  }
  CHECK(sum == std::fabs(signed_area2(poly.data(), poly.size())));
  CHECK(t <= poly.size() - 2);
  return t;
}

}  // namespace

TEST_CASE("validate_polygon: every status", "[geom][validate]") {
  CHECK(validate_polygon(kStar.data(), kStar.size()) == PolyStatus::Ok);
  CHECK(std::string(poly_status_text(PolyStatus::Ok)) == "ok");
  const Pt tri[3] = {{0, 0}, {4, 0}, {0, 3}};
  CHECK(validate_polygon(tri, 3) == PolyStatus::Ok);
  // Both windings are accepted.
  CHECK(validate_polygon(reversed(kStar).data(), kStar.size()) == PolyStatus::Ok);

  CHECK(validate_polygon(tri, 0) == PolyStatus::TooFewVertices);
  CHECK(validate_polygon(tri, 2) == PolyStatus::TooFewVertices);
  CHECK(validate_polygon(nullptr, 2) == PolyStatus::TooFewVertices);  // count first
  std::vector<Pt> many(kMaxVertices + 1, Pt{0, 0});
  CHECK(validate_polygon(many.data(), many.size()) == PolyStatus::TooManyVertices);  // count beats contents
  CHECK(validate_polygon(nullptr, kMaxVertices + 1) == PolyStatus::TooManyVertices);
  CHECK(validate_polygon(nullptr, 3) == PolyStatus::BadVertex);

  for (float bad : {kNaN, kInf, -kInf, 1000000.5f, -1000001.0f, 1e30f}) {
    for (int comp = 0; comp < 2; ++comp) {
      Pt v[3] = {{0, 0}, {4, 0}, {0, 3}};
      (comp == 0 ? v[1].x : v[1].y) = bad;
      CHECK(validate_polygon(v, 3) == PolyStatus::BadVertex);
    }
  }
  const Pt lim[3] = {{-1e6f, -1e6f}, {1e6f, -1e6f}, {1e6f, 1e6f}};
  CHECK(validate_polygon(lim, 3) == PolyStatus::Ok);  // limit is inclusive

  // Degenerate: all collinear or coincident.
  const Pt same[4] = {{1, 1}, {1, 1}, {1, 1}, {1, 1}};
  CHECK(validate_polygon(same, 4) == PolyStatus::Degenerate);
  const Pt line[4] = {{0, 0}, {1, 1}, {2, 2}, {3, 3}};
  CHECK(validate_polygon(line, 4) == PolyStatus::Degenerate);
  const Pt line2[3] = {{0, 0}, {5, 0}, {2, 0}};
  CHECK(validate_polygon(line2, 3) == PolyStatus::Degenerate);
  const Pt line3[4] = {{0, 0}, {0, 0}, {3, 1}, {6, 2}};
  CHECK(validate_polygon(line3, 4) == PolyStatus::Degenerate);
  // Exactly collinear far apart: 0.1f is not exactly representable but these points are exactly on one line.
  const Pt farline[3] = {{-1e6f, -1e6f}, {0.0f, 0.0f}, {1e6f, 1e6f}};
  CHECK(validate_polygon(farline, 3) == PolyStatus::Degenerate);
  // Nearly collinear (not exactly) is a valid triangle.
  const Pt thin[3] = {{0, 0}, {1e6f, 1e6f}, {0.5f, 0.5000001f}};
  CHECK(validate_polygon(thin, 3) == PolyStatus::Ok);

  // Self intersection: crossing (bow tie), touching, overlap, repeated vertices.
  const Pt bow[4] = {{0, 0}, {4, 4}, {4, 0}, {0, 4}};
  CHECK(validate_polygon(bow, 4) == PolyStatus::SelfIntersecting);
  const Pt touch_vertex[5] = {{0, 0}, {4, 0}, {2, 3}, {4, 6}, {0, 6}};  // fine: concave notch, no touching
  CHECK(validate_polygon(touch_vertex, 5) == PolyStatus::Ok);
  const Pt touch_edge[6] = {{0, 0}, {6, 0}, {6, 6}, {3, 0}, {3, 3}, {0, 6}};  // vertex (3,0) lies on edge (0,0)-(6,0)
  CHECK(validate_polygon(touch_edge, 6) == PolyStatus::SelfIntersecting);
  const Pt pinch[8] = {{0, 0}, {4, 0}, {4, 4}, {2, 2}, {4, 4 + 0}, {0, 4}, {0, 2}, {0, 1}};  // repeated vertex (4,4)
  CHECK(validate_polygon(pinch, 8) == PolyStatus::SelfIntersecting);
  const Pt spike[5] = {{0, 0}, {4, 0}, {8, 0}, {4, 4}, {0, 4}};
  CHECK(validate_polygon(spike, 5) == PolyStatus::Ok);  // a collinear vertex on the bottom edge is allowed
  const Pt overlap[5] = {{0, 0}, {6, 0}, {3, 0}, {3, 5}, {0, 5}};     // edge (6,0)->(3,0) doubles back over (0,0)->(6,0)
  CHECK(validate_polygon(overlap, 5) == PolyStatus::SelfIntersecting);
  const Pt dup[5] = {{0, 0}, {4, 0}, {4, 0}, {4, 4}, {0, 4}};  // zero-length edge
  CHECK(validate_polygon(dup, 5) == PolyStatus::SelfIntersecting);
  const Pt cross_far[6] = {{0, 0}, {10, 0}, {10, 10}, {5, -5}, {0, 10}, {-1, 5}};
  CHECK(validate_polygon(cross_far, 6) == PolyStatus::SelfIntersecting);
  // Order of checks: bad vertex before degenerate before self intersection.
  const Pt mix[4] = {{0, 0}, {1, 1}, {2, 2}, {kNaN, 0}};
  CHECK(validate_polygon(mix, 4) == PolyStatus::BadVertex);
  // Zero net area but not collinear (a figure eight) is self-intersecting, not degenerate.
  const Pt eight[4] = {{0, 0}, {2, 2}, {2, 0}, {0, 2}};
  CHECK(validate_polygon(eight, 4) == PolyStatus::SelfIntersecting);
  CHECK(signed_area2(eight, 4) == 0.0);

  std::vector<std::string> texts;
  for (int s = 0; s <= 5; ++s) {
    const std::string t = poly_status_text(static_cast<PolyStatus>(s));
    CHECK_FALSE(t.empty());
    for (const std::string& o : texts) CHECK(o != t);
    texts.push_back(t);
  }
  CHECK(std::string(poly_status_text(static_cast<PolyStatus>(77))) == "unknown status");

  for (const auto* shape : {&kStar, &kComb, &kSpiral, &kSquareCw}) {
    CHECK(validate_polygon(shape->data(), shape->size()) == PolyStatus::Ok);
    CHECK(validate_polygon(reversed(*shape).data(), shape->size()) == PolyStatus::Ok);
  }
}

TEST_CASE("orient is exact where plain double arithmetic is not", "[geom][orient]") {
  // These cases were found by search: the plain double determinant has the wrong sign (or zero) while the exact
  // determinant (computed with rational arithmetic) is negative. Coordinates are given as float bit patterns.
  const std::uint32_t hard[4][6] = {
      {0x48a69880u, 0x48a69880u, 0x3b89f7d8u, 0x399ce373u, 0x3b89f7d8u, 0x399ce374u},
      {0x48e8d660u, 0x48e8d660u, 0x3b08095du, 0x39eceb68u, 0x3b08095du, 0x39eceb69u},
      {0x48b593c0u, 0x48b593c0u, 0x3ba6a9cfu, 0x39a7cc17u, 0x3ba6a9cfu, 0x39a7cc19u},
      {0x48c149e0u, 0x48c149e0u, 0x3b1713b2u, 0x3a78d022u, 0x3b1713b2u, 0x3a78d023u}};
  for (const auto& h : hard) {
    const Pt a{from_bits(h[0]), from_bits(h[1])}, b{from_bits(h[2]), from_bits(h[3])}, c{from_bits(h[4]), from_bits(h[5])};
    CHECK(orient(a, b, c) == -1);
    CHECK(orient(b, c, a) == -1);  // cyclic shifts keep the sign
    CHECK(orient(c, a, b) == -1);
    CHECK(orient(a, c, b) == 1);  // swaps flip it
    CHECK(orient(b, a, c) == 1);
    CHECK(orient(c, b, a) == 1);
  }
  // Huge coordinates: no overflow, exact zero for exactly collinear points.
  CHECK(orient({1e30f, 1e30f}, {0, 0}, {-1e30f, -1e30f}) == 0);
  CHECK(orient({3e38f, 3e38f}, {1.5e38f, 1.5e38f}, {-3e38f, -3e38f}) == 0);
  CHECK(orient({3e38f, 0}, {0, 3e38f}, {1, 1}) == 1);
  CHECK(orient({3e38f, 0}, {0, 3e38f}, {3e38f, 3e38f}) == -1);
  CHECK(orient({1e-40f, 0}, {0, 1e-40f}, {0, 0}) == 1);  // subnormals
  CHECK(orient({1e-45f, 0}, {0, 1e-45f}, {1e-45f, 1e-45f}) == -1);
  CHECK(orient({0, 0}, {1, 0}, {0, 1}) == 1);
  CHECK(orient({0, 0}, {0, 1}, {1, 0}) == -1);
  CHECK(orient({0, 0}, {1, 1}, {2, 2}) == 0);
  CHECK(orient({0, 0}, {0, 0}, {5, 7}) == 0);  // coincident points
  CHECK(orient({-0.0f, 0.0f}, {1, 0}, {0, 1}) == 1);  // signed zero is just zero
  // Not finite: 0.
  CHECK(orient({kNaN, 0}, {1, 0}, {0, 1}) == 0);
  CHECK(orient({0, 0}, {kInf, 0}, {0, 1}) == 0);
  CHECK(orient({0, 0}, {1, 0}, {0, -kInf}) == 0);
  // Integer grids: compare with a 64 bit integer determinant (all values below 2^21 so the integer math is exact).
  std::uint32_t s = 2024;
  auto rnd = [&s](int range) {
    s = s * 1664525u + 1013904223u;
    return static_cast<int>((s >> 8) % static_cast<std::uint32_t>(2 * range + 1)) - range;
  };
  for (int it = 0; it < 20000; ++it) {
    const int range = it % 3 == 0 ? 3 : (it % 3 == 1 ? 1000 : 1000000);
    const int ax = rnd(range), ay = rnd(range), bx = rnd(range), by = rnd(range);
    int cx = rnd(range), cy = rnd(range);
    if (it % 2 == 0) {  // force near collinear
      cx = ax + (bx - ax) * (it % 5 - 2);
      cy = ay + (by - ay) * (it % 5 - 2) + (it % 4 == 0 ? 1 : 0);
    }
    const long long det = static_cast<long long>(bx - ax) * (cy - ay) - static_cast<long long>(by - ay) * (cx - ax);
    const int expect = det > 0 ? 1 : (det < 0 ? -1 : 0);
    const Pt a{static_cast<float>(ax), static_cast<float>(ay)}, b{static_cast<float>(bx), static_cast<float>(by)},
        c{static_cast<float>(cx), static_cast<float>(cy)};
    REQUIRE(orient(a, b, c) == expect);
  }
}

TEST_CASE("signed_area2", "[geom][area]") {
  const Pt sq[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  CHECK(signed_area2(sq, 4) == 2.0);
  const Pt cw[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
  CHECK(signed_area2(cw, 4) == -2.0);
  CHECK(signed_area2(kStar.data(), kStar.size()) == -signed_area2(reversed(kStar).data(), kStar.size()));
  CHECK(signed_area2(kComb.data(), kComb.size()) == 112.0);  // base 10 x 2 plus three teeth of 2 x 6: area 56
  CHECK(signed_area2(kSquareCw.data(), kSquareCw.size()) == -32.0);
  CHECK(signed_area2(nullptr, 5) == 0.0);
  CHECK(signed_area2(sq, 2) == 0.0);
  const Pt nan[3] = {{0, 0}, {kNaN, 1}, {1, 0}};
  CHECK(signed_area2(nan, 3) == 0.0);
  // Exact summation: large terms cancel exactly and the small remainder survives (plain double arithmetic gives 0).
  const Pt big[4] = {{0, 0}, {1e6f, 0}, {1e6f, 1e6f}, {0, 1e6f}};
  CHECK(signed_area2(big, 4) == 2e12);
  const Pt shifted[4] = {{999999.0f, 999999.0f}, {1e6f, 999999.0f}, {1e6f, 1e6f}, {999999.0f, 1e6f}};
  CHECK(signed_area2(shifted, 4) == 2.0);
  const Pt tiny[3] = {{1e6f, 1e6f}, {1e6f, 1e6f + 0.0625f}, {1e6f + 0.0625f, 1e6f}};
  CHECK(signed_area2(tiny, 3) == -0.00390625);
}

TEST_CASE("triangulate: triangle counts, winding, areas", "[geom][triangulate]") {
  // Convex and concave shapes, both windings. Without collinear vertices there are n - 2 triangles.
  for (const auto* shape : {&kStar, &kComb, &kSpiral}) {
    CHECK(check_triangulation(*shape) == shape->size() - 2);
    CHECK(check_triangulation(reversed(*shape)) == shape->size() - 2);
  }
  const std::vector<Pt> tri = {{0, 0}, {4, 0}, {0, 3}};
  CHECK(check_triangulation(tri) == 1);
  CHECK(check_triangulation(reversed(tri)) == 1);
  const std::vector<Pt> square = {{0, 0}, {4, 0}, {4, 4}, {0, 4}};
  CHECK(check_triangulation(square) == 2);
  // Collinear vertices are removed as zero-area ears: fewer than n - 2 triangles, same area.
  const std::size_t c = check_triangulation(kSquareCw);
  CHECK(c <= kSquareCw.size() - 2);
  CHECK(c >= 2);
  check_triangulation(reversed(kSquareCw));
  const std::vector<Pt> collinear_tri = {{0, 0}, {2, 0}, {4, 0}, {2, 3}};
  CHECK(check_triangulation(collinear_tri) == 2);
  // A collinear vertex that is scanned first is removed without a triangle: 5 vertices give 2 triangles, not 3.
  const std::vector<Pt> first_collinear = {{2, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  CHECK(check_triangulation(first_collinear) == 2);
  // A comb, a spiral and a star with collinear points added along edges.
  std::vector<Pt> dense;
  for (std::size_t i = 0; i < kComb.size(); ++i) {
    const Pt a = kComb[i], b = kComb[(i + 1) % kComb.size()];
    dense.push_back(a);
    dense.push_back({(a.x + b.x) / 2, (a.y + b.y) / 2});  // midpoints of even-sized integer edges are integers
  }
  check_triangulation(dense);
  // Right and left turns at the vertices of a rectilinear spiral polygon.
  CHECK(validate_polygon(kSpiral.data(), kSpiral.size()) == PolyStatus::Ok);

  // Invalid polygons: 0 and the output is untouched.
  std::uint32_t out[30];
  std::fill(out, out + 30, 0xDEADBEEFu);
  const Pt bow[4] = {{0, 0}, {4, 4}, {4, 0}, {0, 4}};
  CHECK(triangulate(bow, 4, out) == 0);
  CHECK(triangulate(kStar.data(), 2, out) == 0);
  CHECK(triangulate(nullptr, 5, out) == 0);
  const Pt line[3] = {{0, 0}, {1, 1}, {2, 2}};
  CHECK(triangulate(line, 3, out) == 0);
  CHECK(triangulate(kStar.data(), kStar.size(), nullptr) == 0);
  for (std::uint32_t v : out) CHECK(v == 0xDEADBEEFu);
  // Ear choice is deterministic: the same input gives the same indices, and a rotation of the vertex list gives the
  // same triangles up to relabelling of indices (area check).
  std::size_t t1 = 0, t2 = 0;
  CHECK(triangulated(kSpiral, &t1) == triangulated(kSpiral, &t2));
  std::vector<Pt> rot(kSpiral.begin() + 5, kSpiral.end());
  rot.insert(rot.end(), kSpiral.begin(), kSpiral.begin() + 5);
  CHECK(check_triangulation(rot) == kSpiral.size() - 2);

  // The first triangle of a convex polygon clips the lowest-index ear: vertex 0 with its neighbours.
  const std::vector<Pt> hex = {{2, 0}, {4, 0}, {6, 2}, {4, 4}, {2, 4}, {0, 2}};
  std::size_t th = 0;
  const std::vector<std::uint32_t> hi = triangulated(hex, &th);
  REQUIRE(th == 4);
  CHECK(hi[0] == 5); CHECK(hi[1] == 0); CHECK(hi[2] == 1);
  // Clockwise input: the same ear is written with its last two indices swapped so the triangle is counter-clockwise.
  const std::vector<Pt> hexcw = reversed(hex);
  const std::vector<std::uint32_t> hc = triangulated(hexcw, &th);
  REQUIRE(th == 4);
  CHECK(orient(hexcw[hc[0]], hexcw[hc[1]], hexcw[hc[2]]) == 1);
}

TEST_CASE("point_in_polygon", "[geom][pip]") {
  const std::vector<Pt>& u = kComb;  // a concave shape
  CHECK(point_in_polygon(u.data(), u.size(), {5, 1}));      // base
  CHECK(point_in_polygon(u.data(), u.size(), {1, 5}));      // inside a tooth
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {3, 5}));  // between teeth
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {7, 5}));
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {-1, 1}));
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {11, 4}));
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {5, 9}));
  // Edges and vertices count as inside, including the inner notches.
  CHECK(point_in_polygon(u.data(), u.size(), {0, 0}));
  CHECK(point_in_polygon(u.data(), u.size(), {5, 0}));
  CHECK(point_in_polygon(u.data(), u.size(), {10, 4}));
  CHECK(point_in_polygon(u.data(), u.size(), {3, 2}));   // on the notch floor
  CHECK(point_in_polygon(u.data(), u.size(), {8, 5}));   // on a tooth wall
  CHECK(point_in_polygon(u.data(), u.size(), {6, 2}));   // notch corner
  CHECK(point_in_polygon(u.data(), u.size(), {9, 8}));   // top of a tooth
  // A ray through vertices: the horizontal line y = 2 passes through notch corners.
  CHECK(point_in_polygon(u.data(), u.size(), {-0.5f, 2}) == false);
  CHECK(point_in_polygon(u.data(), u.size(), {1, 2}));
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {2.5f, 8}));
  // Both windings give the same answers.
  const std::vector<Pt> r = reversed(u);
  for (float x = -1.0f; x <= 11.0f; x += 0.5f) {
    for (float y = -1.0f; y <= 9.0f; y += 0.5f) {
      REQUIRE(point_in_polygon(u.data(), u.size(), {x, y}) == point_in_polygon(r.data(), r.size(), {x, y}));
    }
  }
  // Spiral and star.
  CHECK(point_in_polygon(kSpiral.data(), kSpiral.size(), {7, 0}));
  CHECK_FALSE(point_in_polygon(kSpiral.data(), kSpiral.size(), {7, 7}));  // the gap inside the spiral
  CHECK(point_in_polygon(kStar.data(), kStar.size(), {0, 0}));
  CHECK_FALSE(point_in_polygon(kStar.data(), kStar.size(), {-60, 60}));
  // Not finite or malformed input: outside.
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {kNaN, 1}));
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {1, kNaN}));
  CHECK_FALSE(point_in_polygon(u.data(), u.size(), {kInf, 1}));
  CHECK_FALSE(point_in_polygon(nullptr, 5, {1, 1}));
  CHECK_FALSE(point_in_polygon(u.data(), 2, {1, 1}));
  std::vector<Pt> bad = u;
  bad[3].x = kNaN;
  CHECK_FALSE(point_in_polygon(bad.data(), bad.size(), {5, 1}));
  // Exactness near an edge: a point one float step off a slanted edge.
  const Pt slant[3] = {{0, 0}, {1e6f, 1e6f - 1.0f}, {0, 1e6f}};
  const Pt on{500000.0f, 499999.5f};  // exactly on the edge (0,0)-(1e6, 1e6-1)
  CHECK(point_in_polygon(slant, 3, on));
  CHECK(point_in_polygon(slant, 3, {on.x, std::nextafterf(on.y, 1e7f)}));   // inside (above the edge)
  CHECK_FALSE(point_in_polygon(slant, 3, {on.x, std::nextafterf(on.y, -1e7f)}));
}

namespace {

// Brute force: a distinct point is a hull vertex unless it is a convex combination of others on a segment or in a
// triangle (closed), using exact orient.
std::set<std::pair<float, float>> brute_hull(const std::vector<Pt>& pts) {
  std::set<std::pair<float, float>> d;
  for (const Pt& p : pts) d.insert({p.x, p.y});
  std::vector<Pt> u;
  for (const auto& q : d) u.push_back({q.first, q.second});
  std::set<std::pair<float, float>> hull;
  const std::size_t m = u.size();
  for (std::size_t i = 0; i < m; ++i) {
    bool vertex = true;
    for (std::size_t a = 0; a < m && vertex; ++a) {
      if (a == i) continue;
      for (std::size_t b = a + 1; b < m && vertex; ++b) {
        if (b == i) continue;
        // On the closed segment ab, not at an end.
        if (orient(u[a], u[b], u[i]) == 0 && std::min(u[a].x, u[b].x) <= u[i].x && u[i].x <= std::max(u[a].x, u[b].x) &&
            std::min(u[a].y, u[b].y) <= u[i].y && u[i].y <= std::max(u[a].y, u[b].y)) {
          vertex = false;
          break;
        }
        for (std::size_t c = b + 1; c < m && vertex; ++c) {
          if (c == i) continue;
          const int o1 = orient(u[a], u[b], u[i]), o2 = orient(u[b], u[c], u[i]), o3 = orient(u[c], u[a], u[i]);
          const int w = orient(u[a], u[b], u[c]);
          if (w == 0) continue;
          if (o1 * w >= 0 && o2 * w >= 0 && o3 * w >= 0) vertex = false;
        }
      }
    }
    if (vertex) hull.insert({u[i].x, u[i].y});
  }
  return hull;
}

}  // namespace

TEST_CASE("convex_hull against brute force", "[geom][hull]") {
  std::uint32_t s = 77;
  auto rnd = [&s](int range) {
    s = s * 1664525u + 1013904223u;
    return static_cast<int>((s >> 8) % static_cast<std::uint32_t>(range));
  };
  for (int it = 0; it < 300; ++it) {
    const int count = 1 + it % 24;
    const int grid = it % 4 == 0 ? 3 : (it % 4 == 1 ? 6 : 12);  // small grids: many duplicates and collinear points
    std::vector<Pt> pts;
    for (int i = 0; i < count; ++i) pts.push_back({static_cast<float>(rnd(grid)), static_cast<float>(rnd(grid))});
    std::vector<Pt> out(pts.size());
    const std::size_t k = convex_hull(pts.data(), pts.size(), out.data());
    out.resize(k);
    const auto expect = brute_hull(pts);
    std::set<std::pair<float, float>> got;
    for (const Pt& p : out) got.insert({p.x, p.y});
    REQUIRE(got.size() == k);  // no duplicates
    REQUIRE(got == expect);
    if (k >= 3) {
      for (std::size_t i = 0; i < k; ++i) REQUIRE(orient(out[i], out[(i + 1) % k], out[(i + 2) % k]) == 1);  // CCW, no collinear
    }
    // Start at the lowest x, lowest y on ties.
    for (const Pt& p : out) REQUIRE((out[0].x < p.x || (out[0].x == p.x && out[0].y <= p.y)));
  }
}

TEST_CASE("convex_hull edge cases", "[geom][hull]") {
  Pt out[16];
  CHECK(convex_hull(nullptr, 0, out) == 0);
  const Pt one[1] = {{3, 4}};
  CHECK(convex_hull(one, 1, out) == 1);
  CHECK(out[0].x == 3.0f); CHECK(out[0].y == 4.0f);
  const Pt dup[3] = {{3, 4}, {3, 4}, {3, 4}};
  CHECK(convex_hull(dup, 3, out) == 1);
  const Pt two[2] = {{5, 1}, {2, 2}};
  CHECK(convex_hull(two, 2, out) == 2);
  CHECK(out[0].x == 2.0f); CHECK(out[1].x == 5.0f);
  const Pt line[5] = {{0, 0}, {3, 3}, {1, 1}, {2, 2}, {3, 3}};
  CHECK(convex_hull(line, 5, out) == 2);
  CHECK(out[0].x == 0.0f); CHECK(out[1].x == 3.0f);
  const Pt vline[4] = {{1, 5}, {1, 1}, {1, 3}, {1, 9}};
  CHECK(convex_hull(vline, 4, out) == 2);
  CHECK(out[0].y == 1.0f); CHECK(out[1].y == 9.0f);  // lowest y on equal x first
  const Pt sq[6] = {{0, 0}, {2, 0}, {2, 2}, {0, 2}, {1, 1}, {1, 0}};  // (1,0) is collinear on the bottom edge
  CHECK(convex_hull(sq, 6, out) == 4);
  CHECK(out[0].x == 0.0f); CHECK(out[0].y == 0.0f);
  CHECK(out[1].x == 2.0f); CHECK(out[1].y == 0.0f);
  CHECK(out[2].x == 2.0f); CHECK(out[2].y == 2.0f);
  CHECK(out[3].x == 0.0f); CHECK(out[3].y == 2.0f);
  const Pt nan[3] = {{0, 0}, {1, kNaN}, {2, 2}};
  CHECK(convex_hull(nan, 3, out) == 0);
  const Pt inf[3] = {{0, 0}, {kInf, 1}, {2, 2}};
  CHECK(convex_hull(inf, 3, out) == 0);
  const Pt ninf[3] = {{0, 0}, {-kInf, 1}, {2, 2}};
  CHECK(convex_hull(ninf, 3, out) == 0);
  // -0 and +0 are the same point; the first in input order is kept.
  const Pt zeros[3] = {{-0.0f, 0.0f}, {0.0f, 0.0f}, {1, 1}};
  CHECK(convex_hull(zeros, 3, out) == 2);
  CHECK(std::signbit(out[0].x));
  // Huge and tiny coordinates are handled exactly.
  const Pt wide[5] = {{-3e38f, -3e38f}, {3e38f, -3e38f}, {3e38f, 3e38f}, {-3e38f, 3e38f}, {0, 0}};
  CHECK(convex_hull(wide, 5, out) == 4);
  const Pt nearly[4] = {{0, 0}, {1e6f, 1e6f}, {0.5f, 0.5000001f}, {2e5f, 2e5f}};
  CHECK(convex_hull(nearly, 4, out) == 3);  // the slightly off point is a vertex
}

TEST_CASE("digest_indices is FNV-1a 64 over little-endian index bytes", "[geom][digest]") {
  CHECK(digest_indices(nullptr, 0) == 0xcbf29ce484222325ull);
  const std::uint32_t zero[1] = {0};
  CHECK(digest_indices(zero, 1) == 0x4d25767f9dce13f5ull);  // four zero bytes
  const std::uint32_t pair[2] = {1, 2};
  CHECK(digest_indices(pair, 2) == 0xc9c28939c99668c6ull);
  const std::uint32_t a[2] = {1, 2}, b[2] = {2, 1};
  CHECK(digest_indices(a, 2) != digest_indices(b, 2));
}

TEST_CASE("golden digests of triangulations", "[geom][digest]") {
  const std::uint64_t star = golden(kStar), comb = golden(kComb), spiral = golden(kSpiral), sq = golden(kSquareCw);
  INFO(std::hex << "star 0x" << star << " comb 0x" << comb << " spiral 0x" << spiral << " square 0x" << sq);
  CHECK(star == kGoldenStar);
  CHECK(comb == kGoldenComb);
  CHECK(spiral == kGoldenSpiral);
  CHECK(sq == kGoldenSquareCw);
  CHECK(golden(kStar) == star);  // repeatable
}

TEST_CASE("performance: 4096 vertices", "[geom][perf]") {
  const auto start = std::chrono::steady_clock::now();
  // A strictly convex polygon: a parabola arc closed by one straight edge.
  std::vector<Pt> convex(kMaxVertices);
  for (std::size_t i = 0; i < kMaxVertices; ++i) {
    const float x = (static_cast<float>(i) - 2048.0f) / 4.0f;
    convex[i] = {x, x * x};
  }
  REQUIRE(validate_polygon(convex.data(), convex.size()) == PolyStatus::Ok);
  CHECK(check_triangulation(convex) == kMaxVertices - 2);
  // A zigzag strip (many reflex vertices) with collinear vertices along the bottom: 2048 + 2048 vertices.
  std::vector<Pt> zig;
  for (int i = 0; i < 2048; ++i) zig.push_back({static_cast<float>(2 * i), -5.0f});
  for (int i = 2047; i >= 0; --i) zig.push_back({static_cast<float>(2 * i), (i % 2) ? 10.0f : 0.0f});
  REQUIRE(zig.size() == kMaxVertices);
  REQUIRE(validate_polygon(zig.data(), zig.size()) == PolyStatus::Ok);
  const std::size_t t = check_triangulation(zig);
  CHECK(t > 2000);
  // Point queries and a hull of the same vertices.
  int inside = 0;
  for (int i = 0; i < 2000; ++i) inside += point_in_polygon(zig.data(), zig.size(), {static_cast<float>(i * 2 + 1), 1.0f}) ? 1 : 0;
  CHECK(inside > 0);
  std::vector<Pt> hull(zig.size());
  CHECK(convex_hull(zig.data(), zig.size(), hull.data()) == 5);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("took " << secs << " s");
  CHECK(secs < 10.0);
}

TEST_CASE("triangulate always finds an ear for valid polygons (no fallback pass exists)", "[geom][triangulate][fuzz]") {
  std::mt19937 rng(5);
  long valid = 0;
  for (int it = 0; it < 60000; ++it) {
    const int n = 3 + static_cast<int>(rng() % 12), g = 2 + static_cast<int>(rng() % 6);
    std::vector<Pt> p;
    if (rng() % 2) {  // star-shaped around the origin: repeated directions give collinear vertices
      for (int i = 0; i < n; ++i) {
        p.push_back({static_cast<float>(static_cast<int>(rng() % (2 * g + 1)) - g),
                     static_cast<float>(static_cast<int>(rng() % (2 * g + 1)) - g)});
      }
      std::sort(p.begin(), p.end(), [](const Pt& a, const Pt& b) { return std::atan2(a.y, a.x) < std::atan2(b.y, b.x); });
    } else {
      for (int i = 0; i < n; ++i) {
        p.push_back({static_cast<float>(rng() % (2 * g + 1)), static_cast<float>(rng() % (2 * g + 1))});
      }
    }
    if (validate_polygon(p.data(), p.size()) != PolyStatus::Ok) continue;
    ++valid;
    std::vector<std::uint32_t> out(3 * p.size());
    const std::size_t t = triangulate(p.data(), p.size(), out.data());
    REQUIRE(t >= 1);
    REQUIRE(t <= p.size() - 2);
    double sum = 0;
    for (std::size_t k = 0; k < t; ++k) {
      const double a2 = signed_area2(std::vector<Pt>{p[out[3 * k]], p[out[3 * k + 1]], p[out[3 * k + 2]]}.data(), 3);
      REQUIRE(a2 > 0);
      sum += a2;
    }
    REQUIRE(sum == std::fabs(signed_area2(p.data(), p.size())));
  }
  REQUIRE(valid > 10000);
}
