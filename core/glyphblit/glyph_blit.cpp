// Lumora glyph blit (scope AE). Integer only, no heap, no recursion, no mutable statics. See docs/glyph_blit.md.
#include <cstddef>
#include "glyph_blit.h"

namespace lumora::glyphblit {

namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kBasis = 0xcbf29ce484222325ull;
constexpr u64 kPrime = 0x100000001b3ull;

u64 fnv_byte(u64 h, u32 b) { return (h ^ (b & 0xFFu)) * kPrime; }

u64 fnv_word(u64 h, u32 v) {
  for (int k = 0; k < 4; ++k) h = fnv_byte(h, v >> (8 * k));
  return h;
}

bool dim_ok(u32 v) { return v != 0 && v <= kMaxDim; }

}  // namespace

BlitResult blit_coverage(const u8* src, u32 w, u32 h, u32 src_stride, u8* dst, u32 dst_w, u32 dst_h, u32 dst_stride, u32 x, u32 y) {
  if (src == nullptr || dst == nullptr) return BlitResult::NullArgument;
  if (!dim_ok(w) || !dim_ok(h) || !dim_ok(dst_w) || !dim_ok(dst_h)) return BlitResult::BadSize;
  if (src_stride < w || dst_stride < dst_w || src_stride > kMaxStride || dst_stride > kMaxStride) return BlitResult::BadStride;
  if (static_cast<u64>(x) + w > dst_w || static_cast<u64>(y) + h > dst_h) return BlitResult::OutOfBounds;

  const u64 avail_w = static_cast<u64>(dst_w) - x;
  const u64 avail_h = static_cast<u64>(dst_h) - y;
  const u64 wp = (static_cast<u64>(w) + 1 < avail_w) ? static_cast<u64>(w) + 1 : avail_w;
  const u64 hp = (static_cast<u64>(h) + 1 < avail_h) ? static_cast<u64>(h) + 1 : avail_h;
  const u64 src_begin = reinterpret_cast<std::uintptr_t>(src);
  const u64 src_end = src_begin + static_cast<u64>(h - 1) * src_stride + w;
  const u64 dst_begin = reinterpret_cast<std::uintptr_t>(dst) + static_cast<u64>(y) * dst_stride + x;
  const u64 dst_end = reinterpret_cast<std::uintptr_t>(dst) + (static_cast<u64>(y) + hp - 1) * dst_stride + x + wp;
  if (src_begin < dst_end && dst_begin < src_end) return BlitResult::Overlap;

  for (u32 r = 0; r < h; ++r) {
    const u8* from = src + static_cast<std::size_t>(r) * src_stride;
    u8* to = dst + (static_cast<std::size_t>(y) + r) * dst_stride + x;
    for (u32 c = 0; c < w; ++c) to[c] = from[c];
    if (static_cast<u64>(x) + w < dst_w) to[w] = 0;
  }
  if (static_cast<u64>(y) + h < dst_h) {
    u8* row = dst + (static_cast<std::size_t>(y) + h) * dst_stride + x;
    for (u64 c = 0; c < wp; ++c) row[c] = 0;
  }
  return BlitResult::Ok;
}

std::uint64_t image_digest(const u8* pixels, u32 w, u32 h, u32 stride) {
  u64 hash = kBasis;
  if (pixels == nullptr || !dim_ok(w) || !dim_ok(h) || stride < w || stride > kMaxStride) return hash;
  hash = fnv_word(hash, w);
  hash = fnv_word(hash, h);
  for (u32 r = 0; r < h; ++r) {
    const u8* row = pixels + static_cast<std::size_t>(r) * stride;
    for (u32 c = 0; c < w; ++c) hash = fnv_byte(hash, row[c]);
  }
  return hash;
}

}  // namespace lumora::glyphblit
