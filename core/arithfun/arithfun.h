// Arithmetic functions of one number: Euler's totient, the number of divisors, the sum of the divisors, the radical, and the prime-factor counts.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays, no sieve or table. A call takes O(sqrt(n)) steps (trial division) with O(1) automatic memory.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::arithfun {
constexpr std::uint32_t kMaxN = 4294967295u;  // largest n (every u32)
enum class AfResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2 };
// Check order for every function: a null out gives NullArgument; then n = 0 gives BadArgument. Nothing is written to *out on a failure. n = 1 is valid.
// For n = 1 the empty product rules apply: phi(1) = 1, d(1) = 1, sigma(1) = 1, rad(1) = 1, omega(1) = 0, Omega(1) = 0.
//
// euler_phi: *out = phi(n), the number of k with 1 <= k <= n and gcd(k, n) = 1. Examples: phi(1) = 1, phi(2) = 1, phi(6) = 2, phi(12) = 4, phi(36) = 12,
// phi(60) = 16, phi(97) = 96, phi(100) = 40, phi(360) = 96, phi(1000) = 400, phi(720720) = 138240, phi(2^31) = 1073741824, phi(2^32 - 1) = 2147483648,
// phi(4294967291) = 4294967290 (that n is prime).
AfResult euler_phi(std::uint32_t n, std::uint32_t* out);
// divisor_count: *out = d(n), the number of positive divisors. Examples: d(1) = 1, d(2) = 2, d(6) = 4, d(12) = 6, d(36) = 9, d(60) = 12, d(97) = 2, d(100) = 9,
// d(360) = 24, d(1000) = 16, d(720720) = 240, d(2^31) = 32, d(3234846615) = 512 (nine distinct primes, the maximum omega for a u32).
AfResult divisor_count(std::uint32_t n, std::uint32_t* out);
// divisor_sigma: *out = sigma(n), the sum of the positive divisors, as u64 (the largest value for a u32 n is below 2^35). Examples: sigma(1) = 1,
// sigma(2) = 3, sigma(6) = 12, sigma(12) = 28, sigma(36) = 91, sigma(60) = 168, sigma(97) = 98, sigma(100) = 217, sigma(360) = 1170, sigma(1000) = 2340,
// sigma(720720) = 3249792, sigma(2^31) = 4294967295, sigma(2^32 - 1) = 7304603328, sigma(3234846615) = 8360755200.
AfResult divisor_sigma(std::uint32_t n, std::uint64_t* out);
// radical: *out = rad(n), the product of the distinct primes dividing n (rad(1) = 1). Examples: rad(2) = 2, rad(12) = 6, rad(36) = 6, rad(60) = 30,
// rad(100) = 10, rad(1000) = 10, rad(720720) = 30030, rad(2^31) = 2, rad(3^20) = 3, rad(2^32 - 1) = 2^32 - 1 (it is squarefree).
AfResult radical(std::uint32_t n, std::uint32_t* out);
// prime_factor_counts: *distinct = omega(n), the number of distinct primes dividing n, and *with_multiplicity = Omega(n), the number of prime factors
// counted with multiplicity. Both pointers must be non-null (either null gives NullArgument, nothing is written). Examples: (1: 0, 0), (2: 1, 1),
// (12: 2, 3), (36: 2, 4), (60: 3, 4), (360: 3, 6), (1000: 2, 6), (720720: 6, 10), (2^31: 1, 31), (3^20: 1, 20), (2^32 - 1: 5, 5), (3234846615: 9, 9).
AfResult prime_factor_counts(std::uint32_t n, std::uint32_t* distinct, std::uint32_t* with_multiplicity);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::arithfun
