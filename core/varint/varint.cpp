// Stub: implementation lands by pull request (contributor scope). See varint.h for the contract.
#include "varint.h"

namespace lumora::varint {

std::uint32_t encoded_size(std::uint64_t) { return 0; }
bool encode(std::uint64_t, std::uint8_t*, std::uint32_t, std::uint32_t*) { return false; }
bool decode(const std::uint8_t*, std::uint32_t, std::uint64_t*, std::uint32_t*) { return false; }
std::uint64_t zigzag_encode(std::int64_t) { return 0; }
std::int64_t zigzag_decode(std::uint64_t) { return 0; }
bool encode_signed(std::int64_t, std::uint8_t*, std::uint32_t, std::uint32_t*) { return false; }
bool decode_signed(const std::uint8_t*, std::uint32_t, std::int64_t*, std::uint32_t*) { return false; }

}  // namespace lumora::varint
