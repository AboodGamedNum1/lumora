# TrueType reader

`core/ttf` reads a TrueType font that the caller holds in memory and turns glyphs into `core/outline` commands. It parses and validates
the structure (`parse_font`), maps a BMP code point to a glyph (`glyph_index`), reads the advance width (`glyph_advance`) and converts one
glyph into move, line, quad and close commands in 24.8 pixels (`glyph_outline`). The rules for every function are in the comments of
`core/ttf/ttf.h`; this page adds the bounds, the overlap rule, the decisions and the tests.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state and no floating point. The parsed
`Font` only points into the caller's bytes. Evidence label: software (CPU) only.

## Bounds

- A font has at most `2^26` bytes and at most 64 tables. All offset and length sums use 64-bit arithmetic, so a record such as
  `offset = 0xFFFFFFFF, length = 0xFFFFFFFF` cannot wrap and is rejected as outside the file.
- A glyph has at most 340 points after composite expansion and at most 16 components.
- Commands: each point yields at most one Line or Quad, and each contour adds one Move, one Close and, for a contour whose start is an
  implied midpoint, one closing Quad that replaces no point. A contour needs at least 2 points to emit anything, so the number of
  contours is at most half the points. This gives `commands <= points + 2 * contours <= 2 * points <= 680`, which is below
  `kMaxGlyphCommands = 1020 = 3 * 340`. The 1020 bound is the one the interface asks for; the real maximum is 680. The push guard in the
  emitter therefore never fires on valid input; it is kept as a defence and cannot be reached by a test.
- The point staging and contour end arrays are sized by the 340 point bound, and the point count is checked against it before any
  point is stored, so no write can leave the arrays.
- Coordinates are signed 32-bit sums in doubled font units: at most 340 deltas of 16 bits (plus one component offset of 16 bits) is
  below `2^26`, far inside the 32-bit range. Scaling uses exact 64-bit products: `|d| < 2^27`, size at most `2^22`, so the product is
  below `2^49`. The result is checked against `+-2^24` after adding the origin (at most `2^23`), also in 64 bits.
- `glyph_outline` never fails after it started writing in a way that leaves partial data visible: the count and the commands are written
  only after the whole glyph was converted without error.

## Check order

`glyph_outline` returns the first failure of: `NullBuffer` (font data, out or out_count null), `BadSize` (size or origin out of range),
`BadGlyph` (glyph number at or above the glyph count), `BufferTooSmall` (capacity below 1020), `Overlap` (three pairs), then the data
rules: loca range, glyph data and the outline rules, and `OutOfRange` while scaling. Overlap is decided before any data dependent error.

The three overlap pairs are `out` against `out_count`, `out` against the font bytes, and `out_count` (4 bytes) against the font bytes.
The extent of `out` is always `1020 * sizeof(Command)`, whatever the capacity is.

Overlap uses the same modular address rule as the other modules: clear the top byte of both addresses, take the unsigned difference
modulo `2^64`, answer "no overlap" at `|delta| >= 2^47` before any other arithmetic, then compare the exact ranges. Real buffers cannot
reach the `2^47` branch, so it is argued here and not tested.

## Decisions

All of these were open readings of the header and were confirmed before coding.

- `parse_font`: the seven presence checks run first (head, maxp, loca, glyf, cmap, hhea, hmtx), then the content checks in the order head,
  maxp, loca, hhea with the hmtx length, cmap. glyf has no content check.
- `glyph_index` and `glyph_advance`: `NullBuffer` first, no overlap check, nothing written on an error. A stored glyph value of 0 stays 0;
  any other stored value gets the delta added modulo 65536.
- loca: only entries g and g + 1 are read; `loca[g] <= loca[g+1] <= glyf_len`, also for an empty range. A non-empty range below 10 bytes is
  `BadGlyph`.
- Composite components, per component in this order: read flags, glyph and arguments (`BadGlyph` if short); flag checks (`Unsupported`);
  glyph lookup (`BadGlyph`, or `Unsupported` for a composite); the point count from its endPtsOfContours; add to the total and give
  `TooComplex` at once when it exceeds 340; then read its data. The 0x0020 flag on the 16th component gives `TooComplex` only after that
  component is fully processed.
- Flag 0x0100 counts on the last component only. Instruction length and instructions must fit.
- Contour walk: first point on-curve, else last point on-curve, else the exact midpoint in doubled units.
- Coordinates are signed 32-bit sums in doubled units; offsets are signed 16-bit words or signed bytes.

## Tests

`tests/ttf/ttf_tests.cpp` builds every test font in code. A small builder writes the tables, the directory, loca, cmap format 4 and
glyph data. The expected values come from an independent Python reference with its own builder, parser, cmap lookup, advance lookup and
outline converter, all in exact integers; its font digests are compared with the C++ builder, so the two builders agree byte for byte
before any result is compared. Results are checked against constant golden digests and against hand-computed values. The cases are:

- structure: table counts and sizes at both sides of every limit, missing tables, each content rule, the check order, and cmap record
  rules; the directory ending exactly at the end of the file, with an exact-size allocation so the address sanitizer sees an over-read;
- `glyph_index`: a reference table over all code points, segment corner cases, `idRangeOffset` edges and a stored value with a delta that
  wraps;
- `glyph_advance` for glyphs inside and beyond the long metric part;
- `glyph_outline`: reference rows, contour walk variants, the command bound, argument order, the three overlap pairs including the
  one-byte adjacent cases, overlap before data errors, and the simple and composite rules with their limits;
- loca rules, and that a glyph never reads the next glyph's bytes;
- sweeps: every prefix of a font (exact allocation), single byte flips, every byte value in glyph data, glyph cuts, and an exhaustive set
  of small glyphs against the reference.

A mutation run over the module (161 mutants of checks, limits, sizes, order and arithmetic) was caught except these, which cannot change
any result: the overlap `2^47` cutoff and the top byte mask (argued above); the emitter push guard (maximum 680 commands is below it);
fit checks that a later, stricter check already covers (instruction length, endPtsOfContours, component header, cmap length fields, and
record fit); and a count expression that only differs above 1000 commands, which is above the 680 maximum.
