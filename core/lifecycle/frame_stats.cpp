// Stub: implementation lands by pull request (contributor scope). See frame_stats.h for the contract.
#include "frame_stats.h"

namespace lumora::lifecycle {

bool record_frame(FrameStats*, std::uint64_t) { return false; }
bool summarize(const FrameStats*, FrameSummary*) { return false; }
std::size_t format_overlay(const FrameSummary&, char*, std::size_t) { return 0; }

}  // namespace lumora::lifecycle
