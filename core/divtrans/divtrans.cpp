// Divisor-lattice transforms modulo 998244353 (contract in divtrans.h).
//
// Method: every pass runs over the harmonic pairs (d, m = k * d, k >= 2, m <= n), n / d - 1 pairs for each d, so n ln n steps in all, in place, with no
// memory beyond the caller's arrays.
//  - divisor_zeta: d from n down to 1; for every proper multiple m of d, a[m] += a[d]. When the turn of d comes, a[d] is still the original entry: only the
//    turns of smaller divisors change it, and those come later. So each original a(d) reaches every proper multiple of d exactly once, directly, and every
//    a[m] ends as the sum of a(d) over all divisors d of m (m itself is the entry it started with).
//  - divisor_mobius: the exact reverse: d from 1 up to n; for every proper multiple m of d, a[m] -= a[d]. When the turn of d comes, the turns of all smaller
//    divisors of d have been undone, so a[d] is back to its original entry.
//  - divisor_convolve: zero out, then for every pair d * e <= n, out[d * e] += a[d] * b[e]. Each product is below 2^60 and is reduced at once, so a u64
//    never holds more than a residue plus one residue.
// Integer only: no floating point, no 128-bit type, no compiler builtin, no allocation, no recursion, no mutable static.
#include "divtrans.h"

namespace lumora::divtrans {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// x and y are residues, so x + y is below 2 * kMod, which fits a u32.
inline u32 add_mod(u32 x, u32 y) {
  const u32 s = x + y;
  return s >= kMod ? s - kMod : s;
}
inline u32 sub_mod(u32 x, u32 y) { return x >= y ? x - y : x + kMod - y; }
inline bool all_residues(const u32* p, u32 n) {
  bool ok = true;
  for (u32 i = 0; i < n; ++i) {
    if (p[i] >= kMod) ok = false;
  }
  return ok;
}
}  // namespace

DivResult divisor_zeta(u32* a, u32 n) {
  if (n > kMaxN) return DivResult::TooLarge;
  if (n == 0u) return DivResult::Ok;
  if (a == nullptr) return DivResult::NullArgument;
  if (!all_residues(a, n)) return DivResult::ValueOutOfRange;
  for (u32 d = n; d >= 1u; --d) {
    const u32 v = a[d - 1u];
    for (u32 m = 2u * d; m <= n; m += d) a[m - 1u] = add_mod(a[m - 1u], v);
  }
  return DivResult::Ok;
}

DivResult divisor_mobius(u32* a, u32 n) {
  if (n > kMaxN) return DivResult::TooLarge;
  if (n == 0u) return DivResult::Ok;
  if (a == nullptr) return DivResult::NullArgument;
  if (!all_residues(a, n)) return DivResult::ValueOutOfRange;
  for (u32 d = 1u; d <= n; ++d) {
    const u32 v = a[d - 1u];
    for (u32 m = 2u * d; m <= n; m += d) a[m - 1u] = sub_mod(a[m - 1u], v);
  }
  return DivResult::Ok;
}

DivResult divisor_convolve(const u32* a, const u32* b, u32 n, u32* out) {
  if (n > kMaxN) return DivResult::TooLarge;
  if (n == 0u) return DivResult::Ok;
  if (a == nullptr || b == nullptr || out == nullptr) return DivResult::NullArgument;
  if (!all_residues(a, n)) return DivResult::ValueOutOfRange;
  if (!all_residues(b, n)) return DivResult::ValueOutOfRange;
  for (u32 i = 0; i < n; ++i) out[i] = 0u;
  for (u32 d = 1u; d <= n; ++d) {
    const u64 x = a[d - 1u];
    const u32 last = n / d;
    for (u32 e = 1u; e <= last; ++e) {
      const u32 prod = static_cast<u32>((x * static_cast<u64>(b[e - 1u])) % kMod);
      out[d * e - 1u] = add_mod(out[d * e - 1u], prod);
    }
  }
  return DivResult::Ok;
}
}  // namespace lumora::divtrans
