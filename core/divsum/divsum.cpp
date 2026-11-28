// Divisor summatory functions (contract in divsum.h). Integer only: no heap, no recursion, no mutable static, no array, no 128-bit type, no builtin.
// Method and proof: docs/divsum.md. With r = floor(sqrt(n)) (found by the loop itself: the last k with k * k <= n):
//   D(n) = 2 * (sum over k = 1..r of floor(n / k)) - r^2
//   S(n) = (sum over a = 1..r of a * floor(n / a)) + (sum over b = 1..r of (T(floor(n / b)) - T(r))),  T(m) = m (m + 1) / 2.
#include "divsum.h"

namespace lumora::divsum {
namespace {
// T(m) = m (m + 1) / 2 for m below 2^32 (the halving avoids forming m * (m + 1) in the product's full width).
std::uint64_t tri(std::uint64_t m) { return (m % 2u == 0u) ? (m / 2u) * (m + 1u) : m * ((m + 1u) / 2u); }
}  // namespace

DsResult divisor_count_sum(std::uint64_t n, std::uint64_t* out) {
  if (out == nullptr) return DsResult::NullArgument;
  if (n > kMaxCountN) return DsResult::TooLarge;
  std::uint64_t sum = 0;
  std::uint64_t r = 0;
  for (std::uint64_t k = 1u; k * k <= n; ++k) {
    sum += n / k;
    r = k;
  }
  *out = 2u * sum - r * r;
  return DsResult::Ok;
}

DsResult divisor_sum_sum(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return DsResult::NullArgument;
  std::uint64_t r = 0;
  std::uint64_t first = 0;  // the sum over a <= r of a * floor(n / a)
  for (std::uint64_t k = 1u; k * k <= n; ++k) {
    first += k * (n / k);
    r = k;
  }
  const std::uint64_t tr = tri(r);
  std::uint64_t second = 0;  // the sum over b <= r of T(floor(n / b)) - T(r), each term nonnegative
  for (std::uint64_t b = 1u; b <= r; ++b) second += tri(n / b) - tr;
  *out = first + second;
  return DsResult::Ok;
}

}  // namespace lumora::divsum
