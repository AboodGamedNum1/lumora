// Lumora CPU outline builder (scope Y). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/outline/outline.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no floats,
// no heap allocation, no recursion, no static mutable state, no pointer values in any decision except the Overlap check.
// Purpose: turn a vector path (move, line, quadratic, cubic, close) into flat contours (polylines with a closed flag) that feed
// core/stroke and core/tess and core/raster. Curves are flattened with core/flatten (the contributor calls flatten_quad and
// flatten_cubic and does not copy their logic). Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

#include "flatten.h"

namespace lumora::outline {

using Point = lumora::flatten::Point;

constexpr std::uint32_t kMaxCommands = 1024;
constexpr std::uint32_t kPointsPerCommand = lumora::flatten::kMaxSegments;  // output points per command, upper bound

enum class Op : std::uint8_t { Move = 0, Line = 1, Quad = 2, Cubic = 3, Close = 4 };

// Points used per op: Move uses a; Line uses a; Quad uses a (control) and b (end); Cubic uses a, b (controls) and c (end); Close uses
// none. Unused fields are ignored: never validated, never read for a decision.
struct Command {
  Op op;
  Point a, b, c;
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,      // a required pointer is null (all pointers are always required)
  BadCount,        // command_count is 0 or above kMaxCommands
  BadTolerance,    // tolerance outside 1 .. lumora::flatten::kMaxTolerance
  BadCommand,      // an op value outside Move .. Close
  BadPoint,        // a USED coordinate beyond lumora::flatten::kMaxCoord in magnitude
  BadPath,         // sequence rule broken, see below
  BufferTooSmall,  // an output capacity is below its bound (data independent)
  Overlap          // touched byte ranges intersect (modular address rule as in core/composite)
};
// Lower-case spaced names ("ok", "null buffer", "bad count", "bad tolerance", "bad command", "bad point", "bad path",
// "buffer too small", "overlap"), "unknown" otherwise.
const char* status_text(Status s);

// Data independent capacity bounds (0 for an invalid command_count): contour_bound = command_count (every contour starts with a
// command), point_bound = command_count * kPointsPerCommand (a move or line adds at most 1 point, a curve at most 256).
std::uint32_t contour_bound(std::uint32_t command_count);
std::uint32_t point_bound(std::uint32_t command_count);

// Path rules (BadPath otherwise): the first command is Move; after Close only Move or the end may follow; Line, Quad and Cubic need
// a current point (so they follow a Move, Line, Quad or Cubic). A Move starts a new subpath at its point. Close ends the
// current subpath as closed; the current point is then undefined until the next Move. A subpath that ends without Close is open.
//
// Flattening: a Line adds its end point; a Quad or Cubic is flattened with the given tolerance (flatten_quad / flatten_cubic from the
// current point) and adds all flattened points after the first. While building a contour, a point equal to the previous point of the
// contour is not added (consecutive duplicates collapse). A closed contour whose last point equals its first drops that last point.
// A contour is emitted only if it has at least 2 points after this; others (a lone Move, Move then Close, all points equal) are
// dropped without error. Contours are emitted in subpath order.
//
// Output: points of all contours in order into pts; contour_ends[i] the end index (exclusive) of contour i in pts; closed[i] 1 for a
// closed contour and 0 for an open one. *contour_count and *point_count_out receive the counts.
// Checks in this order: NullBuffer (all pointers), BadCount, BadTolerance, BadCommand (all commands in order, then), BadPoint (used
// points in command order), BadPath (first violation in command order), BufferTooSmall (pts_capacity < point_bound,
// ends_capacity < contour_bound, closed_capacity < contour_bound), Overlap (every pair of: commands (command_count Commands),
// pts (point_bound points), contour_ends (contour_bound words), closed (contour_bound bytes), *contour_count and *point_count_out).
// Nothing is written on error; the checks are data independent except that BadPath depends on the sequence of ops only.
Status build_outline(const Command* commands, std::uint32_t command_count, std::int32_t tolerance, Point* pts,
                     std::uint32_t pts_capacity, std::uint32_t* contour_ends, std::uint32_t ends_capacity, std::uint8_t* closed,
                     std::uint32_t closed_capacity, std::uint32_t* contour_count, std::uint32_t* point_count_out);

// FNV-1a 64 over 32-bit little-endian words: the contour count, then per contour its end index, its closed flag (0 or 1), and then
// all points x then y as uint32 two's complement words. Null pointers or zero counts hash only the words that exist.
std::uint64_t digest_outline(const Point* pts, const std::uint32_t* contour_ends, const std::uint8_t* closed,
                             std::uint32_t contour_count);

}  // namespace lumora::outline
