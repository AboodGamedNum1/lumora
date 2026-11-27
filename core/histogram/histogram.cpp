// Lumora histogram (scope AI). Integer only, no heap, no recursion, no mutable statics. See docs/histogram.md.
#include "histogram.h"

namespace lumora::histogram {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

u64 sum_bins(const Histogram* h) {
  u64 t = 0;
  for (u32 v = 0; v < kBins; ++v) t += h->bins[v];
  return t;
}

// Caller has checked h, per_mille <= 1000 and total > 0.
std::uint8_t percentile_of(const Histogram* h, u32 per_mille, u64 tot) {
  if (per_mille == 0) {
    u32 v = 0;
    while (v + 1 < kBins && h->bins[v] == 0) ++v;
    return static_cast<std::uint8_t>(v);
  }
  const u64 target = (tot * per_mille + 999) / 1000;
  u64 run = 0;
  for (u32 v = 0; v + 1 < kBins; ++v) {
    run += h->bins[v];
    if (run >= target) return static_cast<std::uint8_t>(v);
  }
  return static_cast<std::uint8_t>(kBins - 1);  // the last bin completes the total
}

}  // namespace

HistResult build(const std::uint8_t* src, u32 w, u32 h, u32 src_stride, Histogram* out) {
  if (src == nullptr || out == nullptr) return HistResult::NullArgument;
  if (w == 0 || h == 0 || w > kMaxDim || h > kMaxDim) return HistResult::BadSize;
  if (src_stride < w || src_stride > kMaxStride) return HistResult::BadStride;
  u32 counts[kBins] = {};
  for (u32 y = 0; y < h; ++y) {
    const std::uint8_t* row = src + static_cast<u64>(y) * src_stride;
    for (u32 x = 0; x < w; ++x) ++counts[row[x]];
  }
  for (u32 v = 0; v < kBins; ++v) out->bins[v] = counts[v];
  return HistResult::Ok;
}

u64 total(const Histogram* h) {
  if (h == nullptr) return 0;
  return sum_bins(h);
}

bool percentile(const Histogram* h, u32 per_mille, std::uint8_t* out) {
  if (h == nullptr || out == nullptr || per_mille > 1000) return false;
  const u64 tot = sum_bins(h);
  if (tot == 0) return false;
  *out = percentile_of(h, per_mille, tot);
  return true;
}

bool mean_q8(const Histogram* h, u32* out) {
  if (h == nullptr || out == nullptr) return false;
  const u64 tot = sum_bins(h);
  if (tot == 0) return false;
  u64 weighted = 0;
  for (u32 v = 0; v < kBins; ++v) weighted += static_cast<u64>(v) * h->bins[v];
  *out = static_cast<u32>((weighted * 256 + tot / 2) / tot);
  return true;
}

bool stretch_lut(const Histogram* h, u32 lo_pm, u32 hi_pm, std::uint8_t* lut) {
  if (h == nullptr || lut == nullptr || lo_pm >= hi_pm || hi_pm > 1000) return false;
  const u64 tot = sum_bins(h);
  if (tot == 0) return false;
  const u32 lo = percentile_of(h, lo_pm, tot);
  const u32 hi = percentile_of(h, hi_pm, tot);
  for (u32 v = 0; v < kBins; ++v) {
    u32 r;
    if (lo == hi) r = v;
    else if (v <= lo) r = 0;
    else if (v >= hi) r = 255;
    else r = ((v - lo) * 255 + (hi - lo) / 2) / (hi - lo);
    lut[v] = static_cast<std::uint8_t>(r);
  }
  return true;
}

}  // namespace lumora::histogram
