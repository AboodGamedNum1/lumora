// Stub: implementation lands by pull request (contributor scope). See vsync.h for the contract.
#include "vsync.h"

namespace lumora::lifecycle {

bool reset(VsyncEstimator*) { return false; }
bool on_vsync(VsyncEstimator*, std::uint64_t) { return false; }
bool estimate_vsync_ns(const VsyncEstimator*, std::uint64_t*) { return false; }

}  // namespace lumora::lifecycle
