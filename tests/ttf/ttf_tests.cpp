// Tests for the TrueType reader (scope AA). Every test font is built here from code (hand-built bytes, no third-party file).
// Expected values come from an independent Python reference (its own font builder, parser, cmap, hmtx, glyph decoder, contour walker,
// exact integer scaler and FNV) and are pasted below as constants; the C++ builder in this file is checked against the reference
// builder by byte digests. Status checks for hand-made damaged glyphs and tables are derived from the rules in ttf.h by hand.
// Everything here is local CPU evidence. See docs/ttf.md.
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "ttf.h"

using namespace lumora::ttf;
using lumora::outline::Command;
using lumora::outline::Op;

namespace {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using Bytes = std::vector<u8>;

constexpr u64 kFnvBasis = 0xcbf29ce484222325ull;
constexpr u64 kFnvPrime = 0x100000001b3ull;

u64 fnv_bytes(const Bytes& b) {
  u64 h = kFnvBasis;
  for (u8 c : b) {
    h ^= c;
    h *= kFnvPrime;
  }
  return h;
}

struct FnvW {
  u64 h = kFnvBasis;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= kFnvPrime;
    }
  }
};

u32 uc(int v) { return static_cast<u32>(v); }

// ---- the test font builder (mirror of the reference builder) ----
void p16(Bytes& b, int v) {
  b.push_back(static_cast<u8>((v >> 8) & 0xFF));
  b.push_back(static_cast<u8>(v & 0xFF));
}
void p32(Bytes& b, u32 v) {
  p16(b, static_cast<int>(v >> 16));
  p16(b, static_cast<int>(v & 0xFFFFu));
}
void pad4(Bytes& b) {
  while (b.size() % 4) b.push_back(0);
}
void zeros(Bytes& b, std::size_t n) { b.insert(b.end(), n, 0); }

struct Pt {
  int x, y, on;
};
struct Comp {
  int g, dx, dy, extra;
};
struct GDesc {
  int kind;  // 0 empty, 1 simple, 2 composite, 3 raw
  std::vector<std::vector<Pt>> contours;
  std::vector<Comp> comps;
  int ilen;
  Bytes raw;
};
GDesc E() { return GDesc{0, {}, {}, 0, {}}; }
GDesc S(std::vector<std::vector<Pt>> c, int il) { return GDesc{1, std::move(c), {}, il, {}}; }
GDesc C(std::vector<Comp> k, int il) { return GDesc{2, {}, std::move(k), il, {}}; }
GDesc R(Bytes r) { return GDesc{3, {}, {}, 0, std::move(r)}; }

Bytes enc_simple(const std::vector<std::vector<Pt>>& contours, int ilen) {
  std::vector<Pt> pts;
  std::vector<int> ends;
  int n = 0;
  for (const auto& c : contours) {
    for (const Pt& p : c) pts.push_back(p);
    n += static_cast<int>(c.size());
    ends.push_back(n - 1);
  }
  int minx = 0, miny = 0, maxx = 0, maxy = 0;
  if (!pts.empty()) {
    minx = maxx = pts[0].x;
    miny = maxy = pts[0].y;
    for (const Pt& p : pts) {
      if (p.x < minx) minx = p.x;
      if (p.x > maxx) maxx = p.x;
      if (p.y < miny) miny = p.y;
      if (p.y > maxy) maxy = p.y;
    }
  }
  Bytes b;
  p16(b, static_cast<int>(contours.size()));
  p16(b, minx);
  p16(b, miny);
  p16(b, maxx);
  p16(b, maxy);
  for (int e : ends) p16(b, e);
  p16(b, ilen);
  for (int i = 0; i < ilen; ++i) b.push_back(static_cast<u8>((i * 7 + 3) & 0xFF));
  std::vector<int> flags;
  Bytes xb, yb;
  int px = 0, py = 0;
  for (const Pt& p : pts) {
    int f = p.on ? 1 : 0;
    const int dx = p.x - px, dy = p.y - py;
    if (dx == 0) {
      f |= 16;
    } else if (dx >= -255 && dx <= 255) {
      f |= 2;
      xb.push_back(static_cast<u8>(dx < 0 ? -dx : dx));
      if (dx > 0) f |= 16;
    } else {
      p16(xb, dx);
    }
    if (dy == 0) {
      f |= 32;
    } else if (dy >= -255 && dy <= 255) {
      f |= 4;
      yb.push_back(static_cast<u8>(dy < 0 ? -dy : dy));
      if (dy > 0) f |= 32;
    } else {
      p16(yb, dy);
    }
    flags.push_back(f);
    px = p.x;
    py = p.y;
  }
  Bytes fb;
  std::size_t i = 0;
  while (i < flags.size()) {
    std::size_t j = i;
    while (j + 1 < flags.size() && flags[j + 1] == flags[i] && j + 1 - i < 256) ++j;
    const std::size_t r = j - i + 1;
    if (r >= 2) {
      fb.push_back(static_cast<u8>(flags[i] | 8));
      fb.push_back(static_cast<u8>(r - 1));
    } else {
      fb.push_back(static_cast<u8>(flags[i]));
    }
    i = j + 1;
  }
  b.insert(b.end(), fb.begin(), fb.end());
  b.insert(b.end(), xb.begin(), xb.end());
  b.insert(b.end(), yb.begin(), yb.end());
  return b;
}

Bytes enc_comp(const std::vector<Comp>& comps, int ilen) {
  Bytes b;
  p16(b, 0xFFFF);
  zeros(b, 8);
  for (std::size_t k = 0; k < comps.size(); ++k) {
    const Comp& c = comps[k];
    int fl = 2 | c.extra;
    const bool word = !(c.dx >= -128 && c.dx <= 127 && c.dy >= -128 && c.dy <= 127);
    if (word) fl |= 1;
    if (k + 1 < comps.size()) {
      fl |= 0x20;
    } else if (ilen >= 0) {
      fl |= 0x100;
    }
    p16(b, fl);
    p16(b, c.g);
    if (word) {
      p16(b, c.dx);
      p16(b, c.dy);
    } else {
      b.push_back(static_cast<u8>(c.dx & 0xFF));
      b.push_back(static_cast<u8>(c.dy & 0xFF));
    }
  }
  if (ilen >= 0) {
    p16(b, ilen);
    for (int i = 0; i < ilen; ++i) b.push_back(static_cast<u8>((i * 5 + 1) & 0xFF));
  }
  return b;
}

struct Seg {
  int s, e, d;
  std::vector<int> g;
  bool has;
};

Bytes build_cmap4(const std::vector<Seg>& segs) {
  const int n = static_cast<int>(segs.size());
  const int seg2 = 2 * n;
  Bytes garr;
  std::vector<int> ros;
  for (int i = 0; i < n; ++i) {
    if (!segs[static_cast<std::size_t>(i)].has) {
      ros.push_back(0);
    } else {
      const int pos_entry = 14 + seg2 + 2 + seg2 + seg2 + 2 * i;
      const int pos_chunk = 14 + seg2 + 2 + 3 * seg2 + static_cast<int>(garr.size());
      ros.push_back(pos_chunk - pos_entry);
      for (int v : segs[static_cast<std::size_t>(i)].g) p16(garr, v);
    }
  }
  Bytes b;
  p16(b, 4);
  p16(b, 0);
  p16(b, 0);
  p16(b, seg2);
  p16(b, 0);
  p16(b, 0);
  p16(b, 0);
  for (const Seg& s : segs) p16(b, s.e);
  p16(b, 0);
  for (const Seg& s : segs) p16(b, s.s);
  for (const Seg& s : segs) p16(b, s.d);
  for (int r : ros) p16(b, r);
  b.insert(b.end(), garr.begin(), garr.end());
  b[2] = static_cast<u8>((b.size() >> 8) & 0xFF);
  b[3] = static_cast<u8>(b.size() & 0xFF);
  return b;
}
Seg sg(int s, int e, int d) { return Seg{s, e, d, {}, false}; }
Seg sgg(int s, int e, int d, std::vector<int> g) { return Seg{s, e, d, std::move(g), true}; }

struct CRec {
  int plat, enc;
  Bytes sub;
};
Bytes build_cmap(const std::vector<CRec>& recs) {
  const std::size_t n = recs.size();
  const std::size_t off = 4 + 8 * n;
  Bytes hdr, body;
  p16(hdr, 0);
  p16(hdr, static_cast<int>(n));
  for (const CRec& r : recs) {
    p16(hdr, r.plat);
    p16(hdr, r.enc);
    p32(hdr, static_cast<u32>(off + body.size()));
    body.insert(body.end(), r.sub.begin(), r.sub.end());
  }
  hdr.insert(hdr.end(), body.begin(), body.end());
  return hdr;
}

struct FontSpec {
  std::vector<GDesc> glyphs;
  std::vector<int> adv;
  int nhm = 1;
  std::vector<CRec> cmap;
  int upem = 1000;
  bool long_loca = false;
  u32 tag = 0x00010000u;
  u32 maxp_version = 0x00010000u;
  bool nopad = false;  // do not pad glyphs to 4 bytes (long loca or an even size is then needed)
};

Bytes build_font(const FontSpec& fs) {
  Bytes glyf, loca;
  std::vector<std::size_t> offs{0};
  for (const GDesc& g : fs.glyphs) {
    Bytes e;
    if (g.kind == 1) e = enc_simple(g.contours, g.ilen);
    if (g.kind == 2) e = enc_comp(g.comps, g.ilen);
    if (g.kind == 3) e = g.raw;
    if (!fs.nopad) pad4(e);
    glyf.insert(glyf.end(), e.begin(), e.end());
    offs.push_back(glyf.size());
  }
  for (std::size_t o : offs) {
    if (fs.long_loca) {
      p32(loca, static_cast<u32>(o));
    } else {
      p16(loca, static_cast<int>(o / 2));
    }
  }
  const int ng = static_cast<int>(fs.glyphs.size());
  Bytes head;
  p32(head, 0x00010000u);
  p32(head, 0);
  p32(head, 0);
  p32(head, 0x5F0F3CF5u);
  p16(head, 0);
  p16(head, fs.upem);
  zeros(head, 24);
  p16(head, 0);
  p16(head, 8);
  p16(head, 2);
  p16(head, fs.long_loca ? 1 : 0);
  p16(head, 0);
  Bytes maxp;
  p32(maxp, fs.maxp_version);
  p16(maxp, ng);
  zeros(maxp, 26);
  Bytes hhea;
  p32(hhea, 0x00010000u);
  zeros(hhea, 30);
  p16(hhea, fs.nhm);
  Bytes hmtx;
  for (int i = 0; i < fs.nhm; ++i) {
    p16(hmtx, fs.adv[static_cast<std::size_t>(i)]);
    p16(hmtx, 0);
  }
  zeros(hmtx, 2 * static_cast<std::size_t>(ng - fs.nhm));
  const Bytes cmap = build_cmap(fs.cmap);
  const std::array<std::pair<const char*, const Bytes*>, 7> tabs = {{{"cmap", &cmap},
                                                                      {"glyf", &glyf},
                                                                      {"head", &head},
                                                                      {"hhea", &hhea},
                                                                      {"hmtx", &hmtx},
                                                                      {"loca", &loca},
                                                                      {"maxp", &maxp}}};
  Bytes out;
  p32(out, fs.tag);
  p16(out, 7);
  p16(out, 0);
  p16(out, 0);
  p16(out, 0);
  std::size_t off = 12 + 16 * 7;
  Bytes body;
  for (const auto& t : tabs) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<u8>(t.first[i]));
    p32(out, 0);
    p32(out, static_cast<u32>(off + body.size()));
    p32(out, static_cast<u32>(t.second->size()));
    body.insert(body.end(), t.second->begin(), t.second->end());
    pad4(body);
  }
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

struct OutRow {
  int vi, g, sz, ox, oy, st, n;
  u64 dig;
};

static const std::vector<u8> kCmapA = {0,4,0,86,0,0,0,14,0,0,0,0,0,0,0,70,0,99,1,1,78,0,78,2,128,0,255,255,0,0,0,65,0,97,1,0,78,0,78,1,128,0,255,255,255,192,0,0,0,0,0,100,0,5,128,3,0,1,0,0,0,12,0,16,0,0,0,16,0,0,0,0,0,8,0,9,0,10,0,0,0,7,0,0,0,3};
static const std::vector<u8> kCmapC = {0,4,0,42,0,0,0,6,0,0,0,0,0,0,5,0,0,32,255,255,0,0,5,0,0,16,255,255,0,0,0,5,0,1,0,6,0,0,0,0,0,1};
static const std::vector<u8> kCmapB = {0,4,0,32,0,0,0,4,0,0,0,0,0,0,0,65,255,255,0,0,0,65,255,255,0,2,0,1,0,0,0,0};
static const std::vector<GDesc> kBaseGlyphs = {E(),S({{{0,0,1},{600,0,1},{300,700,1}}},0),S({{{500,0,0},{1000,500,0},{500,1000,0},{0,500,0}}},0),S({{{0,0,0},{300,0,0},{300,300,0},{0,300,1}}},0),S({{{0,0,1},{0,800,1},{800,800,1},{800,0,1}},{{200,200,1},{600,200,1},{600,600,1},{200,600,1}}},0),S({{{50,50,1}},{{100,100,1},{200,150,1}},{{300,300,1},{400,300,0},{400,400,1}}},0),S({{{100,0,1},{200,0,1},{300,0,1},{400,0,1},{500,0,1},{600,0,1},{700,0,1},{800,0,1},{800,-300,1},{100,-300,1}}},0),S({{{-1500,-1200,1},{1500,-1200,1},{1400,900,0},{-1400,1000,1}}},10),C({{1,10,20,0},{3,100,-50,0}},-1),C({{4,-300,400,2048},{2,1000,1000,4}},6),C({{0,5,5,0},{1,-200,-200,0},{6,0,0,0}},-1),S({{{0,0,1},{100,0,0},{200,100,0},{300,100,0},{400,0,1}}},10),S({{{0,0,1},{30000,0,1},{60000,20000,1}}},0),C({{1,32767,-32768,0},{1,32767,-32768,0}},-1)};
static const u64 kFontDigest[3] = {2626029091273945610ull,10812955356376590465ull,16637825644230613681ull};
static const u32 kFontSize[3] = {864,852,792};
static const OutRow kOutRows[] = {
  {0,0,4096,0,0,0,0,0ull},
  {0,0,25600,1280,12800,0,0,0ull},
  {0,0,1,0,0,0,0,0ull},
  {0,0,4194304,-8388608,8388608,0,0,0ull},
  {0,0,300,-777,555,0,0,0ull},
  {0,1,4096,0,0,0,4,2092072384283572290ull},
  {0,1,25600,1280,12800,0,4,18112050557543382918ull},
  {0,1,1,0,0,0,4,7171576986130296020ull},
  {0,1,4194304,-8388608,8388608,0,4,15711330864137630650ull},
  {0,1,300,-777,555,0,4,7477699650640223080ull},
  {0,2,4096,0,0,0,6,9864169251943724893ull},
  {0,2,25600,1280,12800,0,6,928643461151915386ull},
  {0,2,1,0,0,0,6,1209715717094705108ull},
  {0,2,4194304,-8388608,8388608,0,6,10147830277899535340ull},
  {0,2,300,-777,555,0,6,1134189354443266573ull},
  {0,3,4096,0,0,0,5,13104087470263413081ull},
  {0,3,25600,1280,12800,0,5,8322707240163180970ull},
  {0,3,1,0,0,0,5,3417277351210299507ull},
  {0,3,4194304,-8388608,8388608,0,5,1219843417965669656ull},
  {0,3,300,-777,555,0,5,11458486516143589229ull},
  {0,4,4096,0,0,0,10,7891885866058465493ull},
  {0,4,25600,1280,12800,0,10,13626022969734307589ull},
  {0,4,1,0,0,0,10,14793108805319472357ull},
  {0,4,4194304,-8388608,8388608,0,10,1125940410667659045ull},
  {0,4,300,-777,555,0,10,9387474316701448981ull},
  {0,5,4096,0,0,0,6,13667918728677437154ull},
  {0,5,25600,1280,12800,0,6,12697461429499291690ull},
  {0,5,1,0,0,0,6,8013896000889425430ull},
  {0,5,4194304,-8388608,8388608,0,6,4217513685351516165ull},
  {0,5,300,-777,555,0,6,16509245889275675510ull},
  {0,6,4096,0,0,0,11,3706512332903277060ull},
  {0,6,25600,1280,12800,0,11,7739376556626452082ull},
  {0,6,1,0,0,0,11,16617168031676567089ull},
  {0,6,4194304,-8388608,8388608,0,11,3837091420969993943ull},
  {0,6,300,-777,555,0,11,18105236082094325666ull},
  {0,7,4096,0,0,0,4,631855975475314910ull},
  {0,7,25600,1280,12800,0,4,15188474575086777404ull},
  {0,7,1,0,0,0,4,14047950579549793872ull},
  {0,7,4194304,-8388608,8388608,0,4,3277298581737906186ull},
  {0,7,300,-777,555,0,4,9298881682680525323ull},
  {0,8,4096,0,0,0,9,18332272448425431057ull},
  {0,8,25600,1280,12800,0,9,2611363320990846037ull},
  {0,8,1,0,0,0,9,7130892070463514738ull},
  {0,8,4194304,-8388608,8388608,0,9,9393305712941491847ull},
  {0,8,300,-777,555,0,9,4031501167240049136ull},
  {0,9,4096,0,0,0,16,11119025996526875131ull},
  {0,9,25600,1280,12800,0,16,3397701538061704454ull},
  {0,9,1,0,0,0,16,388275648790714046ull},
  {0,9,4194304,-8388608,8388608,0,16,17061375015255174853ull},
  {0,9,300,-777,555,0,16,9660535697806377903ull},
  {0,10,4096,0,0,0,15,7857844334467848776ull},
  {0,10,25600,1280,12800,0,15,3068460334818841465ull},
  {0,10,1,0,0,0,15,12159995145924394337ull},
  {0,10,4194304,-8388608,8388608,0,15,5025321143915231981ull},
  {0,10,300,-777,555,0,15,1785167338480151167ull},
  {0,11,4096,0,0,0,5,750696123315431455ull},
  {0,11,25600,1280,12800,0,5,11229578985193257323ull},
  {0,11,1,0,0,0,5,3417277351210299507ull},
  {0,11,4194304,-8388608,8388608,0,5,3699188676018758757ull},
  {0,11,300,-777,555,0,5,5279892652099794168ull},
  {0,12,4096,0,0,0,4,10353828917121127968ull},
  {0,12,25600,1280,12800,0,4,12049117812670284961ull},
  {0,12,1,0,0,0,4,15443859173586781188ull},
  {0,12,4194304,-8388608,8388608,9,0,0ull},
  {0,12,300,-777,555,0,4,5591018756635069990ull},
  {0,13,4096,0,0,0,8,15061138054313694213ull},
  {0,13,25600,1280,12800,0,8,5017932910543006721ull},
  {0,13,1,0,0,0,8,17376923792104420389ull},
  {0,13,4194304,-8388608,8388608,9,0,0ull},
  {0,13,300,-777,555,0,8,13802405019147640197ull},
  {0,14,256,0,0,6,0,0ull},
  {0,65535,256,0,0,6,0,0ull},
  {0,1,0,0,0,5,0,0ull},
  {0,1,4194305,0,0,5,0,0ull},
  {0,1,256,8388609,0,5,0,0ull},
  {0,1,256,0,-8388609,5,0,0ull},
  {1,0,4096,0,0,0,0,0ull},
  {1,0,4194304,-8388608,8388608,0,0,0ull},
  {1,1,4096,0,0,0,4,2092072384283572290ull},
  {1,1,4194304,-8388608,8388608,0,4,15711330864137630650ull},
  {1,2,4096,0,0,0,6,9864169251943724893ull},
  {1,2,4194304,-8388608,8388608,0,6,10147830277899535340ull},
  {1,3,4096,0,0,0,5,13104087470263413081ull},
  {1,3,4194304,-8388608,8388608,0,5,1219843417965669656ull},
  {1,4,4096,0,0,0,10,7891885866058465493ull},
  {1,4,4194304,-8388608,8388608,0,10,1125940410667659045ull},
  {1,5,4096,0,0,0,6,13667918728677437154ull},
  {1,5,4194304,-8388608,8388608,0,6,4217513685351516165ull},
  {1,6,4096,0,0,0,11,3706512332903277060ull},
  {1,6,4194304,-8388608,8388608,0,11,3837091420969993943ull},
  {1,7,4096,0,0,0,4,631855975475314910ull},
  {1,7,4194304,-8388608,8388608,0,4,3277298581737906186ull},
  {1,8,4096,0,0,0,9,18332272448425431057ull},
  {1,8,4194304,-8388608,8388608,0,9,9393305712941491847ull},
  {1,9,4096,0,0,0,16,11119025996526875131ull},
  {1,9,4194304,-8388608,8388608,0,16,17061375015255174853ull},
  {1,10,4096,0,0,0,15,7857844334467848776ull},
  {1,10,4194304,-8388608,8388608,0,15,5025321143915231981ull},
  {1,11,4096,0,0,0,5,750696123315431455ull},
  {1,11,4194304,-8388608,8388608,0,5,3699188676018758757ull},
  {1,12,4096,0,0,0,4,10353828917121127968ull},
  {1,12,4194304,-8388608,8388608,9,0,0ull},
  {1,13,4096,0,0,0,8,15061138054313694213ull},
  {1,13,4194304,-8388608,8388608,9,0,0ull},
  {1,14,256,0,0,6,0,0ull},
  {1,65535,256,0,0,6,0,0ull},
  {1,1,0,0,0,5,0,0ull},
  {1,1,4194305,0,0,5,0,0ull},
  {1,1,256,8388609,0,5,0,0ull},
  {1,1,256,0,-8388609,5,0,0ull},
  {2,0,4096,0,0,0,0,0ull},
  {2,0,4194304,-8388608,8388608,0,0,0ull},
  {2,1,4096,0,0,0,4,14245677057563713891ull},
  {2,1,4194304,-8388608,8388608,0,4,3301709677117420277ull},
  {2,2,4096,0,0,0,6,18206304002872137569ull},
  {2,2,4194304,-8388608,8388608,0,6,4327650228261327280ull},
  {2,3,4096,0,0,0,5,17286531714280183323ull},
  {2,3,4194304,-8388608,8388608,0,5,10465753444242516602ull},
  {2,4,4096,0,0,0,10,8823727490345723013ull},
  {2,4,4194304,-8388608,8388608,0,10,7800636287940909849ull},
  {2,5,4096,0,0,0,6,11394800821533299486ull},
  {2,5,4194304,-8388608,8388608,0,6,10587954479777624623ull},
  {2,6,4096,0,0,0,11,5052007252165134718ull},
  {2,6,4194304,-8388608,8388608,0,11,13276312271570090999ull},
  {2,7,4096,0,0,0,4,941707409465464222ull},
  {2,7,4194304,-8388608,8388608,0,4,9326494407423055665ull},
  {2,8,4096,0,0,0,9,2491712045686502208ull},
  {2,8,4194304,-8388608,8388608,0,9,1987814817940434453ull},
  {2,9,4096,0,0,0,16,14321478418417071542ull},
  {2,9,4194304,-8388608,8388608,0,16,778669050225258527ull},
  {2,10,4096,0,0,0,15,12663472580649250293ull},
  {2,10,4194304,-8388608,8388608,0,15,7701615329974157872ull},
  {2,11,4096,0,0,0,5,16622255271991640563ull},
  {2,11,4194304,-8388608,8388608,0,5,16322926928384885851ull},
  {2,12,4096,0,0,0,4,1031124611304199589ull},
  {2,12,4194304,-8388608,8388608,9,0,0ull},
  {2,13,4096,0,0,0,8,6098698482086067193ull},
  {2,13,4194304,-8388608,8388608,9,0,0ull},
  {2,14,256,0,0,6,0,0ull},
  {2,65535,256,0,0,6,0,0ull},
  {2,1,0,0,0,5,0,0ull},
  {2,1,4194305,0,0,5,0,0ull},
  {2,1,256,8388609,0,5,0,0ull},
  {2,1,256,0,-8388609,5,0,0ull},
};
static const u32 kCps[] = {16,21,32,1280,19969,19970,32768,0,32,64,65,66,67,68,69,70,71,96,97,98,99,100,255,256,257,258,19968,65534,65535,65536,1114111,4294967295};
static const u32 kCmapExpect[3][32] = {{0,0,0,0,0,8,3,0,0,0,1,2,3,4,5,6,0,0,8,9,10,0,0,0,7,0,0,0,0,0,0,0},{0,0,0,0,0,8,3,0,0,0,1,2,3,4,5,6,0,0,8,9,10,0,0,0,7,0,0,0,0,0,0,0},{0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
static const u32 kAdvExpect[3][14] = {{500,520,540,560,580,580,580,580,580,580,580,580,580,580},{500,520,540,560,580,580,580,580,580,580,580,580,580,580},{500,520,540,560,580,600,620,640,660,680,700,720,720,720}};
static const u64 kTruncDigest = 15409705468053132896ull; // prefixes 0..size
static const u64 kFlipDigest = 3999617562428135236ull;
static const u64 kGlyfSweepDigest = 16788363500361837254ull;
static const u64 kCutDigest = 16935316737473014220ull;
static const u64 kExhaustiveDigest = 10001732077589254376ull; static const u32 kExhaustiveGlyphs = 127; static const u64 kExhaustiveFontDigest = 3569447614217599305ull;


std::vector<FontSpec> specs() {
  std::vector<int> adv;
  for (int i = 0; i < 12; ++i) adv.push_back(500 + 20 * i);
  std::vector<FontSpec> v(3);
  v[0].glyphs = kBaseGlyphs;
  v[0].adv = adv;
  v[0].nhm = 5;
  v[0].cmap = {CRec{0, 3, kCmapB}, CRec{3, 1, kCmapA}};
  v[1].glyphs = kBaseGlyphs;
  v[1].adv = adv;
  v[1].nhm = 5;
  v[1].cmap = {CRec{3, 1, kCmapA}};
  v[1].long_loca = true;
  v[2].glyphs = kBaseGlyphs;
  v[2].adv = adv;
  v[2].nhm = 12;
  v[2].cmap = {CRec{0, 3, kCmapC}};
  v[2].upem = 2048;
  v[2].tag = 0x74727565u;
  return v;
}

// A parsed font together with its bytes.
struct Fnt {
  Bytes b;
  Font f{};
  Status st = Status::Ok;
};
Fnt load(Bytes b) {
  Fnt x;
  x.b = std::move(b);
  x.st = parse_font(x.b.data(), x.b.size(), &x.f);
  return x;
}
Fnt load_spec(const FontSpec& s) { return load(build_font(s)); }

// A small font for hand-made glyph tests: glyph list as given, one hmetric, a simple cmap.
FontSpec small_spec(std::vector<GDesc> g) {
  FontSpec s;
  s.glyphs = std::move(g);
  s.adv = {100};
  s.nhm = 1;
  s.cmap = {CRec{3, 1, kCmapA}};
  s.long_loca = true;
  s.nopad = true;
  return s;
}

struct Run {
  Status st;
  u32 n;
  std::vector<Command> cmds;
};
constexpr u32 kSentinelCount = 0x5A5A5A5Au;
Run run(const Font& f, u32 g, i32 sz, i32 ox, i32 oy) {
  std::vector<Command> buf(kMaxGlyphCommands);
  std::memset(buf.data(), 0xA5, buf.size() * sizeof(Command));
  Bytes before(sizeof(Command) * kMaxGlyphCommands);
  std::memcpy(before.data(), buf.data(), before.size());
  u32 n = kSentinelCount;
  Run r;
  r.st = glyph_outline(f, g, sz, ox, oy, buf.data(), kMaxGlyphCommands, &n);
  r.n = n;
  if (r.st == Status::Ok) {
    REQUIRE(n <= kMaxGlyphCommands);
    // nothing is written past the count
    REQUIRE(std::memcmp(reinterpret_cast<const u8*>(buf.data()) + sizeof(Command) * n, before.data() + sizeof(Command) * n,
                        before.size() - sizeof(Command) * n) == 0);
    r.cmds.assign(buf.begin(), buf.begin() + n);
  } else {
    REQUIRE(n == kSentinelCount);
    REQUIRE(std::memcmp(buf.data(), before.data(), before.size()) == 0);
  }
  return r;
}
Run run_default(const Font& f, u32 g) { return run(f, g, 4096, 0, 0); }

u64 digest_ref(const std::vector<Command>& c) {  // independent of ttf::digest_commands
  FnvW w;
  for (const Command& k : c) {
    w.word(static_cast<u32>(k.op));
    w.word(static_cast<u32>(k.a.x));
    w.word(static_cast<u32>(k.a.y));
    w.word(static_cast<u32>(k.b.x));
    w.word(static_cast<u32>(k.b.y));
    w.word(static_cast<u32>(k.c.x));
    w.word(static_cast<u32>(k.c.y));
  }
  return w.h;
}

// directory helpers (the builder sorts the tables: cmap glyf head hhea hmtx loca maxp)
std::size_t rec_pos(const Bytes& b, const char* tag) {
  for (std::size_t i = 0; i < 7; ++i)
    if (std::memcmp(b.data() + 12 + 16 * i, tag, 4) == 0) return 12 + 16 * i;
  FAIL("tag not found");
  return 0;
}
std::size_t tab_off(const Bytes& b, const char* tag) {
  const std::size_t r = rec_pos(b, tag) + 8;
  return (static_cast<std::size_t>(b[r]) << 24) | (static_cast<std::size_t>(b[r + 1]) << 16) | (static_cast<std::size_t>(b[r + 2]) << 8) | b[r + 3];
}
void set16(Bytes& b, std::size_t pos, int v) {
  b[pos] = static_cast<u8>((v >> 8) & 0xFF);
  b[pos + 1] = static_cast<u8>(v & 0xFF);
}
void set32(Bytes& b, std::size_t pos, u32 v) {
  set16(b, pos, static_cast<int>(v >> 16));
  set16(b, pos + 2, static_cast<int>(v & 0xFFFFu));
}
void set_len(Bytes& b, const char* tag, u32 len) { set32(b, rec_pos(b, tag) + 12, len); }
void rename_tag(Bytes& b, const char* tag) { b[rec_pos(b, tag)] = 'X'; }
Bytes base_bytes() { return build_font(specs()[0]); }

Status parse_status(const Bytes& b) {
  Font f{};
  return parse_font(b.data(), b.size(), &f);
}

// 16-bit big endian reader for raw checks
u32 get16(const Bytes& b, std::size_t p) { return (static_cast<u32>(b[p]) << 8) | b[p + 1]; }

// ---- raw glyph helpers ----
Bytes hdr(int nc) {
  Bytes b;
  p16(b, nc);
  zeros(b, 8);
  return b;
}
struct RC {
  int flags, g, a1, a2;
};
Bytes comp_raw(const std::vector<RC>& cs, const Bytes& tail = {}) {
  Bytes b = hdr(-1);
  for (const RC& c : cs) {
    p16(b, c.flags);
    p16(b, c.g);
    if (c.flags & 1) {
      p16(b, c.a1);
      p16(b, c.a2);
    } else {
      b.push_back(static_cast<u8>(c.a1 & 0xFF));
      b.push_back(static_cast<u8>(c.a2 & 0xFF));
    }
  }
  b.insert(b.end(), tail.begin(), tail.end());
  return b;
}
// a simple glyph header that declares `last_end + 1` points in one contour and stops (no instruction length, no flags)
Bytes simple_cut(int last_end) {
  Bytes b = hdr(1);
  p16(b, last_end);
  return b;
}
GDesc line_glyph(int n) {  // one contour of n on-curve points
  std::vector<Pt> c;
  for (int i = 0; i < n; ++i) c.push_back(Pt{i * 3, (i % 7) * 5, 1});
  return S({c}, 0);
}
// simple glyph: 1 contour, 3 points, all deltas zero, flags given verbatim, then extra bytes
Bytes z3(const Bytes& flags_and_coords, int ilen = 0, int ends_last = 2) {
  Bytes b = hdr(1);
  p16(b, ends_last);
  p16(b, ilen);
  for (int i = 0; i < ilen; ++i) b.push_back(0);
  b.insert(b.end(), flags_and_coords.begin(), flags_and_coords.end());
  return b;
}

}  // namespace

namespace {

// Light outline call used inside the large sweeps (the full no-write checks run in run()).
std::vector<Command>& sweep_buf() {
  static std::vector<Command> b(kMaxGlyphCommands);
  return b;
}

void rec_words(const u8* d, std::size_t size, FnvW& w) {
  Font ft{};
  const Status s = parse_font(d, size, &ft);
  w.word(static_cast<u32>(s));
  if (s != Status::Ok) return;
  for (u32 cp : {65u, 97u, 0x100u, 0x4E00u}) {
    u32 g = 0;
    const Status st = glyph_index(ft, cp, &g);
    w.word(static_cast<u32>(st));
    w.word(st == Status::Ok ? g : 0u);
  }
  const u32 lim = ft.num_glyphs < 20 ? ft.num_glyphs : 20u;
  for (u32 g = 0; g < lim; ++g) {
    u32 a = 0;
    const Status st = glyph_advance(ft, g, &a);
    w.word(static_cast<u32>(st));
    w.word(st == Status::Ok ? a : 0u);
    std::vector<Command>& buf = sweep_buf();
    u32 n = 0;
    const Status so = glyph_outline(ft, g, 256 * 16, 0, 256 * 10, buf.data(), kMaxGlyphCommands, &n);
    w.word(static_cast<u32>(so));
    const u32 cnt = so == Status::Ok ? n : 0u;
    w.word(cnt);
    if (cnt > 0) {
      const u64 dg = digest_commands(buf.data(), cnt);
      w.word(static_cast<u32>(dg & 0xFFFFFFFFu));
      w.word(static_cast<u32>(dg >> 32));
    } else {
      w.word(0);
      w.word(0);
    }
  }
}

Status outline_status(const Fnt& f, u32 g, u32* n = nullptr) {
  Run r = run_default(f.f, g);
  if (n) *n = r.n;
  return r.st;
}

}  // namespace

TEST_CASE("the test font builder matches the reference builder byte for byte", "[ttf]") {
  const std::vector<FontSpec> sp = specs();
  for (std::size_t i = 0; i < 3; ++i) {
    const Bytes b = build_font(sp[i]);
    REQUIRE(b.size() == kFontSize[i]);
    REQUIRE(fnv_bytes(b) == kFontDigest[i]);
  }
}

TEST_CASE("status_text names every status", "[ttf]") {
  const char* names[] = {"ok",        "null buffer", "bad font",   "missing table", "unsupported",      "bad size",
                         "bad glyph", "bad cmap",    "too complex", "out of range", "buffer too small", "overlap"};
  for (int i = 0; i < 12; ++i) REQUIRE(std::string(status_text(static_cast<Status>(i))) == names[i]);
  REQUIRE(std::string(status_text(static_cast<Status>(12))) == "unknown");
  REQUIRE(std::string(status_text(static_cast<Status>(255))) == "unknown");
}

TEST_CASE("parse_font fills the view of the three test fonts", "[ttf][parse]") {
  const std::vector<FontSpec> sp = specs();
  const Fnt a = load_spec(sp[0]);
  REQUIRE(a.st == Status::Ok);
  REQUIRE(a.f.data == a.b.data());
  REQUIRE(a.f.size == a.b.size());
  REQUIRE(a.f.units_per_em == 1000);
  REQUIRE(a.f.num_glyphs == 14);
  REQUIRE(a.f.num_hmetrics == 5);
  REQUIRE_FALSE(a.f.loca_long);
  REQUIRE(a.f.loca_len == 30);
  REQUIRE(a.f.hmtx_len == 38);
  REQUIRE(a.f.loca_off == tab_off(a.b, "loca"));
  REQUIRE(a.f.glyf_off == tab_off(a.b, "glyf"));
  REQUIRE(a.f.hmtx_off == tab_off(a.b, "hmtx"));
  // the 3/1 record (the second one) is chosen, not the earlier 0/3 record
  REQUIRE(a.f.cmap_len == kCmapA.size());
  REQUIRE(std::memcmp(a.b.data() + a.f.cmap_off, kCmapA.data(), kCmapA.size()) == 0);
  const Fnt l = load_spec(sp[1]);
  REQUIRE(l.st == Status::Ok);
  REQUIRE(l.f.loca_long);
  REQUIRE(l.f.loca_len == 60);
  const Fnt t = load_spec(sp[2]);
  REQUIRE(t.st == Status::Ok);  // 'true' tag
  REQUIRE(t.f.units_per_em == 2048);
  REQUIRE(t.f.num_hmetrics == 12);
  REQUIRE(t.f.hmtx_len == 52);
  REQUIRE(t.f.cmap_len == kCmapC.size());
}

TEST_CASE("parse_font rejects a null pointer before anything else", "[ttf][parse]") {
  Bytes b = base_bytes();
  Font f{};
  REQUIRE(parse_font(nullptr, b.size(), &f) == Status::NullBuffer);
  REQUIRE(parse_font(nullptr, 0, &f) == Status::NullBuffer);
  REQUIRE(parse_font(b.data(), b.size(), nullptr) == Status::NullBuffer);
  REQUIRE(parse_font(b.data(), 3, nullptr) == Status::NullBuffer);
}

TEST_CASE("parse_font checks size, tag, directory", "[ttf][parse]") {
  Bytes b = base_bytes();
  Font f{};
  REQUIRE(parse_font(b.data(), 11, &f) == Status::BadFont);
  REQUIRE(parse_font(b.data(), 0, &f) == Status::BadFont);
  REQUIRE(parse_font(b.data(), kMaxFontBytes + 1, &f) == Status::BadFont);
  REQUIRE(parse_font(b.data(), 12, &f) == Status::BadFont);  // directory does not fit
  REQUIRE(parse_font(b.data(), 12 + 16 * 7 - 1, &f) == Status::BadFont);
  REQUIRE(parse_font(b.data(), b.size() - 1, &f) == Status::BadFont);  // last table ends one byte past the file
  REQUIRE(parse_font(b.data(), b.size(), &f) == Status::Ok);
  for (u32 tag : {0x4F54544Fu, 0x74746366u, 0u, 0x00020000u, 0x74727566u, 0x00010001u}) {
    Bytes c = b;
    set32(c, 0, tag);
    REQUIRE(parse_status(c) == Status::Unsupported);
  }
  {  // the tag is checked before the table count
    Bytes c = b;
    set32(c, 0, 0x4F54544Fu);
    set16(c, 4, 0);
    REQUIRE(parse_status(c) == Status::Unsupported);
  }
  for (int n : {0, 65, 1000, 65535}) {
    Bytes c = b;
    set16(c, 4, n);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // 64 tables are allowed by the count rule but the directory (12 + 1024) does not fit
    Bytes c = b;
    set16(c, 4, 64);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // fewer tables than needed: the rest are missing
    Bytes c = b;
    set16(c, 4, 6);
    REQUIRE(parse_status(c) == Status::MissingTable);
    set16(c, 4, 1);
    REQUIRE(parse_status(c) == Status::MissingTable);
  }
}

TEST_CASE("parse_font limits: 64 tables, 64 cmap records, the largest size, loca format", "[ttf][parse]") {
  const Bytes b = base_bytes();
  {  // 64 tables are allowed (57 empty extra tables), 65 are not
    Bytes c;
    p32(c, 0x00010000u);
    p16(c, 64);
    zeros(c, 6);
    const std::size_t shift = 16 * (64 - 7);
    for (std::size_t i = 0; i < 7; ++i) {
      c.insert(c.end(), b.begin() + static_cast<std::ptrdiff_t>(12 + 16 * i), b.begin() + static_cast<std::ptrdiff_t>(12 + 16 * i + 8));
      p32(c, static_cast<u32>(get16(b, 12 + 16 * i + 8) * 65536u + get16(b, 12 + 16 * i + 10) + shift));
      p32(c, (get16(b, 12 + 16 * i + 12) << 16) | get16(b, 12 + 16 * i + 14));
    }
    for (int i = 0; i < 57; ++i) {
      c.push_back('z');
      c.push_back(static_cast<u8>('a' + i / 26));
      c.push_back(static_cast<u8>('a' + i % 26));
      c.push_back('_');
      p32(c, 0);
      p32(c, 0);
      p32(c, 0);
    }
    c.insert(c.end(), b.begin() + 124, b.end());
    REQUIRE(c.size() == b.size() + shift);
    REQUIRE(parse_status(c) == Status::Ok);
    set16(c, 4, 65);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // exactly kMaxFontBytes is accepted
    Bytes big(kMaxFontBytes, 0);
    std::memcpy(big.data(), b.data(), b.size());
    Font f{};
    REQUIRE(parse_font(big.data(), big.size(), &f) == Status::Ok);
    REQUIRE(parse_font(big.data(), 12 + 16 * 7 - 1, &f) == Status::BadFont);
  }
  {  // 64 cmap records are allowed, 65 are not (the record array fits in both cases)
    const Bytes sub = build_cmap4({sg(65, 66, 0), sg(0xFFFF, 0xFFFF, 1)});
    FontSpec s = small_spec({E()});
    s.cmap.clear();
    for (int i = 0; i < 64; ++i) s.cmap.push_back(CRec{3, 1, sub});
    const Bytes c = build_font(s);
    REQUIRE(parse_status(c) == Status::Ok);
    Bytes d = c;
    set16(d, tab_off(d, "cmap") + 2, 65);
    REQUIRE(parse_status(d) == Status::BadFont);
  }
  {  // indexToLocFormat 2 on a font whose loca is long enough for either format
    Bytes c = build_font(specs()[1]);
    REQUIRE(parse_status(c) == Status::Ok);
    set16(c, tab_off(c, "head") + 50, 2);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // the record array is one byte short, with a record offset that would otherwise give Unsupported
    const Bytes sub = build_cmap4({sg(65, 66, 0)});
    FontSpec s = small_spec({E()});
    s.cmap = {CRec{3, 1, sub}};
    Bytes c = build_font(s);
    set32(c, tab_off(c, "cmap") + 8, 0);
    set_len(c, "cmap", 4 + 8 - 1);
    REQUIRE(parse_status(c) == Status::BadFont);
    set_len(c, "cmap", 4 + 8);
    REQUIRE(parse_status(c) == Status::Unsupported);  // offset 0 holds the cmap version, not format 4
  }
}

TEST_CASE("parse_font: a directory that ends exactly at the end of the file", "[ttf][parse]") {
  Bytes d;
  p32(d, 0x00010000u);
  p16(d, 7);
  zeros(d, 6);
  const char* tags[7] = {"cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp"};
  for (const char* t : tags) {
    for (int i = 0; i < 4; ++i) d.push_back(static_cast<u8>(t[i]));
    zeros(d, 12);
  }
  REQUIRE(d.size() == 124);
  for (std::size_t n : {std::size_t{123}, std::size_t{124}}) {
    std::unique_ptr<u8[]> p(new u8[n]);  // exact allocation
    std::memcpy(p.get(), d.data(), n);
    Font f{};
    REQUIRE(parse_font(p.get(), n, &f) == Status::BadFont);
  }
}

TEST_CASE("parse_font table bounds use 64-bit arithmetic and a tag may appear once", "[ttf][parse]") {
  const Bytes b = base_bytes();
  const std::size_t r = rec_pos(b, "glyf");
  {
    Bytes c = b;
    set32(c, r + 8, 0xFFFFFFFFu);
    set32(c, r + 12, 2);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {
    Bytes c = b;
    set32(c, r + 8, 0xFFFFFFF0u);
    set32(c, r + 12, 0x20u);  // wraps to a small value in 32-bit arithmetic
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {
    Bytes c = b;
    set32(c, r + 12, static_cast<u32>(c.size()));  // offset + length > size
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // a zero length table at the very end of the file is inside
    Bytes c = b;
    set32(c, r + 8, static_cast<u32>(c.size()));
    set32(c, r + 12, 0);
    REQUIRE(parse_status(c) == Status::Ok);
    set32(c, r + 8, static_cast<u32>(c.size()) + 1);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // duplicate tag, not adjacent; the directory error wins over the missing table it causes
    Bytes c = b;
    std::memcpy(c.data() + rec_pos(c, "maxp"), "cmap", 4);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // adjacent duplicate
    Bytes c = b;
    std::memcpy(c.data() + rec_pos(c, "glyf"), "cmap", 4);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // a bad record after a missing table is still a BadFont (the whole directory is read first)
    Bytes c = b;
    rename_tag(c, "head");
    set32(c, rec_pos(c, "maxp") + 8, 0xFFFFFFFFu);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
}

TEST_CASE("parse_font reports each missing table, and that wins over content errors", "[ttf][parse]") {
  const Bytes b = base_bytes();
  for (const char* t : {"head", "maxp", "loca", "glyf", "cmap", "hhea", "hmtx"}) {
    Bytes c = b;
    rename_tag(c, t);
    REQUIRE(parse_status(c) == Status::MissingTable);
  }
  {  // missing hmtx and a bad head magic: missing first
    Bytes c = b;
    rename_tag(c, "hmtx");
    set32(c, tab_off(c, "head") + 12, 0);
    REQUIRE(parse_status(c) == Status::MissingTable);
  }
  {  // missing glyf and an unsupported maxp: missing first
    Bytes c = b;
    rename_tag(c, "glyf");
    set32(c, tab_off(c, "maxp"), 0x00020000u);
    REQUIRE(parse_status(c) == Status::MissingTable);
  }
}

TEST_CASE("parse_font head, maxp, loca, hhea, hmtx rules", "[ttf][parse]") {
  const Bytes b = base_bytes();
  const std::size_t h = tab_off(b, "head"), m = tab_off(b, "maxp"), hh = tab_off(b, "hhea");
  auto bad = [&](auto&& edit, Status want) {
    Bytes c = b;
    edit(c);
    REQUIRE(parse_status(c) == want);
  };
  bad([&](Bytes& c) { set_len(c, "head", 53); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "head", 54); }, Status::Ok);
  bad([&](Bytes& c) { set32(c, h + 12, 0x5F0F3CF4u); }, Status::BadFont);
  bad([&](Bytes& c) { set32(c, h + 12, 0xDF0F3CF5u); }, Status::BadFont);
  for (int u : {0, 15, 16385, 65535}) bad([&](Bytes& c) { set16(c, h + 18, u); }, Status::BadFont);
  for (int u : {16, 17, 2048, 16384}) bad([&](Bytes& c) { set16(c, h + 18, u); }, Status::Ok);
  bad([&](Bytes& c) { set16(c, h + 50, 2); }, Status::BadFont);
  bad([&](Bytes& c) { set16(c, h + 50, 256); }, Status::BadFont);
  bad([&](Bytes& c) { set16(c, h + 50, 1); }, Status::BadFont);  // says long but loca is short (30 < 60)
  bad([&](Bytes& c) { set_len(c, "maxp", 5); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "maxp", 6); }, Status::Ok);
  bad([&](Bytes& c) { set32(c, m, 0x00020000u); }, Status::Unsupported);
  bad([&](Bytes& c) { set32(c, m, 0x00005000u); }, Status::Unsupported);
  bad([&](Bytes& c) { set32(c, m, 0x00010001u); }, Status::Unsupported);
  bad([&](Bytes& c) { set16(c, m + 4, 0); }, Status::BadFont);
  bad([&](Bytes& c) { set16(c, m + 4, 15); }, Status::BadFont);  // loca (30 bytes) needs 32
  bad([&](Bytes& c) { set16(c, m + 4, 14); }, Status::Ok);
  bad([&](Bytes& c) { set16(c, m + 4, 65535); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "loca", 29); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "loca", 30); }, Status::Ok);
  bad([&](Bytes& c) { set_len(c, "loca", 0); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "hhea", 35); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "hhea", 36); }, Status::Ok);
  bad([&](Bytes& c) { set16(c, hh + 34, 0); }, Status::BadFont);
  bad([&](Bytes& c) { set16(c, hh + 34, 15); }, Status::BadFont);
  bad([&](Bytes& c) { set16(c, hh + 34, 14); }, Status::BadFont);  // hmtx (38 bytes) needs 56
  bad([&](Bytes& c) { set16(c, hh + 34, 6); }, Status::BadFont);   // needs 24 + 16 = 40 > 38
  bad([&](Bytes& c) { set_len(c, "hmtx", 37); }, Status::BadFont);
  bad([&](Bytes& c) { set_len(c, "hmtx", 38); }, Status::Ok);
  {
    Bytes c = b;
    set16(c, hh + 34, 6);
    set_len(c, "hmtx", 40);
    REQUIRE(parse_status(c) == Status::Ok);
    set_len(c, "hmtx", 39);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // glyf has no content check: an empty glyf is fine for parse_font
    Bytes c = b;
    set_len(c, "glyf", 0);
    REQUIRE(parse_status(c) == Status::Ok);
  }
}

TEST_CASE("decision: parse_font runs the content checks in the order head, maxp, loca, hhea+hmtx, cmap", "[ttf][parse][decision]") {
  const Bytes b = base_bytes();
  const std::size_t h = tab_off(b, "head"), m = tab_off(b, "maxp"), hh = tab_off(b, "hhea");
  const std::size_t cm = tab_off(b, "cmap");
  {  // head BadFont beats maxp Unsupported
    Bytes c = b;
    set32(c, h + 12, 0);
    set32(c, m, 0x00020000u);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // maxp Unsupported beats the loca / hhea / hmtx / cmap BadFont
    Bytes c = b;
    set32(c, m, 0x00020000u);
    set_len(c, "loca", 0);
    REQUIRE(parse_status(c) == Status::Unsupported);
    set_len(c, "loca", 30);
    set_len(c, "hhea", 0);
    REQUIRE(parse_status(c) == Status::Unsupported);
    set_len(c, "hhea", 36);
    set_len(c, "hmtx", 0);
    REQUIRE(parse_status(c) == Status::Unsupported);
    set_len(c, "hmtx", 38);
    set16(c, cm, 1);
    REQUIRE(parse_status(c) == Status::Unsupported);
  }
  {  // loca, hhea, hmtx BadFont beat the cmap Unsupported (no usable record)
    Bytes c = b;
    set16(c, cm + 2, 0);
    REQUIRE(parse_status(c) == Status::Unsupported);
    set_len(c, "loca", 0);
    REQUIRE(parse_status(c) == Status::BadFont);
    set_len(c, "loca", 30);
    set16(c, hh + 34, 0);
    REQUIRE(parse_status(c) == Status::BadFont);
    set16(c, hh + 34, 5);
    set_len(c, "hmtx", 0);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
}

TEST_CASE("parse_font cmap rules", "[ttf][parse][cmap]") {
  const Bytes sub4 = build_cmap4({sg(65, 66, 0), sg(0xFFFF, 0xFFFF, 1)});
  Bytes sub6 = sub4;
  set16(sub6, 0, 6);
  auto mk = [&](std::vector<CRec> r) {
    FontSpec s = small_spec({E()});
    s.cmap = std::move(r);
    return build_font(s);
  };
  REQUIRE(parse_status(mk({CRec{3, 1, sub4}})) == Status::Ok);
  REQUIRE(parse_status(mk({CRec{0, 3, sub4}})) == Status::Ok);
  REQUIRE(parse_status(mk({})) == Status::Unsupported);
  REQUIRE(parse_status(mk({CRec{1, 0, sub4}})) == Status::Unsupported);
  REQUIRE(parse_status(mk({CRec{3, 0, sub4}})) == Status::Unsupported);
  REQUIRE(parse_status(mk({CRec{0, 4, sub4}})) == Status::Unsupported);
  REQUIRE(parse_status(mk({CRec{3, 1, sub6}})) == Status::Unsupported);
  // the chosen record decides; there is no fallback to another record
  REQUIRE(parse_status(mk({CRec{3, 1, sub6}, CRec{0, 3, sub4}})) == Status::Unsupported);
  REQUIRE(parse_status(mk({CRec{0, 3, sub6}, CRec{3, 1, sub4}})) == Status::Ok);
  REQUIRE(parse_status(mk({CRec{3, 1, sub6}, CRec{3, 1, sub4}})) == Status::Unsupported);  // the first 3/1 wins
  REQUIRE(parse_status(mk({CRec{0, 3, sub6}, CRec{0, 3, sub4}})) == Status::Unsupported);
  {  // every record's offset must have 4 bytes inside the table, chosen or not
    Bytes c = mk({CRec{3, 1, sub4}, CRec{1, 0, sub4}});
    const std::size_t co = tab_off(c, "cmap");
    const u32 clen = static_cast<u32>(4 + 16 + 2 * sub4.size());
    set32(c, co + 4 + 8 + 4, clen - 4);
    REQUIRE(parse_status(c) == Status::Ok);
    set32(c, co + 4 + 8 + 4, clen - 3);
    REQUIRE(parse_status(c) == Status::BadFont);
    set32(c, co + 4 + 8 + 4, 0xFFFFFFFFu);
    REQUIRE(parse_status(c) == Status::BadFont);
  }
  {  // header: length < 4, version, table count
    const Bytes c = mk({CRec{3, 1, sub4}});
    Bytes d = c;
    set_len(d, "cmap", 3);
    REQUIRE(parse_status(d) == Status::BadFont);
    d = c;
    set16(d, tab_off(d, "cmap"), 1);
    REQUIRE(parse_status(d) == Status::BadFont);
    d = c;
    set16(d, tab_off(d, "cmap") + 2, 65);
    REQUIRE(parse_status(d) == Status::BadFont);
    d = c;
    set16(d, tab_off(d, "cmap") + 2, 64);  // the record array does not fit
    REQUIRE(parse_status(d) == Status::BadFont);
    d = c;
    set_len(d, "cmap", 4 + 8 - 1);  // the single record does not fit
    REQUIRE(parse_status(d) == Status::BadFont);
  }
  {  // subtable: the 4 bytes at the offset (format), then segCountX2 needs offset + 8, then the arrays
    REQUIRE(parse_status(mk({CRec{3, 1, Bytes{0, 4, 0, 4}}})) == Status::BadFont);
    REQUIRE(parse_status(mk({CRec{3, 1, Bytes{0, 4, 0, 4, 0, 0, 0}}})) == Status::BadFont);
    REQUIRE(parse_status(mk({CRec{3, 1, Bytes{0, 4, 0}}})) == Status::BadFont);  // fewer than 4 bytes at the offset
    Bytes one = build_cmap4({sg(65, 66, 0)});
    REQUIRE(one.size() == 24);
    REQUIRE(parse_status(mk({CRec{3, 1, one}})) == Status::Ok);
    Bytes cut = one;
    cut.pop_back();
    REQUIRE(parse_status(mk({CRec{3, 1, cut}})) == Status::BadFont);
    Bytes zero = one;
    set16(zero, 6, 0);
    REQUIRE(parse_status(mk({CRec{3, 1, zero}})) == Status::BadFont);
    Bytes odd = one;
    set16(odd, 6, 3);
    REQUIRE(parse_status(mk({CRec{3, 1, odd}})) == Status::BadFont);
    Bytes big = one;
    set16(big, 6, 0xFFFE);
    REQUIRE(parse_status(mk({CRec{3, 1, big}})) == Status::BadFont);
    Bytes four = one;
    set16(four, 6, 4);  // two segments need 16 + 8 = 24 bytes plus the extra ... 32 in total
    REQUIRE(parse_status(mk({CRec{3, 1, four}})) == Status::BadFont);
  }
  {  // a subtable that is not last extends to the end of the cmap table (the length field is ignored)
    Bytes a = sub4;
    set16(a, 2, 0);
    Fnt f = load(mk({CRec{3, 1, a}, CRec{1, 0, sub4}}));
    REQUIRE(f.st == Status::Ok);
    REQUIRE(f.f.cmap_len == a.size() + sub4.size());
  }
}

TEST_CASE("glyph_index matches the reference on all test fonts", "[ttf][cmap]") {
  const std::vector<FontSpec> sp = specs();
  const std::size_t ncp = sizeof(kCps) / sizeof(kCps[0]);
  for (std::size_t vi = 0; vi < 3; ++vi) {
    const Fnt f = load_spec(sp[vi]);
    REQUIRE(f.st == Status::Ok);
    for (std::size_t i = 0; i < ncp; ++i) {
      u32 g = 0xDEADu;
      REQUIRE(glyph_index(f.f, kCps[i], &g) == Status::Ok);
      INFO("font " << vi << " cp " << kCps[i]);
      REQUIRE(g == kCmapExpect[vi][i]);
    }
  }
}

TEST_CASE("glyph_index handles nulls and writes nothing on error", "[ttf][cmap]") {
  const Fnt f = load_spec(specs()[0]);
  u32 g = 0x1234;
  Font z{};
  REQUIRE(glyph_index(z, 65, &g) == Status::NullBuffer);
  REQUIRE(glyph_index(f.f, 65, nullptr) == Status::NullBuffer);
  REQUIRE(glyph_index(z, 0x10000, &g) == Status::NullBuffer);
  REQUIRE(glyph_index(f.f, 0x10000, nullptr) == Status::NullBuffer);
  REQUIRE(g == 0x1234);
  REQUIRE(glyph_index(f.f, 0x10000, &g) == Status::Ok);
  REQUIRE(g == 0);
  g = 77;
  REQUIRE(glyph_index(f.f, 0xFFFFFFFFu, &g) == Status::Ok);
  REQUIRE(g == 0);
}

TEST_CASE("glyph_index: format 4 corner cases", "[ttf][cmap]") {
  // wrap of the id delta modulo 65536, a stored 0 stays 0, a result at or above numGlyphs is 0, the first matching segment wins
  const Bytes sub = build_cmap4({sg(0x40, 0x40, (5 - 0x40) & 0xFFFF), sgg(0x50, 0x50, (2 - 5) & 0xFFFF, {5}), sgg(0x61, 0x63, 0, {1, 0, 2}), sgg(0x70, 0x71, 7, {0, 3}),
                                  sg(0x80, 0x80, 0xFFFF), sg(0x90, 0x90, 20), sg(0xA0, 0xAF, 5), sg(0x100, 0x100, 0), sg(0x80, 0x90, 1)});
  FontSpec s = small_spec({E(), E(), E(), E(), E(), E(), E(), E(), E(), E()});
  s.cmap = {CRec{3, 1, sub}};
  const Fnt f = load_spec(s);
  REQUIRE(f.st == Status::Ok);
  auto idx = [&](u32 cp) {
    u32 g = 999;
    REQUIRE(glyph_index(f.f, cp, &g) == Status::Ok);
    return g;
  };
  REQUIRE(idx(0x40) == 5);  // 0x40 + 0xFFC5 wraps to 5
  REQUIRE(idx(0x50) == 2);  // 5 + 0xFFFD wraps to 2
  REQUIRE(idx(0x61) == 1);
  REQUIRE(idx(0x62) == 0);  // stored 0
  REQUIRE(idx(0x63) == 2);
  REQUIRE(idx(0x70) == 0);  // stored 0, the delta is not added
  REQUIRE(idx(0x71) == 0);  // 3 + 7 = 10 is not below numGlyphs
  REQUIRE(idx(0x80) == 0);  // 0x80 + 0xFFFF mod 65536 = 0x7F, beyond numGlyphs
  REQUIRE(idx(0x90) == 0);  // 0x90 + 20 beyond numGlyphs; the later segment 0x80 .. 0x90 is never reached
  REQUIRE(idx(0xA0) == 0);
  REQUIRE(idx(0x100) == 0);
  REQUIRE(idx(0x101) == 0);
  REQUIRE(idx(0x60) == 0);  // below the first start in its segment
  REQUIRE(idx(0x64) == 0);  // a gap between segments
}

TEST_CASE("glyph_index: idRangeOffset reads stay inside the subtable", "[ttf][cmap]") {
  const Bytes sub = build_cmap4({sgg(0x61, 0x63, 0, {1, 2, 1}), sg(0xFFFF, 0xFFFF, 1)});
  const std::size_t L = sub.size();
  const std::size_t ro_pos = 16 + 6 * 2;  // idRangeOffset[0]
  auto lookup = [&](int ro, u32 cp, u32* g) {
    Bytes t = sub;
    set16(t, ro_pos, ro);
    FontSpec s = small_spec({E(), E(), E()});
    s.cmap = {CRec{3, 1, t}};
    const Fnt f = load_spec(s);
    REQUIRE(f.st == Status::Ok);
    return glyph_index(f.f, cp, g);
  };
  u32 g = 555;
  // the glyph id of cp 0x63 sits at ro_pos + ro + 4; the last legal word starts at L - 2
  const int ro_last = static_cast<int>(L - 2 - ro_pos - 4);
  REQUIRE(lookup(ro_last, 0x63, &g) == Status::Ok);
  REQUIRE(g == 1);
  g = 555;
  REQUIRE(lookup(ro_last + 1, 0x63, &g) == Status::BadCmap);
  REQUIRE(g == 555);
  REQUIRE(lookup(ro_last + 1, 0x61, &g) == Status::Ok);  // cp 0x61 reads two words earlier
  REQUIRE(lookup(ro_last + 2, 0x63, &g) == Status::BadCmap);
  g = 555;
  REQUIRE(lookup(0xFFFF, 0x61, &g) == Status::BadCmap);
  REQUIRE(g == 555);
  REQUIRE(lookup(0xFFFF, 0x60, &g) == Status::Ok);  // not in the segment: no read
  REQUIRE(g == 0);
  REQUIRE(lookup(0xFFFF, 0x64, &g) == Status::Ok);
  const int ro_first = static_cast<int>(L - 2 - ro_pos);  // cp 0x61 reads the very last word
  REQUIRE(lookup(ro_first, 0x61, &g) == Status::Ok);
  REQUIRE(lookup(ro_first + 1, 0x61, &g) == Status::BadCmap);
}

TEST_CASE("glyph_index: a hand edited view that cuts the subtable gives BadCmap", "[ttf][cmap]") {
  const Fnt f = load_spec(specs()[0]);
  Font v = f.f;
  u32 g = 9;
  v.cmap_len = 15;
  REQUIRE(glyph_index(v, 65, &g) == Status::BadCmap);
  v.cmap_len = 16 + 4 * get16(kCmapA, 6) - 1;  // the arrays do not fit
  REQUIRE(glyph_index(v, 65, &g) == Status::BadCmap);
  REQUIRE(g == 9);
}

TEST_CASE("glyph_advance matches the reference and checks its arguments", "[ttf][hmtx]") {
  const std::vector<FontSpec> sp = specs();
  for (std::size_t vi = 0; vi < 3; ++vi) {
    const Fnt f = load_spec(sp[vi]);
    for (u32 g = 0; g < 14; ++g) {
      u32 a = 0xAAAA;
      REQUIRE(glyph_advance(f.f, g, &a) == Status::Ok);
      INFO("font " << vi << " glyph " << g);
      REQUIRE(a == kAdvExpect[vi][g]);
    }
    u32 a = 0xAAAA;
    REQUIRE(glyph_advance(f.f, 14, &a) == Status::BadGlyph);
    REQUIRE(glyph_advance(f.f, 0xFFFFFFFFu, &a) == Status::BadGlyph);
    REQUIRE(glyph_advance(f.f, 65536, &a) == Status::BadGlyph);
    REQUIRE(a == 0xAAAA);
  }
  const Fnt f = load_spec(sp[0]);
  u32 a = 0xAAAA;
  Font z{};
  REQUIRE(glyph_advance(z, 0, &a) == Status::NullBuffer);
  REQUIRE(glyph_advance(f.f, 0, nullptr) == Status::NullBuffer);
  REQUIRE(glyph_advance(z, 99, &a) == Status::NullBuffer);
  REQUIRE(glyph_advance(f.f, 99, nullptr) == Status::NullBuffer);
  REQUIRE(a == 0xAAAA);
  // glyphs beyond numberOfHMetrics repeat the last advance (glyph 4 is the last entry of the base font)
  u32 last = 0, beyond = 0;
  REQUIRE(glyph_advance(f.f, 4, &last) == Status::Ok);
  REQUIRE(glyph_advance(f.f, 13, &beyond) == Status::Ok);
  REQUIRE(last == beyond);
}

TEST_CASE("decision: glyph_advance reads only inside the hmtx table", "[ttf][hmtx][decision]") {
  const Fnt f = load_spec(specs()[0]);
  Font v = f.f;
  u32 a = 0xAAAA;
  v.hmtx_len = 4 * 4 + 3;  // entry 4 (bytes 16 .. 19) is not complete
  REQUIRE(glyph_advance(v, 3, &a) == Status::Ok);
  REQUIRE(glyph_advance(v, 4, &a) == Status::BadGlyph);
  REQUIRE(glyph_advance(v, 9, &a) == Status::BadGlyph);
  v.hmtx_len = 18;
  REQUIRE(glyph_advance(v, 4, &a) == Status::BadGlyph);
  v.hmtx_len = 4 * 5;
  REQUIRE(glyph_advance(v, 4, &a) == Status::Ok);
}

TEST_CASE("glyph_outline reproduces the reference rows", "[ttf][outline]") {
  const std::vector<FontSpec> sp = specs();
  const Fnt fonts[3] = {load_spec(sp[0]), load_spec(sp[1]), load_spec(sp[2])};
  for (const OutRow& r : kOutRows) {
    INFO("row font " << r.vi << " glyph " << r.g << " size " << r.sz << " origin " << r.ox << "," << r.oy);
    const Run x = run(fonts[r.vi].f, uc(r.g), r.sz, r.ox, r.oy);
    REQUIRE(static_cast<int>(x.st) == r.st);
    if (x.st == Status::Ok) {
      REQUIRE(x.n == uc(r.n));
      if (r.n > 0) {
        REQUIRE(digest_ref(x.cmds) == r.dig);
        REQUIRE(digest_commands(x.cmds.data(), x.cmds.size()) == r.dig);
      }
    }
  }
}

TEST_CASE("digest_commands hashes zero bytes for null or empty input", "[ttf]") {
  REQUIRE(digest_commands(nullptr, 0) == kFnvBasis);
  REQUIRE(digest_commands(nullptr, 5) == kFnvBasis);
  Command c{};
  REQUIRE(digest_commands(&c, 0) == kFnvBasis);
  c.op = Op::Quad;
  c.a = {1, -2};
  c.b = {3, 4};
  c.c = {-5, 6};
  REQUIRE(digest_commands(&c, 1) == digest_ref(std::vector<Command>{c}));
}

TEST_CASE("glyph_outline: hand computed triangle and rounding", "[ttf][outline]") {
  const Fnt f = load_spec(specs()[0]);  // unitsPerEm 1000, glyph 1 = triangle (0,0) (600,0) (300,700)
  const Run r = run(f.f, 1, 4096, 0, 0);
  REQUIRE(r.st == Status::Ok);
  REQUIRE(r.n == 4);
  REQUIRE(r.cmds[0].op == Op::Move);
  REQUIRE(r.cmds[0].a.x == 0);
  REQUIRE(r.cmds[0].a.y == 0);
  REQUIRE(r.cmds[1].op == Op::Line);
  REQUIRE(r.cmds[1].a.x == 2458);  // 600 * 4.096 = 2457.6
  REQUIRE(r.cmds[1].a.y == 0);
  REQUIRE(r.cmds[2].op == Op::Line);
  REQUIRE(r.cmds[2].a.x == 1229);   // 1228.8
  REQUIRE(r.cmds[2].a.y == -2867);  // -(2867.2)
  REQUIRE(r.cmds[3].op == Op::Close);
  REQUIRE(r.cmds[3].a.x == 0);
  REQUIRE(r.cmds[3].b.x == 0);
  REQUIRE(r.cmds[3].c.y == 0);
  // the origin is added after rounding, y grows down
  const Run o = run(f.f, 1, 4096, 100, 200);
  REQUIRE(o.cmds[0].a.x == 100);
  REQUIRE(o.cmds[0].a.y == 200);
  REQUIRE(o.cmds[2].a.x == 1329);
  REQUIRE(o.cmds[2].a.y == 200 - 2867);
  // ties are rounded away from zero, also for negative values
  const Fnt t = load_spec(small_spec({E(), S({{{0, 0, 1}, {250, 0, 1}, {-250, -250, 1}}}, 0), S({{{0, 0, 1}, {750, 0, 1}, {0, 750, 1}}}, 0)}));
  const Run a = run(t.f, 1, 2, 0, 0);  // 250 * 2 / 1000 = 0.5
  REQUIRE(a.st == Status::Ok);
  REQUIRE(a.cmds[1].a.x == 1);
  REQUIRE(a.cmds[1].a.y == 0);
  REQUIRE(a.cmds[2].a.x == -1);
  REQUIRE(a.cmds[2].a.y == 1);  // y = 0 - round(-0.5) = 1
  const Run b = run(t.f, 2, 2, 0, 0);  // 750 * 2 / 1000 = 1.5
  REQUIRE(b.cmds[1].a.x == 2);
  REQUIRE(b.cmds[2].a.y == -2);
}

TEST_CASE("decision: the contour walk starts at the first on-curve, else the last, else the exact midpoint", "[ttf][outline][decision]") {
  const Fnt f = load_spec(small_spec({E(),
                                      S({{{0, 0, 1}, {100, 0, 0}, {200, 100, 1}}}, 0),             // on, off, on
                                      S({{{0, 0, 0}, {100, 0, 0}, {100, 100, 1}}}, 0),             // off off on: start at the last
                                      S({{{0, 0, 0}, {100, 0, 1}, {100, 100, 0}}}, 0),             // off on off: midpoint start
                                      S({{{0, 0, 1}, {100, 0, 0}, {200, 0, 0}, {300, 0, 1}}}, 0),  // two controls in a row
                                      S({{{0, 0, 1}, {100, 100, 0}}}, 0),                          // two points, one control
                                      S({{{0, 0, 1}, {100, 100, 1}}}, 0),                          // two points, a line
                                      S({{{1, 1, 0}, {4, 1, 1}, {2, 4, 0}}}, 0)}));                // odd coordinates
  auto shape = [&](u32 g, i32 size) {
    const Run r = run(f.f, g, size, 0, 0);  // size 1000 at upem 1000: fx = font unit
    REQUIRE(r.st == Status::Ok);
    std::string s;
    for (const Command& c : r.cmds) {
      s += "MLQCX"[static_cast<int>(c.op)];
      s += "(" + std::to_string(c.a.x) + "," + std::to_string(c.a.y);
      if (c.op == Op::Quad) s += ";" + std::to_string(c.b.x) + "," + std::to_string(c.b.y);
      s += ")";
    }
    return s;
  };
  REQUIRE(shape(1, 1000) == "M(0,0)Q(100,0;200,-100)X(0,0)");
  // off off on: the last point is the start, the walk covers points 0 and 1
  REQUIRE(shape(2, 1000) == "M(100,-100)Q(0,0;50,0)Q(100,0;100,-100)X(0,0)");
  // off on off: no on-curve at the ends: start at the midpoint of the first and last points
  REQUIRE(shape(3, 1000) == "M(50,-50)Q(0,0;100,0)Q(100,-100;50,-50)X(0,0)");
  REQUIRE(shape(4, 1000) == "M(0,0)Q(100,0;150,0)Q(200,0;300,0)X(0,0)");
  REQUIRE(shape(5, 1000) == "M(0,0)Q(100,-100;0,0)X(0,0)");
  REQUIRE(shape(6, 1000) == "M(0,0)L(100,-100)X(0,0)");
  // midpoints are exact in doubled units: the start is (1.5, 2.5) font units, size 2000 doubles it
  const Run r = run(f.f, 7, 2000, 0, 0);
  REQUIRE(r.st == Status::Ok);
  REQUIRE(r.cmds[0].op == Op::Move);
  REQUIRE(r.cmds[0].a.x == 3);
  REQUIRE(r.cmds[0].a.y == -5);
}

TEST_CASE("glyph_outline command bound", "[ttf][outline]") {
  std::vector<std::vector<Pt>> cs;
  for (int i = 0; i < 170; ++i) cs.push_back({Pt{i, 0, 0}, Pt{i, 5, 0}});  // 170 contours of two off-curve points: 4 commands each
  const Fnt f = load_spec(small_spec({E(), S(cs, 0), line_glyph(340), S({{Pt{0, 0, 0}, Pt{1, 0, 0}, Pt{1, 1, 0}}}, 0)}));
  u32 n = 0;
  REQUIRE(outline_status(f, 1, &n) == Status::Ok);
  REQUIRE(n == 680);
  REQUIRE(outline_status(f, 2, &n) == Status::Ok);
  REQUIRE(n == 341);
  REQUIRE(outline_status(f, 3, &n) == Status::Ok);
  REQUIRE(n == 5);  // midpoint start, three quads, close
}

TEST_CASE("glyph_outline argument order", "[ttf][outline]") {
  const Fnt f = load_spec(specs()[0]);
  std::vector<Command> out(kMaxGlyphCommands);
  u32 n = 123;
  const u32 cap = kMaxGlyphCommands;
  Font z{};
  REQUIRE(glyph_outline(z, 1, 4096, 0, 0, out.data(), cap, &n) == Status::NullBuffer);
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, nullptr, cap, &n) == Status::NullBuffer);
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, out.data(), cap, nullptr) == Status::NullBuffer);
  // NullBuffer comes first even with a bad size, a bad glyph and a small capacity
  REQUIRE(glyph_outline(z, 99, 0, 0, 0, out.data(), 0, &n) == Status::NullBuffer);
  REQUIRE(glyph_outline(f.f, 99, 0, 0, 0, nullptr, 0, &n) == Status::NullBuffer);
  REQUIRE(glyph_outline(f.f, 99, 0, 0, 0, out.data(), 0, nullptr) == Status::NullBuffer);
  // BadSize before BadGlyph before BufferTooSmall
  REQUIRE(glyph_outline(f.f, 99, 0, 0, 0, out.data(), 0, &n) == Status::BadSize);
  REQUIRE(glyph_outline(f.f, 99, 4096, 0, 0, out.data(), 0, &n) == Status::BadGlyph);
  REQUIRE(glyph_outline(f.f, 14, 4096, 0, 0, out.data(), cap, &n) == Status::BadGlyph);
  REQUIRE(glyph_outline(f.f, 14, 4096, 0, 0, out.data(), cap - 1, &n) == Status::BadGlyph);  // before the capacity check
  REQUIRE(glyph_outline(f.f, 0xFFFFFFFFu, 4096, 0, 0, out.data(), cap, &n) == Status::BadGlyph);
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, out.data(), cap - 1, &n) == Status::BufferTooSmall);
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, out.data(), 0, &n) == Status::BufferTooSmall);
  REQUIRE(glyph_outline(f.f, 0, 4096, 0, 0, out.data(), cap - 1, &n) == Status::BufferTooSmall);  // data independent
  REQUIRE(n == 123);
  for (i32 s : {0, -1, -1000000, static_cast<i32>(-2147483647 - 1), kMaxSizeFx + 1, 2147483647})
    REQUIRE(glyph_outline(f.f, 1, s, 0, 0, out.data(), cap, &n) == Status::BadSize);
  for (i32 s : {1, 2, kMaxSizeFx}) REQUIRE(glyph_outline(f.f, 0, s, 0, 0, out.data(), cap, &n) == Status::Ok);
  for (i32 o : {kMaxOriginFx + 1, -kMaxOriginFx - 1, 2147483647, static_cast<i32>(-2147483647 - 1)}) {
    REQUIRE(glyph_outline(f.f, 1, 4096, o, 0, out.data(), cap, &n) == Status::BadSize);
    REQUIRE(glyph_outline(f.f, 1, 4096, 0, o, out.data(), cap, &n) == Status::BadSize);
  }
  for (i32 o : {kMaxOriginFx, -kMaxOriginFx}) {
    REQUIRE(glyph_outline(f.f, 0, 4096, o, 0, out.data(), cap, &n) == Status::Ok);
    REQUIRE(glyph_outline(f.f, 0, 4096, 0, o, out.data(), cap, &n) == Status::Ok);
  }
  // a larger capacity is fine and does not change the result
  std::vector<Command> big(5000);
  u32 n2 = 0;
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, big.data(), 5000, &n2) == Status::Ok);
  REQUIRE(n2 == 4);
  REQUIRE(digest_commands(big.data(), 4) == digest_ref(std::vector<Command>(big.begin(), big.begin() + 4)));
}

TEST_CASE("decision: glyph_outline checks overlap in three pairs and out always spans kMaxGlyphCommands commands", "[ttf][outline][overlap][decision]") {
  const Fnt f = load_spec(specs()[0]);
  const std::size_t K = kMaxGlyphCommands;
  std::vector<Command> arena(3 * K);
  u8* font_at = reinterpret_cast<u8*>(arena.data() + K);
  std::memcpy(font_at, f.b.data(), f.b.size());
  Font v = f.f;
  v.data = font_at;
  u32 cnt = 0;
  const u32 cap = kMaxGlyphCommands;
  // out directly before the font bytes: no overlap
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data(), cap, &cnt) == Status::Ok);
  REQUIRE(cnt == 4);
  REQUIRE(std::memcmp(font_at, f.b.data(), f.b.size()) == 0);
  // one command further: the end of out is inside the font bytes
  cnt = 77;
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + 1, cap, &cnt) == Status::Overlap);
  REQUIRE(cnt == 77);
  // out starting at the font, and inside the font
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + K, cap, &cnt) == Status::Overlap);
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + K + 10, cap, &cnt) == Status::Overlap);
  // out starting right after the last font byte
  const std::size_t after = (f.b.size() + sizeof(Command) - 1) / sizeof(Command);
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + K + after, cap, &cnt) == Status::Ok);
  REQUIRE(std::memcmp(font_at, f.b.data(), f.b.size()) == 0);
  // the extent of out is 1020 commands, whatever the capacity: a big capacity does not reach into the font
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data(), 2 * cap, &cnt) == Status::Ok);
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + 1, 2 * cap, &cnt) == Status::Overlap);
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + K, 2 * cap, &cnt) == Status::Overlap);
  // out_count against out
  std::vector<Command> ob(K);
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, ob.data(), cap, reinterpret_cast<u32*>(ob.data())) == Status::Overlap);
  REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, ob.data(), cap, reinterpret_cast<u32*>(reinterpret_cast<u8*>(ob.data()) + (K * sizeof(Command) - 4))) ==
          Status::Overlap);
  {  // out_count just outside out, on both sides
    std::vector<Command> wide(K + 8);
    REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, wide.data() + 1, cap, reinterpret_cast<u32*>(wide.data())) == Status::Ok);
    REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, wide.data(), cap, reinterpret_cast<u32*>(wide.data() + K)) == Status::Ok);
    REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, wide.data(), cap, reinterpret_cast<u32*>(reinterpret_cast<u8*>(wide.data() + K) - 4)) ==
            Status::Overlap);
  }
  {  // out_count that ends inside the first byte of out (it is 4 bytes wide)
    std::vector<Command> wide(K + 8);
    u8* start = reinterpret_cast<u8*>(wide.data() + 1);
    REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, wide.data() + 1, cap, reinterpret_cast<u32*>(start - 3)) == Status::Overlap);
    REQUIRE(glyph_outline(f.f, 1, 4096, 0, 0, wide.data() + 1, cap, reinterpret_cast<u32*>(start - 4)) == Status::Ok);
  }
  // out_count (4 bytes) against the font bytes
  std::vector<u32> words(1000);
  u8* fb = reinterpret_cast<u8*>(words.data() + 100);
  std::memcpy(fb, f.b.data(), f.b.size());
  Font w = f.f;
  w.data = fb;
  const std::size_t fwords = f.b.size() / 4;
  REQUIRE(f.b.size() % 4 == 0);
  REQUIRE(glyph_outline(w, 1, 4096, 0, 0, ob.data(), cap, words.data() + 99) == Status::Ok);
  REQUIRE(glyph_outline(w, 1, 4096, 0, 0, ob.data(), cap, words.data() + 100) == Status::Overlap);
  REQUIRE(glyph_outline(w, 1, 4096, 0, 0, ob.data(), cap, words.data() + 100 + fwords - 1) == Status::Overlap);
  REQUIRE(glyph_outline(w, 1, 4096, 0, 0, ob.data(), cap, words.data() + 100 + fwords) == Status::Ok);
  REQUIRE(std::memcmp(fb, f.b.data(), f.b.size()) == 0);
  // order: BufferTooSmall before Overlap, BadGlyph and BadSize before both
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + 1, cap - 1, &cnt) == Status::BufferTooSmall);
  REQUIRE(glyph_outline(v, 99, 4096, 0, 0, arena.data() + 1, cap, &cnt) == Status::BadGlyph);
  REQUIRE(glyph_outline(v, 1, 0, 0, 0, arena.data() + 1, cap, &cnt) == Status::BadSize);
}

TEST_CASE("decision: overlap is decided before the data dependent glyph errors", "[ttf][outline][overlap][decision]") {
  const Fnt f = load_spec(small_spec({E(), line_glyph(341)}));
  const std::size_t K = kMaxGlyphCommands;
  std::vector<Command> arena(3 * K);
  u8* font_at = reinterpret_cast<u8*>(arena.data() + K);
  std::memcpy(font_at, f.b.data(), f.b.size());
  Font v = f.f;
  v.data = font_at;
  u32 cnt = 0;
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data() + 1, kMaxGlyphCommands, &cnt) == Status::Overlap);
  REQUIRE(glyph_outline(v, 1, 4096, 0, 0, arena.data(), kMaxGlyphCommands, &cnt) == Status::TooComplex);
}

TEST_CASE("decision: simple glyph rules and their order of errors", "[ttf][glyph][decision]") {
  struct Case {
    Bytes g;
    Status want;
    int n;
  };
  Bytes hdr3 = hdr(3);
  p16(hdr3, 1);
  p16(hdr3, 2);
  Bytes bad_ilen = hdr(1);
  p16(bad_ilen, 2);
  p16(bad_ilen, 100);
  bad_ilen.push_back(0x39);
  bad_ilen.push_back(2);
  Bytes no_ilen = hdr(1);
  p16(no_ilen, 2);
  no_ilen.push_back(0);
  Bytes nonincr = hdr(2);
  p16(nonincr, 5);
  p16(nonincr, 5);
  p16(nonincr, 0);
  Bytes nonincr_big = hdr(2);
  p16(nonincr_big, 400);
  p16(nonincr_big, 300);
  Bytes one_pt = hdr(1);
  p16(one_pt, 0);
  p16(one_pt, 0);
  one_pt.push_back(0x31);
  Bytes dec_end = hdr(3);
  p16(dec_end, 1);
  p16(dec_end, 2);
  p16(dec_end, 1);
  p16(dec_end, 0);
  const std::vector<Case> cases = {
      {z3({0x39, 2}), Status::Ok, 4},
      {z3({0x39, 3}), Status::BadGlyph, 0},        // a run past the point count
      {z3({0x39}), Status::BadGlyph, 0},           // repeat count missing
      {z3({0x39, 1}), Status::BadGlyph, 0},        // 2 of 3 flags
      {z3({}), Status::BadGlyph, 0},               // no flags
      {z3({0x3B, 2, 1, 2}), Status::BadGlyph, 0},  // x bytes: 2 of 3
      {z3({0x3B, 2, 1, 2, 3}), Status::Ok, 4},
      {z3({0x1D, 2, 1, 2}), Status::BadGlyph, 0},  // y bytes: 2 of 3
      {z3({0x1D, 2, 1, 2, 3}), Status::Ok, 4},
      {bad_ilen, Status::BadGlyph, 0},
      {no_ilen, Status::BadGlyph, 0},
      {hdr3, Status::BadGlyph, 0},          // end points: 2 of 3
      {nonincr, Status::BadGlyph, 0},
      {nonincr_big, Status::BadGlyph, 0},   // BadGlyph (structure) before TooComplex
      {simple_cut(340), Status::TooComplex, 0},  // TooComplex before the missing flags
      {simple_cut(339), Status::BadGlyph, 0},    // 340 points are allowed, the data is missing
      {one_pt, Status::Ok, 0},
      {z3({0x39, 2, 9, 9, 9}), Status::Ok, 4},  // trailing bytes are ignored
      {z3({0xF9, 2}), Status::Ok, 4},           // flag bits 6 and 7 are ignored
      {z3({0x31, 0x31, 0x31}), Status::Ok, 4},
      {z3({0x39, 0, 0x31, 0x31}), Status::Ok, 4},  // a repeat count of 0 is one flag
      {z3({0x09, 2, 0, 10, 0, 10, 0, 10, 0, 20, 0, 20, 0, 20}), Status::Ok, 4},
      {z3({0x09, 2, 0, 10, 0, 10, 0, 10, 0, 20, 0, 20, 0}), Status::BadGlyph, 0},
      {dec_end, Status::BadGlyph, 0},
      {Bytes{0, 1, 0, 0}, Status::BadGlyph, 0},                     // under 10 bytes
      {Bytes{0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, Status::BadGlyph, 0},   // 10 bytes, one contour, no end points
      {hdr(0), Status::Ok, 0},                                      // 0 contours: Ok without reading further
      {Bytes{0, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}, Status::Ok, 0},
  };
  std::vector<GDesc> gl = {E()};
  for (const Case& c : cases) gl.push_back(R(c.g));
  const Fnt f = load_spec(small_spec(gl));
  REQUIRE(f.st == Status::Ok);
  for (std::size_t i = 0; i < cases.size(); ++i) {
    INFO("case " << i);
    u32 n = 0;
    const Status s = outline_status(f, uc(static_cast<int>(i)) + 1, &n);
    REQUIRE(s == cases[i].want);
    if (s == Status::Ok) {
      REQUIRE(n == uc(cases[i].n));
    }
  }
  // the word delta glyph (case 21): points (10,20) (20,40) (30,60) read as signed 16-bit deltas
  const Run w = run(f.f, 22, 1000, 0, 0);
  REQUIRE(w.st == Status::Ok);
  REQUIRE(w.cmds[0].a.x == 10);
  REQUIRE(w.cmds[0].a.y == -20);
  REQUIRE(w.cmds[2].a.x == 30);
  REQUIRE(w.cmds[2].a.y == -60);
  // negative word deltas
  const Fnt g = load_spec(small_spec({E(), R(z3({0x09, 2, 0xFF, 0xF6, 0, 10, 0xFF, 0xF6, 0xFF, 0xEC, 0xFF, 0xEC, 0, 0}))}));
  const Run x = run(g.f, 1, 1000, 0, 0);
  REQUIRE(x.st == Status::Ok);
  REQUIRE(x.cmds[0].a.x == -10);
  REQUIRE(x.cmds[0].a.y == 20);
  REQUIRE(x.cmds[2].a.x == -10);
}

TEST_CASE("decision: signed 32-bit sums, no 16-bit wraparound, signed offsets", "[ttf][glyph][decision]") {
  // base glyph 12 has x = 30000, 60000 (two 30000 deltas); glyph 13 has offsets of +32767 and -32768
  const Fnt f = load_spec(specs()[0]);
  const Run a = run(f.f, 12, 1000, 0, 0);
  REQUIRE(a.st == Status::Ok);
  REQUIRE(a.cmds[1].a.x == 30000);
  REQUIRE(a.cmds[2].a.x == 60000);
  REQUIRE(a.cmds[2].a.y == -20000);
  const Run b = run(f.f, 13, 1000, 0, 0);
  REQUIRE(b.st == Status::Ok);
  REQUIRE(b.n == 8);
  REQUIRE(b.cmds[0].a.x == 32767);
  REQUIRE(b.cmds[0].a.y == 32768);
  REQUIRE(b.cmds[1].a.x == 32767 + 600);
  REQUIRE(b.cmds[4].a.x == 32767);
  REQUIRE(b.cmds[4].a.y == 32768);
  // negative word and byte offsets
  const Fnt g = load_spec(small_spec({E(), S({{{0, 0, 1}, {10, 0, 1}, {0, 10, 1}}}, 0), C({{1, -5, -7, 0}}, -1), C({{1, -200, 300, 0}}, -1)}));
  const Run c = run(g.f, 2, 1000, 0, 0);
  REQUIRE(c.cmds[0].a.x == -5);
  REQUIRE(c.cmds[0].a.y == 7);
  const Run d = run(g.f, 3, 1000, 0, 0);
  REQUIRE(d.cmds[0].a.x == -200);
  REQUIRE(d.cmds[0].a.y == -300);
  REQUIRE(d.cmds[1].a.x == -190);
}

TEST_CASE("decision: composite order of errors and limits", "[ttf][composite][decision]") {
  std::vector<RC> c16(16, RC{2 | 0x20, 1, 0, 0});
  std::vector<RC> ok16 = c16;
  ok16[15].flags = 2;
  std::vector<RC> more16 = c16;
  std::vector<RC> c17 = c16;
  c17.push_back(RC{2, 1, 0, 0});
  std::vector<RC> bad16 = c16;
  bad16[15].g = 99;
  std::vector<RC> unsup16 = c16;
  unsup16[15].flags |= 8;
  Bytes trunc_args = hdr(-1);
  p16(trunc_args, 1 | 8);
  p16(trunc_args, 1);
  trunc_args.push_back(0);
  trunc_args.push_back(0);
  Bytes trunc_hdr = hdr(-1);
  trunc_hdr.push_back(0);
  trunc_hdr.push_back(2);
  trunc_hdr.push_back(0);
  struct Case {
    Bytes g;
    Status want;
    int n;
  };
  const std::vector<Case> cases = {
      /*2*/ {comp_raw({{2, 1, 0, 0}}), Status::Ok, 4},
      /*3*/ {comp_raw({{0, 1, 0, 0}}), Status::Unsupported, 0},      // no 0x0002
      /*4*/ {comp_raw({{2 | 8, 1, 0, 0}}), Status::Unsupported, 0},  // scale flags
      /*5*/ {comp_raw({{2 | 0x40, 1, 0, 0}}), Status::Unsupported, 0},
      /*6*/ {comp_raw({{2 | 0x80, 1, 0, 0}}), Status::Unsupported, 0},
      /*7*/ {comp_raw({{2 | 8, 99, 0, 0}}), Status::Unsupported, 0},  // flags before the glyph lookup
      /*8*/ {comp_raw({{0, 99, 0, 0}}), Status::Unsupported, 0},
      /*9*/ {comp_raw({{2, 99, 0, 0}}), Status::BadGlyph, 0},
      /*10*/ {comp_raw({{2, 2, 0, 0}}), Status::Unsupported, 0},  // a composite component
      /*11*/ {trunc_args, Status::BadGlyph, 0},                   // the arguments are read before the flag checks
      /*12*/ {trunc_hdr, Status::BadGlyph, 0},
      /*13*/ {hdr(-1), Status::BadGlyph, 0},
      /*14*/ {comp_raw({{2 | 4 | 0x10 | 0x200, 1, 3, 4}}), Status::Ok, 4},  // other flag bits ignored
      /*15*/ {comp_raw({{2 | 0x20 | 0x100, 1, 5, 5}, {2, 1, 0, 0}}), Status::Ok, 8},  // 0x0100 ignored on earlier components
      /*16*/ {comp_raw({{2 | 0x100, 1, 0, 0}}, {0, 3, 9, 9, 9}), Status::Ok, 4},
      /*17*/ {comp_raw({{2 | 0x100, 1, 0, 0}}), Status::BadGlyph, 0},
      /*18*/ {comp_raw({{2 | 0x100, 1, 0, 0}}, {0, 4, 9, 9, 9}), Status::BadGlyph, 0},
      /*19*/ {comp_raw({{2 | 0x100, 1, 0, 0}}, {0, 2, 9, 9, 9, 9, 9}), Status::Ok, 4},
      /*20*/ {comp_raw({{2 | 0x100, 1, 0, 0}}, {0}), Status::BadGlyph, 0},
      /*21*/ {comp_raw({{2, 1, 0, 0}}, {9, 9, 9}), Status::Ok, 4},  // no 0x0100: trailing bytes ignored
      /*22*/ {comp_raw(ok16), Status::Ok, 64},
      /*23*/ {comp_raw(more16), Status::TooComplex, 0},  // the 16th component asks for a 17th
      /*24*/ {comp_raw(c17), Status::TooComplex, 0},
      /*25*/ {comp_raw(bad16), Status::BadGlyph, 0},  // the 16th component is processed first
      /*26*/ {comp_raw(unsup16), Status::Unsupported, 0},
      /*27*/ {comp_raw({{2, 0, 0, 0}}), Status::Ok, 0},  // an empty component glyph adds nothing
      /*28*/ {hdr(0), Status::Ok, 0},
      /*29*/ {comp_raw({{2, 28, 1, 1}}), Status::Ok, 0},  // a component with 0 contours
      /*30*/ {comp_raw({{2, 31, 0, 0}}), Status::BadGlyph, 0},  // a component with bad data
      /*31*/ {hdr(1), Status::BadGlyph, 0},
      /*32*/ {Bytes{0, 1, 0, 0}, Status::BadGlyph, 0},
      /*33*/ {comp_raw({{2, 32, 0, 0}}), Status::BadGlyph, 0},  // a component under 10 bytes
  };
  std::vector<GDesc> gl = {E(), S({{{0, 0, 1}, {600, 0, 1}, {300, 700, 1}}}, 0)};
  for (const Case& c : cases) gl.push_back(R(c.g));
  const Fnt f = load_spec(small_spec(gl));
  REQUIRE(f.st == Status::Ok);
  for (std::size_t i = 0; i < cases.size(); ++i) {
    INFO("glyph " << i + 2);
    u32 n = 0;
    const Status s = outline_status(f, uc(static_cast<int>(i)) + 2, &n);
    REQUIRE(s == cases[i].want);
    if (s == Status::Ok) {
      REQUIRE(n == uc(cases[i].n));
    }
  }
  // the 16 components land in order: contour k starts at the offset of component k
  std::vector<RC> offs;
  for (int i = 0; i < 16; ++i) offs.push_back(RC{2 | (i < 15 ? 0x20 : 0), 1, i * 2, -i});
  const Fnt h = load_spec(small_spec({E(), S({{{0, 0, 1}, {6, 0, 1}, {3, 7, 1}}}, 0), R(comp_raw(offs))}));
  const Run r = run(h.f, 2, 1000, 0, 0);
  REQUIRE(r.st == Status::Ok);
  REQUIRE(r.n == 64);
  for (int i = 0; i < 16; ++i) {
    const std::size_t k = static_cast<std::size_t>(4 * i);
    REQUIRE(r.cmds[k].op == Op::Move);
    REQUIRE(r.cmds[k].a.x == i * 2);
    REQUIRE(r.cmds[k].a.y == i);
    REQUIRE(r.cmds[k + 3].op == Op::Close);
  }
}

TEST_CASE("decision: the composite point total is checked when a component's points are counted", "[ttf][composite][decision]") {
  const Fnt f = load_spec(small_spec({E(),
                                      line_glyph(3),                                       // 1
                                      line_glyph(170),                                     // 2
                                      line_glyph(171),                                     // 3
                                      R(simple_cut(199)),                                  // 4: 200 points, no flags: BadGlyph alone
                                      R(simple_cut(340)),                                  // 5: 341 points: TooComplex alone
                                      C({{2, 0, 0, 0}, {2, 500, 0, 0}}, -1),               // 6: 340 points
                                      C({{3, 0, 0, 0}, {2, 0, 0, 0}}, -1),                 // 7: 341
                                      R(comp_raw({{2, 4, 0, 0}})),                         // 8
                                      R(comp_raw({{2 | 0x20, 2, 0, 0}, {2, 4, 0, 0}})),    // 9: 170 + 200 > 340 before the 2nd's flags
                                      R(comp_raw({{2, 5, 0, 0}})),                         // 10
                                      R(comp_raw({{2 | 0x20, 2, 0, 0}, {2, 3, 0, 0}})),    // 11: 341
                                      R(comp_raw({{2 | 0x20, 4, 0, 0}, {2, 4, 0, 0}})),    // 12: 200 then 400: TooComplex? no: BadGlyph first
                                      R(comp_raw({{2 | 0x20, 3, 0, 0}, {2, 3, 0, 0}})),    // 13: 342
                                      line_glyph(340)}));                                  // 14
  const struct {
    u32 g;
    Status want;
    u32 n;
  } t[] = {{2, Status::Ok, 171},          {3, Status::Ok, 172},          {4, Status::BadGlyph, 0},      {5, Status::TooComplex, 0},
           {6, Status::Ok, 342},          {7, Status::TooComplex, 0},    {8, Status::BadGlyph, 0},      {9, Status::TooComplex, 0},
           {10, Status::TooComplex, 0},   {11, Status::TooComplex, 0},   {12, Status::BadGlyph, 0},     {13, Status::TooComplex, 0},
           {14, Status::Ok, 341}};
  for (const auto& c : t) {
    INFO("glyph " << c.g);
    u32 n = 0;
    REQUIRE(outline_status(f, c.g, &n) == c.want);
    if (c.want == Status::Ok) REQUIRE(n == c.n);
  }
}

TEST_CASE("decision: OutOfRange only after the structure of the whole glyph is read", "[ttf][outline][decision]") {
  FontSpec s;
  s.upem = 16384;
  s.adv = {100};
  s.nhm = 1;
  s.long_loca = true;
  s.nopad = true;
  s.cmap = {CRec{3, 1, kCmapA}};
  s.glyphs = {E(),
              S({{{0, 0, 1}, {32767, 0, 1}, {0, 100, 1}}}, 0),                         // 1
              C({{1, 1, 0, 0}}, -1),                                                   // 2: x reaches 32768
              C({{1, 2, 0, 0}}, -1),                                                   // 3: 32769
              S({{{-32768, 0, 1}, {-1, 0, 1}, {-1, 10, 1}}}, 0),                       // 4
              C({{4, 0, 0, 0}}, -1),                                                   // 5
              C({{4, -1, 0, 0}}, -1),                                                  // 6
              S({{{0, 32767, 1}, {5, 0, 1}, {9, 3, 1}}}, 0),                           // 7
              C({{7, 0, 1, 0}}, -1),                                                   // 8
              C({{7, 0, 2, 0}}, -1),                                                   // 9
              R(comp_raw({{2 | 1 | 0x20, 1, 32767, 32767}, {2 | 8, 1, 0, 0}})),        // 10: out of range, then Unsupported
              R(comp_raw({{2 | 1 | 0x20, 1, 32767, 32767}, {2, 99, 0, 0}})),           // 11: ... then BadGlyph
              R(comp_raw({{2 | 1 | 0x20, 1, 32767, 32767}, {2, 1, 0, 0}}))};           // 12: out of range
  const Fnt f = load_spec(s);
  REQUIRE(f.st == Status::Ok);
  const i32 K = 1 << 22;
  auto st = [&](u32 g, i32 ox, i32 oy) { return run(f.f, g, K, ox, oy).st; };
  // 32768 font units * 2^22 / 16384 = 8388608; plus the origin 2^23 is exactly 2^24, which is still in range
  REQUIRE(st(1, 1 << 23, 0) == Status::Ok);
  REQUIRE(st(2, 1 << 23, 0) == Status::Ok);
  REQUIRE(st(3, 1 << 23, 0) == Status::OutOfRange);
  REQUIRE(st(3, 0, 0) == Status::Ok);
  REQUIRE(st(5, -(1 << 23), 0) == Status::Ok);
  REQUIRE(st(6, -(1 << 23), 0) == Status::OutOfRange);
  // y goes the other way: y = oy - fx(dy)
  REQUIRE(st(8, 0, -(1 << 23)) == Status::Ok);
  REQUIRE(st(9, 0, -(1 << 23)) == Status::OutOfRange);
  REQUIRE(st(7, 0, 1 << 23) == Status::Ok);
  // glyphs 10 and 12 carry an out-of-range first component (offset 32767 plus the origin 2^23)
  REQUIRE(st(12, 0, 0) == Status::Ok);
  REQUIRE(st(12, 1 << 23, 0) == Status::OutOfRange);
  REQUIRE(st(10, 1 << 23, 0) == Status::Unsupported);
  REQUIRE(st(11, 1 << 23, 0) == Status::BadGlyph);
  const Run r = run(f.f, 2, K, 1 << 23, 0);
  REQUIRE(r.st == Status::Ok);
  REQUIRE(r.cmds[1].a.x == (1 << 24));  // (32767 + 1) * 256 + 2^23: the boundary value itself
  REQUIRE(r.cmds[0].a.x == (1 << 23) + 256);
}

TEST_CASE("decision: loca entries g and g+1 only, ordered and inside glyf", "[ttf][loca][decision]") {
  const Bytes b = base_bytes();
  const std::size_t L = tab_off(b, "loca");
  const std::size_t gr = rec_pos(b, "glyf");
  const std::size_t glyf_len = (static_cast<std::size_t>(get16(b, gr + 12)) << 16) | get16(b, gr + 14);
  auto at = [&](const Bytes& c, std::size_t i) { return get16(c, L + 2 * i); };
  auto status_of = [&](const Bytes& c, u32 g, u32* n = nullptr) {
    const Fnt f = load(c);
    REQUIRE(f.st == Status::Ok);
    return outline_status(f, g, n);
  };
  REQUIRE(at(b, 2) > at(b, 1));
  REQUIRE(status_of(b, 2) == Status::Ok);
  {  // loca[g+1] < loca[g]
    Bytes c = b;
    set16(c, L + 2 * 3, static_cast<int>(at(b, 2)) - 1);
    REQUIRE(status_of(c, 2) == Status::BadGlyph);
    REQUIRE(status_of(c, 1) == Status::Ok);  // glyphs before are unaffected
  }
  {  // beyond glyf
    Bytes c = b;
    set16(c, L + 2 * 3, 0xFFFF);
    REQUIRE(status_of(c, 2) == Status::BadGlyph);
    REQUIRE(status_of(c, 3) == Status::BadGlyph);
    REQUIRE(status_of(c, 1) == Status::Ok);
    REQUIRE(status_of(c, 4) == Status::Ok);  // loca[4] and loca[5] are untouched
  }
  {  // exactly glyf_len is inside; 2 bytes more is not, also for an empty range
    Bytes c = b;
    set16(c, L + 2 * 4, static_cast<int>(glyf_len / 2));
    set16(c, L + 2 * 3, static_cast<int>(glyf_len / 2));
    u32 n = 5;
    REQUIRE(status_of(c, 3, &n) == Status::Ok);
    REQUIRE(n == 0);
    set16(c, L + 2 * 4, static_cast<int>(glyf_len / 2) + 1);
    set16(c, L + 2 * 3, static_cast<int>(glyf_len / 2) + 1);
    REQUIRE(status_of(c, 3) == Status::BadGlyph);
  }
  {  // the last glyph uses loca[numGlyphs]
    Bytes c = b;
    set16(c, L + 2 * 14, 0xFFFF);
    REQUIRE(status_of(c, 13) == Status::BadGlyph);
    REQUIRE(status_of(c, 12) == Status::Ok);
    set16(c, L + 2 * 13, 0xFFFF);
    REQUIRE(status_of(c, 13) == Status::BadGlyph);
  }
  {  // loca[g] beyond glyf with a larger loca[g+1]
    Bytes c = b;
    set16(c, L + 2 * 12, 0xFFFE);
    set16(c, L + 2 * 13, 0xFFFF);
    REQUIRE(status_of(c, 12) == Status::BadGlyph);
  }
  {  // a non-empty range under 10 bytes
    Bytes c = b;
    set16(c, L + 2 * 3, static_cast<int>(at(b, 2)) + 4);
    REQUIRE(status_of(c, 2) == Status::BadGlyph);
  }
  {  // the long format: 32-bit entries are read as they are, not doubled
    const Bytes lb = build_font(specs()[1]);
    const std::size_t ll = tab_off(lb, "loca");
    auto at4 = [&](const Bytes& c, std::size_t i) { return (get16(c, ll + 4 * i) << 16) | get16(c, ll + 4 * i + 2); };
    Bytes c = lb;
    set32(c, ll + 4 * 3, at4(lb, 2) - 1);
    REQUIRE(status_of(c, 2) == Status::BadGlyph);
    REQUIRE(status_of(c, 1) == Status::Ok);
    c = lb;
    set32(c, ll + 4 * 3, 0xFFFFFFFFu);
    REQUIRE(status_of(c, 2) == Status::BadGlyph);
    REQUIRE(status_of(c, 4) == Status::Ok);
    c = lb;
    set32(c, ll + 4 * 3, at4(lb, 2) + 8);
    REQUIRE(status_of(c, 2) == Status::BadGlyph);
    c = lb;
    set32(c, ll + 4 * 4, at4(lb, 3));  // glyph 3 becomes empty
    u32 n = 9;
    REQUIRE(status_of(c, 3, &n) == Status::Ok);
    REQUIRE(n == 0);
  }
  {  // exactly 10 bytes (a glyph with 0 contours) is fine, 8 is not
    FontSpec s = small_spec({E(), R(hdr(0)), E()});
    s.long_loca = false;
    const Fnt z = load_spec(s);
    REQUIRE(z.st == Status::Ok);
    u32 n = 9;
    REQUIRE(outline_status(z, 1, &n) == Status::Ok);
    REQUIRE(n == 0);
    FontSpec s8 = small_spec({E(), R(Bytes{0, 0, 0, 0, 0, 0, 0, 0}), E()});
    s8.long_loca = false;
    const Fnt y = load_spec(s8);
    REQUIRE(outline_status(y, 1) == Status::BadGlyph);
  }
}

TEST_CASE("glyph_outline writes nothing on error, on every glyph of every font", "[ttf][outline]") {
  const std::vector<FontSpec> sp = specs();
  for (std::size_t vi = 0; vi < 3; ++vi) {
    const Fnt f = load_spec(sp[vi]);
    for (u32 g = 0; g < 16; ++g) {
      for (i32 size : {4096, 1 << 22, 1}) {
        const Run r = run(f.f, g, size, 0, 0);  // run() checks the sentinels itself
        if (g >= 14) REQUIRE(r.st == Status::BadGlyph);
      }
    }
  }
}

TEST_CASE("sweep: every prefix of the base font", "[ttf][sweep]") {
  const Bytes f = base_bytes();
  FnvW w;
  for (std::size_t n = 0; n <= f.size(); ++n) {
    std::unique_ptr<u8[]> p(new u8[n ? n : 1]);  // exact allocation: a read past n bytes is an AddressSanitizer error
    if (n) std::memcpy(p.get(), f.data(), n);
    rec_words(p.get(), n, w);
    w.word(0xDEAD);
  }
  REQUIRE(w.h == kTruncDigest);
}

TEST_CASE("sweep: four byte values at every byte of the base font", "[ttf][sweep]") {
  const Bytes f = base_bytes();
  FnvW w;
  for (std::size_t pos = 0; pos < f.size(); ++pos)
    for (u8 val : {u8{0x00}, u8{0xFF}, u8{0x80}, u8{0x01}}) {
      Bytes g = f;
      g[pos] = val;
      rec_words(g.data(), g.size(), w);
      w.word(0xBEEF);
    }
  REQUIRE(w.h == kFlipDigest);
}

TEST_CASE("sweep: sixteen byte values at every glyf byte of the base font", "[ttf][sweep]") {
  const Bytes f = base_bytes();
  const std::size_t go = tab_off(f, "glyf");
  const std::size_t gr = rec_pos(f, "glyf");
  const std::size_t gl = (static_cast<std::size_t>(get16(f, gr + 12)) << 16) | get16(f, gr + 14);
  FnvW w;
  for (std::size_t pos = go; pos < go + gl; ++pos)
    for (u8 val : {u8{0}, u8{1}, u8{2}, u8{3}, u8{4}, u8{8}, u8{16}, u8{32}, u8{64}, u8{127}, u8{128}, u8{129}, u8{255}, u8{15}, u8{240}, u8{85}}) {
      Bytes g = f;
      g[pos] = val;
      rec_words(g.data(), g.size(), w);
      w.word(0xF00D);
    }
  REQUIRE(w.h == kGlyfSweepDigest);
}

TEST_CASE("exhaustive: every small simple glyph of up to 6 points", "[ttf][sweep]") {
  std::vector<GDesc> gl = {E()};
  for (int n = 1; n <= 6; ++n)
    for (int mask = 0; mask < (1 << n); ++mask) {
      std::vector<Pt> c;
      for (int k = 0; k < n; ++k) c.push_back(Pt{(k * 97 + mask * 13) % 401 - 100, (k * 53 + mask * 29) % 377 - 60, (mask >> k) & 1});
      gl.push_back(S({c}, 0));
    }
  REQUIRE(gl.size() == kExhaustiveGlyphs);
  FontSpec s;
  s.glyphs = gl;
  s.adv = {100};
  s.nhm = 1;
  s.cmap = {CRec{3, 1, build_cmap4({sg(0, 0xFFFE, 0), sg(0xFFFF, 0xFFFF, 1)})}};
  const Bytes fb = build_font(s);
  REQUIRE(fnv_bytes(fb) == kExhaustiveFontDigest);
  const Fnt f = load(fb);
  REQUIRE(f.st == Status::Ok);
  FnvW w;
  for (u32 g = 0; g < gl.size(); ++g) {
    const i32 sz[2] = {256 * 10, 7};
    const i32 ox[2] = {0, 100};
    const i32 oy[2] = {0, -100};
    for (int k = 0; k < 2; ++k) {
      const Run r = run(f.f, g, sz[k], ox[k], oy[k]);
      w.word(static_cast<u32>(r.st));
      const u32 cnt = r.st == Status::Ok ? r.n : 0u;
      w.word(cnt);
      if (cnt > 0) {
        const u64 d = digest_ref(r.cmds);
        w.word(static_cast<u32>(d & 0xFFFFFFFFu));
        w.word(static_cast<u32>(d >> 32));
      } else {
        w.word(0);
        w.word(0);
      }
    }
  }
  REQUIRE(w.h == kExhaustiveDigest);
}

TEST_CASE("sweep: every glyph of the long loca font cut at every length", "[ttf][sweep]") {
  const Bytes lb = build_font(specs()[1]);
  const std::size_t ll = tab_off(lb, "loca");
  auto at4 = [&](std::size_t i) { return (get16(lb, ll + 4 * i) << 16) | get16(lb, ll + 4 * i + 2); };
  FnvW w;
  for (u32 g = 1; g < 14; ++g) {
    const u32 a = at4(g), b = at4(g + 1);
    for (u32 k = 0; k <= b - a; ++k) {
      Bytes c = lb;
      set32(c, ll + 4 * (g + 1), a + k);
      const Fnt f = load(c);
      REQUIRE(f.st == Status::Ok);
      std::vector<Command>& buf = sweep_buf();
      u32 n = 0;
      const Status s = glyph_outline(f.f, g, 4096, 0, 0, buf.data(), kMaxGlyphCommands, &n);
      w.word(static_cast<u32>(s));
      const u32 cnt = s == Status::Ok ? n : 0u;
      w.word(cnt);
      if (cnt > 0) {
        const u64 d = digest_commands(buf.data(), cnt);
        w.word(static_cast<u32>(d & 0xFFFFFFFFu));
        w.word(static_cast<u32>(d >> 32));
      } else {
        w.word(0);
        w.word(0);
      }
    }
  }
  REQUIRE(w.h == kCutDigest);
}

TEST_CASE("decision: exact table and glyph size edges", "[ttf][loca][decision]") {
  {  // a glyph of exactly 9 bytes is under the header size, on its own and as a component
    const Fnt f = load_spec(small_spec({E(), R(Bytes(9, 0)), R(comp_raw({{2, 1, 0, 0}}))}));
    REQUIRE(outline_status(f, 1) == Status::BadGlyph);
    REQUIRE(outline_status(f, 2) == Status::BadGlyph);
  }
  {  // a component index equal to numGlyphs is BadGlyph even when loca has more entries than needed
    const Fnt f = load_spec(small_spec({E(), R(comp_raw({{2, 2, 0, 0}}))}));
    Bytes c = f.b;
    const std::size_t l = tab_off(c, "loca");
    set32(c, l + 12, (get16(c, l + 8) << 16) | get16(c, l + 10));  // loca[3] sits after the table: glyph 2 would be an empty range
    Font v = f.f;
    v.data = c.data();
    v.loca_len = 16;
    REQUIRE(run(v, 1, 4096, 0, 0).st == Status::BadGlyph);
    v.num_glyphs = 3;  // with three glyphs the same bytes make glyph 2 an empty glyph
    REQUIRE(run(v, 1, 4096, 0, 0).st == Status::Ok);
  }
  {  // loca entries g and g + 1 must both exist in the table
    const Fnt s = load_spec(specs()[0]);
    Font v = s.f;
    v.loca_len = 29;
    Run r = run(v, 13, 4096, 0, 0);
    REQUIRE(r.st == Status::BadGlyph);
    REQUIRE(run(v, 12, 4096, 0, 0).st == Status::Ok);
    const Fnt l = load_spec(specs()[1]);
    Font w = l.f;
    w.loca_len = 59;
    REQUIRE(run(w, 13, 4096, 0, 0).st == Status::BadGlyph);
    REQUIRE(run(w, 12, 4096, 0, 0).st == Status::Ok);
  }
  {  // an empty range one byte past glyf is BadGlyph (long format)
    const Bytes lb = build_font(specs()[1]);
    const std::size_t ll = tab_off(lb, "loca");
    const std::size_t gr = rec_pos(lb, "glyf");
    const u32 glyf_len = (get16(lb, gr + 12) << 16) | get16(lb, gr + 14);
    Bytes c = lb;
    set32(c, ll + 4 * 3, glyf_len);
    set32(c, ll + 4 * 4, glyf_len);
    REQUIRE(load(c).st == Status::Ok);
    REQUIRE(outline_status(load(c), 3) == Status::Ok);
    set32(c, ll + 4 * 3, glyf_len + 1);
    set32(c, ll + 4 * 4, glyf_len + 1);
    REQUIRE(outline_status(load(c), 3) == Status::BadGlyph);
  }
  {  // the largest negative y delta: y = -32768 with origin +2^23 reaches +2^24 exactly
    FontSpec s;
    s.upem = 16384;
    s.adv = {100};
    s.nhm = 1;
    s.cmap = {CRec{3, 1, kCmapA}};
    s.long_loca = true;
    s.nopad = true;
    s.glyphs = {E(), S({{{0, -32768, 1}, {1, -1, 1}, {2, 9, 1}}}, 0)};
    const Fnt f = load_spec(s);
    REQUIRE(run(f.f, 1, 1 << 22, 0, 1 << 23).st == Status::Ok);
    const Run r = run(f.f, 1, 1 << 22, 0, 1 << 23);
    REQUIRE(r.cmds[0].a.y == (1 << 24));
    REQUIRE(run(f.f, 1, 1 << 22, 0, (1 << 23) + 0).cmds[0].a.x == 0);
  }
}

TEST_CASE("decision: a glyph never reads the bytes of the glyph after it", "[ttf][glyph][decision]") {
  // each damaged glyph is followed by a glyph whose first bytes would complete it if the reader ran on
  const Bytes next = {0x02, 0x31, 0x31, 0xBB, 0, 0, 0, 0, 0, 0, 0, 0};
  struct Case {
    Bytes g;
  };
  const std::vector<Bytes> bad = {
      z3({0x39}, 0, 2),          // the repeat count is the next glyph's first byte
      z3({}, 0, 0),              // the only flag is the next glyph's first byte (0x02: x short, y word)
      z3({0x21, 0xAA}, 0, 0),    // x is a word, one byte is missing
      z3({0x25, 0xAA}, 0, 0),    // y is short (needs a byte) after a missing x byte
  };
  for (const Bytes& g : bad) {
    for (int variant = 0; variant < 2; ++variant) {
      Bytes follower = next;
      if (variant == 1) follower = Bytes{0x31, 0x31, 0x31, 0xBB, 0, 0, 0, 0, 0, 0, 0, 0};
      const Fnt f = load_spec(small_spec({E(), R(g), R(follower)}));
      INFO("variant " << variant << " size " << g.size());
      REQUIRE(outline_status(f, 1) == Status::BadGlyph);
    }
  }
}
