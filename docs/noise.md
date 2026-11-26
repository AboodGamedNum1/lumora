# Noise module

The noise module (`core/noise/noise.h`, `core/noise/noise.cpp`) is the CPU reference for procedural textures: value noise,
gradient noise, cellular noise and fractal sums. It uses integer arithmetic only (64-bit where needed), no floating point,
no libm, no static mutable state, and no pointer value in any decision. Results are identical on Linux, Windows and Android.
Evidence label: software (CPU) only.

## Positions, cells and Q16

- A position is an `int32` in Q16. The cell index is `floor(p / 65536)` and the fraction is `p & 0xFFFF` (0..65535), both
  computed on the `uint32` bit pattern without any signed shift. The cell index is therefore -32768..32767 for every
  `int32` position, including `INT32_MIN` (cell -32768, fraction 0) and `INT32_MAX` (cell 32767, fraction 65535).
- The neighbour cell `+1` never wraps (the largest cell is 32767, its neighbour is 32768). Cell indices are passed to the hash
  as `int32` and reinterpreted as `uint32`, so negative cells hash their two's complement bit pattern.
- Outputs are Q16 in 0..65535.

## Hash

`hash2` and `hash3` are exactly the definitions in the header, with all arithmetic modulo 2^32. The mixer maps the all-zero
input (seed 0, a 0, b 0) to 0, which is part of the definition. `hash3(seed, x, y, z) = hash2(seed, hash2(seed, x, y), z)`
where the inner `uint32` result is reinterpreted as `int32`. Goldens are in the tests.

## Fade

`fade(t)` for `t` in 0..65536 returns `round_half_up(T / 2^64)` where `T = t^3 * (6t^2 - 15 * 65536 * t + 10 * 2^32)` is the
exact integer value of `t^3 (t (6t - 15) + 10)` with Q16 constants. `T` reaches 2^80 at `t = 65536`, so it does not fit in
64 bits. The product is formed exactly as a 128-bit value from four 32 x 32 partial products (no compiler intrinsic, no
`__int128`), half of 2^64 is added with carry, and the high word is the result. The inner factor is at most
`10 * 2^32` and is always positive on the domain; `t^3 <= 2^48`. There is one rounding only. Results: `fade(0) = 0`,
`fade(32768) = 32768`, `fade(65536) = 65536`, and the function is monotone on 0..65536 (the tests check all 65537 inputs).
An input above 65536 is clamped to 65536.

## Interpolation

Every linear interpolation with weight `w` in 0..65536 is `lerp(a, b, w) = floor((a * (65536 - w) + b * w + 32768) / 65536)`:
one rounding, half up, exact for signed `a` and `b` (the floor of a negative numerator is computed without signed shifts).
At `w = 0` it returns `a` and at `w = 65536` it returns `b`.

- 2D: weights `u = fade(fx)` and `v = fade(fy)`; `lerp(lerp(c00, c10, u), lerp(c01, c11, u), v)`.
- 3D: the same in each of the two z planes (4 lerps in x, 2 in y), then one lerp in z with `w = fade(fz)`.

Value noise corners are `hash >> 16` (0..65535), `hash2` for 2D and `hash3` for 3D. The interpolated value lies between the
smallest and largest corner, so it stays in 0..65535 (the rounding cannot leave the corner range because the weights sum to
65536 and each term is exact before the single division).

## Gradient noise

Corner hash: `hash2` (2D) or `hash3` (3D) of the corner cell. The table index is taken from the top half of the hash.

2D, 8 gradients, index `(h >> 16) & 7`:

| index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| (gx, gy) | (1,0) | (-1,0) | (0,1) | (0,-1) | (1,1) | (-1,1) | (1,-1) | (-1,-1) |

3D, 12 gradients, index `(h >> 16) % 12`:

| index | 0 | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- | --- |
| (gx, gy, gz) | (1,1,0) | (-1,1,0) | (1,-1,0) | (-1,-1,0) | (1,0,1) | (-1,0,1) |

| index | 6 | 7 | 8 | 9 | 10 | 11 |
| --- | --- | --- | --- | --- | --- | --- |
| (gx, gy, gz) | (1,0,-1) | (-1,0,-1) | (0,1,1) | (0,-1,1) | (0,1,-1) | (0,-1,-1) |

The corner dot product uses the Q16 offset from the corner to the sample. For the corner at cell offset `(i, j)` the offset
is `(fx - 65536 i, fy - 65536 j)`, so the components are in -65536..65535. The dot product is the integer gradient times
the offset, an `int64`. The dots are interpolated with the lerp above (signed), giving `raw`, a Q16 number where 65536 is 1.0.

Mapping: `out = clamp(32768 + floor((raw + 1) / 2), 0, 65535)` (round half up of `raw / 2`) for both dimensions. There is
no per-dimension scaling. The zero level is 32768; on every lattice point all dot products are zero, so the output is
exactly 32768.

Range proof. Each dot is a sum of two (2D) or two nonzero (3D, every table row has exactly two nonzero entries) products of
a component in {-1, 0, 1} and an offset component of magnitude at most 65536, so `|dot| <= 131072`. Every lerp returns
a value between its two inputs (same argument as for value noise), so `|raw| <= 131072`, and the pre-clamp value lies in
-32768..98304. All intermediates are at most 2^17 * 2^16 = 2^33 in magnitude, far inside `int64`.
The clamp then guarantees the output is in 0..65535 for every input. The clamp is also reached in practice only at the
very peak: with the four corner gradients pointing at the cell centre (gradients (1,1), (-1,1), (1,-1), (-1,-1) at the
corners (0,0), (1,0), (0,1), (1,1)) each dot is 65536, `raw = 65536`, the pre-clamp value is 65536 and the output is 65535.
The tests check this case with seed 1740. Random sweeps stay well inside the range for 3D (largest seen about 0.93 of full scale) and reach 0.99 for 2D; the proof above does not depend on those measurements.

## Cellular noise

One feature point per cell. For the cell `(cx, cy)` let `h = hash2(seed, cx, cy)`; the point is at the Q16 offset
`(h & 0xFFFF, h >> 16)` inside the cell. The result is the nearest feature point among the 3 x 3 cells around the
sample's cell, by definition of this module. This is not necessarily the global nearest point: in rare cases a point in a
cell two steps away is closer than the nearest point in the 3 x 3 block, and it is ignored. For a sample at `(fx, fy)` inside
its cell and a neighbour at `(i, j)` with `i, j` in -1..1: `dx = 65536 i + (h & 0xFFFF) - fx`, `dy = 65536 j + (h >> 16) - fy`,
`d2 = dx^2 + dy^2` as `uint64` (at most about 2 * 131072^2, far below 2^64). The result is `min(65535, isqrt(min d2))`,
where `isqrt` is the exact floor of the square root (digit by digit, no floats). Cell indices of the neighbours use `uint32`
addition so the border cells are defined. Standing on a feature point gives 0.

## fBm

For `octaves` in 1..8, octave `i` samples at `(uint32(x) << i, uint32(y) << i)` (wrapping, reinterpreted as `int32`) with seed
`seed + i` (wrapping), and weight `128 >> i` (128, 64, 32, 16, 8, 4, 2, 1). With `S` the weighted sum and `W` the sum of the
weights used, the result is `floor((2 S + W) / (2 W))`: one rounding, half up, always in 0..65535. `kind` picks
`value2`, `gradient2` or `cellular2`. A direct call with a `Kind` outside the enum or `octaves` outside 1..8 returns 0.
One octave equals the base function.

## Image

`fill_image` sets pixel `(i, j)` to `fbm2(kind, seed, x, y, octaves) >> 8` with `x = uint32(x0) + i * uint32(step)` and
`y = uint32(y0) + j * uint32(step)` (wrapping, then reinterpreted as `int32`). `step` may be zero (a constant image) or
negative. Checks run in enum order: null buffer, then size (width or height 0, above 8192, or stride below width), then
kind, then octaves. Nothing is written on error, and the stride padding is never written. Status texts: `ok`,
`null buffer`, `bad size`, `bad kind`, `bad octaves`; any other value gives `unknown`.

`digest_image` is FNV-1a 64 (offset basis 14695981039346656037, prime 1099511628211) over the bytes of each row, rows top
to bottom, `width` bytes per row at `stride` spacing. A null pointer or a zero size hashes zero bytes and returns the offset
basis. The stride is not validated.

## Continuity

For a step of one Q16 unit the output of every function changes by at most 3 (the tests check this across cell borders,
including negative cells). The reason: the fade has slope at most 1.875 per unit, the corner values differ by at most 65535
(value noise), so a step changes the value by at most 1.875 plus rounding; for gradient noise a step moves each dot by at
most 1 and the weights by 1.875 times the dot spread (at most 131072 / 65536), and the output halves it; for cellular noise the
distance is 1-Lipschitz and `isqrt` adds at most 1.

## Verification

The expected values in `tests/noise/noise_tests.cpp` come from an independent Python reference (big integers, floor
division, `math.isqrt`) that shares no code with the implementation. Hand-computed tests cover lattice points, cell
centres, the 2D peak and a gradient value worked out from the table. The tests also cover INT32 extremes, status order,
stride padding, and a 256 x 256 image on the heap.

Golden image digests (FNV-1a 64 of the image, tight stride). Columns: kind (0 value, 1 gradient, 2 cellular), seed, x0, y0,
step, octaves, width x height, digest.

| kind | seed | x0 | y0 | step | octaves | size | digest |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 1 | 0 | 0 | 4096 | 1 | 64 x 48 | 0xb3a05315137dfa94 |
| 1 | 2 | -100000 | 50000 | 3000 | 1 | 64 x 64 | 0xafa4ee21cd008561 |
| 2 | 3 | 0 | 0 | 5000 | 1 | 50 x 40 | 0x68d76a07a93ce352 |
| 0 | 4 | 0 | 0 | 2000 | 8 | 64 x 64 | 0xa232be1b517e5af8 |
| 1 | 5 | INT32_MIN | INT32_MIN | 977 | 8 | 64 x 64 | 0x7abf0ce488fad7ba |
| 2 | 6 | 2147482648 | 2147482648 | 6000 | 3 | 64 x 64 | 0xaf355f51918f92ff |
| 1 | 7 | 12345 | -54321 | 0 | 4 | 16 x 16 | 0x9464fb8317157025 |
| 0 | 8 | 100 | 200 | -3333 | 2 | 40 x 30 | 0xd6dd4ae084658a90 |
| 2 | 9 | 0 | 0 | 3000 | 8 | 32 x 32 | 0x9fa6b66f0fbee0d2 |
| 1 | 11 | -500000 | 300000 | 1500 | 3 | 256 x 256 | 0x18548d7ca78b8518 |
| 2 | 12 | 0 | 0 | 2500 | 2 | 256 x 256 | 0x246aad63182dcb11 |
