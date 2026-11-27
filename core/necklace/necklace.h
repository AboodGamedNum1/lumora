// Cycle counting with Burnside's lemma, modulo 998244353: the number of necklaces (rotations identified), bracelets (rotations and reflections
// identified) and Lyndon words (primitive necklaces) of length n over an alphabet of k letters.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays beyond what the caller passes. Each function takes O(sqrt(n) + d(n) log n) steps or better (divisors of n by trial division
// up to sqrt(n), and fast powers); a loop over every d up to n for the largest n is allowed only if it stays within a few million steps per call.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::necklace {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxN = 1048576u;  // largest length n (2^20), which is below kMod so n is invertible modulo kMod
enum class NeckResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };
// The values are the exact counts reduced modulo kMod, in the range 0 .. 998244352. k is any u32, including 0 and values above kMod (k is used as a
// residue in the powers; the exact count is that of an alphabet of k letters). Check order for all three functions: a null out gives NullArgument; then n
// above kMaxN gives TooLarge. Nothing is written on any failure.
//
// necklace_count: *out = (1 / n) * sum over d dividing n of phi(d) * k^(n / d), the number of necklaces of length n. n = 0 gives 1 (the empty necklace,
// for every k); for n above 0 and k = 0 the count is 0; for k = 1 it is 1.
NeckResult necklace_count(std::uint32_t n, std::uint32_t k, std::uint32_t* out);
// bracelet_count: *out = the number of bracelets of length n (a bracelet is a necklace up to reflection). n = 0 gives 1. For n above 0 and k = 0 the
// count is 0. The reflection average follows Burnside's lemma over the dihedral group of order 2n (for odd n, n reflections each fixing k^((n+1)/2)
// strings; for even n, n / 2 reflections fixing k^(n/2 + 1) strings and n / 2 fixing k^(n/2)).
NeckResult bracelet_count(std::uint32_t n, std::uint32_t k, std::uint32_t* out);
// lyndon_count: *out = (1 / n) * sum over d dividing n of mu(d) * k^(n / d), the number of Lyndon words (aperiodic necklaces) of length n, with mu the
// Moebius function. n = 0 gives 0 (there is no Lyndon word of length 0, for every k); for n above 0 and k = 0 the count is 0; n = 1 gives k modulo kMod.
NeckResult lyndon_count(std::uint32_t n, std::uint32_t k, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::necklace
