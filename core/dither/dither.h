// Ordered dither for 16-bit to 8-bit color quantization (gradients on 8-bit displays band without it). Pure integer: no floating
// point, no libm, no 128-bit types (MSVC), no allocation, no clock. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::dither {

constexpr std::uint32_t kMaxRow = 1u << 24;  // longest row dither_row_u16 accepts

// Threshold of the 8x8 Bayer matrix at pixel (x, y); only the low 3 bits of x and y are used, so the pattern repeats every 8 pixels
// (including across uint32 wrap-around, since 8 divides 2^32). Result 0..63, each value exactly once per 8x8 tile. With
// d = (x xor y) and 3-bit fields, bits from most to least significant are: d0, y0, d1, y1, d2, y2 (index k is bit k of the field).
// Check values: row y=0 starts 0 32 8 40 2 34 10 42; row y=1 starts 48 16 56 24 50 18 58 26.
std::uint8_t threshold(std::uint32_t x, std::uint32_t y);

// Quantizes v (0..65535) to 0..255 at pixel (x, y). scaled = v * 255 (uint32), q = scaled / 65535, rem = scaled % 65535. The result
// is q + 1 when rem * 128 >= (2 * threshold + 1) * 65535, else q. Hence 0 maps to 0 and 65535 to 255 at every pixel, the result is
// never above 255, it is monotone non-decreasing in v at a fixed pixel, and it differs from q by at most 1.
// Exact tile property (tested): over any aligned 8x8 tile of one constant v the sum of results is 64 * q + (k + 1) / 2 with
// k = rem * 128 / 65535 (integer division both times).
std::uint8_t quantize_u16(std::uint16_t v, std::uint32_t x, std::uint32_t y);

// Quantizes count values of one row: dst[i] = quantize_u16(src[i], x0 + i, y), with x0 + i computed modulo 2^32. count 0 returns true
// and touches nothing, whatever the pointers. Returns false and writes nothing when count > kMaxRow, when src or dst is null with
// count > 0, or when the byte ranges of src (2 * count bytes) and dst (count bytes) overlap. Otherwise writes count bytes, returns true.
// Check order: count 0 first (true, nothing touched), then count > kMaxRow, then null pointers, then overlap; every failure writes
// nothing. Overlap uses the half-open byte ranges [src, src + 2 * count) and [dst, dst + count) compared as uintptr_t addresses: ranges
// that only touch do not overlap, identical addresses with count > 0 do. No alignment check on src. y may be any uint32 (low 3 bits used).
bool dither_row_u16(const std::uint16_t* src, std::uint8_t* dst, std::uint32_t count, std::uint32_t x0, std::uint32_t y);

}  // namespace lumora::dither
