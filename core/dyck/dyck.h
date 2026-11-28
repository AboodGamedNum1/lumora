// Ranking of Dyck words (balanced parenthesis strings) of semilength n, n at most 32, stored as u64 bitmasks. A word of 2n characters is the mask whose
// most significant used bit (bit 2n - 1) is the first character and whose bit 0 is the last; the character '(' is the bit 0 and ')' is the bit 1. So the
// mask 0b000111 is "((()))" and 0b010101 is "()()()". The words are ranked in lexicographic order with '(' before ')', which is the numeric order of the
// masks: the words of semilength n get the ranks 0 to C_n - 1 with no gaps, C_n the n-th Catalan number (the largest, C_32 = 55534064877048198, fits a u64).
// A mask is valid when it has no bit at position 2n or above and every prefix has at least as many '(' as ')' and the totals are equal.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no static tables (compute the counts). Cost: O(n) steps per call plus the ballot counts (a few thousand elementary steps at most); a loop over
// all words or all ranks is not acceptable.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::dyck {
constexpr std::uint32_t kMaxN = 32u;
enum class DyResult : std::uint8_t { Ok = 0, NullArgument = 1, BadArgument = 2, BadMask = 3, RankTooLarge = 4, NoNext = 5 };
// Check order for every function: a null output pointer gives NullArgument; then n above 32 gives BadArgument; then (functions with a mask) an invalid mask
// gives BadMask; then (dyck_unrank) a rank of C_n or more gives RankTooLarge; dyck_next gives NoNext when the mask is the last word. Nothing is written to
// *out on a failure. n = 0 is valid: the only word is the empty word, mask 0, C_0 = 1.
//
// dyck_count: *out = C_n. Examples: C_0 = 1, C_1 = 1, C_5 = 42, C_10 = 16796, C_20 = 6564120420, C_31 = 14544636039226909, C_32 = 55534064877048198.
DyResult dyck_count(std::uint32_t n, std::uint64_t* out);
// dyck_rank: *out = the rank of the word. Examples: for n = 3 the five words in rank order 0 to 4 are 0b000111, 0b001011, 0b001101, 0b010011, 0b010101; for
// n = 4 the masks in rank order start 15, 23, 27, 29, 39 and end 83, 85; mask 0b10110011 (n = 5) has rank 21; the mask 0x555555555 (n = 18, "()" repeated)
// has rank C_18 - 1 = 477638699; the mask 0xFFFFFFFF (n = 32, 32 '(' then 32 ')') has rank 0; the mask 0x5555555555555555 (n = 32) has rank C_32 - 1; the
// mask 0 (n = 0) has rank 0. Invalid (BadMask): 0b110 and 0b1001 and 0b0100 for n = 2, any mask with a bit at position 2n or above (0b10000011 for n = 3).
DyResult dyck_rank(std::uint64_t mask, std::uint32_t n, std::uint64_t* out);
// dyck_unrank: *out = the word with the given rank (the inverse of dyck_rank). Examples: (n = 10, rank 0) gives 1023; (n = 8, rank 100) gives 0x7A7; (n = 20,
// rank 1000000) gives 0xBF9F727; (n = 32, rank 10^16) gives 0x08F6C4AB1D384BB7; (n = 32, rank 0) gives 0xFFFFFFFF; (n = 32, rank C_32 - 1) gives
// 0x5555555555555555; (n = 0, rank 0) gives 0; (n = 3, rank 5) is RankTooLarge; (n = 0, rank 1) is RankTooLarge.
DyResult dyck_unrank(std::uint32_t n, std::uint64_t rank, std::uint64_t* out);
// dyck_next: *out = the word with the next rank. Examples: (0b000111, 3) gives 0b001011; (0b001101, 3) gives 0b010011; (0b010101, 3) is NoNext; (0xFFFFFFFF,
// 32) gives 0x17FFFFFFF; (0x5555555555555553, 32) gives 0x5555555555555555; (0x5555555555555555, 32) is NoNext; (0, 0) is NoNext.
DyResult dyck_next(std::uint64_t mask, std::uint32_t n, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::dyck
