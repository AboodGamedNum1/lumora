// Stub: implementation lands by pull request (contributor scope). See utf8.h for the contract.
#include "utf8.h"

namespace lumora::utf8 {

Utf8Result validate(const std::uint8_t*, std::uint32_t, std::uint32_t*) { return Utf8Result::NullArgument; }
Utf8Result decode_one(const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) { return Utf8Result::NullArgument; }
Utf8Result encode_one(std::uint32_t, std::uint8_t*, std::uint32_t, std::uint32_t*) { return Utf8Result::NullArgument; }

}  // namespace lumora::utf8
