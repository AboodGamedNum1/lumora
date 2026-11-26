// Lumora CPU Bezier flattener (scope X). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/flatten/flatten.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no floats,
// no heap allocation, no recursion, no static mutable state, no pointer values in any decision except the Overlap check.
// Purpose: turn a quadratic or cubic Bezier curve into a polyline whose points feed core/stroke and core/tess. Evidence label:
// software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::flatten {

constexpr std::int32_t kMaxCoord = 1 << 24;        // |x|, |y| of every control point
constexpr std::int32_t kMaxTolerance = 1 << 16;    // tolerance in 1 .. kMaxTolerance, same units as the coordinates
constexpr std::uint32_t kMaxSegments = 256;        // segments per curve, so at most kMaxSegments + 1 output points
constexpr std::uint32_t kMaxOutputPoints = kMaxSegments + 1;

struct Point {
  std::int32_t x, y;
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,      // a required pointer is null (all pointers are always required)
  BadTolerance,    // tolerance outside 1 .. kMaxTolerance
  BadPoint,        // a control point coordinate beyond kMaxCoord in magnitude
  BufferTooSmall,  // out_capacity < kMaxOutputPoints (data independent)
  Overlap          // touched byte ranges intersect (modular address rule as in core/composite)
};
// Lower-case spaced enum names ("ok", "null buffer", "bad tolerance", "bad point", "buffer too small", "overlap"), "unknown"
// otherwise.
const char* status_text(Status s);

// Number of equal-parameter segments n in 1 .. kMaxSegments used for a curve. Exposed so it can be tested directly.
// Quadratic (p0, c, p1): M = |p0 - 2c + p1| (Euclidean, of the exact integer vector). The deviation of the chord polyline with n
//   equal-parameter segments from the curve is at most M / (4 n^2). n is the smallest integer in 1 .. kMaxSegments with
//   M^2 <= 16 * tol^2 * n^4 (exact integer comparison; M^2 is the exact squared norm), else kMaxSegments.
// Cubic (p0, c0, c1, p1): M = max(|p0 - 2 c0 + c1|, |c0 - 2 c1 + p1|). The deviation is at most 3 M / (4 n^2). n is the smallest
//   integer in 1 .. kMaxSegments with 9 * M^2 <= 16 * tol^2 * n^4, else kMaxSegments.
// When kMaxSegments is returned because the tolerance is not reachable, the deviation bound does not hold; the contributor states
// this and the largest curve and smallest tolerance for which it does in docs/flatten.md. Invalid input gives 0.
std::uint32_t quad_segments(Point p0, Point c, Point p1, std::int32_t tolerance);
std::uint32_t cubic_segments(Point p0, Point c0, Point c1, Point p1, std::int32_t tolerance);

// Flattens the curve. With n = the segment count above, output point i (i = 0 .. n) is the Bernstein form at t = i / n evaluated
// exactly in integers and rounded to the nearest integer per coordinate, ties away from zero:
//   quadratic: ((n - i)^2 p0 + 2 (n - i) i c + i^2 p1) / n^2
//   cubic:     ((n - i)^3 p0 + 3 (n - i)^2 i c0 + 3 (n - i) i^2 c1 + i^3 p1) / n^3
// (numerators are at most 2^24 * n^3 <= 2^48 in magnitude, so they fit int64; the contributor states the bound). The first output
// point is exactly p0 and the last exactly p1. *out_count receives n + 1.
// Checks in this order: NullBuffer (out and out_count; all pointers are always required), BadTolerance, BadPoint,
// BufferTooSmall (out_capacity < kMaxOutputPoints), Overlap (out over kMaxOutputPoints points against out_count). Nothing is written
// on error; the checks are data independent.
Status flatten_quad(Point p0, Point c, Point p1, std::int32_t tolerance, Point* out, std::uint32_t out_capacity,
                    std::uint32_t* out_count);
Status flatten_cubic(Point p0, Point c0, Point c1, Point p1, std::int32_t tolerance, Point* out, std::uint32_t out_capacity,
                     std::uint32_t* out_count);

// FNV-1a 64 over the 32-bit little-endian bytes of x then y of each point (uint32 two's complement words). Null or zero count
// hashes zero bytes.
std::uint64_t digest_points(const Point* pts, std::size_t count);

}  // namespace lumora::flatten
