// Stub: implementation lands by pull request (contributor scope). See prng.h for the contract.
#include "prng.h"

namespace lumora::prng {

bool pcg32_seed(Pcg32*, std::uint64_t, std::uint64_t) { return false; }
std::uint32_t pcg32_next(Pcg32*) { return 0; }
std::uint32_t pcg32_bounded(Pcg32*, std::uint32_t) { return 0; }
bool pcg32_advance(Pcg32*, std::uint64_t) { return false; }

}  // namespace lumora::prng
