// Subset-lattice transforms of an array of 2^k residues modulo 998244353: the Walsh-Hadamard (xor) transform and its inverse, the subset zeta transform
// and its inverse (Mobius), and the xor convolution of two arrays.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is the arrays themselves and the caller scratch named below). Every transform must take
// O(k * 2^k) steps; a quadratic method is not acceptable for k = kMaxK. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::walsh {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxK = 20u;  // largest k; an array holds 2^k entries (1048576 at most)
enum class WalshResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ValueOutOfRange = 3, ScratchTooSmall = 4 };
// An array a of 2^k entries is indexed by bit masks 0 .. 2^k - 1. Every entry must be a residue below kMod (an entry of kMod or more gives
// ValueOutOfRange, every entry is scanned and nothing is written). All results are residues in 0 .. kMod - 1. k = 0 is legal: one entry.
// Check order for the four transforms: a null a gives NullArgument; then k above kMaxK gives TooLarge; then any entry of a at kMod or more gives
// ValueOutOfRange. On any failure the array is unchanged.
//
// walsh_xor: a becomes A with A[s] = sum over t of (-1)^popcount(s & t) * a[t], modulo kMod.
WalshResult walsh_xor(std::uint32_t* a, std::uint32_t k);
// walsh_xor_inverse: the inverse of walsh_xor: a becomes the array whose walsh_xor is the given array (the sum above divided by 2^k modulo kMod).
WalshResult walsh_xor_inverse(std::uint32_t* a, std::uint32_t k);
// subset_zeta: a becomes A with A[s] = sum of a[t] over every t that is a subset of s (t & s == t), modulo kMod.
WalshResult subset_zeta(std::uint32_t* a, std::uint32_t k);
// subset_mobius: the inverse of subset_zeta: a becomes the array whose subset_zeta is the given array.
WalshResult subset_mobius(std::uint32_t* a, std::uint32_t k);
// xor_convolve: out[s] = sum over all pairs (t, u) with t xor u == s of a[t] * b[u], modulo kMod, for s = 0 .. 2^k - 1. a and b are not modified. out holds
// 2^k entries; scratch holds scratch_words u32 words of working memory with unspecified contents afterwards; 2^k words always suffice and the method must
// work inside them. Check order: a null among a, b, out and scratch gives NullArgument; then k above kMaxK gives TooLarge; then scratch_words below 2^k
// gives ScratchTooSmall; then any entry of a, then any entry of b, at kMod or more gives ValueOutOfRange. Nothing is written on any failure. out, scratch,
// a and b do not overlap (outside the contract, not tested).
WalshResult xor_convolve(const std::uint32_t* a, const std::uint32_t* b, std::uint32_t k, std::uint32_t* scratch, std::uint32_t scratch_words,
                         std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::walsh
