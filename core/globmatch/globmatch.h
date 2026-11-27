// Wildcard matching of byte strings: '?' and '*' patterns over raw bytes. Pure integer: no floating point, no libm, no 128-bit types or
// builtins (MSVC), no allocation, no recursion, no global state, no scratch. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::globmatch {

constexpr std::uint32_t kMaxLen = 1u << 16;  // largest length of the pattern and of the text, in bytes

enum class GlobResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BadPattern = 3 };

// Pattern language, over BYTES (values 0 to 255, compared as unsigned char, no locale, no case folding, a NUL byte is an ordinary byte):
//   '?'  matches exactly one byte (any value).
//   '*'  matches any run of bytes, including the empty run.
//   '\\' (backslash) makes the NEXT pattern byte literal, whatever it is (so "\\*" matches the single byte '*'). A backslash that is the last
//        byte of the pattern is BadPattern.
//   any other byte matches itself.
// The whole text must be matched (no partial match, no search). Matching must not take exponential time: it must finish for the largest
// inputs (pattern and text each kMaxLen bytes) in time proportional to their lengths multiplied, with constant extra memory.
// *out_match = true when the text matches the pattern, false otherwise. Empty pattern matches only the empty text; "*" matches everything.
// Check order: a null pointer argument (pattern counts only while pattern_len is above 0, text only while text_len is above 0, out_match
// always) gives NullArgument; then either length above kMaxLen gives TooLarge; then the WHOLE pattern is checked for a trailing lone
// backslash (an odd run of backslashes at the very end) and gives BadPattern whatever the text is. On any failure *out_match is left
// untouched.
GlobResult glob_match(const char* pattern, std::uint32_t pattern_len, const char* text, std::uint32_t text_len, bool* out_match);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::globmatch
