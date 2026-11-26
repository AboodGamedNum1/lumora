# Bezier flattener

The flattener (`core/flatten/flatten.h`, `core/flatten/flatten.cpp`) turns a quadratic or cubic Bezier curve into a polyline with
`n + 1` points, `n` in `1..256`, for core/stroke and core/tess. It uses integer arithmetic only (64-bit), no floating point, no
libm, no heap allocation, no recursion, no static mutable state and no 128-bit type. Results are identical on every platform.
Evidence label: software (CPU) only.

## Inputs

Control point coordinates have magnitude at most `2^24`, the tolerance `tol` is in `1..2^16`. The functions take the points by value,
so only the output buffer and the counter can overlap. An invalid input gives 0 from the segment count functions.

## Segment count

Quadratic `(p0, c, p1)`: `M = |p0 - 2c + p1|`. Cubic: `M = max(|p0 - 2 c0 + c1|, |c0 - 2 c1 + p1|)`. The vectors are exact integers
with each component at most `4 * 2^24 = 2^26`, so `M^2 <= 2 * 2^52 = 2^53` and `M <= 2^26 * sqrt(2) < 2^26.5`.

`n` is the smallest integer in `1..255` that passes the test, else 256: quadratic `M^2 <= 16 tol^2 n^4`, cubic
`9 M^2 <= 16 tol^2 n^4`. The test is monotone in `n`, so the first hit is the smallest. For `M = 0` the answer is `n = 1`.

Overflow: `16 tol^2 n^4` reaches `2^68`, so it is never formed. With `T = tol * n * n` (at most `2^32`, exact in `int64`) the two tests
are the real-number statements `M <= 4T` and `3M <= 4T`, because both sides are non-negative and squaring preserves the order.

- Quadratic threshold: if `T >= 2^25` then `4T >= 2^27 > 2^26.5 >= M`, so the test is true and `n` is accepted without squaring.
  Otherwise `T < 2^25`, so `16 T^2 < 2^54`, and `M^2 <= 2^53`: both sides fit `int64`.
- Cubic threshold: `3M < 3 * 2^26.5 < 2^28.1`. If `T >= 2^27` then `4T >= 2^29`, so the test is true. Otherwise `T < 2^27`, so
  `16 T^2 < 2^58`, and `9 M^2 <= 9 * 2^53 < 2^57`: both sides fit `int64`.
- `T` itself needs only `tol * n * n <= 2^16 * 2^16 = 2^32`.

The tests check the thresholds against the literal 128-bit comparison on random curves with tolerances across the whole range.

## Output points

Output point `i` is the Bernstein form at `t = i / n`, rounded to the nearest integer per coordinate, ties away from zero:
quadratic numerator `(n - i)^2 p0 + 2 (n - i) i c + i^2 p1` over `n^2`, cubic numerator `(n - i)^3 p0 + 3 (n - i)^2 i c0 + 3 (n - i) i^2 c1
+ i^3 p1` over `n^3`. The weights sum to the denominator, so a numerator is at most `2^24 * n^2 <= 2^40` (quadratic) or
`2^24 * n^3 <= 2^48` (cubic) in magnitude. The rounding is `sign * ((2 |num| + den) / (2 den))` on non-negative values; the sum
`2 |num| + den` is at most `2^49 + 2^24`. Every output lies within the convex hull of the control points, so it fits `int32`.
The first point is `p0` and the last `p1`: at `i = 0` the weights are `(den, 0, ...)` and at `i = n` they are `(..., 0, den)`, so both are
exact; the code also stores them directly. Degenerate curves (all points equal, collinear) are not reduced: `n + 1` points come out,
duplicates included. `n = 1` gives exactly `p0, p1`.

## Deviation bounds

Let `L` be the chord polyline through the exact samples at `t = i / n`, with `h = 1 / n`. For a vector function `f` on one segment, the
interpolation error satisfies `|f(t) - L(t)| <= (h^2 / 8) * max |f''|` (Euclidean norm; the error is the integral of the second derivative
against a non-negative kernel of total weight `h^2 / 8`).

- Quadratic: `B''(t) = 2 (p0 - 2c + p1)`, constant, so the error is at most `2M h^2 / 8 = M / (4 n^2)`. This bound is attained: at the
  middle of a segment `B - L = -(p0 - 2c + p1) / (4 n^2)` exactly. The tests check this identity in integers.
- Cubic: `B''(t) = 6 ((1 - t) (p0 - 2 c0 + c1) + t (c0 - 2 c1 + p1))`, a convex combination, so `|B''| <= 6M` and the error is at most
  `6M h^2 / 8 = 3M / (4 n^2)`. This bound is not tight in general (the two second differences are weighted by `t` and `1 - t`).

When `n < 256`, or `n = 256` and the test passes, the test above gives `M / (4 n^2) <= tol` (quadratic) resp. `3M / (4 n^2) <= tol` (cubic).
So the polyline through the exact samples is within `tol` of the curve.

Rounding: each output point differs from its exact sample by at most `1/2` per coordinate, at most `sqrt(2) / 2 < 3/4` in length.
A point of a chord is a convex combination of its two end points, so it moves by at most the same amount. The rounded polyline is
therefore within `tol + sqrt(2) / 2` of the curve. For `tol = 1` this is above `tol`; the header keeps the mandated `n` and rounding.
The tests measure exact rational positions (8 sub-points per segment) and check `tol` for the exact samples and `tol + 3/4` for the
rounded output. They are evidence, not the proof.

## When the tolerance is not reachable

`n` is clamped to 256 and the status stays Ok with 257 points. The deviation bound holds only when the test passes at `n = 256`:
`M <= 4 tol * 256^2 = 2^18 tol` (quadratic) or `3M <= 2^18 tol` (cubic). With `tol = 1` that is `M <= 262144` resp. `M <= 87381`.
The largest possible `M` is `2^26 * sqrt(2) = 94906265.6`, so every curve is covered only when `tol >= 363` (quadratic) and `tol >= 1087`
(cubic). The largest `n < 256` boundary is `M = 4 tol * 255^2` (quadratic); one more selects 256. The tests pin both sides.

## Checks and overlap

Order: NullBuffer (`out`, `out_count`), BadTolerance, BadPoint (every control point), BufferTooSmall (`out_capacity < 257`, even for a
line that needs two points), Overlap. Nothing is written on an error. Overlap uses the shared rule: clear the top byte, take the unsigned
difference modulo `2^64`, answer no overlap at `|delta| >= 2^47` before other arithmetic, then compare exact byte ranges. The extent of
`out` is always 257 points, whatever `n` is and whatever capacity is passed; the counter is 4 bytes. The `2^47` branch cannot be reached
with real buffers in one process, so it is argued here and not tested; the mutation check that lowers it survives for that reason.

## Tests

`tests/flatten/flatten_tests.cpp` holds named cases with explicit points, exhaustive scaled grids (quadratic: 5x5 grid of 3 points, scales 1,
3, 50, 100000, tolerances 1 and 7; cubic: 3x3 grid of 4 points, scales 1, 300, 100000, tolerances 1 and 40), three random corpora (seeds
11, 12, 13 with ranges 50, 5000, `2^24`), the closed form boundaries of `n`, the threshold check, the quadratic attained bound, the
deviation measurements, every status and check order, no write on error, overlap, and a performance case of 600 extreme curves. Expected
values come from an independent Python reference with exact integers (its own comparison with the literal `16 tol^2 n^4`, its own
rounding and FNV). The deviation tests use a small 128-bit product written in the test file.
