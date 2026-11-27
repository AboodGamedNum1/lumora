// Integer partition counts modulo 998244353: unrestricted partitions, partitions into distinct parts, and partitions into at most k parts, as tables over
// every size 0 .. n.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (the output table is the only working memory, except where a function below names caller scratch).
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::partition {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxN = 65536u;  // largest size n (2^16); every table holds n + 1 entries
enum class PartResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3 };
// A partition of v is a multiset of positive integers (order ignored) that sum to v; the empty multiset is the one partition of 0, so every table has
// entry 0 equal to 1. All entries are exact counts reduced modulo kMod, in the range 0 .. 998244352. On any failure nothing is written.
// Check order for all three functions: a null out (or a null scratch for partition_at_most_k, always) gives NullArgument; then n above kMaxN gives
// TooLarge; for partition_at_most_k then k above kMaxN gives TooLarge; then, only there, scratch_words below n + 1 gives ScratchTooSmall.
//
// partition_all: out[v] = the number of partitions of v, for v = 0 .. n. Every function must take O(n * sqrt(n)) steps or better; a method that is
// quadratic in n is not acceptable for n = kMaxN.
PartResult partition_all(std::uint32_t n, std::uint32_t* out);
// partition_distinct: out[v] = the number of partitions of v into pairwise different parts, for v = 0 .. n. Same step bound.
PartResult partition_distinct(std::uint32_t n, std::uint32_t* out);
// partition_at_most_k: out[v] = the number of partitions of v into at most k parts (equivalently, with every part at most k), for v = 0 .. n. k = 0
// counts only the empty partition: out[0] = 1 and out[v] = 0 for v above 0. k above n behaves as k = n. scratch holds scratch_words u32 words of working
// memory with unspecified contents afterwards; n + 1 words always suffice. Step bound: O(n * min(k, sqrt(n))) or better with the standard split
// (parts of size at most sqrt(n) by a table, larger parts by their count); a plain O(n * k) loop is not acceptable for n = k = kMaxN.
PartResult partition_at_most_k(std::uint32_t n, std::uint32_t k, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::partition
