// Lumora bitmap text layout (scope N, M4 text groundwork). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/textlayout/textlayout.cpp only.
// It turns a UTF-8 string and a bitmap font description into positioned glyph quads that a renderer or the sprite
// batcher can draw. Results must be identical on Linux, Windows and Android: arithmetic in 26.6-style integers
// (see Fixed below), no floats in any layout decision, no unordered containers, no constant division by zero or
// constant overflow (MSVC rejects those at compile time).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::textlayout {

// Fixed point with 6 fractional bits (1/64 pixel), stored in int32. All font metrics and all output positions use it.
using Fixed = std::int32_t;
constexpr Fixed kOne = 64;

struct Glyph {
  std::uint32_t codepoint;   // Unicode scalar value, strictly ascending across the font's glyph array
  std::uint16_t tex_x, tex_y, tex_w, tex_h;  // pixel rectangle in the font texture
  Fixed offset_x, offset_y;  // draw offset of the quad's top-left from the pen position (y down from the line top)
  Fixed advance;             // pen advance after this glyph, >= 0
};

struct Kerning {
  std::uint32_t first, second;  // codepoints; pairs strictly ascending by (first, second)
  Fixed amount;                 // added to the pen between the two glyphs (usually negative)
};

struct Font {
  const Glyph* glyphs = nullptr;
  std::size_t glyph_count = 0;
  const Kerning* kerning = nullptr;
  std::size_t kerning_count = 0;
  Fixed line_height = 0;       // > 0
  std::uint16_t tex_width = 0; // font texture size in pixels, > 0
  std::uint16_t tex_height = 0;
  std::uint32_t fallback = 0xFFFD;  // codepoint drawn for a missing glyph; if it is missing too the character is skipped
};

enum class FontStatus : std::uint8_t {
  Ok = 0,
  BadMetrics,        // line_height <= 0, tex size 0, or a glyph advance < 0
  BadGlyphOrder,     // glyph codepoints not strictly ascending, or a codepoint is not a Unicode scalar value
  BadGlyphRect,      // a glyph rectangle exceeds the texture
  BadKerningOrder,   // kerning pairs not strictly ascending by (first, second)
  NullData,          // a non-zero count with a null pointer
};
const char* font_status_text(FontStatus s);
FontStatus validate_font(const Font& f);  // checks in enum order

enum class Align : std::uint8_t { Left = 0, Center = 1, Right = 2 };

struct LayoutOptions {
  Fixed max_width = 0;   // 0 = no wrapping; otherwise lines wrap at spaces (U+0020) or are broken inside a word if one
                         // word is wider than max_width
  Align align = Align::Left;
  std::uint32_t max_glyphs = 65536;  // truncation: the text is cut right after the max_glyphs-th drawn glyph and laid out
                                     // as if the input ended there (wrapping, alignment, line_count, width, height all
                                     // describe that prefix). truncated is true only if a further drawn glyph would follow.
};

struct Quad {
  Fixed x, y, w, h;              // destination rectangle in 1/64 pixel, origin at the top-left of the text block
  std::uint16_t tex_x, tex_y, tex_w, tex_h;  // source rectangle in the font texture (copied from the glyph)
  std::uint32_t codepoint;       // the drawn codepoint (the fallback's, for a substituted glyph)
  std::uint32_t source_offset;   // byte offset of the character in the input string
};

enum class LayoutStatus : std::uint8_t {
  Ok = 0,
  BadFont,        // validate_font failed
  BadOptions,     // max_width < 0, max_glyphs 0, or an invalid align value
  BufferTooSmall, // out_quads capacity smaller than the glyph count; counts below are still the true totals
};
const char* layout_status_text(LayoutStatus s);

struct LayoutResult {
  LayoutStatus status = LayoutStatus::Ok;
  std::size_t quad_count = 0;      // drawn glyph quads (spaces and control characters produce no quad)
  std::size_t line_count = 0;      // lines in the block, at least 1 for non-empty text
  std::size_t invalid_bytes = 0;   // bytes replaced because the UTF-8 was invalid (each maximal bad sequence counts its bytes)
  Fixed width = 0;                 // widest line, after kerning, without trailing spaces
  Fixed height = 0;                // line_count * line_height
  bool truncated = false;          // stopped at max_glyphs
};

// Layout rules (documented in detail by the contributor in docs/textlayout.md; the following are fixed):
// - UTF-8 decoding is strict: overlong forms, surrogates, values above U+10FFFF and truncated sequences are invalid.
//   An invalid maximal subpart is replaced by one U+FFFD character (Unicode "maximal subpart" practice) and its byte
//   count is added to invalid_bytes.
// - '\n' ends a line. '\r' is ignored. '\t' advances to the next multiple of 4 * (advance of space), or 4 * 0 if the font
//   has no space glyph. Other control characters (< U+0020) are skipped.
// - Kerning is applied between two consecutive drawn characters on a line (not across a line break).
// - Wrapping: a line break is inserted at the last space that makes the line fit; trailing spaces do not count toward the
//   line width; a word wider than max_width is split between characters, at least one character per line.
// - Alignment shifts each line by 0, (block_width - line_width) / 2 (floor), or block_width - line_width, where
//   block_width is max_width if non-zero and the widest line otherwise.
// - Quad rectangle: x = pen + offset_x (+ alignment shift), y = line_index * line_height + offset_y, w and h are the glyph
//   pixel size times kOne.
// - Arithmetic: every layout decision (pens, tab stops, the fit test against max_width, wrapping, alignment shifts) uses exact
//   64-bit integer values. Input longer than 2^30 bytes is cut at a character boundary at or below 2^30 bytes and
//   truncated is set, which keeps every intermediate value below 2^63 for any int32 font metrics. Only the values stored in
//   Quad::x, Quad::y, LayoutResult::width and LayoutResult::height are clamped to the int32 range, at the final store.
//   Saturation never changes which glyphs are drawn, where lines break or any alignment decision, and the status stays Ok.
// Returns the totals even when out_quads is null (count only) or too small (BufferTooSmall, quads written up to capacity).
LayoutResult layout_text(const Font& font, const char* utf8, std::size_t length, const LayoutOptions& opt, Quad* out_quads,
                         std::size_t capacity);

// FNV-1a 64 over the little-endian bytes of the quad fields in declaration order, quad by quad (x, y, w, h as 4 bytes;
// the four tex fields as 2 bytes; codepoint and source_offset as 4 bytes), for golden tests.
std::uint64_t digest_quads(const Quad* q, std::size_t n);

}  // namespace lumora::textlayout
