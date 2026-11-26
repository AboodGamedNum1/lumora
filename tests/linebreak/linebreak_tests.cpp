// Tests for core/linebreak (scope AF). Expected values come from an independent Python reference (tools/linebreak_ref.py: it follows
// the rule text literally with Python integers and its own FNV) and are pasted below: scenarios with their expected line starts, and
// seeded random corpora checked by a digest of every result. A separate oracle checks structural properties of the lines. Buffers are
// exact-size heap vectors so ASan sees any stray access. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "linebreak.h"

using namespace lumora::linebreak;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kSentinel = 0xA5A5A5A5u;
constexpr std::int32_t kInt32Max = 2147483647;

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int k = 0; k < 4; ++k) {
      h ^= (v >> (8 * k)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

struct Lcg {
  u64 s;
  explicit Lcg(u64 seed) : s(seed) {}
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  u32 rng(u32 lo, u32 hi) { return lo + next() % (hi - lo + 1); }
};

struct Scn {
  const Item* items;
  u32 count;
  std::int32_t width;
  u32 cap;
  int status;
  const u32* starts;
  u32 nlines;
};

struct CorpusRef {
  u64 digest;
  u32 seed, cases, max_items, max_adv, max_width, words;
};

struct Out {
  BreakResult r;
  u32 lines;
  std::vector<u32> starts;
};

// Runs break_lines with exact-size buffers and checks the no-write rules on failure.
Out run(const std::vector<Item>& items_in, std::int32_t width, u32 cap) {
  std::vector<Item> items = items_in;
  if (items.empty()) items.push_back(Item{});  // a non-null pointer is required even for count 0
  std::vector<u32> starts(cap == 0 ? 1 : cap, kSentinel);
  u32 lines = kSentinel;
  const BreakResult r = break_lines(items.data(), static_cast<u32>(items_in.size()), width, starts.data(), cap, &lines);
  Out o;
  o.r = r;
  if (r != BreakResult::Ok) {
    REQUIRE(lines == kSentinel);
    for (u32 v : starts) REQUIRE(v == kSentinel);
    o.lines = 0;
  } else {
    REQUIRE(lines <= cap);
    for (u32 i = lines; i < starts.size(); ++i) REQUIRE(starts[i] == kSentinel);  // nothing beyond the lines is written
    o.lines = lines;
    o.starts.assign(starts.begin(), starts.begin() + lines);
  }
  return o;
}

const Item kItems_empty[] = {{0, 0}};
const u32 kStarts_empty[] = {0};
const Scn kScn_empty = {kItems_empty, 0u, 10, 100u, 0, kStarts_empty, 0u};
const Item kItems_one_glyph[] = {{5, 0}};
const u32 kStarts_one_glyph[] = {0u};
const Scn kScn_one_glyph = {kItems_one_glyph, 1u, 10, 100u, 0, kStarts_one_glyph, 1u};
const Item kItems_exact_fit[] = {{4, 0}, {6, 0}};
const u32 kStarts_exact_fit[] = {0u};
const Scn kScn_exact_fit = {kItems_exact_fit, 2u, 10, 100u, 0, kStarts_exact_fit, 1u};
const Item kItems_one_over[] = {{4, 0}, {7, 0}};
const u32 kStarts_one_over[] = {0u, 1u};
const Scn kScn_one_over = {kItems_one_over, 2u, 10, 100u, 0, kStarts_one_over, 2u};
const Item kItems_word_wrap[] = {{3, 0}, {3, 0}, {1, 1}, {3, 0}, {3, 0}, {1, 1}, {3, 0}};
const u32 kStarts_word_wrap[] = {0u, 3u, 6u};
const Scn kScn_word_wrap = {kItems_word_wrap, 7u, 8, 100u, 0, kStarts_word_wrap, 3u};
const Item kItems_hang[] = {{5, 0}, {4, 1}, {5, 0}};
const u32 kStarts_hang[] = {0u, 2u};
const Scn kScn_hang = {kItems_hang, 3u, 8, 100u, 0, kStarts_hang, 2u};
const Item kItems_hang_exact[] = {{5, 0}, {1, 1}, {2, 0}, {1, 0}};
const u32 kStarts_hang_exact[] = {0u, 2u};
const Scn kScn_hang_exact = {kItems_hang_exact, 4u, 8, 100u, 0, kStarts_hang_exact, 2u};
const Item kItems_cut_word[] = {{3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}};
const u32 kStarts_cut_word[] = {0u, 2u, 4u};
const Scn kScn_cut_word = {kItems_cut_word, 5u, 7, 100u, 0, kStarts_cut_word, 3u};
const Item kItems_wide_glyph[] = {{20, 0}, {1, 0}};
const u32 kStarts_wide_glyph[] = {0u, 1u};
const Scn kScn_wide_glyph = {kItems_wide_glyph, 2u, 5, 100u, 0, kStarts_wide_glyph, 2u};
const Item kItems_wide_glyph_mid[] = {{2, 0}, {1, 1}, {20, 0}, {1, 0}};
const u32 kStarts_wide_glyph_mid[] = {0u, 2u, 3u};
const Scn kScn_wide_glyph_mid = {kItems_wide_glyph_mid, 4u, 5, 100u, 0, kStarts_wide_glyph_mid, 3u};
const Item kItems_last_space[] = {{2, 0}, {1, 1}, {2, 0}, {1, 1}, {2, 0}, {4, 0}};
const u32 kStarts_last_space[] = {0u, 4u};
const Scn kScn_last_space = {kItems_last_space, 6u, 8, 100u, 0, kStarts_last_space, 2u};
const Item kItems_spaces_only[] = {{1, 1}, {1, 1}, {1, 1}, {1, 1}};
const u32 kStarts_spaces_only[] = {0u};
const Scn kScn_spaces_only = {kItems_spaces_only, 4u, 2, 100u, 0, kStarts_spaces_only, 1u};
const Item kItems_space_then_glyph[] = {{3, 1}, {3, 1}, {3, 1}, {3, 0}};
const u32 kStarts_space_then_glyph[] = {0u, 3u};
const Scn kScn_space_then_glyph = {kItems_space_then_glyph, 4u, 7, 100u, 0, kStarts_space_then_glyph, 2u};
const Item kItems_leading_space_line[] = {{5, 0}, {1, 1}, {1, 1}, {3, 0}, {3, 0}};
const u32 kStarts_leading_space_line[] = {0u, 3u};
const Scn kScn_leading_space_line = {kItems_leading_space_line, 5u, 6, 100u, 0, kStarts_leading_space_line, 2u};
const Item kItems_newline_mid[] = {{2, 0}, {9, 2}, {2, 0}, {2, 0}, {0, 2}, {1, 0}};
const u32 kStarts_newline_mid[] = {0u, 2u, 5u};
const Scn kScn_newline_mid = {kItems_newline_mid, 6u, 100, 100u, 0, kStarts_newline_mid, 3u};
const Item kItems_newline_end[] = {{2, 0}, {9, 2}};
const u32 kStarts_newline_end[] = {0u};
const Scn kScn_newline_end = {kItems_newline_end, 2u, 100, 100u, 0, kStarts_newline_end, 1u};
const Item kItems_newline_only[] = {{5, 2}, {5, 2}, {5, 2}};
const u32 kStarts_newline_only[] = {0u, 1u, 2u};
const Scn kScn_newline_only = {kItems_newline_only, 3u, 100, 100u, 0, kStarts_newline_only, 3u};
const Item kItems_newline_after_space[] = {{1, 0}, {1, 1}, {0, 2}, {1, 1}, {1, 0}};
const u32 kStarts_newline_after_space[] = {0u, 3u};
const Scn kScn_newline_after_space = {kItems_newline_after_space, 5u, 100, 100u, 0, kStarts_newline_after_space, 2u};
const Item kItems_zero_adv[] = {{0, 0}, {0, 0}, {0, 1}, {0, 0}, {0, 2}, {0, 0}};
const u32 kStarts_zero_adv[] = {0u, 5u};
const Scn kScn_zero_adv = {kItems_zero_adv, 6u, 1, 100u, 0, kStarts_zero_adv, 2u};
const Item kItems_width_one[] = {{1, 0}, {1, 0}, {1, 1}, {1, 0}, {2, 0}, {1, 0}};
const u32 kStarts_width_one[] = {0u, 1u, 3u, 4u, 5u};
const Scn kScn_width_one = {kItems_width_one, 6u, 1, 100u, 0, kStarts_width_one, 5u};
const Item kItems_recheck_after_wrap[] = {{4, 0}, {1, 1}, {3, 0}, {3, 0}, {1, 1}, {3, 0}, {3, 0}, {3, 0}};
const u32 kStarts_recheck_after_wrap[] = {0u, 2u, 5u, 7u};
const Scn kScn_recheck_after_wrap = {kItems_recheck_after_wrap, 8u, 7, 100u, 0, kStarts_recheck_after_wrap, 4u};
const Item kItems_rewrap_cut[] = {{2, 0}, {1, 1}, {5, 0}, {5, 0}, {5, 0}};
const u32 kStarts_rewrap_cut[] = {0u, 2u, 3u, 4u};
const Scn kScn_rewrap_cut = {kItems_rewrap_cut, 5u, 6, 100u, 0, kStarts_rewrap_cut, 4u};
const Item kItems_space_run_overflow[] = {{1, 0}, {5, 1}, {5, 1}, {5, 1}, {1, 0}};
const u32 kStarts_space_run_overflow[] = {0u, 4u};
const Scn kScn_space_run_overflow = {kItems_space_run_overflow, 5u, 4, 100u, 0, kStarts_space_run_overflow, 2u};
const Item kItems_glyph_after_wide_line[] = {{9, 0}, {1, 0}, {1, 1}, {1, 0}};
const u32 kStarts_glyph_after_wide_line[] = {0u, 1u};
const Scn kScn_glyph_after_wide_line = {kItems_glyph_after_wide_line, 4u, 9, 100u, 0, kStarts_glyph_after_wide_line, 2u};
const Item kItems_big_adv[] = {{2147483647, 0}, {2147483647, 0}, {2147483647, 0}, {2147483647, 1}, {1, 0}};
const u32 kStarts_big_adv[] = {0u, 1u, 2u, 4u};
const Scn kScn_big_adv = {kItems_big_adv, 5u, 2147483647, 100u, 0, kStarts_big_adv, 4u};
const Item kItems_big_sum[] = {{2147483647, 0}, {2147483647, 1}, {2147483647, 0}, {2147483647, 1}, {2147483647, 0}};
const u32 kStarts_big_sum[] = {0u, 2u, 4u};
const Scn kScn_big_sum = {kItems_big_sum, 5u, 2147483647, 100u, 0, kStarts_big_sum, 3u};
const Item kItems_cap_exact[] = {{5, 0}, {5, 0}, {5, 0}};
const u32 kStarts_cap_exact[] = {0u, 1u, 2u};
const Scn kScn_cap_exact = {kItems_cap_exact, 3u, 5, 3u, 0, kStarts_cap_exact, 3u};
const Item kItems_cap_short[] = {{5, 0}, {5, 0}, {5, 0}};
const u32 kStarts_cap_short[] = {0};
const Scn kScn_cap_short = {kItems_cap_short, 3u, 5, 2u, 5, kStarts_cap_short, 0u};
const Item kItems_cap_zero_empty[] = {{0, 0}};
const u32 kStarts_cap_zero_empty[] = {0};
const Scn kScn_cap_zero_empty = {kItems_cap_zero_empty, 0u, 5, 0u, 0, kStarts_cap_zero_empty, 0u};
const Item kItems_cap_zero_one[] = {{1, 0}};
const u32 kStarts_cap_zero_one[] = {0};
const Scn kScn_cap_zero_one = {kItems_cap_zero_one, 1u, 5, 0u, 5, kStarts_cap_zero_one, 0u};
const Item kItems_bad_width0[] = {{1, 0}};
const u32 kStarts_bad_width0[] = {0};
const Scn kScn_bad_width0 = {kItems_bad_width0, 1u, 0, 100u, 2, kStarts_bad_width0, 0u};
const Item kItems_bad_width_neg[] = {{1, 0}};
const u32 kStarts_bad_width_neg[] = {0};
const Scn kScn_bad_width_neg = {kItems_bad_width_neg, 1u, -3, 100u, 2, kStarts_bad_width_neg, 0u};
const Item kItems_bad_width_min[] = {{0, 0}};
const u32 kStarts_bad_width_min[] = {0};
const Scn kScn_bad_width_min = {kItems_bad_width_min, 0u, -2147483648, 100u, 2, kStarts_bad_width_min, 0u};
const Scn* const kScns[] = {&kScn_empty, &kScn_one_glyph, &kScn_exact_fit, &kScn_one_over, &kScn_word_wrap, &kScn_hang, &kScn_hang_exact, &kScn_cut_word, &kScn_wide_glyph, &kScn_wide_glyph_mid, &kScn_last_space, &kScn_spaces_only, &kScn_space_then_glyph, &kScn_leading_space_line, &kScn_newline_mid, &kScn_newline_end, &kScn_newline_only, &kScn_newline_after_space, &kScn_zero_adv, &kScn_width_one, &kScn_recheck_after_wrap, &kScn_rewrap_cut, &kScn_space_run_overflow, &kScn_glyph_after_wide_line, &kScn_big_adv, &kScn_big_sum, &kScn_cap_exact, &kScn_cap_short, &kScn_cap_zero_empty, &kScn_cap_zero_one, &kScn_bad_width0, &kScn_bad_width_neg, &kScn_bad_width_min};
const CorpusRef kCorpus_f1 = {1905795793429421621ull, 241u, 400u, 12u, 5u, 12u, 1543u};
const CorpusRef kCorpus_f2 = {11114984875723479949ull, 242u, 300u, 40u, 9u, 30u, 1926u};
const CorpusRef kCorpus_f3 = {4046825298950839510ull, 243u, 150u, 200u, 20u, 60u, 3373u};
const CorpusRef kCorpus_f4 = {5981670471604488472ull, 244u, 120u, 60u, 1000u, 3000u, 996u};
const CorpusRef kCorpus_f5 = {11029104669754015256ull, 245u, 200u, 25u, 3u, 3u, 1157u};

}  // namespace

TEST_CASE("linebreak: scenarios against the reference", "[linebreak]") {
  int idx = 0;
  for (const Scn* s : kScns) {
    INFO("scenario " << idx++);
    const std::vector<Item> items(s->items, s->items + s->count);
    const Out o = run(items, s->width, s->cap);
    REQUIRE(static_cast<int>(o.r) == s->status);
    if (o.r == BreakResult::Ok) {
      REQUIRE(o.lines == s->nlines);
      for (u32 i = 0; i < s->nlines; ++i) REQUIRE(o.starts[i] == s->starts[i]);
    }
  }
}

TEST_CASE("linebreak: seeded random corpora", "[linebreak]") {
  const CorpusRef* refs[] = {&kCorpus_f1, &kCorpus_f2, &kCorpus_f3, &kCorpus_f4, &kCorpus_f5};
  for (const CorpusRef* c : refs) {
    Lcg rng(c->seed);
    Fnv f;
    u32 words = 0;
    auto put = [&](u32 v) {
      f.word(v);
      ++words;
    };
    for (u32 k = 0; k < c->cases; ++k) {
      const u32 n = rng.rng(0, c->max_items);
      std::vector<Item> items(n);
      for (u32 i = 0; i < n; ++i) {
        const u32 kk = rng.rng(0, 99);
        const std::uint8_t kind = kk < 62 ? 0 : kk < 90 ? 1 : 2;
        items[i].kind = kind;
        items[i].advance = static_cast<std::int32_t>(rng.rng(0, c->max_adv));
      }
      const std::int32_t width = static_cast<std::int32_t>(rng.rng(1, c->max_width));
      const u32 cap = rng.rng(0, n + 1);
      const Out o = run(items, width, cap);
      put(static_cast<u32>(o.r));
      put(o.lines);
      for (u32 v : o.starts) put(v);
    }
    INFO("seed " << c->seed);
    REQUIRE(words == c->words);
    REQUIRE(f.h == c->digest);
  }
}

TEST_CASE("linebreak: structural oracle on random input", "[linebreak]") {
  Lcg rng(0x5EED);
  int bad = 0;
  int why[6] = {0, 0, 0, 0, 0, 0};
  for (int k = 0; k < 600; ++k) {
    const u32 n = rng.rng(0, 30);
    std::vector<Item> items(n);
    for (u32 i = 0; i < n; ++i) {
      const u32 kk = rng.rng(0, 99);
      items[i].kind = kk < 60 ? 0 : kk < 88 ? 1 : 2;
      items[i].advance = static_cast<std::int32_t>(rng.rng(0, 6));
    }
    const std::int32_t width = static_cast<std::int32_t>(rng.rng(1, 14));
    const Out o = run(items, width, n + 1);
    if (o.r != BreakResult::Ok) {
      ++bad;
      continue;
    }
    if ((n == 0) != (o.lines == 0)) ++bad;
    u32 prev_end = 0;
    for (u32 li = 0; li < o.lines; ++li) {
      const u32 s = o.starts[li];
      const u32 e = li + 1 < o.lines ? o.starts[li + 1] : n;  // exclusive
      if (s != prev_end || e <= s) ++bad, ++why[0];                       // contiguous cover, no empty line
      prev_end = e;
      // a Newline can only be the last item of its line
      for (u32 i = s; i + 1 < e; ++i)
        if (items[i].kind == 2) ++bad, ++why[1];
      // up to the last Glyph of the line the sum fits, unless that glyph is alone at the line start
      u32 g = e;
      for (u32 i = s; i < e; ++i)
        if (items[i].kind == 0) g = i;
      if (g != e && g != s) {
        u64 sum = 0;
        for (u32 i = s; i <= g; ++i) sum += static_cast<u32>(items[i].advance);
        if (sum > static_cast<u64>(width)) ++bad, ++why[2];
      }
      // a line that is not the last ends with a Space or Newline, or is cut right before a glyph (or is one lone wide glyph)
      if (li + 1 < o.lines) {
        const std::uint8_t last = items[e - 1].kind;
        if (last == 0 && e - s > 1 && items[e].kind != 0) ++bad, ++why[3];  // a cut inside a word is followed by a glyph
      }
    }
    if (o.lines > 0 && prev_end != n) ++bad, ++why[4];
  }
  INFO(why[0] << ' ' << why[1] << ' ' << why[2] << ' ' << why[3] << ' ' << why[4]);
  REQUIRE(bad == 0);
}

TEST_CASE("linebreak: hand computed lines", "[linebreak]") {
  auto G = [](std::int32_t a) { return Item{a, 0}; };
  auto S = [](std::int32_t a) { return Item{a, 1}; };
  auto N = [](std::int32_t a) { return Item{a, 2}; };
  {
    // width 10: "G4 G6" fits exactly, a seventh unit does not
    const Out o = run({G(4), G(6)}, 10, 4);
    REQUIRE(o.lines == 1);
    const Out p = run({G(4), G(7)}, 10, 4);
    REQUIRE(p.lines == 2);
    REQUIRE(p.starts[1] == 1);
  }
  {
    // the trailing space hangs: G5 S4 fits in 8 only because the space is not counted against a following glyph on this line
    const Out o = run({G(5), S(4), G(5)}, 8, 4);
    REQUIRE(o.lines == 2);
    REQUIRE(o.starts[0] == 0);
    REQUIRE(o.starts[1] == 2);
  }
  {
    // a lone glyph wider than the width gets its own line
    const Out o = run({G(1), G(20), G(1)}, 5, 4);
    REQUIRE(o.lines == 3);
    REQUIRE(o.starts[1] == 1);
    REQUIRE(o.starts[2] == 2);
  }
  {
    // a newline ends the line and its advance is never counted; none at the end makes an extra line
    const Out o = run({G(2), N(1000000), G(2)}, 3, 4);
    REQUIRE(o.lines == 2);
    REQUIRE(o.starts[1] == 2);
    const Out p = run({G(2), N(1000000)}, 3, 4);
    REQUIRE(p.lines == 1);
  }
  {
    // the line after a wrap at the last space starts with the items after that space, which are examined again
    const Out o = run({G(4), S(1), G(3), G(3), S(1), G(3), G(3), G(3)}, 7, 8);
    REQUIRE(o.lines == 4);
    REQUIRE(o.starts[1] == 2);
    REQUIRE(o.starts[2] == 5);
    REQUIRE(o.starts[3] == 7);
  }
  {
    // sums use 64 bits: three INT32_MAX glyphs never wrap around
    const Out o = run({G(kInt32Max), G(kInt32Max), G(kInt32Max)}, kInt32Max, 4);
    REQUIRE(o.lines == 3);
    const Out p = run({G(kInt32Max), S(kInt32Max), G(1)}, kInt32Max, 4);
    REQUIRE(p.lines == 2);
    REQUIRE(p.starts[1] == 2);
  }
}

TEST_CASE("linebreak: argument checks, their order, nothing written on failure", "[linebreak]") {
  Item items[3] = {{1, 0}, {1, 0}, {1, 0}};
  u32 starts[4] = {kSentinel, kSentinel, kSentinel, kSentinel};
  u32 lines = kSentinel;
  REQUIRE(break_lines(nullptr, 1, 5, starts, 4, &lines) == BreakResult::NullArgument);
  REQUIRE(break_lines(items, 1, 5, nullptr, 4, &lines) == BreakResult::NullArgument);
  REQUIRE(break_lines(items, 1, 5, starts, 4, nullptr) == BreakResult::NullArgument);
  REQUIRE(break_lines(items, 0, 5, starts, 4, nullptr) == BreakResult::NullArgument);  // null even for count 0
  REQUIRE(break_lines(nullptr, 1, 0, nullptr, 0, nullptr) == BreakResult::NullArgument);  // before BadWidth
  REQUIRE(break_lines(items, 1, 0, starts, 4, &lines) == BreakResult::BadWidth);
  REQUIRE(break_lines(items, 1, -1, starts, 4, &lines) == BreakResult::BadWidth);
  REQUIRE(break_lines(items, 1, -2147483647 - 1, starts, 4, &lines) == BreakResult::BadWidth);
  REQUIRE(break_lines(items, kMaxItems + 1, 0, starts, 4, &lines) == BreakResult::BadWidth);  // before TooManyItems
  REQUIRE(break_lines(items, kMaxItems + 1, 5, starts, 4, &lines) == BreakResult::TooManyItems);
  REQUIRE(break_lines(items, 0xFFFFFFFFu, 5, starts, 4, &lines) == BreakResult::TooManyItems);
  items[2].advance = -1;
  REQUIRE(break_lines(items, 3, 5, starts, 4, &lines) == BreakResult::BadItem);  // the last item is checked too
  u32 scratch[4];
  u32 scratch_lines = 0;
  REQUIRE(break_lines(items, 2, 5, scratch, 4, &scratch_lines) == BreakResult::Ok);  // but only the first count items
  lines = kSentinel;
  items[2].advance = 1;
  items[1].kind = 3;
  REQUIRE(break_lines(items, 3, 5, starts, 0, &lines) == BreakResult::BadItem);  // before OutputFull
  items[1].kind = 255;
  REQUIRE(break_lines(items, 3, 5, starts, 4, &lines) == BreakResult::BadItem);
  items[1].kind = 0;
  items[0].advance = -2147483647 - 1;
  REQUIRE(break_lines(items, 3, 5, starts, 4, &lines) == BreakResult::BadItem);
  items[0].advance = 1;
  REQUIRE(lines == kSentinel);
  REQUIRE(starts[0] == kSentinel);
  REQUIRE(starts[3] == kSentinel);
  // OutputFull writes nothing, then the same call with room succeeds
  REQUIRE(break_lines(items, 3, 1, starts, 2, &lines) == BreakResult::OutputFull);
  REQUIRE(lines == kSentinel);
  REQUIRE(starts[0] == kSentinel);
  REQUIRE(starts[1] == kSentinel);
  REQUIRE(break_lines(items, 3, 1, starts, 3, &lines) == BreakResult::Ok);
  REQUIRE(lines == 3);
  REQUIRE(starts[0] == 0);
  REQUIRE(starts[1] == 1);
  REQUIRE(starts[2] == 2);
  REQUIRE(starts[3] == kSentinel);
}

TEST_CASE("linebreak: the item limit", "[linebreak]") {
  std::vector<Item> items(kMaxItems, Item{1, 0});
  std::vector<u32> starts(kMaxItems);
  u32 lines = 0;
  REQUIRE(break_lines(items.data(), kMaxItems, 1, starts.data(), kMaxItems, &lines) == BreakResult::Ok);
  REQUIRE(lines == kMaxItems);
  REQUIRE(starts[kMaxItems - 1] == kMaxItems - 1);
  REQUIRE(break_lines(items.data(), kMaxItems, 1, starts.data(), kMaxItems - 1, &lines) == BreakResult::OutputFull);
  REQUIRE(lines == kMaxItems);  // unchanged
  REQUIRE(break_lines(items.data(), kMaxItems, static_cast<std::int32_t>(kMaxItems), starts.data(), 1, &lines) == BreakResult::Ok);
  REQUIRE(lines == 1);
  REQUIRE(starts[0] == 0);
  // a long run of spaces with one glyph at the end: a single wrap, linear work
  std::vector<Item> sp(kMaxItems, Item{1, 1});
  sp.back() = Item{1, 0};
  REQUIRE(break_lines(sp.data(), kMaxItems, 2, starts.data(), kMaxItems, &lines) == BreakResult::Ok);
  REQUIRE(lines == 2);
  REQUIRE(starts[1] == kMaxItems - 1);
}
