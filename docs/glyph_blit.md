# Glyph blit

`core/glyphblit` copies an 8-bit coverage bitmap (a rasterized glyph) into an 8-bit atlas buffer at a position chosen by
`core/shelfpack`, clears the padding cells that the packer keeps free, and hashes an image. It does not draw, blend or convert pixels.
The rules are in the comments of `core/glyphblit/glyph_blit.h`; this page adds the bounds, the decisions, the tests and the mutation
results.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no floating point and no
128-bit type. Evidence label: local unit tests only.

## What a blit writes

- The w x h block takes the source pixels row by row (each row starts at `src + r * src_stride`).
- If `x + w < dst_w`, the cell right of each of the h rows (column `x + w`) is set to 0.
- If `y + h < dst_h`, the row below (row `y + h`) is cleared over columns `x` to `x + wp - 1`, where `wp = min(w + 1, dst_w - x)`. The
  corner cell is part of that row when there is a right cell.
- Nothing else is written, not even the stride padding of the atlas rows.

## Checks, in order

Null pointer, then size (w, h, dst_w or dst_h equal to 0 or above 4096), then stride (below the width, or above 65536), then bounds
(`x + w > dst_w` or `y + h > dst_h`, computed in 64 bits, so `x = UINT32_MAX` is OutOfBounds and never wraps), then overlap. A failed
call writes nothing. `x` and `y` have no range check of their own.

## Overlap

The source span is the byte range `[src, src + (h - 1) * src_stride + w)`. The destination span is
`[dst + y * dst_stride + x, dst + (y + hp - 1) * dst_stride + x + wp)` with `hp = min(h + 1, dst_h - y)`. Both are compared as
integer addresses in 64 bits; no pointer outside a buffer is formed. Spans that only touch are allowed. Any intersection is Overlap,
even when the rows would not collide, because the destination span is a bounding range. At the right or bottom atlas edge `wp` or `hp`
shrinks, so the span ends earlier and a source that starts right after it is allowed.

## Image digest

`image_digest` is FNV-1a 64 over `w` and `h` as 32-bit little-endian words, then the w pixel bytes of each row from the top. Stride
padding is never hashed. A null pointer, a zero or above-4096 size, or a stride below the width or above 65536 gives the offset basis.
It reads only up to `(h - 1) * stride + w` bytes.

## Tests

`tools/glyph_blit_ref.py` is an independent model on a bytearray with abstract addresses and its own FNV. It prints the constants pasted
into `tests/glyphblit/glyph_blit_tests.cpp`: hand-shaped and random blit cases on separate exact-size buffers, 200 cases on one shared
buffer (touching, one byte of overlap, before, after and inside the atlas buffer), and image digest cases. Each case checks the status
and the digest of every byte of the final buffers, so a stray write is caught. All buffers are exact-size vectors, so ASan reports any
read or write outside them.

Other tests: a pixel oracle for the exact set of written bytes (block, right cell, row below with the corner, everything else
unchanged, atlas stride padding untouched), a hand-computed 3 x 2 blit, the check order and each failure code, and hand-computed
overlap probes at the edges (touching, one shared byte, shrunken `wp` and `hp`).

## Mutation testing

Evidence label: local only. 92 real source mutants of `core/glyphblit/glyph_blit.cpp` were built and run against the test file, each one
a single edit. They cover: each null check; the size check for each of the four dimensions (zero, the limit, `<=` against `<`); the
stride checks and limits; the bounds checks (32-bit sums, `>=` against `>`, dropped checks); `wp` and `hp` (no padding, no shrinking);
each term of the source and destination spans (row count, width, `x`, `y`, the stride used, padding); the overlap comparison (touching
at the front or back, `||` for `&&`, one side only, never); the copy (strides, offsets, a skipped row or column); the right cell and the
row below (`<=`, never, always, wrong row, wrong offset, wrong width, wrong value); and the digest (basis, prime, byte order, byte count,
width and height words and their order, stride, padding, the invalid argument checks).

89 of the 92 were killed. Three survive, and each is equivalent:

- `w + 1 < avail_w` changed to `w + 1 <= avail_w` in `wp`. The two differ only when `w + 1 == avail_w`, and then both branches give
  `avail_w`. The same argument covers the matching change to `hp`, the other survivor of this kind.
- The source span end computed in 32 bits instead of 64. The term `(h - 1) * src_stride + w` is at most `4095 * 65536 + 4096`, below
  2^32, because sizes and strides are limited before the overlap check is reached. The 32-bit value equals the 64-bit value for every
  call that gets there, so no input can tell them apart.
