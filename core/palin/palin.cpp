// Stub: implementation lands by pull request (contributor scope). See palin.h for the contract.
#include "palin.h"

namespace lumora::palin {

PalinResult palin_radii(std::uint32_t, const std::uint8_t*, std::uint32_t*, std::uint32_t*) { return PalinResult::NullArgument; }
PalinResult palin_scratch_words(std::uint32_t, std::uint32_t*) { return PalinResult::NullArgument; }
PalinResult palin_longest(std::uint32_t, const std::uint8_t*, std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) {
  return PalinResult::NullArgument;
}
PalinResult palin_count(std::uint32_t, const std::uint8_t*, std::uint32_t*, std::uint32_t, std::uint64_t*) { return PalinResult::NullArgument; }

}  // namespace lumora::palin
