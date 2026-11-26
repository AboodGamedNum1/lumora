// Stub. The scope W contributor replaces this file with the real implementation.
#include "stroke.h"

namespace lumora::stroke {
const char* status_text(Status) { return "stub"; }
std::uint32_t contour_bound(std::uint32_t, bool) { return 0; }
std::uint32_t point_bound(std::uint32_t, bool) { return 0; }
std::uint32_t isqrt_nearest(std::uint64_t) { return 0; }
Status stroke_polyline(const Point*, std::uint32_t, const Style&, Point*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) { return Status::NullBuffer; }
std::uint64_t digest_points(const Point*, std::size_t) { return 0; }
}  // namespace lumora::stroke
