// Stub: implementation lands by pull request (contributor scope). See base64.h for the contract.
#include "base64.h"

namespace lumora::base64 {

Base64Result encode(const std::uint8_t*, std::uint32_t, char*, std::uint32_t, std::uint32_t*) { return Base64Result::NullArgument; }
Base64Result decode(const char*, std::uint32_t, std::uint8_t*, std::uint32_t, std::uint32_t*) { return Base64Result::NullArgument; }

}  // namespace lumora::base64
