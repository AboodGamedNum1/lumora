// Stub: implementation lands by pull request (contributor scope). See dither.h for the contract.
#include "dither.h"

namespace lumora::dither {

std::uint8_t threshold(std::uint32_t, std::uint32_t) { return 0; }
std::uint8_t quantize_u16(std::uint16_t, std::uint32_t, std::uint32_t) { return 0; }
bool dither_row_u16(const std::uint16_t*, std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t) { return false; }

}  // namespace lumora::dither
