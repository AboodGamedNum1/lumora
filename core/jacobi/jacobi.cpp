// Quadratic residue stub (contract in jacobi.h). Owned by the jacobi contributor; the implementation is not written yet.
#include "jacobi.h"
namespace lumora::jacobi {
JResult kronecker(std::int64_t, std::uint32_t, std::int32_t*) { return JResult::BadModulus; }
JResult jacobi(std::int64_t, std::uint32_t, std::int32_t*) { return JResult::BadModulus; }
JResult sqrt_mod_prime(std::int64_t, std::uint32_t, std::uint32_t*) { return JResult::BadModulus; }
}  // namespace lumora::jacobi
