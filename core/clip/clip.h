// Lumora CPU clipping against an axis-aligned rectangle (scope V). Pure CPU, deterministic, no I/O, no threads, no floating point.
// The interface in this header is fixed; the contributor implements core/clip/clip.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no floats,
// no heap allocation, no recursion, no static mutable state, no pointer values in any decision except the Overlap check.
// Purpose: scissor and viewport clipping of lines, polylines and polygons for the 2D renderer. Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::clip {

constexpr std::uint32_t kMaxPoints = 4096;       // input points of a polygon or polyline
constexpr std::int32_t kMaxCoord = 1 << 28;      // |x| and |y| of every point and rectangle edge

struct Point {
  std::int32_t x, y;
};

// Closed rectangle: x0 <= x <= x1 and y0 <= y <= y1. A rectangle with x0 > x1 or y0 > y1 is invalid.
struct Rect {
  std::int32_t x0, y0, x1, y1;
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,      // a required pointer is null (arrays when their count > 0; result pointers always)
  BadCount,        // point_count > kMaxPoints (a polygon needs at least 3 points, a polyline at least 2)
  BadRect,         // invalid rectangle or an edge beyond kMaxCoord in magnitude
  BadPoint,        // a point coordinate beyond kMaxCoord in magnitude
  BufferTooSmall,  // an output capacity is smaller than the proven bound (see each function)
  Overlap          // touched byte ranges intersect (inputs and outputs; modular address rule as in core/composite)
};
// Lower-case spaced enum names ("ok", "null buffer", "bad count", "bad rect", "bad point", "buffer too small", "overlap"),
// "unknown" otherwise.
const char* status_text(Status s);

// Rounding rule for every computed intersection coordinate, fixed here: a coordinate is x = a + (d * t) / m with integers where
// m > 0; the result is rounded to the NEAREST integer, ties toward positive infinity (floor((2 * d * t + m) / (2 * m)) added to a,
// using floor division, never C++ truncating division on negative values). The numerator 2 * d * t + m must be bounded and
// shown to fit int64 in docs/clip.md (|d| <= 2^29, |t| <= 2^29, m <= 2^29).

// Liang-Barsky style clip of the closed segment p0 p1 to the closed rectangle. The part of the segment inside the rectangle is
// computed exactly (rational parameters, no float); the visible endpoints are rounded by the rule above, each coordinate
// independently, and then clamped into the rectangle. *visible is false when no point of the segment lies in the rectangle (the
// outputs are then untouched). A segment that touches the rectangle in one point is visible with a == b. Checks: NullBuffer,
// BadRect, BadPoint. Writes outputs only on Ok and visible.
Status clip_segment(const Rect& r, const Point& p0, const Point& p1, Point* a, Point* b, bool* visible);

// Sutherland-Hodgman clip of a polygon (any point order, not validated) against the rectangle, edge order fixed: left (x >= x0),
// bottom (y >= y0), right (x <= x1), top (y <= y1). Walk the ring: for each edge from the previous point to the current point
// (the first edge is from the LAST point to the first), output rules are the textbook ones: both inside: output current; leaving:
// output the intersection; entering: output the intersection then current; both outside: nothing. "Inside" is closed (a point on
// the clip line is inside). Intersections use the rounding rule above, computed on the exact input edge of that stage (the stage input
// points are already integers). Output capacity bound: every stage can add at most one point per input edge, so the final count is
// at most 2 * point_count (document the proof); out_capacity < 2 * point_count is BufferTooSmall even if fewer are produced.
// *out_count is 0 when nothing remains. Checks: NullBuffer, BadCount (point_count < 3 or > kMaxPoints), BadRect, BadPoint,
// BufferTooSmall, Overlap (in, out, scratch of 2 * 2 * point_count points: scratch_size, and *out_count), in this order.
// scratch must hold scratch_size(point_count) points (two ping-pong buffers).
std::uint32_t scratch_size(std::uint32_t point_count);  // 4 * point_count, 0 for an invalid count
Status clip_polygon(const Rect& r, const Point* in, std::uint32_t point_count, Point* out, std::uint32_t out_capacity, Point* scratch,
                    std::uint32_t scratch_count, std::uint32_t* out_count);

// Clips an open polyline segment by segment with clip_segment's rule and writes the visible pieces: each piece is a run of
// consecutive visible segments whose clipped joints coincide exactly, as the points of the run in order (a run restarts when a
// segment is not visible or when its clipped start differs from the previous clipped end). Output: pts (all piece points in order)
// and piece_ends (for each piece the end index into pts, exclusive). Bounds: at most 2 * (point_count - 1) points and at most
// point_count - 1 pieces; capacities below those bounds are BufferTooSmall. *piece_count and *out_points receive the counts.
// Checks: NullBuffer, BadCount (< 2 or > kMaxPoints), BadRect, BadPoint, BufferTooSmall, Overlap (all inputs and outputs and the
// two counters), in this order.
Status clip_polyline(const Rect& r, const Point* in, std::uint32_t point_count, Point* pts, std::uint32_t pts_capacity,
                     std::uint32_t* piece_ends, std::uint32_t piece_capacity, std::uint32_t* piece_count, std::uint32_t* out_points);

// FNV-1a 64 over the 32-bit little-endian bytes of the x then y of each point as uint32 two's complement words.
std::uint64_t digest_points(const Point* pts, std::size_t count);

}  // namespace lumora::clip
