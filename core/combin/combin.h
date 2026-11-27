// Exact combinatorial counts over u64: binomial coefficients, multiset counts, factorials and Catalan numbers. A result is the exact mathematical
// value or the status Overflow; it is never wrapped or rounded. Pure integer: no floating point, no libm, no 128-bit types (no __int128, no
// compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global state, no scratch. Every loop must have a fixed bound. Evidence
// label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::combin {
enum class CombResult : std::uint8_t { Ok = 0, NullArgument = 1, Overflow = 2 };
// *out = C(n, k), the number of k-element subsets of an n-element set, for ANY u64 n and k (C(n, k) = 0 when k is above n; C(n, 0) = 1; C(n, k) =
// C(n, n - k)). Overflow when the exact value is above 2^64 - 1. The decision is on the exact value: C(2^64 - 1, 1) fits, C(2^64 - 1, 2) does
// not, C(67, 33) = 14226520737620288370 fits and C(68, 34) does not. A method whose intermediate products exceed 2^64 but whose exact result fits must still
// succeed. A null out gives NullArgument. On failure *out is untouched.
CombResult binom_u64(std::uint64_t n, std::uint64_t k, std::uint64_t* out);
// *out = the number of multisets of size k drawn from n kinds, C(n + k - 1, k), with the convention multichoose(0, 0) = 1 and multichoose(0, k) = 0
// for k above 0 (no kinds and nothing to draw is one empty multiset). n + k - 1 is taken as an exact integer, so it may exceed 2^64 - 1 (then the
// result is still exact if it fits, which requires k small; for example multichoose(2^64 - 1, 1) = 2^64 - 1 fits). Overflow and null as above.
CombResult multichoose_u64(std::uint64_t n, std::uint64_t k, std::uint64_t* out);
// *out = n! for n up to 20 (20! = 2432902008176640000 fits); Overflow for n above 20. Null as above.
CombResult factorial_u64(std::uint32_t n, std::uint64_t* out);
// *out = the n-th Catalan number C(2n, n) / (n + 1) (1, 1, 2, 5, 14, ...). n up to 36 fits (C_36 = 11959798385860453492); n = 37 is the first Overflow, and
// every larger n is Overflow. The decision is on the exact value.
CombResult catalan_u64(std::uint32_t n, std::uint64_t* out);
// Check order for all four: a null out gives NullArgument; then Overflow when the exact value does not fit. On failure *out is untouched.
// Decisions: the method is free as long as the contract above holds and every loop is bounded by a constant. A header ambiguity found later is a
// question to the maintainer, not an interpretation.
}  // namespace lumora::combin
