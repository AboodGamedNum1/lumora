# 2D transforms and camera

`core/xform/xform.h` and `core/xform/xform.cpp` implement affine 2D matrices, rotation with an own sine and cosine,
exact determinants, inversion, an orthographic camera and outward-rounded bounds. Pure CPU, no I/O, no threads. The
interface is in `xform.h`; this page records every convention and the numeric rules.

## Determinism

Results are bit-identical on Linux, Windows and Android. Only `+ - * /` and comparisons on floats and doubles are used;
no libm function is called (no sin, cos, nextafter, floor, fabs). The source asks compilers not to fuse multiply-add,
and the project must not enable fast-math. The golden digests below were also produced with `-O0`,
`-O3 -march=native -ffp-contract=fast` and `-Os` and did not change. A float times a float is exact in double (48
significant bits, far inside the exponent range), so each matrix entry is one double expression converted to float once.
The conversion adds `0.0` first, so a negative zero never comes out of any function.

## Conventions

- Column vectors: `x' = a*x + c*y + tx`, `y' = b*x + d*y + ty`.
- World space is y up. Screen space has the origin at the top-left corner of the viewport and y down, in pixels.
- `rotation(r)` turns counter-clockwise in a y-up space: `a = cos r`, `b = sin r`, `c = -sin r`, `d = cos r`.
- `multiply(lhs, rhs)` applies `rhs` first. Each entry is a fixed left-to-right double evaluation of exact float
  products and sums, with no FMA, converted to float once. The double sum rounds once and the float conversion rounds once; this
  is the documented behaviour, not an exact one.
- `compose_trs(t, r, s)` is `T * R * S`: scale first, then rotate, then translate. An invalid angle acts as 0.
- Camera: `local = R(-theta) * (world - position)`, `screen = (width/2 + zoom * local.x, height/2 - zoom * local.y)`.
  The camera frame is rotated counter-clockwise by `theta`, so the world content appears to turn clockwise: with
  `theta = +90 degrees`, the world direction +y points toward screen right and +x toward screen down.
- `camera_matrix` is the matrix of that map, `a = z*cos`, `b = z*sin`, `c = z*sin`, `d = -z*cos`,
  `tx = width/2 - (a*px + c*py)`, `ty = height/2 - (b*px + d*py)`, each entry converted to float once. Every
  other camera helper uses it: `world_to_screen` applies it, `screen_to_world` applies its inverse, and
  `visible_world_bounds` transforms the screen rectangle `(0, 0, width, height)` by the inverse.
  Because the matrix entries are floats, the center position maps to the viewport center only within float rounding.
- `world_to_screen` and `screen_to_world` return false only for an invalid camera. A non-finite point is not checked and the NaN or
  infinity propagates into the output.

## Rotation, sine and cosine

`rotation` returns the identity for NaN, infinity and `|radians| > 1e4`. Otherwise it reduces `x = k*(pi/2) + r` with
`k = round(x * 2/pi)` using `pi/2` split into three parts: two parts with 33 significant bits (so `k*part` is exact
for the `|k| <= 6367` that occurs) and the remaining part as a double. Then `r` (`|r|` about `pi/4`) goes through a Taylor
polynomial evaluated by Horner's rule: sine up to `r^19`, cosine up to `r^20`; the truncated term is below 1e-20.
The quadrant `k mod 4` selects `sin`, `cos` and the signs. The double result is rounded to float once.

Accuracy: the evaluation error is about 1e-16 in double, so the float result is the correctly rounded value except when
the exact value lies within about 1e-16 relative of a rounding tie. Measured:

- 40 arguments in `tests/xform/xform_tests.cpp` against sine and cosine computed with 90-digit decimal arithmetic:
  every error is at most half a float ulp (the test allows 0.1% more) and the maximum is checked to be under 0.5005 ulp.
- A local scratch comparison on 200000 arguments in `[-1e4, 1e4]` (400000 values) against the double result of the
  platform's libm, rounded to float: no difference, largest error 0.49999 ulp. That is a local check, not CI.

Symmetries `sin(-x) = -sin(x)` and `cos(-x) = cos(x)` hold bit for bit. `rotation(0)` is exactly the identity.
At multiples of 90 degrees the float closest to `k*pi/2` is not exactly `k*pi/2`, so entries are near 0 and 1 within
about `1.5e-7 * (|x| + 1)`, which is the tolerance used by the tests.

## Determinant and inverse

`determinant` is `a*d - b*c`. Both products are exact in double, so the single subtraction rounds the exact
determinant once; it is zero only if the exact determinant is zero. `invert` returns false, leaving `out` untouched,
when an entry is not finite, when that determinant is 0, or when any inverse entry is not finite as a float (for
example `a = 1e-40` makes `1/a` overflow). The entries are `d/det`, `-b/det`, `-c/det`, `a/det` and
`(c*ty - d*tx)/det`, `(b*tx - a*ty)/det`, each with the double operations in that order and one float conversion.
Round trip tolerance used in the tests for well conditioned matrices: `|apply(inv, apply(m, p)) - p| <= 1e-5 * (1 + |px| + |py|)`.

## Rectangles and bounds

`transform_rect` transforms the four corners. Each coordinate is the exact value of `a*x + c*y + tx` (two exact
products and a float, kept as an exact sum), rounded **outward** to a float: the lower bound is the largest float not
above the exact value, the upper bound the smallest float not below it. Neighbouring floats are found by integer
operations on the bit pattern, never by a library call. If a bound exceeds the float range it becomes infinite on the
outside only. If the matrix or the rectangle holds a non-finite value, all four bounds are NaN. The result is always
ordered, also for mirrors.

`visible_world_bounds` is `transform_rect(inverse(camera_matrix), (0, 0, width, height))`, so it encloses exactly the
corners that `screen_to_world` returns for the viewport corners.

## Validation order

`validate_camera` returns the first failing check: `BadViewport` (width or height not finite, `<= 0` or `> 1e6`),
`BadZoom` (not finite, `<= 0`, `> 1e6` or `< 1e-6`), `BadPosition` (not finite or `|coordinate| > 1e9`),
`BadRotation` (not finite or `|radians| > 1e4`). The limits themselves are valid. Invalid cameras make
`camera_matrix`, `world_to_screen`, `screen_to_world` and `visible_world_bounds` return false and leave their output
untouched.

## Digest

`digest_floats` is FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over the little-endian 4 bytes of each
float bit pattern in order.

## Golden digests

Both digests are computed in `tests/xform/xform_tests.cpp`, which checks the same values.

| Set | Digest |
| --- | --- |
| Matrices: `rotation(1)`, `rotation(100)`, `rotation(-3)`, `compose_trs((10,-20), 0.5, (2,3))`, that matrix times `rotation(100)`, the inverses of the `compose_trs` matrix and of `rotation(1)`, and the image of `(1.5, -2.5)` | `0x4834887ccaefa160` |
| Cameras: four cameras (zoom 1 to 40, rotations 0, 0.75, -2, 3), each with its matrix, visible bounds, the screen position of world `(3, 4)` and the world position of screen `(10, 20)` | `0xed727f6d2b619bb9` |

## Evidence limits

The tests are CPU only. Numbers from a local sanitized Linux build with GCC are local results. Only the CI jobs of the
pull request count as CI evidence; no GPU, device or other-compiler claim is made here.
