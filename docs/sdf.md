# SDF module

The SDF module (`core/sdf/sdf.h`, `core/sdf/sdf.cpp`) is the CPU reference for exact squared Euclidean distance transforms and
8-bit signed distance fields for crisp scalable text, shapes and UI. It uses integer arithmetic only (64-bit where needed), no
floating point, no libm, no heap allocation, no static mutable state, and no pointer value in any decision except the overlap
check. Results are identical on Linux, Windows and Android. Evidence label: software (CPU) only.

## Limits and memory

- Width and height are 1..4096 (`kMaxSize`). The spread is 1..127 pixels (`kMaxSpread`). A stride is in bytes and at least `width`.
- `distance_squared` needs no scratch: it works in the caller's output (`width * height` `uint32`).
- `make_sdf` uses the caller's scratch (`width * height` `uint32`, overwritten) and the output bytes. Nothing else is large.
- Stack use is two local arrays of `kMaxSize` entries: 4096 `uint32` (16 KB) and 4096 `uint16` (8 KB), 24 KB in total, plus a few
  scalars.

## Set pixels and thresholds

A pixel is SET when `mask >= threshold` (inclusive). Threshold 0 makes every pixel set. When no pixel is set every output value is
`kNoSet` (0xFFFFFFFF). A set pixel has the value 0.

## Exact distance transform

The squared distance to the nearest set pixel is computed in two exact passes (no approximation, no chamfer, no floats):

- Column pass. For each column two scans (top to bottom, then bottom to top) give the exact distance in rows to the nearest set
  pixel of that column. A column with no set pixel is marked as "no value" and is skipped later.
- Row pass. For each row the value `f(x) = (column distance)^2` of every finite column is a parabola `f(x') + (x - x')^2`. The
  lower envelope of these parabolas gives, for every `x`, `min over x' of f(x') + (x - x')^2`, which is exactly the minimum over all
  set pixels of the squared distance (the minimum over a pixel is first taken within its column and then over the columns).
  The envelope is built left to right with a stack of column indices. A parabola on the stack is dropped when its intersection with
  the new parabola lies at or before its intersection with the parabola below it. Intersections are never divided: with
  `s(a, b) = ((f(b) + b^2) - (f(a) + a^2)) / (2 (b - a))` the test `s(b, c) <= s(a, b)` is done by cross multiplication in `int64`,
  `num1 * den2 <= num2 * den1`. The denominators are positive and at most `2 * 4095`, the numerators are below `2^27`
  (`f <= 4095^2 < 2^24`, `x^2 < 2^24`), so every product is below `2^41`.
  The envelope is then walked once: the pointer advances while the next parabola gives a value less than or equal to the current
  one. Ties choose either parabola; the value is the same, so the result does not depend on tie handling.

Bounds. A column distance is at most 4095 and its square at most `4095^2 = 16,769,025`. An output value is a column square plus
`(x - x')^2 <= 4095^2`, so at most `2 * 4095^2 = 33,538,050`, below `kNoSet`. The result equals the brute-force minimum over all set
pixels; the tests compare it with a brute force on many masks.

## Integer square root

`isqrt(v) = floor(sqrt(v))` for every `uint64`, by the digit-by-digit method (no floats). The result is at most `2^32 - 1`. The tests
check it for every `v` up to `2^20`, at squares, squares plus or minus one, and `UINT64_MAX`.

## Signed distance field

`inside = (mask >= 128)`. Two exact transforms are computed into the one scratch: first the squared distance to the nearest inside
pixel (used for the outside pixels), then the squared distance to the nearest outside pixel (used for the inside pixels). The
distance in Q8 is `q = isqrt(d2 * 65536)`; `d2 * 65536 <= 33,538,050 * 65536 < 2^42`. If the other class does not exist the
transform returns `kNoSet` and `q = spread * 256`.

Encoding, with `den = spread * 256` and `s = +q` (inside) or `-q` (outside):

`out = clamp(128 + sign * r, 1, 255)` with `r = floor((2 * q * 127 + den) / (2 * den))`

`r` is `q * 127 / den` rounded to nearest, ties away from zero (a tie is `2 q 127 = (2 r - 1) den`, which rounds up in
magnitude). The rounding is applied to the magnitude, so it is symmetric for inside and outside.

Clamp proof. If `q <= den` then `q * 127 / den <= 127`, so `r <= 127` and `128 +- r` lies in `1..255`: the clamp does nothing. Only
a pixel farther than the spread (`q > den`) can leave the range, and the clamp then gives 255 (inside) or 1 (outside). The
intermediate `2 * q * 127 + den` is below `2^34`.

The value 128 is never produced. Distances are between pixel centres, so the nearest pixel of the other class is at least one
pixel (256 in Q8) away, and the smallest magnitude is `round(127 / spread)`, at least 1. An inside pixel is therefore at least
`128 + round(127 / spread)` (at least 129) and an outside pixel at most `128 - round(127 / spread)` (at most 127). For spread 1
an edge-adjacent pixel is 255 or 1. For spread 127 it is 129 or 127.

Inverting the mask (`255 - mask`) swaps the classes exactly, and because the rounding and the clamp are symmetric, the two
outputs of any pixel sum to 256. A mask with no inside pixel gives 1 everywhere; one with no outside pixel gives 255 everywhere.

## Reading back

`edge_coverage(v, aa)`: `t = (v - 128) * 255 + aa * 255`, `c = floor(t / (2 aa))` with floor division for negative `t`, result
`clamp(c, 0, 255)`. For `aa` outside 1..127 the result is 0 for `v < 128` and 255 for `v >= 128` (the range is tested before any
arithmetic, so a huge `aa` cannot overflow). `|t| < 255 * 255`, far inside `int64`.

## Validation order

- `distance_squared`: `NullBuffer` (mask or out null), `BadSize` (width or height 0 or above 4096, or stride below width),
  `Overlap` (the mask rows against the contiguous output extent of `width * height * 4` bytes).
- `make_sdf`: `NullBuffer` (mask, out or scratch null), `BadSize` (width or height, and both strides), `BadSpread`,
  `ScratchTooSmall` (`scratch_count < width * height`, compared as 64-bit), then `Overlap`: any two of the touched mask rows,
  the touched output rows, and the first `width * height * 4` bytes of the scratch that intersect.
- Nothing is written on error. The output stride padding is never written.
- `status_text` returns `ok`, `null buffer`, `bad size`, `bad spread`, `scratch too small`, `overlap`, and `unknown` otherwise.

## Overlap

The same rule as `core/composite` and `core/resample`. Rows are `width` bytes (mask and out) at their stride; the scratch and the
`distance_squared` output are one contiguous block of `width * height * 4` bytes. For two row sets:

- Take both base addresses as `uint64` and clear the top byte (bits 56..63), so a tagged pointer compares by its address bits.
- Take the difference modulo 2^64 and read it as signed without negating the minimum value (the absolute value is formed as an
  unsigned number).
- If the absolute difference is at least 2^47, they cannot overlap: return "no overlap" before any more arithmetic.
- Otherwise every row of the first set is checked exactly against the rows of the second set. The first candidate row is found
  by a division and only that range is tested, so the cost is linear in the number of rows.

Why the cutoff is safe: the extent of a mask or output surface is at most `4095 * stride + width`, below `2^12 * 2^32 + 2^12 < 2^45`,
and a contiguous block is at most `4096 * 4096 * 4 = 2^26` bytes, so two blocks whose base addresses are 2^47 or more apart are
disjoint. On 32-bit targets the address is zero extended to 64 bits and the same arithmetic applies. Row start offsets are below
`2^47 + 2^45`, so they fit in `int64`. Real buffers cannot be placed 2^47 apart in tests, so the cutoff branch is argued here and not
executed.

## Digest

`digest_image` is FNV-1a 64 (offset basis 14695981039346656037, prime 1099511628211) over `width` bytes of each row, top to bottom.
A null pointer or a zero width or height returns the offset basis.

## Verification

The expected values in `tests/sdf/sdf_tests.cpp` come from an independent Python reference (numpy brute force over all set pixels,
big-integer encoding) that shares no code with the implementation. The 1024 x 1024 digests use the nearest boundary pixel only
(a set pixel with an unset 4-neighbour; the nearest set pixel of any unset pixel is always such a pixel), which was cross-checked
against the full brute force on a 96 x 96 ring. Hand-computed tests cover a 3 x 3 centre pixel, a 1 x 5 line, the inclusive
threshold, spread 1 and 127, absent classes and the symmetry. The 4096 x 4096 test with a single set pixel (distances `x^2 + y^2`
up to `2 * 4095^2`) is not compiled under AddressSanitizer; the sanitized run covers 1024 x 1024.

Mask shapes for the goldens (integer formulas, `mn = min(width, height)`, `d = (x - width / 2)^2 + (y - height / 2)^2`):
circle `d < (mn / 3)^2`; ring `(mn / 4)^2 <= d <= (mn / 2 - 1)^2`; stripes `((x / 4 + y / 8) mod 2) = 1`; ramp `x * 255 / (width - 1)`;
checker `((x / 5 + y / 5) mod 2) = 0`; border pixels; diagonal `x = y`; corner `(width - 1, height - 1)`. Set pixels are 255 and unset 0
except the ramp. Digests (the `distance_squared` digest is FNV-1a 64 over the little-endian bytes of the `uint32` values; threshold 128):

| shape | size | spread | distance_squared digest | make_sdf digest |
| --- | --- | --- | --- | --- |
| circle | 64 x 64 | 1 | 0x7dbfe6ef7548b416 | 0x558c7dae5f8dc3c3 |
| circle | 64 x 64 | 8 | 0x7dbfe6ef7548b416 | 0x13d25493fa7286db |
| ring | 64 x 48 | 6 | 0x49319e16e7d65836 | 0x992ebbc6f76be7f3 |
| ring | 48 x 64 | 127 | 0x8968fad3f15996d6 | 0xb21fe803dd670d9a |
| stripes | 60 x 40 | 3 | 0x52d6d203be37a4e5 | 0xa2b12ad983a1b745 |
| ramp | 50 x 20 | 10 | 0x0734f327b76842e5 | 0x693f1319d7002945 |
| checker | 45 x 45 | 4 | 0x5bf3a0db8928fdf5 | 0x51f58978f6f212f6 |
| border | 30 x 20 | 5 | 0xca6ea684ba8ff765 | 0x9571555afda9f041 |
| diag | 40 x 40 | 2 | 0x9206a8ecc124e209 | 0xf065ce87d1461b07 |
| corner | 33 x 17 | 127 | 0xb627111647ef1f22 | 0xd7378cfb85ead1a5 |
| circle | 1 x 50 | 4 | 0xb11ac1aa598bae7d | 0x6632beb8d4090fe7 |
| stripes | 50 x 1 | 4 | 0xe2c713632868322c | 0xe23e1faf351d6457 |

The 1024 x 1024 circle with spread 20: `distance_squared` digest 0xbd16eb72cf290682, `make_sdf` digest 0xef6ba41a08b97d0f.
The `edge_coverage` matrix digest over `v` = 0..255 for `aa` = 0, 1, 2, 3, 7, 64, 126, 127, 128, 1000, 4294967295 is 0xbebb6990f6af964e.
