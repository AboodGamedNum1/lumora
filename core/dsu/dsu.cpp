// Stub: implementation lands by pull request (contributor scope). See dsu.h for the contract.
#include "dsu.h"

namespace lumora::dsu {

DsuResult dsu_init(std::uint32_t*, std::uint32_t) { return DsuResult::NullArgument; }
DsuResult dsu_find(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t*) { return DsuResult::NullArgument; }
DsuResult dsu_unite(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, bool*) { return DsuResult::NullArgument; }
DsuResult dsu_count_sets(const std::uint32_t*, std::uint32_t, std::uint32_t*) { return DsuResult::NullArgument; }

}  // namespace lumora::dsu
