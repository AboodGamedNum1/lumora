// Stub: implementation lands by pull request (contributor scope). See rmq.h for the contract.
#include "rmq.h"

namespace lumora::rmq {

RmqResult rmq_table_words(std::uint32_t, std::uint32_t*) { return RmqResult::NullArgument; }
RmqResult rmq_build(const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t) { return RmqResult::NullArgument; }
RmqResult rmq_query(const std::uint32_t*, std::uint32_t, const std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t*) {
  return RmqResult::NullArgument;
}

}  // namespace lumora::rmq
