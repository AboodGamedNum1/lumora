# Composite module

The composite module (`core/composite/composite.h`, `core/composite/composite.cpp`) is the CPU reference for blend modes, mask
fills and box blur on RGBA8 surfaces with premultiplied alpha. It uses integer arithmetic only, no floating point and no
libm, and never lets a pointer value influence an output byte. Results are identical on every platform. Evidence label:
software (CPU) only.

## Pixels and limits

- A pixel is 4 bytes R, G, B, A, premultiplied. A valid pixel has R, G, B <= A. No function checks this on input; every
  function gives defined output for any bytes and never reads or writes out of range.
- A surface is at most 8192 x 8192 (`kMaxSize`) with `stride >= width * 4` bytes. A mask is one byte per pixel with
  `stride >= width`. The radius is at most 255 (`kMaxRadius`). The stride padding is never written.

## div255

`div255(v) = (v + 128 + ((v + 128) >> 8)) >> 8`, evaluated in 64 bits, so it is defined for every `uint32` input. It is the
correctly rounded `v / 255` (nearest, half up) for `0 <= v <= 65025`, which covers every product of two bytes. This was
checked for all 65026 values against `(2v + 255) / 510`. Above 65025 the formula is not the nearest integer (for example
`div255(65663) = 257`, the nearest is 258). The module never calls it above 65025.

## Conversions

- `premultiply`: `c' = div255(c * a)` for R, G, B; A is copied.
- `unpremultiply`: `a == 0` gives `0,0,0,0`; otherwise `c' = min(255, (c * 255 + a / 2) / a)`. The `min` also covers
  inputs with `c > a`. A is copied.
- `in == out` is allowed (all four input bytes are read first). A null `in` or `out` does nothing.
- Round trip: for a valid pixel (`c <= a`, `a >= 1`) the error of `unpremultiply(premultiply(c))` is at most
  `ceil(255 / (2a))`, and opaque pixels round trip exactly.

## Blend formulas

`blend(dst, src, dx, dy, mode, opacity)` places the top-left corner of `src` at `(dx, dy)` in `dst` coordinates. The overlap
is clipped exactly (computed in 64 bits, so any `int32` offset is fine). A placement with nothing inside `dst` is `Ok` and
writes nothing.

1. The source is scaled first: `s = div255(src * opacity)` for each channel including alpha. Opacity 255 changes nothing.
2. With `S` the scaled source channel, `D` the destination channel, `Sa` the scaled source alpha and `Da` the destination
   alpha, every product of two byte values gets exactly one `div255`. The terms are then summed and clamped to 0..255:

| Mode | Result for one channel |
| --- | --- |
| Src | `S` |
| SrcOver | `S + div255(D * (255 - Sa))` |
| DstOver | `D + div255(S * (255 - Da))` |
| SrcIn | `div255(S * Da)` |
| DstIn | `div255(D * Sa)` |
| SrcOut | `div255(S * (255 - Da))` |
| DstOut | `div255(D * (255 - Sa))` |
| SrcAtop | `div255(S * Da) + div255(D * (255 - Sa))` |
| DstAtop | `div255(D * Sa) + div255(S * (255 - Da))` |
| Xor | `div255(S * (255 - Da)) + div255(D * (255 - Sa))` |
| Plus | `S + D`, clamped to 255 |
| Multiply | `div255(S * (255 - Da)) + div255(D * (255 - Sa)) + div255(S * D)` |
| Screen | `S + D - div255(S * D)` (never below 0, clamped to 255) |

3. The alpha channel uses the same formula as the colour channels with `S = Sa` and `D = Da`, products rounded
   separately (so, for example, SrcAtop alpha is `div255(Sa * Da) + div255(Da * (255 - Sa))`, which can differ from `Da` by
   one). Multiply and Screen use the SrcOver rule for alpha: `Sa + div255(Da * (255 - Sa))`, clamped.
4. Last step: every colour channel is clamped to the result alpha. The output therefore always satisfies R, G, B <= A, even
   when the inputs do not.

Malformed inputs (R, G, B > A, for example a pixel with alpha 0 and nonzero colour) are used as supplied in the formulas and
then pass through the final clamp. Nothing divides by a data-dependent value in blend.

### Every mode at Sa = 0

With `Sa = 0` (and so `S` possibly nonzero only for a malformed pixel) the formulas reduce, for `S = 0`, to:

| Mode | Result with S = 0 | Effect on dst |
| --- | --- | --- |
| Src, SrcIn, SrcOut, DstIn, DstAtop | 0 | cleared to `0,0,0,0` |
| SrcOver, DstOver, DstOut, SrcAtop, Xor, Plus, Multiply, Screen | `D` | unchanged |

For DstOver, Plus and Screen the result is `D` because every term with `S` is 0; the other unchanged modes keep `D` through
`div255(D * 255) = D`.

## fill_mask

`fill_mask(dst, mask, dx, dy, color, mode)` scales the colour by the mask byte `m` per pixel: `c' = div255(c * m)` for all four
channels, then applies the mode formulas above with `S = c'` (and the final clamp to alpha). The mask is placed at `(dx, dy)`
and clipped exactly; pixels outside the mask are untouched. A mask byte 0 gives `S = 0`, so the table above applies:
Src, SrcIn, SrcOut, DstIn and DstAtop clear the destination, the other modes leave it unchanged.

## Status checks

Checks run in this order and the first failure is returned. On any error nothing is written.

1. `NullBuffer`: a data pointer is null (`dst.data`, `src.data`; for `fill_mask` also `mask.data` and `color`).
2. `BadSize`: width or height 0 or above 8192, or a stride shorter than a row (`width * 4` for surfaces, `width` for masks),
   for any of the surfaces involved, whether or not they are clipped away.
3. `BadMode`: a `BlendMode` value outside the enum.
4. `Overlap` (blend and fill_mask only).

`box_blur` and `blur3` check `NullBuffer` (surface data only), `BadSize`, then `BadRadius` (either radius above 255).

### Overlap

Overlap is decided on the bytes actually touched, after clipping. Let `w` and `h` be the size of the clipped region. Row `i`
of the touched dst bytes is `[dst0 + i * dst_stride, +w * 4)`. Row `j` of the touched source bytes is
`[src0 + j * src_stride, +w * 4)` (for a mask `+w`). `Overlap` is returned when any touched dst row range intersects any
touched source row range. Regions that sit side by side in one buffer, including rows that interleave in memory, are valid.
A placement that is clipped away completely touches nothing and is `Ok`, even when the buffers alias. Addresses are only
compared as integers for this status and never affect an output byte; addresses are assumed to fit 63 bits. The check is
exact and takes O(rows) time.

## Box blur

`box_blur(s, rx, ry, scratch)` blurs all four channels independently, horizontal pass first, then vertical. Each pass uses a
window of `2r + 1` samples with edge clamping (the first and last sample repeat outside the image), and the output sample is
`(sum + r) / (2r + 1)` (that is `(sum + window / 2) / window` with integer division). The result of the horizontal pass is
stored as 8-bit values before the vertical pass starts. Radius 0 in an axis skips that axis. A window larger than the image
is valid; the clamp makes it read the edge samples repeatedly. `blur3` applies `box_blur` three times with the same radii,
so the order is horizontal, vertical, horizontal, vertical, horizontal, vertical, with 8-bit rounding after every pass.

Memory: nothing is allocated and `scratch` is reserved (never read or written, null is valid). The horizontal pass works in
place row by row through a local array of at most 8192 * 4 bytes, the vertical pass in place column by column through a
local array of at most 8192 * 4 bytes (a column of up to 8192 pixels, 4 channels). A sliding window sum keeps the work
linear in the number of samples. The output equals the naive definition above; the tests compare against a naive
window-sum implementation on random images and radii.

## digest_surface

FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over `width * 4` bytes of every row, top to bottom, padding
excluded. A surface with null data or a zero width or height hashes zero bytes (the offset basis).

## Bounds

All intermediate values fit `uint32` (the code uses 64 bits only in `div255` and for coordinates).

- Opacity or mask scaling: `c * m <= 255 * 255 = 65025`. Every product in a blend formula is at most 65025.
- Sum of up to three products: at most 195075. `S + D` at most 510. All results are clamped before narrowing to a byte.
- Clipping works in `int64`: `dx + width` stays within `2^31 + 2^13`.
- Blur window sum: `(2 * 255 + 1) * 255 = 130305`, and the sliding update `sum + add - sub` never goes negative or above
  that value plus 255.
- Overlap address arithmetic: strides are below `2^32` and there are at most 8192 rows, so offsets stay below `2^46` in `int64`.

## Golden digests

The constants are in `tests/composite/composite_tests.cpp`; they come from an independent Python reference (true-rounding
division, naive blur windows, a prefix-sum blur for the large surfaces cross-checked against the naive one). `digest_surface`
of the result:

| Case | Result |
| --- | --- |
| mix_Multiply (6 x 5 destination, 4 x 4 source at (-1, 2), opacity 200) | `a515d633dfb94b3c` |
| mix_Screen (same inputs) | `646fe1a9b83f09c4` |
| mix_SrcOver (same inputs) | `bfe468d1b75354cd` |
| malformed_Xor (inputs with R, G, B > A) | `8bf127b202d85210` |
| src_left (3 x 3 source clipped at the left edge) | `f466bb07c1c5f65d` |
| rand_9x4_r1_3 (box blur, radius 1 and 3) | `6009bcd7ad019d79` |
| beyond_4x2_r255 (window far larger than the image) | `397b7eee0a7091db` |
| rand_6x7_blur3 (radius 2 and 1) | `465883619fda4621` |
| impulse_blur3_r1 (9 x 9 impulse) | `4514aa4076eee18e` |
| Multiply blend, 1024 x 1024 pattern | `3c64e400d2e3d6dd` |
| Screen fill_mask, 1024 x 1024 pattern | `bf07e27c8e7d7def` |
| box blur 1024 x 768, radius 9 and 5 | `9e7f928727bc3f3a` |
| blur3 1024 x 768, radius 4 and 3 | `c797716cfaf56f06` |
| box blur 8192 x 4, radius 255 and 1 | `18b3882c19683d94` |
| box blur 4 x 8192, radius 1 and 255 | `b75b97c60417eb0a` |

## Tests

The tests check every mode on opaque, transparent and half-alpha pixels with hand-computed examples, every mode at
`Sa = 0`, malformed pixels, exhaustive `div255` for 0..65025, exhaustive premultiply and unpremultiply with the round-trip bound,
clipping on all four sides, at corners and outside (including `int32` extremes), every status and the check order, untouched
stride padding, masked fills with masks 0 and 255 for each mode, the overlap rules, impulse and constant images, radius 0, 1,
beyond the image and 255, non-square surfaces, sliding-window blur against a naive implementation on random images, and the
digests above. The large cases use heap buffers and no timing bound.

## Evidence limits

The reference and the C++ were compared on thousands of random cases on one Linux compiler. Results on other compilers and
platforms are checked by CI, not claimed here.
