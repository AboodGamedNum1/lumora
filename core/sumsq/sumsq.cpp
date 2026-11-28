// Sums of two and four squares (contract in sumsq.h). Integer only: no heap, no recursion, no mutable static, no array, no 128-bit type, no builtin.
// Method and proof: docs/sumsq.md.
//   r2(n) = 4 * the product of (e + 1) over the primes p = 1 (mod 4) with p^e exactly dividing n, and 0 if some prime p = 3 (mod 4) has an odd exponent (r2(0) = 1).
//   r4(n) = 8 * the sum of the divisors of n not divisible by 4 = 8 sigma(n) for odd n and 24 sigma(m) for n = 2^a m, a >= 1, m odd (r4(0) = 1).
// Factoring is trial division by 2 and then by the odd q while q * q <= the remainder; what is left above 1 is a prime. All arithmetic is u64.
#include "sumsq.h"

namespace lumora::sumsq {

SqResult count_two_squares(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return SqResult::NullArgument;
  if (n == 0u) {
    *out = 1u;
    return SqResult::Ok;
  }
  std::uint64_t rem = n;
  std::uint64_t prod = 1u;
  while (rem % 2u == 0u) rem /= 2u;
  for (std::uint64_t q = 3u; q * q <= rem; q += 2u) {
    if (rem % q != 0u) continue;
    std::uint64_t e = 0u;
    while (rem % q == 0u) {
      rem /= q;
      ++e;
    }
    if (q % 4u == 3u) {
      if (e % 2u == 1u) {
        *out = 0u;
        return SqResult::Ok;
      }
    } else {
      prod *= e + 1u;
    }
  }
  if (rem > 1u) {  // a prime with no factor up to its square root
    if (rem % 4u == 3u) {
      *out = 0u;
      return SqResult::Ok;
    }
    prod *= 2u;
  }
  *out = 4u * prod;
  return SqResult::Ok;
}

SqResult count_four_squares(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return SqResult::NullArgument;
  if (n == 0u) {
    *out = 1u;
    return SqResult::Ok;
  }
  std::uint64_t rem = n;
  std::uint64_t sigma = 1u;
  while (rem % 2u == 0u) rem /= 2u;
  for (std::uint64_t q = 3u; q * q <= rem; q += 2u) {
    if (rem % q != 0u) continue;
    std::uint64_t term = 1u;
    std::uint64_t pw = 1u;
    while (rem % q == 0u) {
      rem /= q;
      pw *= q;
      term += pw;
    }
    sigma *= term;
  }
  if (rem > 1u) sigma *= rem + 1u;  // a prime with no factor up to its square root
  *out = ((n % 2u == 1u) ? 8u : 24u) * sigma;
  return SqResult::Ok;
}

}  // namespace lumora::sumsq
