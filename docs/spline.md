# 2D paths and splines

`core/spline/spline.h` and `core/spline/spline.cpp` implement 2D paths: linear polylines, uniform Catmull-Rom splines
and cubic Bezier chains, each open or closed. They provide evaluation, first derivatives, an arc-length table and even
resampling. Pure CPU code with no I/O. The interface is in `spline.h`; this page records the rules the implementation
follows.

## Determinism

Results must be bit-identical on Linux, Windows and Android. Only `+ - * /` and `sqrt` (an exact IEEE operation) are
used, in `double`, and no libm transcendental function is called. Source pragmas ask compilers not to fuse
multiply-add, and the project must not enable fast-math. The golden digests below were also produced with `-O0`,
`-O3 -march=native -ffp-contract=fast` and `-Os` and did not change.

## Validation order and statuses

`validate_path` returns the first failing check in this order:

1. `BadKind`: the kind is not 0, 1 or 2.
2. `TooFewPoints`: Linear and CatmullRom need at least 2 points (open or closed), CubicBezier needs at least 4 (open)
   or 3 (closed).
3. `TooManyPoints`: more than `kMaxPoints` (1024). This is checked before the Bezier shape rule.
4. `BadPointCount`: CubicBezier only. Open needs `1 + 3 * segments` points, closed needs `3 * segments`.
5. `BadPoint`: a point that is NaN, infinite or has a coordinate with absolute value over 1e6 (1e6 itself is valid).
   A null `points` pointer with a count that passed the checks above is also `BadPoint`.

`path_status_text` returns short fixed texts, and `unknown status` for a value outside the enum.

## Segments

| Kind | Open | Closed |
| --- | --- | --- |
| Linear | `count - 1` | `count` (the last segment returns to point 0) |
| CatmullRom | `count - 1` | `count` |
| CubicBezier | `(count - 1) / 3` | `count / 3` (the last end point is point 0) |

`segment_count` is 0 for an invalid path. A closed path with 2 points is a there-and-back path with 2 segments.

## Formulas

All evaluation is in double. The result is rounded once to float. `u` is clamped to 0..1 and NaN gives 0. A segment
index at or above `segment_count`, or an invalid path, gives `{0, 0}` for both `eval_segment` and `tangent_segment`.

- Linear: `P0 + (P1 - P0) u`. Tangent: `P1 - P0`.
- Catmull-Rom (uniform, tension 0.5), segment `i` runs from `P_i` to `P_{i+1}` using `P_{i-1}` and `P_{i+2}`:
  `0.5 * (2 P1 + (P2 - P0) u + (2 P0 - 5 P1 + 4 P2 - P3) u^2 + (3 P1 - P0 - 3 P2 + P3) u^3)`.
  Tangent: `0.5 * ((P2 - P0) + 2 (2 P0 - 5 P1 + 4 P2 - P3) u + 3 (3 P1 - P0 - 3 P2 + P3) u^2)`.
  Open paths duplicate the end points for the missing neighbours, so the tangent at the first point is half the first
  chord and at the last point half the last chord. Closed paths wrap indices.
- Cubic Bezier with points `A B C D` (`p0 c0 c1 p1`), `v = 1 - u`: `v^3 A + 3 v^2 u B + 3 v u^2 C + u^3 D`.
  Tangent: `3 v^2 (B - A) + 6 v u (C - B) + 3 u^2 (D - C)`. Segment `s` uses points `3s` to `3s + 3`, wrapping modulo
  `count` for closed paths.

## Arc length

`build_length_table` flattens each segment into 32 chords at `u = k / 32` (the last at exactly 1) and sums the chord
lengths in double. `cumulative[0] = 0`, `chords = segments * 32` and `total = cumulative[chords]`. It returns false and
leaves the table untouched for an invalid path. The table is about 262 KB: allocate it on the heap, never on the stack.

Approximation error: the polyline is shorter than the curve. For the unit quarter circle Bezier (control constant
0.5522847498) the table gives 1.5708587714, which is 6.2e-5 above the true pi / 2 (the chord shortfall is about 1e-4 and
the Bezier lies slightly outside the circle). Straight lines are exact up to double rounding.

`eval_at_length(p, table, s)`:

- `s` is clamped to 0..total and NaN gives 0. A table whose chord count does not equal `segments * 32`, or exceeds the
  maximum, or an invalid path, gives `{0, 0}`.
- It finds the first chord whose end length is greater than `s`, so zero-length chords are skipped, and interpolates
  linearly between that chord's two end points (along the chord, not the curve).
- At `s = total` it returns the exact end point of the last segment.

`resample_even(p, n, out)` writes `n` points at `s_i = total * i / (n - 1)`, with the last point at exactly `total` and
`n == 1` giving `s = 0`. It returns false for an invalid path, `n == 0` or `n > 65536`, and leaves `out` untouched.
Consecutive resampled points are at most the arc spacing apart (shorter only across sharp corners).

## Digest

`digest_points` is FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over the little-endian IEEE bit
patterns of `x` then `y` of each point, in order. `-0` and `+0` differ.

## Golden digests

Each path is resampled to 100 points. The point sets are `kPoly` (7 points) and `kBez` (10 points; the closed Bezier
uses the first 9) in `tests/spline/spline_tests.cpp`, which checks the same values.

| Path | Digest |
| --- | --- |
| Linear, open | `0x197a7ea2afb2ed2a` |
| Catmull-Rom, open | `0x6cdcbdbcf7df6bbe` |
| Catmull-Rom, closed | `0x65020a83861edd11` |
| Cubic Bezier, open | `0x3b8b259c3d2a9778` |
| Cubic Bezier, closed | `0x260d2de6ee281cb7` |

## Evidence limits

The tests are CPU only. Numbers measured on a local sanitized Linux build are local results. Only the CI jobs of the
pull request count as CI evidence, and no GPU, device or other-compiler claim is made here.
