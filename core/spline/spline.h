// Lumora 2D path module (scope H). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/spline/spline.cpp only.
// Results must be bit-identical on Linux, Windows and Android: no libm transcendental calls
// (sin, cos, atan2, pow, exp, log); only + - * / sqrt, double internally, FMA contraction off.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::spline {

struct Vec2 {
  float x, y;
};

enum class Kind : std::uint8_t { Linear = 0, CatmullRom = 1, CubicBezier = 2 };
// Linear: points are polyline vertices. CatmullRom (uniform, tension 0.5): one segment per consecutive point pair,
// end tangents by duplicating the end points (open) or wrapping (closed).
// CubicBezier: points = 1 + 3 * segments (open) or 3 * segments (closed): p0 c0 c1 p1 c2 c3 p2 ...

constexpr std::size_t kMaxPoints = 1024;

enum class PathStatus : std::uint8_t {
  Ok = 0,
  BadKind,
  TooFewPoints,    // Linear and CatmullRom need >= 2, CubicBezier >= 4 (open) or >= 3 (closed, 3 per segment)
  TooManyPoints,   // more than kMaxPoints
  BadPointCount,   // CubicBezier count not of the form above
  BadPoint,        // NaN, infinite or |coordinate| > 1e6
};
const char* path_status_text(PathStatus s);

struct Path {
  Kind kind = Kind::Linear;
  bool closed = false;
  const Vec2* points = nullptr;
  std::size_t count = 0;
};

PathStatus validate_path(const Path& p);
std::size_t segment_count(const Path& p);  // 0 for an invalid path

// Evaluate segment `seg` at u in 0..1 (clamped, NaN -> 0). Invalid input returns {0,0}.
Vec2 eval_segment(const Path& p, std::size_t seg, float u);
Vec2 tangent_segment(const Path& p, std::size_t seg, float u);  // first derivative d/du

// Arc-length table: each segment is flattened into 32 equal-u chords; lengths are summed in double.
struct LengthTable {
  static constexpr std::size_t kChords = 32;
  double cumulative[kMaxPoints * kChords + 1];  // cumulative[0] = 0
  std::size_t chords = 0;                        // segment_count * kChords
  double total = 0.0;
};
// Returns false (out untouched) on an invalid path.
bool build_length_table(const Path& p, LengthTable& out);

// Position at arc length s (clamped to 0..total, NaN -> 0), linear interpolation inside the chord.
Vec2 eval_at_length(const Path& p, const LengthTable& t, double s);
// Write n points evenly spaced by arc length from s = 0 to s = total (n == 1 gives s = 0).
// Returns false on an invalid path, n == 0 or n > 65536.
bool resample_even(const Path& p, std::size_t n, Vec2* out);

// FNV-1a 64 over the little-endian IEEE bit patterns of x then y of each point, in order, for golden tests.
std::uint64_t digest_points(const Vec2* pts, std::size_t n);

}  // namespace lumora::spline
