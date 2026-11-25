// STUB: owned by the JSON-limits contributor. Compiles and rejects everything until the real
// implementation lands.
#include "json_limits.h"

namespace lumora::package {

JsonScanResult scan_json(std::string_view, const JsonLimits&) { return {JsonVerdict::Malformed, 0}; }
const char* to_string(JsonVerdict) { return "not implemented"; }

}  // namespace lumora::package
