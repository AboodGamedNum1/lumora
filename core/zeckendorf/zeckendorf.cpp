// Fibonacci numbers and Zeckendorf representations (contract in zeckendorf.h). Integer only: no heap, no recursion, no mutable static, no array, no table, no 128-bit type, no builtin.
// Method and proofs: docs/zeckendorf.md.
//   A pair (hi, lo) = (F(k), F(k - 1)) walks up one step to (hi + lo, hi) and down one step to (lo, hi - lo). The walk up stops at k = 93, so F(94) is never formed.
//   zeckendorf: greedy from the largest F(k) <= n, in two passes (count, then write) so a too-small buffer is detected before anything is written.
//   zeckendorf_value: the whole list is checked first (BadArgument), then summed from the top index downward with an overflow test before each add (TooLarge).
#include "zeckendorf.h"

namespace lumora::zeckendorf {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u64 kMax64 = 18446744073709551615ull;

// Greedy expansion of n >= 1. Writes the indices to out when out is not null. Returns the number of terms.
u32 expand(u64 n, std::uint8_t* out) {
  u32 k = 2u;
  u64 hi = 1u;  // F(k)
  u64 lo = 1u;  // F(k - 1)
  while (k < kMaxIndex && hi + lo <= n) {
    const u64 t = hi + lo;
    lo = hi;
    hi = t;
    ++k;
  }
  u32 terms = 0u;
  u64 rem = n;
  while (rem != 0u) {
    while (hi > rem) {
      const u64 t = hi - lo;
      hi = lo;
      lo = t;
      --k;
    }
    if (out != nullptr) out[terms] = static_cast<std::uint8_t>(k);
    ++terms;
    rem -= hi;
  }
  return terms;
}
}  // namespace

ZkResult fib(std::uint32_t k, std::uint64_t* out) {
  if (out == nullptr) return ZkResult::NullArgument;
  if (k > kMaxIndex) return ZkResult::TooLarge;
  u64 a = 0u;  // F(i - 1)
  u64 b = 1u;  // F(i)
  if (k == 0u) {
    *out = 0u;
    return ZkResult::Ok;
  }
  for (u32 i = 1u; i < k; ++i) {
    const u64 t = a + b;
    a = b;
    b = t;
  }
  *out = b;
  return ZkResult::Ok;
}

ZkResult zeckendorf(std::uint64_t n, std::uint8_t* idx, std::uint32_t cap, std::uint32_t* count) {
  if (idx == nullptr || count == nullptr) return ZkResult::NullArgument;
  const u32 terms = n == 0u ? 0u : expand(n, nullptr);
  if (cap < terms) return ZkResult::BufferTooSmall;
  if (n != 0u) expand(n, idx);
  *count = terms;
  return ZkResult::Ok;
}

ZkResult zeckendorf_value(const std::uint8_t* idx, std::uint32_t count, std::uint64_t* out) {
  if (out == nullptr) return ZkResult::NullArgument;
  if (idx == nullptr && count > 0u) return ZkResult::NullArgument;
  for (u32 i = 0u; i < count; ++i) {
    const u32 v = idx[i];
    if (v < 2u || v > kMaxIndex) return ZkResult::BadArgument;
    if (i > 0u && v + 2u > idx[i - 1u]) return ZkResult::BadArgument;
  }
  u64 sum = 0u;
  if (count > 0u) {
    u32 k = 2u;
    u64 hi = 1u;  // F(k)
    u64 lo = 1u;  // F(k - 1)
    while (k < idx[0]) {
      const u64 t = hi + lo;
      lo = hi;
      hi = t;
      ++k;
    }
    for (u32 i = 0u; i < count; ++i) {
      while (k > idx[i]) {
        const u64 t = hi - lo;
        hi = lo;
        lo = t;
        --k;
      }
      if (hi > kMax64 - sum) return ZkResult::TooLarge;
      sum += hi;
    }
  }
  *out = sum;
  return ZkResult::Ok;
}
}  // namespace lumora::zeckendorf
