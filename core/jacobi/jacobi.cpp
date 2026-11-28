// Kronecker and Jacobi symbols and the least square root modulo a prime (contract in jacobi.h).
// Integer only; no heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics. Every product modulo p is one u64 product of two
// residues below p < 2^32 (at most (p - 1)^2 < 2^64) reduced at once.
#include "jacobi.h"

namespace lumora::jacobi {
namespace {
using i32 = std::int32_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// The only product modulo p of the file: a and b are residues below p.
u64 mul(u64 a, u64 b, u64 p) { return (a * b) % p; }

// b^e modulo p by square and multiply (p at least 2, b below p).
u64 power(u64 b, u64 e, u64 p) {
  u64 r = 1u;
  while (e != 0u) {
    if ((e & 1u) != 0u) r = mul(r, b, p);
    b = mul(b, b, p);
    e >>= 1;
  }
  return r;
}

// Trial division by 2, 3 and then 6k - 1, 6k + 1 while d * d <= p: every composite below 2^32 has a prime factor at most 65535.
bool is_prime(u32 p) {
  if (p < 2u) return false;
  if (p < 4u) return true;
  if (p % 2u == 0u || p % 3u == 0u) return false;
  for (u64 d = 5u; d * d <= p; d += 6u) {
    if (p % d == 0u || p % (d + 2u) == 0u) return false;
  }
  return true;
}

// The Kronecker symbol (a / n) for every i64 a and u32 n. n = 2^v m with m odd: (a / n) = (a / 2)^v (a / m); (a / 2) is 0 for an even a, 1 for a = 1, 7 modulo 8 and
// -1 for a = 3, 5 modulo 8 (a & 7 of the two's complement is the mathematical residue); (a / m) by the reciprocity loop on a reduced into [0, m).
i32 symbol(i64 a, u32 n) {
  if (n == 0u) return (a == 1 || a == -1) ? 1 : 0;
  i32 sign = 1;
  u32 m = n;
  u32 v = 0u;
  while ((m & 1u) == 0u) {
    m >>= 1;
    ++v;
  }
  if (v != 0u) {
    const u64 low = static_cast<u64>(a) & 7u;
    if ((low & 1u) == 0u) return 0;
    if ((v & 1u) != 0u && (low == 3u || low == 5u)) sign = -sign;
  }
  if (m == 1u) return sign;
  i64 r = a % static_cast<i64>(m);
  if (r < 0) r += static_cast<i64>(m);
  u64 x = static_cast<u64>(r);
  u64 y = m;
  while (x != 0u) {
    while ((x & 1u) == 0u) {
      x >>= 1;
      if ((y & 7u) == 3u || (y & 7u) == 5u) sign = -sign;
    }
    const u64 t = x;
    x = y;
    y = t;
    if ((x & 3u) == 3u && (y & 3u) == 3u) sign = -sign;
    x %= y;
  }
  return y == 1u ? sign : 0;
}
}  // namespace

JResult kronecker(std::int64_t a, std::uint32_t n, std::int32_t* out) {
  if (out == nullptr) return JResult::NullArgument;
  *out = symbol(a, n);
  return JResult::Ok;
}

JResult jacobi(std::int64_t a, std::uint32_t n, std::int32_t* out) {
  if (out == nullptr) return JResult::NullArgument;
  if ((n & 1u) == 0u) return JResult::BadModulus;
  *out = symbol(a, n);
  return JResult::Ok;
}

JResult sqrt_mod_prime(std::int64_t a, std::uint32_t p, std::uint32_t* out) {
  if (out == nullptr) return JResult::NullArgument;
  if (!is_prime(p)) return JResult::NotPrime;
  i64 res = a % static_cast<i64>(p);
  if (res < 0) res += static_cast<i64>(p);
  const u64 r = static_cast<u64>(res);
  u64 x = 0u;
  if (p == 2u) {
    x = r;
  } else if (r != 0u) {
    if (power(r, (p - 1u) / 2u, p) != 1u) return JResult::NoRoot;
    if (p % 4u == 3u) {
      x = power(r, (p + 1u) / 4u, p);
    } else {
      // Tonelli-Shanks: p - 1 = q 2^s, z the least non-residue, c = z^q, t = r^q, x = r^((q + 1) / 2); invariants x^2 = r t, t has order dividing 2^(m - 1), c has order 2^m.
      u64 q = p - 1u;
      u32 s = 0u;
      while ((q & 1u) == 0u) {
        q >>= 1;
        ++s;
      }
      u64 z = 2u;
      while (power(z, (p - 1u) / 2u, p) != p - 1u) ++z;
      u64 c = power(z, q, p);
      u64 t = power(r, q, p);
      x = power(r, (q + 1u) / 2u, p);
      u32 m = s;
      while (t != 1u) {
        u32 i = 0u;
        u64 tt = t;
        while (tt != 1u) {
          tt = mul(tt, tt, p);
          ++i;
        }
        u64 b = c;
        for (u32 j = 0u; j + 1u + i < m; ++j) b = mul(b, b, p);
        x = mul(x, b, p);
        c = mul(b, b, p);
        t = mul(t, c, p);
        m = i;
      }
    }
  }
  if (p - x < x) x = p - x;
  *out = static_cast<u32>(x);
  return JResult::Ok;
}
}  // namespace lumora::jacobi
