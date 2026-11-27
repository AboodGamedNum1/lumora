// Permutations of small arrays: next lexicographic arrangement, rank and unrank. See perm.h for the contract. Integer only: no allocation,
// no recursion, no global state. Rank and unrank use the factorial number system in 64-bit arithmetic (20! fits).
#include "perm.h"

namespace lumora::perm {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

// n! for n in 0 .. 20, computed by a loop (no table, no overflow: 20! < 2^64).
u64 factorial(u32 n) {
  u64 f = 1;
  for (u32 i = 2; i <= n; ++i) f *= i;
  return f;
}

}  // namespace

PermResult next_permutation_u32(u32* a, u32 n, bool* out_has_next) {
  if (out_has_next == nullptr) return PermResult::NullArgument;
  if (a == nullptr && n > 0) return PermResult::NullArgument;
  if (n > kMaxNextN) return PermResult::TooLarge;
  if (n < 2) {
    *out_has_next = false;
    return PermResult::Ok;
  }
  // Find the rightmost i with a[i] < a[i + 1]; i + 1 is the start of the non-increasing tail.
  u32 tail = n - 1;
  while (tail > 0 && a[tail - 1] >= a[tail]) --tail;
  bool has_next = false;
  if (tail > 0) {
    const u32 pivot = tail - 1;
    // Rightmost element of the tail that is strictly greater than the pivot value.
    u32 j = n - 1;
    while (a[j] <= a[pivot]) --j;
    const u32 t = a[pivot];
    a[pivot] = a[j];
    a[j] = t;
    has_next = true;
  }
  // Reverse the tail (it is non-increasing, so reversing makes it ascending).
  u32 lo = tail;
  u32 hi = n - 1;
  while (lo < hi) {
    const u32 t = a[lo];
    a[lo] = a[hi];
    a[hi] = t;
    ++lo;
    --hi;
  }
  *out_has_next = has_next;
  return PermResult::Ok;
}

PermResult perm_rank(const u32* perm, u32 n, u64* out_rank) {
  if (out_rank == nullptr) return PermResult::NullArgument;
  if (perm == nullptr && n > 0) return PermResult::NullArgument;
  if (n > kMaxRankN) return PermResult::TooLarge;
  u32 seen = 0;
  for (u32 i = 0; i < n; ++i) {
    if (perm[i] >= n) return PermResult::NotPermutation;
    const u32 bit = 1u << perm[i];
    if ((seen & bit) != 0) return PermResult::NotPermutation;
    seen |= bit;
  }
  u64 rank = 0;
  for (u32 i = 0; i < n; ++i) {
    // Number of later elements smaller than perm[i].
    u32 smaller = 0;
    for (u32 k = i + 1; k < n; ++k) {
      if (perm[k] < perm[i]) ++smaller;
    }
    rank += static_cast<u64>(smaller) * factorial(n - 1 - i);
  }
  *out_rank = rank;
  return PermResult::Ok;
}

PermResult perm_unrank(u32 n, u64 rank, u32* out_perm) {
  if (out_perm == nullptr && n > 0) return PermResult::NullArgument;
  if (n > kMaxRankN) return PermResult::TooLarge;
  if (rank >= factorial(n)) return PermResult::OutOfRange;
  u32 used = 0;
  u64 rest = rank;
  for (u32 i = 0; i < n; ++i) {
    const u64 f = factorial(n - 1 - i);
    u32 idx = static_cast<u32>(rest / f);  // index among the unused values, ascending
    rest %= f;
    u32 v = 0;
    for (;; ++v) {
      if ((used & (1u << v)) != 0) continue;
      if (idx == 0) break;
      --idx;
    }
    used |= 1u << v;
    out_perm[i] = v;
  }
  return PermResult::Ok;
}

}  // namespace lumora::perm
