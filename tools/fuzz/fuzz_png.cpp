// Placeholder fuzz target: png. The contributor replaces this file.
#include <cstddef>
#include <cstdint>

#include "png.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  (void)lumora::image::decode_png(data, size);
  return 0;
}
