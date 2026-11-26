# Line breaking

`core/linebreak` is a greedy line breaker over a run of glyph advances (the 24.8 pen advances that `core/textrun` produces). It decides
only where lines start. It does not measure, shape, hyphenate or reorder. The rules are in the comments of `core/linebreak/linebreak.h`;
this page adds the decisions, the bounds, the tests and the mutation results.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no floating point and no 128-bit
type. Evidence label: local unit tests only.

## The rule in short

Items are added to a line one at a time. A Space is always added. A Newline is added and ends the line. A Glyph is added when the sum of
the advances already on the line (spaces included) plus its own advance is at most `max_width`. When a Glyph does not fit:

- if the line holds a Space, the line ends after the last Space and the next line starts right after it, so the items after that Space
  are examined again on the new line with a fresh sum;
- otherwise, if the line already has items, the line ends before the Glyph (an overlong word is cut at a glyph boundary);
- otherwise the Glyph is wider than the width on its own: it is added alone and the line ends after it.

A trailing Space hangs: it is not counted against anything on its line. A line that ends at a Space or Newline keeps that item, and the
next line may start with another Space (nothing is trimmed). A final Newline ends the last line and no empty line follows, so a run of
only Newline items gives one line per Newline. Lines are never empty.

## Checks and failures

Order: null pointer, `max_width <= 0`, more than 65536 items, an item with a negative advance or a kind above 2 (all `count` items are
checked), then OutputFull when the lines do not fit in `capacity`. The lines are counted first and written second, so every failure
leaves `line_starts` and `*line_count` untouched. Capacity 0 succeeds only for count 0. All sums are 64-bit, so three advances of
`INT32_MAX` cannot wrap.

## Bounds

Two passes over the items, each one linear: a wrap re-examines only the items after the last Space of the line, and those items hold no
Space, so each of them is re-examined at most once more. The work is at most about four visits per item.

## Tests

`tools/linebreak_ref.py` follows the rule text literally with Python integers, an explicit open line and its own FNV. It prints the
constants pasted into `tests/linebreak/linebreak_tests.cpp`: 33 scenarios with their expected line starts (including the failure
statuses) and five seeded random corpora, each checked by a digest of every status, line count and line start.

Other tests: a structural oracle on random input (lines cover the items contiguously, none is empty, a Newline is only ever the last item
of a line, the glyphs on a line fit unless one is alone at the start, a cut is followed by a glyph), hand-computed lines, the check order
with the no-write rules (sentinels in exact-size buffers under ASan), and the 65536 item limit.

## Mutation testing

Evidence label: local only. 54 real source mutants of `core/linebreak/linebreak.cpp` were built and run against the test file, each one a
single edit. They cover: each null check; the width check and its limit; the item limit and the order of the checks; the item checks for
advance and kind and the range they scan; the capacity check and its position before any write; the writes of the line starts and the
line count; 32-bit sums; the loop bounds; the Space rule (advance counted, last Space remembered, flag, ending the line); the Newline rule
(next line start, advance counted); the fit comparison (`<`, `+ 1`, ignoring the used width or the advance); every wrap target (after the
Space, after the Glyph, cut, lone glyph) and the order of those cases; the default next start; the fresh sum on a new line; and the start
of the scan.

52 of the 54 were killed. Two survive, and each is equivalent:

- `max_width` converted to `u32` instead of `u64`. The value is positive here, since `max_width <= 0` returned BadWidth earlier, and a
  positive `int32` has the same value as `u32` and `u64`.
- `used += 1` added to the Newline branch. The line ends right after it with `break`, and `used` is declared per line, so the stored
  value is never read again. It is a dead store.
