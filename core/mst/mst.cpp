// Minimum spanning forest by Kruskal's rule: edges sorted by (weight, index) with an iterative heapsort (worst case O(m log m)), components kept
// by a union-find with path halving and union by size (near-linear), early stop once nodes - 1 edges are taken. Integer only: no recursion, no
// allocation, no mutable statics, no 128-bit types, no compiler-specific constructs. See mst.h for the contract.
#include "mst.h"

namespace lumora::mst {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kRootBit = 0x80000000u;  // a union-find slot with this bit set is a root; the low bits hold the component size

// Edge a comes before edge b: lower weight first, then lower index.
bool before(const u32* w, u32 a, u32 b) {
  if (w[a] != w[b]) return w[a] < w[b];
  return a < b;
}

void sift_down(const u32* w, u32* h, u32 root, u32 size) {
  const u32 item = h[root];
  for (;;) {
    u32 child = 2u * root + 1u;
    if (child >= size) break;
    if (child + 1u < size && before(w, h[child], h[child + 1u])) ++child;
    if (!before(w, item, h[child])) break;
    h[root] = h[child];
    root = child;
  }
  h[root] = item;
}

// Sorts h[0 .. n) ascending by (weight, index).
void heap_sort(const u32* w, u32* h, u32 n) {
  for (u32 i = n / 2u; i > 0; --i) sift_down(w, h, i - 1u, n);
  for (u32 end = n; end > 1; --end) {
    const u32 top = h[0];
    h[0] = h[end - 1u];
    h[end - 1u] = top;
    sift_down(w, h, 0, end - 1u);
  }
}

// Root of the component of x, halving the path on the way.
u32 find_root(u32* p, u32 x) {
  for (;;) {
    const u32 px = p[x];
    if ((px & kRootBit) != 0) return x;
    const u32 ppx = p[px];
    if ((ppx & kRootBit) != 0) return px;
    p[x] = ppx;
    x = ppx;
  }
}

}  // namespace

MstResult mst_sizes(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_scratch_words, std::uint32_t* out_forest_words) {
  if (out_scratch_words == nullptr || out_forest_words == nullptr) return MstResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return MstResult::TooLarge;
  *out_scratch_words = nodes + edges;
  *out_forest_words = nodes > 0 ? nodes - 1u : 0u;
  return MstResult::Ok;
}

MstResult mst_forest(std::uint32_t nodes, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev, const std::uint32_t* ew,
                     std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_idx, std::uint32_t out_capacity,
                     std::uint32_t* out_count, std::uint64_t* out_weight) {
  if (out_count == nullptr || out_weight == nullptr) return MstResult::NullArgument;
  if (edges > 0 && (eu == nullptr || ev == nullptr || ew == nullptr)) return MstResult::NullArgument;
  if ((static_cast<u64>(nodes) + edges) > 0 && scratch == nullptr) return MstResult::NullArgument;
  if (nodes > 1 && out_idx == nullptr) return MstResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return MstResult::TooLarge;
  if (scratch_words < nodes + edges) return MstResult::ScratchTooSmall;
  const u32 target = nodes > 0 ? nodes - 1u : 0u;  // the forest never holds more edges than this
  if (out_capacity < target) return MstResult::OutputTooSmall;
  for (u32 i = 0; i < edges; ++i) {
    if (eu[i] >= nodes || ev[i] >= nodes) return MstResult::BadEdge;
  }

  // All validation is done; only now is anything written to the outputs.
  u32 count = 0;
  u64 total = 0;
  if (target > 0 && edges > 0) {
    u32* order = scratch;          // edge indices, sorted
    u32* comp = scratch + edges;   // union-find slots for the nodes
    for (u32 i = 0; i < edges; ++i) order[i] = i;
    heap_sort(ew, order, edges);
    for (u32 i = 0; i < nodes; ++i) comp[i] = kRootBit | 1u;
    for (u32 i = 0; i < edges && count < target; ++i) {
      const u32 e = order[i];
      u32 a = find_root(comp, eu[e]);
      u32 b = find_root(comp, ev[e]);
      if (a == b) continue;
      const u32 sa = comp[a] & ~kRootBit;
      const u32 sb = comp[b] & ~kRootBit;
      if (sa < sb) {
        const u32 t = a;
        a = b;
        b = t;
      }
      comp[b] = a;                        // the smaller component hangs under the larger
      comp[a] = kRootBit | (sa + sb);
      out_idx[count++] = e;
      total += ew[e];
    }
  }
  *out_count = count;
  *out_weight = total;
  return MstResult::Ok;
}

}  // namespace lumora::mst
