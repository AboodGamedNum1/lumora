// Stub. The scope T contributor replaces this file with the real implementation.
#include "spatial.h"

namespace lumora::spatial {
const char* status_text(Status) { return "stub"; }
std::uint32_t node_bound(std::uint32_t, std::uint32_t) { return 0; }
Status build(const Box*, std::uint32_t, std::uint32_t, Node*, std::uint32_t, std::uint32_t*, std::uint32_t*) { return Status::NullBuffer; }
Status query_box(const Box*, std::uint32_t, const Node*, std::uint32_t, const std::uint32_t*, const Box&, std::uint32_t*, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
Status query_point(const Box*, std::uint32_t, const Node*, std::uint32_t, const std::uint32_t*, std::int32_t, std::int32_t, std::uint32_t*, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
Status nearest(const Box*, std::uint32_t, const Node*, std::uint32_t, const std::uint32_t*, std::int32_t, std::int32_t, std::uint32_t*, std::uint64_t*, bool*) { return Status::NullBuffer; }
std::uint64_t digest_indices(const std::uint32_t*, std::size_t) { return 0; }
}  // namespace lumora::spatial
