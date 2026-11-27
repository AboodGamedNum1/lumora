// Lyndon factorization and least rotation of a byte string.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays. The number of steps must be linear in n (for example Duval's algorithm); a quadratic method is not
// acceptable for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::lyndon {
constexpr std::uint32_t kMaxLen = 262144;  // largest string length
enum class LyndonResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2 };
// The string is s[0 .. n) of raw bytes, compared as unsigned values 0 to 255 (so 255 is the largest byte). A Lyndon word is a non-empty string that is
// strictly smaller than each of its proper non-empty suffixes (every single byte is one). Every string has exactly one factorization into Lyndon words
// w1 >= w2 >= ... >= wk (non-increasing in lexicographic order, equal neighbours allowed), written left to right. A string compares by the first
// differing byte, and a proper prefix is smaller than the longer string.
//
// lyndon_factor: *out_count = k, the number of factors, and lens[0 .. k) = the lengths of w1 .. wk in order (they sum to n). The array lens must hold
// n entries (k is at most n). For n = 0 the result is k = 0 and nothing is written to lens. Check order: a null s or null lens while n is above 0, or a
// null out_count (always), gives NullArgument; then n above kMaxLen gives TooLarge (the condition uses the given n even when it is above the limit).
// On failure the outputs are untouched. On success only lens[0 .. k) and *out_count are written.
LyndonResult lyndon_factor(std::uint32_t n, const std::uint8_t* s, std::uint32_t* lens, std::uint32_t* out_count);
// lyndon_min_rotation: *out_start = the smallest index i in 0 .. n-1 such that the rotation s[i .. n) followed by s[0 .. i) is the lexicographically
// smallest among all n rotations. When several indices give the same smallest rotation (a periodic string), the smallest index is returned. For n = 0
// the result is 0. Check order: a null s while n is above 0, or a null out_start (always), gives NullArgument; then TooLarge. On failure *out_start is
// untouched.
LyndonResult lyndon_min_rotation(std::uint32_t n, const std::uint8_t* s, std::uint32_t* out_start);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::lyndon
