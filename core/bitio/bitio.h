// LSB-first bit reading and writing over a caller buffer (compact binary records). Pure integer: no floating point, no libm, no 128-bit
// types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::bitio {

constexpr std::uint32_t kMaxBytes = 1u << 28;  // largest buffer, in bytes, so that 8 * bytes fits in 32 bits

// Bit position p (0-based, counted from the start of the buffer) is bit (p % 8) of byte (p / 8), where bit 0 is the least significant.
// A field of n bits at position p holds value bit 0 at position p, value bit 1 at position p + 1, and so on (LSB first). Every function
// takes the buffer, its size in bytes (cap) and a position that it reads and advances. The position is valid when it is at most 8 * cap and
// cap is at most kMaxBytes; an invalid position or cap, a null buffer or a null position makes every function below fail (false, or 0
// from bits_left) and change nothing. A buffer pointer is never read or written when the call fails.

// Writes the low nbits of value (nbits 0 to 32; bits of value above nbits are ignored) at *pos and advances *pos by nbits. Fails when
// nbits is above 32 or *pos + nbits (64-bit) is above 8 * cap. The bits written replace the old ones; every other bit of the buffer,
// including the other bits of the touched bytes, keeps its value. nbits 0 succeeds and changes nothing.
bool write_bits(std::uint8_t* buf, std::uint32_t cap, std::uint32_t* pos, std::uint32_t value, std::uint32_t nbits);

// Reads nbits (0 to 32) at *pos into *out and advances *pos by nbits. Fails when out is null, nbits is above 32, or *pos + nbits (64-bit)
// is above 8 * cap; then *out and *pos are unchanged. nbits 0 gives *out = 0 and succeeds. The result has only its low nbits set.
bool read_bits(const std::uint8_t* buf, std::uint32_t cap, std::uint32_t* pos, std::uint32_t nbits, std::uint32_t* out);

// Moves *pos up to the next multiple of 8 (unchanged when it is already one). The skipped bits are not touched. Fails (changing nothing)
// for a null position or an invalid position or cap; it cannot run past the end because 8 * cap is a multiple of 8. buf is not used.
bool align_to_byte(std::uint32_t cap, std::uint32_t* pos);

// 8 * cap - *pos, or 0 for a null position or an invalid position or cap.
std::uint32_t bits_left(std::uint32_t cap, const std::uint32_t* pos);

// Decisions: the buffer is never accessed past byte cap - 1, and reads touch only the bytes that hold the field. An overlap of buf with
// pos or out is not promised, not checked and not tested. A header ambiguity found later is a question to the maintainer, not an
// interpretation.

}  // namespace lumora::bitio
