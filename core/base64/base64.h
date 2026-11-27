// Strict Base64 (RFC 4648 standard alphabet with padding) for small embedded blobs. Pure integer: no floating point, no libm, no
// 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::base64 {

constexpr std::uint32_t kMaxInput = 3u << 22;     // largest raw size, in bytes (12582912)
constexpr std::uint32_t kMaxEncoded = 4u << 22;   // largest encoded size, in characters (16777216)

enum class Base64Result : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3, Malformed = 4, NonCanonical = 5 };

// Alphabet: A-Z = 0..25, a-z = 26..51, 0-9 = 52..61, '+' = 62, '/' = 63, padding '='. No line breaks, no whitespace, no URL-safe
// alphabet anywhere.
//
// encode: every 3 raw bytes give 4 characters. A final group of 2 bytes gives 3 characters and one '='; a final group of 1 byte gives 2
// characters and "==". The unused low bits of the last data character are 0. The encoded size is 4 * ceil(n / 3); the empty input gives an
// empty output.
// Check order: out_len null, src null with n > 0, dst null with dst_cap > 0 give NullArgument; n above kMaxInput gives TooLarge; the
// encoded size above dst_cap gives BufferTooSmall. On any failure *out_len and dst are untouched. On Ok exactly *out_len characters of dst
// are written (no terminator) and *out_len is the encoded size.
Base64Result encode(const std::uint8_t* src, std::uint32_t n, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len);

// decode: strict. The input is a sequence of 4-character groups. Check order: the same three NullArgument checks (src null with n > 0,
// dst null with dst_cap > 0, out_len null); n not a multiple of 4 gives Malformed; n above kMaxEncoded gives TooLarge; then the groups are
// scanned from the first to the last and the first problem found wins:
//   - a character outside the alphabet and the padding character, or a padding character anywhere except the last one or two positions
//     of the LAST group, gives Malformed;
//   - in the last group only, "xx==" and "xxx=" are the allowed padded forms ("x=" in position 2, "=" in position 3 alone, or "=x" are
//     Malformed): position 3 may be '=' only when position 4 is also '='; position 4 may be '=' alone;
//   - after the characters of the last group are accepted, nonzero unused low bits (the low 4 bits of the second character with "==",
//     the low 2 bits of the third character with "=") give NonCanonical.
// After the scan a decoded size above dst_cap gives BufferTooSmall. The decoded size is 3 * (n / 4) minus the padding count (0, 1 or 2).
// The scan writes nothing; on any failure *out_len and dst are untouched. On Ok exactly *out_len bytes are written; an empty input gives
// Ok with length 0.
Base64Result decode(const char* src, std::uint32_t n, std::uint8_t* dst, std::uint32_t dst_cap, std::uint32_t* out_len);

// Decisions: src and dst must not overlap; this is not checked and not tested (no aliasing promise). Characters are compared as raw bytes,
// so a negative char value is simply outside the alphabet. A header ambiguity found later is a question to the maintainer, not an
// interpretation.

}  // namespace lumora::base64
