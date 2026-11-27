// Radix sort of 32-bit keys (alone or with 32-bit payload values) using caller scratch, and a sortedness check. Pure integer: no floating
// point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::sortu {

constexpr std::uint32_t kMaxCount = 1u << 26;  // largest element count

enum class SortResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };

// Keys are unsigned 32-bit values, sorted into ascending (non-decreasing) order. Check order for every function below: a null pointer
// argument while n is above 0 gives NullArgument; n above kMaxCount gives TooLarge. With n 0 every pointer may be null and the call is Ok
// without touching anything. On any failure nothing is read or written. A scratch array must have n elements, must not overlap the other
// arrays, and holds unspecified values after the call (not checked, not tested). n 1 is Ok and changes nothing (pointers must still be
// non-null, since n is above 0).

// Sorts keys[0 .. n) ascending using scratch[0 .. n).
SortResult radix_sort_u32(std::uint32_t* keys, std::uint32_t n, std::uint32_t* scratch);

// Sorts keys[0 .. n) ascending and permutes values[0 .. n) the same way, so values[i] stays with keys[i]. The sort is STABLE: elements with
// equal keys keep their original relative order (and so do their values). Uses scratch_keys[0 .. n) and scratch_values[0 .. n).
SortResult radix_sort_pairs_u32(std::uint32_t* keys, std::uint32_t* values, std::uint32_t n, std::uint32_t* scratch_keys,
                                std::uint32_t* scratch_values);

// *out = true when keys[i] <= keys[i + 1] for every i. n 0 and n 1 give true. Null keys while n is above 0 or null out gives NullArgument;
// n above kMaxCount gives TooLarge. *out is untouched on failure.
SortResult is_sorted_u32(const std::uint32_t* keys, std::uint32_t n, bool* out);

// Decisions: the algorithm is free (any stable exact method meets the contract; the result must not depend on it). A header ambiguity
// found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::sortu
