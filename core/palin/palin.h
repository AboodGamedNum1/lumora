// Palindromes in a byte string: the palindrome radius at every center, the longest palindromic substring and the number of palindromic substrings.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). The number of steps must be linear in n (for example Manacher's algorithm); a quadratic method is not acceptable for the
// largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::palin {
constexpr std::uint32_t kMaxLen = 262144;  // largest string length
enum class PalinResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3 };
// The string is s[0 .. n) of raw bytes (any value 0 to 255, compared exactly). A palindromic substring is a run s[i .. j] that reads the same
// backwards; every single byte is one; the empty string is not counted. Two occurrences at different positions count separately.
//
// radii: odd[i] for i in 0 .. n-1 = the number of palindromes of ODD length centered at the byte i (so the longest odd palindrome centered at i has
// length 2 * odd[i] - 1 and odd[i] is at least 1). even[i] for i in 0 .. n-1 = the number of palindromes of EVEN length whose center lies between the
// bytes i - 1 and i (so the longest has length 2 * even[i]); even[0] = 0 always, and even[i] = 0 when s[i - 1] differs from s[i].
// Check order: a null pointer argument (s, odd and even only while n is above 0; the condition uses the given n even when it is above the limit)
// gives NullArgument; then n above kMaxLen gives TooLarge. On failure the outputs are untouched. On success only odd[0 .. n) and even[0 .. n) are
// written; n = 0 is Ok and writes nothing. The arrays must not overlap.
PalinResult palin_radii(std::uint32_t n, const std::uint8_t* s, std::uint32_t* odd, std::uint32_t* even);
// *out_words = the scratch size in u32 words that palin_longest and palin_count need: 2 * n (the algorithm may use less; scratch is working memory
// with unspecified contents afterwards, not tested). Check order: a null out_words gives NullArgument; then n above kMaxLen gives TooLarge (nothing
// written).
PalinResult palin_scratch_words(std::uint32_t n, std::uint32_t* out_words);
// Longest palindromic substring: *out_start and *out_len = the first (smallest start) among the longest palindromic substrings. For n = 0 the result
// is start 0 and length 0. For n above 0 the length is at least 1. Check order: a null s or null scratch while n is above 0, or a null out_start or
// out_len (always), gives NullArgument; then TooLarge; then, only while n is above 0, scratch_words below 2 * n gives ScratchTooSmall. On failure both
// outputs are untouched.
PalinResult palin_longest(std::uint32_t n, const std::uint8_t* s, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_start,
                          std::uint32_t* out_len);
// *out_count = the number of palindromic substrings (occurrences, by position), at most n * (n + 1) / 2, which fits u64 (for n = kMaxLen it is
// 34359869440 for a string of one repeated byte). n = 0 gives 0. Check order: a null s or null scratch while n is above 0, or a null out_count
// (always), gives NullArgument; then TooLarge; then, only while n is above 0, scratch_words below 2 * n gives ScratchTooSmall. On failure *out_count is
// untouched.
PalinResult palin_count(std::uint32_t n, const std::uint8_t* s, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_count);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::palin
