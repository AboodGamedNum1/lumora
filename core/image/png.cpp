// Placeholder for the PNG codec module. The contributor replaces this file; the interface is core/image/png.h.
#include "png.h"

namespace lumora::image {

PngResult decode_png(const std::uint8_t*, std::size_t, const PngLimits&) {
  PngResult r;
  r.status = PngStatus::BadHeader;
  r.message = "not implemented";
  return r;
}

std::vector<std::uint8_t> encode_png(const Rgba8&, int) { return {}; }

}  // namespace lumora::image
