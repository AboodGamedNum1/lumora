// Convex hull by Andrew's monotone chain on points sorted by heapsort (worst case O(n log n), iterative, no recursion). Integer only: the
// cross product is computed in int64_t, which is exact within kCoordLimit. No allocation, no global state.
//
// Working memory: the scratch only (n words) holds the sorted point indices. The lower chain is a stack at the front of the scratch with
// popped points swapped behind it; the upper chain is a stack in already consumed scratch slots. out_idx is written once, at the end, and
// only in out_idx[0 .. *out_count): the words beyond the count are never touched, and nothing is written on a failure.
#include "hull2d.h"

namespace lumora::hull2d {

namespace {

using i32 = std::int32_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;

struct Pts {
  const i32* x;
  const i32* y;
};

// Order by x, then y, then index.
bool before(const Pts& p, u32 a, u32 b) {
  if (p.x[a] != p.x[b]) return p.x[a] < p.x[b];
  if (p.y[a] != p.y[b]) return p.y[a] < p.y[b];
  return a < b;
}

bool same_point(const Pts& p, u32 a, u32 b) { return p.x[a] == p.x[b] && p.y[a] == p.y[b]; }

// Cross product of (a - o) and (b - o): positive for a left turn o -> a -> b.
i64 cross(const Pts& p, u32 o, u32 a, u32 b) {
  const i64 ax = static_cast<i64>(p.x[a]) - p.x[o];
  const i64 ay = static_cast<i64>(p.y[a]) - p.y[o];
  const i64 bx = static_cast<i64>(p.x[b]) - p.x[o];
  const i64 by = static_cast<i64>(p.y[b]) - p.y[o];
  return ax * by - ay * bx;
}

void sift_down(const Pts& p, u32* h, u32 root, u32 size) {
  const u32 item = h[root];
  for (;;) {
    u32 child = 2u * root + 1u;
    if (child >= size) break;
    if (child + 1u < size && before(p, h[child], h[child + 1u])) ++child;
    if (!before(p, item, h[child])) break;
    h[root] = h[child];
    root = child;
  }
  h[root] = item;
}

void heap_sort(const Pts& p, u32* h, u32 n) {
  for (u32 i = n / 2u; i > 0; --i) sift_down(p, h, i - 1u, n);
  for (u32 end = n; end > 1; --end) {
    const u32 top = h[0];
    h[0] = h[end - 1u];
    h[end - 1u] = top;
    sift_down(p, h, 0, end - 1u);
  }
}

}  // namespace

HullResult hull_scratch_words(u32 n, u32* out_scratch_words) {
  if (out_scratch_words == nullptr) return HullResult::NullArgument;
  if (n > kMaxPoints) return HullResult::TooLarge;
  *out_scratch_words = n;
  return HullResult::Ok;
}

HullResult hull_compute(const i32* xs, const i32* ys, u32 n, u32* scratch, u32 scratch_words, u32* out_idx, u32 out_capacity, u32* out_count) {
  if (out_count == nullptr) return HullResult::NullArgument;
  if (n > 0 && (xs == nullptr || ys == nullptr || out_idx == nullptr || scratch == nullptr)) return HullResult::NullArgument;
  if (n > kMaxPoints) return HullResult::TooLarge;
  if (scratch_words < n) return HullResult::ScratchTooSmall;
  if (out_capacity < n) return HullResult::OutputTooSmall;
  for (u32 i = 0; i < n; ++i) {
    if (xs[i] < -kCoordLimit || xs[i] > kCoordLimit || ys[i] < -kCoordLimit || ys[i] > kCoordLimit) return HullResult::CoordinateRange;
  }
  if (n == 0) {
    *out_count = 0;
    return HullResult::Ok;
  }
  const Pts p = {xs, ys};
  for (u32 i = 0; i < n; ++i) scratch[i] = i;
  heap_sort(p, scratch, n);

  // Keep the first (smallest index) of each run of equal points.
  u32 m = 1;
  for (u32 i = 1; i < n; ++i) {
    if (!same_point(p, scratch[m - 1u], scratch[i])) scratch[m++] = scratch[i];
  }
  if (m == 1) {
    out_idx[0] = scratch[0];
    *out_count = 1;
    return HullResult::Ok;
  }

  // Lower chain, left to right, as a stack in scratch[0 .. k). Popped points stay in the slots just above the stack (they are swapped, not
  // lost), so after the pass scratch[k .. m) holds every point that is not on the lower chain, in no particular order.
  u32 k = 0;
  for (u32 i = 0; i < m; ++i) {
    const u32 q = scratch[i];
    while (k >= 2 && cross(p, scratch[k - 2u], scratch[k - 1u], q) <= 0) --k;
    scratch[i] = scratch[k];
    scratch[k] = q;
    ++k;
  }
  const u32 last = scratch[k - 1u];  // the largest point, always the top of the lower chain

  // Upper vertices come only from the points above the lower chain. Sort them again, then sweep right to left. The upper stack sits in
  // scratch[m - 1 - j] for entry j; entry 0 is above `last`, which is the virtual bottom. A pushed point never lands below the slot of the
  // point being processed, so unread points are never overwritten.
  heap_sort(p, scratch + k, m - k);
  u32 s = 0;
  for (u32 i = m; i > k; --i) {
    const u32 q = scratch[i - 1u];
    while (s >= 1) {
      const u32 below = (s >= 2) ? scratch[m - s + 1u] : last;
      if (cross(p, below, scratch[m - s], q) > 0) break;
      --s;
    }
    ++s;
    scratch[m - s] = q;
  }
  // Close the chain at the first point (not pushed).
  const u32 first_pt = scratch[0];
  while (s >= 1) {
    const u32 below = (s >= 2) ? scratch[m - s + 1u] : last;
    if (cross(p, below, scratch[m - s], first_pt) > 0) break;
    --s;
  }

  // All checks and work are done; only now the result is written: the lower chain, then the upper entries 0 .. s-1.
  u32 count = 0;
  for (u32 i = 0; i < k; ++i) out_idx[count++] = scratch[i];
  for (u32 j = 0; j < s; ++j) out_idx[count++] = scratch[m - 1u - j];
  *out_count = count;
  return HullResult::Ok;
}

}  // namespace lumora::hull2d
