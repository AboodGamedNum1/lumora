// Stub: implementation lands by pull request (contributor scope). See utf16.h for the contract.
#include "utf16.h"

namespace lumora::utf16 {

Utf16Result validate(const std::uint16_t*, std::uint32_t, std::uint32_t*) { return Utf16Result::NullArgument; }
Utf16Result count_code_points(const std::uint16_t*, std::uint32_t, std::uint32_t*) { return Utf16Result::NullArgument; }
Utf16Result decode_one(const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) { return Utf16Result::NullArgument; }
Utf16Result encode_one(std::uint32_t, std::uint16_t*, std::uint32_t, std::uint32_t*) { return Utf16Result::NullArgument; }

}  // namespace lumora::utf16
