// Stub: implementation lands by pull request (contributor scope). See byte_ring.h for the contract.
#include "byte_ring.h"

namespace lumora::ring {

void reset(ByteRing*) {}
std::uint32_t size(const ByteRing*) { return 0; }
std::uint32_t free_space(const ByteRing*) { return 0; }
std::uint32_t write(ByteRing*, const std::uint8_t*, std::uint32_t) { return 0; }
std::uint32_t read(ByteRing*, std::uint8_t*, std::uint32_t) { return 0; }
std::uint32_t peek(const ByteRing*, std::uint8_t*, std::uint32_t, std::uint32_t) { return 0; }
std::uint32_t skip(ByteRing*, std::uint32_t) { return 0; }

}  // namespace lumora::ring
