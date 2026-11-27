// Eulerian path or circuit of a directed multigraph (contract in euler.h). Degree check, then Hierholzer's algorithm with an explicit edge stack (no
// recursion): O(n + m) steps, integer only, no allocation. The trail is built in scratch and copied to the caller only when every edge was used.
#include "euler.h"

namespace lumora::euler {

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
}  // namespace

EulerResult euler_scratch_words(u32 n, u32 m, u32* out_words) {
  if (out_words == nullptr) return EulerResult::NullArgument;
  if (n > kMaxNodes || m > kMaxEdges) return EulerResult::TooLarge;
  *out_words = 6u * n + 3u * m + 8u;
  return EulerResult::Ok;
}

EulerResult euler_trail(u32 n, u32 m, const u32* eu, const u32* ev, u32* scratch, u32 scratch_words, u32* out_edges, u32* out_len, EulerKind* out_kind) {
  if (out_kind == nullptr || out_len == nullptr) return EulerResult::NullArgument;
  if (m > 0 && (eu == nullptr || ev == nullptr || out_edges == nullptr)) return EulerResult::NullArgument;
  if (n > 0 && scratch == nullptr) return EulerResult::NullArgument;
  if (n > kMaxNodes || m > kMaxEdges) return EulerResult::TooLarge;
  if (n > 0 && static_cast<u64>(scratch_words) < 6ull * n + 3ull * m + 8ull) return EulerResult::ScratchTooSmall;
  for (u32 i = 0; i < m; ++i) {
    if (eu[i] >= n || ev[i] >= n) return EulerResult::BadNode;
  }
  if (m == 0) {
    *out_kind = EulerKind::Circuit;
    *out_len = 0;
    return EulerResult::Ok;
  }

  // Scratch layout, in words: start[n+1], adj[m], cur[n], ind[n], stk[m], res[m]. In total 3n + 3m + 1, inside 6n + 3m + 8.
  u32* start = scratch;
  u32* adj = start + n + 1u;
  u32* cur = adj + m;
  u32* ind = cur + n;
  u32* stk = ind + n;
  u32* res = stk + m;

  for (u32 v = 0; v <= n; ++v) start[v] = 0;
  for (u32 v = 0; v < n; ++v) ind[v] = 0;
  for (u32 i = 0; i < m; ++i) {
    ++start[eu[i] + 1u];
    ++ind[ev[i]];
  }
  for (u32 v = 0; v < n; ++v) start[v + 1u] += start[v];

  // Degrees: out = start[v+1] - start[v], in = ind[v].
  u32 src = 0, nsrc = 0, nsnk = 0;
  bool ok = true;
  for (u32 v = 0; v < n; ++v) {
    const u32 out = start[v + 1u] - start[v];
    const u32 in = ind[v];
    if (out == in) continue;
    if (out == in + 1u) {
      ++nsrc;
      src = v;
    } else if (in == out + 1u) {
      ++nsnk;
    } else {
      ok = false;
    }
  }
  if (!ok || nsrc != nsnk || nsrc > 1u) {
    *out_kind = EulerKind::None;
    *out_len = 0;
    return EulerResult::Ok;
  }
  const bool is_path = (nsrc == 1u);
  const u32 s = is_path ? src : eu[0];

  for (u32 v = 0; v < n; ++v) cur[v] = start[v];
  for (u32 i = 0; i < m; ++i) adj[cur[eu[i]]++] = i;
  for (u32 v = 0; v < n; ++v) cur[v] = start[v];

  u32 sp = 0, pos = m;
  for (;;) {
    const u32 v = (sp > 0) ? ev[stk[sp - 1u]] : s;
    if (cur[v] < start[v + 1u]) {
      stk[sp++] = adj[cur[v]++];
    } else if (sp > 0) {
      res[--pos] = stk[--sp];
    } else {
      break;
    }
  }
  if (pos != 0) {
    *out_kind = EulerKind::None;
    *out_len = 0;
    return EulerResult::Ok;
  }
  for (u32 i = 0; i < m; ++i) out_edges[i] = res[i];
  *out_kind = is_path ? EulerKind::Path : EulerKind::Circuit;
  *out_len = m;
  return EulerResult::Ok;
}

}  // namespace lumora::euler
