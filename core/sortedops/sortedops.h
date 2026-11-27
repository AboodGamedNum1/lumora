// Operations on ascending arrays of 32-bit unsigned keys: binary search bounds, merge of two arrays, removal of adjacent duplicates. Pure
// integer: no floating point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label:
// unit tests only.
#pragma once
#include <cstdint>

namespace lumora::sortedops {

constexpr std::uint32_t kMaxCount = 1u << 26;  // largest element count of any one input array

enum class OpResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3 };

// SORTED INPUT: every input array is ascending (non-decreasing). That is a precondition and is NOT checked: on an array that is not
// sorted the results are not specified and not tested. Duplicates are allowed.
//
// Check order for every function: a null pointer argument gives NullArgument (an array pointer counts only while its length is above 0; an
// out pointer always counts; for merge, dst counts while na + nb is above 0); then a count above kMaxCount gives TooLarge; then
// BufferTooSmall where it applies. With every length 0 and every needed out pointer set, the call is Ok and no array is touched. On any
// failure nothing is written (out pointers are left untouched). Input arrays must not overlap an output array.

// *out_index = the smallest index i in [0, n] with keys[i] >= key (n when every key is smaller). Reads only.
OpResult lower_bound_u32(const std::uint32_t* keys, std::uint32_t n, std::uint32_t key, std::uint32_t* out_index);

// *out_index = the smallest index i in [0, n] with keys[i] > key (n when no key is larger). Reads only.
OpResult upper_bound_u32(const std::uint32_t* keys, std::uint32_t n, std::uint32_t key, std::uint32_t* out_index);

// Merges a[0 .. na) and b[0 .. nb) into dst[0 .. na + nb), ascending, keeping every element (duplicates included). BufferTooSmall when
// dst_cap < na + nb (the sum is computed without overflow). *out_len = na + nb on Ok. dst must not overlap a or b.
OpResult merge_sorted_u32(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                          std::uint32_t dst_cap, std::uint32_t* out_len);

// Removes adjacent equal keys in place, keeping the first of each run, so keys[0 .. *out_len) holds each distinct key once, ascending.
// Elements at index *out_len and above are unspecified afterwards (not tested). n 0 gives *out_len 0.
OpResult unique_sorted_u32(std::uint32_t* keys, std::uint32_t n, std::uint32_t* out_len);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::sortedops
