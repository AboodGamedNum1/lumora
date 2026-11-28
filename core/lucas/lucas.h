// Lucas sequences modulo m: U_n and V_n for any u64 index n, any i32 parameters P and Q, and any u32 modulus m above 0.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global
// state, no arrays, no tables. A call takes O(log n) steps with O(1) automatic memory (doubling on the bits of n; the multiplication modulo m is a u64 product of
// two residues below m, reduced at once, so nothing wraps for any u32 m); stepping n times is not acceptable for n near 2^64.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::lucas {
constexpr std::uint32_t kMaxModulus = 4294967295u;  // largest modulus m (every u32 above 0)
enum class LResult : std::uint8_t { Ok = 0, NullArgument = 1, BadModulus = 2 };
// The sequences: U_0 = 0, U_1 = 1, U_k = P * U_(k-1) - Q * U_(k-2); V_0 = 2, V_1 = P, V_k = P * V_(k-1) - Q * V_(k-2), over the integers (P and Q may be negative or zero).
// lucas: *u = U_n mod m and *v = V_n mod m, both in [0, m) (the mathematical residue, so a negative integer value gives a positive residue). Check order: a null u or
// a null v gives NullArgument; then m = 0 gives BadModulus. Nothing is written to *u or *v on a failure. m = 1 gives 0 and 0 for every n, P and Q (every residue is 0).
// Examples (n, P, Q, m -> U, V): (0, 1, -1, 1000) -> 0, 2; (1, 1, -1, 1000) -> 1, 1; (10, 1, -1, 1000) -> 55, 123 (Fibonacci and Lucas numbers); (10, 1, -1, 1) -> 0, 0;
// (10, 1, -1, 7) -> 6, 4; (5, 2, -1, 1000) -> 29, 82 (Pell); (7, 3, 2, 1000) -> 127, 129; (7, 3, 2, 5) -> 2, 4; (6, -1, -1, 1000000007) -> 999999999, 18; (9, 0, -1, 1000) -> 1, 0;
// (8, 2, 1, 13) -> 8, 2; (13, 4, 3, 1000000000) -> 797161, 1594324; (20, -3, 2, 998244353) -> 997195778, 1048577; (30, 5, -7, 4294967295) -> 2308723210, 3977004741;
// (1, -5, 3, 7) -> 1, 2; (2, -5, 3, 7) -> 2, 5; (0, 5, 3, 1) -> 0, 0; (25, 1, 1, 30) -> 1, 1; (12, 2147483647, -2147483648, 1000003) -> 987632, 477546;
// (12, -2147483648, 2147483647, 1000003) -> 828650, 263057; (10^18, 1, -1, 1000000007) -> 209783453, 150331332; (2^64 - 1, 3, 2, 4294967295) -> 2147483647, 2147483649;
// (2^64 - 1, -7, 5, 998244353) -> 873880305, 210622387; (2^63, 2, 1, 4294967291) -> 2147483658, 2; (12345678901234567, 1, -1, 4294967291) -> 1216531591, 3395316134;
// (2^64 - 1, 1, -1, 4294967295) -> 2556926665, 1387091969; (10^18, 1, -1, 1) -> 0, 0; a null u or v is NullArgument; m = 0 is BadModulus.
LResult lucas(std::uint64_t n, std::int32_t p, std::int32_t q, std::uint32_t m, std::uint32_t* u, std::uint32_t* v);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an interpretation.
}  // namespace lumora::lucas
