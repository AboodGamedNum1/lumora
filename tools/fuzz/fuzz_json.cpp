// Fuzz target: json. Feeds raw bytes to the JSON parser and to the package JSON limits scanner.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "json_limits.h"
#include "json_value.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text(reinterpret_cast<const char*>(data), size);
  const auto r = lumora::scene::parse_json(text);
  (void)r.ok;
  (void)lumora::package::scan_json(text);
  return 0;
}
