// Stub: implementation lands by pull request (contributor scope). See bridges.h for the contract.
#include "bridges.h"

namespace lumora::bridges {

BridgeResult bridges_scratch_words(std::uint32_t, std::uint32_t, std::uint32_t*) { return BridgeResult::NullArgument; }
BridgeResult bridges_run(std::uint32_t, std::uint32_t, const std::uint32_t*, const std::uint32_t*, std::uint32_t*, std::uint32_t, std::uint8_t*,
                         std::uint8_t*, std::uint32_t*, std::uint32_t*) {
  return BridgeResult::NullArgument;
}

}  // namespace lumora::bridges
