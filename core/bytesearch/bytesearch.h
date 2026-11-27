// Search and comparison on byte strings: first and last occurrence of a needle, count of non-overlapping occurrences, ASCII case-insensitive
// equality. Pure integer: no floating point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::bytesearch {

constexpr std::uint32_t kMaxLen = 1u << 28;  // largest length of any input string, in bytes

enum class SearchResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, EmptyNeedle = 3 };

// A string is s[0 .. n). Bytes are compared as unsigned values. An occurrence of needle in hay at position p means p + nn <= nh and
// hay[p + i] == needle[i] for every i below nn. Matches may overlap each other for find and find_last (they report one position only).
//
// Check order for every function: a null pointer argument gives NullArgument (a string pointer counts only while its length is above 0; every
// out pointer always counts); then a length above kMaxLen gives TooLarge; then EmptyNeedle where it applies. On any failure nothing is
// written (out pointers are left untouched). With a length of 0 nothing is read from that string.

// Finds the smallest position p of an occurrence. *out_found tells whether there is one; *out_pos is written only when found (and is left
// untouched when not found). An empty needle (nn 0) is found at position 0, even in an empty haystack.
SearchResult bytes_find(const std::uint8_t* hay, std::uint32_t nh, const std::uint8_t* needle, std::uint32_t nn, std::uint32_t* out_pos,
                        bool* out_found);

// Finds the largest position p of an occurrence, with the same out rules. An empty needle is found at position nh (the end).
SearchResult bytes_find_last(const std::uint8_t* hay, std::uint32_t nh, const std::uint8_t* needle, std::uint32_t nn, std::uint32_t* out_pos,
                             bool* out_found);

// Counts occurrences from left to right without overlap: after a match at p the next search starts at p + nn. EmptyNeedle when nn is 0 (after
// the null and size checks). *out_count = the count (0 when none).
SearchResult bytes_count(const std::uint8_t* hay, std::uint32_t nh, const std::uint8_t* needle, std::uint32_t nn, std::uint32_t* out_count);

// *out_equal = true when na == nb and the strings are equal after folding the ASCII letters A to Z to a to z (no other byte changes; bytes
// 128 and above are compared as they are). Two empty strings are equal.
SearchResult bytes_equal_ascii_nocase(const std::uint8_t* a, std::uint32_t na, const std::uint8_t* b, std::uint32_t nb, bool* out_equal);

// Decisions: the algorithm is free as long as the contract above holds (naive, KMP and others give identical results). A header ambiguity
// found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::bytesearch
