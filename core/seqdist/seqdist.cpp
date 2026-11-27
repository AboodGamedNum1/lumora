// Sequence distances: longest common subsequence, Levenshtein distance, longest increasing subsequence. See seqdist.h for the contract.
// Integer only: no allocation, no recursion, no global state. All working memory is the caller scratch; it is written before it is read and
// only its first `need` words are touched.
#include "seqdist.h"

namespace lumora::seqdist {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Shared front part of lcs and edit distance. Returns Ok when the call should go on to the computation.
SeqResult check_pair(const u32* a, u32 na, const u32* b, u32 nb, const u32* scratch, u32 scratch_words, const u32* out) {
  if (out == nullptr) return SeqResult::NullArgument;
  if (a == nullptr && na > 0) return SeqResult::NullArgument;
  if (b == nullptr && nb > 0) return SeqResult::NullArgument;
  const u64 shorter = (na < nb) ? na : nb;
  const u64 need = (na > 0 && nb > 0) ? 2 * (shorter + 1) : 0;
  if (scratch == nullptr && need > 0) return SeqResult::NullArgument;
  if (na > kMaxLen || nb > kMaxLen) return SeqResult::TooLarge;
  if (scratch_words < need) return SeqResult::ScratchTooSmall;
  return SeqResult::Ok;
}

u32 max_u32(u32 x, u32 y) { return x > y ? x : y; }
u32 min_u32(u32 x, u32 y) { return x < y ? x : y; }

}  // namespace

SeqResult lcs_length_u32(const u32* a, u32 na, const u32* b, u32 nb, u32* scratch, u32 scratch_words, u32* out) {
  const SeqResult c = check_pair(a, na, b, nb, scratch, scratch_words, out);
  if (c != SeqResult::Ok) return c;
  if (na == 0 || nb == 0) {
    *out = 0;
    return SeqResult::Ok;
  }
  // Rows run over the shorter sequence s (length m); the longer one l is walked once.
  const u32* s = (nb < na) ? b : a;
  const u32 m = (nb < na) ? nb : na;
  const u32* l = (nb < na) ? a : b;
  const u32 k = (nb < na) ? na : nb;
  u32* prev = scratch;
  u32* cur = scratch + (m + 1);
  for (u32 j = 0; j <= m; ++j) prev[j] = 0;
  for (u32 i = 0; i < k; ++i) {
    cur[0] = 0;
    for (u32 j = 1; j <= m; ++j) {
      if (l[i] == s[j - 1]) {
        cur[j] = prev[j - 1] + 1;
      } else {
        cur[j] = max_u32(prev[j], cur[j - 1]);
      }
    }
    u32* t = prev;
    prev = cur;
    cur = t;
  }
  *out = prev[m];
  return SeqResult::Ok;
}

SeqResult edit_distance_u32(const u32* a, u32 na, const u32* b, u32 nb, u32* scratch, u32 scratch_words, u32* out) {
  const SeqResult c = check_pair(a, na, b, nb, scratch, scratch_words, out);
  if (c != SeqResult::Ok) return c;
  if (na == 0 || nb == 0) {
    *out = (na == 0) ? nb : na;
    return SeqResult::Ok;
  }
  const u32* s = (nb < na) ? b : a;
  const u32 m = (nb < na) ? nb : na;
  const u32* l = (nb < na) ? a : b;
  const u32 k = (nb < na) ? na : nb;
  u32* prev = scratch;
  u32* cur = scratch + (m + 1);
  for (u32 j = 0; j <= m; ++j) prev[j] = j;
  for (u32 i = 0; i < k; ++i) {
    cur[0] = i + 1;
    for (u32 j = 1; j <= m; ++j) {
      const u32 sub = prev[j - 1] + ((l[i] == s[j - 1]) ? 0u : 1u);
      cur[j] = min_u32(sub, min_u32(prev[j] + 1, cur[j - 1] + 1));
    }
    u32* t = prev;
    prev = cur;
    cur = t;
  }
  *out = prev[m];
  return SeqResult::Ok;
}

SeqResult lis_length_u32(const u32* a, u32 n, bool strict, u32* scratch, u32 scratch_words, u32* out) {
  if (out == nullptr) return SeqResult::NullArgument;
  if (a == nullptr && n > 0) return SeqResult::NullArgument;
  const u64 need = (n > 0) ? static_cast<u64>(n) + 1 : 0;
  if (scratch == nullptr && need > 0) return SeqResult::NullArgument;
  if (n > kMaxLen) return SeqResult::TooLarge;
  if (scratch_words < need) return SeqResult::ScratchTooSmall;
  // scratch[0 .. len) holds, for each length, the smallest possible last value of a subsequence of that length (patience tails).
  u32 len = 0;
  for (u32 i = 0; i < n; ++i) {
    const u32 x = a[i];
    u32 lo = 0;
    u32 hi = len;
    while (lo < hi) {
      const u32 mid = lo + (hi - lo) / 2;
      const bool go_right = strict ? (scratch[mid] < x) : (scratch[mid] <= x);
      if (go_right) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    scratch[lo] = x;
    if (lo == len) ++len;
  }
  *out = len;
  return SeqResult::Ok;
}

}  // namespace lumora::seqdist
