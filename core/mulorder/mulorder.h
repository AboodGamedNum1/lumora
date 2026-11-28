// Multiplicative structure of the integers modulo m: the multiplicative order of a residue, the Carmichael function and the smallest primitive root.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. A call takes O(sqrt(m)) steps plus a small number of modular powers, with O(1) automatic memory (mult_order needs at most about 40 powers, since the order divides lambda(m) < 2^32 and each prime factor is stripped at most Omega(lambda) times; primitive_root tries candidates g upward and the least primitive root has no tight proved bound, so the docs state the measured worst count and its method instead); trying
// every exponent up to the order, or building a table of residues, is not acceptable for m = kMaxM.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::mulorder {
constexpr std::uint32_t kMaxM = 4294967295u;  // largest modulus m (every u32 above 0)
enum class MoResult : std::uint8_t { Ok = 0, NullArgument = 1, BadModulus = 2, NotCoprime = 3, NoPrimitiveRoot = 4 };
// Check order for all three functions: a null out gives NullArgument; then m = 0 gives BadModulus; then (mult_order only) gcd(a, m) not equal to 1 gives
// NotCoprime; then (primitive_root only) a modulus without a primitive root gives NoPrimitiveRoot. Nothing is written to *out on a failure.
//
// carmichael: *out = lambda(m), the exponent of the group of units modulo m (the least e > 0 with x^e = 1 modulo m for every x coprime to m). lambda(1) = 1,
// lambda(2) = 1, lambda(4) = 2, lambda(8) = 2, lambda(16) = 4, lambda(15) = 4, lambda(24) = 2, lambda(100) = 20, lambda(561) = 80, lambda(1000) = 100.
MoResult carmichael(std::uint32_t m, std::uint32_t* out);
// mult_order: *out = the least e > 0 with a^e = 1 modulo m, for any u32 a (a is taken modulo m) coprime to m. For m = 1 every a is coprime to m (gcd(a, 1) = 1)
// and the order is 1; gcd(0, m) = m, so a = 0 is coprime only when m = 1. Examples: order(2, 7) = 3, order(3, 7) = 6, order(2, 9) = 6, order(10, 7) = 6,
// order(2, 1) = 1, order(5, 13) = 4, order(7, 1000) = 20, order(1, 5) = 1, order(4, 15) = 2, order(2, 101) = 100, order(3, 100) = 20.
MoResult mult_order(std::uint32_t a, std::uint32_t m, std::uint32_t* out);
// primitive_root: *out = the smallest g with 1 <= g < m whose order is the number of units modulo m (g generates the whole group), for the moduli that have
// one (m = 1, 2, 4, p^k and 2 p^k for an odd prime p); other moduli give NoPrimitiveRoot. m = 1 gives g = 0 (the only residue). Examples: root(1) = 0,
// root(2) = 1, root(3) = 2, root(4) = 3, root(5) = 2, root(6) = 5, root(7) = 3, root(9) = 2, root(10) = 3, root(14) = 3, root(17) = 3, root(18) = 5,
// root(25) = 2, root(27) = 2, root(101) = 2, root(998) = 7; root(8), root(15) and root(100) are NoPrimitiveRoot.
MoResult primitive_root(std::uint32_t m, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::mulorder
