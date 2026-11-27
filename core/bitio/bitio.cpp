// Stub: implementation lands by pull request (contributor scope). See bitio.h for the contract.
#include "bitio.h"

namespace lumora::bitio {

bool write_bits(std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t) { return false; }
bool read_bits(const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*) { return false; }
bool align_to_byte(std::uint32_t, std::uint32_t*) { return false; }
std::uint32_t bits_left(std::uint32_t, const std::uint32_t*) { return 0; }

}  // namespace lumora::bitio
