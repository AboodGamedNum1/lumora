// The Calkin-Wilf tree of positive fractions and Stern's diatomic sequence (fusc). Node 1 is 1/1; node i has the left child 2i with value a / (a + b)
// and the right child 2i + 1 with value (a + b) / b, where a / b is the value of node i. Every positive fraction in lowest terms appears exactly once.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. Every call takes O(64) steps or fewer; a loop over all nodes up to n is not acceptable.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::calkinwilf {
constexpr std::uint64_t kMaxNode = 18446744073709551614ull;  // 2^64 - 2, the largest n for calkin_wilf (n + 1 must fit in a u64)
enum class CwResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, NotCoprime = 3, TooLarge = 4 };
// Check order: a null output pointer (any of them) gives NullArgument; then a zero argument (n = 0 for calkin_wilf, p = 0 or q = 0 for
// calkin_wilf_index) gives BadArgument; then (calkin_wilf_index only) gcd(p, q) above 1 gives NotCoprime; then n above kMaxNode (calkin_wilf) or an index that
// does not fit in a u64 (calkin_wilf_index) gives TooLarge. Nothing is written to an output on a failure. fusc has no failure except NullArgument.
//
// fusc: *out = s(n) for every u64 n, with s(0) = 0, s(1) = 1, s(2k) = s(k), s(2k + 1) = s(k) + s(k + 1). Examples: s(0..11) = 0 1 1 2 1 3 2 3 1 4 3 5,
// s(100) = 7, s(1000) = 11, s(10^6) = 191, s(2^32) = 1, s(2^63) = 1, s(2^64 - 3) = 125, s(2^64 - 2) = 63, s(2^64 - 1) = 64. The largest value for n below 2^k is
// the Fibonacci number F(k + 1), so below 2^64 it is F(65) = 17167680177565.
CwResult fusc(std::uint64_t n, std::uint64_t* out);
// calkin_wilf: node n (1 <= n <= kMaxNode) has the value s(n) / s(n + 1), written as *num and *den, always in lowest terms. Examples: nodes 1 to 8 are
// 1/1 1/2 2/1 1/3 3/2 2/3 3/1 1/4; n = 100 gives 7/19; n = 1000 gives 11/39; n = 2^32 gives 1/33; n = 2^63 gives 1/64; n = 2^64 - 3 gives 125/63;
// n = 2^64 - 2 gives 63/64. Both outputs are below 2^45.
CwResult calkin_wilf(std::uint64_t n, std::uint64_t* num, std::uint64_t* den);
// calkin_wilf_index: *out = the node number n whose value is p / q, for p and q from 1 to 2^32 - 1 with gcd(p, q) = 1 (the inverse of calkin_wilf). Examples:
// 1/1 gives 1; 1/2 gives 2; 2/1 gives 3; 3/2 gives 5; 7/5 gives 25; 5/7 gives 22; 22/7 gives 519; 355/113 gives 67107847; 113/355 gives 33555448;
// 46368/28657 gives 5592405; 64/1 gives 2^64 - 1; 1/64 gives 2^63; 63/64 gives 2^64 - 2; 125/63 gives 2^64 - 3; 2971215073/1836311903 gives 58640620148053;
// 1836311903/2971215073 gives 46912496118442. The index needs one more bit than the number of subtraction steps of the Euclid walk from p/q to 1/1, so it
// fits exactly when that number of steps is 63 or fewer: 65/1, 1/65 and 4294967295/4294967294 give TooLarge. (p, q) = (2, 4) gives NotCoprime.
CwResult calkin_wilf_index(std::uint32_t p, std::uint32_t q, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::calkinwilf
