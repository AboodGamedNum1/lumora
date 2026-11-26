// Stub: implementation lands by pull request (contributor scope). See diag_log.h for the contract.
#include "diag_log.h"

namespace lumora::diag {

bool log_clear(DiagLog*) { return false; }
bool log_push(DiagLog*, std::uint64_t, std::uint16_t, std::int32_t, std::int32_t) { return false; }
bool log_read(const DiagLog*, std::uint32_t, LogEntry*) { return false; }
std::uint32_t log_size(const DiagLog*) { return 0; }
std::uint64_t log_digest(const DiagLog*) { return 0; }

}  // namespace lumora::diag
