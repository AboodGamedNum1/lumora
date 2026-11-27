// Stub: implementation lands by pull request (contributor scope). See decfmt.h for the contract.
#include "decfmt.h"

namespace lumora::decfmt {

DecResult format_u64(std::uint64_t, char*, std::uint32_t, std::uint32_t*) { return DecResult::NullArgument; }
DecResult format_i64(std::int64_t, char*, std::uint32_t, std::uint32_t*) { return DecResult::NullArgument; }
DecResult parse_u64(const char*, std::uint32_t, std::uint64_t*) { return DecResult::NullArgument; }
DecResult parse_i64(const char*, std::uint32_t, std::int64_t*) { return DecResult::NullArgument; }

}  // namespace lumora::decfmt
