// Stub: implementation lands by pull request (contributor scope). See modarith.h for the contract.
#include "modarith.h"

namespace lumora::modarith {

ModResult mulmod_u64(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t*) { return ModResult::NullArgument; }
ModResult powmod_u64(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t*) { return ModResult::NullArgument; }
ModResult modinv_u64(std::uint64_t, std::uint64_t, std::uint64_t*) { return ModResult::NullArgument; }
ModResult is_prime_u64(std::uint64_t, bool*) { return ModResult::NullArgument; }

}  // namespace lumora::modarith
