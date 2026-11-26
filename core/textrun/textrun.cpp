// Lumora text run (scope AB). Integer only, no heap, no recursion, no mutable statics. See docs/textrun.md.
#include "textrun.h"

namespace lumora::textrun {

namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;
using ttf::Status;

constexpr u32 kReplacement = 0xFFFD;
constexpr u64 kPenLimit = static_cast<u64>(1) << 30;

bool is_cont(const u8* p, std::size_t len, std::size_t i, u32 lo, u32 hi) { return i < len && p[i] >= lo && p[i] <= hi; }

// Reads one code point at p[i]. Returns the number of bytes used (1 .. 4). A bad or cut short sequence gives U+FFFD and uses
// exactly one byte. The sequence table is the one in textrun.h.
std::size_t next_code_point(const u8* p, std::size_t len, std::size_t i, u32* cp) {
  const u32 b = p[i];
  if (b < 0x80u) {
    *cp = b;
    return 1;
  }
  std::size_t need = 0;  // continuation bytes after the lead
  u32 lo = 0x80u, hi = 0xBFu;  // range of the first continuation byte
  if (b >= 0xC2u && b <= 0xDFu) {
    need = 1;
  } else if (b >= 0xE0u && b <= 0xEFu) {
    need = 2;
    if (b == 0xE0u) lo = 0xA0u;
    if (b == 0xEDu) hi = 0x9Fu;
  } else if (b >= 0xF0u && b <= 0xF4u) {
    need = 3;
    if (b == 0xF0u) lo = 0x90u;
    if (b == 0xF4u) hi = 0x8Fu;
  }
  if (need == 0 || !is_cont(p, len, i + 1, lo, hi)) {
    *cp = kReplacement;
    return 1;
  }
  for (std::size_t k = 2; k <= need; ++k) {
    if (!is_cont(p, len, i + k, 0x80u, 0xBFu)) {
      *cp = kReplacement;
      return 1;
    }
  }
  u32 v = b & (0x3Fu >> need);  // 0x1F, 0x0F, 0x07 for 1, 2, 3 continuation bytes
  for (std::size_t k = 1; k <= need; ++k) v = (v << 6) | (static_cast<u32>(p[i + k]) & 0x3Fu);
  *cp = v;
  return need + 1;
}

// Overlap of two byte ranges: same modular address rule as ttf::glyph_outline. Clear the top byte, take the unsigned difference
// modulo 2^64, answer "no overlap" at |delta| >= 2^47 before any other arithmetic, then compare the exact ranges. An empty range
// overlaps nothing.
bool ranges_overlap(const void* a, u64 a_len, const void* b, u64 b_len) {
  if (a_len == 0 || b_len == 0) return false;
  const u64 kTagMask = 0x00FFFFFFFFFFFFFFull;
  const u64 ua = static_cast<u64>(reinterpret_cast<std::uintptr_t>(a)) & kTagMask;
  const u64 ub = static_cast<u64>(reinterpret_cast<std::uintptr_t>(b)) & kTagMask;
  const u64 diff = ua - ub;
  const bool negative = diff >= (static_cast<u64>(1) << 63);
  const u64 absd = negative ? (static_cast<u64>(0) - diff) : diff;
  if (absd >= (static_cast<u64>(1) << 47)) return false;
  const i64 delta = negative ? -static_cast<i64>(absd) : static_cast<i64>(absd);  // start of A relative to the start of B
  return delta < static_cast<i64>(b_len) && delta + static_cast<i64>(a_len) > 0;
}

// round(u * size / upem) for u >= 0 in unsigned 64-bit arithmetic: u <= 4096 * 65535 and size <= 2^22 keep 2 * u * size below 2^50.
u64 scale(u64 u, u64 size, u64 upem) { return (2 * u * size + upem) / (2 * upem); }

}  // namespace

bool count_code_points(const char* text, std::size_t len, std::uint32_t* count) {
  if (count == nullptr) return false;
  if (text == nullptr) {
    if (len > 0) return false;
    *count = 0;
    return true;
  }
  const u8* p = reinterpret_cast<const u8*>(text);
  u32 n = 0;
  std::size_t i = 0;
  while (i < len) {
    u32 cp;
    i += next_code_point(p, len, i, &cp);
    ++n;
  }
  *count = n;
  return true;
}

std::uint64_t digest_placed(const Placed* p, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (p == nullptr) return h;
  for (std::size_t i = 0; i < count; ++i) {
    const u32 w[2] = {p[i].glyph, static_cast<u32>(p[i].x_fx)};
    for (int k = 0; k < 2; ++k)
      for (int b = 0; b < 4; ++b) {
        h ^= (w[k] >> (8 * b)) & 0xFFu;
        h *= 0x100000001b3ull;
      }
  }
  return h;
}

ttf::Status shape_utf8(const ttf::Font& font, const char* text, std::size_t len, std::int32_t size_fx, Placed* out,
                       std::uint32_t out_capacity, std::uint32_t* out_count, std::int32_t* width_fx) {
  if (font.data == nullptr || out == nullptr || out_count == nullptr || width_fx == nullptr) return Status::NullBuffer;
  if (text == nullptr && len > 0) return Status::NullBuffer;
  if (font.units_per_em == 0) return Status::BadFont;  // a hand-built view; parsed fonts have 16 .. 16384
  if (size_fx < 1 || size_fx > ttf::kMaxSizeFx) return Status::BadSize;
  const u8* p = reinterpret_cast<const u8*>(text);
  // Count the glyphs, but stop as soon as the run is too long: no more than kMaxRunGlyphs + 1 code points are ever decoded.
  u32 n = 0;
  for (std::size_t i = 0; i < len && n <= kMaxRunGlyphs;) {
    u32 cp;
    i += next_code_point(p, len, i, &cp);
    ++n;
  }
  if (n > kMaxRunGlyphs || out_capacity < n) return Status::BufferTooSmall;
  const u64 out_bytes = static_cast<u64>(out_capacity) * sizeof(Placed);
  const u64 font_bytes = static_cast<u64>(font.size);
  if (ranges_overlap(out, out_bytes, out_count, sizeof(u32)) || ranges_overlap(out, out_bytes, width_fx, sizeof(std::int32_t)) ||
      ranges_overlap(out, out_bytes, font.data, font_bytes) || ranges_overlap(out_count, sizeof(u32), width_fx, sizeof(std::int32_t)) ||
      ranges_overlap(out_count, sizeof(u32), font.data, font_bytes) || ranges_overlap(width_fx, sizeof(std::int32_t), font.data, font_bytes))
    return Status::Overlap;
  Placed stage[kMaxRunGlyphs];  // at most 4096 glyphs (32 KiB of stack), copied to out only at the end
  const u64 size = static_cast<u64>(size_fx);
  const u64 upem = font.units_per_em;  // not 0: checked above
  u64 sum = 0;
  u32 count = 0;
  for (std::size_t i = 0; i < len;) {
    u32 cp;
    i += next_code_point(p, len, i, &cp);
    u32 glyph = 0;
    Status s = ttf::glyph_index(font, cp, &glyph);
    if (s != Status::Ok) return s;
    u32 advance = 0;
    s = ttf::glyph_advance(font, glyph, &advance);
    if (s != Status::Ok) return s;
    const u64 x = scale(sum, size, upem);
    if (x > kPenLimit) return Status::OutOfRange;
    stage[count].glyph = glyph;
    stage[count].x_fx = static_cast<std::int32_t>(x);
    ++count;
    sum += advance;
  }
  const u64 width = scale(sum, size, upem);
  if (width > kPenLimit) return Status::OutOfRange;
  for (u32 i = 0; i < count; ++i) out[i] = stage[i];
  *out_count = count;
  *width_fx = static_cast<std::int32_t>(width);
  return Status::Ok;
}

}  // namespace lumora::textrun
