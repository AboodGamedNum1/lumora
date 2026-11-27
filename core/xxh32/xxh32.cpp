// Stub: implementation lands by pull request (contributor scope). See xxh32.h for the contract.
#include "xxh32.h"

namespace lumora::xxh32 {

bool hash(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t*) { return false; }
void reset(State*, std::uint32_t) {}
bool update(State*, const std::uint8_t*, std::uint32_t) { return false; }
bool digest(const State*, std::uint32_t*) { return false; }

}  // namespace lumora::xxh32
