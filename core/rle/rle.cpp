// Lumora run-length coding (scope AJ). Integer only, no heap, no recursion, no mutable statics. See docs/rle.md.
#include "rle.h"

namespace lumora::rle {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kFullRun = 256;

// Length of the maximal run that starts at src[i], cut at 256 bytes.
u32 run_at(const std::uint8_t* src, u32 n, u32 i) {
  u32 len = 1;
  while (i + len < n && len < kFullRun && src[i + len] == src[i]) ++len;
  return len;
}

}  // namespace

RleResult encode(const std::uint8_t* src, u32 n, std::uint8_t* dst, u32 dst_cap, u32* out_len) {
  if (out_len == nullptr || (src == nullptr && n > 0) || (dst == nullptr && dst_cap > 0)) return RleResult::NullArgument;
  if (n > kMaxInput) return RleResult::TooLarge;
  u64 need = 0;
  for (u32 i = 0; i < n;) {
    i += run_at(src, n, i);
    need += 2;
  }
  if (need > dst_cap) return RleResult::BufferTooSmall;
  u32 w = 0;
  for (u32 i = 0; i < n;) {
    const u32 len = run_at(src, n, i);
    dst[w] = static_cast<std::uint8_t>(len - 1);
    dst[w + 1] = src[i];
    w += 2;
    i += len;
  }
  *out_len = w;
  return RleResult::Ok;
}

RleResult decode(const std::uint8_t* src, u32 n, std::uint8_t* dst, u32 dst_cap, u32* out_len) {
  if (out_len == nullptr || (src == nullptr && n > 0) || (dst == nullptr && dst_cap > 0)) return RleResult::NullArgument;
  if (n % 2 != 0) return RleResult::Malformed;
  if (n > kMaxEncoded) return RleResult::TooLarge;
  u64 raw = 0;
  for (u32 i = 0; i < n; i += 2) {
    if (i > 0 && src[i + 1] == src[i - 1] && src[i - 2] != 255) return RleResult::NonCanonical;
    raw += static_cast<u64>(src[i]) + 1;
    if (raw > kMaxInput) return RleResult::TooLarge;
  }
  if (raw > dst_cap) return RleResult::BufferTooSmall;
  u32 w = 0;
  for (u32 i = 0; i < n; i += 2) {
    const u32 len = static_cast<u32>(src[i]) + 1;
    for (u32 k = 0; k < len; ++k) dst[w + k] = src[i + 1];
    w += len;
  }
  *out_len = w;
  return RleResult::Ok;
}

}  // namespace lumora::rle
