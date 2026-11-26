// Lumora greedy line breaking (scope AF). Integer only, no heap, no recursion, no mutable statics. See docs/linebreak.md.
#include "linebreak.h"

namespace lumora::linebreak {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kSpace = static_cast<u32>(ItemKind::Space);
constexpr u32 kNewline = static_cast<u32>(ItemKind::Newline);

// Walks the lines. Returns the number of lines; writes line starts when out is not null (it must then hold that many).
u32 walk(const Item* items, u32 count, u64 max_width, u32* out) {
  u32 lines = 0;
  u32 s = 0;
  while (s < count) {
    if (out != nullptr) out[lines] = s;
    ++lines;
    u64 used = 0;
    bool has_space = false;
    u32 last_space = 0;
    u32 next = count;
    u32 j = s;
    while (j < count) {
      const u32 kind = items[j].kind;
      if (kind == kSpace) {
        used += static_cast<u64>(static_cast<u32>(items[j].advance));
        has_space = true;
        last_space = j;
        ++j;
      } else if (kind == kNewline) {
        next = j + 1;
        break;
      } else {
        const u64 adv = static_cast<u64>(static_cast<u32>(items[j].advance));
        if (used + adv <= max_width) {
          used += adv;
          ++j;
        } else {
          if (has_space) next = last_space + 1;
          else if (j > s) next = j;
          else next = j + 1;
          break;
        }
      }
    }
    s = next;
  }
  return lines;
}

}  // namespace

BreakResult break_lines(const Item* items, std::uint32_t count, std::int32_t max_width, std::uint32_t* line_starts,
                        std::uint32_t capacity, std::uint32_t* line_count) {
  if (items == nullptr || line_starts == nullptr || line_count == nullptr) return BreakResult::NullArgument;
  if (max_width <= 0) return BreakResult::BadWidth;
  if (count > kMaxItems) return BreakResult::TooManyItems;
  for (u32 i = 0; i < count; ++i) {
    if (items[i].advance < 0 || items[i].kind > kNewline) return BreakResult::BadItem;
  }
  const u64 width = static_cast<u64>(max_width);
  const u32 lines = walk(items, count, width, nullptr);
  if (lines > capacity) return BreakResult::OutputFull;
  walk(items, count, width, line_starts);
  *line_count = lines;
  return BreakResult::Ok;
}

}  // namespace lumora::linebreak
