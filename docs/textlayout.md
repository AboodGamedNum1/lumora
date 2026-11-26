# Bitmap text layout

`core/textlayout/textlayout.h` and `core/textlayout/textlayout.cpp` turn a UTF-8 string and a bitmap font description into
positioned glyph quads: `validate_font`, `layout_text`, `digest_quads` and the status texts. Pure CPU, no I/O, no threads.
The interface is in `textlayout.h`; this page records every rule and the exact order of operations.

## Determinism

All layout decisions use integers. Positions are `Fixed` values (26.6, 64 per pixel) held in 64-bit variables during layout. There
is no floating point, no libm, no hashing, no pointer values and no unordered container. Glyphs and kerning pairs are found with
binary search in the sorted arrays. The only containers are `std::vector`. Output is identical on Linux, Windows and Android.

## validate_font

Checks run in enum order and stop at the first failure:

1. `BadMetrics`: `line_height <= 0`, `tex_width == 0`, `tex_height == 0`, or any glyph with `advance < 0`.
2. `BadGlyphOrder`: a codepoint that is not a Unicode scalar value (above U+10FFFF or in U+D800..U+DFFF), or codepoints not
   strictly ascending (an equal or smaller codepoint after another).
3. `BadGlyphRect`: `tex_x + tex_w > tex_width` or `tex_y + tex_h > tex_height`, computed without 16-bit wrap. A rectangle that
   touches the edge is valid.
4. `BadKerningOrder`: pairs not strictly ascending by `(first, second)`. The kerning codepoints are not required to be scalar
   values or to exist as glyphs.
5. `NullData`: a non-zero `glyph_count` or `kerning_count` with a null pointer.

An array behind a null pointer is never read, so the checks over it are skipped and `NullData` is reported last, in enum order. A null
pointer with count 0 is valid. `fallback` is not validated.

## layout_text: order of checks

1. `BadFont` if `validate_font` fails.
2. `BadOptions` if `max_width < 0`, `max_glyphs == 0` or `align` is not 0, 1 or 2. This applies to empty text too.
3. A null `utf8` pointer or `length == 0` is empty text: status `Ok`, every total 0.

An error result has all totals 0 and writes nothing.

## Step 1: decode and classify

The string is walked once, byte by byte.

- UTF-8 is strict. Valid sequences follow the standard ranges: C2..DF with one continuation; E0 then A0..BF; E1..EC and EE..EF
  with two continuations; ED then 80..9F; F0 then 90..BF; F1..F3 with three; F4 then 80..8F. Overlong forms, surrogates, values
  above U+10FFFF and truncated sequences are invalid.
- An invalid sequence is its maximal subpart: the lead byte and every following byte that is still valid for that lead, up to the
  first byte that is not. A byte that cannot start a sequence (80..C1, F5..FF) is a subpart of length 1. Each subpart becomes one
  U+FFFD character and adds its length to `invalid_bytes`. Examples: `C0 80` is two characters (2 bytes); `E0 80 80` is three (3
  bytes); `ED A0 80` is three (3 bytes); `F4 90 80 80` is four (4 bytes); `E2 82` at the end of the string, or before a byte that is
  not a continuation, is one character (2 bytes); `F0 9F` before `A` is one character (2 bytes). The count applies even when
  no glyph is drawn for the replacement.
- A replacement character is looked up like any other character (U+FFFD, then the fallback). Its `source_offset` is the offset of
  the first byte of the invalid sequence.
- `'\r'` is dropped. `'\n'` ends a line. `'\t'` is a tab. Any other character below U+0020 (including NUL) is dropped. U+007F and
  U+0080..U+009F are ordinary characters.
- U+0020 is a space: it never draws a quad and never uses the fallback. Its advance is the advance of the space glyph, or 0 if the
  font has none.
- Any other character is looked up by binary search. If the glyph is missing, the fallback glyph is used and the quad's `codepoint`
  is the fallback's. If the fallback is missing too, the character is skipped: no advance, no quad, no effect on kerning.
- Every drawn character produces one quad, including a glyph with a zero sized rectangle.
- `max_glyphs` (prefix semantics): the text is cut right after the `max_glyphs`-th drawn glyph and laid out as if the input ended
  there. Wrapping, alignment, `line_count`, `width` and `height` describe that prefix only. Spaces, tabs, newlines and skipped
  characters are not drawn glyphs. `truncated` is set only when another drawn glyph would follow; a string with exactly
  `max_glyphs` drawn glyphs is not truncated, and then the whole text is kept (so trailing spaces or newlines still count). When it
  is truncated, whatever follows the last kept glyph (spaces, newlines, controls) is dropped and `invalid_bytes` counts only the
  bytes before the cut. A cut is always on a character boundary, so a UTF-8 sequence is never split. The prefix is laid out on
  its own: for `ab cd` at width 1472, `max_glyphs` 3 keeps `ab c` on one line, while the full text breaks before `cd`.

## Step 2: lines

Pen positions are in 64-bit `Fixed`. A line starts at pen 0 with no previous character.

- Kerning: before a glyph or space the pen moves by the kerning amount of `(previous, current)` using the drawn codepoints, if
  there is a previous character on the line. Spaces count as characters. A tab and a line start clear the previous character, so
  there is no kerning across a tab or a newline. Skipped characters do not change it.
- Advance: the pen then moves by the glyph's advance.
- Tab: with `T = 4 * advance(space)` (0 when there is no space glyph), the pen moves to the next multiple of `T` strictly greater
  than the pen (floor division, so a negative pen works). A pen already on a multiple moves a full `T`. With `T == 0` the tab moves
  nothing. A tab draws no quad, counts toward the width, and is part of a word (it is not a break opportunity).
- Line width is the pen after the last character that is not a space (after its advance, without trailing spaces). A line with
  only spaces has width 0. Leading spaces count.
- `'\n'` always starts a new line, so `"a\n"` and `"\n"` have 2 lines. Empty input has 0 lines, any other input at least 1.
- With `max_width == 0` there is no wrapping. Otherwise lines are filled greedily: a character that is not a space is added if the
  pen after its advance is `<= max_width` (an exact fit stays, one unit over breaks). Spaces are never wrapped; they add to the
  pen but not to the width.
- When a character does not fit:
  1. If the line has a space run that comes after some non-space character, the line breaks at the last such space run: the
     line ends before that run and the next line starts at the first character after it. The whole run is dropped (not carried to the
     next line).
  2. Otherwise, if the line already holds a non-space character, the line breaks before the character (the word is split between
     characters).
  3. Otherwise the character is the first non-space character of the line and it is kept even if it is wider than `max_width`.
     This guarantees at least one character per line. Leading spaces before it stay on the line.
- A line that wraps restarts at pen 0 with no previous character, and the characters of the carried word are laid out again from
  there.

## Step 3: alignment and quads

- `block_width` is `max_width` if it is not 0, otherwise the widest line.
- A line is shifted by 0 (Left), `(block_width - line_width) / 2` (Center) or `block_width - line_width` (Right). If the line is wider than
  `block_width` the shift is 0, so division only sees non-negative numbers and is a floor division.
- A quad is `x = pen + offset_x + shift` (pen after kerning, before the advance), `y = line_index * line_height + offset_y`,
  `w = tex_w * 64`, `h = tex_h * 64`. The texture rectangle, `codepoint` and `source_offset` are copied as described above.
- `width` is the widest line (before any shift) and `height` is `line_count * line_height`.

## Output buffer

- A null `out_quads` is count only: status `Ok` and the totals, whatever `capacity` is.
- A non-null `out_quads` with `capacity < quad_count` gives `BufferTooSmall`. The first `capacity` quads are written and the totals
  are the true ones. A capacity equal to the count is `Ok`.

## Input limit and the 64-bit bound

At most 2^30 bytes of input are laid out. A character (valid, or an invalid maximal subpart) that does not end within the first 2^30
bytes is not laid out: the text ends before it, on a character boundary at or below 2^30, and `truncated` is set. This limit is separate
from `max_glyphs`, which counts only drawn glyphs and so does not bound spaces, tabs or skipped characters. Because of it the number of
characters N is at most 2^30 and the number of lines at most 2^30 + 1.

Why every intermediate stays below 2^63 for any int32 font metrics (advance in `0..2^31-1`, offsets, kerning and `line_height` any
int32):

- One glyph or space moves the pen by `advance + kerning`, less than 2^32 in magnitude. A tab moves it to the next multiple of
  `T = 4 * advance(space)`, so by at most `T < 2^33` (`T <= 4 * (2^31 - 1)`). So each character changes a line position by less than
  2^33 and a line position stays below `N * 2^33 <= 2^63 - 2^32` in magnitude. Moving left (negative kerning) is at most 2^31 per
  character.
- `block_width - line_width`: `block_width` is `max_width` (below 2^31) or the widest line, and the two lines together use at most N
  characters, so the difference is below `N * 2^33`.
- `x = pen + offset_x + shift` adds at most 2^31 for the offset and a shift that is below the same bound, so it stays below
  `2^63 - 2`.
- `y = line_index * line_height + offset_y` and `height = line_count * line_height` are below `(2^30 + 1) * 2^31 < 2^62`.
- Byte offsets are below 2^30, so `source_offset` always fits in 32 bits and needs no clamp. `w` and `h` of a quad are at most
  65535 * 64.

## Overflow policy (saturation)

The layout itself, including every wrapping, tab, alignment and truncation decision, runs on the exact 64-bit values. Only four
stored outputs are clamped to `INT32_MIN..INT32_MAX`, at the final store: `Quad.x`, `Quad.y`, `LayoutResult.width` and
`LayoutResult.height`. The status stays `Ok`. Order of operations: decode and classify (no `Fixed` arithmetic), find the lines
(exact pen, exact `<= max_width` test), compute the widest line and `block_width` (exact), compute each line's shift (exact), compute
each quad value in 64 bits, then clamp when storing.

Saturation therefore cannot change which glyphs are drawn, where lines break or any alignment shift. For example, with
`max_width = INT32_MAX` and glyphs of advance 2^30, the second glyph ends at exactly 2^31, which is above `max_width`, so it wraps; a
clamped pen (`INT32_MAX`) would have fitted. The tests check this case and a centred and right aligned case where the exact widest line
(2^31) differs from the clamped reported width.

How the 2^30 byte limit is verified: it is not allocated in CI (one gibibyte). It was checked once locally, unsanitized, with a
`calloc` of 2^30 + 16 bytes holding mostly zero bytes (controls, skipped) and a few characters near the limit and a total length of 2^30 + 8:
a 3-byte character crossing the limit is not laid out (1 quad, `truncated`); a 3-byte character ending exactly at the limit is laid
out (2 quads, `truncated` because bytes follow); a length of exactly 2^30 is not truncated; an invalid two-byte subpart ending exactly at
the limit is counted (`invalid_bytes` 2). That run took about 4.4 s. CI tests cover the rest of the overflow rules with metrics at
the int32 limits.

## Memory and time

Linear in the input: one 32-byte record per kept character and one 24-byte record per line, on the heap. Measured locally with
the sanitizer build, the 1 MiB string test case (over 400,000 quads) takes about 1 s and peaks at about 250 MiB resident.

## Tests

`tests/textlayout/textlayout_tests.cpp` uses a hand-built font. It covers every status and the check order, exact quads for kerning,
wrapping (exact fit, one over, long words, double spaces, spaces only, leading spaces, trailing spaces), tabs, the three alignments, overflow boundaries (x, y, width and height at, one below and one above the int32 limits, INT32_MIN kerning, a huge space advance for tabs, saturation versus wrapping and alignment),
missing glyphs with and without a fallback, a strict UTF-8 corpus with exact `invalid_bytes`, `max_glyphs`, capacity edges, empty
input and golden digests. The expected values come from an independent Python implementation of this page, which also matched the
C++ on 1,500 random inputs. Every digest below is checked in the tests.

## Golden digests

`digest_quads` is FNV-1a 64 over each quad's fields in declaration order, little-endian: `x`, `y`, `w`, `h` as 4 bytes, the four texture
fields as 2 bytes, `codepoint` and `source_offset` as 4 bytes. The empty digest is `0xcbf29ce484222325`.

| Case | Quads | Lines | Invalid bytes | Digest |
| --- | --- | --- | --- | --- |
| `aa bb cc dd ee`, width 1280, left | 10 | 5 | 0 | `0xac76238be54e36ab` |
| `aaaa\nbb\nW`, width 1600, center | 7 | 3 | 0 | `0x3042637fd56127dc` |
| mixed UTF-8 with invalid bytes, no wrap | 6 | 1 | 4 | `0xba455699f4eaed56` |
| random text (seed 2024, 400 pieces), width 1280, center | 304 | 142 | 27 | `0x166264f16bd558a8` |
| random text (seed 7, 600 pieces), no wrap, right | 486 | 21 | 53 | `0x56e7e8100528b5ca` |
| random text (seed 31337, 300 pieces), fallback `b`, width 640 | 222 | 221 | 29 | `0x8678cc319442b56b` |

The random texts come from the linear congruential generator in the tests (multiplier 1664525, increment 1013904223).

## Evidence limits

The results above are from one Linux machine. CI results for Windows and other compilers are reported on the pull request.
