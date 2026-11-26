// Stub. The scope Z contributor replaces this file with the real implementation.
#include "vecdraw.h"

namespace lumora::vecdraw {
const char* status_text(Status) { return "stub"; }
std::size_t work_bytes_needed(std::uint32_t, bool) { return 0; }
Status fill_path(const outline::Command*, std::uint32_t, std::int32_t, raster::Fill, const raster::Target&, void*, std::size_t) {
  return Status::NullBuffer;
}
Status stroke_path(const outline::Command*, std::uint32_t, std::int32_t, const stroke::Style&, const raster::Target&, void*,
                   std::size_t) {
  return Status::NullBuffer;
}
}  // namespace lumora::vecdraw
