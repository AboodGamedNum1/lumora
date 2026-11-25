// manifest.json of a .lumora package: parsing and the load policy of docs/format.md section 8.1.
// Newer-than-supported packages are rejected, unknown fields are ignored, nothing is guessed.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "json_value.h"

namespace lumora::scene {

struct SemVer {
  std::uint32_t major = 0, minor = 0, patch = 0;
  friend auto operator<=>(const SemVer&, const SemVer&) = default;
};

// Parses "major.minor.patch" (digits only, no prefix, no suffix). False on anything else.
bool parse_semver(std::string_view text, SemVer& out);

struct Manifest {
  std::uint32_t format_version = 0;
  std::uint32_t scene_schema_version = 0;
  std::uint32_t lua_api_version = 0;
  SemVer min_engine_version;
  std::string name;
  std::string author;                    // optional, empty when absent
  std::string scene_path;                // entry.scene, package-relative
  std::string settings_path;             // entry.settings, empty when absent
  std::string script_path;               // entry.script (one Lua file, docs/scripting-slice.md), empty when absent
  std::vector<std::string> backends;     // optional; empty means the full multi-platform package
};

// What this build of the engine supports.
struct EngineSupport {
  SemVer engine_version{0, 0, 1};
  std::uint32_t max_format_version = 1;
  std::uint32_t max_scene_schema_version = 1;
  std::uint32_t max_lua_api_version = 1;
};

enum class ManifestError {
  None,
  NotJson,
  NotAnObject,
  MissingField,
  WrongType,
  BadValue,            // out of range, empty name, unsafe path, bad version text
  FormatTooNew,
  SchemaTooNew,
  LuaApiTooNew,
  EngineTooOld,        // the package needs a newer engine than this build
  UnknownBackend
};

struct ManifestResult {
  ManifestError error = ManifestError::None;
  std::string message;   // human readable, names the field; empty when error is None
  Manifest manifest;
  bool ok() const { return error == ManifestError::None; }
};

ManifestResult parse_manifest(std::string_view json_text, const EngineSupport& support = {});

// True when the package carries artifacts for `backend` ("vulkan", "gles", "d3d12"): either the
// manifest lists it or it lists none (full package).
bool manifest_supports_backend(const Manifest& m, std::string_view backend);

}  // namespace lumora::scene
