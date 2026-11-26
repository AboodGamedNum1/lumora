// Per-scene capability declarations and grants (docs/security-model.md item 4, docs/lua-api.md "Runtime
// capability enforcement"): a manifest declaration never grants anything by itself; the user grants or denies
// per scene; the runtime answers every capability request with an explicit status; a grant is reset when a
// scene update declares more capabilities. Pure state machine on small values: no I/O, no platform calls, no
// persistence (the caller stores and reloads the records). Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lumora::package {

enum class Capability : std::uint8_t {
  Microphone,
  Notifications,
  ActivityRecognition,
  CoarseLocation,
  Count_  // number of capabilities; not a capability
};

// Names as written in the manifest "uses" list: "microphone", "notifications", "activity_recognition",
// "coarse_location". Exact lowercase full-byte match only: wrong case, whitespace, an empty name and embedded NUL
// bytes are unknown (no C-string truncation). capability_name returns an empty string for Count_ or invalid values.
// capability_from_name leaves *out untouched when it returns false; a null out is tolerated and the result still
// says whether the name matches. Never shift by 32 or more: guard the value before computing a bit.
const char* capability_name(Capability c);
bool capability_from_name(std::string_view name, Capability* out);

using CapabilitySet = std::uint32_t;  // bit i set means Capability(i) is in the set
constexpr CapabilitySet capability_bit(Capability c) { return CapabilitySet{1} << static_cast<unsigned>(c); }

enum class DeclareVerdict {
  Ok,
  UnknownCapability,  // a name that is not in the table above
  DuplicateCapability,
  TooManyNames        // more than max_names entries
};

struct DeclareResult {
  DeclareVerdict verdict = DeclareVerdict::Ok;
  std::size_t index = 0;        // offending name index
  CapabilitySet declared = 0;   // valid only when verdict is Ok
};

// Parses the manifest "uses" names. Check order per name in input order: TooManyNames is checked first
// (count above max_names, index max_names), then per name UnknownCapability, then DuplicateCapability.
// An empty list is Ok with an empty set. max_names 0 allows only the empty list; there is no implicit four-name
// limit above that (unknown and duplicate decide); SIZE_MAX works. On any failure declared is 0. Unknown index is
// that name; Duplicate index is the second occurrence; TooManyNames wins even if an earlier name is unknown.
DeclareResult parse_declared(const std::vector<std::string>& names, std::size_t max_names = 32);

enum class CapabilityStatus {
  Available,
  NotDeclared,            // the scene never declared it
  PermissionDenied,       // declared, not granted (or revoked)
  Unsupported,            // declared and granted, but this device or build flavor cannot provide it
  TemporarilyUnavailable  // declared, granted, supported, but the provider is not running right now
};

// What the platform says right now. supported: the device and build flavor can provide the capability at all;
// active: the provider is running (for example the microphone service).
struct PlatformState {
  CapabilitySet supported = 0;
  CapabilitySet active = 0;
};

// One record per installed scene. The caller keeps the record keyed by scene id and persists it.
struct GrantRecord {
  CapabilitySet declared = 0;  // from the installed package manifest
  CapabilitySet granted = 0;   // always a subset of declared
};

// Grant and revoke. Granting a capability that is not declared returns false and changes nothing. Revoking
// a capability that is not granted is allowed and changes nothing (returns true). Revocation takes effect for the
// next status query, including while the scene runs (no callbacks or events). An invalid capability: grant
// returns false, revoke returns true, both unchanged. Null record pointer: both return false. Repeated grant
// succeeds unchanged. Each touches only its own bit and does not repair stray granted bits.
bool grant(GrantRecord* r, Capability c);
bool revoke(GrantRecord* r, Capability c);

// Scene update: returns the record for the new package. If the new declared set is a subset of the old one, the
// grants that are still declared are kept. If it declares anything the old one did not, every grant is reset
// (the user is asked again for all of them). A record with granted bits outside declared is repaired by
// clearing those bits first. Inputs are masked to the four valid bits first (old declared, old granted, new
// declared), so high bits never trigger a reset and never appear in the output. Equal sets count as subset; an empty
// new set clears everything; an empty old set with a nonempty new set resets grants. Output declared is the masked
// new declared set; output granted is repaired old grants intersected with the new declared set (or 0 on reset).
GrantRecord reconcile_update(const GrantRecord& old_record, CapabilitySet new_declared);

// The answer a scene call gets. Order: NotDeclared, PermissionDenied, Unsupported, TemporarilyUnavailable,
// Available. Capability::Count_ and values above it are NotDeclared. The record is read without mutation; a
// granted bit outside declared is NotDeclared; active without supported is Unsupported.
CapabilityStatus query(const GrantRecord& r, Capability c, const PlatformState& platform);

// to_string uses the identifier spelling; unknown enum values return "Unknown".
const char* to_string(DeclareVerdict v);
const char* to_string(CapabilityStatus s);

}  // namespace lumora::package
