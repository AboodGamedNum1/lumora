// Monotonic stack queries (contract in monostack.h). Stub: the implementation belongs to the contributor scope.
#include "monostack.h"
namespace lumora::monostack {
StackResult monostack_scratch_words(std::uint32_t, std::uint32_t*) { return StackResult::NullArgument; }
StackResult monostack_bounds(const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*) {
  return StackResult::NullArgument;
}
StackResult monostack_max_rect(const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint64_t*, std::uint32_t*, std::uint32_t*) {
  return StackResult::NullArgument;
}
StackResult monostack_window_min(const std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t*, std::uint32_t, std::uint32_t*) {
  return StackResult::NullArgument;
}
}  // namespace lumora::monostack
