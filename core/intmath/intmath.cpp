// Stub: implementation lands by pull request (contributor scope). See intmath.h for the contract.
#include "intmath.h"

namespace lumora::intmath {

std::uint32_t isqrt_u64(std::uint64_t) { return 0; }
std::uint64_t gcd_u64(std::uint64_t, std::uint64_t) { return 0; }
bool lcm_u64(std::uint64_t, std::uint64_t, std::uint64_t*) { return false; }
bool add_i64(std::int64_t, std::int64_t, std::int64_t*) { return false; }
bool sub_i64(std::int64_t, std::int64_t, std::int64_t*) { return false; }
bool mul_i64(std::int64_t, std::int64_t, std::int64_t*) { return false; }
bool mul_div_u64(std::uint64_t, std::uint64_t, std::uint64_t, Round, std::uint64_t*) { return false; }

}  // namespace lumora::intmath
