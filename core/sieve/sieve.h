// Arithmetic tables over 0 .. limit by sieving: the smallest prime factor, Euler's totient and the Moebius function. In caller storage. Pure
// integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no scratch. Every loop must have a bound that depends only on limit. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::sieve {
constexpr std::uint32_t kMaxLimit = 1048576;  // 2^20, the largest limit
enum class SieveResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };
// Every function fills an array of limit + 1 entries, index 0 .. limit. Check order: a null out gives NullArgument (also when limit is above the
// maximum: the null check comes first); then limit above kMaxLimit gives TooLarge. On failure the array is untouched. On success exactly out[0 ..
// limit] is written and nothing else. The array must not overlap anything else.
//
// spf: out[0] = 0, out[1] = 1, and for n at least 2 out[n] = the smallest prime that divides n (so out[n] = n exactly when n is prime).
SieveResult spf_table(std::uint32_t limit, std::uint32_t* out);
// phi: out[0] = 0, out[1] = 1, and for n at least 2 out[n] = the number of integers m in 1 .. n with gcd(m, n) = 1 (Euler's totient; phi(p) = p - 1
// for a prime p, phi(12) = 4, phi(2^19) = 2^18).
SieveResult phi_table(std::uint32_t limit, std::uint32_t* out);
// mu: out[0] = 0, out[1] = 1, and for n at least 2 out[n] = 0 when some prime square divides n, otherwise 1 when n has an even number of prime
// factors and -1 (stored as 255, the two's complement of the signed byte) when the number is odd. So mu(2) = -1 (255), mu(4) = 0, mu(6) = 1 and
// mu(30) = -1 (255).
SieveResult mu_table(std::uint32_t limit, std::uint8_t* out);
// Decisions: the method is free (a linear sieve, or any other exact method) as long as the contract above holds. A header ambiguity found later is
// a question to the maintainer, not an interpretation.
}  // namespace lumora::sieve
