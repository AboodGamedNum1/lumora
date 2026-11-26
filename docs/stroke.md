# Polyline stroker

The stroke module (`core/stroke/stroke.h`, `core/stroke/stroke.cpp`) turns a polyline into convex counter-clockwise contours for
a NonZero fill: one quad per segment, one triangle or miter quad per join, and optional square caps. It uses integer arithmetic
only (64-bit), no floating point, no libm, no heap allocation, no recursion, no static mutable state and no negative signed
shifts. Results are identical on every platform. Evidence label: software (CPU) only.

## Inputs and limits

- 2..1024 points, each coordinate at most `2^28` in magnitude, `half_width` h in `1..2^19`, `miter_limit` in `1..64`.
- Points are collapsed first: consecutive equal points merge into one, and for a closed line the last point is compared with the
  first once. One step is enough, because after the consecutive merge the only equal pair left across the seam is last and first.
  Fewer than two collapsed points gives no contour for `Cap::Butt` and the axis aligned square of side `2h` for `Cap::Square`,
  also when closed. Capacities and bounds always use the original point count.
- A closed line of two collapsed points emits two segment quads (forward and back) and no join, because the turn is a full
  reversal and a reversal has a zero cross product.

## Rounding rules

- `isqrt_nearest(d2)` is the integer `L` with `L * (L - 1) < d2 <= L * (L + 1)` (and 0 for 0). It is the floor root `s`
  (bitwise method) if `d2 <= s^2 + s`, else `s + 1`. No tie exists. It saturates at `2^32 - 1` only for `d2 > 2^64 - 2^32`, far
  beyond the contract (`d2 <= 2^60`).
- The left normal of a segment with `d = q - p` is `n = (round(-dy * h / L), round(dx * h / L))`, `L = isqrt_nearest(dx^2 + dy^2)`.
  Rounding is nearest with ties away from zero, written as `sign * ((2 * |num| + den) / (2 * den))` on non-negative values.
  `|num| <= 2^29 * 2^19 = 2^48`, so everything fits `int64`.
- `L >= max(|dx|, |dy|)` (the nearest root of a sum of squares is at least the larger term), so `|n.x|, |n.y| <= h`.

## Contour order and winding

1. Segment quads `(p - n, q - n, q + n, p + n)` in order, the closing segment last for a closed line.
2. Joins in point order (open: points `1..k-2`; closed: points `0..k-1`), only when `cross = d1 x d2` is not zero.
3. Square caps of an open line: first the cap at the first point, then at the last point.

Segment quads are strictly convex and counter-clockwise. With `d x n >= d2 * h / L - (|dx| + |dy|) / 2` and `L <= s + 1/2`:
for `d2 = 1`, `L = 1` and the value is `h - 1/2 > 0`; for `d2 >= 2`, `s >= sqrt 2`, which keeps the bound positive for every
`h >= 1`. The tests also check every direction up to 9 with h up to 9.

Square caps: with `d = (n.y, -n.x)`, the first cap is `(p0 - n, p0 + n, p0 + n - d, p0 - n - d)` and the last is
`(pe + n, pe - n, pe - n + d, pe + n + d)`. The first edge vector is `2n` and the next is `-d`, with cross product `2 |n|^2 > 0`, so
both are counter-clockwise by construction and need no reversal. The tests assert area above zero and strict convexity.

## Joins

The outer side is the right side for a left turn (`cross > 0`) and the left side otherwise. `o1, o2` are the outer offsets,
`a = p + o1`, `b = p + o2`.

- Bevel: the triangle `(p, a, b)`. If its doubled signed area is negative it is listed `(p, b, a)`. A zero area triangle keeps
  `(p, a, b)` and is still emitted, so the contour count never depends on rounding.
- Miter: `m = o1 + o2`. If `m` is the zero vector (rounded outer normals cancel although the turn is not zero, for example
  `(0,0),(0,3),(1,0)` with h = 1) the join is a Bevel. Otherwise the tip offset is `t - p = round(m * 2h^2 / |m|^2)` per
  coordinate. If either coordinate magnitude exceeds `miter_limit * h` the join is a Bevel. Otherwise the squared test
  `|t - p|^2 <= (miter_limit * h)^2` decides. The pre-check equals the squared test in effect, since `|c| > lim` implies
  `c^2 > lim^2`, and it keeps the squares at most `2^52`. The quad `(p, a, t, b)` is listed `(p, b, t, a)` if its doubled signed
  area is negative. It is kept only if all four cyclic turns are strictly positive; otherwise the join is a Bevel.
- Why the convexity rule: with integer rounding the tip can land on the wrong side. Exact counterexamples, all in the tests with
  contours from the reference: `(0,0),(0,1),(1,2)` with h = 3 gives a reflex quad `(0,1),(-3,4),(-2,2),(-3,1)`;
  `(0,0),(1,1),(2,3)` with h = 3 gives the self-intersecting `(1,1),(4,-2),(3,0),(4,-1)`; `(0,0),(0,3),(1,0)` with h = 1 has a zero
  `m`. The reference counted, on the 5x5 grid of three points with h in 1,2,3,5: 343104 contours, 8272 of zero area,
  1400 reflex and 96 self-intersecting; with random coordinates to `2^28`: 16598 zero area, 1382 reflex and 4 self-intersecting
  among 542587 contours. All of these end as Bevel triangles under the rules above.
- A full reversal and a straight pass have `cross = 0` and add no contour.

## Arithmetic and output bounds

- `cross <= 2 * 2^29 * 2^29 = 2^59`.
- `m` coordinates `<= 2^20`; `|m|^2 <= 2^41`; `m * 2h^2 <= 2^20 * 2^39 = 2^59`; the rounded tip offset is at most `2h^2 <= 2^39`.
  The numerator `2 * |num| + den` fits `int64`.
- `miter_limit * h <= 2^25`, so the squares in the limit test are at most `2^52`.
- Orientation values use coordinate differences below `2^30`, so products are below `2^60` and the values below `2^61`.
- Output coordinates: input `2^28` plus a normal offset (at most `2^19`, doubled for caps and the square, `2^20`) or a miter
  tip (at most `2^25`). All stay below `2^29`, so the narrowing to `int32` is exact.

## Bounds

A segment contributes one contour each and a join contributes at most one. Open: `S = n - 1` segments, `J = n - 2` joins, 2 caps:
`2n - 1` contours. Closed: `n` segments, `n` joins, no caps: `2n`. `point_bound = 4 * contour_bound`. Both are 0 for an invalid
count. Every contour has three or four points, so the output never exceeds `point_bound`. The bounds are data independent and
checked before the overlap rule.

## Checks and overlap

Order: NullBuffer (all five pointers), BadCount, BadStyle (half width, miter limit, join, cap; the miter limit is checked even for
Bevel), BadPoint, BufferTooSmall, Overlap. Nothing is written on an error. Overlap uses the shared rule: clear the top byte,
take the unsigned difference modulo `2^64`, answer no overlap at `|delta| >= 2^47` before other arithmetic, then compare the exact
byte ranges. The extents are `in` (n points), `pts` (`point_bound` points), `contour_ends` (`contour_bound` words) and the two
counters, every pair. The `2^47` branch cannot be reached with real buffers in one process, so it is argued here and not tested;
the mutation check that lowers it survives for that reason.

## Tests

`tests/stroke/stroke_tests.cpp` holds explicit contours for named cases, digests for the style grid, three exhaustive grids
(3 points on 4x4, 4 points on 3x3, 2 points on 3x3, all styles), three random corpora (seeds 41, 42, 43; ranges 6, 1000, `2^28`),
square root properties and goldens, every status and check order, no write on error, overlap for every pair of regions and edge,
and a 1024 point performance case. The expected values come from an independent Python reference with exact integers. Every
output is also checked for contour size, winding, strict convexity, counts, ranges and untouched memory beyond the result.
