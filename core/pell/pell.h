// Pell's equation: the smallest solution in positive integers of x^2 - d * y^2 = 1 for a non-square d, reported modulo 998244353, together with the
// length of the period of the continued fraction of sqrt(d).
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. Every call takes O(P) steps where P is the continued fraction period of sqrt(d), with O(1)
// automatic memory; a search over y, or any method that builds the solution in big integers, is not acceptable for d = kMaxD.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::pell {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxD = 1048576u;  // largest d (2^20)
enum class PellResult : std::uint8_t { Ok = 0, NullArgument = 1, BadD = 2 };
// pell_fundamental: for d from 2 to kMaxD that is not a perfect square, sets *x and *y to the smallest solution (x, y) with y > 0 of x^2 - d * y^2 = 1,
// each reduced modulo kMod (so each in 0 .. kMod - 1), and *period to the length P of the repeating part of the continued fraction
// sqrt(d) = [a0; a1, ..., aP, a1, ...]. Examples: d = 2 gives (3, 2) with P = 1; d = 3 gives (2, 1) with P = 2; d = 7 gives (8, 3) with P = 4; d = 61 gives
// (1766319049, 226153980) reduced modulo kMod, with P = 11. The exact solution can have millions of digits, only its residues are reported. The
// fundamental solution is the convergent p_{P-1} / q_{P-1} when P is even and p_{2P-1} / q_{2P-1} when P is odd (convergent index counted from 0).
// Check order: a null x, y or period gives NullArgument (any of the three); then d = 0, d = 1, d above kMaxD or d a perfect square gives BadD. Nothing is
// written to any output on a failure.
PellResult pell_fundamental(std::uint32_t d, std::uint32_t* x, std::uint32_t* y, std::uint32_t* period);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::pell
