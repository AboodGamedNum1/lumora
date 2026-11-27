// Histogram of an 8-bit image with percentile, mean and a contrast-stretch table (auto levels for wallpaper textures). Pure integer: no
// floating point, no libm, no 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::histogram {

constexpr std::uint32_t kMaxDim = 4096;      // largest width or height
constexpr std::uint32_t kMaxStride = 65536;  // largest row stride in bytes
constexpr std::uint32_t kBins = 256;

struct Histogram {
  std::uint32_t bins[kBins] = {};  // bins[v] = number of pixels with value v
};

enum class HistResult : std::uint8_t { Ok = 0, NullArgument = 1, BadSize = 2, BadStride = 3 };

// Counts the w x h image src (row r starts at src + r * src_stride, only the first w bytes of each row are read) into *out.
// Check order: src or out null gives NullArgument; w or h equal to 0 or above kMaxDim gives BadSize; src_stride < w or above kMaxStride
// gives BadStride. A failure leaves *out as it was. On Ok every bin is overwritten (not added to), and the bins sum to w * h (at most
// 2^24). The caller guarantees (h - 1) * src_stride + w readable bytes. No aliasing promise for out and src.
HistResult build(const std::uint8_t* src, std::uint32_t w, std::uint32_t h, std::uint32_t src_stride, Histogram* out);

// The pixel count is always the sum of the bins, computed in 64 bits; there is no stored total. Hand-built histograms are allowed: each
// bin is any uint32, so the sum is at most 256 * (2^32 - 1) and every product below fits in 64 bits.
std::uint64_t total(const Histogram* h);  // 0 for null

// Value at the given per-mille rank. h null, out null, per_mille above 1000 or a total of 0 gives false and *out is unchanged. Otherwise
// target = ceil(total * per_mille / 1000) (integer arithmetic in 64 bits); for per_mille 0 the result is the smallest value whose bin is
// not 0. For per_mille above 0 the result is the smallest v such that the sum of bins[0..v] >= target. per_mille 1000 gives the
// largest value whose bin is not 0.
bool percentile(const Histogram* h, std::uint32_t per_mille, std::uint8_t* out);

// Mean value in Q8 (value * 256), rounded half up: (sum of v * bins[v] * 256 + total / 2) / total, in 64 bits. h null, out null or a
// total of 0 gives false and *out is unchanged. The result is at most 255 * 256.
bool mean_q8(const Histogram* h, std::uint32_t* out);

// Contrast-stretch table. lo = percentile(lo_pm), hi = percentile(hi_pm). Returns false and leaves lut unchanged when h or lut is null,
// lo_pm >= hi_pm, hi_pm above 1000, or the total is 0. If lo == hi the table is the identity (lut[v] = v). Otherwise lut[v] = 0 for
// v <= lo, 255 for v >= hi, and for lo < v < hi lut[v] = ((v - lo) * 255 + (hi - lo) / 2) / (hi - lo) (integer division). lut has 256
// entries and is written only on a true return.
bool stretch_lut(const Histogram* h, std::uint32_t lo_pm, std::uint32_t hi_pm, std::uint8_t* lut);

// Decisions: results never depend on any stored total. A histogram with bins that do not come from build() is handled by the same
// formulas. A header ambiguity found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::histogram
