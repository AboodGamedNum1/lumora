// Strict hexadecimal text. See hex.h for the contract. Integer only, no allocation, no global state.
#include "hex.h"

namespace lumora::hex {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Value of one alphabet character, or -1. Compared as an unsigned byte.
int nibble(char c) {
  const unsigned u = static_cast<unsigned char>(c);
  if (u >= '0' && u <= '9') return static_cast<int>(u - '0');
  if (u >= 'a' && u <= 'f') return static_cast<int>(u - 'a') + 10;
  if (u >= 'A' && u <= 'F') return static_cast<int>(u - 'A') + 10;
  return -1;
}

}  // namespace

HexResult encode(const std::uint8_t* src, u32 n, char* dst, u32 dst_cap, u32* out_len, bool upper) {
  if (out_len == nullptr || (src == nullptr && n > 0) || (dst == nullptr && dst_cap > 0)) return HexResult::NullArgument;
  if (n > kMaxInput) return HexResult::TooLarge;
  const u64 need = 2ull * n;
  if (need > dst_cap) return HexResult::BufferTooSmall;
  const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  for (u32 i = 0; i < n; ++i) {
    dst[2ull * i] = digits[src[i] >> 4];
    dst[2ull * i + 1] = digits[src[i] & 15u];
  }
  *out_len = static_cast<u32>(need);
  return HexResult::Ok;
}

HexResult decode(const char* src, u32 n, std::uint8_t* dst, u32 dst_cap, u32* out_len) {
  if (out_len == nullptr || (src == nullptr && n > 0) || (dst == nullptr && dst_cap > 0)) return HexResult::NullArgument;
  if (n > kMaxEncoded) return HexResult::TooLarge;
  if ((n & 1u) != 0) return HexResult::Malformed;
  for (u32 i = 0; i < n; ++i) {
    if (nibble(src[i]) < 0) return HexResult::Malformed;
  }
  const u32 size = n / 2;
  if (size > dst_cap) return HexResult::BufferTooSmall;
  for (u32 i = 0; i < size; ++i) {
    const int hi = nibble(src[2ull * i]);
    const int lo = nibble(src[2ull * i + 1]);
    dst[i] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  *out_len = size;
  return HexResult::Ok;
}

}  // namespace lumora::hex
