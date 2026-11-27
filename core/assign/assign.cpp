// Minimum-cost assignment (contract in assign.h). The Hungarian method with potentials (rows are added one at a time, each by a shortest augmenting
// path found with a Dijkstra-like scan), iterative, O(n^3) steps, integer only, no allocation. All numbers are unsigned 64-bit: the row potential u[i]
// and the column potential w[j] (the negated column potential of the usual formulation) are never negative, so no signed arithmetic is needed. The
// reduced cost of an edge is cost + w[j] - u[i] and is never negative. The proof of the step count and of the range of every value is in docs/assign.md.
#include "assign.h"

namespace lumora::assign {

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u64 kInf = 0xFFFFFFFFFFFFFFFFull;

// A u64 array stored as two u32 words per entry in the caller's scratch (no pointer casts).
u64 get(const u32* a, u32 i) { return (static_cast<u64>(a[2u * i + 1u]) << 32) | static_cast<u64>(a[2u * i]); }
void put(u32* a, u32 i, u64 v) {
  a[2u * i] = static_cast<u32>(v);
  a[2u * i + 1u] = static_cast<u32>(v >> 32);
}
}  // namespace

AssignResult assign_scratch_words(u32 n, u32* out_words) {
  if (out_words == nullptr) return AssignResult::NullArgument;
  if (n > kMaxN) return AssignResult::TooLarge;
  *out_words = 16u * n + 16u;
  return AssignResult::Ok;
}

AssignResult assign_run(u32 n, const u32* cost, u32* scratch, u32 scratch_words, u64* out_total, u32* out_col) {
  if (out_total == nullptr) return AssignResult::NullArgument;
  if (n > 0 && (cost == nullptr || scratch == nullptr || out_col == nullptr)) return AssignResult::NullArgument;
  if (n > kMaxN) return AssignResult::TooLarge;
  if (n > 0 && static_cast<u64>(scratch_words) < 16ull * n + 16ull) return AssignResult::ScratchTooSmall;
  if (n == 0) {
    *out_total = 0;
    return AssignResult::Ok;
  }

  // Scratch layout, in words, with M = n + 1 (index 0 is the sentinel column): u[M], w[M], minv[M] (two words per entry), p[M], way[M], used[M].
  // In total 9 * (n + 1), inside 16n + 16.
  const u32 M = n + 1u;
  u32* uu = scratch;
  u32* ww = uu + 2u * M;
  u32* mv = ww + 2u * M;
  u32* p = mv + 2u * M;    // p[j]: row matched to column j (1-based), 0 when free
  u32* way = p + M;        // way[j]: previous column on the alternating path
  u32* used = way + M;     // 1 when the column is in the tree of this phase
  for (u32 j = 0; j < M; ++j) {
    put(uu, j, 0);
    put(ww, j, 0);
    p[j] = 0;
  }

  for (u32 i = 1; i <= n; ++i) {
    p[0] = i;
    u32 j0 = 0;
    for (u32 j = 0; j < M; ++j) {
      put(mv, j, kInf);
      used[j] = 0;
    }
    do {
      used[j0] = 1;
      const u32 i0 = p[j0];
      const u64 ui0 = get(uu, i0);
      const u32* row = cost + static_cast<u64>(i0 - 1u) * n;
      u64 delta = kInf;
      u32 j1 = 0;
      for (u32 j = 1; j <= n; ++j) {
        if (used[j] != 0) continue;
        const u64 cur = (static_cast<u64>(row[j - 1u]) + get(ww, j)) - ui0;
        if (cur < get(mv, j)) {
          put(mv, j, cur);
          way[j] = j0;
        }
        const u64 mj = get(mv, j);
        if (mj < delta) {
          delta = mj;
          j1 = j;
        }
      }
      for (u32 j = 0; j <= n; ++j) {
        if (used[j] != 0) {
          const u32 r = p[j];
          put(uu, r, get(uu, r) + delta);
          put(ww, j, get(ww, j) + delta);
        } else {
          put(mv, j, get(mv, j) - delta);
        }
      }
      j0 = j1;
    } while (p[j0] != 0);
    do {
      const u32 j1 = way[j0];
      p[j0] = p[j1];
      j0 = j1;
    } while (j0 != 0);
  }

  u64 total = 0;
  for (u32 j = 1; j <= n; ++j) {
    const u32 r = p[j] - 1u;
    out_col[r] = j - 1u;
    total += cost[static_cast<u64>(r) * n + (j - 1u)];
  }
  *out_total = total;
  return AssignResult::Ok;
}

}  // namespace lumora::assign
