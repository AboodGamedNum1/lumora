// Stub: implementation lands by pull request (contributor scope). See zorder.h for the contract.
#include "zorder.h"

namespace lumora::zorder {

std::uint32_t morton2_encode(std::uint16_t, std::uint16_t) { return 0; }
ZResult morton2_decode(std::uint32_t, std::uint16_t*, std::uint16_t*) { return ZResult::NullArgument; }
ZResult morton3_encode(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t*) { return ZResult::NullArgument; }
ZResult morton3_decode(std::uint32_t, std::uint32_t*, std::uint32_t*, std::uint32_t*) { return ZResult::NullArgument; }
ZResult hilbert_xy2d(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t*) { return ZResult::NullArgument; }
ZResult hilbert_d2xy(std::uint32_t, std::uint32_t, std::uint32_t*, std::uint32_t*) { return ZResult::NullArgument; }

}  // namespace lumora::zorder
