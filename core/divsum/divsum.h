// Divisor summatory functions: the total number of divisors and the total of the divisors of all integers from 1 to n.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. A call takes O(sqrt(n)) steps (the hyperbola method) with O(1) automatic memory; a loop over all k up to n, or a sieve, is not
// acceptable for n = kMaxN.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::divsum {
constexpr std::uint64_t kMaxCountN = 70368744177664ull;  // largest n for divisor_count_sum (2^46)
constexpr std::uint32_t kMaxSigmaN = 4294967295u;         // largest n for divisor_sum_sum (every u32)
enum class DsResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };
// Check order for both functions: a null out gives NullArgument; then n above the function's maximum gives TooLarge (divisor_count_sum only; every u32 n is
// valid for divisor_sum_sum). Nothing is written to *out on a failure. n = 0 is valid and gives 0 for both.
//
// divisor_count_sum: *out = D(n) = the sum over k from 1 to n of d(k), the number of divisors of k (equivalently the sum of floor(n / k) over k from 1 to n).
// Examples: D(0) = 0, D(1) = 1, D(2) = 3, D(3) = 5, D(4) = 8, D(5) = 10, D(10) = 27, D(100) = 482, D(1000) = 7069, D(10^6) = 13970034. D(2^46) is below 2^52.
DsResult divisor_count_sum(std::uint64_t n, std::uint64_t* out);
// divisor_sum_sum: *out = S(n) = the sum over k from 1 to n of sigma(k), the sum of the divisors of k (equivalently the sum of k * floor(n / k)). Examples:
// S(0) = 0, S(1) = 1, S(2) = 4, S(3) = 8, S(4) = 15, S(5) = 21, S(10) = 87, S(100) = 8299, S(1000) = 823081, S(10^6) = 822468118437. S(2^32 - 1) is
// 15171838869443902239 (about 0.822 n^2, below 2^64), so a u64 holds every result exactly.
DsResult divisor_sum_sum(std::uint32_t n, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::divsum
