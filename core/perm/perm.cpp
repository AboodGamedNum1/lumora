// Stub: implementation lands by pull request (contributor scope). See perm.h for the contract.
#include "perm.h"

namespace lumora::perm {

PermResult next_permutation_u32(std::uint32_t*, std::uint32_t, bool*) { return PermResult::NullArgument; }
PermResult perm_rank(const std::uint32_t*, std::uint32_t, std::uint64_t*) { return PermResult::NullArgument; }
PermResult perm_unrank(std::uint32_t, std::uint64_t, std::uint32_t*) { return PermResult::NullArgument; }

}  // namespace lumora::perm
