// Ranking of k-subsets of {0, ..., n - 1} (n at most 64) stored as u64 bitmasks, in the combinatorial number system. The order is the numeric order of
// the masks (equivalently colex order): the subsets with c_1 < c_2 < ... < c_k have rank C(c_1, 1) + C(c_2, 2) + ... + C(c_k, k), and the k-subsets of n
// elements get the ranks 0 to C(n, k) - 1 with no gaps. Every C(n, k) for n at most 64 fits a u64 (the largest is C(64, 32) = 1832624140942590534).
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no static tables (compute the binomials). Every call takes O(n * k) steps or fewer.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::combrank {
constexpr std::uint32_t kMaxN = 64u;
enum class CbResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, BadMask = 3, RankTooLarge = 4, NoNext = 5 };
// Check order for every function: a null output pointer gives NullArgument; then n above 64 or k above n gives BadArgument; then (functions with a
// mask) a mask with a bit at position n or above, or with a popcount different from k, gives BadMask; then (combo_unrank) a rank of C(n, k) or more gives
// RankTooLarge; combo_next gives NoNext when the mask is the last of its kind. Nothing is written to *out on a failure. n = 0 and k = 0 are valid: the only
// 0-subset is the mask 0 (C(n, 0) = 1) and C(0, 0) = 1.
//
// combo_count: *out = C(n, k). Examples: C(0, 0) = 1, C(5, 2) = 10, C(10, 4) = 210, C(64, 0) = 1, C(64, 1) = 64, C(64, 2) = 2016, C(64, 16) = 488526937079580,
// C(64, 32) = 1832624140942590534, C(63, 31) = 916312070471295267, C(64, 64) = 1.
CbResult combo_count(std::uint32_t n, std::uint32_t k, std::uint64_t* out);
// combo_rank: *out = the rank of mask among the k-subsets of n elements. Examples: for n = 5, k = 2 the ten masks in rank order 0 to 9 are 0b00011, 0b00101,
// 0b00110, 0b01001, 0b01010, 0b01100, 0b10001, 0b10010, 0b10100, 0b11000; mask 0b101101 (n = 6, k = 4) has rank 0 + 1 + 1 + 5 = 7; the mask 1 << 63 (n = 64,
// k = 1) has rank 63; the mask 0xFFFFFFFF00000000 (n = 64, k = 32) has rank C(64, 32) - 1; the mask 0 (k = 0) has rank 0; the mask of all 64 bits (n = 64,
// k = 64) has rank 0.
CbResult combo_rank(std::uint64_t mask, std::uint32_t n, std::uint32_t k, std::uint64_t* out);
// combo_unrank: *out = the mask of the k-subset of n elements with the given rank (the inverse of combo_rank). Examples: (n = 10, k = 4, rank 100) gives
// 353 (0b101100001); (n = 64, k = 2, rank 2015) gives 0xC000000000000000; (n = 64, k = 32, rank C(64, 32) - 1) gives 0xFFFFFFFF00000000; (n = 64, k = 10,
// rank 100000000000) gives 0x2211000002221101; (n = 5, k = 2, rank 10) is RankTooLarge.
CbResult combo_unrank(std::uint32_t n, std::uint32_t k, std::uint64_t rank, std::uint64_t* out);
// combo_next: *out = the mask with the next rank (the next larger mask with the same popcount inside n bits); the last mask gives NoNext, and so does every
// call with k = 0. Examples: (0b00011, n = 5, k = 2) gives 0b00101; (0b01100, 5, 2) gives 0b10001; (0b11000, 5, 2) is NoNext; (1 << 62, 64, 1) gives 1 << 63;
// (1 << 63, 64, 1) is NoNext; (0x7FFFFFFFFFFFFFFF, 64, 63) gives 0xBFFFFFFFFFFFFFFF; (0xFFFFFFFF00000000, 64, 32) is NoNext; (0xFFFFFFFF, 64, 32) gives
// 0x17FFFFFFF. The step of "add the lowest set bit" must not wrap for n = 64.
CbResult combo_next(std::uint64_t mask, std::uint32_t n, std::uint32_t k, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::combrank
