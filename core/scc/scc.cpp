// Strongly connected components of an edge list. Iterative Tarjan with explicit stacks in the caller scratch, then a renumbering pass so that the
// components are numbered by their smallest member node. See scc.h for the contract.
#include "scc.h"

namespace lumora::scc {

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u32 kNone = 0xFFFFFFFFu;
}  // namespace

SccResult scc_scratch_words(u32 nodes, u32 edges, u32* out_words) {
  if (out_words == nullptr) return SccResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return SccResult::TooLarge;
  *out_words = 6u * nodes + edges + 1u;
  return SccResult::Ok;
}

SccResult scc_run(u32 nodes, u32 edges, const u32* eu, const u32* ev, u32* scratch, u32 scratch_words, u32* comp, u32* out_count) {
  if (out_count == nullptr) return SccResult::NullArgument;
  if (edges > 0 && (eu == nullptr || ev == nullptr)) return SccResult::NullArgument;
  if (nodes > 0 && (scratch == nullptr || comp == nullptr)) return SccResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return SccResult::TooLarge;
  if (nodes > 0 && static_cast<u64>(scratch_words) < 6ull * nodes + edges + 1ull) return SccResult::ScratchTooSmall;
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] >= nodes || ev[i] >= nodes) return SccResult::BadNode;
  }
  if (nodes == 0) {
    *out_count = 0;
    return SccResult::Ok;
  }

  // Scratch layout: start[N+1], adj[E], idx[N], low[N], stk[N], call[N], iter[N]  (5N + E + 1 words of the 6N + E + 1 declared).
  u32* start = scratch;
  u32* adj = start + nodes + 1u;
  u32* idx = adj + edges;
  u32* low = idx + nodes;
  u32* stk = low + nodes;
  u32* call = stk + nodes;
  u32* iter = call + nodes;

  for (u32 v = 0; v <= nodes; ++v) start[v] = 0;
  for (u32 i = 0; i < edges; ++i) ++start[eu[i] + 1u];
  for (u32 v = 0; v < nodes; ++v) start[v + 1u] += start[v];
  for (u32 v = 0; v < nodes; ++v) iter[v] = start[v];  // fill cursor
  for (u32 i = 0; i < edges; ++i) adj[iter[eu[i]]++] = ev[i];
  for (u32 v = 0; v < nodes; ++v) {
    iter[v] = start[v];
    idx[v] = 0;
    comp[v] = kNone;
  }

  u32 counter = 1;  // preorder numbers start at 1, 0 means unvisited
  u32 ncomp = 0;
  u32 stk_top = 0;
  for (u32 root = 0; root < nodes; ++root) {
    if (idx[root] != 0) continue;
    u32 depth = 0;
    idx[root] = low[root] = counter++;
    stk[stk_top++] = root;
    call[depth++] = root;
    while (depth > 0) {
      const u32 u = call[depth - 1u];
      if (iter[u] < start[u + 1u]) {
        const u32 w = adj[iter[u]++];
        if (idx[w] == 0) {
          idx[w] = low[w] = counter++;
          stk[stk_top++] = w;
          call[depth++] = w;
        } else if (comp[w] == kNone) {  // w is still on the Tarjan stack
          if (idx[w] < low[u]) low[u] = idx[w];
        }
      } else {
        if (low[u] == idx[u]) {
          for (;;) {
            const u32 w = stk[--stk_top];
            comp[w] = ncomp;
            if (w == u) break;
          }
          ++ncomp;
        }
        --depth;
        if (depth > 0) {
          const u32 p = call[depth - 1u];
          if (low[u] < low[p]) low[p] = low[u];
        }
      }
    }
  }

  // Renumber by first appearance in node order, which is the order of the smallest member node. idx is free now and serves as the map.
  for (u32 c = 0; c < ncomp; ++c) idx[c] = kNone;
  u32 next = 0;
  for (u32 v = 0; v < nodes; ++v) {
    const u32 t = comp[v];
    if (idx[t] == kNone) idx[t] = next++;
    comp[v] = idx[t];
  }
  *out_count = ncomp;
  return SccResult::Ok;
}

}  // namespace lumora::scc
