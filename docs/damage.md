# Damage set

`core/damage` keeps up to 8 disjoint dirty rectangles for partial redraw, built on `core/rect`. The rules are in the comments of
`core/damage/damage.h`; this page adds the decisions, the bounds, the tests and the mutation results.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no floating point and no 128-bit
type. Evidence label: local unit tests only.

## The rule in short

`add(set, r, bounds)` clips `r` to `bounds` and then merges. Rectangles that share only an edge are never merged. Rectangles with a
positive shared area are, one at a time: the scan takes the first used rectangle (lowest index) that overlaps the current rectangle,
replaces the current rectangle by the bounding union of the two, removes the stored one (the rest move down and keep their order) and
starts again from index 0, until nothing overlaps. Then the current rectangle is appended. If the set is still full after the removals
(8 rectangles left), the whole set becomes ONE rectangle: the bounding union of every remaining rectangle and the current one.

## Decisions (confirmed by the maintainer)

1. A union that cannot fit (`bounds_union` returns false) makes `add` return false and leaves the set unchanged. `add` works on a local
   copy and writes the set once, at the end.
2. A fully covered add still reorders: the stored rectangle is removed and the union, equal to the stored one, is appended at the end.
3. Full is judged after the merge removals. An add that merges into a full set does not collapse it unless 8 rectangles are still left.
4. The collapse folds the remaining rectangles in index order and then the current rectangle, starting from the zero rectangle.
5. `total_area` is a plain `uint64` sum of the rectangle areas with no saturation. A set built by `add` is disjoint inside one
   bounds rectangle, so its area is at most 2^62. A hand-built set of huge overlapping rectangles wraps modulo 2^64.
6. Hand-built and corrupt sets: only the first `count` rectangles are read, stale ones beyond `count` are ignored, and stored rectangles
   are not re-checked. A count above 8 is corrupt: `size`, `total_area` and `bounding_box` give 0 or all zeros, `get` returns false and
   leaves `*out` alone, `add` returns false and changes nothing. `clear` repairs a corrupt set.
7. Check order in `add`: null set, corrupt set, intersect with the bounds, an empty result returns true with the set unchanged. A null or
   corrupt set therefore fails even for an empty rectangle.

Different calls may use different bounds, so a stored rectangle can lie outside the bounds of a later call. That is how a union can fail
even though every input is valid: a stored rectangle at x 0 up to INT32_MAX, and a new rectangle clipped to bounds that start at
-2000000000, overlap, and their union is wider than INT32_MAX.

## Bounds

`add` makes one scan per merge and at most 8 merges, so at most 8 x 8 overlap tests plus the shifts, and one fold of 9 rectangles for a
collapse. `total_area` and `bounding_box` are one pass over at most 8 rectangles. The only storage is a local copy of 8 rectangles.

## Tests

`tools/damage_ref.py` follows the rule text literally with Python integers and its own FNV. It prints the constants pasted into
`tests/damage/damage_tests.cpp`: 29 scenario programs and four seeded random corpora. A program is a list of operations (add with its
bounds, clear, write a rectangle or the count directly). After every operation the test records the size, total area, bounding box and
every `get` result, and a digest of all of them is compared, together with a digest of the final state, the count and the area.

Other tests: hand-computed adds, merges, clipping and order; touching rectangles that stay separate and a one-pixel overlap that merges;
the covered-add reorder; the merge restart and chain; a full set that merges without collapsing, a collapse, a full empty add and a full
covered add; invalid and empty adds; a failing merge union and a failing collapse, each compared byte for byte with a copy of the set;
exact fit at INT32_MAX width; null pointers for every function; corrupt counts 9, 100 and 0xFFFFFFFF for every function; `get` at every
index for every count; stale rectangles beyond the count; total area wrap on a hand-built set; and an invariant oracle on 4000 random
adds (non-empty, disjoint, count at most 8, every clipped pixel covered, an empty clip changes nothing).

## Mutation testing

Evidence label: local only. 84 real source mutants of `core/damage/damage.cpp` were built and run against the test file, each one a
single edit. They cover: the null and corrupt checks of every function and their order; the corrupt limit; the clip and the empty
return; the copy, scan, hit selection and shift loops (short, from 1, swap remove, no shift); the touching rule; merge failure handling;
the full test and its position; the append; every part of the collapse; the final write and the count; `size`, `get`, `total_area` and
`bounding_box` bounds, saturation and 32-bit sums.

82 of the 84 were killed. Two survive, and each is equivalent:

- Taking the last overlapping rectangle instead of the first (the scan does not stop at the first hit). Stored rectangles are disjoint, so
  the set of rectangles that end up merged is the closure under overlap, which does not depend on the order. Removal keeps the order of the
  rest, the final union is the same, and when the full union does not fit, every order reaches a failing step, because each earlier union
  is contained in the final one. The result and the false return are identical.
- `Rect merged = cur` instead of a default `Rect merged`. `bounds_union` always assigns `*out` when it returns true, and on false the
  function returns before reading `merged`, so the initial value is never read.
