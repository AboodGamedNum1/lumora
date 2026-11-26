// Stub. The extraction plan contributor replaces this file with the real implementation.
#include "extract_plan.h"

namespace lumora::package {
ExtractPlan plan_extraction(const std::vector<EntryInfo>&, std::uint64_t, const ExtractLimits&) { return {}; }
std::vector<std::string> cleanup_paths(const ExtractPlan&, const std::vector<EntryInfo>&, std::size_t, bool) {
  return {};
}
const char* to_string(ExtractVerdict) { return "stub"; }
}  // namespace lumora::package
