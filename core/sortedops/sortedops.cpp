// Stub: implementation lands by pull request (contributor scope). See sortedops.h for the contract.
#include "sortedops.h"

namespace lumora::sortedops {

OpResult lower_bound_u32(const std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t*) { return OpResult::NullArgument; }
OpResult upper_bound_u32(const std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t*) { return OpResult::NullArgument; }
OpResult merge_sorted_u32(const std::uint32_t*, std::uint32_t, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*) { return OpResult::NullArgument; }
OpResult unique_sorted_u32(std::uint32_t*, std::uint32_t, std::uint32_t*) { return OpResult::NullArgument; }

}  // namespace lumora::sortedops
