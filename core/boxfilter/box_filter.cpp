// Lumora box-filter downscale (scope AG). Integer only, no heap, no recursion, no mutable statics. See docs/box_filter.md.
#include <cstddef>

#include "box_filter.h"

namespace lumora::boxfilter {

namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

}  // namespace

FilterResult box_downscale(const u8* src, u32 w, u32 h, u32 src_stride, u32 factor, u8* dst, u32 dst_stride, u32* out_w, u32* out_h) {
  if (src == nullptr || dst == nullptr || out_w == nullptr || out_h == nullptr) return FilterResult::NullArgument;
  if (w == 0 || w > kMaxDim || h == 0 || h > kMaxDim) return FilterResult::BadSize;
  if (src_stride < w || src_stride > kMaxStride) return FilterResult::BadStride;
  if (factor == 0 || factor > kMaxFactor || factor > w || factor > h) return FilterResult::BadFactor;
  const u32 ow = w / factor;
  const u32 oh = h / factor;
  if (dst_stride < ow || dst_stride > kMaxStride) return FilterResult::BadStride;

  const u64 src_begin = reinterpret_cast<std::uintptr_t>(src);
  const u64 src_end = src_begin + (static_cast<u64>(oh) * factor - 1) * src_stride + static_cast<u64>(ow) * factor;
  const u64 dst_begin = reinterpret_cast<std::uintptr_t>(dst);
  const u64 dst_end = dst_begin + static_cast<u64>(oh - 1) * dst_stride + ow;
  if (src_begin < dst_end && dst_begin < src_end) return FilterResult::Overlap;

  const u32 n = factor * factor;
  for (u32 oy = 0; oy < oh; ++oy) {
    u8* out_row = dst + static_cast<std::size_t>(oy) * dst_stride;
    for (u32 ox = 0; ox < ow; ++ox) {
      u32 sum = 0;
      for (u32 dy = 0; dy < factor; ++dy) {
        const u8* in_row = src + static_cast<std::size_t>(oy * factor + dy) * src_stride + static_cast<std::size_t>(ox) * factor;
        for (u32 dx = 0; dx < factor; ++dx) sum += in_row[dx];
      }
      out_row[ox] = static_cast<u8>((sum + n / 2) / n);
    }
  }
  *out_w = ow;
  *out_h = oh;
  return FilterResult::Ok;
}

}  // namespace lumora::boxfilter
