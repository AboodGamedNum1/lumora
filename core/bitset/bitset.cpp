// Stub: implementation lands by pull request (contributor scope). See bitset.h for the contract.
#include "bitset.h"

namespace lumora::bitset {

std::uint32_t words_for(std::uint32_t) { return 0; }
bool set(std::uint64_t*, std::uint32_t, std::uint32_t, bool) { return false; }
bool test(const std::uint64_t*, std::uint32_t, std::uint32_t, bool*) { return false; }
bool fill(std::uint64_t*, std::uint32_t, bool) { return false; }
bool popcount(const std::uint64_t*, std::uint32_t, std::uint32_t*) { return false; }
bool find_next(const std::uint64_t*, std::uint32_t, std::uint32_t, bool, std::uint32_t*) { return false; }

}  // namespace lumora::bitset
