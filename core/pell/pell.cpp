// Pell's equation x^2 - d y^2 = 1: the smallest solution modulo 998244353 and the continued fraction period of sqrt(d) (contract in pell.h).
//
// Method: the continued fraction of sqrt(d) = [a0; a1, a2, ...] by the standard integer recurrence, with a0 = floor(sqrt(d)), m_0 = 0, dd_0 = 1 and
//   m_{n+1} = a_n * dd_n - m_n,   dd_{n+1} = (d - m_{n+1}^2) / dd_n,   a_{n+1} = (a0 + m_{n+1}) / dd_{n+1}.
// The expansion repeats from a1, and the first quotient equal to 2 * a0 is a_P, the end of the first period (P is the period). The convergents p_n / q_n
// (p_n = a_n p_{n-1} + p_{n-2}, q_n likewise, p_{-1} = 1, p_{-2} = 0, q_{-1} = 0, q_{-2} = 1) are kept modulo kMod only, as two rolling pairs. The
// fundamental solution is the convergent of index P - 1 when P is even and 2 P - 1 when P is odd, so for an odd P the expansion runs through a second period.
// The loop therefore takes at most 2 P steps, with a handful of scalars and no array.
// Bounds (measured over every non-square d up to 2^20 by tools/pell_ref.py): m <= 1023, dd <= 2046, a <= 2046 (so a * dd <= 4186116 < 2^22 and
// m * m < 2^20, d - m * m < 2^20 + 1, all in u32). The convergent step a * p + p' is computed in u64 (a < 2^11 and p < 2^30 give a * p < 2^41, plus p' below 2^30),
// then reduced with one % kMod. The same for q. No 128-bit type, no overflow, no floating point, no builtin, no recursion, no array, no mutable static.
// Local scalars: a0, m, dd, a, n, period, pm1, pm2, qm1, qm2, and the one product temporary.
#include "pell.h"

namespace lumora::pell {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
}  // namespace

PellResult pell_fundamental(std::uint32_t d, std::uint32_t* x, std::uint32_t* y, std::uint32_t* period) {
  if (x == nullptr || y == nullptr || period == nullptr) return PellResult::NullArgument;
  if (d < 2u || d > kMaxD) return PellResult::BadD;
  u32 a0 = 1u;
  while ((a0 + 1u) * (a0 + 1u) <= d) ++a0;  // a0 <= 1024, so (a0 + 1)^2 fits u32
  if (a0 * a0 == d) return PellResult::BadD;
  u32 m = 0u;
  u32 dd = 1u;
  u32 a = a0;
  u32 n = 0u;       // the number of quotients consumed so far
  u32 per = 0u;     // 0 until the first period has been found
  u64 pm2 = 0u, pm1 = 1u;  // p_{n-2}, p_{n-1}
  u64 qm2 = 1u, qm1 = 0u;
  for (;;) {
    const u64 p = (static_cast<u64>(a) * pm1 + pm2) % kMod;
    const u64 q = (static_cast<u64>(a) * qm1 + qm2) % kMod;
    pm2 = pm1;
    pm1 = p;
    qm2 = qm1;
    qm1 = q;
    ++n;
    m = a * dd - m;
    dd = (d - m * m) / dd;
    a = (a0 + m) / dd;
    if (a == 2u * a0) {
      if (per == 0u) {
        per = n;
        if ((per & 1u) == 0u) break;
      } else {
        break;
      }
    }
  }
  *x = static_cast<u32>(pm1);
  *y = static_cast<u32>(qm1);
  *period = per;
  return PellResult::Ok;
}
}  // namespace lumora::pell
