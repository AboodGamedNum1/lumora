// Exact rational numbers over 64-bit integers: construction, add, subtract, multiply, divide, compare, floor and ceiling. A result is either the
// exact mathematical value in lowest terms or the status Overflow; it is never rounded or wrapped. Pure integer: no floating point, no libm, no
// 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::ratio {
// A value is num / den. A VALID value has den above 0 and gcd(|num|, den) equal to 1 (lowest terms), so zero is exactly 0 / 1. Every input to
// the functions below must be valid; every output is valid.
struct Ratio {
  std::int64_t num;
  std::int64_t den;
};
enum class RatioResult : std::uint8_t { Ok = 0, NullArgument = 1, BadRatio = 2, ZeroDivision = 3, Overflow = 4 };
// Builds the value num / den in lowest terms with a positive denominator. Check order: a null out gives NullArgument; then den equal to 0 gives
// ZeroDivision; then, when the lowest-terms value does not fit (a numerator or denominator outside the int64 range after reduction and sign
// normalization, for example num = INT64_MIN with den = -1), Overflow. Fits is decided on the REDUCED value: num = INT64_MIN with den =
// INT64_MIN is 1 / 1 and succeeds, and num = INT64_MIN with den = 2 is -2^62 / 1 and succeeds. On failure *out is untouched.
RatioResult ratio_make(std::int64_t num, std::int64_t den, Ratio* out);
// out = a + b, a - b, a * b, a / b. Check order: a null pointer argument (a, b and out) gives NullArgument; then an invalid a or invalid b gives
// BadRatio; then (divide only) b equal to zero (b.num = 0) gives ZeroDivision; then Overflow when the exact result in lowest terms does not fit
// (num outside the int64 range, or den above INT64_MAX). Overflow is decided on the exact lowest-terms result, not on any intermediate value: a
// method that overflows on an intermediate product but whose exact result fits must still return Ok with that result. On failure *out is
// untouched. The pointers may alias (out may equal a or b).
RatioResult ratio_add(const Ratio* a, const Ratio* b, Ratio* out);
RatioResult ratio_sub(const Ratio* a, const Ratio* b, Ratio* out);
RatioResult ratio_mul(const Ratio* a, const Ratio* b, Ratio* out);
RatioResult ratio_div(const Ratio* a, const Ratio* b, Ratio* out);
// *out = -1, 0 or 1 when a is below, equal to or above b (exact, never overflows). Check order: a null pointer argument gives NullArgument; then an
// invalid a or b gives BadRatio. On failure *out is untouched.
RatioResult ratio_compare(const Ratio* a, const Ratio* b, std::int32_t* out);
// *out = the largest integer not above a (floor), or the smallest integer not below a (ceil); always fits. Same check order and failure rule as
// ratio_compare.
RatioResult ratio_floor(const Ratio* a, std::int64_t* out);
RatioResult ratio_ceil(const Ratio* a, std::int64_t* out);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::ratio
