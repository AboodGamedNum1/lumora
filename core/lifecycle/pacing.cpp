// Stub: implementation lands by pull request (contributor scope). See pacing.h for the contract.
#include "pacing.h"

namespace lumora::lifecycle {

std::uint64_t cap_period_ns(FrameCap) { return 0; }
bool frame_interval_ns(const PacingInput&, std::uint64_t*) { return false; }
bool valid(const ScaleConfig&) { return false; }
bool on_frame(ScaleState*, std::uint64_t, std::uint64_t, const ScaleConfig&, bool*) { return false; }
bool on_thermal_throttle(ScaleState*, const ScaleConfig&, bool*) { return false; }
const char* to_string(FrameCap) { return ""; }

}  // namespace lumora::lifecycle
