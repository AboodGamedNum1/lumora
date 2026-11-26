// Lumora CPU stroker (scope W). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/stroke/stroke.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no floats,
// no heap allocation, no recursion, no static mutable state, no pointer values in any decision except the Overlap check.
// Purpose: turn a polyline or polygon outline into a set of convex counter-clockwise contours that core/raster fill_polygons
// fills with the NonZero rule (overlaps union correctly because every contour has the same winding). Evidence label: software
// (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::stroke {

constexpr std::uint32_t kMaxPoints = 1024;           // input points
constexpr std::int32_t kMaxCoord = 1 << 28;          // |x|, |y| of every input point
constexpr std::int32_t kMaxHalfWidth = 1 << 19;      // half_width in 1 .. kMaxHalfWidth (same units as the coordinates)
constexpr std::int32_t kMaxMiterLimit = 64;          // miter_limit in 1 .. kMaxMiterLimit (ratio of miter length to half width)

struct Point {
  std::int32_t x, y;
};

enum class Join : std::uint8_t { Bevel = 0, Miter = 1 };
enum class Cap : std::uint8_t { Butt = 0, Square = 1 };

struct Style {
  std::int32_t half_width;   // 1 .. kMaxHalfWidth
  std::int32_t miter_limit;  // used by Join::Miter only, 1 .. kMaxMiterLimit; checked always
  Join join;
  Cap cap;
  bool closed;               // closed: the last point connects to the first, no caps, a join at every point
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,      // a required pointer is null (all pointers are always required)
  BadCount,        // point_count < 2 or > kMaxPoints
  BadStyle,        // half_width, miter_limit, join or cap out of range
  BadPoint,        // a coordinate beyond kMaxCoord in magnitude
  BufferTooSmall,  // an output capacity is below contour_bound or point_bound for this point_count (data independent)
  Overlap          // touched byte ranges intersect (modular address rule as in core/composite)
};
// Lower-case spaced enum names ("ok", "null buffer", "bad count", "bad style", "bad point", "buffer too small", "overlap"),
// "unknown" otherwise.
const char* status_text(Status s);

// Capacity bounds, data independent (0 for an invalid point_count). Per segment one quad (4 points); per join one triangle (bevel)
// or one quad (miter: p, a, tip, b), 4 points; per cap of an open line one quad (square cap) or nothing; so the bounds are exact
// upper bounds, not exact counts: with S = number of segments (point_count - 1 open, point_count closed), J = number of joins
// (point_count - 2 open, point_count closed) and C = 2 caps (open only):
//   contour_bound = S + J + C,  point_bound = 4 * (S + J + C).
// Open: contour_bound = 2 * point_count - 3 + 2 = 2 * point_count - 1; closed: 2 * point_count. The contributor states this in docs.
std::uint32_t contour_bound(std::uint32_t point_count, bool closed);
std::uint32_t point_bound(std::uint32_t point_count, bool closed);

// Integer square root used for lengths: for d2 >= 1 the NEAREST integer L to the real square root of d2 (no ties exist), L >= 1.
// d2 up to 2^60. Exposed so it can be tested directly. Returns 0 for d2 == 0.
std::uint32_t isqrt_nearest(std::uint64_t d2);

// Strokes the polyline. Points are first de-duplicated: consecutive equal points collapse to one (for a closed line also the last
// against the first); the following rules apply to the collapsed list of k points. If k < 2 (all points equal): Cap::Butt gives no
// contours; Cap::Square gives one axis aligned square of side 2 * half_width centred on the point (counter-clockwise starting at
// the lower left corner), also when closed.
//
// Offset vector of a segment from p to q (dx, dy, d2 = dx * dx + dy * dy, L = isqrt_nearest(d2)): left normal
// n = (round(-dy * h / L), round(dx * h / L)) with h = half_width and round = nearest, ties away from zero, using exact integer
// arithmetic (|dy * h| <= 2^48). The "left" side is the counter-clockwise side.
//
// Contours, in this order, each as a counter-clockwise convex polygon with its points listed starting at the first listed:
//  1. For each segment i in order (closed: also the closing segment last): the quad (p - n, q - n, q + n, p + n). Check that this
//     winding is counter-clockwise for the left normal convention above; the contributor proves it in docs/stroke.md.
//  2. For each join at point p between the segment before (normal n1) and the segment after (normal n2), joins in point order
//     (open: points 1 .. k-2; closed: points 0 .. k-1 with the closing segment as the one before point 0): only when the turn is not
//     zero. cross = dx1 * dy2 - dy1 * dx2 of the two directions (exact int64); a zero cross (straight or a full reversal) adds no join
//     contour. The outer side is the right side when cross > 0 (left turn) and the left side when cross < 0. Let a, b be p + o1 and
//     p + o2 where o1, o2 are the outer-side offsets (-n1 and -n2 when cross > 0, n1 and n2 when cross < 0).
//       Bevel: the triangle (p, a, b), reordered so the winding is counter-clockwise.
//       Miter: m = o1 + o2 (int64), the tip t = p + round(m * 2 * h * h / (m.x * m.x + m.y * m.y)) per coordinate (nearest, ties away
//       from zero; m is not zero because cross != 0). If |t - p|^2 > (miter_limit * h)^2 the join is a Bevel instead. Otherwise the
//       quad (p, a, t, b), reordered counter-clockwise. The bounds: |m| <= 2^21, h <= 2^19, so m * 2 * h * h <= 2^59 fits int64.
//  3. Caps of an open line, Cap::Square only: at the first point the quad (p0 - n, p0 + n, p0 + n - d, p0 - n - d) and at the last
//     point (pe + n, pe - n, pe - n + d, pe + n + d) where n is the normal of the first (resp. last) segment and d is the segment
//     direction offset vector (n rotated a quarter turn back, i.e. d = (n.y, -n.x) for the normal n = (-dy, dx) scaled), each listed
//     starting at its first written point and made counter-clockwise if the listed order is not. Butt caps add nothing.
// Output: points of all contours in order into pts; contour_ends[i] is the end index (exclusive) of contour i in pts. *contour_count
// and *point_count_out receive the counts. Every output coordinate must fit int32: |p| <= 2^28 plus offsets of at most 2^20 and
// miter tips bounded by miter_limit * h <= 2^25, so the sums stay below 2^29, which fits (document).
//
// Checks in this order: NullBuffer (all pointers), BadCount, BadStyle, BadPoint, BufferTooSmall (pts_capacity < point_bound,
// ends_capacity < contour_bound), Overlap (every pair of: in (point_count points), pts (point_bound points), contour_ends
// (contour_bound words), *contour_count and *point_count_out). Nothing is written on error; the checks are data independent.
Status stroke_polyline(const Point* in, std::uint32_t point_count, const Style& style, Point* pts, std::uint32_t pts_capacity,
                       std::uint32_t* contour_ends, std::uint32_t ends_capacity, std::uint32_t* contour_count,
                       std::uint32_t* point_count_out);

// FNV-1a 64 over the 32-bit little-endian bytes of x then y of each point (uint32 two's complement words). Null or zero count
// hashes zero bytes.
std::uint64_t digest_points(const Point* pts, std::size_t count);

}  // namespace lumora::stroke
