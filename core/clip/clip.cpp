// Stub. The scope V contributor replaces this file with the real implementation.
#include "clip.h"

namespace lumora::clip {
const char* status_text(Status) { return "stub"; }
Status clip_segment(const Rect&, const Point&, const Point&, Point*, Point*, bool*) { return Status::NullBuffer; }
std::uint32_t scratch_size(std::uint32_t) { return 0; }
Status clip_polygon(const Rect&, const Point*, std::uint32_t, Point*, std::uint32_t, Point*, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
Status clip_polyline(const Rect&, const Point*, std::uint32_t, Point*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) { return Status::NullBuffer; }
std::uint64_t digest_points(const Point*, std::size_t) { return 0; }
}  // namespace lumora::clip
