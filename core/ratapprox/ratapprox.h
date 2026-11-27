// Best rational approximations of an exact rational with a bounded denominator (Farey neighbours and the nearest fraction). Pure integer: no
// floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global state,
// no scratch. Every loop must have a fixed bound. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::ratapprox {
// A fraction num / den with den above 0 and gcd(|num|, den) equal to 1 (lowest terms; zero is exactly 0 / 1). Every output is such a value.
struct Frac {
  std::int64_t num;
  std::int64_t den;
};
enum class ApproxResult : std::uint8_t { Ok = 0, NullArgument = 1, ZeroDivision = 2, BadBound = 3, Overflow = 4 };
// THE INPUT value x is num / den with any int64 num and any int64 den except 0 (den may be negative, INT64_MIN is allowed, the fraction need not be
// reduced; the value is the exact mathematical num / den). max_den is the denominator bound, 1 to INT64_MAX (a bound of 0 or above INT64_MAX is
// BadBound). A candidate is any fraction p / q in lowest terms with 1 <= q <= max_den and p any integer. The results below fit in int64 for every
// input except ONE: num = INT64_MIN with den = -1, whose exact value is +2^63 (an integer, so a candidate at every bound, and every result would
// need the numerator +2^63). That input gives Overflow at every bound, for both functions. For every other input the docs must argue why the results fit
// (the numerators of the candidates that matter never exceed the reduced numerator of x, and the denominators never exceed max_den).
//
// ratapprox_bounds: *lo is the LARGEST candidate that is at most x, and *hi is the SMALLEST candidate that is at least x. When x itself is a
// candidate (its reduced denominator is at most max_den) then *lo = *hi = x. Otherwise lo < x < hi, and lo and hi are neighbours in the Farey
// sequence of order max_den (hi.num * lo.den - lo.num * hi.den = 1).
//
// ratapprox_nearest: *out = the candidate closest to x (smallest |x - p / q|, exact). Ties (two candidates exactly equally far from x, one below
// and one above) are broken by the SMALLER denominator, and if the denominators are also equal by the SMALLER value. When x is a candidate the
// result is x.
//
// Check order for both: a null pointer argument (lo and hi, or out) gives NullArgument; then den equal to 0 gives ZeroDivision; then max_den equal
// to 0 or above INT64_MAX gives BadBound; then the input num = INT64_MIN with den = -1 gives Overflow (after BadBound, so a bad bound is reported first).
// On any failure the outputs are untouched. Outputs must not overlap each other (they are distinct objects).
ApproxResult ratapprox_bounds(std::int64_t num, std::int64_t den, std::uint64_t max_den, Frac* lo, Frac* hi);
ApproxResult ratapprox_nearest(std::int64_t num, std::int64_t den, std::uint64_t max_den, Frac* out);
// Decisions: the method is free (continued fractions with semiconvergents, or Stern-Brocot with batched steps) as long as the contract above
// holds and every loop is bounded by a constant. A header ambiguity found later is a question to the maintainer, not an interpretation.
}  // namespace lumora::ratapprox
