// Maximum flow by Dinic's method: a BFS level graph and a blocking flow found by an iterative depth-first search over an explicit arc stack.
// The number of steps depends on the graph only (nodes and edges), never on the capacity values. Integer only: no recursion, no allocation, no
// mutable statics, no 128-bit types, no compiler-specific constructs. See maxflow.h for the contract.
#include "maxflow.h"

namespace lumora::maxflow {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kNone = 0xFFFFFFFFu;

}  // namespace

FlowResult maxflow_scratch_words(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_scratch_words) {
  if (out_scratch_words == nullptr) return FlowResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return FlowResult::TooLarge;
  *out_scratch_words = 8u * edges + 6u * nodes;
  return FlowResult::Ok;
}

FlowResult maxflow_run(std::uint32_t nodes, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev, const std::uint32_t* ecap,
                       std::uint32_t src, std::uint32_t sink, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_flow,
                       std::uint8_t* out_side) {
  if (out_flow == nullptr) return FlowResult::NullArgument;
  if (edges > 0 && (eu == nullptr || ev == nullptr || ecap == nullptr)) return FlowResult::NullArgument;
  if ((static_cast<u64>(nodes) + edges) > 0 && scratch == nullptr) return FlowResult::NullArgument;
  if (nodes > 0 && out_side == nullptr) return FlowResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return FlowResult::TooLarge;
  if (static_cast<u64>(scratch_words) < 8ull * edges + 6ull * nodes) return FlowResult::ScratchTooSmall;
  if (src >= nodes || sink >= nodes) return FlowResult::BadNode;
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] >= nodes || ev[i] >= nodes) return FlowResult::BadNode;
  }
  if (src == sink) return FlowResult::SameTerminal;

  // All validation is done. Scratch layout (words): to[2 * edges], cap[2 * edges], adj[2 * edges], start[nodes + 1], level[nodes], iter[nodes],
  // queue[nodes], path[nodes]. That is 6 * edges + 5 * nodes + 1, within the 8 * edges + 6 * nodes the caller provides (nodes is at least 2).
  // Edge i becomes the arcs 2i (eu to ev, capacity ecap) and 2i + 1 (ev to eu, capacity 0, the residual of the first). Self-loops and edges of
  // capacity 0 carry nothing and get no arcs.
  const u32 arcs = 2u * edges;
  u32* to = scratch;
  u32* cap = to + arcs;
  u32* adj = cap + arcs;
  u32* start = adj + arcs;
  u32* level = start + nodes + 1u;
  u32* iter = level + nodes;
  u32* queue = iter + nodes;
  u32* path = queue + nodes;

  for (u32 v = 0; v <= nodes; ++v) start[v] = 0;
  for (u32 i = 0; i < edges; ++i) {
    to[2u * i] = ev[i];
    to[2u * i + 1u] = eu[i];
    const bool live = (eu[i] != ev[i]) && (ecap[i] > 0);
    cap[2u * i] = live ? ecap[i] : 0u;
    cap[2u * i + 1u] = 0u;
    if (live) {
      ++start[eu[i] + 1u];
      ++start[ev[i] + 1u];
    }
  }
  for (u32 v = 0; v < nodes; ++v) start[v + 1u] += start[v];
  for (u32 v = 0; v < nodes; ++v) iter[v] = start[v];  // used as the fill cursor here
  for (u32 i = 0; i < edges; ++i) {
    if (cap[2u * i] > 0) {
      adj[iter[eu[i]]++] = 2u * i;
      adj[iter[ev[i]]++] = 2u * i + 1u;
    }
  }

  u64 flow = 0;
  for (;;) {
    // BFS levels over arcs with residual capacity.
    for (u32 v = 0; v < nodes; ++v) level[v] = kNone;
    u32 head = 0, tail = 0;
    level[src] = 0;
    queue[tail++] = src;
    while (head < tail) {
      const u32 u = queue[head++];
      for (u32 k = start[u]; k < start[u + 1u]; ++k) {
        const u32 a = adj[k];
        if (cap[a] > 0 && level[to[a]] == kNone) {
          level[to[a]] = level[u] + 1u;
          queue[tail++] = to[a];
        }
      }
    }
    if (level[sink] == kNone) break;

    // Blocking flow: depth-first search with the current-arc pointers, the path kept as a stack of arcs.
    for (u32 v = 0; v < nodes; ++v) iter[v] = start[v];
    u32 depth = 0;
    u32 u = src;
    for (;;) {
      if (u == sink) {
        u32 bottleneck = cap[path[0]];
        for (u32 d = 1; d < depth; ++d) {
          if (cap[path[d]] < bottleneck) bottleneck = cap[path[d]];
        }
        u32 first_full = kNone;
        for (u32 d = 0; d < depth; ++d) {
          cap[path[d]] -= bottleneck;
          cap[path[d] ^ 1u] += bottleneck;
          if (cap[path[d]] == 0 && first_full == kNone) first_full = d;
        }
        flow += bottleneck;
        depth = first_full;  // back to the tail of the first saturated arc
        u = (depth == 0) ? src : to[path[depth - 1u]];
        continue;
      }
      bool advanced = false;
      while (iter[u] < start[u + 1u]) {
        const u32 a = adj[iter[u]];
        if (cap[a] > 0 && level[to[a]] == level[u] + 1u) {
          path[depth++] = a;
          u = to[a];
          advanced = true;
          break;
        }
        ++iter[u];
      }
      if (advanced) continue;
      if (u == src) break;
      level[u] = kNone;  // dead end: no more flow through this node in this phase
      --depth;
      u = to[path[depth] ^ 1u];
      ++iter[u];
    }
  }

  // Residual reachability from src: the source side of the minimum cut closest to src.
  for (u32 v = 0; v < nodes; ++v) level[v] = kNone;
  u32 head = 0, tail = 0;
  level[src] = 0;
  queue[tail++] = src;
  while (head < tail) {
    const u32 u = queue[head++];
    for (u32 k = start[u]; k < start[u + 1u]; ++k) {
      const u32 a = adj[k];
      if (cap[a] > 0 && level[to[a]] == kNone) {
        level[to[a]] = 0;
        queue[tail++] = to[a];
      }
    }
  }
  for (u32 v = 0; v < nodes; ++v) out_side[v] = (level[v] != kNone) ? 1u : 0u;
  *out_flow = flow;
  return FlowResult::Ok;
}

}  // namespace lumora::maxflow
