// Fuzz target: scene. Feeds raw bytes to parse_scene, with the default and a small depth limit.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "scene.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text(reinterpret_cast<const char*>(data), size);
  (void)lumora::scene::parse_scene(text);
  (void)lumora::scene::parse_scene(text, 4);
  return 0;
}
