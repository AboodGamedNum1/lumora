// Stub. The scope P contributor replaces this file with the real implementation.
#include "composite.h"

namespace lumora::composite {
const char* status_text(Status) { return "stub"; }
std::uint32_t div255(std::uint32_t) { return 0; }
void premultiply(const std::uint8_t[4], std::uint8_t[4]) {}
void unpremultiply(const std::uint8_t[4], std::uint8_t[4]) {}
Status blend(const Surface&, const ConstSurface&, std::int32_t, std::int32_t, BlendMode, std::uint8_t) { return Status::NullBuffer; }
Status fill_mask(const Surface&, const Mask&, std::int32_t, std::int32_t, const std::uint8_t[4], BlendMode) { return Status::NullBuffer; }
Status box_blur(const Surface&, std::uint32_t, std::uint32_t, std::uint8_t*) { return Status::NullBuffer; }
Status blur3(const Surface&, std::uint32_t, std::uint32_t, std::uint8_t*) { return Status::NullBuffer; }
std::uint64_t digest_surface(const ConstSurface&) { return 0; }
}  // namespace lumora::composite
