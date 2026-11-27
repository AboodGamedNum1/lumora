// Stub: implementation lands by pull request (contributor scope). See iroot.h for the contract.
#include "iroot.h"

namespace lumora::iroot {

RootResult iroot_u64(std::uint64_t, std::uint32_t, std::uint64_t*) { return RootResult::NullArgument; }
RootResult perfect_power_u64(std::uint64_t, std::uint64_t*, std::uint32_t*) { return RootResult::NullArgument; }
RootResult ipow_u64(std::uint64_t, std::uint32_t, std::uint64_t*) { return RootResult::NullArgument; }

}  // namespace lumora::iroot
