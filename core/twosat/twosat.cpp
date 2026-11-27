// 2-SAT (contract in twosat.h). Implication graph on the 2n literals, strongly connected components by an iterative Tarjan search (explicit stacks,
// no recursion), O(n + m) steps, integer only, no allocation. Literal l is the variable l / 2, negated when l is odd; its negation is l ^ 1. A clause
// (a OR b) gives the edges (not a) -> b and (not b) -> a.
#include "twosat.h"

namespace lumora::twosat {

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u8 = std::uint8_t;
constexpr u32 kNone = 0xFFFFFFFFu;
}  // namespace

TwoSatResult twosat_scratch_words(u32 n, u32 m, u32* out_words) {
  if (out_words == nullptr) return TwoSatResult::NullArgument;
  if (n > kMaxVars || m > kMaxClauses) return TwoSatResult::TooLarge;
  *out_words = 16u * n + 2u * m + 2u;
  return TwoSatResult::Ok;
}

TwoSatResult twosat_solve(u32 n, u32 m, const u32* la, const u32* lb, u32* scratch, u32 scratch_words, u8* value, u8* out_sat, u32* out_conflict) {
  if (out_sat == nullptr || out_conflict == nullptr) return TwoSatResult::NullArgument;
  if (m > 0 && (la == nullptr || lb == nullptr)) return TwoSatResult::NullArgument;
  if (n > 0 && (value == nullptr || scratch == nullptr)) return TwoSatResult::NullArgument;
  if (n > kMaxVars || m > kMaxClauses) return TwoSatResult::TooLarge;
  if (n > 0 && static_cast<u64>(scratch_words) < 16ull * n + 2ull * m + 2ull) return TwoSatResult::ScratchTooSmall;
  for (u32 i = 0; i < m; ++i) {
    if (la[i] >= 2u * n || lb[i] >= 2u * n) return TwoSatResult::BadLiteral;
  }
  if (n == 0) {
    *out_sat = 1;
    return TwoSatResult::Ok;
  }

  // Scratch layout, in words: start[N+1], adj[2m], idx[N], low[N], comp[N], stk[N], call[N], cur[N], with N = 2n. In total 12n + 2m + 1, inside
  // 16n + 2m + 2.
  const u32 N = 2u * n;
  u32* start = scratch;
  u32* adj = start + N + 1u;
  u32* idx = adj + 2u * m;
  u32* low = idx + N;
  u32* comp = low + N;
  u32* stk = comp + N;
  u32* call = stk + N;
  u32* cur = call + N;

  for (u32 v = 0; v <= N; ++v) start[v] = 0;
  for (u32 i = 0; i < m; ++i) {
    ++start[(la[i] ^ 1u) + 1u];
    ++start[(lb[i] ^ 1u) + 1u];
  }
  for (u32 v = 0; v < N; ++v) start[v + 1u] += start[v];
  for (u32 v = 0; v < N; ++v) cur[v] = start[v];
  for (u32 i = 0; i < m; ++i) {
    adj[cur[la[i] ^ 1u]++] = lb[i];
    adj[cur[lb[i] ^ 1u]++] = la[i];
  }

  for (u32 v = 0; v < N; ++v) {
    idx[v] = 0;
    comp[v] = kNone;
  }
  u32 counter = 0, ncomp = 0, sp = 0;
  for (u32 r = 0; r < N; ++r) {
    if (idx[r] != 0) continue;
    u32 cp = 0;
    idx[r] = low[r] = ++counter;
    stk[sp++] = r;
    call[cp++] = r;
    cur[r] = start[r];
    while (cp > 0) {
      const u32 u = call[cp - 1u];
      if (cur[u] < start[u + 1u]) {
        const u32 w = adj[cur[u]++];
        if (idx[w] == 0) {
          idx[w] = low[w] = ++counter;
          stk[sp++] = w;
          call[cp++] = w;
          cur[w] = start[w];
        } else if (comp[w] == kNone && idx[w] < low[u]) {
          low[u] = idx[w];
        }
      } else {
        if (low[u] == idx[u]) {
          for (;;) {
            const u32 x = stk[--sp];
            comp[x] = ncomp;
            if (x == u) break;
          }
          ++ncomp;
        }
        --cp;
        if (cp > 0) {
          const u32 p = call[cp - 1u];
          if (low[u] < low[p]) low[p] = low[u];
        }
      }
    }
  }

  for (u32 v = 0; v < n; ++v) {
    if (comp[2u * v] == comp[2u * v + 1u]) {
      *out_sat = 0;
      *out_conflict = v;
      return TwoSatResult::Ok;
    }
  }
  // Tarjan numbers the components in reverse topological order of the implication graph; a literal is true when its component comes later in that
  // order, i.e. has the smaller number.
  for (u32 v = 0; v < n; ++v) value[v] = (comp[2u * v] < comp[2u * v + 1u]) ? 1u : 0u;
  *out_sat = 1;
  return TwoSatResult::Ok;
}

}  // namespace lumora::twosat
