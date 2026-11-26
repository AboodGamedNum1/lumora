// Stub. The zip audit contributor replaces this file with the real implementation.
#include "zip_audit.h"

namespace lumora::package {
ZipAuditResult audit_zip(const std::uint8_t*, std::size_t, const ZipAuditLimits&) { return {}; }
std::vector<EntryInfo> to_entry_infos(const std::vector<ZipEntry>&) { return {}; }
const char* to_string(ZipVerdict) { return "stub"; }
}  // namespace lumora::package
