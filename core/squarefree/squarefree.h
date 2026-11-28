// Squarefree integers: the Moebius function of one number and the count of squarefree numbers up to n.
// A number is squarefree when no square above 1 divides it (1 is squarefree, 0 is not).
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. mobius takes O(sqrt(n)) steps; squarefree_count takes O(sqrt(n) * sqrt(sqrt(n))) steps or better with O(1) automatic memory; a loop
// over all k up to n, or any table of size sqrt(n), is not acceptable for n = kMaxN.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::squarefree {
constexpr std::uint32_t kMaxN = 4294967295u;  // largest n (every u32)
enum class SfResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2 };
// mobius: *out = mu(n) for n from 1 to kMaxN: 1 when n = 1, 0 when n has a square factor above 1, otherwise (-1)^k for k distinct prime factors. Examples:
// mu(1) = 1, mu(2) = -1, mu(3) = -1, mu(4) = 0, mu(6) = 1, mu(12) = 0, mu(30) = -1, mu(210) = 1, mu(2310) = -1. n = 0 gives BadArgument.
SfResult mobius(std::uint32_t n, std::int32_t* out);
// squarefree_count: *out = the number of squarefree k with 1 <= k <= n, for every n including 0 (the count is 0). Examples: Q(0) = 0, Q(1) = 1, Q(2) = 2,
// Q(4) = 3, Q(10) = 7, Q(100) = 61, Q(1000) = 608, Q(10^6) = 607926. The count is below 2^32, so u64 holds it with room.
SfResult squarefree_count(std::uint32_t n, std::uint64_t* out);
// Check order for both functions: a null out gives NullArgument; then (mobius only) n = 0 gives BadArgument. Nothing is written to *out on a failure.
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::squarefree
