// Exact integer helpers: square root, gcd and lcm, 64-bit checked arithmetic, and a * b / d with an exact 128-bit intermediate. Pure
// integer: no floating point, no libm, no __int128 and no compiler builtins or intrinsics (MSVC), no allocation, no global state.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::intmath {

// Largest r with r * r <= x. Exact for every 64-bit x (isqrt_u64(UINT64_MAX) is 4294967295).
std::uint32_t isqrt_u64(std::uint64_t x);

// Greatest common divisor. gcd(0, 0) is 0 and gcd(a, 0) is a.
std::uint64_t gcd_u64(std::uint64_t a, std::uint64_t b);

// Least common multiple into *out. lcm(0, x) and lcm(x, 0) are 0. Returns false when out is null or the result does not fit in 64 bits;
// *out is untouched on failure.
bool lcm_u64(std::uint64_t a, std::uint64_t b, std::uint64_t* out);

// Checked signed arithmetic into *out. False when out is null or the exact result does not fit in int64 (*out untouched). INT64_MIN is a
// valid operand and a valid result.
bool add_i64(std::int64_t a, std::int64_t b, std::int64_t* out);
bool sub_i64(std::int64_t a, std::int64_t b, std::int64_t* out);
bool mul_i64(std::int64_t a, std::int64_t b, std::int64_t* out);

enum class Round : std::uint8_t { Down = 0, Up = 1, NearestUp = 2 };

// a * b / d with the product held exactly (up to 128 bits), rounded to an integer: Down is the floor, Up is the ceiling, NearestUp rounds
// to the nearest integer and a remainder of exactly half rounds up. Returns false when out is null, d is 0, or the rounded result does not
// fit in 64 bits (*out untouched on failure). Any Round value outside the three is false. The result of a * b of 0 is 0 for every mode.
bool mul_div_u64(std::uint64_t a, std::uint64_t b, std::uint64_t d, Round mode, std::uint64_t* out);

// Decisions: the check order for mul_div_u64 is not part of the contract (every failure has the same observable result). A header ambiguity
// found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::intmath
