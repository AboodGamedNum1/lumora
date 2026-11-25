// Stub: the contributor implements everything declared in settings.h.
#include "settings.h"

namespace lumora::scene {

SettingsParseResult parse_settings_schema(const std::string&) {
  SettingsParseResult r;
  r.error = SettingsError::NotJson;
  r.message = "not implemented";
  return r;
}
ValueCheck validate_value(const SettingDef&, SettingValue&) { return ValueCheck::WrongType; }
SettingsValues resolve_values(const SettingsSchema&, const SettingsValues&, std::vector<std::string>*) { return {}; }
SettingsValues migrate_values(const SettingsSchema&, std::uint32_t, const SettingsValues& s) { return s; }
bool is_visible(const SettingsSchema&, const SettingDef&, const SettingsValues&) { return true; }
SettingsValues reset_values(const SettingsSchema&, const SettingsValues& c, bool) { return c; }

}  // namespace lumora::scene
