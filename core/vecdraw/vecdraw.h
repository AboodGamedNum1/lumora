// Lumora CPU vector path drawing (scope Z). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/vecdraw/vecdraw.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only, no libm, no floats, no heap allocation, no
// recursion, no static mutable state, no pointer values in any decision except the Overlap check.
// Purpose: the first end-to-end vector pipeline. A path (core/outline commands) is flattened (core/outline over core/flatten),
// optionally stroked (core/stroke) and filled into an 8-bit anti-aliased coverage mask with core/raster fill_polygons. The module
// only composes those modules; it contains no flattening, stroking or rasterising logic of its own. Evidence label: software
// (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

#include "outline.h"
#include "raster.h"
#include "stroke.h"

namespace lumora::vecdraw {

// All coordinates are raster 24.8 fixed point (1 pixel = 256 units, |value| <= raster::kMaxCoord = 2^24, which equals
// flatten::kMaxCoord and is below stroke::kMaxCoord). The tolerance and the half width are in the same units.
constexpr std::uint32_t kMaxCommands = 64;  // commands per path (the outline limit is 1024; this module bounds its work memory)
constexpr std::int32_t kMaxTolerance = 1 << 16;
constexpr std::int32_t kMaxHalfWidth = 1 << 19;  // equals stroke::kMaxHalfWidth

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,        // a required pointer is null (commands, target.data, work; all are always required)
  BadSize,           // target width or height is 0 or above raster::kMaxSize, or stride < width
  BadCount,          // command_count is 0 or above kMaxCommands
  BadTolerance,      // tolerance outside 1 .. kMaxTolerance
  BadFill,           // fill rule outside raster::Fill
  BadStyle,          // stroke style out of range (half_width, miter_limit, join, cap; the closed field is ignored and replaced per contour)
  BadCommand,        // an op outside Move .. Close
  BadPoint,          // a used coordinate beyond raster::kMaxCoord in magnitude
  BadPath,           // the outline sequence rule is broken
  WorkTooSmall,      // work_bytes is below work_bytes_needed(command_count, stroke), or work is misaligned (not a multiple of 8)
  Overlap,           // touched byte ranges intersect (modular address rule as in core/composite)
  TooManyPoints,     // stroke: a flattened contour has more than stroke::kMaxPoints points (data dependent, target untouched)
  TooManyContours    // stroke: the stroke outline needs more than raster::kMaxContours contours (data dependent, target untouched)
};
// Lower-case spaced names ("ok", "null buffer", "bad size", "bad count", "bad tolerance", "bad fill", "bad style", "bad command",
// "bad point", "bad path", "work too small", "overlap", "too many points", "too many contours"), "unknown" otherwise.
const char* status_text(Status s);

// Bytes of working memory (data independent, 0 for an invalid command_count). The contributor derives the exact bound in
// docs/vecdraw.md from the outline bounds (contour_bound, point_bound) and, for stroke, the stroke bounds
// (contour_bound(n) = 2n and point_bound = 8n per closed polyline of n points; sum over contours, using that the contour point
// counts add up to at most the outline point_bound), plus the raster contour table. The memory is caller provided and 8-byte aligned.
// All carving is by integer offsets; no pointer value influences a decision.
std::size_t work_bytes_needed(std::uint32_t command_count, bool stroke);

// Fills the path: every contour (open ones implicitly closed) with the given rule; contours with fewer than 3 points are skipped.
// The mask is fully written (every pixel of every row, not the stride padding), exactly as raster::fill_polygons does for the same
// contours in contour order. Checks in this order: NullBuffer, BadSize, BadCount, BadTolerance, BadFill, BadCommand, BadPoint,
// BadPath, WorkTooSmall, Overlap (commands, target rows exactly stride * (height - 1) + width bytes, work work_bytes), then the work.
// The target is untouched on every error.
Status fill_path(const outline::Command* commands, std::uint32_t command_count, std::int32_t tolerance, raster::Fill rule,
                 const raster::Target& target, void* work, std::size_t work_bytes);

// Strokes the path: every contour (open or closed as the outline says) is stroked with the style (the style.closed field is ignored,
// the contour decides), all stroke contours are filled with raster::Fill::NonZero. Contours are processed in order. A contour of
// 2 or more points but whose stroke call reports a status other than Ok cannot happen after the checks (the contributor proves it
// in docs/vecdraw.md; the defensive branch maps to the same status as the matching check). The same check order as fill_path with
// BadStyle after BadTolerance and the mask written only on success (TooManyPoints and TooManyContours are found before the
// rasteriser runs).
Status stroke_path(const outline::Command* commands, std::uint32_t command_count, std::int32_t tolerance,
                   const stroke::Style& style, const raster::Target& target, void* work, std::size_t work_bytes);

}  // namespace lumora::vecdraw
