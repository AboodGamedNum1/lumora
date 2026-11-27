// Strict UTF-16 (host-order 16-bit code units, no byte order mark handling): validation, code point count, single code point decode and
// encode. Pure integer: no floating point, no libm, no 128-bit types (MSVC), no wide character types, no allocation, no global state.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::utf16 {

constexpr std::uint32_t kMaxUnits = 1u << 27;  // largest text for validate and count, in 16-bit units
constexpr std::uint32_t kMaxCodePoint = 0x10FFFFu;

enum class Utf16Result : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3, Malformed = 4 };

// Well-formed means: a unit outside D800-DFFF is one code point by itself; a high surrogate D800-DBFF must be followed immediately by a low
// surrogate DC00-DFFF and the pair is one code point 0x10000 + ((high - D800) << 10) + (low - DC00); a lone low surrogate, a lone high
// surrogate, and a high surrogate at the end of the text are malformed. U+0000, U+FEFF and the noncharacters are ordinary valid code points.

// validate: s null with n > 0 gives NullArgument; n above kMaxUnits gives TooLarge (nothing is read); then the text is scanned from the
// start and the first ill-formed sequence gives Malformed. bad_offset may be null. On Malformed *bad_offset is the index of the first unit
// of the first ill-formed sequence (the high surrogate that lacks a low one, or the lone low surrogate itself). On Ok *bad_offset = n. On
// NullArgument and TooLarge *bad_offset is untouched. Empty text is Ok.
Utf16Result validate(const std::uint16_t* s, std::uint32_t n, std::uint32_t* bad_offset);

// count_code_points: the number of code points in a well-formed text into *out. Same argument checks and errors as validate (out null
// gives NullArgument, checked together with s). On Malformed *out is untouched. Empty text gives 0.
Utf16Result count_code_points(const std::uint16_t* s, std::uint32_t n, std::uint32_t* out);

// decode_one: decodes the sequence that starts at s[0]. s null, cp null or len null gives NullArgument; n == 0 gives Malformed. At most
// min(n, 2) units are read. An ill-formed or cut-off sequence gives Malformed. On Ok *cp is the code point and *len is 1 or 2. On any
// failure *cp and *len are untouched.
Utf16Result decode_one(const std::uint16_t* s, std::uint32_t n, std::uint32_t* cp, std::uint32_t* len);

// encode_one: dst null with dst_cap > 0, or len null, gives NullArgument; then cp above kMaxCodePoint or in D800-DFFF gives Malformed; then
// a needed length (1 unit up to FFFF, else 2) above dst_cap gives BufferTooSmall (dst may be null with dst_cap 0 here). On Ok exactly *len
// units are written. On any failure dst and *len are untouched.
Utf16Result encode_one(std::uint32_t cp, std::uint16_t* dst, std::uint32_t dst_cap, std::uint32_t* len);

// Decisions: encode_one of any value that decode_one accepts reproduces the same units. A header ambiguity found later is a question to the
// maintainer, not an interpretation.

}  // namespace lumora::utf16
