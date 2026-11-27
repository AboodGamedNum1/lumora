// Digit counting in a positional base: how many integers in 0 .. n have a given digit sum, and the sum of the digit sums of 0 .. n.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no tables larger than a few dozen u64 on the stack. Every call takes O(digits * base * digit_sum_limit) steps or better, where the number
// of digits of n in the base is at most 41 (n below 2^40) and the base is at most 64.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::digitcount {
constexpr std::uint64_t kMaxN = 1099511627776ull;  // largest n (2^40); every result fits a u64 without wrapping
constexpr std::uint32_t kMinBase = 2u;
constexpr std::uint32_t kMaxBase = 64u;
enum class DigitResult : std::uint8_t { Ok = 0, NullArgument = 1, BadBase = 2, TooLarge = 3 };
// The digit sum of x in base b is the sum of the digits of x written in base b (0 for x = 0). Check order for both functions: a null out gives
// NullArgument; then a base below kMinBase or above kMaxBase gives BadBase; then n above kMaxN gives TooLarge. Nothing is written to *out on any failure.
//
// count_digit_sum: *out = the number of integers x with 0 <= x <= n whose digit sum in base `base` equals s. s is any u64: a value above the largest
// possible digit sum (about (base - 1) * 41) simply gives 0. n = 0 gives 1 for s = 0 and 0 otherwise.
DigitResult count_digit_sum(std::uint64_t n, std::uint32_t base, std::uint64_t s, std::uint64_t* out);
// digit_sum_total: *out = the sum over every integer x with 0 <= x <= n of the digit sum of x in base `base`. n = 0 gives 0.
DigitResult digit_sum_total(std::uint64_t n, std::uint32_t base, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::digitcount
