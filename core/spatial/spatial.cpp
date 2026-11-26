// Lumora CPU spatial index (scope T). Integer only, no heap, no recursion, no mutable statics. See docs/spatial.md.
#include "spatial.h"

namespace lumora::spatial {

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kBuildStack = 32;  // pending ranges during build: at most depth + 1 <= 22
constexpr u32 kQueryStack = 64;  // pending nodes during queries; a valid tree never needs more than 22

bool box_invalid(const Box& b) { return b.x0 > b.x1 || b.y0 > b.y1; }

bool box_out_of_range(const Box& b) {
  return b.x0 < -kMaxCoord || b.x0 > kMaxCoord || b.y0 < -kMaxCoord || b.y0 > kMaxCoord || b.x1 < -kMaxCoord || b.x1 > kMaxCoord ||
         b.y1 < -kMaxCoord || b.y1 > kMaxCoord;
}

bool intersects(const Box& a, const Box& b) { return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1; }

// Sort key of a box on an axis: x0 + x1 (axis 0) or y0 + y1 (axis 1), exact in int64.
i64 key_of(const Box& b, int axis) { return axis == 0 ? static_cast<i64>(b.x0) + b.x1 : static_cast<i64>(b.y0) + b.y1; }

// Total order (key, item index): true when item a sorts strictly before item b.
bool less_item(const Box* items, int axis, u32 a, u32 b) {
  const i64 ka = key_of(items[a], axis), kb = key_of(items[b], axis);
  return ka < kb || (ka == kb && a < b);
}

// In-place heapsort of order[lo, hi) by (key, index). No recursion, no extra memory.
void sift_down_items(const Box* items, int axis, u32* a, u32 n, u32 root) {
  for (;;) {
    u32 child = 2 * root + 1;
    if (child >= n) return;
    if (child + 1 < n && less_item(items, axis, a[child], a[child + 1])) ++child;
    if (!less_item(items, axis, a[root], a[child])) return;
    const u32 t = a[root];
    a[root] = a[child];
    a[child] = t;
    root = child;
  }
}

void heapsort_items(const Box* items, int axis, u32* a, u32 n) {
  if (n < 2) return;
  for (u32 i = n / 2; i-- > 0;) sift_down_items(items, axis, a, n, i);
  for (u32 end = n - 1; end > 0; --end) {
    const u32 t = a[0];
    a[0] = a[end];
    a[end] = t;
    sift_down_items(items, axis, a, end, 0);
  }
}

// Bounded max-heap of the smallest `cap` indices seen, stored in the caller's out buffer.
struct IndexHeap {
  u32* a;
  u32 cap;
  u32 size;
  u32 found;

  void add(u32 id) {
    ++found;
    if (cap == 0) return;
    if (size < cap) {
      u32 i = size++;
      while (i > 0) {
        const u32 p = (i - 1) / 2;
        if (a[p] >= id) break;
        a[i] = a[p];
        i = p;
      }
      a[i] = id;
    } else if (id < a[0]) {
      sift_down(id);
    }
  }
  // Puts v at the root position and sifts it down.
  void sift_down(u32 v) {
    u32 i = 0;
    for (;;) {
      u32 c = 2 * i + 1;
      if (c >= size) break;
      if (c + 1 < size && a[c] < a[c + 1]) ++c;
      if (a[c] <= v) break;
      a[i] = a[c];
      i = c;
    }
    a[i] = v;
  }
  // Heapsort in place: ascending order.
  void finish() {
    for (u32 end = size; end > 1; --end) {
      const u32 top = a[0];
      const u32 last = a[end - 1];
      a[end - 1] = top;
      size = end - 1;
      sift_down(last);
    }
    size = 0;
  }
};

// O(1) guards shared by the queries. Returns Ok, NullBuffer or BadIndex.
Status guard_index(const Box* items, u32 n, const Node* nodes, u32 node_count, const u32* order) {
  if (n > kMaxItems) return Status::BadIndex;
  if (n == 0) return node_count == 0 ? Status::Ok : Status::BadIndex;
  if (items == nullptr || nodes == nullptr || order == nullptr) return Status::NullBuffer;
  if (node_count == 0 || node_count > 2 * n - 1) return Status::BadIndex;
  const Node& r = nodes[0];
  if (r.count > 0) {
    if (r.right != 0 || static_cast<u64>(r.first) + r.count > n || node_count != 1) return Status::BadIndex;
  } else if (node_count < 3 || r.first != 1 || r.right <= 1 || r.right >= node_count) {
    return Status::BadIndex;
  }
  return Status::Ok;
}

// Box query traversal. Status is Ok or BadIndex. Visit budgets keep a damaged index from looping or fanning out: a valid tree
// visits every node and every item position at most once.
Status run_box(const Box* items, u32 n, const Node* nodes, u32 node_count, const u32* order, const Box& q, IndexHeap& heap) {
  u32 stack[kQueryStack];
  u32 sp = 0;
  stack[sp++] = 0;
  u32 node_budget = node_count;
  u32 item_budget = n;
  while (sp > 0) {
    const u32 idx = stack[--sp];
    if (node_budget == 0) return Status::BadIndex;
    --node_budget;
    const Node& nd = nodes[idx];
    if (!intersects(nd.box, q)) continue;
    if (nd.count > 0) {
      if (nd.count > item_budget || static_cast<u64>(nd.first) + nd.count > n) return Status::BadIndex;
      item_budget -= nd.count;
      for (u32 k = 0; k < nd.count; ++k) {
        const u32 id = order[nd.first + k];
        if (id >= n) return Status::BadIndex;
        if (intersects(items[id], q)) heap.add(id);
      }
    } else {
      const u32 l = nd.first, r = nd.right;
      if (l <= idx || r <= idx || l >= node_count || r >= node_count || sp + 2 > kQueryStack) return Status::BadIndex;
      stack[sp++] = r;
      stack[sp++] = l;
    }
  }
  return Status::Ok;
}

Status query_common(const Box* items, u32 n, const Node* nodes, u32 node_count, const u32* order, const Box& q, u32* out,
                    u32 out_capacity, u32* found) {
  if (found == nullptr || (out == nullptr && out_capacity > 0)) return Status::NullBuffer;
  *found = 0;
  const Status g = guard_index(items, n, nodes, node_count, order);
  if (g != Status::Ok) return g;
  if (n == 0) return Status::Ok;
  IndexHeap heap{out, out_capacity, 0, 0};
  if (run_box(items, n, nodes, node_count, order, q, heap) != Status::Ok) {
    *found = 0;
    return Status::BadIndex;
  }
  *found = heap.found;
  heap.finish();
  return heap.found > out_capacity ? Status::BufferTooSmall : Status::Ok;
}

// Exact squared distance from a point to a box (0 inside or touching); int64 differences, at most about 2^63 in total.
u64 dist2_box(const Box& b, std::int32_t px, std::int32_t py) {
  i64 dx = 0, dy = 0;
  if (px < b.x0) dx = static_cast<i64>(b.x0) - px;
  else if (px > b.x1) dx = static_cast<i64>(px) - b.x1;
  if (py < b.y0) dy = static_cast<i64>(b.y0) - py;
  else if (py > b.y1) dy = static_cast<i64>(py) - b.y1;
  return static_cast<u64>(dx) * static_cast<u64>(dx) + static_cast<u64>(dy) * static_cast<u64>(dy);
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadCount: return "bad count";
    case Status::BadBox: return "bad box";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::BadIndex: return "bad index";
  }
  return "unknown";
}

// Level by level. At every depth the range sizes are floor(n / 2^d) and ceil(n / 2^d) only, so two (size, multiplicity) pairs per
// level are enough; a small merge keeps that exact without assuming it silently.
std::uint32_t node_bound(std::uint32_t item_count, std::uint32_t leaf_size) {
  if (item_count == 0 || item_count > kMaxItems || leaf_size == 0 || leaf_size > kMaxLeaf) return 0;
  u32 size[4] = {item_count, 0, 0, 0};
  u64 mult[4] = {1, 0, 0, 0};
  u64 total = 0;
  for (;;) {
    u32 nsize[4] = {0, 0, 0, 0};
    u64 nmult[4] = {0, 0, 0, 0};
    u32 used = 0;
    bool any_split = false;
    for (u32 i = 0; i < 4; ++i) {
      if (mult[i] == 0) continue;
      total += mult[i];  // every range is one node
      if (size[i] <= leaf_size) continue;
      any_split = true;
      const u32 half[2] = {size[i] / 2, size[i] - size[i] / 2};
      for (u32 h = 0; h < 2; ++h) {
        u32 j = 0;
        while (j < used && nsize[j] != half[h]) ++j;
        if (j == used) {
          if (used == 4) return 0;  // cannot happen: only two distinct sizes exist per level
          nsize[used] = half[h];
          nmult[used] = 0;
          ++used;
        }
        nmult[j] += mult[i];
      }
    }
    if (!any_split) break;
    for (u32 i = 0; i < 4; ++i) {
      size[i] = nsize[i];
      mult[i] = nmult[i];
    }
  }
  return static_cast<u32>(total);
}

Status build(const Box* items, std::uint32_t item_count, std::uint32_t leaf_size, Node* nodes, std::uint32_t node_capacity,
             std::uint32_t* order, std::uint32_t* node_count) {
  if (item_count > 0 && (items == nullptr || nodes == nullptr || order == nullptr)) return Status::NullBuffer;
  if (item_count > kMaxItems || leaf_size == 0 || leaf_size > kMaxLeaf) return Status::BadCount;
  for (u32 i = 0; i < item_count; ++i)
    if (box_invalid(items[i]) || box_out_of_range(items[i])) return Status::BadBox;
  const u32 need = node_bound(item_count, leaf_size);
  if (node_capacity < need) return Status::BufferTooSmall;

  if (node_count != nullptr) *node_count = need;
  if (item_count == 0) return Status::Ok;
  for (u32 i = 0; i < item_count; ++i) order[i] = i;

  struct Frame {
    u32 lo, hi, parent;
    bool right;
  };
  Frame stack[kBuildStack];
  u32 sp = 0;
  u32 next = 0;
  stack[sp++] = Frame{0, item_count, 0, false};
  while (sp > 0) {
    const Frame f = stack[--sp];
    const u32 idx = next++;
    if (idx > 0) {
      if (f.right) nodes[f.parent].right = idx;
      else nodes[f.parent].first = idx;
    }
    Node& nd = nodes[idx];
    Box bb = items[order[f.lo]];
    for (u32 k = f.lo + 1; k < f.hi; ++k) {
      const Box& b = items[order[k]];
      if (b.x0 < bb.x0) bb.x0 = b.x0;
      if (b.y0 < bb.y0) bb.y0 = b.y0;
      if (b.x1 > bb.x1) bb.x1 = b.x1;
      if (b.y1 > bb.y1) bb.y1 = b.y1;
    }
    nd.box = bb;
    const u32 cnt = f.hi - f.lo;
    if (cnt <= leaf_size) {
      nd.first = f.lo;
      nd.count = cnt;
      nd.right = 0;
      continue;
    }
    nd.first = 0;
    nd.count = 0;
    nd.right = 0;
    const i64 ex = static_cast<i64>(bb.x1) - bb.x0, ey = static_cast<i64>(bb.y1) - bb.y0;
    const int axis = ey > ex ? 1 : 0;
    heapsort_items(items, axis, order + f.lo, cnt);
    const u32 mid = f.lo + cnt / 2;
    stack[sp++] = Frame{mid, f.hi, idx, true};
    stack[sp++] = Frame{f.lo, mid, idx, false};
  }
  return Status::Ok;
}

Status query_box(const Box* items, std::uint32_t item_count, const Node* nodes, std::uint32_t node_count, const std::uint32_t* order,
                 const Box& q, std::uint32_t* out, std::uint32_t out_capacity, std::uint32_t* found) {
  if (found == nullptr || (out == nullptr && out_capacity > 0)) return Status::NullBuffer;
  *found = 0;
  if (item_count > 0 && (items == nullptr || nodes == nullptr || order == nullptr)) return Status::NullBuffer;
  if (box_invalid(q)) return Status::BadBox;
  return query_common(items, item_count, nodes, node_count, order, q, out, out_capacity, found);
}

Status query_point(const Box* items, std::uint32_t item_count, const Node* nodes, std::uint32_t node_count, const std::uint32_t* order,
                   std::int32_t px, std::int32_t py, std::uint32_t* out, std::uint32_t out_capacity, std::uint32_t* found) {
  const Box q{px, py, px, py};
  return query_common(items, item_count, nodes, node_count, order, q, out, out_capacity, found);
}

Status nearest(const Box* items, std::uint32_t item_count, const Node* nodes, std::uint32_t node_count, const std::uint32_t* order,
               std::int32_t px, std::int32_t py, std::uint32_t* index, std::uint64_t* dist2, bool* found) {
  if (index == nullptr || dist2 == nullptr || found == nullptr) return Status::NullBuffer;
  const Status g = guard_index(items, item_count, nodes, node_count, order);
  if (g != Status::Ok) return g;  // nothing is written on error
  if (item_count == 0) {
    *index = 0;
    *dist2 = 0;
    *found = false;
    return Status::Ok;
  }

  struct Entry {
    u32 idx;
    u64 d;
  };
  Entry stack[kQueryStack];
  u32 sp = 0;
  stack[sp++] = Entry{0, 0};
  bool have = false;
  u32 best_id = 0;
  u64 best_d = 0;
  u32 node_budget = node_count;
  u32 item_budget = item_count;
  while (sp > 0) {
    const Entry e = stack[--sp];
    if (have && e.d > best_d) continue;  // equal distance may still hide a lower index
    if (node_budget == 0) return Status::BadIndex;
    --node_budget;
    const Node& nd = nodes[e.idx];
    if (nd.count > 0) {
      if (nd.count > item_budget || static_cast<u64>(nd.first) + nd.count > item_count) return Status::BadIndex;
      item_budget -= nd.count;
      for (u32 k = 0; k < nd.count; ++k) {
        const u32 id = order[nd.first + k];
        if (id >= item_count) return Status::BadIndex;
        const u64 d = dist2_box(items[id], px, py);
        if (!have || d < best_d || (d == best_d && id < best_id)) {
          have = true;
          best_d = d;
          best_id = id;
        }
      }
    } else {
      const u32 l = nd.first, r = nd.right;
      if (l <= e.idx || r <= e.idx || l >= node_count || r >= node_count || sp + 2 > kQueryStack) return Status::BadIndex;
      const u64 dl = dist2_box(nodes[l].box, px, py), dr = dist2_box(nodes[r].box, px, py);
      if (dr < dl) {  // right is nearer: visit it first (pushed last)
        stack[sp++] = Entry{l, dl};
        stack[sp++] = Entry{r, dr};
      } else {
        stack[sp++] = Entry{r, dr};
        stack[sp++] = Entry{l, dl};
      }
    }
  }
  *index = best_id;
  *dist2 = best_d;
  *found = have;
  return Status::Ok;
}

std::uint64_t digest_indices(const std::uint32_t* values, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (values == nullptr) return h;
  for (std::size_t i = 0; i < count; ++i) {
    u32 v = values[i];
    for (int b = 0; b < 4; ++b) {
      h ^= static_cast<u64>(v & 0xFFu);
      h *= 0x100000001b3ull;
      v >>= 8;
    }
  }
  return h;
}

}  // namespace lumora::spatial
