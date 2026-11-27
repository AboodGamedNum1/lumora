// Lumora strict Base64 (scope AK). Integer only, no heap, no recursion, no mutable statics. See docs/base64.md.
#include "base64.h"

namespace lumora::base64 {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kPad = '=';

// Value 0..63 of an alphabet character, or -1 for anything else (the byte is compared unsigned).
int sextet(char c) {
  const unsigned u = static_cast<unsigned char>(c);
  if (u >= 'A' && u <= 'Z') return static_cast<int>(u - 'A');
  if (u >= 'a' && u <= 'z') return static_cast<int>(u - 'a') + 26;
  if (u >= '0' && u <= '9') return static_cast<int>(u - '0') + 52;
  if (u == '+') return 62;
  if (u == '/') return 63;
  return -1;
}

}  // namespace

Base64Result encode(const std::uint8_t* src, u32 n, char* dst, u32 dst_cap, u32* out_len) {
  if (out_len == nullptr || (src == nullptr && n > 0) || (dst == nullptr && dst_cap > 0)) return Base64Result::NullArgument;
  if (n > kMaxInput) return Base64Result::TooLarge;
  const u64 need = 4ull * ((static_cast<u64>(n) + 2) / 3);
  if (need > dst_cap) return Base64Result::BufferTooSmall;
  u32 w = 0;
  u32 i = 0;
  for (; i + 3 <= n; i += 3) {
    const u32 v = (static_cast<u32>(src[i]) << 16) | (static_cast<u32>(src[i + 1]) << 8) | src[i + 2];
    dst[w] = kAlphabet[(v >> 18) & 63u];
    dst[w + 1] = kAlphabet[(v >> 12) & 63u];
    dst[w + 2] = kAlphabet[(v >> 6) & 63u];
    dst[w + 3] = kAlphabet[v & 63u];
    w += 4;
  }
  if (n - i == 1) {
    const u32 v = static_cast<u32>(src[i]) << 16;
    dst[w] = kAlphabet[(v >> 18) & 63u];
    dst[w + 1] = kAlphabet[(v >> 12) & 63u];
    dst[w + 2] = kPad;
    dst[w + 3] = kPad;
    w += 4;
  } else if (n - i == 2) {
    const u32 v = (static_cast<u32>(src[i]) << 16) | (static_cast<u32>(src[i + 1]) << 8);
    dst[w] = kAlphabet[(v >> 18) & 63u];
    dst[w + 1] = kAlphabet[(v >> 12) & 63u];
    dst[w + 2] = kAlphabet[(v >> 6) & 63u];
    dst[w + 3] = kPad;
    w += 4;
  }
  *out_len = w;
  return Base64Result::Ok;
}

Base64Result decode(const char* src, u32 n, std::uint8_t* dst, u32 dst_cap, u32* out_len) {
  if (out_len == nullptr || (src == nullptr && n > 0) || (dst == nullptr && dst_cap > 0)) return Base64Result::NullArgument;
  if (n % 4 != 0) return Base64Result::Malformed;
  if (n > kMaxEncoded) return Base64Result::TooLarge;
  const u32 groups = n / 4;
  u32 pad = 0;
  for (u32 g = 0; g < groups; ++g) {
    const char* p = src + static_cast<u64>(g) * 4;
    const bool last = g + 1 == groups;
    for (u32 k = 0; k < 4; ++k) {
      if (sextet(p[k]) >= 0) continue;
      if (!last || p[k] != kPad || k < 2) return Base64Result::Malformed;
      if (k == 2 && p[3] != kPad) return Base64Result::Malformed;
    }
    if (last) {
      if (p[3] == kPad) pad = 1;
      if (p[2] == kPad) pad = 2;
      if (pad == 2 && (sextet(p[1]) & 15) != 0) return Base64Result::NonCanonical;
      if (pad == 1 && (sextet(p[2]) & 3) != 0) return Base64Result::NonCanonical;
    }
  }
  const u64 size = 3ull * groups - pad;
  if (size > dst_cap) return Base64Result::BufferTooSmall;
  u32 w = 0;
  for (u32 g = 0; g < groups; ++g) {
    const char* p = src + static_cast<u64>(g) * 4;
    const int a = sextet(p[0]);
    const int b = sextet(p[1]);
    const int c = p[2] == kPad ? 0 : sextet(p[2]);
    const int d = p[3] == kPad ? 0 : sextet(p[3]);
    const u32 v = (static_cast<u32>(a) << 18) | (static_cast<u32>(b) << 12) | (static_cast<u32>(c) << 6) | static_cast<u32>(d);
    const u32 count = (g + 1 == groups) ? 3 - pad : 3;
    if (count >= 1) dst[w] = static_cast<std::uint8_t>(v >> 16);
    if (count >= 2) dst[w + 1] = static_cast<std::uint8_t>((v >> 8) & 255u);
    if (count >= 3) dst[w + 2] = static_cast<std::uint8_t>(v & 255u);
    w += count;
  }
  *out_len = w;
  return Base64Result::Ok;
}

}  // namespace lumora::base64
