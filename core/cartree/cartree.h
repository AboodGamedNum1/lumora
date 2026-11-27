// Cartesian tree of a u32 array (min-heap on the values, in-order equals the array order), built in linear time.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no scratch memory beyond the three caller output arrays (they may be used as working space while building, but their final contents
// are exactly as specified below). The work is O(n) steps in total.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::cartree {
constexpr std::uint32_t kMaxN = 1048576u;  // largest array length (2^20)
constexpr std::int32_t kNone = -1;
enum class CartResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };
// cartesian_tree: for the array a[0 .. n-1] writes the tree in which every node is an index, the root is the index of the minimum, and each subtree
// is built the same way on the part of the array to the left and to the right of its root. Ties: among equal minima the LEFTMOST index is the root
// (so an equal element to the right is a descendant of an equal element to its left, never the other way round).
// Outputs: parent[i], left[i], right[i] for i in 0 .. n-1 (each kNone when absent; parent[root] = kNone) and *root = the root index, or kNone when
// n = 0. The in-order walk of the tree visits 0, 1, ..., n-1.
// Check order: a null root gives NullArgument; then n above kMaxN gives TooLarge; then, only when n above 0, a null a, parent, left or right gives
// NullArgument (for n = 0 the arrays may all be null and are not touched; *root = kNone and Ok). On any failure nothing is written to any output,
// including *root. The input a is never modified, and the output arrays must not overlap a or each other (undefined behaviour otherwise).
CartResult cartesian_tree(const std::uint32_t* a, std::uint32_t n, std::int32_t* parent, std::int32_t* left, std::int32_t* right, std::int32_t* root);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::cartree
