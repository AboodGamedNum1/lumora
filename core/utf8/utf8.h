// Strict UTF-8 (RFC 3629, Unicode well-formed byte sequences) validation and single code point decode and encode. Pure integer: no
// floating point, no libm, no 128-bit types (MSVC), no wide character types, no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::utf8 {

constexpr std::uint32_t kMaxInput = 1u << 28;  // largest text for validate, in bytes
constexpr std::uint32_t kMaxCodePoint = 0x10FFFFu;

enum class Utf8Result : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3, Malformed = 4 };

// Well-formed means exactly the Unicode table of well-formed UTF-8 sequences: 1 byte 00-7F; 2 bytes C2-DF then 80-BF; 3 bytes E0 then A0-BF
// then 80-BF, E1-EC or EE-EF then 80-BF twice, ED then 80-9F then 80-BF; 4 bytes F0 then 90-BF then 80-BF twice, F1-F3 then 80-BF three
// times, F4 then 80-8F then 80-BF twice. So overlong forms, the surrogates U+D800 to U+DFFF, values above U+10FFFF, a lone continuation
// byte, and the bytes C0, C1 and F5-FF are all malformed. U+0000, U+FEFF and the noncharacters are ordinary valid code points. No byte
// order mark handling.

// validate: s null with n > 0 gives NullArgument; n above kMaxInput gives TooLarge (nothing is read); then the text is scanned from the
// start and the first ill-formed sequence gives Malformed. bad_offset may be null. On Malformed *bad_offset is the index of the first
// byte of the first ill-formed sequence: the lead byte when a lead byte is followed by a missing or wrong continuation byte (a
// sequence cut off by the end of the text counts as ill-formed at its lead byte), or the byte itself when it is a continuation or an
// invalid lead. On Ok *bad_offset = n. On NullArgument and TooLarge *bad_offset is untouched. Empty text is Ok.
Utf8Result validate(const std::uint8_t* s, std::uint32_t n, std::uint32_t* bad_offset);

// decode_one: decodes the sequence that starts at s[0]. s null, cp null or len null gives NullArgument; n == 0 gives Malformed. At most
// min(n, 4) bytes are read. Ill-formed or cut-off sequences give Malformed. On Ok *cp is the code point and *len is 1 to 4. On any
// failure *cp and *len are untouched.
Utf8Result decode_one(const std::uint8_t* s, std::uint32_t n, std::uint32_t* cp, std::uint32_t* len);

// encode_one: dst null with dst_cap > 0, or len null, gives NullArgument; then cp above kMaxCodePoint or in D800-DFFF gives Malformed; then
// a needed length (1 for up to 7F, 2 up to 7FF, 3 up to FFFF, else 4) above dst_cap gives BufferTooSmall (dst may be null with dst_cap 0
// here). On Ok exactly *len bytes are written, shortest form. On any failure dst and *len are untouched.
Utf8Result encode_one(std::uint32_t cp, std::uint8_t* dst, std::uint32_t dst_cap, std::uint32_t* len);

// Decisions: encode_one of any value that decode_one accepts reproduces the same bytes. A header ambiguity found later is a question to
// the maintainer, not an interpretation.

}  // namespace lumora::utf8
