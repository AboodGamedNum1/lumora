// Stub: implementation lands by pull request (contributor scope). See power_policy.h for the contract.
#include "power_policy.h"

namespace lumora::lifecycle {

bool render_scale_percent(std::uint32_t, std::uint32_t, std::uint32_t*) { return false; }
bool quality_preset(bool, std::uint32_t, std::uint32_t, Quality*) { return false; }
bool scaled_size(std::uint32_t, std::uint32_t, std::uint32_t*) { return false; }
const char* to_string(Quality) { return ""; }

}  // namespace lumora::lifecycle
