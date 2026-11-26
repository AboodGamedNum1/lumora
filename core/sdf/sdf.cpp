// Stub. The scope S contributor replaces this file with the real implementation.
#include "sdf.h"

namespace lumora::sdf {
const char* status_text(Status) { return "stub"; }
Status distance_squared(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint8_t, std::uint32_t*) { return Status::NullBuffer; }
std::uint32_t isqrt(std::uint64_t) { return 0; }
Status make_sdf(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t*, std::size_t) { return Status::NullBuffer; }
std::uint8_t edge_coverage(std::uint8_t, std::uint32_t) { return 0; }
std::uint64_t digest_image(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t) { return 0; }
}  // namespace lumora::sdf
