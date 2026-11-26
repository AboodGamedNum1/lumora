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

// Fills the whole target (every pixel of every row, not the stride padding) with coverage 0..255. Checks in enum order
// (NullBuffer, BadSize, BadFill, BadContour, BadPoint); on any error nothing is written. contour_count == 0 is valid and
// clears the mask. Every contour is implicitly closed. Points may lie outside the target (clipped exactly).
//
// Coverage rule (the exact definition, documented in docs/raster.md; it is a definition, not an approximation of geometry):
//  1. For each pixel, A is the signed area, in units of 1/65536 pixel^2 (that is, 256 x 256 sub-units), accumulated over all
//     contours by the cover/area cell method: each edge contributes exactly (integer arithmetic, no rounding) to the
//     cells it crosses. Edge direction gives the sign. The sum is exact; the contributions of the edges to one pixel
//     are added in contour order, then point order (the sum is order independent because it is exact integers).
//  2. NonZero: v = min(|A|, 65536). EvenOdd: m = |A| mod 131072, v = m if m <= 65536 else 131072 - m.
//  3. coverage = (v * 255 + 32768) >> 16.
// Overlapping contours of the same direction therefore saturate under NonZero, and opposite directions cancel. This is the
// accumulation rule used by common font rasterizers; it is the specified behaviour of this module.
RasterStatus fill_polygons(const Contour* contours, std::size_t contour_count, Fill rule, const Target& target);

// Flattening of curves into points, integer only. Both append to out (capacity cap) the points AFTER p0, ending exactly at
// the last control point, and return the number written, or 0 when cap is too small or an input coordinate is out of range.
// Rule: de Casteljau subdivision with rounding (a + b + 1) >> 1 at every midpoint, to a fixed depth k chosen as the smallest
// k in 0..6 such that the control polygon deviation test in docs/raster.md holds with tolerance tol (in Fx units, tol >= 1;
// tol < 1 is treated as 1). Segments are emitted left to right (in parameter order).
std::size_t flatten_quad(const P& p0, const P& p1, const P& p2, Fx tol, P* out, std::size_t cap);
std::size_t flatten_cubic(const P& p0, const P& p1, const P& p2, const P& p3, Fx tol, P* out, std::size_t cap);

// FNV-1a 64 over the pixel bytes, rows top to bottom, width bytes per row (stride padding excluded), for golden tests.
std::uint64_t digest_mask(const Target& t);

}  // namespace lumora::raster
