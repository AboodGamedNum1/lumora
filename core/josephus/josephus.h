// The Josephus problem: n people stand in a circle numbered 0 .. n - 1; counting starts at person 0, every k-th person is removed, and counting continues
// from the person after the one removed (so k = 1 removes 0, 1, 2, ... in order, and k = 2 with n = 5 removes 1, 3, 0, 4 and leaves 2).
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. Every call takes O(n) steps or better (a loop over the circle sizes, one modulo per size); a method that simulates the circle
// element by element, or any method quadratic in n, is not acceptable for n = kMaxN.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::josephus {
constexpr std::uint32_t kMaxN = 16777216u;  // largest n (2^24)
enum class JosResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, TooLarge = 3, OutOfRange = 4 };
// Check order for both functions: a null out gives NullArgument; then n = 0 or k = 0 gives BadArgument; then n above kMaxN gives TooLarge; then (for
// josephus_removed only) step at n or more gives OutOfRange. Nothing is written to *out on any failure. k is any u32 above 0 (including values far above n).
//
// josephus_survivor: *out = the number (0 .. n - 1) of the person who is left when n - 1 people have been removed. n = 1 gives 0.
JosResult josephus_survivor(std::uint32_t n, std::uint32_t k, std::uint32_t* out);
// josephus_removed: *out = the number of the person removed at step `step` (0-based: step 0 is the first removal, step n - 1 removes the survivor, who is
// the last one standing, so josephus_removed(n, k, n - 1) equals josephus_survivor(n, k)).
JosResult josephus_removed(std::uint32_t n, std::uint32_t k, std::uint32_t step, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::josephus
