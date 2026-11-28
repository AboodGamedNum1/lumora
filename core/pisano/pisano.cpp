// The Pisano period pi(m): the smallest n > 0 with F(n) = 0 and F(n + 1) = 1 modulo m (contract in pisano.h).
//
// Method. Let Q = [[1, 1], [1, 0]], so Q^n = [[F(n+1), F(n)], [F(n), F(n-1)]]. Then Q^n = I modulo m exactly when F(n) = 0 and F(n + 1) = 1 modulo m, and pi(m) is
// the order of Q in GL2(Z/m). That order is the LCM of the orders modulo the prime powers p^k exactly dividing m. For one prime power the order is found as follows.
//   (1) A multiple of it is known. Modulo a prime p, Q has the characteristic polynomial x^2 - x - 1 of discriminant 5. For p = 1 or 4 (mod 5) the two roots are
//       in F_p and distinct, Q is diagonalisable over F_p and Q^(p-1) = I. For p = 2 or 3 (mod 5) the roots are conjugate in F_(p^2), distinct, each root r has
//       norm r^(p+1) = det(Q) * (-1) = -1 (the product of the roots is -1), so r^(2(p+1)) = 1 and Q^(2(p+1)) = I. For p = 5 the polynomial is (x - 3)^2, so
//       Q = 3 I + N with N^2 = 0 and Q^20 = 3^20 I + 20 * 3^19 N = I (3^4 = 1 and 20 = 0 modulo 5). So with N0 = p - 1, 2(p + 1) or 20 (p = 2 gives 6, which is a
//       multiple of its true order 3), Q^N0 = I modulo p. Lift to p^k: if Q^n = I + p^j A (j >= 1) then (I + p^j A)^p = I modulo p^(j+1), also for p = 2 ((I + 2A)^2 =
//       I + 4(A + A^2)), so Q^(N0 p^(k-1)) = I modulo p^k. The candidate is c = N0 * p^(k-1), a MULTIPLE of the order, not the order.
//   (2) The exact order is the candidate stripped: for every prime q dividing c, while q divides the current value n and Q^(n/q) = I modulo p^k, n = n / q.
//       The order divides n throughout, and no prime can be removed at the end, so n is the order. No assumption about pi(p^k) = p^(k-1) pi(p) is used.
// Factoring: m by trial division up to 65535 (a cofactor above 1 with no factor up to 65535 is a prime since m < 2^32), N0 by trial division up to its square root
// (N0 < 2^33 + 2, so at most about 46000 odd candidates), each prime stripped as soon as it is found, so no array is needed. Q^n modulo a modulus below 2^32 is
// the fast doubling of (F(n), F(n + 1)): F(2k) = F(k) (2 F(k+1) - F(k)), F(2k + 1) = F(k)^2 + F(k+1)^2.
// Bounds (u64): residues are below the modulus, at most 2^32 - 1, so every product is below 2^64 and is reduced at once; 2 b + mod - a is below 3 * 2^32 and
// is reduced before the product; two reduced squares sum below 2^33. The candidate is at most 3 m (2(p + 1) / p <= 3; p = 2 gives 3 * 2^k), below 2^34. The LCM of
// any set of the prime-power periods divides pi(m) <= 6 m < 2^35, and a / gcd(a, b) * b equals that LCM, so nothing overflows. Integer only: no floating point,
// no 128-bit type, no builtin, no heap, no recursion, no array, no mutable static.
#include "pisano.h"

namespace lumora::pisano {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// true iff Q^n = I modulo mod, i.e. F(n) = 0 and F(n + 1) = 1 % mod (n >= 1, mod >= 1).
bool is_identity(u64 n, u64 mod) {
  u64 a = 0u;        // F(k)
  u64 b = 1u % mod;  // F(k + 1)
  for (int bit = 63; bit >= 0; --bit) {
    const u64 t = (2u * b + mod - a) % mod;
    const u64 c = (t * a) % mod;                     // F(2k)
    const u64 d = ((a * a) % mod + (b * b) % mod) % mod;  // F(2k + 1)
    if (((n >> bit) & 1u) != 0u) {
      a = d;
      b = (c + d) % mod;
    } else {
      a = c;
      b = d;
    }
  }
  return a == 0u && b == 1u % mod;
}

// removes the prime q from n while the identity still holds modulo mod
u64 strip(u64 n, u64 q, u64 mod) {
  while (n % q == 0u && is_identity(n / q, mod)) n /= q;
  return n;
}

// the order of Q modulo pk = p^k (k >= 1)
u64 order_prime_power(u64 p, u32 k, u64 pk) {
  u64 n0;
  if (p == 5u) {
    n0 = 20u;
  } else if (p % 5u == 1u || p % 5u == 4u) {
    n0 = p - 1u;
  } else {
    n0 = 2u * (p + 1u);
  }
  u64 n = n0;
  for (u32 i = 1; i < k; ++i) n *= p;  // the candidate n0 * p^(k-1)
  u64 rem = n0;
  for (u64 q = 2u; q * q <= rem; q += (q == 2u) ? 1u : 2u) {
    if (rem % q != 0u) continue;
    while (rem % q == 0u) rem /= q;
    if (q != p) n = strip(n, q, pk);
  }
  if (rem > 1u && rem != p) n = strip(n, rem, pk);
  return strip(n, p, pk);
}

u64 gcd(u64 a, u64 b) {
  while (b != 0u) {
    const u64 t = a % b;
    a = b;
    b = t;
  }
  return a;
}
}  // namespace

PisanoResult pisano_period(std::uint32_t m, std::uint64_t* out) {
  if (out == nullptr) return PisanoResult::NullArgument;
  if (m == 0u) return PisanoResult::BadModulus;
  u64 result = 1u;
  u64 rem = m;
  for (u64 q = 2u; q * q <= rem; q += (q == 2u) ? 1u : 2u) {
    if (rem % q != 0u) continue;
    u32 k = 0u;
    u64 pk = 1u;
    while (rem % q == 0u) {
      rem /= q;
      pk *= q;
      ++k;
    }
    const u64 per = order_prime_power(q, k, pk);
    result = result / gcd(result, per) * per;
  }
  if (rem > 1u) {  // a prime with no factor up to 65535
    const u64 per = order_prime_power(rem, 1u, rem);
    result = result / gcd(result, per) * per;
  }
  *out = result;
  return PisanoResult::Ok;
}
}  // namespace lumora::pisano
