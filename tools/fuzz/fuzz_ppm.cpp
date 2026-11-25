// Fuzz target: ppm. Feeds raw bytes to decode_ppm with a small dimension cap. A decoded image must be valid and
// must survive an encode/decode round trip with the same size.
#include <cstdint>
#include <cstdlib>
#include <string>

#include "cpu_renderer.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::render2d;
  Image img;
  std::string err;
  if (!decode_ppm(data, size, img, err, 256)) return 0;
  if (!img.valid()) std::abort();
  const auto enc = encode_ppm(img);
  Image again;
  if (!decode_ppm(enc.data(), enc.size(), again, err, 256)) std::abort();
  if (again.width != img.width || again.height != img.height) std::abort();
  return 0;
}
