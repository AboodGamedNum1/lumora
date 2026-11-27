// Stub: implementation lands by pull request (contributor scope). See histogram.h for the contract.
#include "histogram.h"

namespace lumora::histogram {

HistResult build(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t, Histogram*) { return HistResult::NullArgument; }
std::uint64_t total(const Histogram*) { return 0; }
bool percentile(const Histogram*, std::uint32_t, std::uint8_t*) { return false; }
bool mean_q8(const Histogram*, std::uint32_t*) { return false; }
bool stretch_lut(const Histogram*, std::uint32_t, std::uint32_t, std::uint8_t*) { return false; }

}  // namespace lumora::histogram
