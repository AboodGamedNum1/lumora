// Convex hull of integer points in the plane (the vertices only, as indices into the caller's arrays). Pure integer: no floating point, no
// libm, no 128-bit types, no compiler builtins or intrinsics (MSVC), no allocation, no recursion, no global state. The sort must have a
// worst case of O(n log n): no quadratic method. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::hull2d {
constexpr std::uint32_t kMaxPoints = 65536;                 // largest point count
constexpr std::int32_t kCoordLimit = (1 << 30) - 1;         // every coordinate must be in [-kCoordLimit, kCoordLimit]
enum class HullResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutputTooSmall = 4, CoordinateRange = 5 };
// Points: point i is (xs[i], ys[i]) for i in 0 .. n-1. Points may repeat and may be collinear. Coordinates within kCoordLimit make every
// difference at most 2^31 - 2 in size and the cross product of two differences, a * d - b * c, smaller than 2^63 in size, so it is exact in
// int64_t. The caller must not make the arrays overlap each other, scratch or the outputs.
//
// RESULT: the vertices of the convex hull, where a vertex is a point that is not on a segment between two other distinct points, so points in
// the middle of a hull edge are NOT vertices. Order: counter-clockwise (the y axis points up for the purpose of this order), starting at the
// vertex with the smallest x, ties broken by the smallest y. Of several points with identical coordinates, the vertex reported is the one with
// the smallest index. Degenerate input: n = 0 gives 0 vertices; all points identical give 1 vertex; all points collinear (not all identical)
// give 2 vertices, the lexicographically smallest (x, then y) first and the largest second. Otherwise at least 3 vertices, each turn strictly left.
//
// *out_scratch_words = the scratch size in u32 words that hull_compute needs: n (0 for n = 0). Check order: a null out_scratch_words gives
// NullArgument; then n above kMaxPoints gives TooLarge (nothing written).
HullResult hull_scratch_words(std::uint32_t n, std::uint32_t* out_scratch_words);
// Computes the hull. On Ok, out_idx[0 .. *out_count) holds the vertex indices in the order above and *out_count their number.
// Check order: a null pointer argument (xs, ys, out_idx only while n is above 0, scratch only while n is above 0, out_count always) gives
// NullArgument; then n above kMaxPoints gives TooLarge; then scratch_words below n gives ScratchTooSmall; then out_capacity below n gives
// OutputTooSmall (the capacity is checked against n, not against the hull size, so the result of this check does not depend on the
// coordinates); then any coordinate outside [-kCoordLimit, kCoordLimit] gives CoordinateRange. On any failure out_idx and *out_count are
// untouched. The contents of scratch after the call are unspecified and are not tested. No other memory is written. Stack use: a few dozen
// words (no large local arrays).
HullResult hull_compute(const std::int32_t* xs, const std::int32_t* ys, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words,
                        std::uint32_t* out_idx, std::uint32_t out_capacity, std::uint32_t* out_count);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::hull2d
