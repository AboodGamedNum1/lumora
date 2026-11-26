# Rectangle clipping module

The clipping module (`core/clip/clip.h`, `core/clip/clip.cpp`) clips segments, polylines and polygons against a closed
axis-aligned rectangle for the 2D renderer scissor and viewport. It uses integer arithmetic only (64-bit), no floating point, no
libm, no heap allocation, no recursion, no static mutable state, no negative signed shifts and no pointer value in any decision
except the overlap check. Results are identical on Linux, Windows and Android. Evidence label: software (CPU) only.

## Limits and arithmetic bounds

- Every point coordinate and every rectangle edge has magnitude at most `kMaxCoord = 2^28`. A rectangle needs `x0 <= x1` and
  `y0 <= y1` (a point or a line is a valid rectangle). Polygons and polylines have 3..4096 and 2..4096 points.
- Coordinate differences are at most `2^29` in magnitude. The rounded coordinates below are of points on the input segment, so
  they stay within `2^28`.
- Rounding rule: a computed coordinate is `a + d * t / m` with `m > 0`, rounded to the nearest integer with ties toward positive
  infinity, as `a + floor((2 * d * t + m) / (2 * m))`. The floor division is written for a positive divisor as truncating
  division followed by a decrement when the remainder is negative, never truncation of a negative dividend. With `|d| <= 2^29`,
  `|t| <= 2^29` and `m <= 2^29`: `|2 * d * t| <= 2^59`, so `|2 * d * t + m| <= 2^59 + 2^29 < 2^63`, and `2 * m <= 2^30`. The
  quotient and remainder are exact in `int64`.
- The rule is a rounding of the exact rational value `a + d * t / m`, and adding the integer `a` commutes with `floor(v + 1/2)`.
  So the result does not depend on which end of a segment is used as the base `a` (the other end gives `a' = a + d` and
  `t' = t - m`, the same rational). It also does not depend on which equivalent fraction is used.
- Clamping into the rectangle is applied after rounding but is a no-op: the exact point is inside a rectangle with integer edges,
  and `floor(v + 1/2)` of a value `v` in `[lo, hi]` stays in `[lo, hi]` for integer `lo` and `hi`. The code keeps the clamp as the
  header requires.
- Liang-Barsky parameters are fractions `n / m` with `m > 0`, `0 <= n <= m <= 2^29` after the visible interval is formed, and
  during the pass `|n|, m <= 2^29`. They are compared by cross multiplication, with products at most `2^58`.

## clip_segment

The segment `p0 + t * (p1 - p0)`, `t` in `[0, 1]`, is clipped against the four edges with `p = -dx, dx, -dy, dy` and
`q = x0' - x0, x1 - x0', y0' - y0, y1 - y0'` (`x0'`, `y0'` the start point). A zero `p` with a negative `q` makes the segment
invisible. A negative `p` is an entering edge (`t = (-q) / (-p)`, raising the lower parameter); a positive `p` is a leaving edge
(`t = q / p`, lowering the upper parameter). The segment is visible exactly when the lower parameter is not above the upper one,
so a segment touching the rectangle in one point (a corner, or an end point on an edge) is visible with equal parameters and
`a == b`. A degenerate segment (`p0 == p1`) is visible exactly when the point is in the rectangle. Both end points of the visible
part are computed from the original segment and rounded by the rule above, each coordinate independently. The coordinate on the
clipped edge comes out exactly because `d * (c - a) / d = c - a`. The end points use the original input, never a rounded
intermediate point. The outputs are written after the whole result is computed, so `a` and `b` may alias `p0` and `p1`.
Nothing is written when the segment is not visible, and nothing on an error.

Checks in order: `NullBuffer` (`a`, `b`, `visible`), `BadRect`, `BadPoint` (`p0`, then `p1`).

## clip_polygon

Four Sutherland-Hodgman stages in the fixed order left (`x >= x0`), bottom (`y >= y0`), right (`x <= x1`), top (`y <= y1`).
A stage walks the ring from the edge (last point, first point) onward: both inside outputs the current point, leaving outputs the
intersection, entering outputs the intersection and then the current point, both outside outputs nothing. Inside is closed. The
intersection of the edge `p -> q` with the line `c` has `m = |q_axis - p_axis|`, `t = |c - p_axis|` with matching sign handling,
and `d = q_other - p_other`, then the other coordinate is rounded by the rule above. The edge strictly crosses the line, so
`m > 0`, and `|t| <= m <= 2^29`. Points are kept as they come: repeated and collinear points stay, fewer than three output points
is a valid result, and an empty result is `Ok` with `*out_count = 0`.

Output bound and why the old bound is false. One stage turns `m` points into at most `g(m) = m + floor(m / 2)` points.
Proof: let `I` be the number of inside points and `E` the number of entering edges (outside to inside). Around a ring the
entering and leaving edges alternate, so the number of leaving edges is also `E`. Each entering edge starts at a different
outside point and ends at a different inside point, so `E <= floor(m / 2)` and `I <= m - E`. The stage writes `I` points (one for
each edge whose end is inside), `E` extra intersections for the entering edges and `E` intersections for the leaving edges:
`I + 2 * E <= m + E <= m + floor(m / 2)`. The bound is reached by a ring that alternates inside and outside points.
The four stages therefore give at most `polygon_bound(n) = g(g(g(g(n))))` points. A bound of `2 * n` (an earlier version of the
header) is false: the triangle `(11,13) (-6,6) (10,-2)` against the rectangle `(0,0) (10,10)` gives `3, 4, 5, 6, 7` points over
the four stages, `(13,-2) (7,15) (15,-4) (-5,10)` gives `4, 5, 7, 9, 10`. A worst-case search by an independent script (the table
below) finds outputs above `2 * n` for every `n` from 3 to 8. For `n = 4096` the bound is 20736 points. The first stage can
use its full `g(4096) = 6144` (a zigzag in the tests does).

Scratch layout. `scratch_size(n) = g(g(g(n))) + g(g(n))` points (23040 for 4096). Stage 1 writes the first region (room for
`g(g(g(n)))` points), stage 2 the second region (`g(g(n))`), stage 3 the first region again and stage 4 writes straight to `out`.
Each stage reads the region the previous stage wrote, so the regions never overlap. Stage 1 needs `g(n)`, stage 2 `g(g(n))`, stage 3
`g(g(g(n)))` and stage 4 `polygon_bound(n)`, which are the proven bounds. Because the bounds are data independent, the capacities are
checked up front: `out_capacity < polygon_bound(n)` or `scratch_count < scratch_size(n)` is `BufferTooSmall` even when fewer points
would be produced. Both functions return 0 for a count below 3 or above 4096.

Checks in order: `NullBuffer` (`in`, `out`, `scratch`, `out_count`, all required), `BadCount`, `BadRect`, `BadPoint`,
`BufferTooSmall`, `Overlap`. Nothing is written on any error: no stage runs before all checks have passed, and `out`, `scratch` and
`*out_count` are untouched.

## clip_polyline

Each segment is clipped by the `clip_segment` rule. A run of consecutive visible segments continues while the clipped start of the
next segment is exactly the clipped end of the previous one (rounded integer points are compared); then only the end point is
appended, so a run of `k` segments writes `k + 1` points. An invisible segment, or a clipped start that differs from the previous
clipped end, closes the run (its end index is recorded in `piece_ends`) and starts a new one. A segment visible in a single point
(touching, or a degenerate one) writes a piece of two equal points. Nothing is removed (no de-duplication).
Bounds: a run of `k >= 1` segments has `k + 1 <= 2 * k` points, so the output has at most `2 * (n - 1)` points, and every piece
holds at least one segment, so at most `n - 1` pieces. Capacities below these bounds are `BufferTooSmall`, even if fewer are produced.

Checks in order: `NullBuffer` (`in`, `pts`, `piece_ends`, `piece_count`, `out_points`, all required), `BadCount` (below 2 or above
4096), `BadRect`, `BadPoint`, `BufferTooSmall`, `Overlap`. Nothing is written on error.

## Overlap

The touched ranges are compared in every pair with the shared modular address rule (as in core/composite, core/resample, core/sdf
and core/tess): clear the top byte, take the unsigned difference modulo `2^64`, answer "no overlap" at a distance of `2^47` or more
before any other arithmetic, then compare exact ranges. Ranges, all of positive length:
- polygon: `in` (`n` points), `scratch` (`scratch_size(n)` points), `out` (`polygon_bound(n)` points, not the capacity) and `*out_count`
  (4 bytes);
- polyline: `in` (`n` points), `pts` (`2 * (n - 1)` points), `piece_ends` (`n - 1` words), `*piece_count` and `*out_points`
  (4 bytes each, which also makes the two counters overlap if they are the same word).
The `2^47` branch cannot be executed with real buffers (it needs two ranges more than `2^47` bytes apart), so it is argued here, not
tested. A range whose extent ends exactly where another starts is not an overlap.

## digest_points

FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over the bytes of the `x` then `y` of each point, each as a 32-bit
little-endian two's complement word. A null pointer or a zero count hashes zero bytes and returns the offset.

## Tests and goldens

`tests/clip/clip_tests.cpp` checks:
- statuses and their order, no write on error, exact capacities, every overlap pair and boundary, and aliasing of `a` and `b`;
- `polygon_bound` and `scratch_size` for every count 3..4096 (sums 42469366 and 47190004 from the independent script);
- 304 named segment rows (38 segments against 8 rectangles: crossing, touching, edge-collinear, corner, degenerate, point and line
  rectangles, rounding ties, +/-2^28 extremes; 152 visible), 105 named polygon rows (35 shapes in three windings: convex,
  concave comb and star, self-intersecting, enclosing, inside, outside, touching, repeated and collinear points, ties, point and
  line rectangles, +/-2^28 extremes), and 17 named polylines;
- every endpoint pair of the 5x5 grid (625 segments) against seven rectangles;
- random corpora (a 64-bit LCG with multiplier `6364136223846793005`, increment `1442695040888963407`, high 32 bits): 3 segment
  corpora of 3000 (seeds 11, 12, 13), 3 polygon corpora of 400 (21, 22, 23) and 3 polyline corpora of 400 (31, 32, 33) with ranges 6,
  1000 and 2^28;
- a worst-case search (the Python script and the test use the same LCG).

The expected values are independent: an exact `Fraction` script computes the visible interval from the candidate parameters (`0`, `1`
and the four edge crossings) instead of Liang-Barsky, rounds the exact rational with `floor(v + 1/2)`, and has its own FNV. The test
file also holds references in 64-bit integers (the same candidate method with rationals, and a vector-based Sutherland-Hodgman with the
numerator form of the rounding) and compares every result with them.

| Grid rectangle | Visible of 625 | Digest |
| --- | --- | --- |
| (1,1)-(3,3) | 473 | `0x1edf34229c9f38d5` |
| (0,0)-(4,4) | 625 | `0xcb1abd5ec6fedde5` |
| (2,2)-(2,2) | 89 | `0x00130d9be4fabb85` |
| (1,2)-(3,2) | 287 | `0x3170391f5f22591d` |
| (2,0)-(2,4) | 425 | `0x26c8f1c943fc3f45` |
| (1,1)-(2,3) | 380 | `0x5739b853f9078a01` |
| (-1,-1)-(0,0) | 49 | `0xe623768314e97d65` |

| Worst-case search (20000 random polygons, rectangle (0,0)-(10,10), points in [-6,16]) | n=3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- |
| Largest output | 7 | 10 | 12 | 14 | 16 | 18 |
| Outputs above 2n | 204 | 206 | 118 | 56 | 37 | 24 |

The corpora and the search are supporting evidence on small inputs. The bound itself is proved above; the tests never see an output
above `polygon_bound`, and the 4096-point zigzag reaches the first stage bound of 6144 points.

## Measured cost (local, Linux g++, not CI)

The sanitizer-safe performance test clips a 4096-point random polygon, a 4096-point polyline and a 4096-point zigzag, including the
in-file reference, in a few milliseconds (about 1 ms for the whole test case with the code compiled -O2 uninstrumented, about 4 ms with ASan+UBSan at -O0). Each stage is
linear in its input size, so a polygon clip is linear in the point count (at most `polygon_bound(n)` points of work in the
last stage).
