// LEB128 variable-length integers and zigzag mapping for compact binary streams (future save state and telemetry records). Pure integer:
// no floating point, no libm, no 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::varint {

constexpr std::uint32_t kMaxBytes = 10;  // longest encoding of a 64-bit value

// Number of bytes of the canonical (shortest) encoding of v: 1 for 0 to 127, up to 10. Seven value bits per byte, least significant group
// first, bit 7 of a byte set when another byte follows.
std::uint32_t encoded_size(std::uint64_t v);

// Writes the canonical encoding of v to out and the byte count to *written. Returns false, writes nothing and leaves *written as it was
// when out or written is null or capacity < encoded_size(v). Bytes beyond the encoding are never touched.
bool encode(std::uint64_t v, std::uint8_t* out, std::uint32_t capacity, std::uint32_t* written);

// Decodes one value from in[0 .. length). Only the canonical form is accepted. Returns false and leaves *value and *consumed as they were
// when: in, value or consumed is null; length is 0; the data ends while a continuation bit is still set (truncated; bytes past length are
// never read); the encoding is longer than kMaxBytes; the 10th byte is above 1 (bits beyond 64); or the last byte is 0 and the encoding has
// more than one byte (non-canonical padding, such as 0x80 0x00). On success writes the value and the number of bytes used (bytes after the
// first complete encoding are not read) and returns true.
bool decode(const std::uint8_t* in, std::uint32_t length, std::uint64_t* value, std::uint32_t* consumed);

// Zigzag mapping of signed to unsigned: 0, -1, 1, -2, 2 map to 0, 1, 2, 3, 4. For v >= 0 the result is 2 * v; for v < 0 it is
// 2 * (-(v + 1)) + 1. INT64_MIN maps to UINT64_MAX and INT64_MAX to UINT64_MAX - 1. Defined by arithmetic, with no shift of a negative
// value and no signed overflow.
std::uint64_t zigzag_encode(std::int64_t v);

// Inverse of zigzag_encode for every uint64 input: even u gives u / 2, odd u gives -(u / 2) - 1 (UINT64_MAX gives INT64_MIN).
std::int64_t zigzag_decode(std::uint64_t u);

// Signed convenience wrappers: encode(zigzag_encode(v), ...) and decode followed by zigzag_decode, with the failure rules above. On a
// failed decode_signed, *value and *consumed are unchanged.
bool encode_signed(std::int64_t v, std::uint8_t* out, std::uint32_t capacity, std::uint32_t* written);
bool decode_signed(const std::uint8_t* in, std::uint32_t length, std::int64_t* value, std::uint32_t* consumed);

// Decisions: decode reads at most kMaxBytes bytes and never more than length. Ten bytes that all have the continuation bit set (even when
// length is exactly 10) are an ordinary false, as is any 10th byte above 1; a 10th byte of exactly 1 is accepted as bit 63. A zero payload
// byte in the middle of an encoding is canonical; only a zero FINAL byte of a multi-byte encoding is padding. Boundaries: 0 and 127 take
// 1 byte, 128 takes 2, values with bit 63 set take 10. decode_signed decodes into temporaries and publishes *value and *consumed only after
// full success. No promise is made, and nothing is checked or tested, about overlap between the buffers and the value, consumed and
// written pointers: callers pass distinct objects. A new ambiguity is a question to the maintainer, not an interpretation.

}  // namespace lumora::varint
