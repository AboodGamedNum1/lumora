// Suffix array by prefix doubling with counting sorts (O(n log n), iterative), and LCP array by Kasai's method. Integer only, no allocation,
// no recursion, no mutable statics. The only fixed-size working array outside the caller scratch is 256 local counters for the first byte sort.
#include "suffix.h"

namespace lumora::suffix {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kNoRank = 0xFFFFFFFFu;

}  // namespace

SufResult suffix_array_u8(const std::uint8_t* text, u32 n, u32* sa_out, u32* scratch, u32 scratch_words) {
  const u64 need = (n > 1) ? 3ull * n : 0ull;
  if (n > 0 && (text == nullptr || sa_out == nullptr)) return SufResult::NullArgument;
  if (need > 0 && scratch == nullptr) return SufResult::NullArgument;
  if (n > kMaxLen) return SufResult::TooLarge;
  if (static_cast<u64>(scratch_words) < need) return SufResult::ScratchTooSmall;
  if (n == 0) return SufResult::Ok;
  if (n == 1) {
    sa_out[0] = 0;
    return SufResult::Ok;
  }
  u32* rk = scratch;
  u32* tmp = scratch + n;
  u32* cnt = scratch + 2u * n;

  // Round 0: stable counting sort on the byte value, then dense ranks.
  u32 first[256];
  for (u32 c = 0; c < 256u; ++c) first[c] = 0;
  for (u32 i = 0; i < n; ++i) ++first[text[i]];
  u32 sum = 0;
  for (u32 c = 0; c < 256u; ++c) {
    const u32 count = first[c];
    first[c] = sum;
    sum += count;
  }
  for (u32 i = 0; i < n; ++i) sa_out[first[text[i]]++] = i;
  rk[sa_out[0]] = 0;
  for (u32 i = 1; i < n; ++i) rk[sa_out[i]] = rk[sa_out[i - 1]] + ((text[sa_out[i]] != text[sa_out[i - 1]]) ? 1u : 0u);
  u32 classes = rk[sa_out[n - 1]] + 1u;

  // Doubling: after the round with step k, ranks describe prefixes of length 2k.
  for (u32 k = 1; classes < n; k *= 2u) {
    // tmp = positions ordered by their second key (the rank of position + k; suffixes without one come first).
    u32 m = 0;
    for (u32 i = n - k; i < n; ++i) tmp[m++] = i;
    for (u32 j = 0; j < n; ++j) {
      if (sa_out[j] >= k) tmp[m++] = sa_out[j] - k;
    }
    // Stable counting sort of tmp by first key into sa_out.
    for (u32 c = 0; c < classes; ++c) cnt[c] = 0;
    for (u32 i = 0; i < n; ++i) ++cnt[rk[i]];
    u32 acc = 0;
    for (u32 c = 0; c < classes; ++c) {
      const u32 count = cnt[c];
      cnt[c] = acc;
      acc += count;
    }
    for (u32 j = 0; j < n; ++j) {
      const u32 p = tmp[j];
      sa_out[cnt[rk[p]]++] = p;
    }
    // New dense ranks into tmp, then swap the roles of rk and tmp.
    tmp[sa_out[0]] = 0;
    u32 top = 0;
    for (u32 i = 1; i < n; ++i) {
      const u32 x = sa_out[i - 1];
      const u32 y = sa_out[i];
      const u32 sx = (x + k < n) ? rk[x + k] + 1u : 0u;
      const u32 sy = (y + k < n) ? rk[y + k] + 1u : 0u;
      if (rk[x] != rk[y] || sx != sy) ++top;
      tmp[y] = top;
    }
    u32* swap = rk;
    rk = tmp;
    tmp = swap;
    classes = top + 1u;
  }
  return SufResult::Ok;
}

SufResult lcp_array_u8(const std::uint8_t* text, u32 n, const u32* sa, u32* lcp_out, u32* scratch, u32 scratch_words) {
  const u64 need = (n > 0) ? static_cast<u64>(n) : 0ull;
  if (n > 0 && (text == nullptr || sa == nullptr || lcp_out == nullptr)) return SufResult::NullArgument;
  if (need > 0 && scratch == nullptr) return SufResult::NullArgument;
  if (n > kMaxLen) return SufResult::TooLarge;
  if (static_cast<u64>(scratch_words) < need) return SufResult::ScratchTooSmall;
  if (n == 0) return SufResult::Ok;
  u32* rank = scratch;
  for (u32 i = 0; i < n; ++i) rank[i] = kNoRank;
  for (u32 i = 0; i < n; ++i) {
    const u32 s = sa[i];
    if (s >= n || rank[s] != kNoRank) return SufResult::BadSuffixArray;
    rank[s] = i;
  }
  lcp_out[0] = 0;
  u32 h = 0;
  for (u32 i = 0; i < n; ++i) {
    const u32 r = rank[i];
    if (r == 0) {
      h = 0;
      continue;
    }
    const u32 j = sa[r - 1];
    while (i + h < n && j + h < n && text[i + h] == text[j + h]) ++h;
    lcp_out[r] = h;
    if (h > 0) --h;
  }
  return SufResult::Ok;
}

}  // namespace lumora::suffix
