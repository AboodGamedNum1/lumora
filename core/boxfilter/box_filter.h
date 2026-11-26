// Box-filter downscale of an 8-bit coverage image by a whole factor (smaller glyph bitmaps, mip levels of masks). Integer only: no floating
// point, no libm, no 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::boxfilter {

constexpr std::uint32_t kMaxDim = 4096;      // largest source width or height
constexpr std::uint32_t kMaxStride = 65536;  // largest row stride in bytes
constexpr std::uint32_t kMaxFactor = 16;

enum class FilterResult : std::uint8_t { Ok = 0, NullArgument = 1, BadSize = 2, BadStride = 3, BadFactor = 4, Overlap = 5 };

// Downscales the w x h image src (row r starts at src + r * src_stride) by factor f into dst (row r starts at dst + r * dst_stride).
// The output size is out_w = w / f and out_h = h / f (integer division; source columns and rows beyond out_w * f and out_h * f are
// ignored). Each output pixel is (S + n / 2) / n with n = f * f and S the sum of its n source pixels (integer division, so half rounds
// up; S is at most 255 * 256, no overflow), so a constant block gives that constant and f = 1 copies the image. The caller guarantees
// that src has (h - 1) * src_stride + w readable bytes and dst has (out_h - 1) * dst_stride + out_w writable bytes.
// Check order, each failure writes nothing and leaves *out_w and *out_h as they were: src, dst, out_w or out_h null gives NullArgument;
// w or h equal to 0 or above kMaxDim gives BadSize; src_stride < w or above kMaxStride gives BadStride; factor 0, above kMaxFactor, or
// larger than w or h (so that out_w or out_h would be 0) gives BadFactor; dst_stride < out_w or above kMaxStride gives BadStride; then
// Overlap. Overlap compares, as integer addresses in 64 bits, the half-open source span [src, src + (out_h * f - 1) * src_stride +
// out_w * f) with the destination span [dst, dst + (out_h - 1) * dst_stride + out_w); any intersection is Overlap and spans that only
// touch are allowed. On Ok exactly out_w bytes of each of the out_h rows of dst are written (the stride padding is not touched),
// *out_w and *out_h are set, and the result is Ok.
FilterResult box_downscale(const std::uint8_t* src, std::uint32_t w, std::uint32_t h, std::uint32_t src_stride, std::uint32_t factor,
                           std::uint8_t* dst, std::uint32_t dst_stride, std::uint32_t* out_w, std::uint32_t* out_h);

}  // namespace lumora::boxfilter
