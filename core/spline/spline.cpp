// 2D path module. Formulas, clamp and NaN rules, endpoint behavior and golden digests: docs/spline.md.
// Determinism: only + - * / and sqrt (an exact IEEE operation) are used, in double. No libm transcendental function is
// called. Compilers are asked not to fuse multiply-add; the project must not enable fast-math.
#include "spline.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::spline {
namespace {

constexpr double kMaxCoord = 1e6;

struct D2 {
  double x, y;
};

D2 at(const Vec2& v) { return {static_cast<double>(v.x), static_cast<double>(v.y)}; }

bool good_coord(float v) { return std::isfinite(v) && std::fabs(static_cast<double>(v)) <= kMaxCoord; }

double clamp_u(float u) {
  if (!(u > 0.0f)) return 0.0;  // also NaN
  return u > 1.0f ? 1.0 : static_cast<double>(u);
}

// Point index helper: open paths clamp (end points duplicated), closed paths wrap.
std::size_t catmull_index(const Path& p, long long i) {
  const long long n = static_cast<long long>(p.count);
  if (p.closed) {
    long long m = i % n;
    if (m < 0) m += n;
    return static_cast<std::size_t>(m);
  }
  if (i < 0) return 0;
  if (i >= n) return static_cast<std::size_t>(n - 1);
  return static_cast<std::size_t>(i);
}

std::size_t bezier_index(const Path& p, std::size_t seg, std::size_t k) {
  const std::size_t i = 3 * seg + k;
  return p.closed ? i % p.count : i;
}

D2 eval_d(const Path& p, std::size_t seg, double u) {
  switch (p.kind) {
    case Kind::Linear: {
      const D2 a = at(p.points[seg]);
      const D2 b = at(p.points[p.closed ? (seg + 1) % p.count : seg + 1]);
      return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u};
    }
    case Kind::CatmullRom: {
      const long long s = static_cast<long long>(seg);
      const D2 p0 = at(p.points[catmull_index(p, s - 1)]);
      const D2 p1 = at(p.points[catmull_index(p, s)]);
      const D2 p2 = at(p.points[catmull_index(p, s + 1)]);
      const D2 p3 = at(p.points[catmull_index(p, s + 2)]);
      const double u2 = u * u, u3 = u2 * u;
      auto f = [&](double a, double b, double c, double d) {
        return 0.5 * ((2.0 * b) + (c - a) * u + (2.0 * a - 5.0 * b + 4.0 * c - d) * u2 + (3.0 * b - a - 3.0 * c + d) * u3);
      };
      return {f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)};
    }
    case Kind::CubicBezier: {
      const D2 a = at(p.points[bezier_index(p, seg, 0)]);
      const D2 b = at(p.points[bezier_index(p, seg, 1)]);
      const D2 c = at(p.points[bezier_index(p, seg, 2)]);
      const D2 d = at(p.points[bezier_index(p, seg, 3)]);
      const double v = 1.0 - u;
      const double w0 = v * v * v, w1 = 3.0 * v * v * u, w2 = 3.0 * v * u * u, w3 = u * u * u;
      return {w0 * a.x + w1 * b.x + w2 * c.x + w3 * d.x, w0 * a.y + w1 * b.y + w2 * c.y + w3 * d.y};
    }
  }
  return {0.0, 0.0};
}

D2 tangent_d(const Path& p, std::size_t seg, double u) {
  switch (p.kind) {
    case Kind::Linear: {
      const D2 a = at(p.points[seg]);
      const D2 b = at(p.points[p.closed ? (seg + 1) % p.count : seg + 1]);
      return {b.x - a.x, b.y - a.y};
    }
    case Kind::CatmullRom: {
      const long long s = static_cast<long long>(seg);
      const D2 p0 = at(p.points[catmull_index(p, s - 1)]);
      const D2 p1 = at(p.points[catmull_index(p, s)]);
      const D2 p2 = at(p.points[catmull_index(p, s + 1)]);
      const D2 p3 = at(p.points[catmull_index(p, s + 2)]);
      const double u2 = u * u;
      auto f = [&](double a, double b, double c, double d) {
        return 0.5 * ((c - a) + 2.0 * (2.0 * a - 5.0 * b + 4.0 * c - d) * u + 3.0 * (3.0 * b - a - 3.0 * c + d) * u2);
      };
      return {f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)};
    }
    case Kind::CubicBezier: {
      const D2 a = at(p.points[bezier_index(p, seg, 0)]);
      const D2 b = at(p.points[bezier_index(p, seg, 1)]);
      const D2 c = at(p.points[bezier_index(p, seg, 2)]);
      const D2 d = at(p.points[bezier_index(p, seg, 3)]);
      const double v = 1.0 - u;
      const double w0 = 3.0 * v * v, w1 = 6.0 * v * u, w2 = 3.0 * u * u;
      return {w0 * (b.x - a.x) + w1 * (c.x - b.x) + w2 * (d.x - c.x),
              w0 * (b.y - a.y) + w1 * (c.y - b.y) + w2 * (d.y - c.y)};
    }
  }
  return {0.0, 0.0};
}

Vec2 to_vec(const D2& v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }

}  // namespace

const char* path_status_text(PathStatus s) {
  switch (s) {
    case PathStatus::Ok: return "ok";
    case PathStatus::BadKind: return "bad path kind";
    case PathStatus::TooFewPoints: return "too few points";
    case PathStatus::TooManyPoints: return "too many points";
    case PathStatus::BadPointCount: return "bad point count for this kind";
    case PathStatus::BadPoint: return "bad point";
  }
  return "unknown status";
}

PathStatus validate_path(const Path& p) {
  const std::uint8_t k = static_cast<std::uint8_t>(p.kind);
  if (k > static_cast<std::uint8_t>(Kind::CubicBezier)) return PathStatus::BadKind;
  std::size_t min_points = 2;
  if (p.kind == Kind::CubicBezier) min_points = p.closed ? 3 : 4;
  if (p.count < min_points) return PathStatus::TooFewPoints;
  if (p.count > kMaxPoints) return PathStatus::TooManyPoints;
  if (p.kind == Kind::CubicBezier) {
    const bool shape_ok = p.closed ? (p.count % 3 == 0) : ((p.count - 1) % 3 == 0);
    if (!shape_ok) return PathStatus::BadPointCount;
  }
  if (p.points == nullptr) return PathStatus::BadPoint;
  for (std::size_t i = 0; i < p.count; ++i) {
    if (!good_coord(p.points[i].x) || !good_coord(p.points[i].y)) return PathStatus::BadPoint;
  }
  return PathStatus::Ok;
}

std::size_t segment_count(const Path& p) {
  if (validate_path(p) != PathStatus::Ok) return 0;
  if (p.kind == Kind::CubicBezier) return p.closed ? p.count / 3 : (p.count - 1) / 3;
  return p.closed ? p.count : p.count - 1;
}

Vec2 eval_segment(const Path& p, std::size_t seg, float u) {
  if (seg >= segment_count(p)) return {0.0f, 0.0f};
  return to_vec(eval_d(p, seg, clamp_u(u)));
}

Vec2 tangent_segment(const Path& p, std::size_t seg, float u) {
  if (seg >= segment_count(p)) return {0.0f, 0.0f};
  return to_vec(tangent_d(p, seg, clamp_u(u)));
}

bool build_length_table(const Path& p, LengthTable& out) {
  const std::size_t segs = segment_count(p);
  if (segs == 0) return false;
  constexpr std::size_t kC = LengthTable::kChords;
  out.cumulative[0] = 0.0;
  double sum = 0.0;
  std::size_t idx = 0;
  for (std::size_t s = 0; s < segs; ++s) {
    D2 prev = eval_d(p, s, 0.0);
    for (std::size_t k = 1; k <= kC; ++k) {
      const double u = k == kC ? 1.0 : static_cast<double>(k) / static_cast<double>(kC);
      const D2 cur = eval_d(p, s, u);
      const double dx = cur.x - prev.x, dy = cur.y - prev.y;
      sum += std::sqrt(dx * dx + dy * dy);
      out.cumulative[++idx] = sum;
      prev = cur;
    }
  }
  out.chords = segs * kC;
  out.total = sum;
  return true;
}

Vec2 eval_at_length(const Path& p, const LengthTable& t, double s) {
  constexpr std::size_t kC = LengthTable::kChords;
  const std::size_t segs = segment_count(p);
  if (segs == 0 || t.chords > kMaxPoints * kC || t.chords != segs * kC) return {0.0f, 0.0f};
  if (!(s > 0.0)) s = 0.0;  // also NaN
  if (s > t.total) s = t.total;
  // First chord whose end is beyond s; zero-length chords are skipped.
  const double* first = t.cumulative + 1;
  const std::size_t j = static_cast<std::size_t>(std::upper_bound(first, first + t.chords, s) - first);
  if (j >= t.chords) return to_vec(eval_d(p, segs - 1, 1.0));
  const std::size_t seg = j / kC, k = j % kC;
  const double c0 = t.cumulative[j], c1 = t.cumulative[j + 1];
  const double frac = (s - c0) / (c1 - c0);  // c1 > s >= c0, so the divisor is positive
  const double u0 = static_cast<double>(k) / static_cast<double>(kC);
  const double u1 = k + 1 == kC ? 1.0 : static_cast<double>(k + 1) / static_cast<double>(kC);
  const D2 a = eval_d(p, seg, u0), b = eval_d(p, seg, u1);
  return to_vec({a.x + (b.x - a.x) * frac, a.y + (b.y - a.y) * frac});
}

bool resample_even(const Path& p, std::size_t n, Vec2* out) {
  if (n == 0 || n > 65536 || out == nullptr || validate_path(p) != PathStatus::Ok) return false;
  std::unique_ptr<LengthTable> table(new LengthTable());
  if (!build_length_table(p, *table)) return false;
  for (std::size_t i = 0; i < n; ++i) {
    double s = 0.0;
    if (n > 1) s = i + 1 == n ? table->total : table->total * static_cast<double>(i) / static_cast<double>(n - 1);
    out[i] = eval_at_length(p, *table, s);
  }
  return true;
}

std::uint64_t digest_points(const Vec2* pts, std::size_t n) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  auto mix = [&h](float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    for (int i = 0; i < 4; ++i) {
      h ^= static_cast<std::uint8_t>((b >> (8 * i)) & 0xFFu);
      h *= 0x100000001b3ull;
    }
  };
  for (std::size_t i = 0; i < n; ++i) {
    mix(pts[i].x);
    mix(pts[i].y);
  }
  return h;
}

}  // namespace lumora::spline
