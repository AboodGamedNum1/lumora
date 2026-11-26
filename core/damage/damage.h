// Damage set: up to 8 disjoint dirty rectangles for partial redraw, built on core/rect. Pure integer: no floating point, no libm, no
// 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

#include "rect.h"

namespace lumora::damage {

constexpr std::uint32_t kMaxRects = 8;

struct DamageSet {
  lumora::rect::Rect rects[kMaxRects];  // rects[0 .. count) are used; the rest are ignored and may hold anything
  std::uint32_t count = 0;              // 0 .. kMaxRects
};

// Invariant kept by every function: count <= kMaxRects, every used rectangle is non-empty and valid (core/rect), and no two used
// rectangles overlap (touching edges are allowed and are NOT merged). A set whose count is above kMaxRects is corrupt: every function
// then treats it as invalid (see below). Used rectangles are not re-checked.

// Sets count to 0. A null set is ignored.
void clear(DamageSet* set);

// Adds r, clipped to bounds. Steps, in order:
//  1. set null: return false, nothing changes. count above kMaxRects: return false, nothing changes.
//  2. cur = intersect(r, bounds) (core/rect: an invalid or empty r or bounds gives an empty result). If cur is empty: return true, the
//     set is unchanged (nothing to add).
//  3. Merge loop: scan used rectangles in index order; at the first one that overlaps cur (positive shared area, core/rect overlaps),
//     set cur = bounds_union(cur, that one), remove it (the later used rectangles keep their order and move down by one), and restart
//     the scan from index 0. Repeat until no used rectangle overlaps cur. Touching rectangles are not merged. The union cannot fail
//     because every input lies inside the valid bounds; if bounds_union reports false anyway, return false and leave the set unchanged.
//  4. If count < kMaxRects: append cur at index count, increment count, return true.
//  5. Otherwise (the set is full): replace the whole set by ONE rectangle, the bounding union of every used rectangle and cur, so
//     count becomes 1, and return true.
// The set is written only after every step has succeeded (a false return never changes it).
bool add(DamageSet* set, const lumora::rect::Rect& r, const lumora::rect::Rect& bounds);

// Number of used rectangles; 0 for a null set or a corrupt one (count above kMaxRects).
std::uint32_t size(const DamageSet* set);

// Copies the used rectangle at index i to *out. Returns false and leaves *out unchanged when set or out is null, the set is corrupt, or i
// is not below count.
bool get(const DamageSet* set, std::uint32_t i, lumora::rect::Rect* out);

// Sum of the areas of the used rectangles (core/rect area), as uint64; 0 for a null or corrupt set.
std::uint64_t total_area(const DamageSet* set);

// Smallest rectangle containing every used rectangle, as all zeros when the set is empty, null or corrupt. It cannot fail for a set built
// by add() with valid bounds; if bounds_union reports false while folding the used rectangles in index order, return all zeros.
lumora::rect::Rect bounding_box(const DamageSet* set);

// Decisions: add() never leaves two overlapping rectangles, so total_area is a true covered area only when the set was built with one
// consistent bounds; with different bounds per call it is still the sum of the stored disjoint rectangles. Merging is by overlap only:
// two rectangles that share just an edge stay separate. A header ambiguity found later is a question to the maintainer, not an
// interpretation.

}  // namespace lumora::damage
