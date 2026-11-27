// Bridges and articulation points of an undirected edge list. Iterative depth-first search with low-points in caller scratch; the parent EDGE
// (not the parent node) is skipped, so parallel edges are never bridges. See bridges.h for the contract.
#include "bridges.h"

namespace lumora::bridges {

namespace {
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u32 kNone = 0xFFFFFFFFu;
}  // namespace

BridgeResult bridges_scratch_words(u32 nodes, u32 edges, u32* out_words) {
  if (out_words == nullptr) return BridgeResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return BridgeResult::TooLarge;
  *out_words = 8u * nodes + 2u * edges + 1u;
  return BridgeResult::Ok;
}

BridgeResult bridges_run(u32 nodes, u32 edges, const u32* eu, const u32* ev, u32* scratch, u32 scratch_words, u8* is_bridge, u8* is_cut,
                         u32* out_bridges, u32* out_cuts) {
  if (out_bridges == nullptr || out_cuts == nullptr) return BridgeResult::NullArgument;
  if (edges > 0 && (eu == nullptr || ev == nullptr || is_bridge == nullptr)) return BridgeResult::NullArgument;
  if (nodes > 0 && (scratch == nullptr || is_cut == nullptr)) return BridgeResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return BridgeResult::TooLarge;
  if (nodes > 0 && static_cast<u64>(scratch_words) < 8ull * nodes + 2ull * edges + 1ull) return BridgeResult::ScratchTooSmall;
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] >= nodes || ev[i] >= nodes) return BridgeResult::BadNode;
  }
  if (nodes == 0) {
    *out_bridges = 0;
    *out_cuts = 0;
    return BridgeResult::Ok;
  }

  // Scratch layout: start[N+1], adj[2E] (arc ids), idx[N], low[N], iter[N], call[N], par[N] (parent arc), kids[N]: 7N + 2E + 1 words.
  // Arc a = 2i is eu[i] -> ev[i], arc 2i + 1 is ev[i] -> eu[i]. Loops get no arcs.
  u32* start = scratch;
  u32* adj = start + nodes + 1u;
  u32* idx = adj + 2u * edges;
  u32* low = idx + nodes;
  u32* iter = low + nodes;
  u32* call = iter + nodes;
  u32* par = call + nodes;
  u32* kids = par + nodes;

  for (u32 v = 0; v <= nodes; ++v) start[v] = 0;
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] != ev[i]) {
      ++start[eu[i] + 1u];
      ++start[ev[i] + 1u];
    }
  }
  for (u32 v = 0; v < nodes; ++v) start[v + 1u] += start[v];
  for (u32 v = 0; v < nodes; ++v) iter[v] = start[v];  // fill cursor
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] != ev[i]) {
      adj[iter[eu[i]]++] = 2u * i;
      adj[iter[ev[i]]++] = 2u * i + 1u;
    }
  }
  for (u32 v = 0; v < nodes; ++v) {
    iter[v] = start[v];
    idx[v] = 0;
    is_cut[v] = 0;
    kids[v] = 0;
  }
  for (u32 i = 0; i < edges; ++i) is_bridge[i] = 0;

  u32 counter = 1;  // preorder numbers start at 1, 0 means unvisited
  u32 nb = 0, nc = 0;
  for (u32 root = 0; root < nodes; ++root) {
    if (idx[root] != 0) continue;
    u32 depth = 0;
    idx[root] = low[root] = counter++;
    par[root] = kNone;
    call[depth++] = root;
    while (depth > 0) {
      const u32 u = call[depth - 1u];
      if (iter[u] < start[u + 1u]) {
        const u32 a = adj[iter[u]++];
        if (par[u] != kNone && (a >> 1) == (par[u] >> 1)) continue;  // the edge we came in by
        const u32 w = ((a & 1u) == 0u) ? ev[a >> 1] : eu[a >> 1];
        if (idx[w] == 0) {
          idx[w] = low[w] = counter++;
          par[w] = a;
          ++kids[u];
          call[depth++] = w;
        } else if (idx[w] < low[u]) {
          low[u] = idx[w];
        }
      } else {
        --depth;
        if (depth > 0) {
          const u32 p = call[depth - 1u];
          if (low[u] < low[p]) low[p] = low[u];
          if (low[u] > idx[p]) {
            is_bridge[par[u] >> 1] = 1;
            ++nb;
          }
          if (p != root && low[u] >= idx[p] && is_cut[p] == 0) {
            is_cut[p] = 1;
            ++nc;
          }
        }
      }
    }
    if (kids[root] >= 2u) {
      is_cut[root] = 1;
      ++nc;
    }
  }
  *out_bridges = nb;
  *out_cuts = nc;
  return BridgeResult::Ok;
}

}  // namespace lumora::bridges
