// Stub. The scope U contributor replaces this file with the real implementation.
#include "tess.h"

namespace lumora::tess {
const char* status_text(Status) { return "stub"; }
Status area2(const Point*, std::uint32_t, std::int64_t*) { return Status::NullBuffer; }
Status is_simple(const Point*, std::uint32_t, bool*) { return Status::NullBuffer; }
Status locate(const Point*, std::uint32_t, std::int32_t, std::int32_t, Location*) { return Status::NullBuffer; }
std::uint32_t scratch_size(std::uint32_t) { return 0; }
Status triangulate(const Point*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
std::uint64_t digest_indices(const std::uint32_t*, std::size_t) { return 0; }
}  // namespace lumora::tess
