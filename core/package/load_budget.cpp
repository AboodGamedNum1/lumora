// Stub. The load budget contributor replaces this file with the real implementation.
#include "load_budget.h"

namespace lumora::package {
bool texture_bytes(const TextureInfo&, std::uint64_t*) { return false; }
BudgetResult check_load_budget(const SceneStats&, const LoadBudgetLimits&) { return {}; }
const char* to_string(BudgetVerdict) { return "stub"; }
const char* to_string(TextureFormat) { return "stub"; }
}  // namespace lumora::package
