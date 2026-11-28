// Ranking of Dyck words as u64 bitmasks (contract in dyck.h).
// Integer only; no heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics. The number of words that finish the current prefix is
// carried along and updated by one exact division per character (see docs/dyck.md).
#include "dyck.h"

namespace lumora::dyck {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

u64 gcd(u64 x, u64 y) {
  while (y != 0u) {
    const u64 t = x % y;
    x = y;
    y = t;
  }
  return x;
}

// With cur = B(pos, depth), the number of ways to finish a prefix that has pos characters left and depth open parentheses (B(r, k) counts the walks of r steps
// from height k down to 0 that never go below 0), this is B(pos - 1, depth + 1), the number of completions that put '(' next:
// B(pos - 1, depth + 1) = B(pos, depth) * (depth + 2) * m / (pos * (depth + 1)) with m = (pos - depth) / 2 (from B(r, k) = (k + 1) / (r + 1) * C(r + 1, (r - k) / 2)).
// It is 0 when m = 0. The division is exact and is done as (cur / g) * (((depth + 2) * m) / (den / g)) with g = gcd(cur, den), den = pos * (depth + 1) at most 2112, so
// no intermediate exceeds cur.
u64 up_count(u32 pos, u32 depth, u64 cur) {
  const u64 den = static_cast<u64>(pos) * (static_cast<u64>(depth) + 1u);
  const u64 num = (static_cast<u64>(depth) + 2u) * ((pos - depth) / 2u);
  const u64 g = gcd(cur, den);
  return (cur / g) * (num / (den / g));
}

// 1 when mask is a Dyck word of semilength n (n at most 32). The shift 2n is only taken for n below 32.
bool valid(u64 mask, u32 n) {
  if (n < 32u && (mask >> (2u * n)) != 0u) return false;
  u32 depth = 0u;
  for (u32 pos = 2u * n; pos > 0u; --pos) {
    if (((mask >> (pos - 1u)) & 1u) != 0u) {
      if (depth == 0u) return false;
      --depth;
    } else {
      ++depth;
    }
  }
  return depth == 0u;
}

// C_n by C_(i+1) = C_i * 2 (2 i + 1) / (i + 2): the product is at most C_31 * 126 < 2^61 and the division is exact.
u64 catalan(u32 n) {
  u64 c = 1u;
  for (u32 i = 0u; i < n; ++i) c = c * (2u * (2u * static_cast<u64>(i) + 1u)) / (static_cast<u64>(i) + 2u);
  return c;
}
}  // namespace

DyResult dyck_count(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return DyResult::NullArgument;
  if (n > kMaxN) return DyResult::BadArgument;
  *out = catalan(n);
  return DyResult::Ok;
}

DyResult dyck_rank(std::uint64_t mask, std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return DyResult::NullArgument;
  if (n > kMaxN) return DyResult::BadArgument;
  if (!valid(mask, n)) return DyResult::BadMask;
  u64 rank = 0u;
  u32 depth = 0u;
  u64 cur = catalan(n);
  for (u32 pos = 2u * n; pos > 0u; --pos) {
    const u64 up = up_count(pos, depth, cur);
    if (((mask >> (pos - 1u)) & 1u) != 0u) {
      rank += up;
      cur -= up;
      --depth;
    } else {
      cur = up;
      ++depth;
    }
  }
  *out = rank;
  return DyResult::Ok;
}

DyResult dyck_unrank(std::uint32_t n, std::uint64_t rank, std::uint64_t* out) {
  if (out == nullptr) return DyResult::NullArgument;
  if (n > kMaxN) return DyResult::BadArgument;
  if (rank >= catalan(n)) return DyResult::RankTooLarge;
  u64 mask = 0u;
  u32 depth = 0u;
  u64 cur = catalan(n);
  for (u32 pos = 2u * n; pos > 0u; --pos) {
    const u64 up = up_count(pos, depth, cur);
    if (rank < up) {
      cur = up;
      ++depth;
    } else {
      rank -= up;
      cur -= up;
      mask |= static_cast<u64>(1u) << (pos - 1u);
      --depth;
    }
  }
  *out = mask;
  return DyResult::Ok;
}

DyResult dyck_next(std::uint64_t mask, std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return DyResult::NullArgument;
  if (n > kMaxN) return DyResult::BadArgument;
  if (!valid(mask, n)) return DyResult::BadMask;
  // The rightmost '(' with an open parenthesis before it becomes ')'; the rest is the least completion: as many '(' as fit, then ')'.
  u32 best_pos = 0u;
  u32 best_depth = 0u;
  u32 depth = 0u;
  for (u32 pos = 2u * n; pos > 0u; --pos) {
    if (((mask >> (pos - 1u)) & 1u) != 0u) {
      --depth;
    } else {
      if (depth >= 1u) {
        best_pos = pos;
        best_depth = depth;
      }
      ++depth;
    }
  }
  if (best_pos == 0u) return DyResult::NoNext;
  const u32 opens = (best_pos - best_depth) / 2u;
  const u32 closes = best_depth - 1u + opens;
  const u64 high = (mask >> (best_pos - 1u)) << (best_pos - 1u);
  const u64 low = (static_cast<u64>(1u) << closes) - 1u;
  *out = high | (static_cast<u64>(1u) << (best_pos - 1u)) | low;
  return DyResult::Ok;
}
}  // namespace lumora::dyck
