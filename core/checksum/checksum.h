// CRC-32 (IEEE 802.3, as in zip and png) and Adler-32 (zlib) over byte ranges, with incremental use. Pure integer: no floating point, no
// libm, no 128-bit types (MSVC), no allocation, no mutable global state (a constant table is allowed). Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::checksum {

// CRC-32: reflected polynomial 0xEDB88320, initial value 0xFFFFFFFF, final xor 0xFFFFFFFF (so crc32 of "123456789" is 0xCBF43926 and of
// the empty input is 0). crc32_update takes the FINISHED value of the earlier part, so parts chain: crc32_update(crc32_update(0, a, na), b,
// nb) equals the crc32 of a followed by b. Starting value 0 means "nothing yet".
// data null with n > 0 reads nothing and returns crc unchanged; n 0 returns crc unchanged whatever data is.
std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data, std::uint32_t n);

// crc32_update(0, data, n).
std::uint32_t crc32(const std::uint8_t* data, std::uint32_t n);

// Adler-32: s1 = low 16 bits, s2 = high 16 bits of the value; both start at 1 and 0 (starting value 1, so adler32 of the empty input is
// 1 and of "Wikipedia" is 0x11E60398). Each byte: s1 = (s1 + byte) mod 65521, s2 = (s2 + s1) mod 65521. adler32_update takes the finished
// value of the earlier part, so parts chain. A starting value whose low or high half is 65521 or more has that half reduced modulo 65521
// before use (decision: no failure). data null with n > 0 reads nothing and returns the value after that reduction; n 0 likewise. The
// result is always a valid value (both halves below 65521).
std::uint32_t adler32_update(std::uint32_t adler, const std::uint8_t* data, std::uint32_t n);

// adler32_update(1, data, n).
std::uint32_t adler32(const std::uint8_t* data, std::uint32_t n);

// Decisions: n may be any value up to 2^32 - 1 (the sums are taken in 64 bits or deferred modulo, never overflowing). Results must not
// depend on how the input is split into parts. A header ambiguity found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::checksum
