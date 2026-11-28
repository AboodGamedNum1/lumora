// Gray codes over u64: the binary reflected Gray code of a u64, and the k-ary reflected Gray code of n digits (base k from 2 to 16, n from 1 to 16) with the
// digits packed four bits each into a u64 (digit i is bits 4i to 4i + 3; digit n - 1 is the most significant and is the slowest changing). In the reflected
// code the ranks 0 to k^n - 1 give codes in which two consecutive codes differ in exactly one digit, by exactly 1 (not modulo k); the code of rank 0 is all zero
// digits. For k = 2 it is the usual binary reflected code, written one bit per nibble. The reflected code of n digits is built from the one of n - 1 digits: the top digit d
// runs 0 to k - 1 and the lower digits follow the (n - 1)-digit sequence forward when d is even and backward when d is odd, so with k = 3, n = 2 the order is
// 0x00 0x01 0x02 0x12 0x11 0x10 0x20 0x21 0x22 and with k = 4, n = 2 it starts 0x00 0x01 0x02 0x03 0x13 0x12 0x11 0x10 0x20 and ends ... 0x30.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no global
// state, no static tables, no arrays. Cost: O(n) steps per call for the k-ary functions (a loop over all ranks or codes is not acceptable), O(64) for the binary ones.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::graycode {
constexpr std::uint32_t kMaxBase = 16u;
constexpr std::uint32_t kMaxDigits = 16u;
enum class GrResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, BadCode = 3, RankTooLarge = 4, NoNext = 5, Overflow = 6 };
// gray_encode: x xor (x >> 1). Examples: 0 gives 0, 1 gives 1, 2 gives 3, 3 gives 2, 0xFFFFFFFFFFFFFFFF gives 0x8000000000000000, 0x123456789ABCDEF0 gives
// 0x1B2E7D44D7E2B188. gray_decode is its inverse for every u64: 0x123456789ABCDEF0 gives 0x1C279BAF132894A0, 0xFFFFFFFFFFFFFFFF gives 0xAAAAAAAAAAAAAAAA,
// 0x8000000000000000 gives 0xFFFFFFFFFFFFFFFF, 3 gives 2. No failure case; both are O(64) at most.
std::uint64_t gray_encode(std::uint64_t x);
std::uint64_t gray_decode(std::uint64_t g);
// Check order for the k-ary functions: a null output pointer gives NullArgument; then k below 2 or above 16, or n of 0 or above 16, gives BadArgument; then
// (functions with a code) a code with a digit of k or more, or any bit at position 4n or above, gives BadCode; then (kgray_unrank) a rank of k^n or more gives
// RankTooLarge; kgray_next gives NoNext when the code is the last. Nothing is written to *out on a failure.
//
// kgray_count: *out = k^n, or Overflow when it is 2^64 or more (only k = 16 with n = 16). Examples: (2, 1) = 2, (3, 2) = 9, (10, 16) = 10^16, (15, 16) =
// 6568408355712890625, (16, 15) = 2^60, (16, 16) is Overflow.
GrResult kgray_count(std::uint32_t k, std::uint32_t n, std::uint64_t* out);
// kgray_unrank: *out = the code of the rank. Examples: (3, 2, rank 0 to 8) gives 0x00, 0x01, 0x02, 0x12, 0x11, 0x10, 0x20, 0x21, 0x22; (4, 2, 15) gives 0x30; (2, 16, 40000)
// gives 0x1101001001100000; (10, 3, 999) gives 0x900; (10, 16, 10^16 - 1) gives 0x9000000000000000; (10, 16, 12345678901234) gives 0x17355371991735; (3, 16,
// 3^16 - 1) gives 0x2222222222222222; (16, 16, 0xFFFFFFFFFFFFFFFF) gives 0xF000000000000000; (16, 16, 0x123456789ABCDEF0) gives 0x1D3B597795B3D1FF; (3, 2, 9) is
// RankTooLarge; (10, 16, 10^16) is RankTooLarge; (16, 16, any rank) never is.
GrResult kgray_unrank(std::uint32_t k, std::uint32_t n, std::uint64_t rank, std::uint64_t* out);
// kgray_rank: *out = the rank of the code (the inverse of kgray_unrank). Examples: (3, 2, 0x12) gives 3; (4, 2, 0x30) gives 15; (2, 16, 0x1101001001100000)
// gives 40000; (10, 16, 0x9000000000000000) gives 10^16 - 1. BadCode: (3, 2, 0x03), (3, 2, 0x100), (2, 3, 0x2), (10, 1, 0xA), (4, 2, 0x40).
GrResult kgray_rank(std::uint32_t k, std::uint32_t n, std::uint64_t code, std::uint64_t* out);
// kgray_next: *out = the code of the next rank; NoNext for the last code. Examples: (3, 2, 0x02) gives 0x12; (4, 2, 0x03) gives 0x13; (3, 2, 0x22) is NoNext; (2, 3,
// 0x100) is NoNext; (16, 16, 0xF000000000000000) is NoNext; (10, 16, 0x9000000000000000) is NoNext; (3, 2, 0x20) gives 0x21.
GrResult kgray_next(std::uint32_t k, std::uint32_t n, std::uint64_t code, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::graycode
