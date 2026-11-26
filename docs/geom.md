# 2D polygon geometry

`core/geom/geom.h` and `core/geom/geom.cpp` implement validation, exact orientation, area, ear-clipping triangulation,
point-in-polygon and convex hull for simple 2D polygons without holes. Pure CPU code with no I/O. The interface is in
`geom.h`; this page records the rules, the exactness argument and its limits.

## Determinism

Results must be bit-identical on Linux, Windows and Android. Only `+ - * /` and comparisons are used and no libm
function is called. Every geometric decision is exact (see below), so no result depends on rounding, on FMA
contraction or on the optimisation level. Source pragmas ask compilers not to fuse multiply-add anyway, and the
project must not enable fast-math. The golden digests below were also produced with `-O0`,
`-O3 -march=native -ffp-contract=fast` and `-Os` and did not change.

## Exact orientation: argument and limits

The header says float inputs make the determinant exactly computable in double. That is **not true** for the plain
formula `(bx-ax)(cy-ay) - (by-ay)(cx-ax)`:

- A difference of two floats is exact in double only when their exponents are within about 29 of each other. Floats
  reach from 2^-149 to 2^128, so a difference can need about 270 bits.
- Even with exact differences the product of two of them can need 106 bits, more than double holds.
- The tests contain float triples (around 2^18 against values near 1e-3) where the plain double determinant has the
  wrong sign.

The implementation does not rely on the claim. It uses the expanded form

`det = bx*cy - bx*ay - ax*cy - by*cx + by*ax + ay*cx`

(the `ax*ay` terms cancel exactly). Each product of two floats **is** exact in double: 24 bit times 24 bit significands
give 48 bits, and the product stays far inside double's exponent range for every finite float (smallest about 2^-298,
largest about 2^256). The six exact terms are added exactly as a nonoverlapping floating-point expansion (`two_sum`
using only `+` and `-`, exact unless it overflows, and these sums stay below 2^270). The sign of the sum is the sign of
the largest component. A plain double determinant with Shewchuk's error bound `3.33e-16 * (|left| + |right|)` is used
only as a filter; if it cannot prove the sign, the exact expansion decides.

Result: `orient` is the exact sign for **all finite float inputs**, with no coordinate bound. Limits: a non-finite
coordinate returns 0, and the argument covers float inputs only.

`signed_area2` adds the shoelace terms (exact products) to one expansion. Its sign and zero are exact; the returned
double is the expansion summed from the smallest component up, within one unit in the last place of the exact value.
For integer coordinates whose area is representable the result is exact.

## Validation order

`validate_polygon` returns the first failing check in this order:

1. `TooFewVertices`: fewer than 3.
2. `TooManyVertices`: more than `kMaxVertices` (4096).
3. `BadVertex`: null pointer, or any NaN, infinite or `|coordinate| > 1e6` (1e6 itself is valid).
4. `Degenerate`: all vertices coincide or are collinear (exact: every vertex has `orient == 0` against the first two
   distinct vertices). A zero net area figure eight is not degenerate; it is self-intersecting.
5. `SelfIntersecting`: two adjacent edges that run in the same direction from their common vertex, a zero-length
   edge, or any two non-adjacent edges that cross or touch (shared point, vertex on an edge, collinear overlap), all
   decided exactly. Collinear vertices that continue straight are allowed.

## Triangulation

`triangulate` returns 0 for an invalid polygon and leaves the output untouched. Otherwise it writes counter-clockwise
triangles of vertex indices and returns the triangle count, which is `n - 2` unless collinear vertices were removed
without a triangle.

- Winding comes from the exact sign of the area. For clockwise input the ear `(prev, cur, next)` is written as
  `(prev, next, cur)`, so every triangle is counter-clockwise.
- Ear choice: scan the remaining vertices cyclically forward, starting at vertex 0 for the first clip and at the
  previous vertex of the last clip afterwards. The first valid ear in that order is clipped.
- A vertex with `orient == 0` (collinear) is clipped as a zero-area ear without writing a triangle.
- A convex vertex is a valid ear when no other remaining vertex lies inside or on the closed triangle. Only reflex or
  collinear vertices can block, so only they are tested.
- There is no fallback pass. Validation rejects touching edges and all tests are exact, so by the two ears theorem
  a valid polygon always has an ear (a vertex lying exactly on a candidate diagonal blocks it, which is why the
  inside test is closed). If a pass finds no ear the function returns 0 as a loop guard; this was never reached in a
  seeded fuzz test of valid polygons with collinear vertices (the test "triangulate always finds an ear" repeats it with 60000 seeded draws).
- The last three vertices write one triangle unless they are collinear.

Triangle areas add up exactly to the polygon area when the coordinates are integers.

## Point in polygon

`point_in_polygon` uses the even-odd rule with exact `orient`. A point on an edge or vertex is inside. A NaN or
infinite point, a null polygon, `n < 3` or a non-finite vertex gives false. It does only an O(n) pass and does not run
the simplicity test; for a non-simple polygon the even-odd answer is still defined.

## Convex hull

Andrew monotone chain on a stable sort by `(x, y)`. Duplicates are removed (`-0` and `+0` are the same point and the
first in input order stays), collinear points are removed with exact `orient`, and the hull is counter-clockwise
starting at the lowest x (lowest y on ties). The result has 0 points for `n == 0`, 1 for one distinct point, and 2
(the two extremes) when all points are collinear. Any NaN or infinite point returns 0. There is no coordinate bound.

## Digest

`digest_indices` is FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over the little-endian 4 bytes of
each index in order.

## Golden digests

Shapes (counter-clockwise integer coordinates) are `kStar` (10 vertices), `kComb` (12), `kSpiral` (16) and `kSquareCw`
(8 vertices, clockwise, with collinear midpoints) in `tests/geom/geom_tests.cpp`, which checks the same values.

| Shape | Triangles | Digest |
| --- | --- | --- |
| Star | 8 | `0x4c8185a3172a36a7` |
| Comb | 10 | `0x8ae73f62f25b81bf` |
| Spiral | 14 | `0x2bc354ceabfdba3d` |
| Square, clockwise, collinear vertices | 6 | `0xd9a50c642d2e1d37` |

## Evidence limits

The tests are CPU only. Numbers measured on a local sanitized Linux build are local results. Only the CI jobs of the
pull request count as CI evidence, and no GPU, device or other-compiler claim is made here.
