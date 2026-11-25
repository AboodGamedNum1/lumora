// STUB: owned by the package-policy contributor. Everything below is a placeholder that
// compiles and rejects everything, so the library links until the real implementation lands.
#include "entry_policy.h"

namespace lumora::package {

NameVerdict validate_name(std::string_view, const Limits&) { return NameVerdict::Empty; }

const char* to_string(NameVerdict) { return "not implemented"; }
const char* to_string(SetVerdict) { return "not implemented"; }

SetResult validate_entries(const std::vector<EntryInfo>&, const Limits&) {
  SetResult r;
  r.verdict = SetVerdict::BadName;
  r.name_verdict = NameVerdict::Empty;
  return r;
}

}  // namespace lumora::package
