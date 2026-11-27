// Floor sums (contract in floorsum.h). Euclid-like reduction, integer only, O(log m) iterations, no allocation, no recursion, no 128-bit types.
// All sums are accumulated modulo 2^64 (wrapping u64) or modulo kMod. The mathematical value is never formed.
#include "floorsum.h"

namespace lumora::floorsum {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// n * (n - 1) / 2 for n below 2^32, exact in u64 (below 2^63). The even factor is halved first so no precision is lost.
u64 tri(u64 n) { return (n % 2u == 0u) ? (n / 2u) * (n - 1u) : n * ((n - 1u) / 2u); }

// Exact S modulo 2^64 for n, m >= 1, a, b below 2^32 as the loop of the usual floor sum reduction. Each pass: split off the quotients of a and b by m, then
// swap the roles of m and a. Invariants at the top of a pass: n <= 2^32 - 1, m, a, b <= 2^32 - 1.
u64 wrap_core(u64 n, u64 m, u64 a, u64 b) {
  u64 ans = 0;
  while (n > 0u) {
    if (a >= m) {
      ans += tri(n) * (a / m);  // wraps modulo 2^64: the wrapped product is the exact product modulo 2^64
      a %= m;
    }
    if (b >= m) {
      ans += n * (b / m);
      b %= m;
    }
    const u64 y_max = a * n + b;  // a < m < 2^32 and n < 2^32, so this is below 2^64 (exact)
    if (y_max < m) break;
    n = y_max / m;
    b = y_max % m;
    const u64 t = m;
    m = a;
    a = t;
  }
  return ans;
}

u32 mod_core(u64 n, u64 m, u64 a, u64 b) {
  const u64 p = kMod;
  u64 ans = 0;
  while (n > 0u) {
    if (a >= m) {
      ans = (ans + (tri(n) % p) * ((a / m) % p)) % p;
      a %= m;
    }
    if (b >= m) {
      ans = (ans + (n % p) * ((b / m) % p)) % p;
      b %= m;
    }
    const u64 y_max = a * n + b;
    if (y_max < m) break;
    n = y_max / m;
    b = y_max % m;
    const u64 t = m;
    m = a;
    a = t;
  }
  return static_cast<u32>(ans);
}

// floor(v / m) for any i64 v and m >= 1 (toward minus infinity), and the remainder in 0 .. m - 1. |v| is at most 2^31 here.
i64 floor_div(i64 v, i64 m) { return (v >= 0) ? v / m : -((-v + m - 1) / m); }
}  // namespace

FloorResult floorsum_wrap(std::uint32_t n, std::uint32_t m, std::uint32_t a, std::uint32_t b, std::uint64_t* out) {
  if (out == nullptr) return FloorResult::NullArgument;
  if (m == 0u) return FloorResult::ZeroModulus;
  *out = wrap_core(n, m, a, b);
  return FloorResult::Ok;
}

FloorResult floorsum_mod(std::uint32_t n, std::uint32_t m, std::uint32_t a, std::uint32_t b, std::uint32_t* out) {
  if (out == nullptr) return FloorResult::NullArgument;
  if (m == 0u) return FloorResult::ZeroModulus;
  *out = mod_core(n, m, a, b);
  return FloorResult::Ok;
}

FloorResult floorsum_signed(std::uint32_t n, std::uint32_t m, std::int32_t a, std::int32_t b, std::uint64_t* out) {
  if (out == nullptr) return FloorResult::NullArgument;
  if (m == 0u) return FloorResult::ZeroModulus;
  const i64 mm = static_cast<i64>(m);
  const i64 qa = floor_div(a, mm);
  const i64 qb = floor_div(b, mm);
  const i64 ra = static_cast<i64>(a) - qa * mm;  // 0 .. m - 1
  const i64 rb = static_cast<i64>(b) - qb * mm;
  // S = tri(n) * qa + n * qb + S(n, m, ra, rb). The quotients may be negative: their two's complement u64 images multiply modulo 2^64.
  u64 ans = tri(n) * static_cast<u64>(qa);
  ans += static_cast<u64>(n) * static_cast<u64>(qb);
  ans += wrap_core(n, m, static_cast<u64>(ra), static_cast<u64>(rb));
  *out = ans;
  return FloorResult::Ok;
}
}  // namespace lumora::floorsum
