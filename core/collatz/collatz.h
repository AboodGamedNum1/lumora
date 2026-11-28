// Collatz trajectories over u64: the map x -> x / 2 for even x and x -> 3 x + 1 for odd x, run from n until it reaches 1. collatz_steps gives the number of
// steps taken (1 is reached after 0 steps from n = 1), collatz_peak the largest value met on the way (n itself included). The map is only followed while 3 x + 1
// fits a u64 (at most 2^64 - 1): the first odd x on the trajectory with 3 x + 1 above 2^64 - 1 makes the call fail with Overflow, even when the trajectory would
// come back down later.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global
// state, no arrays, no tables. Cost: O(steps) per call with O(1) memory (at most a few thousand steps for any n that does not overflow); the overflow test is
// an exact comparison, not a floating-point or wrapped-result test.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::collatz {
enum class CzResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, Overflow = 3 };
// Check order for both functions: a null output pointer gives NullArgument; then n = 0 gives BadArgument; then Overflow when the trajectory leaves u64. Nothing
// is written to *out on a failure.
//
// collatz_steps: *out = the number of steps. Examples: n = 1 gives 0, 2 gives 1, 3 gives 7, 6 gives 8, 7 gives 16, 27 gives 111, 97 gives 118, 871 gives 178,
// 6171 gives 261, 77031 gives 350, 837799 gives 524, 63728127 gives 949, 670617279 gives 986, 9780657630 gives 1132, 75128138247 gives 1228,
// 989345275647 gives 1348, 80867137596217 gives 1662, 942488749153153 gives 1862, 13194139533312 (3 * 2^42) gives 49, 4294967295
// gives 451, 4611686018427387905 (2^62 + 1) gives 481. Overflow: 7887663552367, 2^63 - 1, 2^63 + 1, 2^64 - 1, 2^64 - 2.
// The number 2^63 gives 63 steps (every step halves it) and is not an overflow.
CzResult collatz_steps(std::uint64_t n, std::uint64_t* out);
// collatz_peak: *out = the largest value on the trajectory. Examples: 1 gives 1, 2 gives 2, 3 gives 16, 6 gives 16, 7 gives 52, 27 gives 9232, 97 gives 9232,
// 871 gives 190996, 6171 gives 975400, 77031 gives 21933016, 837799 gives 2974984576, 63728127 gives 966616035460, 670617279 gives 966616035460,
// 9780657630 gives 319497287463520, 75128138247 gives 319497287463520, 989345275647 gives 1219624271099764, 80867137596217 gives 1180174841128253392,
// 942488749153153 gives 1180174841128253392, 13194139533312 gives 13194139533312, 4294967295 gives 3706040377703680, 4611686018427387905 gives
// 13835058055282163716, 2^63 gives 2^63. Overflow exactly where collatz_steps gives Overflow (same conditions, same order).
CzResult collatz_peak(std::uint64_t n, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::collatz
