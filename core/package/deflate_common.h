// Shared constants for the DEFLATE module (RFC 1951). Internal to core/package.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::package::deflate_detail {

inline constexpr std::size_t kMinMatch = 3;
inline constexpr std::size_t kMaxMatch = 258;
inline constexpr std::size_t kWindowSize = 32768;
inline constexpr std::size_t kMaxStoredBlock = 65535;
inline constexpr int kMaxCodeBits = 15;    // literal/length and distance codes
inline constexpr int kMaxLenCodeBits = 7;  // code-length (bit-length) codes
inline constexpr int kNumLenSymbols = 29;  // length symbols 257..285
inline constexpr int kNumDistSymbols = 30;

// Length symbol bases and extra bits, symbols 257..285 (index 0..28).
inline constexpr std::uint16_t kLenBase[kNumLenSymbols] = {
    3,  4,  5,  6,  7,  8,  9,   10,  11,  13,  15,  17,  19,  23, 27,
    31, 35, 43, 51, 59, 67, 83,  99,  115, 131, 163, 195, 227, 258};
inline constexpr std::uint8_t kLenExtra[kNumLenSymbols] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                                           1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                                           4, 4, 4, 4, 5, 5, 5, 5, 0};

// Distance symbol bases and extra bits, symbols 0..29.
inline constexpr std::uint16_t kDistBase[kNumDistSymbols] = {
    1,    2,    3,    4,    5,    7,    9,    13,   17,   25,
    33,   49,   65,   97,   129,  193,  257,  385,  513,  769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
inline constexpr std::uint8_t kDistExtra[kNumDistSymbols] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// Order in which code-length code lengths are stored (RFC 1951 3.2.7).
inline constexpr std::uint8_t kCodeLengthOrder[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                      11, 4,  12, 3, 13, 2, 14, 1, 15};

// Fixed literal/length code lengths, symbols 0..287 (RFC 1951 3.2.6).
inline constexpr std::uint8_t kFixedLitLen[288] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8};

// Fixed distance code lengths, symbols 0..31 (all 5 bits; 30 and 31 exist in
// the code but are invalid symbols, rejected with BadSymbol when used).
inline constexpr std::uint8_t kFixedDistLen[32] = {
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

// Reverses the low `bits` of `v`. Used to translate between the MSB-first
// canonical codes of RFC 1951 and the LSB-first bit stream.
constexpr std::uint32_t reverse_bits(std::uint32_t v, int bits) {
  std::uint32_t r = 0;
  for (int i = 0; i < bits; ++i) {
    r = (r << 1) | ((v >> i) & 1u);
  }
  return r;
}

// Maps an exact match length (3..258) to its length symbol index 0..28.
constexpr int length_to_symbol(std::size_t len) {
  int sym = kNumLenSymbols - 1;  // 258 maps to the last symbol
  for (int i = 0; i < kNumLenSymbols - 1; ++i) {
    if (len >= kLenBase[i] && len < kLenBase[i + 1]) {
      sym = i;
      break;
    }
  }
  return sym;
}

// Maps an exact distance (1..32768) to its distance symbol index 0..29.
constexpr int dist_to_symbol(std::size_t dist) {
  int sym = 0;
  for (int i = 0; i < kNumDistSymbols; ++i) {
    if (dist >= static_cast<std::size_t>(kDistBase[i])) {
      sym = i;
    }
  }
  return sym;
}

}  // namespace lumora::package::deflate_detail
