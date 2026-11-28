// Fibonacci numbers and Zeckendorf representations. F(0) = 0, F(1) = 1, F(k) = F(k - 1) + F(k - 2). Every positive integer is the sum of
// Fibonacci numbers F(i) with i at least 2 and no two indices adjacent, in exactly one way (Zeckendorf's theorem); zero is the empty sum.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no tables of Fibonacci numbers (compute them). Every call takes O(93) steps or fewer.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::zeckendorf {
constexpr std::uint32_t kMaxIndex = 93u;       // F(93) = 12200160415121876738 is the largest Fibonacci number below 2^64 (F(94) is not)
constexpr std::uint32_t kMaxTerms = 46u;       // the most terms any u64 needs
enum class ZkResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, BufferTooSmall = 3, TooLarge = 4 };
// Check order for every function: a null pointer argument gives NullArgument; then the other checks in the order each function lists. Nothing is written
// to an output on a failure (for BufferTooSmall too).
//
// fib: *out = F(k) for k from 0 to 93; k above 93 gives TooLarge. Examples: F(0) = 0, F(1) = 1, F(2) = 1, F(10) = 55, F(40) = 102334155,
// F(70) = 190392490709135, F(80) = 23416728348467685, F(90) = 2880067194370816120, F(92) = 7540113804746346429, F(93) = 12200160415121876738, F(94) is TooLarge.
ZkResult fib(std::uint32_t k, std::uint64_t* out);
// zeckendorf: writes the Zeckendorf indices of n, largest first, to idx[0] .. idx[count - 1] and the number of terms to *count. n = 0 gives count 0 (and
// idx is not touched, but must not be null). If cap is smaller than the number of terms the result is BufferTooSmall (check order: nulls, then the
// buffer). Every index is from 2 to 93, strictly decreasing, with no two adjacent. Examples: 0 -> (none); 1 -> 2; 2 -> 3; 3 -> 4; 4 -> 4 2; 10 -> 6 3;
// 100 -> 11 6 4; 1000 -> 16 7; 10^6 -> 30 26 24 12 10; 2^63 -> 92 88 86 84 75 71 67 63 61 54 51 49 42 40 38 34 31 27 25 22 20 17 12 8; F(93) -> 93;
// F(93) - 1 -> 92 90 88 ... 4 2 (46 terms, every even index from 92 down to 2); 2^64 - 1 -> 93 91 88 86 82 76 74 70 66 63 59 56 47 44 40 37 33 31 29 25 19 17
// 13 11 5 3 (26 terms).
ZkResult zeckendorf(std::uint64_t n, std::uint8_t* idx, std::uint32_t cap, std::uint32_t* count);
// zeckendorf_value: the inverse. *out = the sum of F(idx[i]) for i from 0 to count - 1. The indices must be a valid Zeckendorf index list: each from 2 to
// 93, strictly decreasing, no two adjacent (a difference of at least 2); any violation gives BadArgument (an empty list, count 0, is valid and gives 0;
// idx may then be null only if count is 0). Check order: nulls (idx is null with count above 0, or out is null), then the list checks, then a sum that does
// not fit a u64 gives TooLarge. Examples: (2) -> 1; (4, 2) -> 4; (11, 6, 4) -> 100; (93) -> F(93); (93, 91) -> 16860207025497407047; (93, 91, 89) is
// TooLarge (sum 18640186441502121236 is above 2^64 - 1); (3, 3), (4, 3), (1), (94) are BadArgument.
ZkResult zeckendorf_value(const std::uint8_t* idx, std::uint32_t count, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::zeckendorf
