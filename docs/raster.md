# Raster module

The raster module (`core/raster/raster.h`, `core/raster/raster.cpp`) fills polygons into an 8-bit coverage mask with integer
arithmetic only. The result is a definition, not an approximation of geometry: the same input gives the same bytes on every
platform and compiler. This file states every rule. The header comments carry the same rules in short form.

## Types and limits

- `Fx` is `int32` in 24.8 fixed point, so one pixel is 256 units. Coordinates must lie in `[-2^24, 2^24]` (`kMaxCoord`).
- A target is at most 8192 x 8192 (`kMaxSize`) and `stride >= width`. The stride padding is never written.
- At most 4096 contours (`kMaxContours`), each with 3 to 2^20 points (`kMaxPointsPerContour`). Contours are closed
  implicitly, the last point joins the first.
- `contour_count == 0` is valid and clears the mask to 0.

## Status checks

`fill_polygons` checks in this order and returns the first failure. On any error nothing is written.

1. `NullBuffer`: `target.data` is null; or `contours` is null with `contour_count > 0`; or a contour has `count > 0` and a
   null `pts`. (When `contour_count > 4096` the contour array is not inspected for null points, the count is the error.)
2. `BadSize`: width or height is 0 or above 8192, or `stride < width`.
3. `BadFill`: the `Fill` value is not `NonZero` or `EvenOdd`.
4. `BadContour`: `contour_count > 4096`, or any contour has fewer than 3 or more than 2^20 points.
5. `BadPoint`: any coordinate outside `[-2^24, 2^24]`. The limit itself is allowed.

## Edge snapping

Slanted edges cross pixel boundaries at rational positions. Every edge is snapped to integers by this fixed rule. The rule
depends on the edge only (not on the polygon around it), so two shapes that share an edge, in either direction, get the
same pieces.

1. Normalize the edge so that y increases. Swapping the endpoints flips the orientation sign (`+1` if the edge already
   went down, `-1` if it was swapped). Horizontal edges and repeated points contribute nothing.
2. For each row boundary `Y = j * 256` strictly inside the edge, with `n = (x1 - x0) * (Y - y0)` and `d = y1 - y0 > 0`, the
   crossing is `x0 + floor((2n + d) / (2d))`. This is round to nearest with ties toward +infinity, using floor division, so
   it is correct for negative `n`. The original endpoints are kept exactly. The edge is now a chain of row segments.
3. Each row segment is normalized so x increases and split at the vertical lines `X = i * 256` strictly inside it by the
   same rule with x and y swapped: `y = yl + floor((2m + e) / (2e))`, `m = (yr - yl) * (X - xl)`, `e = xr - xl`.
4. Every resulting piece has integer coordinates and lies inside one pixel.

Ties: `n / d` exactly `k + 1/2` rounds to `k + 1`, also for negative values (-1/2 rounds to 0, -3/2 rounds to -1).

## Accumulation

For one piece in one pixel, with `cover = sign * (y end - y start)` (a signed height in 0..256 units) and `lp`, `lq` the x
offsets of its two ends inside the pixel (0..256), the piece adds `cover` to the pixel's `cov` and
`cover * (512 - lp - lq)` to its `area`. That is twice the signed trapezoid area to the right of the piece inside the pixel.

The doubled area of a pixel is `A2 = area + 512 * (sum of cov of the pixels to its left in the same row)`. `A2` is in units
of 1/131072 pixel^2. Everything is an integer, so the sum is exact and does not depend on the order of edges or contours.

## Coverage rule

- NonZero: `v2 = min(|A2|, 131072)`. This is absolute-area saturation, not a winding number. Two same-direction contours
  saturate, opposite directions cancel.
- EvenOdd: `m = |A2| mod 262144`, `v2 = m` if `m <= 131072`, else `262144 - m`.
- `coverage = (v2 * 255 + 65536) >> 17`.

Reversing every contour flips every sign and gives the same bytes. The order of the contours does not matter.

## Clipping

Clipping is applied to the snapped pieces, so the result equals rendering into an unbounded target and cropping.

- Pieces above row 0 or at or below the last row are dropped.
- Pieces left of `x = 0` collapse onto the left border of column 0 (both x offsets 0), keeping their `cover`, so they
  still contribute to every pixel to their right exactly as a vertical edge at `x = 0` would.
- Pieces at or right of `x = width * 256` are dropped (they only influence pixels further right).

## Memory and bands

Memory does not depend on the coordinate range, the number of points or the number of contours. The accumulator is two
dense `int64` arrays (`cov`, `area`) of `band_rows * width` cells where `band_rows = clamp(2^20 / width, 1, height)`. That is
at most 2^20 cells per array, 16 MiB for both arrays together, allocated on the heap once per call. The target is processed
in bands of `band_rows` rows. For each band every edge is visited once and rejected at once when its y range misses the band,
so the work is `bands * edges` plus the cells touched by the pieces. Nothing is allocated per edge.

At the limits (4096 contours of 2^20 points) the edge count is 2^32; the rejection scan then dominates and the call is slow
but correct. Typical inputs are far below that.

## Bounds

All intermediate values fit `int64`, and the products below also fit the types used.

- Edge deltas are at most `2^25`. Row crossing: `n <= 2^25 * 2^25 = 2^50`, so `2n + d < 2^51 + 2^25`.
- Column split inside a row segment: `m <= 2^8 * 2^25 = 2^33`.
- One piece: `|cover| <= 256` and `512 - lp - lq` in `[0, 512]`, so `|cover * (512 - lp - lq)| <= 2^17`.
- One edge adds at most one piece to a given pixel, and the pieces of one edge in one row add up to at most 256 to `cov`.
  There are at most `4096 * 2^20 = 2^32` edges. So per pixel `|area| <= 2^32 * 2^17 = 2^49`. Along one row,
  `|sum of cov| <= 2^32 * 256 = 2^40` whatever the width, so `512 * sum <= 2^49`.
- Hence `|A2| <= 2^49 + 2^49 = 2^50`, far below `2^63`. The fold multiplies `v2 <= 131072` by 255.
- For `flatten_*`, the squared norm of a difference of up to four coordinates: components at most `2^26`, squares
  at most `2^52`, sum at most `2^53`, times 9 at most `2^57`. All fit `int64`.

## Flattening

`flatten_quad` and `flatten_cubic` append points after `p0` and return their count. They return 0 and write nothing when
`out` is null, when `cap` is smaller than the number of points, or when any control point is outside `[-2^24, 2^24]`.

Depth test, on the original control points only, with `tol` (values below 1 are treated as 1, values above 2^28 are
clamped to 2^28):

- Quad, `d = p0 - 2*p1 + p2`: the smallest `k` in 0..6 with `|d|^2 <= (4 * tol * 4^k)^2`.
- Cubic, `d1 = p0 - 2*p1 + p2`, `d2 = p1 - 2*p2 + p3`: the smallest `k` in 0..6 with
  `9 * max(|d1|^2, |d2|^2) <= (4 * tol * 4^k)^2`.
- `|.|^2` is the exact squared Euclidean norm in 64 bits. When `4 * tol * 4^k >= 2^28` the test passes without computing it.
- If no `k` passes, `k = 6`.

The curve is split by de Casteljau `k` times. Midpoints use `(a + b + 1) >> 1` (arithmetic shift) per coordinate at every
step. The output is the `2^k` end points of the leaf segments, in parameter order, the last one exactly the final control
point. The intermediate midpoints use `int64`, so no overflow occurs at the coordinate limits.

Hand check, quad `(0,0) (0,1024) (1024,0)`, `tol = 64`: `d = (1024, -2048)`, `|d|^2 = 5242880`; `(4*64*4^k)^2` first exceeds it
at `k = 2`. Level one: `a = (0,512)`, `b = (512,512)`, `m = (256,512)`. The first leaf of the left half has end point
`(64,384)`. The four points are `(64,384) (256,512) (576,384) (1024,0)`.

## digest_mask

FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over the pixel bytes, rows top to bottom, `width` bytes
per row, padding excluded. A target with null data or a zero width or height hashes zero bytes, which gives the offset
basis. Known values: `"a"` gives `af63dc4c8601ec8c`, `"foobar"` gives `85944171f73967e8`.

## Golden digests

The digests below come from an independent Python reference (naive per-edge accumulation written from the rules above). The
same constants are in `tests/raster/raster_tests.cpp`. The cases are in that file by name.

| Case | Target | Rule | digest_mask |
| --- | --- | --- | --- |
| rect_aligned | 4 x 4 | NonZero | `39374f4ff20962c9` |
| rect_frac_all_sides | 4 x 3 | NonZero | `ad4ed2ef0e6e919c` |
| triangle_odd | 8 x 8 | NonZero | `6e5dd1400d05cd75` |
| star_nonzero | 8 x 8 | NonZero | `325a60709f687ca3` |
| star_evenodd | 8 x 8 | EvenOdd | `6e4746bc54cafa59` |
| quad_whole (also the union of two triangles) | 8 x 8 | either | `2a6e7f1c1daceb3f` |
| tie_neg_num_a | 4 x 4 | NonZero | `200102b1c243b032` |
| slanted_far_coords | 5 x 5 | NonZero | `96fdd03483dfbb16` |
| large triangle and rectangle | 4096 x 4096 | NonZero | `4041e8c50354ce33` |
| large triangle and rectangle | 4096 x 4096 | EvenOdd | `79099e7bf6aef0b9` |
| wide triangle and rectangle (several bands) | 8192 x 300 | NonZero | `dc163e8ee5fdf1a4` |
| wide triangle and rectangle (several bands) | 8192 x 300 | EvenOdd | `2a5a4b9264f4ab04` |

## Tests

The tests check exact masks (aligned and fractional rectangles, triangles, a pixel-sized offset square), overlap under both
rules in both directions, the self-intersecting star, partly and fully outside contours, every status and the check order,
untouched stride padding, rounding ties with negative numerators, steep and shallow edges, a shared slanted boundary
(union of two triangles equals the quad byte for byte), reversal of all contours, clipped output equal to a larger
output cropped (also after a whole-pixel shift), flatten points and caps, a contour with 2^20 points, 4096 contours, and the
large-target digests above. The large cases use heap buffers and no timing bound.

## Evidence limits

The reference and the C++ were compared on thousands of random polygons (negative, outside and extreme coordinates, both
rules) on one Linux compiler. Matching digests on other compilers and platforms are checked by CI, not claimed here.
