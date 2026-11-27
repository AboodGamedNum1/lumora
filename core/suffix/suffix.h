// Suffix array and longest-common-prefix array of a byte string. Pure integer: no floating point, no libm, no 128-bit types or builtins
// (MSVC), no allocation, no recursion, no global state. Working memory is caller scratch. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::suffix {

constexpr std::uint32_t kMaxLen = 1u << 16;  // largest text length

enum class SufResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadSuffixArray = 4 };

// Bytes are compared as unsigned char values (0 to 255, a NUL byte is an ordinary byte); a proper prefix of a string sorts BEFORE the longer
// string. Scratch contents are unspecified afterwards.

// sa_out[0 .. n) = the start positions 0 .. n-1 of all n suffixes of text[0 .. n), ordered ascending (the suffix at sa_out[0] is the smallest).
// All suffixes are different strings, so the order is unique. Scratch need: 3 * n words when n is above 1, else 0. n 0 writes nothing; n 1
// writes sa_out[0] = 0. The algorithm must not compare suffixes character by character from their starts in a way that is quadratic on a text
// of 65536 equal bytes or on a periodic text (for example a prefix-doubling or induced-sorting algorithm); the tests do not measure time.
// Check order: a null pointer argument (text and sa_out only while n is above 0, scratch only while the need is above 0) gives NullArgument;
// then n above kMaxLen gives TooLarge; then scratch_words below the need gives ScratchTooSmall. On any failure sa_out is left untouched.
SufResult suffix_array_u8(const std::uint8_t* text, std::uint32_t n, std::uint32_t* sa_out, std::uint32_t* scratch,
                          std::uint32_t scratch_words);

// lcp_out[0] = 0 and, for i from 1 to n-1, lcp_out[i] = the length of the longest common prefix of the suffixes starting at sa[i-1] and sa[i].
// sa must hold each of 0 .. n-1 exactly once, else BadSuffixArray (checked after ScratchTooSmall). Whether sa is really the SORTED suffix
// array of text is NOT checked: for a permutation that is not the suffix array of text the values written are unspecified, and tests must not
// rely on them (they may still call it only to check that nothing crashes or reads outside the arrays). Scratch need: n words when n is above 0,
// else 0. Check order: a null pointer argument (text, sa and lcp_out only while n is above 0, scratch only while the need is above 0) gives
// NullArgument; then n above kMaxLen gives TooLarge; then scratch_words below the need gives ScratchTooSmall; then a non-permutation sa gives
// BadSuffixArray. On any failure lcp_out is left untouched. Linear or n log n time.
SufResult lcp_array_u8(const std::uint8_t* text, std::uint32_t n, const std::uint32_t* sa, std::uint32_t* lcp_out, std::uint32_t* scratch,
                       std::uint32_t scratch_words);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::suffix
