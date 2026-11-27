// Maximum matching of a bipartite edge list, and for every vertex whether it is matched in every, some or no maximum matching. Iterative
// Hopcroft-Karp for the size, then alternating-path reachability from the unmatched vertices of each side (Dulmage-Mendelsohn). All working memory
// is caller scratch. See matching.h for the contract.
#include "matching.h"

namespace lumora::matching {

namespace {
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u32 kNone = 0xFFFFFFFFu;
}  // namespace

MatchResult matching_scratch_words(u32 left, u32 right, u32 edges, u32* out_words) {
  if (out_words == nullptr) return MatchResult::NullArgument;
  if (left > kMaxSide || right > kMaxSide || edges > kMaxEdges) return MatchResult::TooLarge;
  *out_words = 8u * left + 8u * right + 2u * edges + 2u;
  return MatchResult::Ok;
}

MatchResult matching_run(u32 left, u32 right, u32 edges, const u32* eu, const u32* ev, u32* scratch, u32 scratch_words, u8* cl, u8* cr,
                         u32* out_size) {
  if (out_size == nullptr) return MatchResult::NullArgument;
  if (edges > 0 && (eu == nullptr || ev == nullptr)) return MatchResult::NullArgument;
  if (static_cast<u64>(left) + right > 0 && scratch == nullptr) return MatchResult::NullArgument;
  if (left > 0 && cl == nullptr) return MatchResult::NullArgument;
  if (right > 0 && cr == nullptr) return MatchResult::NullArgument;
  if (left > kMaxSide || right > kMaxSide || edges > kMaxEdges) return MatchResult::TooLarge;
  if (static_cast<u64>(left) + right > 0 && static_cast<u64>(scratch_words) < 8ull * left + 8ull * right + 2ull * edges + 2ull)
    return MatchResult::ScratchTooSmall;
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] >= left || ev[i] >= right) return MatchResult::BadNode;
  }
  if (left == 0 && right == 0) {
    *out_size = 0;
    return MatchResult::Ok;
  }

  // Scratch layout, in words: lstart[L+1], ladj[E], rstart[R+1], radj[E], mL[L], mR[R], dist[L], it[L], stk[L], seenL[L], seenR[R], q[L+R].
  // In total 7L + 4R + 2E + 2 words, inside 8L + 8R + 2E + 2.
  u32* lstart = scratch;
  u32* ladj = lstart + left + 1u;
  u32* rstart = ladj + edges;
  u32* radj = rstart + right + 1u;
  u32* mL = radj + edges;
  u32* mR = mL + left;
  u32* dist = mR + right;
  u32* it = dist + left;
  u32* stk = it + left;
  u32* seenL = stk + left;
  u32* seenR = seenL + left;
  u32* q = seenR + right;

  for (u32 v = 0; v <= left; ++v) lstart[v] = 0;
  for (u32 w = 0; w <= right; ++w) rstart[w] = 0;
  for (u32 i = 0; i < edges; ++i) {
    ++lstart[eu[i] + 1u];
    ++rstart[ev[i] + 1u];
  }
  for (u32 v = 0; v < left; ++v) lstart[v + 1u] += lstart[v];
  for (u32 w = 0; w < right; ++w) rstart[w + 1u] += rstart[w];
  for (u32 v = 0; v < left; ++v) it[v] = lstart[v];  // fill cursors (left)
  for (u32 w = 0; w < right; ++w) seenR[w] = rstart[w];  // fill cursors (right)
  for (u32 i = 0; i < edges; ++i) {
    ladj[it[eu[i]]++] = ev[i];
    radj[seenR[ev[i]]++] = eu[i];
  }
  for (u32 v = 0; v < left; ++v) mL[v] = kNone;
  for (u32 w = 0; w < right; ++w) mR[w] = kNone;

  u32 size = 0;
  for (;;) {
    // Breadth-first layers from the free left vertices.
    u32 qh = 0, qt = 0;
    for (u32 v = 0; v < left; ++v) {
      if (mL[v] == kNone) {
        dist[v] = 0;
        q[qt++] = v;
      } else {
        dist[v] = kNone;
      }
    }
    bool found = false;
    while (qh < qt) {
      const u32 u = q[qh++];
      for (u32 a = lstart[u]; a < lstart[u + 1u]; ++a) {
        const u32 y = mR[ladj[a]];
        if (y == kNone) {
          found = true;
        } else if (dist[y] == kNone) {
          dist[y] = dist[u] + 1u;
          q[qt++] = y;
        }
      }
    }
    if (!found) break;
    for (u32 v = 0; v < left; ++v) it[v] = lstart[v];
    for (u32 s = 0; s < left; ++s) {
      if (mL[s] != kNone) continue;
      u32 depth = 0;
      stk[depth++] = s;
      while (depth > 0) {
        const u32 u = stk[depth - 1u];
        if (it[u] < lstart[u + 1u]) {
          const u32 w = ladj[it[u]++];
          const u32 y = mR[w];
          if (y == kNone) {
            for (u32 k = depth; k > 0; --k) {
              const u32 x = stk[k - 1u];
              const u32 wx = ladj[it[x] - 1u];
              mL[x] = wx;
              mR[wx] = x;
            }
            ++size;
            depth = 0;
          } else if (dist[y] == dist[u] + 1u) {
            stk[depth++] = y;
          }
        } else {
          dist[u] = kNone;
          --depth;
        }
      }
    }
  }

  // Reachability. A left vertex is unmatched in some maximum matching exactly when it is free or reachable from a free left vertex by an
  // alternating path of even length (non-matching edge to a right vertex, then its matching edge back); the right side is symmetric.
  for (u32 v = 0; v < left; ++v) seenL[v] = 0;
  for (u32 w = 0; w < right; ++w) seenR[w] = 0;
  u32 qh = 0, qt = 0;
  for (u32 v = 0; v < left; ++v) {
    if (mL[v] == kNone) {
      seenL[v] = 1;
      q[qt++] = v;
    }
  }
  while (qh < qt) {
    const u32 u = q[qh++];
    for (u32 a = lstart[u]; a < lstart[u + 1u]; ++a) {
      const u32 y = mR[ladj[a]];
      if (y != kNone && seenL[y] == 0) {
        seenL[y] = 1;
        q[qt++] = y;
      }
    }
  }
  // Right side: the same search from the free right vertices, marking seenR directly.
  qh = 0;
  qt = 0;
  for (u32 w = 0; w < right; ++w) {
    if (mR[w] == kNone) {
      seenR[w] = 1;
      q[qt++] = w;
    }
  }
  while (qh < qt) {
    const u32 w = q[qh++];
    for (u32 a = rstart[w]; a < rstart[w + 1u]; ++a) {
      const u32 x = mL[radj[a]];
      if (x != kNone && seenR[x] == 0) {
        seenR[x] = 1;
        q[qt++] = x;
      }
    }
  }
  for (u32 v = 0; v < left; ++v) cl[v] = (lstart[v + 1u] == lstart[v]) ? 0u : (seenL[v] != 0u ? 1u : 2u);
  for (u32 w = 0; w < right; ++w) cr[w] = (rstart[w + 1u] == rstart[w]) ? 0u : (seenR[w] != 0u ? 1u : 2u);
  *out_size = size;
  return MatchResult::Ok;
}

}  // namespace lumora::matching
