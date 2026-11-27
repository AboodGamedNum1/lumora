// Stub: implementation lands by pull request (contributor scope). See checksum.h for the contract.
#include "checksum.h"

namespace lumora::checksum {

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t*, std::uint32_t) { return crc; }
std::uint32_t crc32(const std::uint8_t*, std::uint32_t) { return 0; }
std::uint32_t adler32_update(std::uint32_t adler, const std::uint8_t*, std::uint32_t) { return adler; }
std::uint32_t adler32(const std::uint8_t*, std::uint32_t) { return 1; }

}  // namespace lumora::checksum
