// Divisor-lattice transforms modulo 998244353 on the integers 1 .. n: the divisor zeta transform, its inverse (the divisor Moebius transform) and the
// Dirichlet convolution. An array of n entries is indexed by the integers 1 .. n through the position m - 1: a[m - 1] belongs to the integer m.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays beyond what the caller passes. The two transforms must take O(n log log n) steps (a pass per prime) or O(n log n) at worst;
// the convolution must take O(n log n) steps (the harmonic sum over pairs d * e <= n), not O(n^2). Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::divtrans {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxN = 1048576u;  // largest n (2^20)
enum class DivResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ValueOutOfRange = 3 };
// Every entry must be a residue below kMod (an entry of kMod or more gives ValueOutOfRange, every entry is scanned and nothing is written). All results
// are residues. n = 0 is legal: nothing to do, Ok, and the arrays may then be null.
// Check order for the transforms: n above kMaxN gives TooLarge; then, only for n above 0, a null a gives NullArgument; then any bad entry gives
// ValueOutOfRange. For divisor_convolve: n above kMaxN gives TooLarge; then, only for n above 0, a null among a, b, out gives NullArgument; then any bad
// entry of a, then of b, gives ValueOutOfRange. On any failure nothing is written (the array, or out, keeps its contents).
//
// divisor_zeta: a becomes A with A(m) = sum of a(d) over every divisor d of m, for m = 1 .. n.
DivResult divisor_zeta(std::uint32_t* a, std::uint32_t n);
// divisor_mobius: the inverse of divisor_zeta: a becomes the array whose divisor_zeta is the given array (a(m) = sum over d | m of mu(m / d) * A(d)).
DivResult divisor_mobius(std::uint32_t* a, std::uint32_t n);
// divisor_convolve: out(m) = sum over every divisor d of m of a(d) * b(m / d), modulo kMod, for m = 1 .. n. a and b are not modified; out holds n entries and
// does not overlap a or b (outside the contract, not tested). a and b may be the same array.
DivResult divisor_convolve(const std::uint32_t* a, const std::uint32_t* b, std::uint32_t n, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::divtrans
