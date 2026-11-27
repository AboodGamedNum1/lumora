// Sparse table for range minimum queries. Row k of the table holds, for every start i with i + 2^k <= n, the index of the leftmost smallest
// value of a[i .. i + 2^k). Row k lives at table[k * n .. k * n + n). Integer only, no allocation, no recursion, no global state.
#include "rmq.h"

#include <cstddef>

namespace lumora::rmq {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

// floor(log2(x)) for x above 0, by a loop.
u32 floor_log2(u32 x) {
  u32 k = 0;
  while ((x >> k) > 1u) ++k;
  return k;
}

u64 need_words(u32 n) {
  if (n == 0) return 0;
  return static_cast<u64>(n) * (static_cast<u64>(floor_log2(n)) + 1u);
}

}  // namespace

RmqResult rmq_table_words(u32 n, u32* out_words) {
  if (out_words == nullptr) return RmqResult::NullArgument;
  if (n > kMaxLen) return RmqResult::TooLarge;
  *out_words = static_cast<u32>(need_words(n));
  return RmqResult::Ok;
}

RmqResult rmq_build(const u32* a, u32 n, u32* table, u32 table_words) {
  if (n > 0 && (a == nullptr || table == nullptr)) return RmqResult::NullArgument;
  if (n > kMaxLen) return RmqResult::TooLarge;
  if (static_cast<u64>(table_words) < need_words(n)) return RmqResult::ScratchTooSmall;
  if (n == 0) return RmqResult::Ok;
  for (u32 i = 0; i < n; ++i) table[i] = i;
  const u32 top = floor_log2(n);
  for (u32 k = 1; k <= top; ++k) {
    const u32 half = 1u << (k - 1);
    const u32 span = 1u << k;
    const u32* prev = table + static_cast<std::size_t>(k - 1) * n;
    u32* cur = table + static_cast<std::size_t>(k) * n;
    for (u32 i = 0; i + span <= n; ++i) {
      const u32 x = prev[i];
      const u32 y = prev[i + half];
      cur[i] = (a[x] <= a[y]) ? x : y;
    }
  }
  return RmqResult::Ok;
}

RmqResult rmq_query(const u32* a, u32 n, const u32* table, u32 table_words, u32 l, u32 r, u32* out_index) {
  if (a == nullptr || table == nullptr || out_index == nullptr) return RmqResult::NullArgument;
  if (n > kMaxLen) return RmqResult::TooLarge;
  if (static_cast<u64>(table_words) < need_words(n)) return RmqResult::ScratchTooSmall;
  if (l > r || r >= n) return RmqResult::OutOfRange;
  const u32 k = floor_log2(r - l + 1u);
  const u32* row = table + static_cast<std::size_t>(k) * n;
  const u32 x = row[l];
  const u32 y = row[r + 1u - (1u << k)];
  *out_index = (a[x] <= a[y]) ? x : y;
  return RmqResult::Ok;
}

}  // namespace lumora::rmq
