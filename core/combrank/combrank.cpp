// Combination ranking stub (contract in combrank.h). Owned by the combrank contributor; the implementation is not written yet.
#include "combrank.h"
namespace lumora::combrank {
CbResult combo_count(std::uint32_t, std::uint32_t, std::uint64_t*) { return CbResult::BadArgument; }
CbResult combo_rank(std::uint64_t, std::uint32_t, std::uint32_t, std::uint64_t*) { return CbResult::BadArgument; }
CbResult combo_unrank(std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t*) { return CbResult::BadArgument; }
CbResult combo_next(std::uint64_t, std::uint32_t, std::uint32_t, std::uint64_t*) { return CbResult::BadArgument; }
}  // namespace lumora::combrank
