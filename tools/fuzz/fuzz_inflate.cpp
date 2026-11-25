// Fuzz target: inflate. Raw bytes go to inflate_raw with a small output limit. The input is also compressed at
// levels 1 and 6 and must come back identical.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "deflate.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::package;
  constexpr std::uint64_t kMaxOut = 1u << 16;
  const InflateResult r = inflate_raw(data, size, kMaxOut);
  if (r.status == InflateStatus::Ok) {
    if (r.data.size() > kMaxOut || r.consumed > size) std::abort();
  } else if (!r.data.empty()) {
    std::abort();
  }
  const std::size_t n = size < kMaxOut ? size : kMaxOut;
  for (int level : {1, 6}) {
    const std::vector<std::uint8_t> packed = deflate_raw(data, n, level);
    const InflateResult back = inflate_raw(packed.data(), packed.size(), n);
    if (back.status != InflateStatus::Ok || back.data.size() != n) std::abort();
    for (std::size_t i = 0; i < n; ++i)
      if (back.data[i] != data[i]) std::abort();
  }
  return 0;
}
