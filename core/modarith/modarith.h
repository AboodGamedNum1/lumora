// Modular arithmetic on 64-bit values with EXACT results for every modulus up to 2^64 - 1: multiplication, exponentiation, inverse and a
// deterministic primality test. Pure integer: no floating point, no libm, and NO 128-bit types, no compiler builtins or intrinsics (MSVC has
// none of them), no allocation, no recursion, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::modarith {

enum class ModResult : std::uint8_t { Ok = 0, NullArgument = 1, ZeroModulus = 2, NotInvertible = 3 };

// Check order for every function: a null out pointer gives NullArgument; then a modulus of 0 gives ZeroModulus (is_prime_u64 has no modulus);
// then the function-specific failure. On any failure *out is left untouched. Inputs a, b and base may be any 64-bit value, also not below m;
// they are used as their residues modulo m.

// *out = (a * b) mod m, exact for every a, b and m (the product can need 128 bits; the implementation must not use a wider type to get it).
ModResult mulmod_u64(std::uint64_t a, std::uint64_t b, std::uint64_t m, std::uint64_t* out);

// *out = (base ^ exp) mod m by exact arithmetic. exp 0 gives 1 mod m (so 0 when m is 1, and 0 ^ 0 gives 1 for m above 1). Must finish in time
// proportional to the bit length of exp.
ModResult powmod_u64(std::uint64_t base, std::uint64_t exp, std::uint64_t m, std::uint64_t* out);

// *out = the x in [0, m) with (a * x) mod m == 1 when a and m are coprime. m 1: every residue is 0, so *out = 0 and the result is Ok.
// NotInvertible when gcd(a mod m, m) is not 1 (so a multiple of m, including a 0, is NotInvertible for every m above 1).
ModResult modinv_u64(std::uint64_t a, std::uint64_t m, std::uint64_t* out);

// *out = true when n is prime, false otherwise; 0 and 1 are not prime. EXACT for every 64-bit n (a deterministic test, no probability, no
// false positives for any n up to 2^64 - 1). Check order: a null out gives NullArgument.
ModResult is_prime_u64(std::uint64_t n, bool* out);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::modarith
