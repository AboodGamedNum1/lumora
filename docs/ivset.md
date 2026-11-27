# ivset: set of disjoint half-open u32 intervals

Code: `core/ivset/ivset.cpp` (contract in `core/ivset/ivset.h`). Tests: `tests/ivset/ivset_tests.cpp`. Reference: `tools/ivset_ref.py`.

## Rule

The set is stored in two caller arrays, `starts` and `ends`, with a count. Interval i is [starts[i], ends[i]). The form is canonical and exact: every interval is non-empty, intervals are sorted ascending, and consecutive intervals are separated by a gap (`ends[i] < starts[i+1]`), so touching intervals such as [1, 3) and [3, 5) are always merged into [1, 5). Members run from 0 to 0xFFFFFFFE. The value 0xFFFFFFFF is a valid end and never a member. Integer only: no allocation, no recursion, no global state.

## Method

`insert`: find the first interval whose end reaches lo and the first interval that starts after hi. Everything between them overlaps or touches [lo, hi) and is replaced by one interval from the smaller start to the larger end. The tail is shifted by the difference. `remove`: find the first interval that has a member at or above lo and the first that starts at or above hi. The affected intervals are replaced by at most two pieces, the left remainder [start, lo) and the right remainder [hi, end), and only non-empty pieces are kept. `contains`: binary search for the last interval with start at or below x, then compare x with its end. No bound is ever incremented, so an end of 0xFFFFFFFF cannot overflow.

## Check order

For every function: a null pointer (starts, ends, count, and the out pointer of `contains`) gives NullArgument, even with capacity 0. Then capacity above kMaxCapacity (2^24) gives TooLarge, before `*count` is read. Then `*count` above capacity gives BadCount. Then, for insert and remove, lo above hi gives InvalidRange. Then the operation. `contains` has no range check.

`lo == hi` is Ok and writes nothing, even when the set is full or the capacity is 0. The new count is computed before anything is written. Full is returned only when the new count would exceed the capacity, and then nothing is written. So an insert that merges into a full set succeeds, an insert that adds a new interval to a full set is Full, a remove that splits an interval in a full set is Full, and a remove that trims, deletes or removes nothing in a full set is Ok. On any failure the arrays, `*count` and the out value are untouched. On success only indexes below max(old count, new count) are written. Entries between a smaller new count and the old count are not specified and not tested.

## Decisions

Nine readings were confirmed by the maintainer through the coordinating agent, with no corrections, including the exact-limit tests on one-element arrays. No aliasing claim: the header says the arrays and `*count` must not overlap, and overlap is not tested.

## Tests

Expected values come from `tools/ivset_ref.py`, an independent model: a plain Python set of integers. Insert adds lo .. hi-1, remove discards them, and the canonical intervals are rebuilt from the sorted members after every operation. A result is Full when the rebuilt list would be longer than the capacity. It emits 36 scenarios of 220 operations: 3 bases (0, 1000 and 0xFFFFFFFF - 63, so ranges end at 0xFFFFFFFF) times 4 capacities (1, 2, 4, 64) times 3 maximum range lengths (3, 12, 63). After every operation the test compares the result code, the count, the full active prefix of both arrays, that entries at or above max(old count, new count) keep their sentinel values, that a Full result changed nothing, and `contains` for all 64 positions against a bit mask. Arrays have the exact capacity, so ASan sees any overrun. Capacity 0 uses a one-element allocation so the pointers are not null.

11 test cases, 12364264 assertions:
- the 36 scenarios against the reference
- a golden digest (FNV-1a, 64 bit) of a larger run: 3000 operations over 2000 positions, capacity 25, base 70000, 916 Full results, hashed after every operation
- exhaustive: positions 0..5, capacity 3, all 56 operations (28 ranges, insert and remove), every sequence of 3, against a bit-mask oracle
- hand cases: merging, touching on one side, bridging, swallowing, splitting, trimming, deleting, remove that touches but removes nothing
- Full behaviour, including a merge into a full set, a trim in a full set and capacity 0
- `lo == hi` in a full set
- the top of the u32 range (end 0xFFFFFFFF, whole range, 0xFFFFFFFF never a member, an InvalidRange with lo = 0xFFFFFFFF)
- argument rules and check order, including nothing written on failure and the out value untouched
- `contains` on empty sets, edges and gaps
- exact capacity limit on tiny arrays: capacity = kMaxCapacity with count 0 and one-element arrays, insert, remove and contains succeed; kMaxCapacity + 1 is TooLarge; count = kMaxCapacity + 1 is BadCount
- random operations against a set of positions, with the canonical form checked after each step

Local run (g++, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI.

## Not run

- A set that actually holds kMaxCapacity (2^24) intervals: it needs two arrays of 64 MiB. Only the count-0 and rejection sides of that limit are run.
- Arrays not in canonical form, or made by other means: unspecified by the header, not tested.
- Overlapping arrays: excluded by the header, not tested.
- MSVC, Clang, Windows and Android: no result seen. The test file has no compiler-specific constructs.

## Mutation

79 source mutants of `core/ivset/ivset.cpp`, each built with ASan+UBSan and `-fno-sanitize-recover=all`, run against the full test file. 74 killed, 5 survived. Local result.

Of the 74 kills, 8 come from a crash, not from a failed assertion, and are listed individually: `ck null starts`, `ck null ends`, `ck null count`, `ck null and` (a null pointer is dereferenced instead of returning NullArgument), `ct null out` (the same for the out pointer of `contains`), `in full none` (insert without the Full check writes past an exact-size array), `rm full none` (the same for remove), `rm pieces` (remove with the wrong piece count writes out of bounds). The other 66 are killed by a failed assertion.

Survivors, all equivalent (5):
- `shift_tail` with `to >= from` instead of `to > from`: when to equals from the backward loop copies every entry onto itself, so nothing changes.
- `shift_tail` with `to <= from` instead of `to < from`: the same for the forward loop.
- remove with `ends[i0] < lo` instead of `<=`: an interval that ends exactly at lo is then counted as affected. It keeps a left piece equal to itself, so the pieces, the new count and every written value are the same as before (when it is the only affected interval the early return is replaced by a rewrite of identical values; the new count equals the old one, so Full cannot occur).
- remove with `starts[i1] <= hi` instead of `<`: an interval that starts exactly at hi is then counted as affected and kept as a right piece [hi, end), equal to itself. Same pieces, count and values.
- `contains` with `mid = (lo + hi) / 2` instead of `lo + (hi - lo) / 2`: the two are equal as integers, and lo and hi are at most 2^24, so there is no overflow.

No survivor is non-equivalent.
