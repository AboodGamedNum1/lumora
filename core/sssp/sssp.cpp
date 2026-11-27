// Single-source shortest paths (contract in sssp.h). Dijkstra with an indexed binary heap in scratch, O((n + m) log n), iterative, no recursion, no
// allocation, integer only. Weights are at least 1, so every shortest-path predecessor of a node has a strictly smaller distance and is settled
// before it: the path count and the canonical predecessor are final when a node is settled and are accumulated along the way.
#include "sssp.h"

namespace lumora::sssp {

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u32 kNone = 0xFFFFFFFFu;

// Binary min-heap of node ids ordered by dist[]; pos[v] is the heap slot of v, or kNone when v is not in the heap.
void sift_up(u32* heap, u32* pos, const u64* dist, u32 i) {
  const u32 v = heap[i];
  while (i > 0) {
    const u32 p = (i - 1u) / 2u;
    if (dist[heap[p]] <= dist[v]) break;
    heap[i] = heap[p];
    pos[heap[i]] = i;
    i = p;
  }
  heap[i] = v;
  pos[v] = i;
}

void sift_down(u32* heap, u32* pos, const u64* dist, u32 size, u32 i) {
  const u32 v = heap[i];
  for (;;) {
    u32 c = 2u * i + 1u;
    if (c >= size) break;
    if (c + 1u < size && dist[heap[c + 1u]] < dist[heap[c]]) ++c;
    if (dist[v] <= dist[heap[c]]) break;
    heap[i] = heap[c];
    pos[heap[i]] = i;
    i = c;
  }
  heap[i] = v;
  pos[v] = i;
}
}  // namespace

SsspResult sssp_scratch_words(u32 n, u32 m, u32* out_words) {
  if (out_words == nullptr) return SsspResult::NullArgument;
  if (n > kMaxNodes || m > kMaxEdges) return SsspResult::TooLarge;
  *out_words = 8u * n + 3u * m + 2u;
  return SsspResult::Ok;
}

SsspResult sssp_run(u32 n, u32 m, const u32* eu, const u32* ev, const u32* w, u32 source, u32* scratch, u32 scratch_words, u64* dist, u64* count,
                    u32* parent) {
  if (m > 0 && (eu == nullptr || ev == nullptr || w == nullptr)) return SsspResult::NullArgument;
  if (n > 0 && (scratch == nullptr || dist == nullptr || count == nullptr || parent == nullptr)) return SsspResult::NullArgument;
  if (n > kMaxNodes || m > kMaxEdges) return SsspResult::TooLarge;
  if (n > 0 && static_cast<u64>(scratch_words) < 8ull * n + 3ull * m + 2ull) return SsspResult::ScratchTooSmall;
  if (source >= n) return SsspResult::BadNode;
  for (u32 i = 0; i < m; ++i) {
    if (eu[i] >= n || ev[i] >= n) return SsspResult::BadNode;
  }
  for (u32 i = 0; i < m; ++i) {
    if (w[i] == 0) return SsspResult::BadWeight;
  }

  // Scratch layout, in words: start[n+1], adjv[m], adjw[m], heap[n], pos[n]. In total 3n + 2m + 1, inside 8n + 3m + 2. The heap array doubles as the
  // fill cursors while the adjacency is built.
  u32* start = scratch;
  u32* adjv = start + n + 1u;
  u32* adjw = adjv + m;
  u32* heap = adjw + m;
  u32* pos = heap + n;

  for (u32 v = 0; v <= n; ++v) start[v] = 0;
  for (u32 i = 0; i < m; ++i) ++start[eu[i] + 1u];
  for (u32 v = 0; v < n; ++v) start[v + 1u] += start[v];
  for (u32 v = 0; v < n; ++v) heap[v] = start[v];
  for (u32 i = 0; i < m; ++i) {
    const u32 slot = heap[eu[i]]++;
    adjv[slot] = ev[i];
    adjw[slot] = w[i];
  }

  for (u32 v = 0; v < n; ++v) {
    dist[v] = kInf;
    count[v] = 0;
    parent[v] = kNoNode;
    pos[v] = kNone;
  }
  dist[source] = 0;
  count[source] = 1;
  u32 size = 1;
  heap[0] = source;
  pos[source] = 0;
  while (size > 0) {
    const u32 u = heap[0];
    --size;
    pos[u] = kNone;
    if (size > 0) {
      heap[0] = heap[size];
      sift_down(heap, pos, dist, size, 0);
    }
    const u64 du = dist[u];
    const u64 cu = count[u];
    for (u32 a = start[u]; a < start[u + 1u]; ++a) {
      const u32 v = adjv[a];
      const u64 nd = du + adjw[a];
      if (nd < dist[v]) {
        dist[v] = nd;
        count[v] = cu;
        parent[v] = u;
        if (pos[v] == kNone) {
          heap[size] = v;
          pos[v] = size;
          ++size;
        }
        sift_up(heap, pos, dist, pos[v]);
      } else if (nd == dist[v]) {
        count[v] += cu;
        if (u < parent[v]) parent[v] = u;
      }
    }
  }
  return SsspResult::Ok;
}

}  // namespace lumora::sssp
