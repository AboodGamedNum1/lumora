// Greedy line breaking over a run of glyph advances (the 24.8 pen advances that core/textrun produces). It decides only where lines
// start; it does not measure, shape, hyphenate or reorder. Integer only: no floating point, no libm, no 128-bit types (MSVC), no
// allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::linebreak {

constexpr std::uint32_t kMaxItems = 1u << 16;

enum class ItemKind : std::uint8_t {
  Glyph = 0,   // no break opportunity after it
  Space = 1,   // a line may break after it; at the end of a line its advance hangs (is not counted)
  Newline = 2  // forces a break after it; its advance is not counted
};

struct Item {
  std::int32_t advance = 0;  // 24.8 fixed point, 0 or more
  std::uint8_t kind = 0;     // an ItemKind value
};

enum class BreakResult : std::uint8_t { Ok = 0, NullArgument = 1, BadWidth = 2, TooManyItems = 3, BadItem = 4, OutputFull = 5 };

// Fills line_starts with the index of the first item of every line, in order, and *line_count with the number of lines. Line i covers the
// items from line_starts[i] up to (not including) line_starts[i + 1], the last line up to count. Lines are never empty. count 0 gives
// 0 lines and Ok (the pointers must still be non null). Check order, each failure writes nothing and leaves *line_count as it was:
// items, line_starts or line_count null gives NullArgument; max_width <= 0 gives BadWidth; count > kMaxItems gives TooManyItems; any item
// with advance < 0 or kind > 2 gives BadItem; then OutputFull when the lines do not fit in capacity (so the line count is found before
// anything is written).
//
// Rule, with all sums in 64 bits. A line starts at s. Items are added one at a time; item j is added by these rules:
//  - kind Space: always added (it hangs when it ends the line).
//  - kind Newline: added, and the line ends after j.
//  - kind Glyph: let used = the sum of the advances of the items s..j-1 (all of them, spaces included). If used + advance[j] <= max_width
//    the glyph is added. Otherwise the line must wrap before j: if the line s..j-1 holds a Space, the line ends after the LAST Space at
//    index b (b >= s) and the next line starts at b + 1 (items after b are re-examined on the new line); else if j > s the line ends
//    after j - 1 and the next line starts at j (an overlong word is cut at a glyph boundary); else (j == s, a single glyph wider than
//    max_width) the glyph is added alone and the line ends after j.
// The last line ends at count - 1. A line that ends at a Space or Newline keeps that item as its last item, and a following line may
// start with another Space (no trimming).
// Decisions: a final Newline ends the last non-empty line and no trailing empty line is emitted (so "ab\n" is one line, and a run of only
// Newline items gives one line per Newline). Every failure, OutputFull included, leaves line_starts and *line_count untouched (count the
// lines first, then write). Capacity 0 succeeds only for count 0; otherwise it is OutputFull. Space advances are added with no width check
// and count toward `used` of later Glyph items; a Glyph that does not fit may wrap after the latest Space of the current line. A Newline's
// advance is never counted. After a wrap the items after the break index are re-examined from the new line start with a fresh `used`.
BreakResult break_lines(const Item* items, std::uint32_t count, std::int32_t max_width, std::uint32_t* line_starts,
                        std::uint32_t capacity, std::uint32_t* line_count);

}  // namespace lumora::linebreak
