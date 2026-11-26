// Stub. The scope Y contributor replaces this file with the real implementation.
#include "outline.h"

namespace lumora::outline {
const char* status_text(Status) { return "stub"; }
std::uint32_t contour_bound(std::uint32_t) { return 0; }
std::uint32_t point_bound(std::uint32_t) { return 0; }
Status build_outline(const Command*, std::uint32_t, std::int32_t, Point*, std::uint32_t, std::uint32_t*, std::uint32_t,
                     std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) { return Status::NullBuffer; }
std::uint64_t digest_outline(const Point*, const std::uint32_t*, const std::uint8_t*, std::uint32_t) { return 0; }
}  // namespace lumora::outline
