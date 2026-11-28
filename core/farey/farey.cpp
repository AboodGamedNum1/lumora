// Neighbours in the Farey sequence F_n (contract in farey.h).
// Integer only; no heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics. One extended Euclid step on (b, a) gives gcd(a, b) and the
// inverse of a modulo b; the neighbour denominator is then the largest d <= n in one residue class modulo b. Every product is a * d with a, d <= n < 2^32.
#include "farey.h"

namespace lumora::farey {
namespace {
using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Extended Euclid on (b, a), b at least 1: returns gcd(a, b) and sets inv to the t with t * a = gcd (mod b), reduced into [0, b). The coefficients stay within b in magnitude.
u64 euclid(u64 b, u64 a, u64& inv) {
  u64 r0 = b;
  u64 r1 = a;
  i64 t0 = 0;
  i64 t1 = 1;
  while (r1 != 0u) {
    const u64 q = r0 / r1;
    const u64 r2 = r0 - q * r1;
    const i64 t2 = t0 - static_cast<i64>(q) * t1;
    r0 = r1;
    r1 = r2;
    t0 = t1;
    t1 = t2;
  }
  i64 m = t0 % static_cast<i64>(b);
  if (m < 0) m += static_cast<i64>(b);
  inv = static_cast<u64>(m);
  return r0;
}

// The term after (next true) or before (next false) a / b in F_n. The neighbour c / d has b c - a d = 1 (next) or -1 (prev) and the largest d <= n, which makes d + b > n.
FResult neighbour(u32 n, u32 a, u32 b, u32* c, u32* d, bool next) {
  if (c == nullptr || d == nullptr) return FResult::NullArgument;
  if (n == 0u) return FResult::BadOrder;
  if (b == 0u || b > n || a > b) return FResult::BadFraction;
  u64 inv = 0u;
  if (euclid(b, a, inv) != 1u) return FResult::BadFraction;
  if (next ? (a == b) : (a == 0u)) return FResult::NoNeighbour;
  const u64 d0 = next ? (b - inv) % b : inv;
  const u64 dd = d0 + ((n - d0) / b) * b;
  const u64 cc = next ? (1u + static_cast<u64>(a) * dd) / b : (static_cast<u64>(a) * dd - 1u) / b;
  *c = static_cast<u32>(cc);
  *d = static_cast<u32>(dd);
  return FResult::Ok;
}
}  // namespace

FResult farey_next(std::uint32_t n, std::uint32_t a, std::uint32_t b, std::uint32_t* c, std::uint32_t* d) { return neighbour(n, a, b, c, d, true); }

FResult farey_prev(std::uint32_t n, std::uint32_t a, std::uint32_t b, std::uint32_t* c, std::uint32_t* d) { return neighbour(n, a, b, c, d, false); }
}  // namespace lumora::farey
