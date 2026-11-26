// Lumora CPU polygon rasterizer (scope O). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/raster/raster.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit intermediates where
// needed), no libm, no floats, no unordered containers, no pointer values in any decision.
// Purpose: anti-aliased 8-bit coverage masks for filled polygons (vector shapes, glyph outlines, UI shapes), the missing
// step between core/spline (curves) and the sprite batcher. Evidence label for this module: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::raster {

// Coordinates are 24.8 fixed point: 1 pixel = 256 units. Pixel (i, j) covers [i*256, (i+1)*256) x [j*256, (j+1)*256).
// x grows right, y grows down. The target origin is (0, 0).
using Fx = std::int32_t;
constexpr Fx kOne = 256;
constexpr Fx kMaxCoord = 1 << 24;  // |x| and |y| must be <= kMaxCoord (65536 pixels)

struct P {
  Fx x, y;
};

constexpr std::uint32_t kMaxSize = 8192;           // width and height of a target
constexpr std::size_t kMaxContours = 4096;
constexpr std::size_t kMaxPointsPerContour = 1u << 20;

enum class Fill : std::uint8_t { NonZero = 0, EvenOdd = 1 };

enum class RasterStatus : std::uint8_t {
  Ok = 0,
  NullBuffer,   // target data is null, or a contour pointer is null while its count is > 0, or the contour array is null while contour_count > 0
  BadSize,      // width or height is 0 or > kMaxSize, or stride < width
  BadFill,      // value of Fill outside the enum
  BadContour,   // contour_count > kMaxContours, a contour with fewer than 3 points or more than kMaxPointsPerContour
  BadPoint,     // a coordinate with |x| or |y| > kMaxCoord
};
const char* raster_status_text(RasterStatus s);

struct Contour {
  const P* pts;
  std::size_t count;
};

struct Target {
  std::uint8_t* data;  // row-major, row j starts at data + j * stride
  std::uint32_t width, height, stride;
};

// A contour_count above kMaxContours is reported as BadContour; the contour entries are then not inspected for null pointers.
// Fills the whole target (every pixel of every row, not the stride padding) with coverage 0..255. Checks in enum order
// (NullBuffer, BadSize, BadFill, BadContour, BadPoint); on any error nothing is written. contour_count == 0 is valid and
// clears the mask. Every contour is implicitly closed. Points may lie outside the target (clipped exactly).
//
// Coverage rule (the exact definition, documented in docs/raster.md; it is a definition, not an approximation of geometry):
//  1. Edge snapping. Slanted edges have rational intercepts, so every edge is first snapped to integers by this fixed rule.
//     Normalize the edge so y increases (swap the endpoints if needed and remember the orientation sign: +1 if it was
//     already downward, -1 if swapped). Horizontal edges and repeated points contribute nothing. At every row boundary
//     y = j * 256 strictly inside the edge, the x of the crossing is x0 + floor((2*n + d) / (2*d)) with n = (x1 - x0) *
//     (y - y0), d = y1 - y0 > 0 and floor division (correct for negative n): nearest integer, ties toward +infinity. The
//     original endpoints are kept. Each resulting row segment is then split at vertical pixel boundaries x = i * 256 by the
//     same rule with the roles of x and y swapped (the row segment is first normalized so x increases; ties toward
//     +infinity again). Every piece now has integer coordinates inside one pixel. The snapping depends only on the edge
//     itself, so two shapes that share an edge (in either direction) get the same pieces.
//  2. Accumulation. A2 is twice the signed area, in units of 1/131072 pixel^2, summed per pixel over all pieces and all
//     contours by the cover/area cell method (a piece contributes its exact trapezoid area times its orientation sign; every
//     value is an integer, so the sum is exact and order independent).
//  3. NonZero: v2 = min(|A2|, 131072). EvenOdd: m = |A2| mod 262144, v2 = m if m <= 131072 else 262144 - m. NonZero is
//     absolute-area saturation, not a winding number fill (this is the specified behaviour).
//  4. coverage = (v2 * 255 + 65536) >> 17.
// Clipping is applied to the snapped pieces: pieces above row 0 or below the last row are dropped; the part left of x = 0
// collapses onto x = 0 and keeps its cover; the part right of x = width * 256 is dropped. The result is the same as if the
// target were unbounded. Overlapping contours of the same direction saturate under NonZero, opposite directions cancel.
// Reversing the direction of every contour gives identical bytes. This is the accumulation rule used by common font
// rasterizers. Memory use must be documented in docs/raster.md and must not depend on the coordinate range (a row or band
// based accumulation is expected, not one cell per coordinate unit).
RasterStatus fill_polygons(const Contour* contours, std::size_t contour_count, Fill rule, const Target& target);

// Flattening of curves into points, integer only. Both append to out (capacity cap) the points AFTER p0, ending exactly at
// the last control point, and return the number written, or 0 when cap is too small or an input coordinate is out of range.
// Rule: de Casteljau subdivision with rounding (a + b + 1) >> 1 at every midpoint, to a fixed depth k chosen as the smallest
// k in 0..6 for which the deviation test holds, else 6. Test on the ORIGINAL control points, with tol (in Fx units; tol < 1
// is treated as 1): quad, with d = p0 - 2*p1 + p2: |d|^2 <= (4 * tol * 4^k)^2. Cubic, with d1 = p0 - 2*p1 + p2 and
// d2 = p1 - 2*p2 + p3: 9 * max(|d1|^2, |d2|^2) <= (4 * tol * 4^k)^2 (|.|^2 is the exact squared Euclidean norm in 64 bits;
// when the threshold reaches 2^28 the test passes without computing it). Exactly 2^k points are emitted after p0, in
// parameter order, the last one exactly the final control point. A null out, too small cap or out-of-range input returns 0
// and writes nothing. The exact deviation argument is documented in docs/raster.md.
std::size_t flatten_quad(const P& p0, const P& p1, const P& p2, Fx tol, P* out, std::size_t cap);
std::size_t flatten_cubic(const P& p0, const P& p1, const P& p2, const P& p3, Fx tol, P* out, std::size_t cap);

// A target with null data or a zero width or height hashes zero bytes (the FNV offset basis).
// FNV-1a 64 over the pixel bytes, rows top to bottom, width bytes per row (stride padding excluded), for golden tests.
std::uint64_t digest_mask(const Target& t);

}  // namespace lumora::raster
