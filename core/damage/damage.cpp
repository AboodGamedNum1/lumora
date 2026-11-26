// Lumora damage set (scope AH). Integer only, no heap, no recursion, no mutable statics. See docs/damage.md.
#include "damage.h"

namespace lumora::damage {

using lumora::rect::Rect;

namespace {

using u32 = std::uint32_t;

bool corrupt(const DamageSet* set) { return set->count > kMaxRects; }

}  // namespace

void clear(DamageSet* set) {
  if (set == nullptr) return;
  set->count = 0;
}

bool add(DamageSet* set, const Rect& r, const Rect& bounds) {
  if (set == nullptr) return false;
  if (corrupt(set)) return false;
  Rect cur = lumora::rect::intersect(r, bounds);
  if (lumora::rect::is_empty(cur)) return true;

  // Work on a copy so that a failure leaves the set untouched.
  Rect work[kMaxRects];
  u32 n = set->count;
  for (u32 i = 0; i < n; ++i) work[i] = set->rects[i];

  for (;;) {
    u32 hit = n;
    for (u32 i = 0; i < n; ++i) {
      if (lumora::rect::overlaps(cur, work[i])) {
        hit = i;
        break;
      }
    }
    if (hit == n) break;
    Rect merged;
    if (!lumora::rect::bounds_union(cur, work[hit], &merged)) return false;
    cur = merged;
    for (u32 i = hit; i + 1 < n; ++i) work[i] = work[i + 1];
    --n;
  }

  if (n < kMaxRects) {
    work[n] = cur;
    ++n;
  } else {
    Rect box;
    bool ok = true;
    for (u32 i = 0; i < n && ok; ++i) ok = lumora::rect::bounds_union(box, work[i], &box);
    if (ok) ok = lumora::rect::bounds_union(box, cur, &box);
    if (!ok) return false;
    work[0] = box;
    n = 1;
  }

  for (u32 i = 0; i < n; ++i) set->rects[i] = work[i];
  set->count = n;
  return true;
}

std::uint32_t size(const DamageSet* set) {
  if (set == nullptr || corrupt(set)) return 0;
  return set->count;
}

bool get(const DamageSet* set, std::uint32_t i, Rect* out) {
  if (set == nullptr || out == nullptr || corrupt(set) || i >= set->count) return false;
  *out = set->rects[i];
  return true;
}

std::uint64_t total_area(const DamageSet* set) {
  if (set == nullptr || corrupt(set)) return 0;
  std::uint64_t sum = 0;
  for (u32 i = 0; i < set->count; ++i) sum += lumora::rect::area(set->rects[i]);
  return sum;
}

Rect bounding_box(const DamageSet* set) {
  if (set == nullptr || corrupt(set)) return Rect{};
  Rect box;
  for (u32 i = 0; i < set->count; ++i) {
    if (!lumora::rect::bounds_union(box, set->rects[i], &box)) return Rect{};
  }
  return box;
}

}  // namespace lumora::damage
