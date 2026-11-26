// Lumora text run (scope AB, M4 text groundwork). Pure CPU, deterministic, no I/O, no threads, no floating point.
// The interface in this header is fixed; the contributor implements core/textrun/textrun.cpp only (plus tests and docs).
// It turns a UTF-8 string and a parsed TrueType font (core/ttf) into a list of glyph ids with pen positions, no kerning, no
// shaping, no bidi: one glyph per code point, left to right. Integer arithmetic only (64-bit where needed), no libm, no heap
// allocation, no recursion, no mutable statics, no constant division by zero or overflow (MSVC). Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>

#include "ttf.h"

namespace lumora::textrun {

constexpr std::uint32_t kMaxRunGlyphs = 4096;  // glyphs in one run

struct Placed {
  std::uint32_t glyph;  // glyph id from ttf::glyph_index
  std::int32_t x_fx;    // pen position of the glyph origin in 24.8 pixels, relative to the run start
};

// UTF-8 decoding, strict. A code point is read from the longest valid sequence: 1 byte 00..7F; 2 bytes C2..DF 80..BF; 3 bytes E0
// A0..BF 80..BF, E1..EC and EE..EF 80..BF 80..BF, ED 80..9F 80..BF (no surrogates); 4 bytes F0 90..BF 80..BF 80..BF, F1..F3 80..BF
// x3, F4 80..8F 80..BF 80..BF. Anything else (a stray continuation byte, an invalid lead byte such as C0, C1 or F5..FF, a sequence
// cut short by a byte that does not fit or by the end of the text, an overlong form, a surrogate, a value above 10FFFF) yields one
// U+FFFD for the first byte only and decoding continues at the next byte (so n bad bytes give n replacement characters).
// Code points above 0xFFFF map to glyph 0 (ttf::glyph_index rule), they are not an error.

// Shapes the text. For each code point in order: glyph = ttf::glyph_index; advance = ttf::glyph_advance(glyph) in font units;
// the glyph is placed at x_fx = round(U * size_fx / units_per_em), where U is the sum of the advances of all earlier glyphs in
// font units (cumulative rounding, nearest, ties away from zero, exact 64-bit integer arithmetic; the per-glyph rounding error
// never accumulates). *width_fx is the same formula applied to the sum of all advances. An empty text (len 0) gives count 0 and
// width 0 and is Ok.
//
// Checks in this order: NullBuffer (font.data, out, out_count, width_fx; text may be null only when len is 0, else NullBuffer),
// BadSize (size_fx not in 1 .. ttf::kMaxSizeFx), BufferTooSmall (out_capacity < the glyph count of the text, counted after UTF-8
// decoding, and also whenever the count exceeds kMaxRunGlyphs, in which case out_capacity is irrelevant), Overlap (out over
// out_capacity Placed values against out_count, against width_fx, against the font bytes font.data .. +size, and out_count against
// width_fx and against the font bytes, same modular address rule as ttf::glyph_outline; text is not checked), then the ttf errors
// in glyph order (BadCmap from glyph_index, BadGlyph from glyph_advance), and the first one wins. A position beyond +/- 2^30 is
// OutOfRange (checked on the final x of each glyph and on the width). Nothing is written to out, out_count or width_fx on any error
// (stage locally, copy at the end).
ttf::Status shape_utf8(const ttf::Font& font, const char* text, std::size_t len, std::int32_t size_fx, Placed* out,
                       std::uint32_t out_capacity, std::uint32_t* out_count, std::int32_t* width_fx);

// Number of code points of the text after the strict decoding above (replacement characters count). text null with len 0 gives 0;
// text null with len > 0 gives 0 and false. Returns true otherwise, with *count written.
bool count_code_points(const char* text, std::size_t len, std::uint32_t* count);

// FNV-1a 64 over 32-bit little-endian words: per Placed the glyph, then x_fx (as uint32 two's complement). Null or zero count hashes
// zero bytes.
std::uint64_t digest_placed(const Placed* p, std::size_t count);

}  // namespace lumora::textrun
