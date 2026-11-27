// Chinese remainder theorem for moduli that need not be coprime: solve a system of simultaneous congruences over u64 moduli exactly. Pure integer:
// no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global state,
// no scratch. Every loop must have a fixed bound. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::crt {
constexpr std::uint32_t kMaxEquations = 64;  // largest number of congruences
enum class CrtResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ZeroModulus = 3, NoSolution = 4, Overflow = 5 };
// The system is: x = r[i] (mod m[i]) for i in 0 .. n-1. Each r[i] is ANY u64 (it is taken modulo m[i], it need not be below m[i]); each modulus
// m[i] must be at least 1 (a modulus of 1 accepts every x). The moduli need not be coprime or distinct.
//
// RESULT: let L be the least common multiple of all the moduli (exact, as a mathematical integer, possibly far above 2^64). The solutions of the
// system, when there are any, are exactly the integers congruent to one value modulo L. On success *out_m = L and *out_r = that value reduced to
// the range 0 .. L-1 (the smallest non-negative solution). The empty system (n = 0) has *out_r = 0 and *out_m = 1.
//
// NoSolution is decided on the exact mathematics: the system has a solution if and only if r[i] and r[j] are congruent modulo gcd(m[i], m[j]) for
// EVERY pair i, j. It does not matter that L may be huge. Overflow means the system HAS a solution but L is above 2^64 - 1, so (*out_r, *out_m) do
// not fit; this is decided on the exact value of L (a system whose intermediate products exceed 2^64 but whose final L fits must succeed).
// When a system has no solution the result is NoSolution even if L would overflow (NoSolution is checked before Overflow).
//
// Check order: a null pointer argument (r and m only while n is above 0; out_r and out_m always; the condition uses the given n even when it is
// above the limit) gives NullArgument; then n above kMaxEquations gives TooLarge; then any modulus equal to 0 (every m[i] is checked, in one pass)
// gives ZeroModulus; then NoSolution; then Overflow. On any failure *out_r and *out_m are untouched. The input arrays and outputs must not overlap.
CrtResult crt_solve(std::uint32_t n, const std::uint64_t* r, const std::uint64_t* m, std::uint64_t* out_r, std::uint64_t* out_m);
// Decisions: the method is free (pairwise merging with the extended Euclidean algorithm and a two-word helper written with ordinary uint64
// operations, or any other exact method) as long as the contract above holds and every loop is bounded by a constant. A header ambiguity found
// later is a question to the maintainer, not an interpretation.
}  // namespace lumora::crt
