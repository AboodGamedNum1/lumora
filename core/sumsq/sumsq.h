// Representations as sums of squares: the number of ordered integer solutions (signs and order both count) of x^2 + y^2 = n and of
// x^2 + y^2 + z^2 + w^2 = n, for every u32 n.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. A call takes O(sqrt(n)) steps or better with O(1) automatic memory; counting solutions by looping over x and y or over
// x, y, z is not acceptable for n = kMaxN.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::sumsq {
constexpr std::uint32_t kMaxN = 4294967295u;  // largest n (every u32)
enum class SqResult : std::uint8_t { Ok = 0, NullArgument = 1 };
// count_two_squares: *out = r2(n), the number of pairs (x, y) of integers (negative values and both orders count) with x^2 + y^2 = n. r2(0) = 1 (the pair
// (0, 0)). Examples: r2(1) = 4, r2(2) = 4, r2(3) = 0, r2(4) = 4, r2(5) = 8, r2(25) = 12, r2(65) = 16, r2(100) = 12, r2(1000) = 16, r2(7) = 0.
SqResult count_two_squares(std::uint32_t n, std::uint64_t* out);
// count_four_squares: *out = r4(n), the number of quadruples (x, y, z, w) of integers (signs and order count) with x^2 + y^2 + z^2 + w^2 = n. r4(0) = 1.
// Examples: r4(1) = 8, r4(2) = 24, r4(3) = 32, r4(4) = 24, r4(5) = 48, r4(7) = 64, r4(25) = 248, r4(65) = 672, r4(100) = 744, r4(1000) = 3744.
SqResult count_four_squares(std::uint32_t n, std::uint64_t* out);
// Check order for both functions: a null out gives NullArgument; every u32 n is valid. Nothing is written to *out on a failure.
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::sumsq
