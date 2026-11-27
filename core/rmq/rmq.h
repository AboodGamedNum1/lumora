// Range minimum queries over an array of 32-bit values with a sparse table kept in caller storage. Pure integer: no floating point, no libm,
// no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::rmq {

constexpr std::uint32_t kMaxLen = 1u << 16;  // largest array length

enum class RmqResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutOfRange = 4 };

// Table size in u32 words for an array of n values: n * (floor(log2(n)) + 1) when n is above 0, else 0 (n 1 gives 1, n 2 gives 4, n 65536
// gives 65536 * 17). *out_words = that value. Check order: a null out_words gives NullArgument; then n above kMaxLen gives TooLarge (nothing
// written).
RmqResult rmq_table_words(std::uint32_t n, std::uint32_t* out_words);

// Builds the sparse table for a[0 .. n) into table[0 .. need) where need is the value of rmq_table_words. The table contents are UNSPECIFIED:
// they may be used only through rmq_query below, and the array a must not change between the build and the queries.
// Check order: a null pointer argument (a and table only while n is above 0) gives NullArgument; then n above kMaxLen gives TooLarge; then
// table_words below the need gives ScratchTooSmall. On any failure the table is left untouched. n 0 succeeds with nothing written.
RmqResult rmq_build(const std::uint32_t* a, std::uint32_t n, std::uint32_t* table, std::uint32_t table_words);

// *out_index = the index of the SMALLEST value (unsigned comparison) among a[l .. r], both ends included; when several positions hold that
// value, the LEFTMOST of them. The table must come from rmq_build of the same, unchanged array with the same n; a table that was never built
// or an array that changed gives unspecified results and must not be tested.
// Check order: a null pointer argument (a and table always, out_index always; a query on an empty array fails with OutOfRange, but the
// pointers are still checked first) gives NullArgument; then n above kMaxLen gives TooLarge; then table_words below the need gives
// ScratchTooSmall; then l above r, or r not below n (so every query when n is 0), gives OutOfRange. On any failure *out_index is left
// untouched. The query takes constant time and reads at most the two table rows it needs.
RmqResult rmq_query(const std::uint32_t* a, std::uint32_t n, const std::uint32_t* table, std::uint32_t table_words, std::uint32_t l,
                    std::uint32_t r, std::uint32_t* out_index);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::rmq
