// Neighbours in the Farey sequence F_n: the reduced fractions a / b with 0 <= a <= b <= n in increasing order, for any u32 order n above 0.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global
// state, no arrays, no tables. A call takes O(log n) steps with O(1) automatic memory (an extended gcd step on a / b); walking the sequence term by term, or
// searching all denominators up to n, is not acceptable for n near 2^32.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::farey {
constexpr std::uint32_t kMaxOrder = 4294967295u;  // largest order n (every u32 above 0)
enum class FResult : std::uint8_t { Ok = 0, NullArgument = 1, BadOrder = 2, BadFraction = 3, NoNeighbour = 4 };
// Check order for both functions: a null c or d gives NullArgument; then n = 0 gives BadOrder; then a / b must be a term of F_n, else BadFraction: b = 0, b above n, a above b,
// or gcd(a, b) not 1 (so 0 / 1 and 1 / 1 are terms, 0 / 5 and 2 / 4 are not); then no neighbour on that side gives NoNeighbour. Nothing is written to *c or *d on a failure.
// c and d must point to different objects: the result for c equal to d is unspecified and is not tested.
//
// farey_next: *c / *d = the term of F_n that follows a / b (the least fraction of F_n above a / b); 1 / 1 has none (NoNeighbour).
// farey_prev: *c / *d = the term of F_n that precedes a / b; 0 / 1 has none (NoNeighbour).
// Examples (n, a / b -> next, prev): (1, 0/1) -> 1/1, none; (1, 1/1) -> none, 0/1; (2, 0/1) -> 1/2, none; (2, 1/2) -> 1/1, 0/1; (5, 1/2) -> 3/5, 2/5; (5, 2/5) -> 1/2, 1/3;
// (5, 1/3) -> 2/5, 1/4; (5, 1/1) -> none, 4/5; (5, 0/1) -> 1/5, none; (7, 3/7) -> 1/2, 2/5; (7, 2/5) -> 3/7, 1/3; (10, 1/3) -> 3/8, 3/10; (10, 3/10) -> 1/3, 2/7; (10, 1/2) -> 5/9, 4/9;
// (10, 9/10) -> 1/1, 8/9; (10, 1/10) -> 1/9, 0/1; (100, 1/3) -> 33/98, 33/100; (100, 22/67) -> 23/70, 21/64; (2^32 - 1, 1/2) -> 2147483648/4294967295, 2147483647/4294967295;
// (2^32 - 1, 0/1) -> 1/4294967295, none; (2^32 - 1, 1/4294967295) -> 1/4294967294, 0/1; (2^32 - 1, 4294967294/4294967295) -> 1/1, 4294967293/4294967294;
// (2^32 - 1, 65536/65537) -> 4294836225/4294901759, 4294901759/4294967294; (2^32 - 1, 1234567/7654321) -> 691930712/4289973553, 692018895/4290520288;
// (2^32 - 1, 2147483647/4294967295) -> 1/2, 2147483646/4294967293. For every F_n and every pair of neighbours c/d after a/b: b * c - a * d = 1.
FResult farey_next(std::uint32_t n, std::uint32_t a, std::uint32_t b, std::uint32_t* c, std::uint32_t* d);
FResult farey_prev(std::uint32_t n, std::uint32_t a, std::uint32_t b, std::uint32_t* c, std::uint32_t* d);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an interpretation.
}  // namespace lumora::farey
