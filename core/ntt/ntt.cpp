// Number-theoretic convolution (contract in ntt.h). Stub: the implementation belongs to the contributor scope.
#include "ntt.h"
namespace lumora::ntt {
NttResult ntt_scratch_words(std::uint32_t, std::uint32_t, std::uint32_t*) { return NttResult::NullArgument; }
NttResult ntt_convolve(const std::uint32_t*, std::uint32_t, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*,
                       std::uint32_t, std::uint32_t*) {
  return NttResult::NullArgument;
}
}  // namespace lumora::ntt
