// Lagrange interpolation modulo 998244353 at the nodes 0, 1, ..., n - 1: given y[i] = f(i) for the unique polynomial f of degree below n, the value f(x).
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays beyond the caller's y. Every call takes O(n log kMod) steps or better (extra memory O(1): no prefix-product table is allowed).
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::lagrange {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxN = 1048576u;  // largest number of nodes n (2^20), below kMod so every factorial up to n! is invertible
enum class LagResult : std::uint8_t { Ok = 0, NullArgument = 1, EmptyInput = 2, TooLarge = 3, ValueOutOfRange = 4 };
// lagrange_eval: *out = f(x) modulo kMod, where f is the unique polynomial of degree below n with f(i) = y[i] for i = 0 .. n - 1 (all arithmetic modulo
// kMod). x is any u32 and is used as its residue x mod kMod (so x = kMod + 2 acts as 2, and x = kMod acts as 0); when that residue is below n the answer
// is y[residue] directly. n = 1 gives y[0] for every x. Every entry of y must be a residue below kMod.
// Check order: a null y or out gives NullArgument; then n = 0 gives EmptyInput; then n above kMaxN gives TooLarge; then any entry y[i] at kMod or more
// gives ValueOutOfRange (every entry is scanned). Nothing is written to *out on any failure; y is never modified.
LagResult lagrange_eval(const std::uint32_t* y, std::uint32_t n, std::uint32_t x, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::lagrange
