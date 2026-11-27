// Coin tables over a list of coin values: the fewest coins for every amount, the number of ways to make every amount with unlimited coins, and the
// number of subsets of the coin list that sum to every amount.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (the output table is the only working memory). Each function takes O(m * n) steps for m coins and amounts
// 0 .. n. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::coins {
constexpr std::uint32_t kMod = 998244353u;      // modulus of the counting tables
constexpr std::uint32_t kMaxCoins = 64u;        // largest number of coins m
constexpr std::uint32_t kMaxAmount = 1048576u;  // largest amount n (2^20); the tables hold n + 1 entries
constexpr std::uint32_t kUnreachable = 0xFFFFFFFFu;
enum class CoinsResult : std::uint8_t { Ok = 0, NullArgument = 1, TooManyCoins = 2, TooLarge = 3, BadCoin = 4 };
// coins[0 .. m-1] are the coin values: any u32 except 0 (a value above n is legal and simply never used). The same value may appear more than once;
// every list position is its own coin KIND. out holds n + 1 entries (index = amount); on any failure nothing is written. m = 0 is legal (no coins).
// Check order for all three functions: a null out (always) or a null coins (only while m is above 0) gives NullArgument; then m above kMaxCoins gives
// TooManyCoins; then n above kMaxAmount gives TooLarge; then any coin equal to 0 gives BadCoin (every coin is scanned, whatever n is).
//
// coins_min_count: out[v] = the fewest coins (each kind usable any number of times) whose values sum to v exactly, or kUnreachable when v cannot be
// made. out[0] = 0 (always, also with m = 0).
CoinsResult coins_min_count(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, std::uint32_t* out);
// coins_ways: out[v] = the number of multisets over the m coin KINDS (each kind usable any number of times, order ignored) whose values sum to v,
// modulo kMod. Two kinds with the same value count separately (coins {1, 1} give 3 ways to make 2: two of the first, two of the second, one each).
// out[0] = 1 (the empty multiset, also with m = 0).
CoinsResult coins_ways(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, std::uint32_t* out);
// coins_subset_ways: out[v] = the number of subsets of the m list POSITIONS (each coin at most once) whose values sum to v, modulo kMod. out[0] counts
// the empty subset, so it is 1 plus the number of non-empty subsets that sum to 0, which is none because coins are above 0: out[0] = 1.
CoinsResult coins_subset_ways(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::coins
