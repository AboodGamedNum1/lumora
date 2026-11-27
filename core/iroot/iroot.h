// Exact integer roots and powers over u64: the floor of the k-th root, the maximal perfect-power form of a number, and a checked power. Pure
// integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no scratch. Every loop must have a fixed bound. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::iroot {
enum class RootResult : std::uint8_t { Ok = 0, NullArgument = 1, BadExponent = 2, Overflow = 3 };
// *out = the largest r with r^k <= x (the floor of the k-th root), for k in 1 .. 64. iroot(0, k) = 0 and iroot(1, k) = 1 for every k; k = 1 gives x.
// For k = 64 the result is 1 for every x above 0 (2^64 is above every u64). The result is exact for every u64 x (for example iroot(2^64 - 1, 2) =
// 4294967295, iroot(2^64 - 1, 3) = 2642245, iroot(2^63, 63) = 2, iroot(2^63 - 1, 63) = 1). Check order: a null out gives NullArgument; then k = 0 or
// k above 64 gives BadExponent. On failure *out is untouched. There is no Overflow result for this function.
RootResult iroot_u64(std::uint64_t x, std::uint32_t k, std::uint64_t* out);
// The maximal perfect-power form: for x at least 2, *out_exp = the largest e at least 1 such that x = b^e for an integer b, and *out_base = that b
// (the smallest base). So 64 gives base 2 and exponent 6, 36 gives 6 and 2, 12 gives 12 and 1, 2^63 gives 2 and 63, 3^40 gives 3 and 40. For x = 0 and
// x = 1 the result is base x and exponent 1 (a convention: 0 and 1 are every power of themselves). Check order: a null out_base or out_exp gives
// NullArgument (both are checked, so with one null nothing is written). No other failure exists. On failure both outputs are untouched.
RootResult perfect_power_u64(std::uint64_t x, std::uint64_t* out_base, std::uint32_t* out_exp);
// *out = base^exp exactly (0^0 = 1, x^0 = 1, 0^e = 0 for e above 0, 1^e = 1) for ANY u64 base and ANY u32 exponent. Overflow when the exact value is
// above 2^64 - 1 (for example 2^63 fits, 2^64 does not, 3^40 fits, 3^41 does not, (2^32 - 1)^2 fits, 2^32 squared does not, any base above 1 with
// exp above 63 is Overflow). The decision is on the exact value, so a huge exponent with base 0 or 1 still succeeds. Check order: a null out gives
// NullArgument; then Overflow. On failure *out is untouched.
RootResult ipow_u64(std::uint64_t base, std::uint32_t exp, std::uint64_t* out);
// Decisions: the method is free (binary search on the root with an overflow-checked power, or any other exact method) as long as the contract above
// holds and every loop is bounded by a constant. A header ambiguity found later is a question to the maintainer, not an interpretation.
}  // namespace lumora::iroot
