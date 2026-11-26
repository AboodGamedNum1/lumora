// Stub. The scope X contributor replaces this file with the real implementation.
#include "flatten.h"

namespace lumora::flatten {
const char* status_text(Status) { return "stub"; }
std::uint32_t quad_segments(Point, Point, Point, std::int32_t) { return 0; }
std::uint32_t cubic_segments(Point, Point, Point, Point, std::int32_t) { return 0; }
Status flatten_quad(Point, Point, Point, std::int32_t, Point*, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
Status flatten_cubic(Point, Point, Point, Point, std::int32_t, Point*, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
std::uint64_t digest_points(const Point*, std::size_t) { return 0; }
}  // namespace lumora::flatten
