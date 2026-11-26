// Stub: implementation lands by pull request (contributor scope). See crash_breaker.h for the contract.
#include "crash_breaker.h"

namespace lumora::lifecycle {

bool valid(const Policy&) { return false; }
std::uint32_t recent_crashes(const CrashLog&, std::uint64_t, std::uint64_t) { return 0; }
bool record_crash(CrashLog*, std::uint64_t, const Policy&) { return false; }
bool decide(const CrashLog*, const CrashLog*, std::uint64_t, const Policy&, bool, Decision*) { return false; }
const char* to_string(Action) { return ""; }
const char* to_string(Backend) { return ""; }

}  // namespace lumora::lifecycle
