// Quadratic residue tools: the Kronecker symbol, the Jacobi symbol and the least square root modulo a prime.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global
// state, no arrays, no tables. kronecker and jacobi take O(log n) steps (a gcd-style reduction, no factoring). sqrt_mod_prime takes O(sqrt(p)) steps for the
// primality check plus O(log^2 p) steps for the root; the multiplication modulo p is a u64 product of two residues below p (p < 2^32), reduced at once.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::jacobi {
constexpr std::uint32_t kMaxModulus = 4294967295u;  // largest n or p (every u32)
enum class JResult : std::uint8_t { Ok = 0, NullArgument = 1, BadModulus = 2, NotPrime = 3, NoRoot = 4 };
// Check order for all three functions: a null out gives NullArgument; then the function-specific failure below. Nothing is written to *out on a failure.
//
// kronecker: *out = the Kronecker symbol (a / n) in {-1, 0, 1} for ANY i64 a (negative included) and ANY u32 n (n = 0 included); it never fails except for a null
// out. Definition: (a / 0) = 1 when a is 1 or -1, otherwise 0; (a / 1) = 1 for every a; for n above 1 write n as a product of primes, and (a / n) is the product of
// the symbols of its prime factors with multiplicity, where for an odd prime p the symbol (a / p) is the Legendre symbol (0 when p divides a, 1 when a is a nonzero
// square modulo p, otherwise -1), and (a / 2) is 0 for an even a, 1 for a congruent to 1 or 7 modulo 8 and -1 for a congruent to 3 or 5 modulo 8 (the residue of a
// negative a is the mathematical one, in 0..7).
// Examples: (1 / 1) = 1, (10 / 1) = 1, (0 / 1) = 1, (0 / 9) = 0, (2 / 15) = 1, (7 / 15) = -1, (-1 / 7) = -1, (-1 / 3) = -1, (3 / 9) = 0, (5 / 21) = 1,
// (2 / 7) = 1, (3 / 7) = -1, (-3 / 5) = -1, (0 / 0) = 0, (1 / 0) = 1, (-1 / 0) = 1, (2 / 0) = 0, (2 / 2) = 0, (3 / 2) = -1, (1 / 2) = 1, (7 / 8) = 1, (5 / 8) = -1,
// (-1 / 8) = 1, (6 / 10) = 0, (9 / 10) = 1, (100 / 999) = 1, (-17 / 1001) = 1, (2^40 / 45) = 1, (-2^63 / 3) = 1, (2^63 - 1 / 4294967295) = -1.
JResult kronecker(std::int64_t a, std::uint32_t n, std::int32_t* out);
// jacobi: the same value for an ODD n (n = 1 included); an even n, including 0, gives BadModulus (the Jacobi symbol is defined for odd positive n only).
// jacobi(a, n) equals kronecker(a, n) whenever it succeeds. Examples: (2 / 15) = 1, (7 / 15) = -1, (-1 / 3) = -1, (0 / 1) = 1; n = 0, 2, 8 and 10 are BadModulus.
JResult jacobi(std::int64_t a, std::uint32_t n, std::int32_t* out);
// sqrt_mod_prime: *out = the LEAST x in [0, p) with x * x congruent to a modulo p, for a prime p and any i64 a (a is taken modulo p, so a negative a means the
// mathematical residue). p = 2 is allowed (a even gives 0, a odd gives 1). A zero residue gives 0. A p that is 0, 1 or composite gives NotPrime (check order after the
// null check: the prime test comes first, then NoRoot). A nonzero residue that is not a square modulo p gives NoRoot.
// Examples: sqrt(0 mod 2) = 0, sqrt(1 mod 2) = 1, sqrt(3 mod 2) = 1, sqrt(2 mod 7) = 3, sqrt(4 mod 7) = 2, sqrt(3 mod 7) = NoRoot, sqrt(5 mod 13) = NoRoot,
// sqrt(-1 mod 5) = 2, sqrt(-1 mod 13) = 5, sqrt(10 mod 13) = 6, sqrt(8 mod 17) = 5, sqrt(0 mod 5) = 0, sqrt(2 mod 11) = NoRoot, sqrt(1 mod 3) = 1, sqrt(2 mod 3) = NoRoot,
// sqrt(-1 mod 65537) = 256, sqrt(3 mod 65537) = NoRoot, sqrt(-1 mod 998244353) = 86583718, sqrt(2 mod 1000000007) = 59713600, sqrt(3 mod 1000000007) = 82062379,
// sqrt(7 mod 1000000007) = 21125790, sqrt(-1 mod 1000000007) = NoRoot, sqrt(5 mod 4294967291) = 65536, sqrt(3 mod 4294967291) = 257244144; p = 0, 1, 4, 9, 15 and
// 4294967295 are NotPrime.
JResult sqrt_mod_prime(std::int64_t a, std::uint32_t p, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an interpretation.
}  // namespace lumora::jacobi
