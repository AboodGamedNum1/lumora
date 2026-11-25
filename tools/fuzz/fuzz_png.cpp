// Fuzz target: png. Feeds raw bytes to decode_png with small limits (256 per side, 65536 pixels). A failure must leave
// the image empty. A success must give a valid image inside the limits that survives an encode/decode round trip
// pixel-exact. Anything else aborts, so the fuzzer reports it as a crash.
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "png.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::image;
  PngLimits limits;
  limits.max_dim = 256;
  limits.max_pixels = 65536;
  const PngResult r = decode_png(data, size, limits);
  if (!r.ok()) {
    if (!r.image.rgba.empty() || r.image.width != 0 || r.image.height != 0) std::abort();
    return 0;
  }
  const Rgba8& img = r.image;
  if (!img.valid()) std::abort();
  if (img.width > limits.max_dim || img.height > limits.max_dim) std::abort();
  if (std::uint64_t{img.width} * img.height > limits.max_pixels) std::abort();
  const auto enc = encode_png(img, 1);
  if (enc.empty()) std::abort();
  const PngResult again = decode_png(enc.data(), enc.size(), limits);
  if (!again.ok()) std::abort();
  if (again.image.width != img.width || again.image.height != img.height) std::abort();
  if (again.image.rgba != img.rgba) std::abort();
  return 0;
}
