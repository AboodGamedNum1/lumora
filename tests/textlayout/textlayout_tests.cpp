// Tests for core/textlayout. Pure CPU, integer only, deterministic (own LCG, no std distributions). The expected quads and
// golden digests were produced by an independent implementation of the rules in docs/textlayout.md (Python), not by the code
// under test. The golden digests are also written in docs/textlayout.md and must match on Linux and Windows.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "textlayout.h"

using namespace lumora::textlayout;

namespace {

// Test font: texture 64 x 64, line height 10 px (640). Glyphs: space (advance 5 px, no rectangle), A B V W, a b c d e i, e acute
// (U+00E9) and U+FFFD. Kerning: (space,A) -1 px, (A,V) -2 px, (V,A) -3 px, (a,b) +1 px.
const Glyph f0_g[] = {
    {32, 0, 0, 0, 0, 0, 0, 320},
    {65, 1, 1, 6, 8, 64, 128, 448},
    {66, 8, 1, 5, 8, 0, 128, 384},
    {86, 14, 1, 7, 8, 0, 128, 448},
    {87, 22, 1, 10, 8, 0, 128, 704},
    {97, 1, 10, 4, 5, 0, 192, 320},
    {98, 6, 10, 5, 5, 0, 192, 384},
    {99, 12, 10, 5, 5, 0, 192, 384},
    {100, 18, 10, 5, 5, 0, 192, 384},
    {101, 24, 10, 4, 5, 0, 192, 320},
    {105, 30, 10, 1, 5, 64, 192, 128},
    {233, 33, 10, 4, 7, 0, 64, 320},
    {65533, 40, 1, 6, 8, 0, 128, 448},
};
const Kerning f0_k[] = {
    {32, 65, -64},
    {65, 86, -128},
    {86, 65, -192},
    {97, 98, 64},
};
const Glyph f2_g[] = {
    {65, 1, 1, 6, 8, 64, 128, 448},
    {66, 8, 1, 5, 8, 0, 128, 384},
    {86, 14, 1, 7, 8, 0, 128, 448},
    {87, 22, 1, 10, 8, 0, 128, 704},
    {97, 1, 10, 4, 5, 0, 192, 320},
    {98, 6, 10, 5, 5, 0, 192, 384},
    {99, 12, 10, 5, 5, 0, 192, 384},
    {100, 18, 10, 5, 5, 0, 192, 384},
    {101, 24, 10, 4, 5, 0, 192, 320},
    {105, 30, 10, 1, 5, 64, 192, 128},
    {233, 33, 10, 4, 7, 0, 64, 320},
    {65533, 40, 1, 6, 8, 0, 128, 448},
};
const Kerning f2_k[] = {
    {65, 86, -128},
    {86, 65, -192},
    {97, 98, 64},
};

Font make_font(int which) {
  Font f;
  f.line_height = 640;
  f.tex_width = 64;
  f.tex_height = 64;
  if (which == 2) {  // no space glyph
    f.glyphs = f2_g;
    f.glyph_count = sizeof f2_g / sizeof f2_g[0];
    f.kerning = f2_k;
    f.kerning_count = sizeof f2_k / sizeof f2_k[0];
    return f;
  }
  f.glyphs = f0_g;
  f.glyph_count = sizeof f0_g / sizeof f0_g[0];
  f.kerning = f0_k;
  f.kerning_count = sizeof f0_k / sizeof f0_k[0];
  if (which == 1) f.fallback = 0x3F;  // missing too
  if (which == 3) f.fallback = 0x62;  // 'b'
  return f;
}

struct Exp {
  Fixed x, y, w, h;
  std::uint16_t tx, ty, tw, th;
  std::uint32_t cp, src;
};

struct Case {
  const char* name;
  int font;
  const char* text;  // may hold any bytes: the length is text_len
  std::size_t text_len;
  Fixed max_width;
  int align;
  std::uint32_t max_glyphs;
  std::vector<Exp> quads;
  std::size_t lines, invalid;
  Fixed width, height;
  bool truncated;
};

const std::vector<Case>& cases();

struct Lcg {
  std::uint32_t s;
  std::uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
};

const char* const kAlpha[] = {"a", "b", "c", "d", "e", "A", "V", "W", "i", " ", " ", " ", "\n", "\t", "\r", "\xc3\xa9", "\xe2\x82\xac",
                              "\xff", "\xc3", "\x01", "?", "ab", "AV"};
constexpr std::size_t kAlphaCount = sizeof kAlpha / sizeof kAlpha[0];

std::string random_text(std::uint32_t seed, std::size_t pieces, std::size_t min_bytes = 0) {
  Lcg g{seed};
  std::string out;
  for (std::size_t i = 0; i < pieces || out.size() < min_bytes; ++i) out += kAlpha[(g.next() >> 8) % kAlphaCount];
  return out;
}

struct Out {
  LayoutResult r;
  std::vector<Quad> q;
};

Out run(const Font& f, const std::string& t, Fixed max_width, int align, std::uint32_t max_glyphs) {
  LayoutOptions o;
  o.max_width = max_width;
  o.align = static_cast<Align>(align);
  o.max_glyphs = max_glyphs;
  Out out;
  out.q.resize(t.size() + 2);
  out.r = layout_text(f, t.data(), t.size(), o, out.q.data(), out.q.size());
  out.q.resize(out.r.quad_count);
  return out;
}

void same(const Quad& q, const Exp& e) {
  REQUIRE(q.x == e.x);
  REQUIRE(q.y == e.y);
  REQUIRE(q.w == e.w);
  REQUIRE(q.h == e.h);
  REQUIRE(q.tex_x == e.tx);
  REQUIRE(q.tex_y == e.ty);
  REQUIRE(q.tex_w == e.tw);
  REQUIRE(q.tex_h == e.th);
  REQUIRE(q.codepoint == e.cp);
  REQUIRE(q.source_offset == e.src);
}

}  // namespace

TEST_CASE("status texts are distinct", "[textlayout]") {
  const FontStatus fs[] = {FontStatus::Ok,           FontStatus::BadMetrics,      FontStatus::BadGlyphOrder,
                           FontStatus::BadGlyphRect, FontStatus::BadKerningOrder, FontStatus::NullData};
  for (std::size_t i = 0; i < 6; ++i) {
    REQUIRE(std::strlen(font_status_text(fs[i])) > 0);
    for (std::size_t j = i + 1; j < 6; ++j) REQUIRE(std::strcmp(font_status_text(fs[i]), font_status_text(fs[j])) != 0);
  }
  const LayoutStatus ls[] = {LayoutStatus::Ok, LayoutStatus::BadFont, LayoutStatus::BadOptions, LayoutStatus::BufferTooSmall};
  for (std::size_t i = 0; i < 4; ++i) {
    REQUIRE(std::strlen(layout_status_text(ls[i])) > 0);
    for (std::size_t j = i + 1; j < 4; ++j) REQUIRE(std::strcmp(layout_status_text(ls[i]), layout_status_text(ls[j])) != 0);
  }
}

TEST_CASE("validate_font: every status and the check order", "[textlayout][font]") {
  REQUIRE(validate_font(make_font(0)) == FontStatus::Ok);
  REQUIRE(validate_font(make_font(2)) == FontStatus::Ok);
  Font empty;
  empty.line_height = 1;
  empty.tex_width = 1;
  empty.tex_height = 1;
  REQUIRE(validate_font(empty) == FontStatus::Ok);  // no glyphs, null pointers with zero counts

  Font f = make_font(0);
  f.line_height = 0;
  REQUIRE(validate_font(f) == FontStatus::BadMetrics);
  f.line_height = -5;
  REQUIRE(validate_font(f) == FontStatus::BadMetrics);
  f = make_font(0);
  f.tex_width = 0;
  REQUIRE(validate_font(f) == FontStatus::BadMetrics);
  f = make_font(0);
  f.tex_height = 0;
  REQUIRE(validate_font(f) == FontStatus::BadMetrics);

  std::vector<Glyph> g(f0_g, f0_g + sizeof f0_g / sizeof f0_g[0]);
  f = make_font(0);
  f.glyphs = g.data();
  g[3].advance = -1;
  REQUIRE(validate_font(f) == FontStatus::BadMetrics);
  g[3].advance = 0;
  g[3].codepoint = g[2].codepoint;  // equal, not strictly ascending
  REQUIRE(validate_font(f) == FontStatus::BadGlyphOrder);
  g[3].codepoint = g[2].codepoint - 1;
  REQUIRE(validate_font(f) == FontStatus::BadGlyphOrder);
  g.assign(f0_g, f0_g + sizeof f0_g / sizeof f0_g[0]);
  g[0].codepoint = 0xD800;  // surrogate (also not ascending against the next one is irrelevant: it is not a scalar value)
  REQUIRE(validate_font(f) == FontStatus::BadGlyphOrder);
  g.assign(f0_g, f0_g + sizeof f0_g / sizeof f0_g[0]);
  g.back().codepoint = 0x110000;
  REQUIRE(validate_font(f) == FontStatus::BadGlyphOrder);
  g.back().codepoint = 0x10FFFF;
  REQUIRE(validate_font(f) == FontStatus::Ok);
  g.assign(f0_g, f0_g + sizeof f0_g / sizeof f0_g[0]);
  g[1].tex_x = 60;  // 60 + 6 > 64
  REQUIRE(validate_font(f) == FontStatus::BadGlyphRect);
  g[1].tex_x = 58;  // 58 + 6 == 64: touching the edge is fine
  REQUIRE(validate_font(f) == FontStatus::Ok);
  g[1].tex_y = 57;  // 57 + 8 > 64
  REQUIRE(validate_font(f) == FontStatus::BadGlyphRect);
  g[1].tex_y = 56;
  g[1].tex_x = 65535;  // 16-bit sums must not wrap
  g[1].tex_w = 65535;
  REQUIRE(validate_font(f) == FontStatus::BadGlyphRect);

  f = make_font(0);
  std::vector<Kerning> k(f0_k, f0_k + sizeof f0_k / sizeof f0_k[0]);
  f.kerning = k.data();
  REQUIRE(validate_font(f) == FontStatus::Ok);
  std::swap(k[0], k[1]);
  REQUIRE(validate_font(f) == FontStatus::BadKerningOrder);
  k.assign(f0_k, f0_k + sizeof f0_k / sizeof f0_k[0]);
  k[1] = k[0];  // equal pair
  REQUIRE(validate_font(f) == FontStatus::BadKerningOrder);
  k.assign(f0_k, f0_k + sizeof f0_k / sizeof f0_k[0]);
  k[2].first = k[1].first;  // same first, second not ascending
  k[2].second = k[1].second;
  REQUIRE(validate_font(f) == FontStatus::BadKerningOrder);

  f = make_font(0);
  f.glyphs = nullptr;
  REQUIRE(validate_font(f) == FontStatus::NullData);
  f = make_font(0);
  f.kerning = nullptr;
  REQUIRE(validate_font(f) == FontStatus::NullData);
  // Order: BadMetrics before everything else, BadKerningOrder before NullData.
  f = make_font(0);
  f.glyphs = nullptr;
  f.line_height = 0;
  REQUIRE(validate_font(f) == FontStatus::BadMetrics);
  f = make_font(0);
  f.glyphs = nullptr;
  k.assign(f0_k, f0_k + sizeof f0_k / sizeof f0_k[0]);
  std::swap(k[0], k[1]);
  f.kerning = k.data();
  REQUIRE(validate_font(f) == FontStatus::BadKerningOrder);
  // A glyph order error is found before a rectangle error, and a rectangle error before a kerning error.
  g.assign(f0_g, f0_g + sizeof f0_g / sizeof f0_g[0]);
  f = make_font(0);
  f.glyphs = g.data();
  f.kerning = k.data();
  g[1].tex_x = 63;
  REQUIRE(validate_font(f) == FontStatus::BadGlyphRect);
  g[4].codepoint = 1;
  REQUIRE(validate_font(f) == FontStatus::BadGlyphOrder);
}

TEST_CASE("layout_text: BadFont, BadOptions and their order", "[textlayout][status]") {
  Font bad = make_font(0);
  bad.line_height = 0;
  Quad q[4];
  LayoutOptions o;
  LayoutResult r = layout_text(bad, "a", 1, o, q, 4);
  REQUIRE(r.status == LayoutStatus::BadFont);
  REQUIRE(r.quad_count == 0);
  REQUIRE(r.line_count == 0);
  o.max_width = -1;
  REQUIRE(layout_text(make_font(0), "a", 1, o, q, 4).status == LayoutStatus::BadOptions);
  REQUIRE(layout_text(bad, "a", 1, o, q, 4).status == LayoutStatus::BadFont);  // font is checked first
  o = LayoutOptions();
  o.max_glyphs = 0;
  r = layout_text(make_font(0), "a", 1, o, q, 4);
  REQUIRE(r.status == LayoutStatus::BadOptions);
  REQUIRE(r.height == 0);
  o = LayoutOptions();
  o.align = static_cast<Align>(3);
  REQUIRE(layout_text(make_font(0), "a", 1, o, q, 4).status == LayoutStatus::BadOptions);
  o.align = static_cast<Align>(255);
  REQUIRE(layout_text(make_font(0), "a", 1, o, q, 4).status == LayoutStatus::BadOptions);
  // Options are checked even for empty text.
  o = LayoutOptions();
  o.max_width = -64;
  REQUIRE(layout_text(make_font(0), "", 0, o, q, 4).status == LayoutStatus::BadOptions);
  // Null text pointer is treated as empty text.
  o = LayoutOptions();
  r = layout_text(make_font(0), nullptr, 5, o, q, 4);
  REQUIRE(r.status == LayoutStatus::Ok);
  REQUIRE(r.line_count == 0);
}

TEST_CASE("table of exact layouts (hand-built font)", "[textlayout][exact]") {
  for (const Case& c : cases()) {
    INFO(c.name);
    const Font f = make_font(c.font);
    const std::string text(c.text, c.text_len);
    const Out o = run(f, text, c.max_width, c.align, c.max_glyphs);
    REQUIRE(o.r.status == LayoutStatus::Ok);
    REQUIRE(o.r.quad_count == c.quads.size());
    REQUIRE(o.q.size() == c.quads.size());
    for (std::size_t i = 0; i < c.quads.size(); ++i) same(o.q[i], c.quads[i]);
    REQUIRE(o.r.line_count == c.lines);
    REQUIRE(o.r.invalid_bytes == c.invalid);
    REQUIRE(o.r.width == c.width);
    REQUIRE(o.r.height == c.height);
    REQUIRE(o.r.truncated == c.truncated);

    LayoutOptions opt;
    opt.max_width = c.max_width;
    opt.align = static_cast<Align>(c.align);
    opt.max_glyphs = c.max_glyphs;
    // Count only: the same totals, nothing written, status Ok.
    LayoutResult r = layout_text(f, text.data(), text.size(), opt, nullptr, 0);
    REQUIRE(r.status == LayoutStatus::Ok);
    REQUIRE(r.quad_count == c.quads.size());
    REQUIRE(r.line_count == c.lines);
    REQUIRE(r.invalid_bytes == c.invalid);
    REQUIRE(r.width == c.width);
    REQUIRE(r.height == c.height);
    REQUIRE(r.truncated == c.truncated);
    r = layout_text(f, text.data(), text.size(), opt, nullptr, 1000);  // null output ignores capacity
    REQUIRE(r.status == LayoutStatus::Ok);
    // Capacity edges: exact fit is Ok, one short is BufferTooSmall with the first quads written, zero writes nothing.
    std::vector<Quad> buf(c.quads.size() + 1);
    const Quad mark{-7, -7, -7, -7, 9, 9, 9, 9, 7, 7};
    r = layout_text(f, text.data(), text.size(), opt, buf.data(), c.quads.size());
    REQUIRE(r.status == LayoutStatus::Ok);
    if (!c.quads.empty()) {
      buf.assign(c.quads.size() + 1, mark);
      r = layout_text(f, text.data(), text.size(), opt, buf.data(), c.quads.size() - 1);
      REQUIRE(r.status == LayoutStatus::BufferTooSmall);
      REQUIRE(r.quad_count == c.quads.size());
      REQUIRE(r.line_count == c.lines);
      REQUIRE(r.width == c.width);
      for (std::size_t i = 0; i + 1 < c.quads.size(); ++i) same(buf[i], c.quads[i]);
      REQUIRE(buf[c.quads.size() - 1].x == -7);  // not written
      r = layout_text(f, text.data(), text.size(), opt, buf.data(), 0);
      REQUIRE(r.status == LayoutStatus::BufferTooSmall);
      REQUIRE(r.quad_count == c.quads.size());
    }
  }
}

TEST_CASE("hand checked rules", "[textlayout][rules]") {
  const Font f = make_font(0);
  {  // Kerning between A and V is -2 px: V sits at 7 px - 2 px = 5 px (320); A has offset_x 1 px.
    const Out o = run(f, "AV", 0, 0, 100);
    REQUIRE(o.q.size() == 2);
    REQUIRE(o.q[0].x == 64);
    REQUIRE(o.q[1].x == 320);
    REQUIRE(o.q[0].y == 128);  // offset_y 2 px
    REQUIRE(o.q[0].w == 6 * 64);
    REQUIRE(o.q[0].h == 8 * 64);
    REQUIRE(o.r.width == 768);  // 7 px + 7 px - 2 px
    REQUIRE(o.r.height == 640);
  }
  {  // "Kerning off": a font without kerning pairs lays the same text without the adjustment.
    Font nk = f;
    nk.kerning = nullptr;
    nk.kerning_count = 0;
    const Out o = run(nk, "AV", 0, 0, 100);
    REQUIRE(o.q[1].x == 448);
    REQUIRE(o.r.width == 896);
  }
  {  // Spaces never draw a quad; the quad's source offset points at the byte.
    const Out o = run(f, "a b", 0, 0, 100);
    REQUIRE(o.q.size() == 2);
    REQUIRE(o.q[0].source_offset == 0);
    REQUIRE(o.q[1].source_offset == 2);
    REQUIRE(o.q[1].x == 320 + 320);
  }
  {  // Trailing spaces do not count toward the width; leading ones do.
    REQUIRE(run(f, "aa   ", 0, 0, 100).r.width == 640);
    REQUIRE(run(f, "   aa", 0, 0, 100).r.width == 3 * 320 + 640);
    REQUIRE(run(f, "     ", 0, 0, 100).r.width == 0);
  }
  {  // max_glyphs boundary.
    REQUIRE(run(f, "abcd", 0, 0, 3).r.truncated);
    REQUIRE(!run(f, "abcd", 0, 0, 4).r.truncated);
    REQUIRE(!run(f, "abcd", 0, 0, 5).r.truncated);
    REQUIRE(run(f, "abcd", 0, 0, 3).r.quad_count == 3);
  }
  {  // Empty input.
    const Out o = run(f, "", 0, 0, 100);
    REQUIRE(o.r.status == LayoutStatus::Ok);
    REQUIRE(o.r.quad_count == 0);
    REQUIRE(o.r.line_count == 0);
    REQUIRE(o.r.height == 0);
    REQUIRE(o.r.width == 0);
    REQUIRE(!o.r.truncated);
    REQUIRE(digest_quads(nullptr, 0) == 0xcbf29ce484222325ull);
  }
}

namespace {
struct Utf8Case {
  const char* name;
  std::string bytes;
  std::size_t invalid;  // invalid_bytes
  std::size_t repl;     // number of replacement characters drawn (U+FFFD quads)
};
}  // namespace

TEST_CASE("strict UTF-8: maximal subparts and invalid byte counts", "[textlayout][utf8]") {
  const Font f = make_font(0);  // U+FFFD exists
  const std::vector<Utf8Case> corpus = {
      {"valid ascii", "abc", 0, 0},
      {"valid 2 byte", "\xc3\xa9", 0, 0},
      {"valid 3 byte", "\xe2\x82\xac", 0, 1},  // U+20AC has no glyph: fallback drawn, but it is not an invalid byte
      {"valid 4 byte max", "\xf4\x8f\xbf\xbf", 0, 1},
      {"valid last before surrogates", "\xed\x9f\xbf", 0, 1},
      {"valid first after surrogates", "\xee\x80\x80", 0, 1},
      {"overlong 2 byte C0 80", "\xc0\x80", 2, 2},
      {"overlong 2 byte C1 BF", "\xc1\xbf", 2, 2},
      {"overlong 3 byte", "\xe0\x80\x80", 3, 3},
      {"overlong 3 byte E0 9F BF", "\xe0\x9f\xbf", 3, 3},
      {"overlong 4 byte", "\xf0\x80\x80\x80", 4, 4},
      {"overlong 4 byte F0 8F BF BF", "\xf0\x8f\xbf\xbf", 4, 4},
      {"surrogate D800", "\xed\xa0\x80", 3, 3},
      {"surrogate DFFF", "\xed\xbf\xbf", 3, 3},
      {"above range 110000", "\xf4\x90\x80\x80", 4, 4},
      {"above range F5", "\xf5\x80\x80\x80", 4, 4},
      {"above range FF", "\xff", 1, 1},
      {"FE", "\xfe", 1, 1},
      {"truncated 2 byte at end", "\xc2", 1, 1},
      {"truncated 3 byte after 1", "\xe2", 1, 1},
      {"truncated 3 byte after 2 at end", "\xe2\x82", 2, 1},
      {"truncated 3 byte before ascii", "\xe2\x82" "A", 2, 1},
      {"truncated 4 byte after 3 at end", "\xf0\x9f\x98", 3, 1},
      {"truncated 4 byte after 2 before ascii", "\xf0\x9f" "A", 2, 1},
      {"truncated 4 byte after 1 before ascii", "\xf0" "A", 1, 1},
      {"truncated before lead byte", "\xe2\x82\xc3\xa9", 2, 1},
      {"lone continuation 80", "\x80", 1, 1},
      {"lone continuation BF", "\xbf", 1, 1},
      {"two lone continuations", "\xbf\xbf", 2, 2},
      {"continuation after valid", "a\xc3\xa9\x80", 1, 1},
      {"E0 then A0 then ascii", "\xe0\xa0" "A", 2, 1},
      {"ED then 9F then ascii", "\xed\x9f" "A", 2, 1},
      {"F4 then 8F then ascii", "\xf4\x8f" "A", 2, 1},
      {"mixed", "A\xff\xc3" "B\xe2\x82", 4, 3},
  };
  for (const Utf8Case& c : corpus) {
    INFO(c.name);
    const Out o = run(f, c.bytes, 0, 0, 1000);
    REQUIRE(o.r.status == LayoutStatus::Ok);
    REQUIRE(o.r.invalid_bytes == c.invalid);
    std::size_t repl = 0;
    for (const Quad& q : o.q) repl += (q.codepoint == 0xFFFD) ? 1 : 0;
    REQUIRE(repl == c.repl);
  }
  {  // The replacement points at the first byte of its sequence.
    const Out o = run(f, std::string("a\xe2\x82") + "b", 0, 0, 100);
    REQUIRE(o.q.size() == 3);
    REQUIRE(o.q[1].codepoint == 0xFFFD);
    REQUIRE(o.q[1].source_offset == 1);
    REQUIRE(o.q[2].source_offset == 3);
  }
  {  // Embedded NUL is a control character and is skipped.
    const std::string s("a\0b", 3);
    const Out o = run(f, s, 0, 0, 100);
    REQUIRE(o.q.size() == 2);
    REQUIRE(o.q[1].source_offset == 2);
    REQUIRE(o.r.invalid_bytes == 0);
  }
  {  // invalid bytes of a replacement are counted even when no glyph is drawn for it.
    Font nf = make_font(2);
    nf.glyph_count -= 1;  // drop U+FFFD (the last glyph) so the replacement is skipped
    const Out o = run(nf, "\xff\xff", 0, 0, 100);
    REQUIRE(o.q.empty());
    REQUIRE(o.r.invalid_bytes == 2);
  }
}

TEST_CASE("golden layout digests", "[textlayout][digest]") {
  struct G {
    const char* name;
    std::string text;
    int font;
    Fixed max_width;
    int align;
    std::uint64_t digest;
    std::size_t quads, lines, invalid;
    Fixed width, height;
  };
  const G golden[] = {
      {"wrap three words", "aa bb cc dd ee", 0, 1280, 0, 0xac76238be54e36abull, 10, 5, 0, 768, 3200},
      {"center aligned lines", "aaaa\nbb\nW", 0, 1600, 1, 0x3042637fd56127dcull, 7, 3, 0, 1280, 1920},
      {"mixed utf8 with invalid bytes", std::string("A\xc3\xa9\xff\xc3" "B\xe2\x82"), 0, 0, 0, 0xba455699f4eaed56ull, 6, 1, 4, 2496, 640},
      {"random text, wrap and center", random_text(2024, 400), 0, 1280, 1, 0x166264f16bd558a8ull, 304, 142, 27, 1280, 90880},
      {"random text, right aligned", random_text(7, 600), 0, 0, 2, 0x56e7e8100528b5caull, 486, 21, 53, 49664, 13440},
      {"random text, fallback b", random_text(31337, 300), 3, 640, 0, 0x8678cc319442b56bull, 222, 221, 29, 1280, 141440},
  };
  for (const G& g : golden) {
    INFO(g.name);
    const Out o = run(make_font(g.font), g.text, g.max_width, g.align, 65536);
    REQUIRE(o.r.status == LayoutStatus::Ok);
    INFO("digest " << std::hex << digest_quads(o.q.data(), o.q.size()));
    REQUIRE(o.r.quad_count == g.quads);
    REQUIRE(o.r.line_count == g.lines);
    REQUIRE(o.r.invalid_bytes == g.invalid);
    REQUIRE(o.r.width == g.width);
    REQUIRE(o.r.height == g.height);
    REQUIRE(digest_quads(o.q.data(), o.q.size()) == g.digest);
  }
  // digest_quads of one known quad: FNV-1a 64 over 10 + 8 + 8 little-endian bytes.
  const Quad one{1, -2, 3, 4, 5, 6, 7, 8, 9, 10};
  REQUIRE(digest_quads(&one, 1) == 0x8c0b86b9e778c639ull);
  const Quad two[2] = {one, one};
  REQUIRE(digest_quads(two, 2) != digest_quads(&one, 1));
  REQUIRE(digest_quads(two, 1) == digest_quads(&one, 1));
}

TEST_CASE("1 MiB string layout (sanitized perf)", "[textlayout][perf]") {
  const std::string text = random_text(555, 0, 1048576).substr(0, 1048576);
  REQUIRE(text.size() == 1048576);
  const Font f = make_font(0);
  LayoutOptions o;
  o.max_width = 2000;
  o.align = Align::Center;
  o.max_glyphs = 2000000;
  const LayoutResult c = layout_text(f, text.data(), text.size(), o, nullptr, 0);
  REQUIRE(c.status == LayoutStatus::Ok);
  REQUIRE(c.quad_count > 400000);
  REQUIRE(c.line_count > 1000);
  REQUIRE(c.invalid_bytes > 0);
  std::vector<Quad> q(c.quad_count);  // large buffers on the heap
  const LayoutResult r = layout_text(f, text.data(), text.size(), o, q.data(), q.size());
  REQUIRE(r.status == LayoutStatus::Ok);
  REQUIRE(r.quad_count == c.quad_count);
  REQUIRE(r.line_count == c.line_count);
  REQUIRE(r.invalid_bytes == c.invalid_bytes);
  REQUIRE(r.width == c.width);
  REQUIRE(r.height == c.height);
  REQUIRE(r.height == static_cast<Fixed>(r.line_count * 640));
  // Quads come out in source order, inside their line and the block.
  std::uint32_t prev = 0;
  for (const Quad& x : q) {
    REQUIRE(x.source_offset >= prev);
    prev = x.source_offset;
    REQUIRE(x.x >= -64);
    REQUIRE(x.y >= 0);
    REQUIRE(x.y < r.height);
  }
  // A smaller buffer still reports the true totals.
  std::vector<Quad> small(1000);
  const LayoutResult s = layout_text(f, text.data(), text.size(), o, small.data(), small.size());
  REQUIRE(s.status == LayoutStatus::BufferTooSmall);
  REQUIRE(s.quad_count == c.quad_count);
  REQUIRE(digest_quads(small.data(), small.size()) == digest_quads(q.data(), small.size()));
}

namespace {

constexpr Fixed kMax = 2147483647;
constexpr Fixed kMin = -2147483647 - 1;

// A one-glyph font ('a' and 'b' with the same metrics, an optional space) for the overflow boundary tests.
struct Mini {
  std::vector<Glyph> g;
  std::vector<Kerning> k;
  Font f;
  Mini(Fixed advance, Fixed ox, Fixed oy, Fixed line_height, Fixed kern_aa = 0, bool has_kern = false, Fixed space_advance = -1) {
    if (space_advance >= 0) g.push_back(Glyph{0x20, 0, 0, 0, 0, 0, 0, space_advance});
    g.push_back(Glyph{0x61, 0, 0, 1, 1, ox, oy, advance});
    g.push_back(Glyph{0x62, 0, 0, 1, 1, ox, oy, advance});
    if (has_kern) k.push_back(Kerning{0x61, 0x61, kern_aa});
    f.glyphs = g.data();
    f.glyph_count = g.size();
    f.kerning = k.empty() ? nullptr : k.data();
    f.kerning_count = k.size();
    f.line_height = line_height;
    f.tex_width = 1;
    f.tex_height = 1;
  }
};

Out lay(const Font& f, const char* t, Fixed max_width = 0, int align = 0) { return run(f, t, max_width, align, 1000); }

}  // namespace

TEST_CASE("overflow: Quad.x saturates only at the final store", "[textlayout][overflow]") {
  REQUIRE(validate_font(Mini(1, 0, 0, 1).f) == FontStatus::Ok);
  {  // offset_x at INT32_MAX is exact, one above (pen 1) clamps.
    const Mini m(1, kMax, 0, 64);
    const Out o = lay(m.f, "aaa");
    REQUIRE(o.r.status == LayoutStatus::Ok);
    REQUIRE(o.q[0].x == kMax);
    REQUIRE(o.q[1].x == kMax);  // exact value INT32_MAX + 1
    REQUIRE(o.q[2].x == kMax);  // exact value INT32_MAX + 2
  }
  {  // x exactly INT32_MAX from the pen: advance INT32_MAX, second glyph.
    const Mini m(kMax, 0, 0, 64);
    const Out o = lay(m.f, "aaa");
    REQUIRE(o.q[0].x == 0);
    REQUIRE(o.q[1].x == kMax);  // exactly INT32_MAX
    REQUIRE(o.q[2].x == kMax);  // exact value 2 * INT32_MAX
  }
  {  // advance INT32_MAX - 1: second glyph is one below INT32_MAX, third clamps.
    const Mini m(kMax - 1, 0, 0, 64);
    const Out o = lay(m.f, "aaa");
    REQUIRE(o.q[1].x == kMax - 1);
    REQUIRE(o.q[2].x == kMax);
  }
  {  // negative edge: offset_x INT32_MIN is exact; INT32_MIN - 1 (offset -1 plus kerning INT32_MIN) clamps.
    const Mini a(0, kMin, 0, 64);
    REQUIRE(lay(a.f, "a").q[0].x == kMin);
    const Mini b(0, 0, 0, 64, kMin, true);
    const Out ob = lay(b.f, "aaa");
    REQUIRE(ob.q[0].x == 0);
    REQUIRE(ob.q[1].x == kMin);  // exactly INT32_MIN
    REQUIRE(ob.q[2].x == kMin);  // exact value 2 * INT32_MIN
    const Mini c(0, -1, 0, 64, kMin, true);
    const Out oc = lay(c.f, "aa");
    REQUIRE(oc.q[0].x == -1);
    REQUIRE(oc.q[1].x == kMin);  // exact value INT32_MIN - 1
    const Mini d(0, 1, 0, 64, kMin, true);
    REQUIRE(lay(d.f, "aa").q[1].x == kMin + 1);  // INT32_MIN + 1, not clamped
  }
  {  // kerning INT32_MIN with a large advance: pen = INT32_MAX + INT32_MIN = -1.
    const Mini m(kMax, 0, 0, 64, kMin, true);
    const Out o = lay(m.f, "aa");
    REQUIRE(o.q[1].x == -1);
  }
}

TEST_CASE("overflow: Quad.y, height and width boundaries", "[textlayout][overflow]") {
  const Fixed two30 = 1073741824;
  {  // y = line_index * line_height + offset_y crosses 2^31 on line 2.
    const Mini m(1, 0, 0, two30);
    const Out o = lay(m.f, "a\na\na\na");
    REQUIRE(o.q[0].y == 0);
    REQUIRE(o.q[1].y == two30);
    REQUIRE(o.q[2].y == kMax);  // exact value 2^31
    REQUIRE(o.q[3].y == kMax);  // exact value 3 * 2^30
    REQUIRE(o.r.line_count == 4);
    REQUIRE(o.r.height == kMax);
  }
  {  // y exactly INT32_MAX (2^31 - 1) and one above.
    const Mini m(1, 0, -1, two30);
    const Out o = lay(m.f, "a\na\na");
    REQUIRE(o.q[2].y == kMax);  // exactly 2^31 - 1
    const Mini n(1, 0, 0, two30 - 1);
    REQUIRE(lay(n.f, "a\na\na").q[2].y == 2147483646);  // 2 * (2^30 - 1), not clamped
    const Mini p(1, 0, 1, two30 - 1);
    REQUIRE(lay(p.f, "a\na\na").q[2].y == kMax);  // exactly 2^31 - 1
    const Mini q(1, 0, 2, two30 - 1);
    REQUIRE(lay(q.f, "a\na\na").q[2].y == kMax);  // exact value 2^31 (one above)
  }
  {  // offset_y at the negative edge.
    const Mini m(1, 0, kMin, kMax);
    const Out o = lay(m.f, "a\na");
    REQUIRE(o.q[0].y == kMin);
    REQUIRE(o.q[1].y == -1);  // INT32_MAX + INT32_MIN
  }
  {  // height boundaries: line_count * line_height.
    REQUIRE(lay(Mini(1, 0, 0, kMax).f, "a").r.height == kMax);                 // exactly INT32_MAX
    REQUIRE(lay(Mini(1, 0, 0, kMax).f, "a\na").r.height == kMax);              // exact value 2 * INT32_MAX
    REQUIRE(lay(Mini(1, 0, 0, two30 - 1).f, "a\na").r.height == 2147483646);   // exactly below
    REQUIRE(lay(Mini(1, 0, 0, two30).f, "a\na").r.height == kMax);             // exactly 2^31: one above
    REQUIRE(lay(Mini(1, 0, 0, 715827882).f, "a\na\na").r.height == 2147483646);
    REQUIRE(lay(Mini(1, 0, 0, 715827883).f, "a\na\na").r.height == kMax);      // 2147483649 clamps
  }
  {  // width boundaries.
    REQUIRE(lay(Mini(kMax, 0, 0, 64).f, "a").r.width == kMax);
    REQUIRE(lay(Mini(two30 - 1, 0, 0, 64).f, "aa").r.width == 2147483646);
    REQUIRE(lay(Mini(two30, 0, 0, 64).f, "aa").r.width == kMax);  // exactly 2^31
    REQUIRE(lay(Mini(kMax, 0, 0, 64).f, "aaa").r.width == kMax);
    REQUIRE(lay(Mini(0, 0, 0, 64, kMin, true).f, "aaa").r.width == 0);  // the widest line starts at 0, never below
  }
}

TEST_CASE("overflow: tab with a huge space advance", "[textlayout][overflow]") {
  // T = 4 * INT32_MAX = 8589934588 (above 2^32): exact in 64 bits.
  const Mini m(1, 0, 0, 64, 0, false, kMax);
  const Out o = lay(m.f, "a\tb\t\tb");
  REQUIRE(o.r.status == LayoutStatus::Ok);
  REQUIRE(o.q.size() == 3);
  REQUIRE(o.q[0].x == 0);
  REQUIRE(o.q[1].x == kMax);  // exact value T
  REQUIRE(o.q[2].x == kMax);  // exact value 3 * T
  REQUIRE(o.r.width == kMax);
  // With wrapping at max_width INT32_MAX the exact pen decides: the tab (pen T > max_width) starts its own line, kept because it
  // is the first non-space item of the line, and 'b' after it (T + 1) wraps again.
  const Out w = lay(m.f, "a\tb", kMax);
  REQUIRE(w.r.line_count == 3);
  REQUIRE(w.q.size() == 2);
  REQUIRE(w.q[1].x == 0);
  REQUIRE(w.q[1].y == 2 * 64 + 0);
}

TEST_CASE("overflow: saturation never changes wrapping or alignment", "[textlayout][overflow]") {
  const Fixed two30 = 1073741824;
  {  // Wrapping: the second glyph ends at exactly 2^31 > max_width INT32_MAX, so it must wrap. A clamped pen (INT32_MAX) would fit.
    const Mini m(two30, 0, 0, 64);
    const Out o = lay(m.f, "aaa", kMax);
    REQUIRE(o.r.line_count == 3);
    REQUIRE(o.q.size() == 3);
    for (const Quad& q : o.q) REQUIRE(q.x == 0);
    // One unit less advance: two glyphs end at 2^31 - 2, they fit together on one line.
    const Mini n(two30 - 1, 0, 0, 64);
    const Out p = lay(n.f, "aaa", kMax);
    REQUIRE(p.r.line_count == 2);
    REQUIRE(p.q[1].x == two30 - 1);
  }
  {  // Alignment: the widest line is 2^31 (the reported width clamps) but the block width stays exact, so the shorter line
     // is centred with the exact 2^31 and right aligned by the exact difference.
    const Mini m(two30, 0, 0, 64);
    const Out c = lay(m.f, "aa\na", 0, 1);
    REQUIRE(c.r.width == kMax);
    REQUIRE(c.q[2].x == 536870912);  // (2^31 - 2^30) / 2, not (INT32_MAX - 2^30) / 2 = 536870911
    const Out r = lay(m.f, "aa\na", 0, 2);
    REQUIRE(r.q[2].x == two30);  // 2^31 - 2^30, not 2^30 - 1
  }
  {  // Alignment with max_width: block width is max_width, line width 2^30: shift (INT32_MAX - 2^30) / 2 = 536870911 (floor).
    const Mini m(two30, 0, 0, 64);
    const Out c = lay(m.f, "a", kMax, 1);
    REQUIRE(c.q[0].x == 536870911);
    const Out r = lay(m.f, "a", kMax, 2);
    REQUIRE(r.q[0].x == two30 - 1);
  }
  {  // Truncation is not affected either: max_glyphs counts glyphs, whatever the positions.
    const Mini m(kMax, 0, 0, 64);
    const Out o = run(m.f, "aaaa", 0, 0, 3);
    REQUIRE(o.r.quad_count == 3);
    REQUIRE(o.r.truncated);
  }
}

namespace {
const std::vector<Case>& cases() {
  static const std::vector<Case> v = {
    {"kern_AV", 0, "\x41" "\x56", 2, 0, 0, 65536, {{64,128,384,512,1,1,6,8,65,0},{320,128,448,512,14,1,7,8,86,1}}, 1, 0, 768, 640, false},
    {"kern_AVA", 0, "\x41" "\x56" "\x41", 3, 0, 0, 65536, {{64,128,384,512,1,1,6,8,65,0},{320,128,448,512,14,1,7,8,86,1},{640,128,384,512,1,1,6,8,65,2}}, 1, 0, 1024, 640, false},
    {"kern_ab", 0, "\x61" "\x62", 2, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1}}, 1, 0, 768, 640, false},
    {"kern_space_A", 0, "\x20" "\x41", 2, 0, 0, 65536, {{320,128,384,512,1,1,6,8,65,1}}, 1, 0, 704, 640, false},
    {"kern_none_across_newline", 0, "\x41" "\x0a" "\x56", 3, 0, 0, 65536, {{64,128,384,512,1,1,6,8,65,0},{0,768,448,512,14,1,7,8,86,2}}, 2, 0, 448, 1280, false},
    {"kern_through_space", 0, "\x41" "\x20" "\x56", 3, 0, 0, 65536, {{64,128,384,512,1,1,6,8,65,0},{768,128,448,512,14,1,7,8,86,2}}, 1, 0, 1216, 640, false},
    {"fit_exact", 0, "\x61" "\x61" "\x61" "\x61", 4, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3}}, 1, 0, 1280, 640, false},
    {"fit_one_over", 0, "\x61" "\x61" "\x61" "\x61" "\x61", 5, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,256,320,1,10,4,5,97,4}}, 2, 0, 1280, 1280, false},
    {"wrap_space", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x61" "\x61" "\x61" "\x61", 9, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,256,320,1,10,4,5,97,5},{320,832,256,320,1,10,4,5,97,6},{640,832,256,320,1,10,4,5,97,7},{960,832,256,320,1,10,4,5,97,8}}, 2, 0, 1280, 1280, false},
    {"wrap_long_word", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61", 14, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,256,320,1,10,4,5,97,5},{320,832,256,320,1,10,4,5,97,6},{640,832,256,320,1,10,4,5,97,7},{960,832,256,320,1,10,4,5,97,8},{0,1472,256,320,1,10,4,5,97,9},{320,1472,256,320,1,10,4,5,97,10},{640,1472,256,320,1,10,4,5,97,11},{960,1472,256,320,1,10,4,5,97,12},{0,2112,256,320,1,10,4,5,97,13}}, 4, 0, 1280, 2560, false},
    {"wrap_double_space", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x20" "\x61" "\x61" "\x61" "\x61", 10, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,256,320,1,10,4,5,97,6},{320,832,256,320,1,10,4,5,97,7},{640,832,256,320,1,10,4,5,97,8},{960,832,256,320,1,10,4,5,97,9}}, 2, 0, 1280, 1280, false},
    {"wrap_trailing_space_fit", 0, "\x61" "\x61" "\x61" "\x61" "\x20", 5, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3}}, 1, 0, 1280, 640, false},
    {"wrap_spaces_only", 0, "\x20" "\x20" "\x20" "\x20" "\x20", 5, 1280, 0, 65536, {}, 1, 0, 0, 640, false},
    {"wrap_leading_spaces", 0, "\x20" "\x20" "\x61" "\x61" "\x61" "\x61", 6, 1280, 0, 65536, {{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,256,320,1,10,4,5,97,4},{320,832,256,320,1,10,4,5,97,5}}, 2, 0, 1280, 1280, false},
    {"wrap_leading_then_long", 0, "\x20" "\x20" "\x20" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61", 11, 1280, 0, 65536, {{960,192,256,320,1,10,4,5,97,3},{0,832,256,320,1,10,4,5,97,4},{320,832,256,320,1,10,4,5,97,5},{640,832,256,320,1,10,4,5,97,6},{960,832,256,320,1,10,4,5,97,7},{0,1472,256,320,1,10,4,5,97,8},{320,1472,256,320,1,10,4,5,97,9},{640,1472,256,320,1,10,4,5,97,10}}, 3, 0, 1280, 1920, false},
    {"wrap_three_words", 0, "\x61" "\x61" "\x20" "\x62" "\x62" "\x20" "\x63" "\x63" "\x20" "\x64" "\x64" "\x20" "\x65" "\x65", 14, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{0,832,320,320,6,10,5,5,98,3},{384,832,320,320,6,10,5,5,98,4},{0,1472,320,320,12,10,5,5,99,6},{384,1472,320,320,12,10,5,5,99,7},{0,2112,320,320,18,10,5,5,100,9},{384,2112,320,320,18,10,5,5,100,10},{0,2752,256,320,24,10,4,5,101,12},{320,2752,256,320,24,10,4,5,101,13}}, 5, 0, 768, 3200, false},
    {"wrap_narrow", 0, "\x61" "\x62" "\x63", 3, 64, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{0,832,320,320,6,10,5,5,98,1},{0,1472,320,320,12,10,5,5,99,2}}, 3, 0, 384, 1920, false},
    {"wrap_narrow_words", 0, "\x61" "\x20" "\x62" "\x20" "\x63", 5, 64, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{0,832,320,320,6,10,5,5,98,2},{0,1472,320,320,12,10,5,5,99,4}}, 3, 0, 384, 1920, false},
    {"wrap_newlines", 0, "\x61" "\x61" "\x0a" "\x0a" "\x61" "\x61" "\x0a", 7, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{0,1472,256,320,1,10,4,5,97,4},{320,1472,256,320,1,10,4,5,97,5}}, 4, 0, 640, 2560, false},
    {"tab_basic", 0, "\x61" "\x09" "\x62", 3, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{1280,192,320,320,6,10,5,5,98,2}}, 1, 0, 1664, 640, false},
    {"tab_second", 0, "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x61" "\x09" "\x62", 19, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{1280,192,256,320,1,10,4,5,97,4},{1600,192,256,320,1,10,4,5,97,5},{1920,192,256,320,1,10,4,5,97,6},{2240,192,256,320,1,10,4,5,97,7},{2560,192,256,320,1,10,4,5,97,8},{2880,192,256,320,1,10,4,5,97,9},{3200,192,256,320,1,10,4,5,97,10},{3520,192,256,320,1,10,4,5,97,11},{3840,192,256,320,1,10,4,5,97,12},{4160,192,256,320,1,10,4,5,97,13},{4480,192,256,320,1,10,4,5,97,14},{4800,192,256,320,1,10,4,5,97,15},{5120,192,256,320,1,10,4,5,97,16},{6400,192,320,320,6,10,5,5,98,18}}, 1, 0, 6784, 640, false},
    {"tab_at_stop", 0, "\x61" "\x61" "\x61" "\x61" "\x09" "\x62", 6, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{2560,192,320,320,6,10,5,5,98,5}}, 1, 0, 2944, 640, false},
    {"tab_wrap", 0, "\x61" "\x61" "\x09" "\x61" "\x61" "\x09" "\x61", 7, 1280, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{0,832,256,320,1,10,4,5,97,3},{320,832,256,320,1,10,4,5,97,4},{0,1472,256,320,1,10,4,5,97,6}}, 3, 0, 1280, 1920, false},
    {"tab_no_space", 2, "\x61" "\x09" "\x62", 3, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,320,320,6,10,5,5,98,2}}, 1, 0, 704, 640, false},
    {"cr_ignored", 0, "\x61" "\x0d" "\x0a" "\x62" "\x0d", 5, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{0,832,320,320,6,10,5,5,98,3}}, 2, 0, 384, 1280, false},
    {"controls", 0, "\x61" "\x01" "\x07" "\x62" "\x1f" "\x63" "\x7f", 7, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,3},{768,192,320,320,12,10,5,5,99,5},{1152,128,384,512,40,1,6,8,65533,6}}, 1, 0, 1600, 640, false},
    {"align_left", 0, "\x61" "\x61" "\x61" "\x61" "\x0a" "\x62" "\x62" "\x0a" "\x57", 9, 1600, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,320,320,6,10,5,5,98,5},{384,832,320,320,6,10,5,5,98,6},{0,1408,640,512,22,1,10,8,87,8}}, 3, 0, 1280, 1920, false},
    {"align_center", 0, "\x61" "\x61" "\x61" "\x61" "\x0a" "\x62" "\x62" "\x0a" "\x57", 9, 1600, 1, 65536, {{160,192,256,320,1,10,4,5,97,0},{480,192,256,320,1,10,4,5,97,1},{800,192,256,320,1,10,4,5,97,2},{1120,192,256,320,1,10,4,5,97,3},{416,832,320,320,6,10,5,5,98,5},{800,832,320,320,6,10,5,5,98,6},{448,1408,640,512,22,1,10,8,87,8}}, 3, 0, 1280, 1920, false},
    {"align_right", 0, "\x61" "\x61" "\x61" "\x61" "\x0a" "\x62" "\x62" "\x0a" "\x57", 9, 1600, 2, 65536, {{320,192,256,320,1,10,4,5,97,0},{640,192,256,320,1,10,4,5,97,1},{960,192,256,320,1,10,4,5,97,2},{1280,192,256,320,1,10,4,5,97,3},{832,832,320,320,6,10,5,5,98,5},{1216,832,320,320,6,10,5,5,98,6},{896,1408,640,512,22,1,10,8,87,8}}, 3, 0, 1280, 1920, false},
    {"align_center_nowrap", 0, "\x61" "\x61" "\x61" "\x61" "\x0a" "\x62" "\x62" "\x0a" "\x57", 9, 0, 1, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{256,832,320,320,6,10,5,5,98,5},{640,832,320,320,6,10,5,5,98,6},{288,1408,640,512,22,1,10,8,87,8}}, 3, 0, 1280, 1920, false},
    {"align_right_nowrap", 0, "\x61" "\x61" "\x61" "\x61" "\x0a" "\x62" "\x62" "\x0a" "\x57", 9, 0, 2, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{512,832,320,320,6,10,5,5,98,5},{896,832,320,320,6,10,5,5,98,6},{576,1408,640,512,22,1,10,8,87,8}}, 3, 0, 1280, 1920, false},
    {"align_odd", 0, "\x61" "\x61" "\x61" "\x0a" "\x62" "\x0a" "\x69", 7, 0, 1, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{288,832,320,320,6,10,5,5,98,4},{480,1472,64,320,30,10,1,5,105,6}}, 3, 0, 960, 1920, false},
    {"align_wide_char", 0, "\x57" "\x20" "\x61" "\x61" "\x61" "\x61", 6, 320, 2, 65536, {{0,128,640,512,22,1,10,8,87,0},{0,832,256,320,1,10,4,5,97,2},{0,1472,256,320,1,10,4,5,97,3},{0,2112,256,320,1,10,4,5,97,4},{0,2752,256,320,1,10,4,5,97,5}}, 5, 0, 704, 3200, false},
    {"missing_fallback", 0, "\x61" "\xc3" "\xa8" "\x62", 4, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,128,384,512,40,1,6,8,65533,1},{768,192,320,320,6,10,5,5,98,3}}, 1, 0, 1152, 640, false},
    {"missing_fallback_present", 0, "\x61" "\xc3" "\xa9" "\x62" "\xe2" "\x82" "\xac" "\x62", 8, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,64,256,448,33,10,4,7,233,1},{640,192,320,320,6,10,5,5,98,3},{1024,128,384,512,40,1,6,8,65533,4},{1472,192,320,320,6,10,5,5,98,7}}, 1, 0, 1856, 640, false},
    {"missing_no_fallback", 1, "\x61" "\xe2" "\x82" "\xac" "\x62", 5, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,4}}, 1, 0, 768, 640, false},
    {"missing_other_fallback", 3, "\x61" "\xe2" "\x82" "\xac" "\x62", 5, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1},{768,192,320,320,6,10,5,5,98,4}}, 1, 0, 1152, 640, false},
    {"space_missing_glyph", 2, "\x61" "\x20" "\x62", 3, 0, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{320,192,320,320,6,10,5,5,98,2}}, 1, 0, 704, 640, false},
    {"max_glyphs_cut", 0, "\x61" "\x62" "\x63" "\x64", 4, 0, 0, 2, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1}}, 1, 0, 768, 640, true},
    {"max_glyphs_exact", 0, "\x61" "\x62" "\x63" "\x64", 4, 0, 0, 4, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1},{768,192,320,320,12,10,5,5,99,2},{1152,192,320,320,18,10,5,5,100,3}}, 1, 0, 1536, 640, false},
    {"max_glyphs_one", 0, "\x61" "\x62" "\x63" "\x64", 4, 0, 0, 1, {{0,192,256,320,1,10,4,5,97,0}}, 1, 0, 320, 640, true},
    {"max_glyphs_skip_spaces", 0, "\x61" "\x62" "\x20" "\x20", 4, 0, 0, 2, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1}}, 1, 0, 768, 640, false},
    {"max_glyphs_cut_lines", 0, "\x61" "\x62" "\x0a" "\x63" "\x64" "\x0a" "\x61" "\x65", 8, 0, 0, 3, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1},{0,832,320,320,12,10,5,5,99,3}}, 2, 0, 768, 1280, true},
    {"trunc_in_word", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x62" "\x62" "\x62" "\x62", 9, 1280, 0, 6, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3},{0,832,320,320,6,10,5,5,98,5},{384,832,320,320,6,10,5,5,98,6}}, 2, 0, 1280, 1280, true},
    {"trunc_at_wrap_point", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x62" "\x62" "\x62" "\x62", 9, 1280, 0, 4, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3}}, 1, 0, 1280, 640, true},
    {"trunc_exact_count", 0, "\x61" "\x61" "\x61" "\x61", 4, 1280, 0, 4, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3}}, 1, 0, 1280, 640, false},
    {"trunc_exact_then_spaces", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x20", 6, 1280, 0, 4, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3}}, 1, 0, 1280, 640, false},
    {"trunc_exact_then_newline", 0, "\x61" "\x61" "\x61" "\x61" "\x0a", 5, 0, 0, 4, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1},{640,192,256,320,1,10,4,5,97,2},{960,192,256,320,1,10,4,5,97,3}}, 2, 0, 1280, 1280, false},
    {"trunc_then_controls_then_glyph", 0, "\x61" "\x61" "\x01" "\x0d" "\x20" "\x65", 6, 0, 0, 2, {{0,192,256,320,1,10,4,5,97,0},{320,192,256,320,1,10,4,5,97,1}}, 1, 0, 640, 640, true},
    {"trunc_prefix_differs", 0, "\x61" "\x62" "\x20" "\x63" "\x64", 5, 1472, 0, 3, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1},{1088,192,320,320,12,10,5,5,99,3}}, 1, 0, 1472, 640, true},
    {"trunc_full_for_comparison", 0, "\x61" "\x62" "\x20" "\x63" "\x64", 5, 1472, 0, 65536, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1},{0,832,320,320,12,10,5,5,99,3},{384,832,320,320,18,10,5,5,100,4}}, 2, 0, 768, 1280, false},
    {"trunc_last_line_center", 0, "\x61" "\x61" "\x61" "\x61" "\x0a" "\x62" "\x62" "\x0a" "\x63" "\x63" "\x63" "\x63", 12, 1600, 1, 6, {{160,192,256,320,1,10,4,5,97,0},{480,192,256,320,1,10,4,5,97,1},{800,192,256,320,1,10,4,5,97,2},{1120,192,256,320,1,10,4,5,97,3},{416,832,320,320,6,10,5,5,98,5},{800,832,320,320,6,10,5,5,98,6}}, 2, 0, 1280, 1280, true},
    {"trunc_last_line_right", 0, "\x61" "\x61" "\x61" "\x61" "\x20" "\x62" "\x62" "\x20" "\x63" "\x63", 10, 1600, 2, 6, {{320,192,256,320,1,10,4,5,97,0},{640,192,256,320,1,10,4,5,97,1},{960,192,256,320,1,10,4,5,97,2},{1280,192,256,320,1,10,4,5,97,3},{832,832,320,320,6,10,5,5,98,5},{1216,832,320,320,6,10,5,5,98,6}}, 2, 0, 1280, 1280, true},
    {"trunc_before_invalid", 0, "\x61" "\x62" "\xff", 3, 0, 0, 2, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1}}, 1, 0, 768, 640, true},
    {"trunc_after_invalid", 0, "\x61" "\x62" "\xff" "\x63", 4, 0, 0, 3, {{0,192,256,320,1,10,4,5,97,0},{384,192,320,320,6,10,5,5,98,1},{768,128,384,512,40,1,6,8,65533,2}}, 1, 1, 1216, 640, true},
    {"trunc_utf8_boundary", 0, "\x61" "\xc3" "\xa9" "\xe2" "\x82" "\xac" "\x62", 7, 0, 0, 2, {{0,192,256,320,1,10,4,5,97,0},{320,64,256,448,33,10,4,7,233,1}}, 1, 0, 640, 640, true},
    {"empty", 0, "", 0, 0, 0, 65536, {}, 0, 0, 0, 0, false},
    {"only_newline", 0, "\x0a", 1, 0, 0, 65536, {}, 2, 0, 0, 1280, false},
    {"only_cr", 0, "\x0d", 1, 0, 0, 65536, {}, 1, 0, 0, 640, false},
    {"only_skipped", 1, "\xe2" "\x82" "\xac", 3, 0, 0, 65536, {}, 1, 0, 0, 640, false},
    {"mixed_utf8", 0, "\x41" "\xc3" "\xa9" "\xff" "\xc3" "\x42" "\xe2" "\x82", 8, 0, 0, 65536, {{64,128,384,512,1,1,6,8,65,0},{448,64,256,448,33,10,4,7,233,1},{768,128,384,512,40,1,6,8,65533,3},{1216,128,384,512,40,1,6,8,65533,4},{1664,128,320,512,8,1,5,8,66,5},{2048,128,384,512,40,1,6,8,65533,6}}, 1, 4, 2496, 640, false},

  };
  return v;
}
}  // namespace
