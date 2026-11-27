// Disjoint-set union over a caller parent array. See dsu.h for the contract. Integer only: no allocation, no recursion, no global
// state. The root of a set is its smallest element.
#include "dsu.h"

namespace lumora::dsu {

namespace {

using u32 = std::uint32_t;

// Walks to the root of x, then points every node on the path at the root. Only non-root entries are written. Iterative.
u32 find_root(u32* parent, u32 x) {
  u32 root = x;
  while (parent[root] != root) root = parent[root];
  u32 cur = x;
  while (parent[cur] != root) {
    const u32 next = parent[cur];
    parent[cur] = root;
    cur = next;
  }
  return root;
}

}  // namespace

DsuResult dsu_init(u32* parent, u32 n) {
  if (parent == nullptr) return DsuResult::NullArgument;
  if (n > kMaxElements) return DsuResult::TooLarge;
  for (u32 i = 0; i < n; ++i) parent[i] = i;
  return DsuResult::Ok;
}

DsuResult dsu_find(u32* parent, u32 n, u32 x, u32* out_root) {
  if (parent == nullptr || out_root == nullptr) return DsuResult::NullArgument;
  if (n > kMaxElements) return DsuResult::TooLarge;
  if (x >= n) return DsuResult::OutOfRange;
  *out_root = find_root(parent, x);
  return DsuResult::Ok;
}

DsuResult dsu_unite(u32* parent, u32 n, u32 a, u32 b, bool* out_merged) {
  if (parent == nullptr || out_merged == nullptr) return DsuResult::NullArgument;
  if (n > kMaxElements) return DsuResult::TooLarge;
  if (a >= n || b >= n) return DsuResult::OutOfRange;
  const u32 ra = find_root(parent, a);
  const u32 rb = find_root(parent, b);
  if (ra == rb) {
    *out_merged = false;
    return DsuResult::Ok;
  }
  if (ra < rb) {
    parent[rb] = ra;
  } else {
    parent[ra] = rb;
  }
  *out_merged = true;
  return DsuResult::Ok;
}

DsuResult dsu_count_sets(const u32* parent, u32 n, u32* out_count) {
  if (parent == nullptr || out_count == nullptr) return DsuResult::NullArgument;
  if (n > kMaxElements) return DsuResult::TooLarge;
  u32 count = 0;
  for (u32 i = 0; i < n; ++i) {
    if (parent[i] == i) ++count;
  }
  *out_count = count;
  return DsuResult::Ok;
}

}  // namespace lumora::dsu
