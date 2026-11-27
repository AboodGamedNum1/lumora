// Stub: implementation lands by pull request (contributor scope). See seqdist.h for the contract.
#include "seqdist.h"

namespace lumora::seqdist {

SeqResult lcs_length_u32(const std::uint32_t*, std::uint32_t, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t,
                         std::uint32_t*) {
  return SeqResult::NullArgument;
}
SeqResult edit_distance_u32(const std::uint32_t*, std::uint32_t, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t,
                            std::uint32_t*) {
  return SeqResult::NullArgument;
}
SeqResult lis_length_u32(const std::uint32_t*, std::uint32_t, bool, std::uint32_t*, std::uint32_t, std::uint32_t*) {
  return SeqResult::NullArgument;
}

}  // namespace lumora::seqdist
