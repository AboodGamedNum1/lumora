// Longest strictly increasing subsequence: its length, the number of longest ones modulo 2^64, and one canonical longest one.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). The number of steps must be O(n log n) (for example patience sorting with
// binary search and prefix sums); a quadratic method is not acceptable for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::lis {
constexpr std::uint32_t kMaxLen = 262144;  // largest sequence length
enum class LisResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3 };
// The sequence is a[0 .. n) of u32 values. A subsequence is a set of indices i1 < i2 < ... < iL; it is strictly increasing when a[i1] < a[i2] < ... <
// a[iL]. Equal values never chain. Two subsequences are different when their index sets differ (equal values at different positions count separately).
//
// lis_scratch_words: *out_words = the scratch size in u32 words that lis_run needs: 16 * n + 8 (the algorithm may use less; scratch is working memory
// with unspecified contents afterwards, not tested). Check order: a null out_words gives NullArgument; then n above kMaxLen gives TooLarge (nothing
// written).
LisResult lis_scratch_words(std::uint32_t n, std::uint32_t* out_words);
// lis_run: on Ok, *out_len = L, the length of the longest strictly increasing subsequence (0 for n = 0); *out_count = the number of such longest
// subsequences, modulo 2^64 (1 for n = 0: the empty one; it may be 0 for large n when the true count is a multiple of 2^64, and a test with the exact
// count 2^64 may exist); out_idx[0 .. L) = the canonical one: the longest subsequence whose index list is smallest in lexicographic order (compare i1
// first, then i2, and so on; all have the same length). It must be computed exactly: the first index is the smallest index that starts some longest
// subsequence, each next index is the smallest index after the previous one that has a larger value and can still finish a longest subsequence.
// out_idx must hold n entries when n is above 0; only out_idx[0 .. L) is written on success.
// Check order: a null pointer argument gives NullArgument (a, scratch and out_idx only while n is above 0; out_len and out_count always); then n above
// kMaxLen gives TooLarge (the condition uses the given n even when above the limit); then, only while n is above 0, scratch_words below 16 * n + 8
// gives ScratchTooSmall. Nothing is written on any failure.
LisResult lis_run(std::uint32_t n, const std::uint32_t* a, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_len,
                  std::uint64_t* out_count, std::uint32_t* out_idx);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::lis
