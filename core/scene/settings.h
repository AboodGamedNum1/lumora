// settings.json: the per-scene user-facing knobs (docs/format.md, "settings.json"). Pure CPU, no UI code.
// The Android app builds its settings screen from a parsed SettingsSchema; Lua writes and stored user values are
// validated against it. Rules fixed here (normative for this module):
//  * Document shape (schema_version 1):
//      { "settings_version": 3,
//        "settings": [ { "id": "sky_color", "label": "settings.sky_color", "type": "color",
//                        "default": [0.1, 0.2, 0.5, 1.0], "color_space": "srgb" },
//                      { "id": "density", "label": "settings.density", "type": "float",
//                        "min": 0, "max": 1, "step": 0.05, "default": 0.5, "scope": "scene" },
//                      { "id": "mode", "type": "enum", "default": "calm", "label": "settings.mode",
//                        "options": [ {"id": "calm", "label": "settings.mode.calm"}, {"id": "wild", "label": "..."} ] },
//                      { "id": "fast", "type": "bool", "default": false, "label": "...",
//                        "visible_if": {"setting": "mode", "equals": "wild"} } ],
//        "migrations": [ { "from_version": 2, "rename": [ {"from": "speed", "to": "density"} ], "remove": ["old_id"] } ] }
//  * Types: "bool", "int", "float", "enum", "color". Ids match [a-z][a-z0-9_]{0,63} and are unique; the id is the
//    identity, never the label. "label" is a localization key, never display text, 1..128 bytes, non-empty.
//  * int and float need min, max (min < max, finite, |value| <= 1e9) and a default inside the range; "step" is
//    optional (> 0) and when present values are on the grid min + k*step (validate_value snaps within half a step).
//    An int setting's min, max, step and default must be whole numbers.
//  * enum needs 1..64 options with unique ids (same id rule), the default must be one of them.
//  * color is an array of 4 numbers (r,g,b,a), each in [0,1], color_space "srgb" or "linear" (default "srgb").
//  * "scope" is "scene" (default; one value per scene) or "device" (shared by every scene on the device).
//  * "reset": "default" (default; the reset action restores the default) or "keep" (reset does not touch it).
//  * visible_if refers to an EARLIER setting in the list (no forward or self references, so no cycles) and is
//    {"setting": id, "equals": value} or {"setting": id, "not_equals": value}; the value must be valid for that setting.
//  * At most 256 settings. Unknown fields are ignored, wrong types for known fields are errors.
//  * Migration: "settings_version" (whole number >= 1) is the version of this schema. Each migration entry upgrades
//    stored values written at "from_version" by applying "rename" then "remove", for from_version < settings_version;
//    entries apply in ascending from_version order, each only when the stored version is <= its from_version.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lumora::scene {

enum class SettingType { Bool, Int, Float, Enum, Color };
enum class SettingScope { Scene, Device };
enum class ResetBehavior { Default, Keep };

struct SettingValue {
  SettingType type = SettingType::Bool;
  bool b = false;
  std::int64_t i = 0;
  double f = 0;
  std::string e;          // enum option id
  float color[4] = {0, 0, 0, 1};
  friend bool operator==(const SettingValue&, const SettingValue&) = default;
};

struct EnumOption {
  std::string id;
  std::string label;
};

struct VisibleIf {
  bool present = false;
  std::string setting;
  bool negate = false;   // true for not_equals
  SettingValue value;
};

struct SettingDef {
  std::string id;
  std::string label;                 // localization key
  SettingType type = SettingType::Bool;
  SettingValue default_value;
  double min = 0, max = 0, step = 0; // int and float; step 0 means none
  std::vector<EnumOption> options;   // enum
  std::string color_space = "srgb";  // color
  SettingScope scope = SettingScope::Scene;
  ResetBehavior reset = ResetBehavior::Default;
  VisibleIf visible_if;
};

struct Migration {
  std::uint32_t from_version = 0;
  std::vector<std::pair<std::string, std::string>> rename;  // from, to
  std::vector<std::string> remove;
};

struct SettingsSchema {
  std::uint32_t settings_version = 1;
  std::vector<SettingDef> settings;
  std::vector<Migration> migrations;
};

enum class SettingsError {
  None, NotJson, WrongType, MissingField, BadValue, DuplicateId, BadReference, TooMany
};

struct SettingsParseResult {
  SettingsError error = SettingsError::None;
  std::string message;  // names the setting and field
  SettingsSchema schema;
  bool ok() const { return error == SettingsError::None; }
};

SettingsParseResult parse_settings_schema(const std::string& json_text);

using SettingsValues = std::map<std::string, SettingValue>;  // key: setting id

// UnknownSetting is returned only for a definition with an empty id (a definition is always a known setting).
// NotOnStep means the nearest step-grid point lies above max, so the value cannot be snapped inside the range.
enum class ValueCheck { Ok, UnknownSetting, WrongType, OutOfRange, NotOnStep, UnknownOption, BadColor };

// Validates (and, for step grids, snaps) one value against its definition. On Ok, `value` may have been snapped.
ValueCheck validate_value(const SettingDef& def, SettingValue& value);

// Effective values: every setting gets a value. Stored values that are unknown or invalid are dropped and
// replaced by the default; `problems` (optional) lists "id: reason" strings for what was dropped.
SettingsValues resolve_values(const SettingsSchema& schema, const SettingsValues& stored,
                              std::vector<std::string>* problems = nullptr);

// Applies the migration rules for values stored at `stored_version` (see the header comment).
SettingsValues migrate_values(const SettingsSchema& schema, std::uint32_t stored_version, const SettingsValues& stored);

// True when the setting should be shown given the current values (visible_if rule; no rule means visible).
bool is_visible(const SettingsSchema& schema, const SettingDef& def, const SettingsValues& values);

// Reset action: restores defaults except for settings marked "keep"; with only_scene_scope set, device-scope
// settings are left alone.
SettingsValues reset_values(const SettingsSchema& schema, const SettingsValues& current, bool only_scene_scope = true);

}  // namespace lumora::scene
