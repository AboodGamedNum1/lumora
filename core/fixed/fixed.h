// 16.16 fixed-point arithmetic with saturation, plus an exact integer square root. Pure integer: no floating point, no libm, no
// 128-bit types (MSVC), no allocation, no global state. Every function is defined for every input. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::fixed {

using Q16 = std::int32_t;  // value = raw / 65536
constexpr Q16 kOne = 65536;
constexpr Q16 kMax = INT32_MAX;
constexpr Q16 kMin = INT32_MIN;

// Rounding rule used below: "round half away from zero" means the exact quotient is rounded to the nearest integer and an exact half goes
// to the larger magnitude. "Saturate" means clamp to [kMin, kMax].

// Q16 from a whole number: n * 65536 saturated.
Q16 from_int(std::int32_t n);

// Whole part rounded toward minus infinity (floor): to_int_floor(-1) is -1, to_int_floor(kOne - 1) is 0. Defined by arithmetic, not by a
// right shift of a negative value.
std::int32_t to_int_floor(Q16 a);

// a * b / 65536 with the 64-bit product, round half away from zero, saturated.
Q16 mul(Q16 a, Q16 b);

// a * 65536 / b with a 64-bit numerator, round half away from zero (the sign is the product of the signs), saturated. Division by zero
// does not trap: a > 0 gives kMax, a < 0 gives kMin, a == 0 gives 0.
Q16 div(Q16 a, Q16 b);

// a + (b - a) * t / 65536 with b - a and the product kept in 64 bits (no overflow for any inputs), the product term rounded half away
// from zero, the sum saturated. t is not clamped, so values outside 0..kOne extrapolate.
Q16 lerp(Q16 a, Q16 b, Q16 t);

// floor(sqrt(v)) exact for every uint64_t (the result always fits 32 bits). No floating point and no libm.
std::uint32_t isqrt_u64(std::uint64_t v);

// Square root of a Q16 value: 0 for a <= 0, otherwise isqrt_u64(uint64(a) << 16), which is floor(sqrt(a / 65536) * 65536).
Q16 sqrt(Q16 a);

}  // namespace lumora::fixed
