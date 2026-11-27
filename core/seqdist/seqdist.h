// Distances between two sequences of 32-bit values: longest common subsequence, Levenshtein edit distance, and the longest increasing
// subsequence length. Pure integer: no floating point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global
// state. All working memory is caller scratch. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::seqdist {

constexpr std::uint32_t kMaxLen = 1u << 16;  // largest length of any input sequence

enum class SeqResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3 };

// Scratch need in u32 words: lcs_length_u32 and edit_distance_u32 need 2 * (min(na, nb) + 1) when both na and nb are above 0, else 0;
// lis_length_u32 needs n + 1 when n is above 0, else 0. Scratch contents are unspecified afterwards.
// Check order for all three functions: a null pointer argument gives NullArgument (a counts only while its length is above 0, b likewise,
// scratch only while the need above is above 0, out always); then a length above kMaxLen gives TooLarge; then scratch_words below the need
// gives ScratchTooSmall. On any failure *out is left untouched.

// *out = length of the longest common subsequence of a[0 .. na) and b[0 .. nb) (equal values match; an empty side gives 0).
SeqResult lcs_length_u32(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* scratch,
                         std::uint32_t scratch_words, std::uint32_t* out);

// *out = the Levenshtein distance: the fewest single-value insertions, deletions and substitutions that turn a into b (an empty side gives
// the length of the other side). Same check order and scratch rule as lcs_length_u32.
SeqResult edit_distance_u32(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* scratch,
                            std::uint32_t scratch_words, std::uint32_t* out);

// *out = the length of the longest STRICTLY increasing subsequence of a[0 .. n) by unsigned comparison when strict is true, the longest
// NON-DECREASING one when strict is false. n 0 gives 0. Check order and scratch rule as above.
SeqResult lis_length_u32(const std::uint32_t* a, std::uint32_t n, bool strict, std::uint32_t* scratch, std::uint32_t scratch_words,
                         std::uint32_t* out);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::seqdist
