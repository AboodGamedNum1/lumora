// Stub: the contributor implements inflate_raw and deflate_raw (see deflate.h).
#include "deflate.h"

namespace lumora::package {

InflateResult inflate_raw(const std::uint8_t*, std::size_t, std::uint64_t) { return {}; }

std::vector<std::uint8_t> deflate_raw(const std::uint8_t*, std::size_t, int) { return {}; }

}  // namespace lumora::package
