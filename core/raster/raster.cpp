// Stub. The scope O contributor replaces this file with the real implementation.
#include "raster.h"

namespace lumora::raster {
const char* raster_status_text(RasterStatus) { return "stub"; }
RasterStatus fill_polygons(const Contour*, std::size_t, Fill, const Target&) { return RasterStatus::NullBuffer; }
std::size_t flatten_quad(const P&, const P&, const P&, Fx, P*, std::size_t) { return 0; }
std::size_t flatten_cubic(const P&, const P&, const P&, const P&, Fx, P*, std::size_t) { return 0; }
std::uint64_t digest_mask(const Target&) { return 0; }
}  // namespace lumora::raster
