# Text run

`core/textrun` turns a UTF-8 string and a parsed font from `core/ttf` into a list of glyph ids with pen positions: one glyph per code
point, left to right, no kerning, no shaping, no bidi. The rules are in the comments of `core/textrun/textrun.h`; this page adds the
bounds, the overlap rule, the decisions and the tests.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state and no floating point. The
module only composes `ttf::glyph_index` and `ttf::glyph_advance`. Evidence label: local unit tests only.

## Decoding

The decoder follows the sequence table of the header exactly. A code point is read from the longest valid sequence. A stray
continuation byte, an invalid lead byte (C0, C1, F5 to FF), an overlong form, a surrogate, a value above 10FFFF, or a sequence cut
short (by a byte that does not fit or by the end of the text) gives one U+FFFD for the first byte only, and decoding goes on at the
next byte. So n bad bytes give n replacement characters. The replacement and code points above 0xFFFF go through
`ttf::glyph_index` like any other code point; above 0xFFFF it answers glyph 0 and Ok.

The first continuation byte has its own range (A0 to BF after E0, 80 to 9F after ED, 90 to BF after F0, 80 to 8F after F4), the later
ones are 80 to BF. The lead byte mask can be 0x3F shifted by the number of continuation bytes because every valid lead byte already
has the bits above that mask in a fixed pattern, and a continuation byte is masked with 0x3F after it was checked to be 80 to BF.

## Bounds

- At most 4096 glyphs (`kMaxRunGlyphs`). The glyph count is found first, and counting stops at 4097, so the work for an oversize
  text is bounded. A text of more than 4096 code points gives `BufferTooSmall` whatever the capacity is. Bad bytes count one glyph
  each, so 4097 bad bytes are too many.
- The staging area is a local array of 4096 `Placed` values (32 KiB of stack). The output is copied from it only after the last
  glyph and the width were accepted, so an error leaves `out`, `out_count` and `width_fx` as they were.
- The sum of the advances U is at most 4096 * 65535 (below 2^28), the size is at most 2^22, so `2 * U * size + upem` is below 2^51
  and fits unsigned 64-bit arithmetic. No 128-bit type is used.
- Position: `x = (2 * U * size + upem) / (2 * upem)` is the exact round half up of `U * size / upem` for U >= 0, which is the
  "nearest, ties away from zero" of the header for non-negative values. The rounding is applied to the cumulative sum, so the
  per-glyph error never accumulates.
- A pen or the width above 2^30 is `OutOfRange`. A value of exactly 2^30 is accepted, so it fits `std::int32_t`.
- A hand made `Font` view with `units_per_em == 0` is not a parsed font (a parsed font has 16 to 16384). It gives `BadFont` right
  after the `NullBuffer` checks, so nothing divides by zero and nothing is substituted.

## Check order

1. `NullBuffer`: font data, `out`, `out_count` or `width_fx` null, or a null text with a length above 0. An empty text still needs
   `out`, `out_count` and `width_fx`.
2. `BadFont`: `units_per_em` is 0 (hand-built view only), also for an empty text.
3. `BadSize`: size not in 1 to `ttf::kMaxSizeFx`.
4. `BufferTooSmall`: more than 4096 glyphs, or capacity below the glyph count (counted after decoding).
5. `Overlap`.
6. Per glyph in text order: `ttf::glyph_index`, then `ttf::glyph_advance`, then the glyph's own pen against 2^30. After the last
   glyph the width is checked. The first error wins, so a lookup error of an earlier glyph beats an overflow found later.

An empty text goes through steps 1 to 5 and then stores count 0 and width 0.

## Overlap

Six pairs are checked: `out` against `out_count`, against `width_fx` and against the font bytes (`font.data` for `font.size`),
`out_count` against `width_fx` and against the font bytes, and `width_fx` (4 bytes) against the font bytes, so a written width cannot
land in the font. The extent of `out` is always `out_capacity * sizeof(Placed)`, whatever the
glyph count is, and a capacity of 0 is an empty range that overlaps nothing. The text is not checked.

The address rule is the one of `ttf::glyph_outline`: clear the top byte of both addresses, take the unsigned difference modulo `2^64`,
answer "no overlap" at `|delta| >= 2^47` before any other arithmetic, then compare the exact ranges. Real buffers cannot reach the
`2^47` branch or differ in the top byte, so both are argued here and not tested.

## Tests

`tests/textrun/textrun_tests.cpp` builds all four test fonts in code (all glyphs empty, only the format 4 cmap and the metrics
matter): a font with a 1000 unit em and several kinds of cmap segments, a font with a lookup that points outside the subtable, a font
with a 16 unit em that reaches the 2^30 limit with 256 glyphs, and a font with 65535 glyphs where the glyph id is the code point. The
expected values come from `tools/textrun_ref.py`, which has its own font builder, its own UTF-8 decoder written from the header table,
its own format 4 lookup, advance lookup, cumulative rounding and FNV. The cases are:

- the C++ font builder against the reference builder by byte digest;
- 334 reference rows (status, count, width and digest): ASCII, two, three and four byte characters, every kind of bad sequence, cut
  sequences, supplementary code points, bad cmap lookups, tie rounding, the 255 to 300 glyph runs around the limit, and pens and widths that land
  on 2^30 - 1, 2^30 and 2^30 + 1 with sizes that are not powers of two;
- a decoder sweep over all 1 and 2 byte inputs, all 3 byte inputs with a lead byte C0 to FE, and the 4 byte inputs with every second
  byte and eleven boundary values for the others, compared with the reference by one digest, plus a hand list of sequences with their
  code point counts;
- a random corpus (seed 0xA1, 3000 strings);
- hand computed positions, including the 0.5 tie, the 0.75, 1.5 and 2.25 steps, and the replacement glyph;
- null, size, capacity and run bound checks with their order; the 4096, 4097 and bad byte cases;
- overlap for all six pairs with adjacent and one byte cases (the font bytes may start at any address), the capacity extent also above
  4096 values, an empty range, and the order against `BufferTooSmall` and the cmap errors;
- `BadFont` for `units_per_em` 0 with its place in the check order, also with an empty text;
- no write on any error, with exact-size allocations so the address sanitizer sees any overrun.

A mutation run over the module (85 mutants of the decoder ranges, the limits, the rounding, the check order, the overlap pairs and
extents, the null handling, the `BadFont` check and the digest) was caught except these, which cannot change a result: the lead byte `F0` lower bound
(`EF` already belongs to the three byte branch, which is tested first); the continuation mask and the lead masks (the bits that
differ are zero for every checked byte); the overlap `2^47` cutoff and the top byte mask (argued above).
