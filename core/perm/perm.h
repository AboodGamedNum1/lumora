// Permutations of small arrays of 32-bit values: next lexicographic arrangement, rank and unrank of a permutation of 0 .. n-1. Pure integer:
// no floating point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label: unit tests
// only.
#pragma once
#include <cstdint>

namespace lumora::perm {

constexpr std::uint32_t kMaxRankN = 20;           // largest n for rank and unrank (20! = 2432902008176640000 fits in 64 bits)
constexpr std::uint32_t kMaxNextN = 1u << 20;     // largest n for next_permutation

enum class PermResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, NotPermutation = 3, OutOfRange = 4 };

// Check order for every function: a null pointer argument (an array counts only while its length n is above 0; every out pointer always)
// gives NullArgument; then n above its limit gives TooLarge; then the function-specific failure. On any failure nothing is written (out
// pointers and arrays are left untouched).

// Rearranges a[0 .. n) into the next arrangement in lexicographic order of the VALUES (unsigned comparison, duplicates allowed, the same
// rule as the common next-permutation routine of other libraries: equal values never swap with each other, so a multiset is visited without
// repeats). *out_has_next = true when a next arrangement existed and a now holds it; when a is already the last one (non-increasing) it is
// rearranged into the first (ascending) and *out_has_next = false. n 0 and n 1: no change, false.
PermResult next_permutation_u32(std::uint32_t* a, std::uint32_t n, bool* out_has_next);

// perm[0 .. n) must hold each of 0 .. n-1 exactly once, else NotPermutation (checked after TooLarge). *out_rank = the position of perm among
// the n! permutations of 0 .. n-1 in lexicographic order, counting from 0 (the identity has rank 0; the reverse has rank n! - 1). n 0 gives 0.
PermResult perm_rank(const std::uint32_t* perm, std::uint32_t n, std::uint64_t* out_rank);

// Inverse of perm_rank: writes the permutation of 0 .. n-1 with that rank to out_perm[0 .. n). OutOfRange when rank is not below n!
// (checked after TooLarge; n 0 has exactly one permutation, rank 0).
PermResult perm_unrank(std::uint32_t n, std::uint64_t rank, std::uint32_t* out_perm);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::perm
