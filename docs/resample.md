# Resample module

The resample module (`core/resample/resample.h`, `core/resample/resample.cpp`) is the CPU reference for image scaling and mip
chains on RGBA8 surfaces with premultiplied alpha. It uses integer arithmetic only (64-bit where needed), no floating point,
no libm, no heap allocation, no static mutable state, and no pointer value in any decision except the overlap check. Results
are identical on Linux, Windows and Android. Evidence label: software (CPU) only.

## Pixels, sizes and limits

- A pixel is 4 bytes R, G, B, A, premultiplied. Sizes are 1..8192 per side (`kMaxSize`). A stride is in bytes and at least
  `width * 4`. Stride padding is never written.
- Memory: there is no heap use and no array larger than a few `int64` values (tap weights and four accumulators per pixel).
  The stack use is a few hundred bytes. The only large objects are the caller's surfaces.

## Sample mapping

For destination index `i` of `N` from a source of `n` pixels (the same rule on each axis):

`sx = floor(((2i + 1) * n * 65536) / (2N)) - 32768`

The numerator is at most `16383 * 8192 * 65536 < 2^43`, evaluated in `uint64` on non-negative numbers, then 32768 is
subtracted. `sx` is therefore in `-32768 .. n * 65536` and may be negative. It is split without a shift of a negative number:
`x0 = floor(sx / 65536)` and `fx = sx - x0 * 65536` in 0..65535 (a negative `sx` gives `x0 = -1`, `fx = sx + 65536`).

## Nearest

`x = floor(((2i + 1) * n) / (2N))`, always in `0..n-1`. The pixel is copied, then the alpha clamp below applies.

## Bilinear

Taps `x0` and `x0 + 1` (and the same for y) clamped into `0..n-1`, weights `65536 - fx` and `fx`. The 2D sum is
`S = sum(wy * wx * value)`. The four weights sum to `2^32`, so `S <= 2^32 * 255 < 2^40`, and the result is `floor((S + 2^31) / 2^32)`: one rounding,
half up, always in 0..255.

## Catmull-Rom

Taps `x0 - 1 .. x0 + 2` (clamped), weights from the Catmull-Rom cubic with `t = f / 65536`:

- `w0 = (-t^3 + 2t^2 - t) / 2`
- `w1 = (3t^3 - 5t^2 + 2) / 2`
- `w2 = (-3t^3 + 4t^2 + t) / 2`
- `w3 = (t^3 - t^2) / 2`

In Q16 each exact weight is `N / 2^33` where `N` is an integer polynomial in `f`:

- `N0 = -f^3 + 2 * 65536 f^2 - 2^32 f`
- `N1 = 3 f^3 - 5 * 65536 f^2 + 2^49`
- `N2 = -3 f^3 + 4 * 65536 f^2 + 2^32 f`
- `N3 = f^3 - 65536 f^2`

`f^3 < 2^48`, so every `N` fits in `int64` with a large margin. Each weight is rounded half up: `floor((N + 2^32) / 2^33)`, using a
floor that never shifts a negative value. The four rounded weights then sum to `65536 + e` with a small error `e`. The
correction rule: the tap with the larger magnitude, `w1` for `f <= 32768` and `w2` for `f > 32768`, is replaced by
`65536` minus the other three. The sum is exactly 65536 by construction. The correction moves that tap by at most 2 units
(measured maximum 1.5 over all fractions; each of the three rounded taps is off by at most 0.5). Checked exhaustively over all
65536 fractions: the sum is 65536, `w0 <= 0`, `w3 <= 0`, `w1, w2 <= 65536`. Fixed points: `f = 0` gives `(0, 65536, 0, 0)` and
`f = 32768` gives `(-4096, 36864, 36864, -4096)`, which need no correction.

Bounds. Before correction the absolute weight sum is `1 + t (1 - t) <= 1.25`, so at most 81920 in Q16. The correction adds
at most 2, so `sum |w| <= 81922` per axis. The 2D sum is `S = sum(wy * wx * value)` with `|S| <= 81922^2 * 255 < 2^41`
(about 1.7e12). The result is `floor((S + 2^31) / 2^32)`, clamped to 0..255, then the alpha clamp applies. At identity size `fx = 0`
exactly, so the weights are `(0, 65536, 0, 0)` and the result is the source pixel.

## Box

Destination pixel `i` covers the source interval `[i * n / N, (i + 1) * n / N)`. In units of `1 / N` source pixel it is
`[i * n, (i + 1) * n)` and source pixel `k` is `[k * N, (k + 1) * N)`. The weight of `k` is the overlap
`max(0, min((i + 1) n, (k + 1) N) - max(i n, k N))`, an exact integer. The source range per destination pixel is
`k = floor(i * n / N) .. ceil((i + 1) * n / N) - 1`, computed once per destination pixel (and once per row for y); the tap
loops contain no division. The weights of one axis sum to `n`. The 2D weight is the product of both axes, so the weights sum
to `w * h`, the result is `floor((2 S + c) / (2 c))` with `c = w * h` (one rounding, half up, exact in 64 bits).

Bounds. One axis weight is at most `min(n, N) <= 8192`; a product is at most `2^26`; the sum of all weights is `w * h <= 2^26`.
So `S <= 2^26 * 255 < 2^34` and `2 S + c < 2^36`. The overlap terms use products of at most `8192 * 8192 = 2^26`. Cost is
about `(w + W) * (h + H)` taps per channel set, because each source pixel is visited by at most two destination pixels per axis.
Box also works when upscaling (one or two taps); the header describes it for downscaling.

At identical size the overlap of destination `i` is only source `i` with weight `n`, so the result is exactly that pixel:
Box equals Nearest (both before and after the alpha clamp).

## Alpha clamp and identity

For every filter the last step clamps each of R, G, B to the result alpha A; A is kept. So the output always satisfies
`R, G, B <= A`. For valid premultiplied input (R, G, B <= A) the clamp does nothing. At identical size Nearest, Box, Bilinear and
Catmull-Rom all give the source pixel, so for valid pixels each is an exact copy. For malformed pixels (some colour above
alpha) every filter gives the clamped pixel, and Box still equals Nearest. Mip level 0 is the one exception: it is a literal
byte copy of the source and is not clamped. Later mip levels are the clamped Box resize of the previous tight level.

## Validation order

`resize` checks in this order: `NullBuffer` (a data pointer is null), `BadSize` (a width or height is 0 or above 8192, or a stride
is below `width * 4`, for source or destination), `BadFilter` (outside the enum), `Overlap`. Nothing is written on error.

## Overlap

The source rows (`width * 4` bytes each at `stride` spacing) and the destination rows are compared as byte ranges. The two
surfaces may have different row counts, strides and lengths. The rule is:

- Take both base addresses as `uint64` and clear the top byte (bits 56..63), so a tagged pointer compares by its address bits.
- Take the difference modulo 2^64 and read it as signed without negating the minimum value (the absolute value is formed as an
  unsigned number).
- If the absolute difference is at least 2^47, the surfaces cannot overlap: return "no overlap" before any more arithmetic.
- Otherwise check every destination row against the source rows exactly. For each row the first candidate source row is found by
  a division and only that range is tested, so the cost is linear in the number of rows.

Why the cutoff is safe: the extent of a surface is at most `8191 * stride + width * 4`, below `2^32 * 2^13 + 2^15 < 2^46`, so
two surfaces whose base addresses are 2^47 or more apart are disjoint. The same holds for the mip chain output (below 2^29 bytes).
On 32-bit targets the address is zero extended to 64 bits and the same arithmetic applies. Row start offsets are below `2^47 + 2^45`, so they fit in `int64`.
Real buffers cannot be placed 2^47 apart in tests, so the cutoff branch is argued here and not executed.

`build_mip_chain` compares the whole output extent `[out, out + mip_chain_bytes)` (not the capacity) against the source rows.
This check comes before `BufferTooSmall`.

## Mip chain

- `mip_count(w, h) = 1 + floor(log2(max(w, h)))`, 0 for a zero or oversized size.
- Level 0 has the source size. Level `n + 1` has size `max(1, w >> 1) x max(1, h >> 1)`.
- `mip_level_size` writes the size and returns true, or returns false (outputs untouched) for a level out of range or a bad size.
  A null output pointer is skipped.
- `mip_chain_bytes` is the sum of `w * h * 4` over the levels, 0 on a bad size. The largest value is for 8192 x 8192
  (14 levels), far below 2^64.
- Layout: all levels are tightly packed in one buffer, level 0 first. Level `n` starts at the sum of the byte sizes of the levels
  before it and has stride `width_n * 4`. Level 0 is a literal copy of the source rows. Each next level is the Box resize of the
  previous tight level.
- `build_mip_chain` checks `NullBuffer` (source data or `out` null), `BadSize`, `Overlap`, then `BufferTooSmall`
  (`capacity < mip_chain_bytes`). Nothing is written on error and no byte beyond the chain is written.

## Digest

`digest_surface` is FNV-1a 64 (offset basis 14695981039346656037, prime 1099511628211) over `width * 4` bytes of each row, top to
bottom. Stride padding is excluded. A null pointer or a zero width or height returns the offset basis.

## Verification

The expected values in `tests/resample/resample_tests.cpp` come from an independent Python reference (big integers, floor division,
naive loops) that shares no code with the implementation. Hand-computed tests cover a 2 x 1 to 4 x 1 resize for every filter,
1 x 1 and 1 x N shapes, exact halving, constant surfaces, identity, the alpha clamp, the overshoot clamp, stride padding,
status order and overlap layouts. The weights are checked over all 65536 fractions.

The golden surfaces use the pixel formula `A = (31 x + 17 y + 3) mod 256`, `R = (5 x + 3 y) mod (A + 1)`,
`G = (11 x + 7 y + 1) mod (A + 1)`, `B = (2 x + 19 y + 9) mod (A + 1)`. Digests of the resized results (tight stride):

| filter | source | destination | digest |
| --- | --- | --- | --- |
| Nearest | 64 x 48 | 40 x 30 | 0x076fc668d941abf6 |
| Bilinear | 64 x 48 | 40 x 30 | 0xb0eb7bbed01b89be |
| Box | 64 x 48 | 40 x 30 | 0x069cab8b3614ab40 |
| CatmullRom | 64 x 48 | 40 x 30 | 0xbfd8bfbbb9847ce0 |
| Nearest | 64 x 48 | 100 x 75 | 0xd2b1d218ed2db792 |
| Bilinear | 64 x 48 | 100 x 75 | 0xdb96fad49af6deb2 |
| Box | 64 x 48 | 100 x 75 | 0x1cc6151507f7a3bc |
| CatmullRom | 64 x 48 | 100 x 75 | 0xb6f1c536a39fa165 |
| Nearest | 33 x 17 | 16 x 8 | 0xd79e1b89942a2f01 |
| Bilinear | 33 x 17 | 16 x 8 | 0x7bd39156e39644e7 |
| Box | 33 x 17 | 16 x 8 | 0xaa06fb312a9d4437 |
| CatmullRom | 33 x 17 | 16 x 8 | 0xd4b83421f7661062 |
| Nearest | 1 x 40 | 1 x 13 | 0x987b1d48a2ab9061 |
| Bilinear | 1 x 40 | 1 x 13 | 0xcfaa114f075d9771 |
| Box | 1 x 40 | 1 x 13 | 0x9bb1579e5e408f45 |
| CatmullRom | 1 x 40 | 1 x 13 | 0x742f15b421e8e2ad |
| Nearest | 40 x 1 | 13 x 1 | 0x495025b54b1d4e9c |
| Bilinear | 40 x 1 | 13 x 1 | 0x84d488eb3d99c925 |
| Box | 40 x 1 | 13 x 1 | 0xc388541633016131 |
| CatmullRom | 40 x 1 | 13 x 1 | 0x84b487fb6f4ec0fe |
| Nearest | 97 x 61 | 50 x 50 | 0x10d92c12c57b47f0 |
| Bilinear | 97 x 61 | 50 x 50 | 0xe89f5e829eca738b |
| Box | 97 x 61 | 50 x 50 | 0xeb680cfc9fde3911 |
| CatmullRom | 97 x 61 | 50 x 50 | 0x8cf21f22ad291fbe |
| Nearest | 50 x 50 | 97 x 61 | 0x7142c19273672f62 |
| Bilinear | 50 x 50 | 97 x 61 | 0xdaa7e2df5c81e2de |
| Box | 50 x 50 | 97 x 61 | 0x8e3348c651a23e33 |
| CatmullRom | 50 x 50 | 97 x 61 | 0xc5900385722b2254 |
| Nearest | 31 x 31 | 31 x 31 | 0x4df40e2a7759174f |
| Bilinear | 31 x 31 | 31 x 31 | 0x4df40e2a7759174f |
| Box | 31 x 31 | 31 x 31 | 0x4df40e2a7759174f |
| CatmullRom | 31 x 31 | 31 x 31 | 0x4df40e2a7759174f |
| Nearest | 200 x 150 | 77 x 59 | 0xb909203e7f469aa7 |
| Bilinear | 200 x 150 | 77 x 59 | 0x76590807d98eb7af |
| Box | 200 x 150 | 77 x 59 | 0x13ff8035dbb6b718 |
| CatmullRom | 200 x 150 | 77 x 59 | 0x31878879b2b15f8e |
| Nearest | 77 x 59 | 200 x 150 | 0xf46412faa9a25627 |
| Bilinear | 77 x 59 | 200 x 150 | 0xbed424245cbfda11 |
| Box | 77 x 59 | 200 x 150 | 0xcb2cf98110023695 |
| CatmullRom | 77 x 59 | 200 x 150 | 0x97b8efa4e7f24f3f |

Mip chains of the same pattern (all levels in one buffer):

| source | levels | bytes | digest |
| --- | --- | --- | --- |
| 1 x 1 | 1 | 4 | 0x447f627f98e8fa26 |
| 2 x 1 | 2 | 12 | 0xfaef716ac589430f |
| 1 x 2 | 2 | 12 | 0x98ddf8ec2b21baa3 |
| 3 x 5 | 3 | 72 | 0x10eca0985ed1a166 |
| 8 x 8 | 4 | 340 | 0x11c155229eec5703 |
| 7 x 13 | 4 | 452 | 0xe10380a2bf29e82a |
| 64 x 17 | 7 | 5724 | 0x8d6fe872140e8a6e |
| 100 x 1 | 7 | 788 | 0x625eb2203bb72441 |

The 200 x 150 chain digest is 0x61423edc268c18ce.
