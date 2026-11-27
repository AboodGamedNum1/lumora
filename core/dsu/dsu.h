// Disjoint-set union (union-find) over the elements 0 .. n-1, stored in one caller array of 32-bit parents. Pure integer: no floating
// point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion (an iterative find is required, since chains can be
// long), no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::dsu {

constexpr std::uint32_t kMaxElements = 1u << 26;  // largest element count

enum class DsuResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, OutOfRange = 3 };

// The state is parent[0 .. n). It must be made by dsu_init and changed only by the functions below; an array that was changed any other way
// is not valid and the behaviour of these functions on it is not specified and not tested. Elements are numbered 0 .. n-1.
//
// CANONICAL ROOT: the root of a set is its SMALLEST element, and an element is a root exactly when parent[i] == i. So after any sequence
// of calls, dsu_find returns the smallest element of the set that contains its argument. The values of parent[i] for non-root elements
// are NOT specified (path compression is allowed and expected) and are not tested; only roots (parent[i] == i) are specified.
//
// Check order for every function: a null pointer argument (parent, and any out pointer) gives NullArgument, even when n is 0; n above
// kMaxElements gives TooLarge; an element argument at or above n gives OutOfRange (so with n 0 every element is out of range). On any
// failure nothing is written (for find and the queries: not even compression; out pointers are left untouched).

// Makes n singleton sets: parent[i] = i for every i.
DsuResult dsu_init(std::uint32_t* parent, std::uint32_t n);

// *out_root = smallest element of the set containing x. May rewrite non-root entries of parent (compression), never roots.
DsuResult dsu_find(std::uint32_t* parent, std::uint32_t n, std::uint32_t x, std::uint32_t* out_root);

// Merges the sets containing a and b (a == b, or already in one set, is Ok and changes no root). *out_merged = true when two different
// sets were joined, false otherwise. The new root is the smaller of the two old roots.
DsuResult dsu_unite(std::uint32_t* parent, std::uint32_t n, std::uint32_t a, std::uint32_t b, bool* out_merged);

// *out_count = number of sets (number of i with parent[i] == i). Reads only; n 0 gives 0.
DsuResult dsu_count_sets(const std::uint32_t* parent, std::uint32_t n, std::uint32_t* out_count);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer,
// not an interpretation.

}  // namespace lumora::dsu
