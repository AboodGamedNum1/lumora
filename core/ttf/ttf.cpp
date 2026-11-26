// Lumora CPU TrueType reader (scope AA). Integer only, no heap, no recursion, no mutable statics. See docs/ttf.md.
#include "ttf.h"

namespace lumora::ttf {

namespace {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using outline::Command;
using outline::Op;
using outline::Point;

// Big endian reads. The caller has checked that the bytes exist.
u32 be16(const u8* p, std::size_t off) { return (static_cast<u32>(p[off]) << 8) | p[off + 1]; }
u32 be32(const u8* p, std::size_t off) { return (be16(p, off) << 16) | be16(p, off + 2); }
i32 sext16(u32 v) { return static_cast<i32>(v >= 0x8000u ? static_cast<i64>(v) - 65536 : static_cast<i64>(v)); }
i32 sext8(u32 v) { return static_cast<i32>(v >= 0x80u ? static_cast<i64>(v) - 256 : static_cast<i64>(v)); }

constexpr u32 kTagHead = 0x68656164, kTagMaxp = 0x6d617870, kTagLoca = 0x6c6f6361, kTagGlyf = 0x676c7966, kTagCmap = 0x636d6170,
              kTagHhea = 0x68686561, kTagHmtx = 0x686d7478;

// Overlap of two touched byte ranges. Same modular address rule as the other modules: clear the top byte, take the unsigned
// difference modulo 2^64, answer "no overlap" at |delta| >= 2^47 before any other arithmetic, then compare the exact ranges. Both
// lengths are positive.
bool ranges_overlap(const void* a, u64 a_len, const void* b, u64 b_len) {
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

struct Pt {  // a glyph point in DOUBLED font units
  i32 x, y;
  bool on;
};

// Everything a glyph needs while it is read. kMaxGlyphPoints points, at most that many contours (every contour has a point).
struct Glyph {
  Pt pts[kMaxGlyphPoints];
  u32 ends[kMaxGlyphPoints];  // end index (exclusive) of each contour in pts
  u32 npts = 0, ncont = 0;
};

// Range of a glyph's data inside glyf. BadGlyph for a bad loca entry, a decreasing pair or data outside glyf.
Status glyph_range(const Font& f, u32 g, std::size_t* start, std::size_t* end) {
  const std::size_t entry = f.loca_long ? 4 : 2;
  if ((static_cast<std::size_t>(g) + 2) * entry > f.loca_len) return Status::BadGlyph;
  const u8* l = f.data + f.loca_off;
  std::size_t a, b;
  if (f.loca_long) {
    a = be32(l, static_cast<std::size_t>(g) * 4);
    b = be32(l, static_cast<std::size_t>(g) * 4 + 4);
  } else {
    a = static_cast<std::size_t>(be16(l, static_cast<std::size_t>(g) * 2)) * 2;
    b = static_cast<std::size_t>(be16(l, static_cast<std::size_t>(g) * 2 + 2)) * 2;
  }
  if (a > b || b > f.glyf_len) return Status::BadGlyph;
  *start = a;
  *end = b;
  return Status::Ok;
}

// Reads a simple glyph (numberOfContours >= 0, already read) and appends its points and contours, moved by (dx, dy) font units.
// Coordinates are accumulated without wraparound and stored doubled.
Status read_simple(const u8* d, std::size_t len, i32 nc, i32 dx, i32 dy, Glyph& gl) {
  if (nc == 0) return Status::Ok;
  const std::size_t n_end = static_cast<std::size_t>(nc);
  if (10 + 2 * n_end > len) return Status::BadGlyph;
  u32 prev = 0;
  for (std::size_t i = 0; i < n_end; ++i) {
    const u32 e = be16(d, 10 + 2 * i);
    if (i > 0 && e <= prev) return Status::BadGlyph;
    prev = e;
  }
  const u32 count = prev + 1;
  if (static_cast<u64>(gl.npts) + count > kMaxGlyphPoints) return Status::TooComplex;
  std::size_t pos = 10 + 2 * n_end;
  if (pos + 2 > len) return Status::BadGlyph;
  const std::size_t ilen = be16(d, pos);
  pos += 2;
  if (pos + ilen > len) return Status::BadGlyph;
  pos += ilen;
  u8 flags[kMaxGlyphPoints];
  for (u32 k = 0; k < count;) {
    if (pos >= len) return Status::BadGlyph;
    const u8 f = d[pos++];
    u32 reps = 1;
    if (f & 8u) {
      if (pos >= len) return Status::BadGlyph;
      reps += d[pos++];
    }
    if (k + reps > count) return Status::BadGlyph;
    for (u32 r = 0; r < reps; ++r) flags[k + r] = f;
    k += reps;
  }
  i64 x = 0, y = 0;
  Pt* out = gl.pts + gl.npts;
  for (u32 k = 0; k < count; ++k) {
    const u8 f = flags[k];
    if (f & 2u) {
      if (pos >= len) return Status::BadGlyph;
      const i32 v = static_cast<i32>(d[pos++]);
      x += (f & 16u) ? v : -v;
    } else if (!(f & 16u)) {
      if (pos + 2 > len) return Status::BadGlyph;
      x += sext16(be16(d, pos));
      pos += 2;
    }
    out[k].x = static_cast<i32>(2 * (x + dx));
    out[k].on = (f & 1u) != 0;
  }
  for (u32 k = 0; k < count; ++k) {
    const u8 f = flags[k];
    if (f & 4u) {
      if (pos >= len) return Status::BadGlyph;
      const i32 v = static_cast<i32>(d[pos++]);
      y += (f & 32u) ? v : -v;
    } else if (!(f & 32u)) {
      if (pos + 2 > len) return Status::BadGlyph;
      y += sext16(be16(d, pos));
      pos += 2;
    }
    out[k].y = static_cast<i32>(2 * (y + dy));
  }
  // endPts again from the bytes (validated above)
  for (std::size_t i = 0; i < n_end; ++i) gl.ends[gl.ncont + i] = gl.npts + be16(d, 10 + 2 * i) + 1;
  gl.ncont += static_cast<u32>(n_end);
  gl.npts += count;
  return Status::Ok;
}

Status read_glyph(const Font& f, u32 g, Glyph& gl) {
  std::size_t a, b;
  Status s = glyph_range(f, g, &a, &b);
  if (s != Status::Ok) return s;
  if (a == b) return Status::Ok;
  const u8* d = f.data + f.glyf_off + a;
  const std::size_t len = b - a;
  if (len < 10) return Status::BadGlyph;
  const i32 nc = sext16(be16(d, 0));
  if (nc >= 0) return read_simple(d, len, nc, 0, 0, gl);
  std::size_t pos = 10;
  u32 comps = 0;
  u32 flags = 0;
  for (;;) {
    if (pos + 4 > len) return Status::BadGlyph;
    flags = be16(d, pos);
    const u32 cg = be16(d, pos + 2);
    pos += 4;
    i32 a1, a2;
    if (flags & 1u) {
      if (pos + 4 > len) return Status::BadGlyph;
      a1 = sext16(be16(d, pos));
      a2 = sext16(be16(d, pos + 2));
      pos += 4;
    } else {
      if (pos + 2 > len) return Status::BadGlyph;
      a1 = sext8(d[pos]);
      a2 = sext8(d[pos + 1]);
      pos += 2;
    }
    ++comps;
    if (!(flags & 2u)) return Status::Unsupported;
    if (flags & (0x0008u | 0x0040u | 0x0080u)) return Status::Unsupported;
    if (cg >= f.num_glyphs) return Status::BadGlyph;
    std::size_t ca, cb;
    s = glyph_range(f, cg, &ca, &cb);
    if (s != Status::Ok) return s;
    if (ca != cb) {
      const u8* cd = f.data + f.glyf_off + ca;
      const std::size_t clen = cb - ca;
      if (clen < 10) return Status::BadGlyph;
      const i32 cnc = sext16(be16(cd, 0));
      if (cnc < 0) return Status::Unsupported;  // no composite inside a composite
      s = read_simple(cd, clen, cnc, a1, a2, gl);
      if (s != Status::Ok) return s;
    }
    if (!(flags & 0x0020u)) break;
    if (comps >= kMaxComponents) return Status::TooComplex;
  }
  if (flags & 0x0100u) {
    if (pos + 2 > len) return Status::BadGlyph;
    if (pos + 2 + be16(d, pos) > len) return Status::BadGlyph;
  }
  return Status::Ok;
}

// round(num / den) to nearest, ties away from zero, exact. den > 0.
i64 rint_div(i64 num, i64 den) {
  const u64 an = static_cast<u64>(num < 0 ? -num : num);
  const u64 ud = static_cast<u64>(den);
  u64 q = an / ud;
  const u64 r = an % ud;
  if (2 * r >= ud) ++q;
  return num < 0 ? -static_cast<i64>(q) : static_cast<i64>(q);
}

struct Emitter {
  Command cmds[kMaxGlyphCommands];
  u32 n = 0;
  i64 size_fx, den, ox, oy;
  bool bad = false;
  Point conv(i32 dx, i32 dy) {
    const i64 fx = ox + rint_div(static_cast<i64>(dx) * size_fx, den);
    const i64 fy = oy - rint_div(static_cast<i64>(dy) * size_fx, den);
    const i64 lim = static_cast<i64>(1) << 24;
    if (fx < -lim || fx > lim || fy < -lim || fy > lim) {
      bad = true;
      return Point{0, 0};
    }
    return Point{static_cast<i32>(fx), static_cast<i32>(fy)};
  }
  void push(Op op, const Point& a, const Point& b) {
    // The count never exceeds kMaxGlyphCommands (docs/ttf.md). The guard keeps the staging array safe if that proof were wrong.
    if (n >= kMaxGlyphCommands) {
      bad = true;
      return;
    }
    cmds[n++] = Command{op, a, b, Point{0, 0}};
  }
  void move(i32 x, i32 y) { push(Op::Move, conv(x, y), Point{0, 0}); }
  void line(i32 x, i32 y) { push(Op::Line, conv(x, y), Point{0, 0}); }
  void quad(i32 cx, i32 cy, i32 x, i32 y) {
    const Point c = conv(cx, cy);
    push(Op::Quad, c, conv(x, y));
  }
  void close() { push(Op::Close, Point{0, 0}, Point{0, 0}); }
};

void emit_contour(Emitter& e, const Pt* p, u32 n) {
  if (n < 2) return;
  i32 sx, sy;
  u32 first, last;  // walk range [first, last)
  if (p[0].on) {
    sx = p[0].x;
    sy = p[0].y;
    first = 1;
    last = n;
  } else if (p[n - 1].on) {
    sx = p[n - 1].x;
    sy = p[n - 1].y;
    first = 0;
    last = n - 1;
  } else {
    // doubled coordinates: the midpoint of the doubled values (2a + 2b) / 2 is exactly a + b
    sx = static_cast<i32>((static_cast<i64>(p[0].x) + p[n - 1].x) / 2);
    sy = static_cast<i32>((static_cast<i64>(p[0].y) + p[n - 1].y) / 2);
    first = 0;
    last = n;
  }
  e.move(sx, sy);
  bool pending = false;
  i32 cx = 0, cy = 0;
  for (u32 i = first; i < last; ++i) {
    if (p[i].on) {
      if (pending) {
        e.quad(cx, cy, p[i].x, p[i].y);
        pending = false;
      } else {
        e.line(p[i].x, p[i].y);
      }
    } else {
      if (pending) {
        const i32 mx = static_cast<i32>((static_cast<i64>(cx) + p[i].x) / 2);
        const i32 my = static_cast<i32>((static_cast<i64>(cy) + p[i].y) / 2);
        e.quad(cx, cy, mx, my);
      }
      cx = p[i].x;
      cy = p[i].y;
      pending = true;
    }
  }
  if (pending) e.quad(cx, cy, sx, sy);
  e.close();
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadFont: return "bad font";
    case Status::MissingTable: return "missing table";
    case Status::Unsupported: return "unsupported";
    case Status::BadSize: return "bad size";
    case Status::BadGlyph: return "bad glyph";
    case Status::BadCmap: return "bad cmap";
    case Status::TooComplex: return "too complex";
    case Status::OutOfRange: return "out of range";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

Status parse_font(const std::uint8_t* data, std::size_t size, Font* out) {
  if (data == nullptr || out == nullptr) return Status::NullBuffer;
  if (size < 12 || size > kMaxFontBytes) return Status::BadFont;
  const u32 tag = be32(data, 0);
  if (tag != 0x00010000u && tag != 0x74727565u) return Status::Unsupported;
  const u32 num_tables = be16(data, 4);
  if (num_tables < 1 || num_tables > kMaxTables) return Status::BadFont;
  if (12 + static_cast<u64>(16) * num_tables > size) return Status::BadFont;
  const u32 want[7] = {kTagHead, kTagMaxp, kTagLoca, kTagGlyf, kTagCmap, kTagHhea, kTagHmtx};
  u64 toff[7] = {}, tlen[7] = {};
  bool found[7] = {};
  for (u32 i = 0; i < num_tables; ++i) {
    const std::size_t r = 12 + 16 * static_cast<std::size_t>(i);
    const u32 t = be32(data, r);
    const u64 off = be32(data, r + 8), len = be32(data, r + 12);
    if (off + len > size) return Status::BadFont;
    for (u32 j = 0; j < i; ++j)
      if (be32(data, 12 + 16 * static_cast<std::size_t>(j)) == t) return Status::BadFont;
    for (int k = 0; k < 7; ++k)
      if (t == want[k]) {
        found[k] = true;
        toff[k] = off;
        tlen[k] = len;
      }
  }
  for (int k = 0; k < 7; ++k)
    if (!found[k]) return Status::MissingTable;
  // head
  if (tlen[0] < 54) return Status::BadFont;
  const u8* head = data + toff[0];
  if (be32(head, 12) != 0x5F0F3CF5u) return Status::BadFont;
  const u32 upem = be16(head, 18);
  if (upem < 16 || upem > 16384) return Status::BadFont;
  const u32 loca_fmt = be16(head, 50);
  if (loca_fmt > 1) return Status::BadFont;
  // maxp
  if (tlen[1] < 6) return Status::BadFont;
  const u8* maxp = data + toff[1];
  if (be32(maxp, 0) != 0x00010000u) return Status::Unsupported;
  const u32 num_glyphs = be16(maxp, 4);
  if (num_glyphs < 1) return Status::BadFont;
  // loca
  if (tlen[2] < static_cast<u64>(num_glyphs + 1) * (loca_fmt ? 4u : 2u)) return Status::BadFont;
  // hhea and hmtx
  if (tlen[5] < 36) return Status::BadFont;
  const u32 nhm = be16(data + toff[5], 34);
  if (nhm < 1 || nhm > num_glyphs) return Status::BadFont;
  if (tlen[6] < static_cast<u64>(4) * nhm + static_cast<u64>(2) * (num_glyphs - nhm)) return Status::BadFont;
  // cmap
  if (tlen[4] < 4) return Status::BadFont;
  const u8* cmap = data + toff[4];
  const u64 clen = tlen[4];
  if (be16(cmap, 0) != 0) return Status::BadFont;
  const u32 nt = be16(cmap, 2);
  if (nt > 64) return Status::BadFont;
  if (4 + static_cast<u64>(8) * nt > clen) return Status::BadFont;
  bool have_a = false, have_b = false;
  u64 off_a = 0, off_b = 0;
  for (u32 i = 0; i < nt; ++i) {
    const std::size_t r = 4 + 8 * static_cast<std::size_t>(i);
    const u32 plat = be16(cmap, r), enc = be16(cmap, r + 2);
    const u64 so = be32(cmap, r + 4);
    if (so + 4 > clen) return Status::BadFont;
    if (plat == 3 && enc == 1 && !have_a) {
      have_a = true;
      off_a = so;
    }
    if (plat == 0 && enc == 3 && !have_b) {
      have_b = true;
      off_b = so;
    }
  }
  if (!have_a && !have_b) return Status::Unsupported;
  const u64 so = have_a ? off_a : off_b;
  if (be16(cmap, static_cast<std::size_t>(so)) != 4) return Status::Unsupported;
  if (so + 8 > clen) return Status::BadFont;
  const u64 seg2 = be16(cmap, static_cast<std::size_t>(so) + 6);
  if (seg2 == 0 || (seg2 & 1u)) return Status::BadFont;
  if (16 + 4 * seg2 > clen - so) return Status::BadFont;
  Font f;
  f.data = data;
  f.size = size;
  f.units_per_em = upem;
  f.num_glyphs = num_glyphs;
  f.num_hmetrics = nhm;
  f.loca_long = loca_fmt == 1;
  f.loca_off = static_cast<std::size_t>(toff[2]);
  f.loca_len = static_cast<std::size_t>(tlen[2]);
  f.glyf_off = static_cast<std::size_t>(toff[3]);
  f.glyf_len = static_cast<std::size_t>(tlen[3]);
  f.hmtx_off = static_cast<std::size_t>(toff[6]);
  f.hmtx_len = static_cast<std::size_t>(tlen[6]);
  f.cmap_off = static_cast<std::size_t>(toff[4] + so);
  f.cmap_len = static_cast<std::size_t>(clen - so);
  *out = f;
  return Status::Ok;
}

Status glyph_index(const Font& font, std::uint32_t code_point, std::uint32_t* glyph_out) {
  if (font.data == nullptr || glyph_out == nullptr) return Status::NullBuffer;
  if (code_point > 0xFFFFu) {
    *glyph_out = 0;
    return Status::Ok;
  }
  const u8* sub = font.data + font.cmap_off;
  const std::size_t slen = font.cmap_len;
  if (slen < 16) return Status::BadCmap;
  const std::size_t seg2 = be16(sub, 6);
  if (16 + 4 * seg2 > slen) return Status::BadCmap;
  const std::size_t segs = seg2 / 2;
  u32 result = 0;
  for (std::size_t i = 0; i < segs; ++i) {
    const u32 end = be16(sub, 14 + 2 * i);
    if (end < code_point) continue;
    const u32 start = be16(sub, 16 + seg2 + 2 * i);
    if (start > code_point) break;
    const u32 delta = be16(sub, 16 + 2 * seg2 + 2 * i);
    const std::size_t ro_pos = 16 + 3 * seg2 + 2 * i;
    const u32 ro = be16(sub, ro_pos);
    if (ro == 0) {
      result = (code_point + delta) & 0xFFFFu;
    } else {
      const std::size_t pos = ro_pos + ro + 2 * static_cast<std::size_t>(code_point - start);
      if (pos + 2 > slen) return Status::BadCmap;
      const u32 v = be16(sub, pos);
      result = v == 0 ? 0 : ((v + delta) & 0xFFFFu);
    }
    break;
  }
  if (result >= font.num_glyphs) result = 0;
  *glyph_out = result;
  return Status::Ok;
}

Status glyph_advance(const Font& font, std::uint32_t glyph, std::uint32_t* advance_out) {
  if (font.data == nullptr || advance_out == nullptr) return Status::NullBuffer;
  if (glyph >= font.num_glyphs) return Status::BadGlyph;
  if (font.num_hmetrics < 1) return Status::BadGlyph;
  const std::size_t idx = glyph < font.num_hmetrics ? glyph : font.num_hmetrics - 1;
  if (4 * idx + 4 > font.hmtx_len) return Status::BadGlyph;
  *advance_out = be16(font.data + font.hmtx_off, 4 * idx);
  return Status::Ok;
}

Status glyph_outline(const Font& font, std::uint32_t glyph, std::int32_t size_fx, std::int32_t origin_x_fx, std::int32_t origin_y_fx,
                     outline::Command* out, std::uint32_t out_capacity, std::uint32_t* out_count) {
  if (font.data == nullptr || out == nullptr || out_count == nullptr) return Status::NullBuffer;
  if (size_fx < 1 || size_fx > kMaxSizeFx || origin_x_fx < -kMaxOriginFx || origin_x_fx > kMaxOriginFx || origin_y_fx < -kMaxOriginFx ||
      origin_y_fx > kMaxOriginFx)
    return Status::BadSize;
  if (glyph >= font.num_glyphs) return Status::BadGlyph;
  if (out_capacity < kMaxGlyphCommands) return Status::BufferTooSmall;
  struct Ext {
    const void* p;
    u64 bytes;
  };
  const Ext ex[3] = {{out, static_cast<u64>(kMaxGlyphCommands) * sizeof(Command)}, {out_count, sizeof(u32)}, {font.data, font.size}};
  for (int i = 0; i < 3; ++i)
    for (int j = i + 1; j < 3; ++j)
      if (ranges_overlap(ex[i].p, ex[i].bytes, ex[j].p, ex[j].bytes)) return Status::Overlap;
  Glyph gl;
  Status s = read_glyph(font, glyph, gl);
  if (s != Status::Ok) return s;
  Emitter em;
  em.size_fx = size_fx;
  em.den = static_cast<i64>(2) * font.units_per_em;
  em.ox = origin_x_fx;
  em.oy = origin_y_fx;
  u32 begin = 0;
  for (u32 c = 0; c < gl.ncont; ++c) {
    emit_contour(em, gl.pts + begin, gl.ends[c] - begin);
    begin = gl.ends[c];
  }
  if (em.bad) return Status::OutOfRange;
  for (u32 i = 0; i < em.n; ++i) out[i] = em.cmds[i];
  *out_count = em.n;
  return Status::Ok;
}

std::uint64_t digest_commands(const outline::Command* cmds, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (cmds == nullptr) return h;
  auto word = [&h](u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  };
  for (std::size_t i = 0; i < count; ++i) {
    word(static_cast<u32>(cmds[i].op));
    word(static_cast<u32>(cmds[i].a.x));
    word(static_cast<u32>(cmds[i].a.y));
    word(static_cast<u32>(cmds[i].b.x));
    word(static_cast<u32>(cmds[i].b.y));
    word(static_cast<u32>(cmds[i].c.x));
    word(static_cast<u32>(cmds[i].c.y));
  }
  return h;
}

}  // namespace lumora::ttf
