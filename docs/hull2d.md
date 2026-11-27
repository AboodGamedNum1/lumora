# hull2d: convex hull of integer points

Code: `core/hull2d/hull2d.cpp` (contract in `core/hull2d/hull2d.h`). Tests: `tests/hull2d/hull2d_tests.cpp`. Reference: `tools/hull2d_ref.py`.

## Rule

- `hull_compute(xs, ys, n, scratch, scratch_words, out_idx, out_capacity, out_count)` writes the indices of the hull vertices to `out_idx[0 .. *out_count)`, counter-clockwise, starting at the point with the smallest x (ties: the smallest y).
- Points on a hull edge that are not corners are excluded. Equal points count as one point and are reported by their smallest index.
- All points equal gives one vertex. All points on a line gives the two end points. n = 0 gives an empty hull.
- Coordinates must be within +/-1073741823 (`kCoordLimit`, 2^30 - 1). Differences then fit in 32 bits and cross products are exact in int64.
- `hull_scratch_words` returns n; at most `kMaxPoints` = 65536 points.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types. No aliasing claim is made.

## Output rule

Compute writes only scratch, `out_idx[0 .. *out_count)` and `*out_count`. On success the entries of `out_idx` at and beyond the returned count are unchanged. On failure nothing is written (including `*out_count` unless n = 0 succeeds). All validation finishes before the first output write, and `out_idx` is written once, at the very end. The capacity is checked against n, never against the hull size.

## Check order

1. NullArgument: `out_count` always; `xs`, `ys`, `out_idx`, `scratch` only while n is above 0.
2. TooLarge (n above `kMaxPoints`).
3. ScratchTooSmall (`scratch_words` below n).
4. OutputTooSmall (`out_capacity` below n).
5. CoordinateRange: every one of the n points is scanned (first, last and duplicates included), so it comes after all earlier errors.
6. n = 0: `*out_count = 0`, Ok.

## Method

Andrew's monotone chain on indices held in the scratch.

- Heapsort of the indices by (x, y, index). It is iterative, with a worst case of O(n log n), so all-equal or sorted input is not special.
- One in-place pass keeps the smallest index of each run of equal points.
- Lower chain: a stack in `scratch[0 .. k)`, popping while the cross product is not positive. Popped points are swapped to just above the stack, so `scratch[k .. m)` holds exactly the points not on the lower chain.
- Those points are heapsorted again and swept from the right. The upper stack lives in `scratch[m-1-j]` for entry j, with the last lower point as virtual bottom, and the sweep is closed against the first point. A pushed point never lands below the slot being read.
- Each sweep is linear, and the heapsorts dominate: O(n log n) worst case.

## Decisions

Confirmed by the maintainer through the coordinating agent: the check order above, capacity compared with n, the coordinate limit and its inclusive boundary, the heapsort for a true worst case, and the output rule (no write beyond the returned count, tails asserted with a sentinel-filled output of exactly n entries).

## Tests

Expected values come from `tools/hull2d_ref.py`, which does not share the method. Small sets use a Jarvis march (gift wrapping, farthest point on ties); the 65536-point sets use a Graham scan with a separate comparator, cross-checked against the Jarvis march on 20000 random small sets and on mid and large patterns, and two large patterns against closed forms. Every run uses a sentinel-filled output of exactly n entries and checks that the tail is still the sentinel. Success paths use buffers of the declared length under ASan; tiny buffers only for null, TooLarge and length-0 cases.

Exhaustive extents:
- every ordered sequence of 0 to 4 points on a 3x3 grid, compared entry by entry;
- 3x3 grid, length 1 to 5, and the grid {-L, 0, L}, length 1 to 5, and 4x4 grid, length 1 to 4, by digest;
- 80 mid-size sets.

Sampled (not exhaustive): seven 65536-point patterns (parabola, all equal, scrambled diagonal, scrambled square perimeter, random over the full square, dense grid with many duplicates, four repeated limit corners), compared by count, digest, first and last entry.

Also: a golden digest, hand-checked cases (clockwise input, edge point, tie on smallest x then y, duplicate smallest index, collinear, all equal, vertical line), one hand-written orientation test with explicit numbers, the coordinate limit (+/-L accepted; L+1 and -L-1 rejected for the first point, the last point and a duplicate), and argument rules and check order.

## Mutation

One run of all 113 source mutants against the final test file (9 test cases, 269 assertions), local, g++ -std=c++17 -O1, ASan+UBSan with `-fno-sanitize-recover=all`, 10 s harness timeout per mutant. Every count below is from that run.

- 93 killed by assertions (a Catch2 FAILED line).
- 15 crash-only (a sanitizer abort or fault, no failed assertion), listed individually: cross 32-bit product; sift break >=; sift sibling +1; words null; c null out_count; c null xs; c null ys; c null out; c null scratch; c null and; c null n>1; c n0 none; upper push; out count; out count none.
- 0 timeouts.
- 5 survivors.

The 32-bit variants, with their exact text (the mutated line replaces the original in `cross`):

- `cross 32-bit` (SURVIVED). Original: `const i64 ax = static_cast<i64>(p.x[a]) - p.x[o];`. Mutant: `const i64 ax = static_cast<i32>(p.x[a] - p.x[o]);`. The difference is computed in 32 bits and widened to int64 before any multiplication. Equivalent: coordinates are within +/-(2^30 - 1), so a difference is at most 2^31 - 2 in magnitude and fits int32 on every accepted input (inputs outside the limit return CoordinateRange before any cross product). The products and the result stay int64, so the value is identical.
- `cross 32-bit product` (killed, crash-only here). Original: `return ax * by - ay * bx;`. Mutant: `return static_cast<i32>(ax) * static_cast<i32>(by) - static_cast<i32>(ay) * static_cast<i32>(bx);`. Under UBSan the int multiplication overflows (for example -1073676287 * -65534) and aborts, so it is in the crash-only list. Built without instrumentation (and linked against the same sanitized Catch2 libraries) it is killed by 15 failed assertions. This is a 32-bit product, so it is not equivalent.
- Not one of the 113 (checked once separately, same test file): narrowing the result, `return static_cast<i32>(ax * by - ay * bx);`. It is killed by 15 failed assertions, both with sanitizer instrumentation and without. Not equivalent.

The other four survivors, all equivalent in result:

- `heap end >0` (`for (u32 end = n; end > 0; --end)`): the extra pass swaps element 0 with itself and sifts a heap of size 0, a no-op.
- `c init id` (scratch starts as n - 1 - i): the sort key includes the index, so the order is total and the sorted result does not depend on the start order.
- `c dedupe cmp` (compare with `scratch[i - 1]` instead of the last kept entry): equal points are adjacent, and `scratch[i - 1]` is either the last kept entry or an untouched original of the same run, so the decision is the same.
- `c m1 none` (drop the m = 1 fast path): for m = 1 the general path gives k = 1, no upper points, and writes the same single index. The fast path is redundant.

"c null n>1" (null check only for n above 1) is a non-equivalent mutant that survived an earlier run until an n = 1 null-pointer test was added; in this run it is crash-only.

## Not run

MSVC, Clang, Windows, Android, any timing assertion, any exhaustive case at 65536 points, a memory sanitizer. CI results are reported separately from this local evidence.
