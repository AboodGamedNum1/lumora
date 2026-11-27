// Floor sums: the exact sum over i = 0 .. n-1 of floor((a * i + b) / m), reported modulo 2^64 or modulo 998244353, for unsigned and for signed a and b.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. Every function must take O(log m) steps (a Euclid-like reduction); a loop over i is not acceptable for the largest input.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::floorsum {
constexpr std::uint32_t kMod = 998244353u;
enum class FloorResult : std::uint8_t { Ok = 0, NullArgument = 1, ZeroModulus = 2 };
// S = the EXACT mathematical sum over i = 0 .. n-1 of floor((a * i + b) / m), with floor rounding toward minus infinity and no overflow of the
// mathematical value (S is an integer of any size; it is never rounded or clamped). n = 0 gives S = 0. Every u32 value is legal for n, a and b; m must be
// at least 1. Check order for all three functions: a null out gives NullArgument; then m equal to 0 gives ZeroModulus. Nothing is written on a failure.
//
// floorsum_wrap: *out = S modulo 2^64 (the low 64 bits of the exact value).
FloorResult floorsum_wrap(std::uint32_t n, std::uint32_t m, std::uint32_t a, std::uint32_t b, std::uint64_t* out);
// floorsum_mod: *out = S modulo 998244353, in the range 0 .. 998243352.
FloorResult floorsum_mod(std::uint32_t n, std::uint32_t m, std::uint32_t a, std::uint32_t b, std::uint32_t* out);
// floorsum_signed: a and b may be negative (any i32); the floor is taken toward minus infinity, so floor(-1 / 3) = -1 and S may be negative.
// *out = S modulo 2^64 as the two's complement bit pattern (the low 64 bits of the exact value, S negative or not).
FloorResult floorsum_signed(std::uint32_t n, std::uint32_t m, std::int32_t a, std::int32_t b, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::floorsum
