// Strict hexadecimal text for small binary blobs (digests, ids). Pure integer: no floating point, no libm, no 128-bit types (MSVC),
// no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::hex {

constexpr std::uint32_t kMaxInput = 1u << 27;     // largest raw size, in bytes (134217728)
constexpr std::uint32_t kMaxEncoded = 1u << 28;   // largest encoded size, in characters (268435456)

enum class HexResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3, Malformed = 4 };

// Alphabet: '0'-'9' = 0..9, 'a'-'f' and 'A'-'F' = 10..15. Each byte is two characters, high nibble first. No prefix, no separators, no
// whitespace anywhere.
//
// encode: writes 2 * n characters, lowercase when upper is false and uppercase when it is true; no terminator. The empty input gives an
// empty output. Check order: out_len null, src null with n > 0, dst null with dst_cap > 0 give NullArgument; n above kMaxInput gives
// TooLarge; 2 * n above dst_cap gives BufferTooSmall. On any failure *out_len and dst are untouched. On Ok exactly *out_len characters
// are written and *out_len = 2 * n.
HexResult encode(const std::uint8_t* src, std::uint32_t n, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len, bool upper);

// decode: strict. Both cases are accepted and may be mixed ("aB" is valid). Check order: the same three NullArgument checks (src null with
// n > 0, dst null with dst_cap > 0, out_len null); n above kMaxEncoded gives TooLarge; n odd gives Malformed; then every character is
// scanned from first to last and any character outside the alphabet gives Malformed (nothing is written during the scan); then n / 2 above
// dst_cap gives BufferTooSmall. On any failure *out_len and dst are untouched. On Ok exactly n / 2 bytes are written and *out_len = n / 2.
// An empty input gives Ok with length 0. Note the order: an odd n above kMaxEncoded is TooLarge, an odd n at or below it is Malformed.
HexResult decode(const char* src, std::uint32_t n, std::uint8_t* dst, std::uint32_t dst_cap, std::uint32_t* out_len);

// Decisions: src and dst must not overlap; not checked and not tested (no aliasing promise). Characters are compared as raw bytes, so a
// negative char value is simply outside the alphabet. A header ambiguity found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::hex
