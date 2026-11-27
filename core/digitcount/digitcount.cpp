// Digit counting in a positional base (contract in digitcount.h).
//
// Notation: b is the base, k the number of base-b digits of n (0 for n = 0), d[k-1] ... d[0] its digits, top digit first, and W(i, t) the number of
// strings of i digits (each below b) whose sum is t, so W(0, .) = [1] and W(i + 1, .) is W(i, .) times (1 + x + ... + x^(b-1)).
//
// count_digit_sum: every x in 0 .. n either equals n, or has a longest common prefix with n that ends just above some position i: x agrees with n above
// position i, has a digit c < d[i] at position i, and any i digits below. These cases are disjoint and cover 0 .. n - 1 (x = 0 is the case where every digit
// is below, leading zeros included). So with pre(i) = the sum of the digits of n above position i,
//     count(n, s) = [digit sum of n == s] + sum over i = 0 .. k-1 of sum over c = 0 .. d[i]-1 of W(i, s - pre(i) - c).
// The positions are visited from the bottom, building W(i, .) in a single array in place: the prefix sums of W(i) are formed in place, then W(i + 1, t) =
// prefix(t) - prefix(t - b) for t from the top down, which reads only entries not yet overwritten.
//
// digit_sum_total: for the position of weight p = b^i, the b * p consecutive integers of one block hold every digit exactly p times, and the n + 1 integers
// 0 .. n are f = (n + 1) / (b * p) full blocks plus a remainder of r = (n + 1) % (b * p) integers. The remainder has q = r / p complete runs of p integers
// for the digits 0 .. q - 1 (a digit c < q appears p times) and r % p integers with the digit q. The contribution of the position is
//     f * p * (b * (b - 1) / 2) + p * (q * (q - 1) / 2) + q * (r % p).
//
// Bounds of every intermediate (all quantities are non-negative and every sum below is a sum of terms each at most the final value of its quantity):
//  - n + 1 <= 2^40 + 1; b * p <= 2^40 * 64 = 2^46 for every position that is visited (p <= n);
//  - W(i, t) <= b^i <= 2^40 (i < k, so b^i <= n), and the prefix sums of W(i, .) are at most b^i, so every array entry is at most 2^40;
//  - count(n, s) <= n + 1 <= 2^40 + 1;
//  - the contribution of a position is at most (n + 1) * (b - 1), so the total is at most 41 * 63 * (2^40 + 1) < 2^52.
// Working memory is automatic and of fixed size: one array of kWays = 384 u64 words and one array of 41 u32 digits (at most 425 words in all, below the 512
// words of the contract); 384 words are enough because the largest index ever used is (k - 1) * (b - 1) <= 378 (b = 64, k = 7). Integer only: no
// floating point, no 128-bit type, no builtin, no heap, no recursion, no static table.
#include "digitcount.h"

namespace lumora::digitcount {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u32 kMaxDigits = 41u;  // digits of 2^40 in base 2
constexpr u32 kWays = 384u;      // words of the W array: 1 + (k - 1) * (b - 1) <= 379 for every base and every n <= 2^40

// Writes the digits of n (least significant first) and returns their number.
u32 digits_of(u64 n, u32 base, u32* d) {
  u32 k = 0;
  while (n > 0u) {
    d[k++] = static_cast<u32>(n % base);
    n /= base;
  }
  return k;
}
}  // namespace

DigitResult count_digit_sum(u64 n, u32 base, u64 s, u64* out) {
  if (out == nullptr) return DigitResult::NullArgument;
  if (base < kMinBase || base > kMaxBase) return DigitResult::BadBase;
  if (n > kMaxN) return DigitResult::TooLarge;
  u32 d[kMaxDigits];
  const u32 k = digits_of(n, base, d);
  u64 total = 0u;  // the digit sum of n
  for (u32 i = 0; i < k; ++i) total += d[i];
  const u64 cap = static_cast<u64>(base - 1u) * k;  // no x in 0 .. n has a larger digit sum
  if (s > cap) {
    *out = 0u;
    return DigitResult::Ok;
  }
  u64 w[kWays];
  for (u32 t = 0; t < kWays; ++t) w[t] = 0u;
  w[0] = 1u;
  u64 count = (total == s) ? 1u : 0u;  // x = n itself
  u64 pre = total;                     // the sum of the digits of n above position i, starting with all of them
  for (u32 i = 0; i < k; ++i) {
    pre -= d[i];
    // the digits c below d[i] at position i: W(i, s - pre - c), where W(i, t) is w[t] for t <= i * (b - 1)
    const u64 top = static_cast<u64>(i) * (base - 1u);
    for (u32 c = 0; c < d[i]; ++c) {
      const u64 used = pre + c;
      if (s >= used && s - used <= top) count += w[s - used];
    }
    if (i + 1u < k) {
      // W(i + 1, .) = W(i, .) * (1 + x + ... + x^(b-1)), in place, indices up to (i + 1) * (b - 1)
      const u32 hi = static_cast<u32>(top);
      const u32 nhi = hi + base - 1u;
      for (u32 t = 1; t <= nhi; ++t) w[t] += w[t - 1u];  // prefix sums (entries above hi are 0, so they repeat the last prefix sum)
      for (u32 t = nhi; t >= base; --t) w[t] -= w[t - base];
    }
  }
  *out = count;
  return DigitResult::Ok;
}

DigitResult digit_sum_total(u64 n, u32 base, u64* out) {
  if (out == nullptr) return DigitResult::NullArgument;
  if (base < kMinBase || base > kMaxBase) return DigitResult::BadBase;
  if (n > kMaxN) return DigitResult::TooLarge;
  const u64 total_count = n + 1u;  // the integers 0 .. n
  const u64 pair = static_cast<u64>(base) * (base - 1u) / 2u;
  u64 sum = 0u;
  u64 p = 1u;  // b^i
  while (p <= n) {
    const u64 block = p * base;
    const u64 f = total_count / block;
    const u64 r = total_count % block;
    const u64 q = r / p;
    const u64 run_pairs = (q == 0u) ? 0u : q * (q - 1u) / 2u;  // 0 + 1 + ... + (q - 1)
    sum += f * p * pair + p * run_pairs + q * (r % p);
    p = block;
  }
  *out = sum;
  return DigitResult::Ok;
}
}  // namespace lumora::digitcount
