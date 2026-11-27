// Number-theoretic convolution modulo the prime 998244353: the product of two polynomials with coefficients below that prime.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). The number of steps must be O(m log m) for the padded size m defined
// below (for example an iterative radix-2 transform with the primitive root 3); a quadratic method is not acceptable for the largest input. Evidence
// label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::ntt {
constexpr std::uint32_t kMod = 998244353u;      // 119 * 2^23 + 1, prime
constexpr std::uint32_t kMaxLen = 262144u;      // largest length of each input (2^18)
enum class NttResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutTooSmall = 4, ValueOutOfRange = 5 };
// The inputs are a[0 .. na) and b[0 .. nb), every entry a residue in 0 .. kMod - 1. The result has r = na + nb - 1 entries (r = 0 when na or nb is 0):
// out[k] = (sum over i of a[i] * b[k - i], taken over all i with 0 <= i < na and 0 <= k - i < nb) modulo kMod, for k = 0 .. r - 1. The padded size m is
// the smallest power of two that is at least r (m = 1 for r = 1); for r = 0 there is no padded size.
//
// ntt_scratch_words: *out_words = the scratch size in u32 words that ntt_convolve needs: 2 * m for na and nb both above 0, and 0 when na or nb is 0
// (the algorithm may use less; scratch is working memory with unspecified contents afterwards, not tested). Check order: a null out_words gives
// NullArgument; then na or nb above kMaxLen gives TooLarge (nothing written).
NttResult ntt_scratch_words(std::uint32_t na, std::uint32_t nb, std::uint32_t* out_words);
// ntt_convolve: on Ok, out[0 .. r) holds the result and *out_len = r (also for r = 0, where nothing is written to out). Check order: a null out_len
// gives NullArgument; then a null pointer among a (only while na is above 0), b (only while nb is above 0), scratch and out (both only while r is
// above 0) gives NullArgument; then na or nb above kMaxLen gives TooLarge (the condition uses the given lengths even when above the limit); then,
// only while r is above 0, scratch_words below 2 * m gives ScratchTooSmall; then, only while r is above 0, out_cap below r gives OutTooSmall; then an
// entry of a or b that is not below kMod gives ValueOutOfRange (every entry is looked at; the input is not modified). Nothing is written on any
// failure, not even *out_len. The scratch pointer needs only the alignment of u32. The arrays a, b and out may alias each other (the result is the
// same as with separate arrays) but must not overlap the scratch.
NttResult ntt_convolve(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* scratch,
                       std::uint32_t scratch_words, std::uint32_t* out, std::uint32_t out_cap, std::uint32_t* out_len);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::ntt
