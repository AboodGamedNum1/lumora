// Order statistics of arrays of 32-bit values: the k-th smallest value and the number of inversions. Pure integer: no floating point, no libm,
// no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Working memory is caller scratch. Evidence label: unit
// tests only.
#pragma once
#include <cstdint>

namespace lumora::ordstat {

constexpr std::uint32_t kMaxLen = 1u << 20;  // largest array length

enum class OrdResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, OutOfRange = 3, ScratchTooSmall = 4 };

// *out = the value at index k of the array a[0 .. n) when it is sorted ascending by unsigned comparison (k 0 is the minimum, k n-1 the
// maximum; duplicates count each). The array is rearranged IN PLACE in an unspecified order that holds exactly the same multiset of values
// as before (the tests may check the multiset, not the order); on failure the array is not touched at all. The running time must not be
// quadratic on already sorted, reverse sorted or all-equal input of kMaxLen values.
// Check order: a null pointer argument (a only while n is above 0, out always) gives NullArgument; then n above kMaxLen gives TooLarge; then
// k not below n (so every k when n is 0) gives OutOfRange. On any failure *out is left untouched.
OrdResult select_kth_u32(std::uint32_t* a, std::uint32_t n, std::uint32_t k, std::uint32_t* out);

// *out = the number of pairs (i, j) with i < j and a[i] > a[j] (unsigned comparison; equal values are not inversions). The input array is
// const and is never written. Scratch need: 2 * n words when n is above 1, else 0 (scratch contents are unspecified afterwards). The count
// can reach n * (n - 1) / 2, which fits in 64 bits.
// Check order: a null pointer argument (a only while n is above 0, scratch only while the need is above 0, out always) gives NullArgument;
// then n above kMaxLen gives TooLarge; then scratch_words below the need gives ScratchTooSmall. On any failure *out is left untouched.
// The need is compared as a 64-bit value (2 * 4294967295 does not wrap).
OrdResult inversion_count_u32(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words,
                              std::uint64_t* out);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::ordstat
