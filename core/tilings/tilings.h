// Domino tilings of a rectangle: the number of ways to cover an h by w grid exactly with 1 by 2 dominoes (each placed either way round), modulo 998244353.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no static tables. Working memory is automatic (stack) memory of fixed size only: at most 16384 u32 words (64 KiB) in total across all local
// arrays, no variable-length arrays. A call takes O(8^h * log w) steps or better (profile transfer matrix and fast powers, or a linear recurrence found
// from it); a method that loops over all w columns is not acceptable for w = kMaxW.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::tilings {
constexpr std::uint32_t kMod = 998244353u;
constexpr std::uint32_t kMaxH = 6u;           // largest height h
constexpr std::uint32_t kMaxW = 4294967295u;  // largest width w (2^32 - 1, every u32)
enum class TileResult : std::uint8_t { Ok = 0, NullArgument = 1, BadHeight = 2 };
// domino_tilings: *out = the number of domino tilings of the grid with h rows and w columns, modulo kMod, in the range 0 .. kMod - 1. w = 0 gives 1 (the
// empty tiling) for every h in range; when h * w is odd the count is 0; h = 1 gives 1 for even w and 0 for odd w; h = 2 gives the Fibonacci numbers
// (w = 1: 1, w = 2: 2, w = 3: 3). The reflection symmetry means domino_tilings(h, w) equals domino_tilings(w, h) whenever both are in range (h, w <= 6).
// Check order: a null out gives NullArgument; then h = 0 or h above kMaxH gives BadHeight (the height is the short side limit; every u32 w is valid).
// Nothing is written to *out on any failure.
TileResult domino_tilings(std::uint32_t h, std::uint32_t w, std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::tilings
