// Arithmetic functions of one u32 (contract in arithfun.h). Integer only: no heap, no recursion, no mutable static, no array, no 128-bit type, no builtin.
// Method and proofs: docs/arithfun.md. One trial-division pass over p = 2, 3, 5, 7, 9, ... (odd p after 2; composite p never divide, their prime factors were removed
// first) while p * p <= m (m is what is left of n), then one leftover prime if m > 1. Each prime p with exponent e updates the running values:
//   phi: phi -= phi / p (p divides phi at that point); d: d *= e + 1; sigma: sigma *= 1 + p + ... + p^e; rad: rad *= p; omega: += 1; Omega: += e.
#include "arithfun.h"

namespace lumora::arithfun {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

struct Facts {
  u32 phi;
  u32 d;
  u64 sigma;
  u32 rad;
  u32 omega;
  u32 big;
};

void take(Facts& f, u32 p, u32 e, u64 t) {
  f.phi -= f.phi / p;
  f.d *= e + 1u;
  f.sigma *= t;
  f.rad *= p;
  f.omega += 1u;
  f.big += e;
}

// All six values of n >= 1. The factor loop runs at most 32768 times (p * p <= m <= n < 2^32 gives p <= 65535, and p moves by 2 after 2).
Facts factorise(u32 n) {
  Facts f{n, 1u, 1u, 1u, 0u, 0u};
  u32 m = n;
  for (u32 p = 2u; static_cast<u64>(p) * p <= m; p += (p == 2u) ? 1u : 2u) {
    if (m % p != 0u) continue;
    u32 e = 0u;
    u64 pp = 1u;
    u64 t = 1u;
    while (m % p == 0u) {
      m /= p;
      ++e;
      pp *= p;
      t += pp;
    }
    take(f, p, e, t);
  }
  if (m > 1u) take(f, m, 1u, static_cast<u64>(m) + 1u);
  return f;
}
}  // namespace

AfResult euler_phi(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return AfResult::NullArgument;
  if (n == 0u) return AfResult::BadArgument;
  *out = factorise(n).phi;
  return AfResult::Ok;
}

AfResult divisor_count(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return AfResult::NullArgument;
  if (n == 0u) return AfResult::BadArgument;
  *out = factorise(n).d;
  return AfResult::Ok;
}

AfResult divisor_sigma(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return AfResult::NullArgument;
  if (n == 0u) return AfResult::BadArgument;
  *out = factorise(n).sigma;
  return AfResult::Ok;
}

AfResult radical(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return AfResult::NullArgument;
  if (n == 0u) return AfResult::BadArgument;
  *out = factorise(n).rad;
  return AfResult::Ok;
}

AfResult prime_factor_counts(std::uint32_t n, std::uint32_t* distinct, std::uint32_t* with_multiplicity) {
  if (distinct == nullptr || with_multiplicity == nullptr) return AfResult::NullArgument;
  if (n == 0u) return AfResult::BadArgument;
  const Facts f = factorise(n);
  *distinct = f.omega;
  *with_multiplicity = f.big;
  return AfResult::Ok;
}
}  // namespace lumora::arithfun
