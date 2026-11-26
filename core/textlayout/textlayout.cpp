// Deterministic bitmap text layout. Rules, operation order, overflow policy and golden digests: docs/textlayout.md.
// Determinism: integer arithmetic only (64-bit intermediates, no floating point anywhere), binary search over sorted
// arrays, plain vectors, no unordered containers, no pointer values.
#include "textlayout.h"

#include <vector>

namespace lumora::textlayout {
namespace {

constexpr std::int64_t kFixedMax = 2147483647;
constexpr std::int64_t kFixedMin = -2147483647 - 1;

// Overflow policy (documented): layout decisions use the exact 64-bit values; only the stored x, y, width and height are
// clamped to the int32 range, at the final store.
Fixed clamp32(std::int64_t v) {
  if (v > kFixedMax) return static_cast<Fixed>(kFixedMax);
  if (v < kFixedMin) return static_cast<Fixed>(kFixedMin);
  return static_cast<Fixed>(v);
}

// Input limit: at most 2^30 bytes are laid out (see docs/textlayout.md, "Input limit and the 64-bit bound"). This keeps the
// number of characters, and so every intermediate value, below 2^63 for any int32 font metrics.
constexpr std::size_t kMaxInputBytes = static_cast<std::size_t>(1) << 30;

bool is_scalar(std::uint32_t c) { return c <= 0x10FFFFu && !(c >= 0xD800u && c <= 0xDFFFu); }

// Index of the glyph with this codepoint, or glyph_count when missing.
std::size_t find_glyph(const Font& f, std::uint32_t cp) {
  std::size_t lo = 0, hi = f.glyph_count;
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    if (f.glyphs[mid].codepoint < cp) lo = mid + 1;
    else hi = mid;
  }
  return (lo < f.glyph_count && f.glyphs[lo].codepoint == cp) ? lo : f.glyph_count;
}

std::int64_t find_kerning(const Font& f, std::uint32_t a, std::uint32_t b) {
  std::size_t lo = 0, hi = f.kerning_count;
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    const Kerning& k = f.kerning[mid];
    if (k.first < a || (k.first == a && k.second < b)) lo = mid + 1;
    else hi = mid;
  }
  if (lo < f.kerning_count && f.kerning[lo].first == a && f.kerning[lo].second == b) return f.kerning[lo].amount;
  return 0;
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {  // b > 0
  std::int64_t q = a / b;
  if ((a % b != 0) && (a < 0)) --q;
  return q;
}

enum Kind : std::uint8_t { kGlyph = 0, kSpace = 1, kTab = 2, kNewline = 3 };

struct Item {
  Kind kind;
  std::uint32_t cp;     // drawn codepoint (for a space: 0x20)
  std::size_t glyph;    // glyph index, or glyph_count when there is none (a space without a glyph)
  std::size_t source;   // byte offset
};

struct Line {
  std::size_t begin, end;  // items [begin, end)
  std::int64_t width;
};

// Pen state of one line. kNone means no previous character for kerning.
constexpr std::int64_t kNone = -1;
struct Pen {
  std::int64_t pen = 0;
  std::int64_t prev = kNone;
};

struct Metrics {
  const Font& font;
  std::int64_t tab_width;  // 4 * space advance, or 0
};

// Moves the pen over item `it`. Returns the pen before the glyph box (after kerning) in `start`.
void advance_pen(const Metrics& m, const Item& it, Pen& p, std::int64_t& start) {
  if (it.kind == kTab) {
    if (m.tab_width > 0) p.pen = (floor_div(p.pen, m.tab_width) + 1) * m.tab_width;
    p.prev = kNone;
    start = p.pen;
    return;
  }
  if (p.prev != kNone) p.pen += find_kerning(m.font, static_cast<std::uint32_t>(p.prev), it.cp);
  start = p.pen;
  if (it.glyph < m.font.glyph_count) p.pen += m.font.glyphs[it.glyph].advance;
  p.prev = static_cast<std::int64_t>(it.cp);
}

// Strict UTF-8 decoding. Returns the codepoint (0xFFFD for an invalid sequence) and sets `used` (>= 1) and `bad`.
std::uint32_t decode(const unsigned char* s, std::size_t n, std::size_t i, std::size_t& used, bool& bad) {
  const unsigned b0 = s[i];
  bad = false;
  used = 1;
  if (b0 < 0x80) return b0;
  unsigned need = 0, lo = 0x80, hi = 0xBF;
  std::uint32_t cp = 0;
  if (b0 >= 0xC2 && b0 <= 0xDF) {
    need = 1;
    cp = b0 & 0x1Fu;
  } else if (b0 >= 0xE0 && b0 <= 0xEF) {
    need = 2;
    cp = b0 & 0x0Fu;
    if (b0 == 0xE0) lo = 0xA0;
    if (b0 == 0xED) hi = 0x9F;
  } else if (b0 >= 0xF0 && b0 <= 0xF4) {
    need = 3;
    cp = b0 & 0x07u;
    if (b0 == 0xF0) lo = 0x90;
    if (b0 == 0xF4) hi = 0x8F;
  } else {
    bad = true;  // continuation byte, C0, C1, F5..FF
    return 0xFFFDu;
  }
  for (unsigned k = 1; k <= need; ++k) {
    if (i + k >= n) {
      bad = true;
      used = k;
      return 0xFFFDu;
    }
    const unsigned b = s[i + k];
    if (b < lo || b > hi) {
      bad = true;
      used = k;
      return 0xFFFDu;
    }
    lo = 0x80;
    hi = 0xBF;
    cp = (cp << 6) | (b & 0x3Fu);
  }
  used = need + 1;
  return cp;
}

}  // namespace

const char* font_status_text(FontStatus s) {
  switch (s) {
    case FontStatus::Ok: return "ok";
    case FontStatus::BadMetrics: return "bad metrics";
    case FontStatus::BadGlyphOrder: return "bad glyph order";
    case FontStatus::BadGlyphRect: return "bad glyph rectangle";
    case FontStatus::BadKerningOrder: return "bad kerning order";
    case FontStatus::NullData: return "null data";
  }
  return "unknown font status";
}

const char* layout_status_text(LayoutStatus s) {
  switch (s) {
    case LayoutStatus::Ok: return "ok";
    case LayoutStatus::BadFont: return "bad font";
    case LayoutStatus::BadOptions: return "bad options";
    case LayoutStatus::BufferTooSmall: return "buffer too small";
  }
  return "unknown layout status";
}

FontStatus validate_font(const Font& f) {
  const bool glyphs_ok = f.glyphs != nullptr || f.glyph_count == 0;
  const bool kerning_ok = f.kerning != nullptr || f.kerning_count == 0;
  // Arrays behind a null pointer are never read; NullData is reported in enum order, last.
  if (f.line_height <= 0 || f.tex_width == 0 || f.tex_height == 0) return FontStatus::BadMetrics;
  if (glyphs_ok) {
    for (std::size_t i = 0; i < f.glyph_count; ++i) {
      if (f.glyphs[i].advance < 0) return FontStatus::BadMetrics;
    }
    for (std::size_t i = 0; i < f.glyph_count; ++i) {
      if (!is_scalar(f.glyphs[i].codepoint)) return FontStatus::BadGlyphOrder;
      if (i > 0 && f.glyphs[i].codepoint <= f.glyphs[i - 1].codepoint) return FontStatus::BadGlyphOrder;
    }
    for (std::size_t i = 0; i < f.glyph_count; ++i) {
      const Glyph& g = f.glyphs[i];
      if (static_cast<std::uint32_t>(g.tex_x) + g.tex_w > f.tex_width || static_cast<std::uint32_t>(g.tex_y) + g.tex_h > f.tex_height)
        return FontStatus::BadGlyphRect;
    }
  }
  if (kerning_ok) {
    for (std::size_t i = 1; i < f.kerning_count; ++i) {
      const Kerning& a = f.kerning[i - 1];
      const Kerning& b = f.kerning[i];
      if (b.first < a.first || (b.first == a.first && b.second <= a.second)) return FontStatus::BadKerningOrder;
    }
  }
  if (!glyphs_ok || !kerning_ok) return FontStatus::NullData;
  return FontStatus::Ok;
}

LayoutResult layout_text(const Font& font, const char* utf8, std::size_t length, const LayoutOptions& opt, Quad* out_quads,
                         std::size_t capacity) {
  LayoutResult res;
  if (validate_font(font) != FontStatus::Ok) {
    res.status = LayoutStatus::BadFont;
    return res;
  }
  if (opt.max_width < 0 || opt.max_glyphs == 0 || static_cast<std::uint8_t>(opt.align) > 2) {
    res.status = LayoutStatus::BadOptions;
    return res;
  }
  if (utf8 == nullptr) length = 0;
  if (length == 0) return res;

  // Step 1: decode and classify. Controls and unmappable characters are dropped here. max_glyphs cuts the text.
  std::vector<Item> items;
  items.reserve(length < 1024 ? length : 1024);
  const unsigned char* s = reinterpret_cast<const unsigned char*>(utf8);
  const std::size_t space_glyph = find_glyph(font, 0x20);
  std::size_t quads = 0, cut_items = 0, cut_invalid = 0;
  for (std::size_t i = 0; i < length;) {
    std::size_t used = 1;
    bool bad = false;
    const std::uint32_t cp = decode(s, length, i, used, bad);
    if (i + used > kMaxInputBytes) {  // a character that does not end within the first 2^30 bytes: the text ends before it
      res.truncated = true;
      break;
    }
    Item it{kGlyph, cp, font.glyph_count, i};
    bool keep = true;
    if (!bad && cp == '\r') {
      keep = false;
    } else if (!bad && cp == '\n') {
      it.kind = kNewline;
    } else if (!bad && cp == '\t') {
      it.kind = kTab;
    } else if (!bad && cp < 0x20) {
      keep = false;
    } else if (!bad && cp == 0x20) {
      it.kind = kSpace;
      it.glyph = space_glyph;
    } else {
      std::size_t g = find_glyph(font, cp);
      if (g == font.glyph_count) {
        it.cp = font.fallback;
        g = find_glyph(font, font.fallback);
      }
      if (g == font.glyph_count) keep = false;  // neither the glyph nor the fallback exists: skipped
      it.glyph = g;
    }
    if (keep && it.kind == kGlyph) {
      if (quads == opt.max_glyphs) {
        // The text ends right after the last glyph that was kept: drop what followed it (spaces, newlines).
        res.truncated = true;
        items.resize(cut_items);
        res.invalid_bytes = cut_invalid;
        break;
      }
      ++quads;
    }
    if (bad) res.invalid_bytes += used;
    if (keep) items.push_back(it);
    if (keep && it.kind == kGlyph && quads == opt.max_glyphs) {
      cut_items = items.size();
      cut_invalid = res.invalid_bytes;
    }
    i += used;
  }
  res.quad_count = quads;

  // Step 2: find the lines (greedy wrapping on exact 64-bit pen positions).
  Metrics m{font, space_glyph < font.glyph_count ? 4 * static_cast<std::int64_t>(font.glyphs[space_glyph].advance) : 0};
  const std::int64_t max_w = opt.max_width;
  std::vector<Line> lines;
  const std::size_t n = items.size();
  std::size_t start = 0;
  for (;;) {
    Pen p;
    std::int64_t width = 0;
    bool content = false, in_space_run = false, have_cand = false;
    std::size_t cand_end = 0, cand_next = 0;
    std::int64_t cand_width = 0;
    std::size_t j = start;
    Line line{start, n, 0};
    std::size_t next = n;
    bool more = false;  // another line follows
    for (; j < n; ++j) {
      const Item& it = items[j];
      if (it.kind == kNewline) {
        line.end = j;
        line.width = width;
        next = j + 1;
        more = true;
        break;
      }
      Pen q = p;
      std::int64_t x0 = 0;
      advance_pen(m, it, q, x0);
      if (it.kind == kSpace) {
        if (content && !in_space_run) {
          in_space_run = true;
          have_cand = true;
          cand_end = j;
          cand_width = width;
          cand_next = n;  // set at the next non-space item
        }
        p = q;
        continue;
      }
      if (max_w > 0 && q.pen > max_w) {
        if (have_cand) {
          if (in_space_run) cand_next = j;
          line.end = cand_end;
          line.width = cand_width;
          next = cand_next;
          more = true;
          break;
        }
        if (content) {
          line.end = j;
          line.width = width;
          next = j;
          more = true;
          break;
        }
        // first non-space item of the line: kept even if it overflows (at least one character per line)
      }
      if (in_space_run) {
        in_space_run = false;
        cand_next = j;
      }
      p = q;
      width = p.pen;
      content = true;
    }
    if (!more) {
      line.end = n;
      line.width = width;
    }
    lines.push_back(line);
    if (!more) break;
    start = next;
  }
  res.line_count = lines.size();

  std::int64_t widest = 0;
  for (const Line& l : lines) {
    if (l.width > widest) widest = l.width;
  }
  const std::int64_t block_w = max_w > 0 ? max_w : widest;
  res.width = clamp32(widest);
  res.height = clamp32(static_cast<std::int64_t>(lines.size()) * font.line_height);

  // Step 3: emit quads.
  std::size_t written = 0;
  if (out_quads != nullptr && capacity < quads) res.status = LayoutStatus::BufferTooSmall;
  for (std::size_t li = 0; li < lines.size() && out_quads != nullptr && written < capacity; ++li) {
    const Line& l = lines[li];
    std::int64_t shift = 0;
    if (opt.align != Align::Left && block_w > l.width) {
      const std::int64_t d = block_w - l.width;
      shift = opt.align == Align::Center ? d / 2 : d;
    }
    const std::int64_t y_base = static_cast<std::int64_t>(li) * font.line_height;
    Pen p;
    for (std::size_t j = l.begin; j < l.end && written < capacity; ++j) {
      const Item& it = items[j];
      std::int64_t x0 = 0;
      advance_pen(m, it, p, x0);
      if (it.kind != kGlyph) continue;
      const Glyph& g = font.glyphs[it.glyph];
      Quad& q = out_quads[written++];
      q.x = clamp32(x0 + g.offset_x + shift);
      q.y = clamp32(y_base + g.offset_y);
      q.w = static_cast<Fixed>(static_cast<std::int32_t>(g.tex_w) * kOne);
      q.h = static_cast<Fixed>(static_cast<std::int32_t>(g.tex_h) * kOne);
      q.tex_x = g.tex_x;
      q.tex_y = g.tex_y;
      q.tex_w = g.tex_w;
      q.tex_h = g.tex_h;
      q.codepoint = it.cp;
      q.source_offset = static_cast<std::uint32_t>(it.source);  // < 2^30, so it always fits
    }
  }
  return res;
}

std::uint64_t digest_quads(const Quad* q, std::size_t n) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  auto put = [&h](std::uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      h ^= (v >> (8 * i)) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  if (q == nullptr) return h;
  for (std::size_t i = 0; i < n; ++i) {
    put(static_cast<std::uint32_t>(q[i].x), 4);
    put(static_cast<std::uint32_t>(q[i].y), 4);
    put(static_cast<std::uint32_t>(q[i].w), 4);
    put(static_cast<std::uint32_t>(q[i].h), 4);
    put(q[i].tex_x, 2);
    put(q[i].tex_y, 2);
    put(q[i].tex_w, 2);
    put(q[i].tex_h, 2);
    put(q[i].codepoint, 4);
    put(q[i].source_offset, 4);
  }
  return h;
}

}  // namespace lumora::textlayout
