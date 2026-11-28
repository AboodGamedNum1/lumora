// Squarefree integers (contract in squarefree.h). Integer only: no heap, no recursion, no mutable static, no array, no 128-bit type, no builtin.
// Method and proof: docs/squarefree.md.
//   mu(m): trial division by 2 and then by the odd q while q * q <= the remainder; an exponent of 2 or more gives 0, each prime flips the sign, and what is left
//          above 1 is a prime (one more flip).
//   Q(n) = sum over d = 1..floor(sqrt(n)) of mu(d) * floor(n / d^2), held in a signed 64-bit sum.
#include "squarefree.h"

namespace lumora::squarefree {
namespace {
// mu(m) for m >= 1.
std::int32_t mu_of(std::uint64_t m) {
  std::int32_t sign = 1;
  std::uint64_t rem = m;
  if (rem % 2u == 0u) {
    rem /= 2u;
    if (rem % 2u == 0u) return 0;
    sign = -sign;
  }
  for (std::uint64_t q = 3u; q * q <= rem; q += 2u) {
    if (rem % q != 0u) continue;
    rem /= q;
    if (rem % q == 0u) return 0;
    sign = -sign;
  }
  if (rem > 1u) sign = -sign;  // a prime with no factor up to its square root
  return sign;
}
}  // namespace

SfResult mobius(std::uint32_t n, std::int32_t* out) {
  if (out == nullptr) return SfResult::NullArgument;
  if (n == 0u) return SfResult::BadArgument;
  *out = mu_of(n);
  return SfResult::Ok;
}

SfResult squarefree_count(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return SfResult::NullArgument;
  std::int64_t sum = 0;
  for (std::uint64_t d = 1u; d * d <= n; ++d) {
    const std::int32_t mu = mu_of(d);
    if (mu == 0) continue;
    const std::int64_t term = static_cast<std::int64_t>(n / (d * d));
    sum += (mu > 0) ? term : -term;
  }
  *out = static_cast<std::uint64_t>(sum);
  return SfResult::Ok;
}

}  // namespace lumora::squarefree
