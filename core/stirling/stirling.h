// Stirling numbers and Bell numbers modulo 998244353: one full row of the Stirling numbers of the second kind, one full row of the unsigned Stirling
// numbers of the first kind, and the Bell numbers B(0) .. B(n).
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is the output and the caller scratch named below). Each function takes O(n^2) steps.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::stirling {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxN = 4096u;  // largest n; every output table holds n + 1 entries
enum class StirResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3 };
// All entries are exact values reduced modulo kMod, in the range 0 .. 998244352. On any failure nothing is written.
// Check order for all three functions: a null out (or, for bell_table only, a null scratch, always) gives NullArgument; then n above kMaxN gives
// TooLarge; then, for bell_table only, scratch_words below n + 1 gives ScratchTooSmall.
//
// stirling2_row: out[k] = S(n, k) for k = 0 .. n, the number of ways to split an n-element set into exactly k non-empty unlabelled blocks. S(0, 0) = 1;
// for n above 0, S(n, 0) = 0 and S(n, n) = 1.
StirResult stirling2_row(std::uint32_t n, std::uint32_t* out);
// stirling1_row: out[k] = c(n, k) for k = 0 .. n, the unsigned Stirling numbers of the first kind: the number of permutations of n elements with exactly
// k cycles. c(0, 0) = 1; for n above 0, c(n, 0) = 0 and c(n, n) = 1; the row sums to n! modulo kMod.
StirResult stirling1_row(std::uint32_t n, std::uint32_t* out);
// bell_table: out[v] = B(v) for v = 0 .. n, the Bell numbers (the number of set partitions of v elements, B(0) = 1). scratch holds scratch_words u32
// words of working memory with unspecified contents afterwards; n + 1 words always suffice and the method must work inside them.
StirResult bell_table(std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::stirling
