// Stub. The capabilities contributor replaces this file with the real implementation.
#include "capabilities.h"

namespace lumora::package {
const char* capability_name(Capability) { return "stub"; }
bool capability_from_name(std::string_view, Capability*) { return false; }
DeclareResult parse_declared(const std::vector<std::string>&, std::size_t) { return {}; }
bool grant(GrantRecord*, Capability) { return false; }
bool revoke(GrantRecord*, Capability) { return false; }
GrantRecord reconcile_update(const GrantRecord&, CapabilitySet) { return {}; }
CapabilityStatus query(const GrantRecord&, Capability, const PlatformState&) { return CapabilityStatus::NotDeclared; }
const char* to_string(DeclareVerdict) { return "stub"; }
const char* to_string(CapabilityStatus) { return "stub"; }
}  // namespace lumora::package
