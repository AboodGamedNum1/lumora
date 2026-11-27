// Stub: implementation lands by pull request (contributor scope). See combin.h for the contract.
#include "combin.h"

namespace lumora::combin {

CombResult binom_u64(std::uint64_t, std::uint64_t, std::uint64_t*) { return CombResult::NullArgument; }
CombResult multichoose_u64(std::uint64_t, std::uint64_t, std::uint64_t*) { return CombResult::NullArgument; }
CombResult factorial_u64(std::uint32_t, std::uint64_t*) { return CombResult::NullArgument; }
CombResult catalan_u64(std::uint32_t, std::uint64_t*) { return CombResult::NullArgument; }

}  // namespace lumora::combin
