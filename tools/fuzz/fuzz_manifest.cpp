// Fuzz target: manifest. Feeds raw bytes to parse_manifest.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "manifest.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text(reinterpret_cast<const char*>(data), size);
  const auto r = lumora::scene::parse_manifest(text);
  if (r.ok()) {
    (void)lumora::scene::manifest_supports_backend(r.manifest, "vulkan");
    for (const auto& b : r.manifest.backends) (void)lumora::scene::manifest_supports_backend(r.manifest, b);
  }
  return 0;
}
