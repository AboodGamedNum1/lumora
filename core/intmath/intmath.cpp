// Exact integer helpers. See intmath.h for the contract. Integer only: no __int128, no builtins or intrinsics, no allocation, no
// recursion, no global state.
#include "intmath.h"

namespace lumora::intmath {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr u64 kU64Max = ~0ull;
constexpr i64 kI64Max = 0x7FFFFFFFFFFFFFFFll;
constexpr i64 kI64Min = -kI64Max - 1;

// Full 128-bit product of two u64 as (hi, lo), from 32-bit halves.
void mul_wide(u64 a, u64 b, u64* hi, u64* lo) {
  const u64 a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
  const u64 b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const u64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const u64 mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  *lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
  *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

}  // namespace

u32 isqrt_u64(u64 x) {
  u64 res = 0;
  u64 bit = 1ull << 62;
  while (bit > x) bit >>= 2;
  while (bit != 0) {
    if (x >= res + bit) {
      x -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<u32>(res);
}

u64 gcd_u64(u64 a, u64 b) {
  while (b != 0) {
    const u64 t = a % b;
    a = b;
    b = t;
  }
  return a;
}

bool lcm_u64(u64 a, u64 b, u64* out) {
  if (out == nullptr) return false;
  if (a == 0 || b == 0) {
    *out = 0;
    return true;
  }
  const u64 q = a / gcd_u64(a, b);
  if (q > kU64Max / b) return false;
  *out = q * b;
  return true;
}

bool add_i64(i64 a, i64 b, i64* out) {
  if (out == nullptr) return false;
  if (b > 0 ? a > kI64Max - b : a < kI64Min - b) return false;
  *out = a + b;
  return true;
}

bool sub_i64(i64 a, i64 b, i64* out) {
  if (out == nullptr) return false;
  if (b > 0 ? a < kI64Min + b : a > kI64Max + b) return false;
  *out = a - b;
  return true;
}

bool mul_i64(i64 a, i64 b, i64* out) {
  if (out == nullptr) return false;
  if (a == 0 || b == 0) {
    *out = 0;
    return true;
  }
  const bool neg = (a < 0) != (b < 0);
  const u64 ua = a < 0 ? 0ull - static_cast<u64>(a) : static_cast<u64>(a);
  const u64 ub = b < 0 ? 0ull - static_cast<u64>(b) : static_cast<u64>(b);
  const u64 limit = neg ? (1ull << 63) : static_cast<u64>(kI64Max);
  if (ua > limit / ub) return false;
  const u64 m = ua * ub;
  *out = neg ? static_cast<i64>(0ull - m) : static_cast<i64>(m);
  return true;
}

bool mul_div_u64(u64 a, u64 b, u64 d, Round mode, u64* out) {
  if (out == nullptr || d == 0) return false;
  if (mode != Round::Down && mode != Round::Up && mode != Round::NearestUp) return false;
  u64 hi = 0, lo = 0;
  mul_wide(a, b, &hi, &lo);
  u64 qhi = 0, qlo = 0, rem = 0;
  for (int i = 127; i >= 0; --i) {
    const u64 word = i >= 64 ? hi : lo;
    const u64 in = (word >> (i & 63)) & 1u;
    const u64 carry = rem >> 63;
    rem = (rem << 1) | in;
    if (carry != 0 || rem >= d) {
      rem -= d;
      if (i >= 64) qhi |= 1ull << (i - 64);
      else qlo |= 1ull << i;
    }
  }
  if (qhi != 0) return false;
  bool bump = false;
  if (mode == Round::Up) bump = rem != 0;
  else if (mode == Round::NearestUp) bump = rem >= d - rem;
  if (bump) {
    if (qlo == kU64Max) return false;
    ++qlo;
  }
  *out = qlo;
  return true;
}

}  // namespace lumora::intmath
