// Stub. The scope R contributor replaces this file with the real implementation.
#include "resample.h"

namespace lumora::resample {
const char* status_text(Status) { return "stub"; }
Status resize(const ConstSurface&, const Surface&, Filter) { return Status::NullBuffer; }
std::uint32_t mip_count(std::uint32_t, std::uint32_t) { return 0; }
bool mip_level_size(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t*, std::uint32_t*) { return false; }
std::uint64_t mip_chain_bytes(std::uint32_t, std::uint32_t) { return 0; }
Status build_mip_chain(const ConstSurface&, std::uint8_t*, std::size_t) { return Status::NullBuffer; }
std::uint64_t digest_surface(const ConstSurface&) { return 0; }
}  // namespace lumora::resample
