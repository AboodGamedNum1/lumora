// Fuzz target: settings. Feeds raw bytes to parse_settings_schema. A schema that parses is also pushed through
// the value helpers, which must not crash on any accepted schema.
#include <cstddef>
#include <cstdint>
#include <string>

#include "settings.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::scene;
  const std::string text(reinterpret_cast<const char*>(data), size);
  const auto r = parse_settings_schema(text);
  if (!r.ok()) return 0;
  std::vector<std::string> problems;
  const SettingsValues defaults = resolve_values(r.schema, {}, &problems);
  for (const auto& def : r.schema.settings) {
    SettingValue v = def.default_value;
    (void)validate_value(def, v);
    (void)is_visible(r.schema, def, defaults);
  }
  (void)reset_values(r.schema, defaults);
  (void)reset_values(r.schema, defaults, false);
  (void)migrate_values(r.schema, 0, defaults);
  return 0;
}
