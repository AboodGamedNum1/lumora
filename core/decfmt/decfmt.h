// Decimal text for 64-bit integers: format into a caller buffer, strict parse from bytes. Pure integer: no floating point, no libm, no
// printf family, no strtol family, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label: unit
// tests only.
#pragma once
#include <cstdint>

namespace lumora::decfmt {

constexpr std::uint32_t kMaxParseLen = 1u << 20;  // largest input length accepted by the parse functions, in bytes

enum class DecResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3, Invalid = 4, Overflow = 5 };

// FORMAT: the text is the usual decimal form: ASCII digits '0' to '9' with no leading zeros (the value 0 is the single digit "0"), and a
// leading '-' for a negative i64 (and for no other value, so there is no "-0" and no '+'). No terminator is written, nothing beyond the text
// is written. The text is at most 20 bytes long. *out_len = its length. BufferTooSmall when dst_cap is below the length (nothing written).
// Check order: a null pointer argument (dst counts only while dst_cap is above 0; out_len always) gives NullArgument; then BufferTooSmall.
DecResult format_u64(std::uint64_t value, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len);
DecResult format_i64(std::int64_t value, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len);

// PARSE (strict, whole input, no tolerated extras): the input is src[0 .. n). parse_u64 accepts one or more ASCII digits and nothing else:
// no sign (neither '+' nor '-'), no spaces, no separators, no terminator. Leading zeros are allowed and have no effect ("007" is 7, and
// 30 zeros followed by a digit are fine), so only the VALUE decides overflow. parse_i64 accepts an optional single '-' then one or more
// digits ('+' is invalid; "-0" is valid and gives 0; a lone "-" is invalid). The value must fit: 0 to 2^64 - 1 for u64, -2^63 to 2^63 - 1 for
// i64.
// Check order: a null pointer argument (src counts only while n is above 0; out always) gives NullArgument; n above kMaxParseLen gives
// TooLarge; then the whole input is checked for form: an empty input, a byte that is not allowed, or a missing digit gives Invalid; and only
// when the form is valid does a value out of range give Overflow. So Invalid wins over Overflow, wherever the bad byte is. On any failure
// *out is left untouched.
DecResult parse_u64(const char* src, std::uint32_t n, std::uint64_t* out);
DecResult parse_i64(const char* src, std::uint32_t n, std::int64_t* out);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::decfmt
