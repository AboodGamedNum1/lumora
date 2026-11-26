# Box filter

`core/boxfilter` downscales an 8-bit coverage image by a whole factor from 1 to 16: smaller glyph bitmaps and mip levels of masks. The
rules are in the comments of `core/boxfilter/box_filter.h`; this page adds the decisions, the bounds, the tests and the mutation results.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no floating point and no 128-bit
type. Evidence label: local unit tests only.

## What it computes

- The output size is `out_w = w / f` and `out_h = h / f` (integer division). Source columns and rows beyond `out_w * f` and `out_h * f`
  are ignored.
- Each output pixel is `(S + n / 2) / n` with `n = f * f` and `S` the sum of its n source pixels. Half rounds up, a constant block gives
  that constant, and `f = 1` copies the image. `S` is at most 255 * 256 = 65280, so a 32-bit sum is enough.
- Exactly `out_w` bytes of each of the `out_h` destination rows are written. The stride padding is not touched.

## Checks, in order

Null pointer (source, destination, `out_w` or `out_h`); size (w or h equal to 0 or above 4096); source stride (below w or above 65536);
factor (0, above 16, or larger than w or h); destination stride (below `out_w` or above 65536); overlap. A failed call writes nothing and
leaves `*out_w` and `*out_h` as they were. The destination stride is checked after the factor because `out_w` depends on it.

## Overlap

The source span is `[src, src + (out_h * f - 1) * src_stride + out_w * f)`: only the rows and columns that are read. The destination span
is `[dst, dst + (out_h - 1) * dst_stride + out_w)`. Both are compared as integer addresses in 64 bits. Spans that only touch are
allowed; any intersection is Overlap. The ignored source tail (extra columns or rows) is not part of the span, so a destination placed in
that tail is allowed.

## Bounds

The work is `out_w * out_h * f * f` byte reads, at most the source size. The largest value of every product is below 2^32 once the size
and stride limits hold (`4095 * 65536 + 4096`), and spans are computed in 64 bits.

## Tests

`tools/box_filter_ref.py` is an independent model on a bytearray with abstract addresses and its own FNV, with per-pixel sums written as
loops. It prints the constants pasted into `tests/boxfilter/box_filter_tests.cpp`: hand-shaped and random cases on separate exact-size
buffers (valid and invalid arguments) and 220 cases on one shared buffer (touching, one byte of overlap, destination before or after the
source, destination in the ignored source tail). Every case checks the status, the reported size and the digest of every byte of the final
buffers, so a stray write is caught. All buffers are exact-size vectors, so ASan reports any read or write outside them.

Other tests: a pixel oracle over every factor from 1 to 16 that uses a different rounding formula, `(2 S + n) / (2 n)`; hand-computed
results (a ramp, half rounding with block sums 1 to 6, an ignored column and row, the largest factor with 127, 128 and 256 set
pixels, the size limits); the check order with sentinels on the output sizes; and hand-computed overlap probes at the span edges.

## Mutation testing

Evidence label: local only. 75 real source mutants of `core/boxfilter/box_filter.cpp` were built and run against the test file, each
one a single edit. They cover: each null check; the size checks (zero, the limit, `>=` against `>`); the stride checks and their limits;
the factor checks (zero, the maximum, larger than w or h) and the order of the checks; the output size (swapped dimensions, rounding
up); every term of the source and destination spans (rows, columns, strides used, 32-bit arithmetic, padding); the overlap comparison
(touching at either end, `||`, one side only, never, wrong code); the divisor, the rounding term and the sum (a skipped row or column,
the wrong stride or offset); the row and column loops; and the writes of `*out_w` and `*out_h` (missing, swapped, wrong value).

73 of the 75 were killed. Two survive, and each is equivalent:

- The source span end computed in 32 bits instead of 64. The term `(out_h * f - 1) * src_stride` is at most `4095 * 65536`, and the
  full end offset below `4095 * 65536 + 4096`, both under 2^32, because sizes and strides are limited before the overlap check is
  reached. The 32-bit value equals the 64-bit value for every call that gets there.
- Writing `*out_w` and `*out_h` before the filtering loops instead of after. No failure path exists after the overlap check, so a call
  that reaches the loops always returns Ok and ends with the same two values written. The order is unobservable only on the condition
  that `out_w` and `out_h` do not alias `src` or `dst` (their bytes), because the loops read `src` and write `dst`. The header makes no
  aliasing promise, no check exists and no test covers it; callers pass distinct objects. If an output pointer did alias a source or
  destination byte, the mutant could change the pixels read or written, and this survivor would no longer be equivalent.
