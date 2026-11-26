// 2D polygon geometry. Exactness argument, rules, ear choice and golden digests: docs/geom.md.
// Determinism: only + - * / and comparisons on floats and doubles; no libm function is called. The orientation sign and
// the area are computed by exact floating-point expansions (sums of exact float*float products), so every decision is
// exact and does not depend on the host, on FMA contraction or on the optimisation level. Compilers are asked not to
// fuse multiply-add anyway; the project must not enable fast-math.
#include "geom.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::geom {
namespace {

constexpr double kMaxCoord = 1e6;

// ---------------------------------------------------------------------------------------------------------------
// Exact arithmetic. Every float*float product is exact in double (48 significant bits, exponent range far inside
// double's). Sums of doubles are kept exact as nonoverlapping expansions (Shewchuk): two_sum is exact unless it
// overflows, and the sums here stay below 2^270.
inline void two_sum(double a, double b, double& x, double& y) {
  x = a + b;
  const double bv = x - a;
  const double av = x - bv;
  const double br = b - bv;
  const double ar = a - av;
  y = ar + br;
}

// Nonoverlapping expansion, components in increasing magnitude, no zero components.
struct Expansion {
  std::vector<double> c;
  void add(double b) {
    if (b == 0.0) return;
    std::size_t out = 0;
    double q = b;
    for (std::size_t i = 0; i < c.size(); ++i) {
      double nq, h;
      two_sum(q, c[i], nq, h);
      if (h != 0.0) c[out++] = h;
      q = nq;
    }
    c.resize(out);
    if (q != 0.0) c.push_back(q);
  }
  int sign() const { return c.empty() ? 0 : (c.back() > 0.0 ? 1 : -1); }  // the largest component decides
  double value() const {  // sum from the smallest component up: within one unit in the last place of the exact sum
    double s = 0.0;
    for (double v : c) s += v;
    return s;
  }
};

inline double prod(float a, float b) { return static_cast<double>(a) * static_cast<double>(b); }

bool finite(const Pt& p) { return std::isfinite(p.x) && std::isfinite(p.y); }

int sgn_cmp(float a, float b) { return (a > b) - (a < b); }

bool same(const Pt& a, const Pt& b) { return a.x == b.x && a.y == b.y; }

// c lies in the closed bounding box of a and b (used only when a, b, c are collinear).
bool in_box(const Pt& a, const Pt& b, const Pt& c) {
  return std::min(a.x, b.x) <= c.x && c.x <= std::max(a.x, b.x) && std::min(a.y, b.y) <= c.y &&
         c.y <= std::max(a.y, b.y);
}

// Closed segments ab and cd share at least one point.
bool segments_touch(const Pt& a, const Pt& b, const Pt& c, const Pt& d) {
  if (std::max(a.x, b.x) < std::min(c.x, d.x) || std::max(c.x, d.x) < std::min(a.x, b.x) ||
      std::max(a.y, b.y) < std::min(c.y, d.y) || std::max(c.y, d.y) < std::min(a.y, b.y)) {
    return false;
  }
  const int o1 = orient(a, b, c), o2 = orient(a, b, d), o3 = orient(c, d, a), o4 = orient(c, d, b);
  if (o1 * o2 < 0 && o3 * o4 < 0) return true;
  if (o1 == 0 && in_box(a, b, c)) return true;
  if (o2 == 0 && in_box(a, b, d)) return true;
  if (o3 == 0 && in_box(c, d, a)) return true;
  if (o4 == 0 && in_box(c, d, b)) return true;
  return false;
}

// Edges (p,s) and (s,q) share the vertex s. They overlap when they run in the same direction from s.
bool adjacent_overlap(const Pt& p, const Pt& s, const Pt& q) {
  if (same(p, s) || same(q, s)) return true;  // zero-length edge
  if (orient(p, s, q) != 0) return false;
  // Collinear: compare directions from s along the axis where p differs from s (no arithmetic needed).
  const int px = sgn_cmp(p.x, s.x), qx = sgn_cmp(q.x, s.x);
  if (px != 0) return px == qx;
  return sgn_cmp(p.y, s.y) == sgn_cmp(q.y, s.y);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
const char* poly_status_text(PolyStatus s) {
  switch (s) {
    case PolyStatus::Ok: return "ok";
    case PolyStatus::TooFewVertices: return "too few vertices";
    case PolyStatus::TooManyVertices: return "too many vertices";
    case PolyStatus::BadVertex: return "bad vertex";
    case PolyStatus::Degenerate: return "degenerate polygon";
    case PolyStatus::SelfIntersecting: return "self-intersecting polygon";
  }
  return "unknown status";
}

int orient(const Pt& a, const Pt& b, const Pt& c) {
  if (!finite(a) || !finite(b) || !finite(c)) return 0;
  // Filter: the plain double determinant with Shewchuk's error bound. Its sign is returned only when the bound proves
  // it; otherwise the exact expansion decides, so the result is always the exact sign.
  const double acx = static_cast<double>(b.x) - static_cast<double>(a.x);
  const double acy = static_cast<double>(c.y) - static_cast<double>(a.y);
  const double bcx = static_cast<double>(b.y) - static_cast<double>(a.y);
  const double bcy = static_cast<double>(c.x) - static_cast<double>(a.x);
  const double left = acx * acy, right = bcx * bcy;
  const double det = left - right;
  const double bound = 3.3306690738754716e-16 * (std::fabs(left) + std::fabs(right));
  if (det > bound) return 1;
  if (-det > bound) return -1;
  // Exact: det = bx*cy - bx*ay - ax*cy - by*cx + by*ax + ay*cx (the ax*ay terms cancel).
  Expansion e;
  e.add(prod(b.x, c.y));
  e.add(-prod(b.x, a.y));
  e.add(-prod(a.x, c.y));
  e.add(-prod(b.y, c.x));
  e.add(prod(b.y, a.x));
  e.add(prod(a.y, c.x));
  return e.sign();
}

namespace {

// Exact shoelace sum as an expansion.
void area2_expansion(const Pt* v, std::size_t n, Expansion& e) {
  for (std::size_t i = 0; i < n; ++i) {
    const Pt& a = v[i];
    const Pt& b = v[i + 1 == n ? 0 : i + 1];
    e.add(prod(a.x, b.y));
    e.add(-prod(b.x, a.y));
  }
}

}  // namespace

double signed_area2(const Pt* v, std::size_t n) {
  if (v == nullptr || n < 3) return 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite(v[i])) return 0.0;
  }
  Expansion e;
  area2_expansion(v, n, e);
  return e.value();
}

PolyStatus validate_polygon(const Pt* v, std::size_t n) {
  if (n < 3) return PolyStatus::TooFewVertices;
  if (n > kMaxVertices) return PolyStatus::TooManyVertices;
  if (v == nullptr) return PolyStatus::BadVertex;
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite(v[i]) || std::fabs(static_cast<double>(v[i].x)) > kMaxCoord ||
        std::fabs(static_cast<double>(v[i].y)) > kMaxCoord) {
      return PolyStatus::BadVertex;
    }
  }
  // Degenerate: all vertices coincide or are collinear (exact).
  std::size_t k = 1;
  while (k < n && same(v[k], v[0])) ++k;
  if (k == n) return PolyStatus::Degenerate;
  bool all_collinear = true;
  for (std::size_t i = k + 1; i < n && all_collinear; ++i) all_collinear = orient(v[0], v[k], v[i]) == 0;
  if (all_collinear) return PolyStatus::Degenerate;
  // Self intersection: adjacent edges may only share their common vertex, other edges may not touch.
  for (std::size_t i = 0; i < n; ++i) {
    if (adjacent_overlap(v[i == 0 ? n - 1 : i - 1], v[i], v[i + 1 == n ? 0 : i + 1])) return PolyStatus::SelfIntersecting;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const Pt& a = v[i];
    const Pt& b = v[i + 1 == n ? 0 : i + 1];
    for (std::size_t j = i + 2; j < n; ++j) {
      if (i == 0 && j == n - 1) continue;  // adjacent through the closing edge
      const Pt& c = v[j];
      const Pt& d = v[j + 1 == n ? 0 : j + 1];
      if (segments_touch(a, b, c, d)) return PolyStatus::SelfIntersecting;
    }
  }
  return PolyStatus::Ok;
}

std::size_t triangulate(const Pt* v, std::size_t n, std::uint32_t* out_indices) {
  if (out_indices == nullptr || validate_polygon(v, n) != PolyStatus::Ok) return 0;
  Expansion area;
  area2_expansion(v, n, area);
  const int w = area.sign();  // +1 counter-clockwise, -1 clockwise (never 0 for a valid polygon)
  std::vector<std::uint32_t> next(n), prev(n);
  std::vector<std::uint8_t> reflex(n, 0);  // not strictly convex: reflex or collinear
  for (std::size_t i = 0; i < n; ++i) {
    next[i] = static_cast<std::uint32_t>(i + 1 == n ? 0 : i + 1);
    prev[i] = static_cast<std::uint32_t>(i == 0 ? n - 1 : i - 1);
  }
  auto convexity = [&](std::uint32_t i) { return orient(v[prev[i]], v[i], v[next[i]]) * w; };
  for (std::size_t i = 0; i < n; ++i) reflex[i] = convexity(static_cast<std::uint32_t>(i)) <= 0 ? 1 : 0;

  // A remaining vertex blocks the ear (a, b, c) if it lies inside it or on its boundary.
  auto blocked = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    for (std::uint32_t k = next[c]; k != a; k = next[k]) {
      if (!reflex[k]) continue;  // a strictly convex vertex cannot lie inside an ear of a simple polygon
      const int o1 = orient(v[a], v[b], v[k]) * w, o2 = orient(v[b], v[c], v[k]) * w, o3 = orient(v[c], v[a], v[k]) * w;
      if (o1 >= 0 && o2 >= 0 && o3 >= 0) return true;
    }
    return false;
  };

  std::size_t remaining = n, tris = 0;
  std::uint32_t start = 0;
  while (remaining > 3) {
    bool clipped = false;
    std::uint32_t cur = start;
    for (std::size_t tried = 0; tried < remaining; ++tried, cur = next[cur]) {
      const std::uint32_t a = prev[cur], c = next[cur];
      const int cv = convexity(cur);
      bool ear;
      if (cv == 0) {
        ear = true;  // zero-area ear: removed without a triangle
      } else {
        ear = cv > 0 && !blocked(a, cur, c);
      }
      if (!ear) continue;
      if (cv != 0) {
        out_indices[3 * tris + 0] = a;
        out_indices[3 * tris + 1] = w > 0 ? cur : c;
        out_indices[3 * tris + 2] = w > 0 ? c : cur;
        ++tris;
      }
      next[a] = c;
      prev[c] = a;
      --remaining;
      reflex[a] = convexity(a) <= 0 ? 1 : 0;
      reflex[c] = convexity(c) <= 0 ? 1 : 0;
      start = a;  // restart the scan from the previous vertex
      clipped = true;
      break;
    }
    // A valid polygon always has an ear (two ears theorem; validation rejects touching, and all tests are exact),
    // so this is not reached. It only guards against an endless loop.
    if (!clipped) return 0;
  }
  const std::uint32_t a = start, b = next[a], c = next[b];
  if (orient(v[a], v[b], v[c]) != 0) {
    out_indices[3 * tris + 0] = a;
    out_indices[3 * tris + 1] = w > 0 ? b : c;
    out_indices[3 * tris + 2] = w > 0 ? c : b;
    ++tris;
  }
  return tris;
}

bool point_in_polygon(const Pt* v, std::size_t n, const Pt& p) {
  if (v == nullptr || n < 3 || !finite(p)) return false;
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite(v[i])) return false;
  }
  bool inside = false;
  for (std::size_t i = 0; i < n; ++i) {
    const Pt& a = v[i];
    const Pt& b = v[i + 1 == n ? 0 : i + 1];
    if (in_box(a, b, p) && orient(a, b, p) == 0) return true;  // on an edge or vertex
    if ((a.y > p.y) != (b.y > p.y)) {
      // The edge crosses the horizontal line through p; the crossing is right of p when p is left of the upward edge
      // or right of the downward edge.
      const int o = orient(a, b, p);
      if (b.y > a.y ? o > 0 : o < 0) inside = !inside;
    }
  }
  return inside;
}

std::size_t convex_hull(const Pt* pts, std::size_t n, Pt* out) {
  if (n == 0) return 0;
  if (pts == nullptr || out == nullptr) return 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite(pts[i])) return 0;
  }
  std::vector<Pt> s(pts, pts + n);
  std::stable_sort(s.begin(), s.end(), [](const Pt& a, const Pt& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
  // Remove duplicates (-0 and +0 compare equal; the first in input order stays).
  std::size_t m = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (m == 0 || !same(s[m - 1], s[i])) s[m++] = s[i];
  }
  if (m == 1) {
    out[0] = s[0];
    return 1;
  }
  std::vector<Pt> h(2 * m);
  std::size_t k = 0;
  for (std::size_t i = 0; i < m; ++i) {  // lower hull
    while (k >= 2 && orient(h[k - 2], h[k - 1], s[i]) <= 0) --k;
    h[k++] = s[i];
  }
  const std::size_t lower = k + 1;
  for (std::size_t i = m - 1; i-- > 0;) {  // upper hull
    while (k >= lower && orient(h[k - 2], h[k - 1], s[i]) <= 0) --k;
    h[k++] = s[i];
  }
  --k;  // the last point equals the first
  for (std::size_t i = 0; i < k; ++i) out[i] = h[i];
  return k;
}

std::uint64_t digest_indices(const std::uint32_t* idx, std::size_t n) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  for (std::size_t i = 0; i < n; ++i) {
    for (int b = 0; b < 4; ++b) {
      h ^= static_cast<std::uint8_t>((idx[i] >> (8 * b)) & 0xFFu);
      h *= 0x100000001b3ull;
    }
  }
  return h;
}

}  // namespace lumora::geom
