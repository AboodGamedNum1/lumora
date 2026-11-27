// Integer partition counts (contract in partition.h). Integer only, no allocation, no recursion, no 128-bit types, no arrays beyond the caller tables.
// partition_all: Euler's pentagonal recurrence in place, O(n sqrt n). partition_distinct: Q(x) E(x) = E(x^2) in place, O(n sqrt n). partition_at_most_k:
// a part-by-part table (k * k <= n) or P(x) times the product over i > k of (1 - x^i), expanded as an alternating sum of terms T_j = P / ((1 - x)...(1 - x^j))
// kept in the caller scratch. See docs/partition.md.
#include "partition.h"

namespace lumora::partition {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// p(v) for v = 0 .. n into t (n + 1 words): p(v) = sum over j >= 1 of (-1)^(j+1) (p(v - j(3j-1)/2) + p(v - j(3j+1)/2)).
void all_table(u32 n, u32* t) {
  t[0] = 1u;
  for (u32 v = 1; v <= n; ++v) {
    u64 pos = 0;
    u64 neg = 0;
    for (u64 j = 1;; ++j) {
      const u64 g1 = j * (3u * j - 1u) / 2u;
      if (g1 > v) break;
      const u64 g2 = g1 + j;  // j (3j + 1) / 2
      u64 s = t[v - g1];
      if (g2 <= v) s += t[v - g2];
      if (j % 2u == 1u) pos += s; else neg += s;
    }
    t[v] = static_cast<u32>((pos % kMod + kMod - neg % kMod) % kMod);
  }
}
}  // namespace

PartResult partition_all(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return PartResult::NullArgument;
  if (n > kMaxN) return PartResult::TooLarge;
  all_table(n, out);
  return PartResult::Ok;
}

PartResult partition_distinct(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return PartResult::NullArgument;
  if (n > kMaxN) return PartResult::TooLarge;
  // q(v) = e(v) + sum over j >= 1 of (-1)^(j+1) (q(v - g1) + q(v - g2)), where e(v) is the coefficient of x^v in E(x^2): 1 for v = 0, (-1)^j when
  // v = 2 g1(j) or v = 2 g2(j), else 0.
  out[0] = 1u;
  for (u32 v = 1; v <= n; ++v) {
    u64 pos = 0;
    u64 neg = 0;
    for (u64 j = 1;; ++j) {
      const u64 g1 = j * (3u * j - 1u) / 2u;
      if (g1 > v) break;
      const u64 g2 = g1 + j;
      u64 s = out[v - g1];
      if (g2 <= v) s += out[v - g2];
      if (j % 2u == 1u) pos += s; else neg += s;
      // The terms of E(x^2) at x^v.
      if (2u * g1 == v || 2u * g2 == v) {
        if (j % 2u == 1u) neg += 1u; else pos += 1u;
      }
    }
    out[v] = static_cast<u32>((pos % kMod + kMod - neg % kMod) % kMod);
  }
  return PartResult::Ok;
}

PartResult partition_at_most_k(std::uint32_t n, std::uint32_t k, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out) {
  if (out == nullptr || scratch == nullptr) return PartResult::NullArgument;
  if (n > kMaxN) return PartResult::TooLarge;
  if (k > kMaxN) return PartResult::TooLarge;
  if (static_cast<u64>(scratch_words) < static_cast<u64>(n) + 1u) return PartResult::ScratchTooSmall;
  if (k > n) k = n;
  if (static_cast<u64>(k) * k <= n) {
    // Part-by-part table with parts 1 .. k: at most k * n steps, which is at most n sqrt n.
    for (u32 v = 0; v <= n; ++v) out[v] = 0u;
    out[0] = 1u;
    for (u32 i = 1; i <= k; ++i) {
      for (u32 v = i; v <= n; ++v) {
        u32 x = out[v] + out[v - i];
        if (x >= kMod) x -= kMod;
        out[v] = x;
      }
    }
    return PartResult::Ok;
  }
  // k * k > n: P(x) times the product over i > k of (1 - x^i) = sum over j of (-1)^j x^(s_j) / ((1 - x) ... (1 - x^j)), s_j = j k + j (j + 1) / 2.
  // scratch holds T_j = P / ((1 - x) ... (1 - x^j)) (n + 1 words, the only working memory besides out).
  all_table(n, scratch);
  for (u32 v = 0; v <= n; ++v) out[v] = scratch[v];
  for (u64 j = 1;; ++j) {
    const u64 shift = j * k + j * (j + 1u) / 2u;
    if (shift > n) break;
    const u64 top = n - shift;  // only T_j[0 .. top] is used from here on
    for (u64 v = j; v <= top; ++v) {
      u32 x = scratch[v] + scratch[v - j];
      if (x >= kMod) x -= kMod;
      scratch[v] = x;
    }
    for (u64 v = shift; v <= n; ++v) {
      const u32 tv = scratch[v - shift];
      u32 x;
      if (j % 2u == 1u) {
        x = out[v] >= tv ? out[v] - tv : out[v] + kMod - tv;
      } else {
        x = out[v] + tv;
        if (x >= kMod) x -= kMod;
      }
      out[v] = x;
    }
  }
  return PartResult::Ok;
}
}  // namespace lumora::partition
