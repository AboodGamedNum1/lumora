// Tests for the PNG codec (core/image/png.h). Fixtures come from png_fixtures.h: a test-only PNG writer with stored
// DEFLATE and its own reference model. Nothing here uses third-party PNG files.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "deflate.h"
#include "png.h"
#include "png_fixtures.h"

using namespace lumora::image;
using namespace png_fixtures;

namespace {

PngResult decode(const Bytes& b, const PngLimits& limits = {}) {
  static const std::uint8_t kEmpty = 0;
  return decode_png(b.empty() ? &kEmpty : b.data(), b.size(), limits);
}

Spec make_spec(std::uint32_t w, std::uint32_t h, int ctype, int depth, bool interlaced, int filter = 5,
               std::uint32_t seed = 1) {
  Spec s;
  s.w = w;
  s.h = h;
  s.ctype = ctype;
  s.depth = depth;
  s.interlaced = interlaced;
  s.filter = filter;
  if (ctype == 3) {
    const std::size_t entries = depth == 8 ? 200 : (std::size_t{1} << depth);
    s.plte = make_palette(entries, seed);
    s.samples = make_samples(s, seed, entries);
  } else {
    s.samples = make_samples(s, seed);
  }
  return s;
}

void expect_decodes(const Spec& s, const std::string& what) {
  INFO(what);
  const PngResult r = decode(build(s));
  REQUIRE(r.status == PngStatus::Ok);
  REQUIRE(r.image.valid());
  CHECK(r.image.width == s.w);
  CHECK(r.image.height == s.h);
  CHECK(r.image.rgba == expected_rgba(s));
}

// A small valid file used as the base of the malformed variants: 3x2 RGBA, 8 bit, non-interlaced.
Spec base_spec() { return make_spec(3, 2, 6, 8, false, 0, 7); }

// Chunk list builders for hand-made files.
Bytes file_of(const std::vector<Bytes>& chunks) {
  Bytes out = signature();
  for (const auto& c : chunks) append(out, c);
  return out;
}
Bytes good_ihdr(const Spec& s) { return chunk("IHDR", ihdr_body(s.w, s.h, s.depth, s.ctype, 0, 0, s.interlaced ? 1 : 0)); }
Bytes good_idat(const Spec& s) { return chunk("IDAT", zlib_stored(raw_data(s))); }

void flip_crc(Bytes& png, std::size_t chunk_index) {
  const auto spans = chunk_spans(png);
  REQUIRE(chunk_index < spans.size());
  png[spans[chunk_index].first + spans[chunk_index].second - 1] ^= 0x01;
}

PngStatus status_of(const Bytes& b, const PngLimits& l = {}) { return decode(b, l).status; }

}  // namespace

TEST_CASE("every colour type and depth, every filter mode, both interlace methods decode pixel-exact", "[image][png]") {
  struct Combo {
    int ctype, depth;
  };
  const Combo combos[] = {{0, 1}, {0, 2}, {0, 4}, {0, 8}, {0, 16}, {2, 8}, {2, 16}, {3, 1}, {3, 2}, {3, 4},
                          {3, 8}, {4, 8}, {4, 16}, {6, 8}, {6, 16}};
  for (const Combo& c : combos) {
    for (int interlaced = 0; interlaced < 2; ++interlaced) {
      for (int filter = 0; filter <= 5; ++filter) {
        const Spec s = make_spec(11, 9, c.ctype, c.depth, interlaced == 1, filter, static_cast<std::uint32_t>(c.ctype * 17 + c.depth));
        expect_decodes(s, "ctype " + std::to_string(c.ctype) + " depth " + std::to_string(c.depth) + " interlaced " +
                              std::to_string(interlaced) + " filter " + std::to_string(filter));
      }
    }
  }
}

TEST_CASE("all five filters at row and column edges (width or height 1, first row, first column)", "[image][png]") {
  const std::uint32_t sizes[][2] = {{1, 1}, {1, 7}, {7, 1}, {2, 2}, {5, 4}};
  const int types[][2] = {{0, 8}, {2, 8}, {6, 8}, {6, 16}, {4, 16}, {3, 8}};
  for (const auto& sz : sizes) {
    for (const auto& t : types) {
      for (int filter = 0; filter <= 4; ++filter) {
        expect_decodes(make_spec(sz[0], sz[1], t[0], t[1], false, filter, 3),
                       "size " + std::to_string(sz[0]) + "x" + std::to_string(sz[1]) + " type " + std::to_string(t[0]) +
                           " depth " + std::to_string(t[1]) + " filter " + std::to_string(filter));
      }
    }
  }
}

TEST_CASE("Adam7 for every size from 1x1 to 9x9, including empty passes", "[image][png]") {
  for (std::uint32_t w = 1; w <= 9; ++w) {
    for (std::uint32_t h = 1; h <= 9; ++h) {
      expect_decodes(make_spec(w, h, 6, 8, true, 5, w * 10 + h), "rgba " + std::to_string(w) + "x" + std::to_string(h));
      expect_decodes(make_spec(w, h, 0, 1, true, 5, w * 10 + h), "gray1 " + std::to_string(w) + "x" + std::to_string(h));
      expect_decodes(make_spec(w, h, 3, 2, true, 2, w * 10 + h), "pal2 " + std::to_string(w) + "x" + std::to_string(h));
    }
  }
}

TEST_CASE("a 1x1 interlaced image has six empty passes and one pixel", "[image][png]") {
  Spec s = make_spec(1, 1, 6, 8, true, 0);
  s.samples = {10, 20, 30, 40};
  const PngResult r = decode(build(s));
  REQUIRE(r.ok());
  CHECK(r.image.rgba == Bytes({10, 20, 30, 40}));
}

TEST_CASE("tRNS for greyscale, RGB and palette images", "[image][png]") {
  SECTION("greyscale colour key, 8 and 16 bit and 2 bit") {
    Spec s = make_spec(4, 1, 0, 8, false, 0);
    s.samples = {0, 50, 51, 50};
    s.trns = {0, 50};
    expect_decodes(s, "gray8");
    CHECK(expected_rgba(s)[3] == 255);
    CHECK(expected_rgba(s)[7] == 0);
    Spec s16 = make_spec(3, 1, 0, 16, false, 0);
    s16.samples = {0x1234, 0x1235, 0x1234};
    s16.trns = {0x12, 0x34};
    expect_decodes(s16, "gray16");
    Spec s2 = make_spec(4, 1, 0, 2, true, 1);
    s2.samples = {0, 1, 2, 3};
    s2.trns = {0, 2};
    expect_decodes(s2, "gray2");
  }
  SECTION("RGB colour key, 8 and 16 bit") {
    Spec s = make_spec(3, 1, 2, 8, false, 4);
    s.samples = {1, 2, 3, 1, 2, 4, 1, 2, 3};
    s.trns = {0, 1, 0, 2, 0, 3};
    expect_decodes(s, "rgb8");
    Spec s16 = make_spec(2, 1, 2, 16, true, 3);
    s16.samples = {0x0102, 0x0304, 0x0506, 0x0102, 0x0304, 0x0507};
    s16.trns = {1, 2, 3, 4, 5, 6};
    expect_decodes(s16, "rgb16");
  }
  SECTION("palette alpha: shorter than the palette, as long as the palette, more than one chunk is an error") {
    Spec s = make_spec(4, 2, 3, 2, false, 0);
    s.samples = {0, 1, 2, 3, 3, 2, 1, 0};
    s.trns = {10, 20};  // entries 2 and 3 stay opaque
    expect_decodes(s, "short tRNS");
    s.trns = {10, 20, 30, 40};
    expect_decodes(s, "full tRNS");
  }
}

TEST_CASE("16-bit samples are reduced by taking the high byte, no rounding", "[image][png]") {
  Spec s = make_spec(1, 1, 6, 16, false, 0);
  s.samples = {0x12FF, 0x8000, 0x00FF, 0xFFFF};
  const PngResult r = decode(build(s));
  REQUIRE(r.ok());
  CHECK(r.image.rgba == Bytes({0x12, 0x80, 0x00, 0xFF}));
}

TEST_CASE("low bit depths scale to the full 8 bit range", "[image][png]") {
  Spec s = make_spec(4, 1, 0, 2, false, 0);
  s.samples = {0, 1, 2, 3};
  const PngResult r = decode(build(s));
  REQUIRE(r.ok());
  CHECK(r.image.rgba[0] == 0);
  CHECK(r.image.rgba[4] == 85);
  CHECK(r.image.rgba[8] == 170);
  CHECK(r.image.rgba[12] == 255);
}

TEST_CASE("palette judgment calls: sizes below capacity, RGB PLTE is ignored, grey PLTE is rejected", "[image][png]") {
  SECTION("fewer entries than the depth allows is fine") {
    Spec s = make_spec(4, 2, 3, 4, false, 0);
    s.plte = make_palette(3, 9);
    s.samples = {0, 1, 2, 2, 1, 0, 0, 1};
    expect_decodes(s, "3 entries at depth 4");
    Spec one = make_spec(2, 2, 3, 8, false, 0);
    one.plte = make_palette(1, 9);
    one.samples = {0, 0, 0, 0};
    expect_decodes(one, "1 entry at depth 8");
  }
  SECTION("a suggested palette in an RGB or RGBA image is validated and ignored") {
    Spec s = make_spec(3, 2, 2, 8, false, 0);
    s.plte = make_palette(5, 2);
    expect_decodes(s, "rgb with PLTE");
    Spec s6 = make_spec(3, 2, 6, 8, false, 0);
    s6.plte = make_palette(256, 2);
    expect_decodes(s6, "rgba with PLTE");
  }
  SECTION("PLTE in a greyscale image is a BadChunk") {
    Spec g = make_spec(2, 2, 0, 8, false, 0);
    g.plte = make_palette(2, 1);
    CHECK(status_of(build(g)) == PngStatus::BadChunk);
    Spec ga = make_spec(2, 2, 4, 8, false, 0);
    ga.plte = make_palette(2, 1);
    CHECK(status_of(build(ga)) == PngStatus::BadChunk);
  }
}

TEST_CASE("BadPalette has at least two triggers each", "[image][png]") {
  SECTION("index beyond the palette") {
    Spec a = make_spec(2, 2, 3, 2, false, 0);
    a.plte = make_palette(2, 1);
    a.samples = {0, 1, 2, 0};
    CHECK(status_of(build(a)) == PngStatus::BadPalette);
    Spec b = make_spec(2, 1, 3, 8, false, 0);
    b.plte = make_palette(4, 1);
    b.samples = {3, 200};
    CHECK(status_of(build(b)) == PngStatus::BadPalette);
    Spec c = make_spec(3, 3, 3, 4, true, 1);
    c.plte = make_palette(15, 1);
    c.samples = {0, 1, 2, 3, 4, 5, 6, 7, 15};
    CHECK(status_of(build(c)) == PngStatus::BadPalette);
  }
  SECTION("missing PLTE for colour type 3") {
    Spec a = make_spec(2, 2, 3, 8, false, 0);
    a.plte.clear();
    CHECK(status_of(build(a)) == PngStatus::BadPalette);
    CHECK(status_of(file_of({good_ihdr(a), good_idat(a), chunk("IEND")})) == PngStatus::BadPalette);
  }
  SECTION("bad PLTE length") {
    Spec a = make_spec(2, 2, 3, 8, false, 0);
    for (std::size_t len : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{771}}) {
      a.plte.assign(len, 0);
      const Bytes f = file_of({good_ihdr(a), chunk("PLTE", a.plte), good_idat(a), chunk("IEND")});
      CHECK(status_of(f) == PngStatus::BadPalette);
    }
    Spec d = make_spec(2, 2, 3, 2, false, 0);
    d.plte = make_palette(5, 1);  // 5 entries at depth 2 (capacity 4)
    CHECK(status_of(build(d)) == PngStatus::BadPalette);
  }
  SECTION("bad tRNS length") {
    Spec g = make_spec(2, 2, 0, 8, false, 0);
    g.trns = {1};
    CHECK(status_of(build(g)) == PngStatus::BadPalette);
    g.trns = {1, 2, 3};
    CHECK(status_of(build(g)) == PngStatus::BadPalette);
    Spec r = make_spec(2, 2, 2, 8, false, 0);
    r.trns = {1, 2, 3, 4};
    CHECK(status_of(build(r)) == PngStatus::BadPalette);
    Spec p = make_spec(2, 2, 3, 2, false, 0);
    p.plte = make_palette(2, 1);
    p.samples = {0, 1, 0, 1};
    p.trns = {1, 2, 3};
    CHECK(status_of(build(p)) == PngStatus::BadPalette);
  }
}

TEST_CASE("tRNS rules: once, before IDAT, not with an alpha channel, after PLTE for palettes", "[image][png]") {
  Spec g = make_spec(2, 2, 0, 8, false, 0);
  const Bytes key = {0, 5};
  CHECK(status_of(file_of({good_ihdr(g), chunk("tRNS", key), chunk("tRNS", key), good_idat(g), chunk("IEND")})) == PngStatus::BadChunk);
  CHECK(status_of(file_of({good_ihdr(g), good_idat(g), chunk("tRNS", key), chunk("IEND")})) == PngStatus::BadChunk);
  Spec ga = make_spec(2, 2, 4, 8, false, 0);
  ga.trns = {0, 5};
  CHECK(status_of(build(ga)) == PngStatus::BadChunk);
  Spec rgba = make_spec(2, 2, 6, 8, false, 0);
  rgba.trns = {0, 0, 0, 0, 0, 0};
  CHECK(status_of(build(rgba)) == PngStatus::BadChunk);
  Spec p = make_spec(2, 2, 3, 2, false, 0);
  p.plte = make_palette(4, 1);
  CHECK(status_of(file_of({good_ihdr(p), chunk("tRNS", Bytes{1, 2}), chunk("PLTE", p.plte), good_idat(p), chunk("IEND")})) == PngStatus::BadChunk);
}

TEST_CASE("NotPng: at least two triggers", "[image][png]") {
  CHECK(status_of({}) == PngStatus::NotPng);
  CHECK(decode_png(nullptr, 0).status == PngStatus::NotPng);
  Bytes wrong = build(base_spec());
  wrong[1] = 'X';
  CHECK(status_of(wrong) == PngStatus::NotPng);
  CHECK(status_of(Bytes(40, 0x41)) == PngStatus::NotPng);
  Bytes jpeg = {0xFF, 0xD8, 0xFF, 0xE0, 0, 0x10, 'J', 'F', 'I', 'F'};
  CHECK(status_of(jpeg) == PngStatus::NotPng);
  Bytes crlf = build(base_spec());
  crlf[4] = '\n';  // the signature's line-ending check bytes
  CHECK(status_of(crlf) == PngStatus::NotPng);
}

TEST_CASE("Truncated: every proper prefix of a valid file is Truncated, never Ok and never a crash", "[image][png][truncation]") {
  std::vector<Bytes> files;
  files.push_back(build(base_spec()));
  Spec pal = make_spec(5, 3, 3, 2, true, 5, 4);
  pal.trns = {9, 8};
  files.push_back(build(pal));
  files.push_back(encode_png([] {
    Rgba8 im;
    im.width = 3;
    im.height = 3;
    im.rgba.assign(36, 77);
    return im;
  }()));
  Bytes extra = file_of({good_ihdr(base_spec()), chunk("gAMA", Bytes{0, 0, 0xB1, 0x8F}), good_idat(base_spec()), chunk("tEXt", Bytes{'a', 0, 'b'}), chunk("IEND")});
  files.push_back(extra);
  for (const Bytes& f : files) {
    REQUIRE(status_of(f) == PngStatus::Ok);
    for (std::size_t n = 1; n < f.size(); ++n) {
      const PngResult r = decode_png(f.data(), n);
      INFO("prefix length " << n << " of " << f.size());
      CHECK(r.status == PngStatus::Truncated);
      CHECK(r.image.rgba.empty());
    }
    CHECK(status_of(Bytes(f.begin(), f.begin() + 0)) == PngStatus::NotPng);
  }
}

TEST_CASE("Truncated: a file without IEND, a chunk length that runs past the end", "[image][png]") {
  const Spec s = base_spec();
  CHECK(status_of(file_of({good_ihdr(s), good_idat(s)})) == PngStatus::Truncated);
  Bytes f = file_of({good_ihdr(s), good_idat(s), chunk("IEND")});
  Bytes big = f;
  const auto spans = chunk_spans(big);
  big[spans[1].first] = 0x7F;  // IDAT claims about 2 GB
  CHECK(status_of(big) == PngStatus::Truncated);
}

TEST_CASE("BadChunk: at least two triggers per rule", "[image][png]") {
  const Spec s = base_spec();
  const Bytes ihdr = good_ihdr(s), idat = good_idat(s), iend = chunk("IEND");
  SECTION("IHDR must be first and unique") {
    CHECK(status_of(file_of({idat, ihdr, iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({chunk("gAMA", Bytes{0, 0, 0, 1}), ihdr, idat, iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({ihdr, ihdr, idat, iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({ihdr, idat, ihdr, iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({chunk("IHDR", Bytes(12, 0)), idat, iend})) == PngStatus::BadChunk);
  }
  SECTION("PLTE rules") {
    Spec p = make_spec(2, 2, 3, 8, false, 0);
    const Bytes pl = chunk("PLTE", p.plte);
    CHECK(status_of(file_of({good_ihdr(p), pl, pl, good_idat(p), iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({good_ihdr(p), pl, good_idat(p), pl, iend})) == PngStatus::BadChunk);
  }
  SECTION("IDAT run must be contiguous") {
    const Bytes z = zlib_stored(raw_data(s));
    const Bytes a(z.begin(), z.begin() + 10), b(z.begin() + 10, z.end());
    CHECK(status_of(file_of({ihdr, chunk("IDAT", a), chunk("gAMA", Bytes{0, 0, 0, 1}), chunk("IDAT", b), iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({ihdr, chunk("IDAT", a), chunk("tEXt", Bytes{'k', 0, 'v'}), chunk("IDAT", b), iend})) == PngStatus::BadChunk);
  }
  SECTION("no IDAT") {
    CHECK(status_of(file_of({ihdr, iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({ihdr, chunk("gAMA", Bytes{0, 0, 0, 1}), iend})) == PngStatus::BadChunk);
  }
  SECTION("IEND must be empty and last") {
    CHECK(status_of(file_of({ihdr, idat, chunk("IEND", Bytes{1})})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({ihdr, idat, iend, iend})) == PngStatus::BadChunk);
    Bytes trailing = file_of({ihdr, idat, iend});
    trailing.push_back(0);
    CHECK(status_of(trailing) == PngStatus::BadChunk);
    Bytes many = file_of({ihdr, idat, iend});
    append(many, Bytes(100, 0x55));
    CHECK(status_of(many) == PngStatus::BadChunk);
  }
  SECTION("unknown critical chunks") {
    CHECK(status_of(file_of({ihdr, chunk("ABCD", Bytes{1}), idat, iend})) == PngStatus::BadChunk);
    CHECK(status_of(file_of({ihdr, idat, chunk("ZZZZ"), iend})) == PngStatus::BadChunk);
  }
  SECTION("chunk types must be four letters") {
    Bytes f = file_of({ihdr, idat, iend});
    const auto spans = chunk_spans(f);
    Bytes digit = f;
    digit[spans[1].first + 4] = '1';
    CHECK(status_of(digit) == PngStatus::BadChunk);
    Bytes zero = f;
    zero[spans[0].first + 7] = 0;
    CHECK(status_of(zero) == PngStatus::BadChunk);
  }
  SECTION("a chunk length above 2^31-1") {
    Bytes f = file_of({ihdr, idat, iend});
    f[chunk_spans(f)[1].first] = 0x80;
    CHECK(status_of(f) == PngStatus::BadChunk);
    f[chunk_spans(f)[1].first] = 0xFF;
    CHECK(status_of(f) == PngStatus::BadChunk);
  }
}

TEST_CASE("BadCrc: every chunk kind", "[image][png]") {
  Spec p = make_spec(3, 2, 3, 8, false, 0);
  p.trns = {255, 128};
  Bytes f = signature();
  append(f, good_ihdr(p));
  append(f, chunk("gAMA", Bytes{0, 0, 0xB1, 0x8F}));
  append(f, chunk("PLTE", p.plte));
  append(f, chunk("tRNS", p.trns));
  append(f, chunk("teXt", Bytes{'a', 0, 'b'}));
  append(f, chunk("acTL", Bytes{0, 0, 0, 1, 0, 0, 0, 0}));
  append(f, good_idat(p));
  append(f, chunk("IEND"));
  REQUIRE(status_of(f) == PngStatus::Ok);
  const std::size_t count = chunk_spans(f).size();
  for (std::size_t i = 0; i < count; ++i) {
    Bytes bad = f;
    flip_crc(bad, i);
    INFO("chunk " << i);
    CHECK(status_of(bad) == PngStatus::BadCrc);
    Bytes data = f;
    const auto sp = chunk_spans(data)[i];
    if (sp.second > 12) {
      data[sp.first + 8] ^= 0x40;  // first data byte
      CHECK(status_of(data) == PngStatus::BadCrc);
    }
    Bytes type = f;
    type[sp.first + 7] ^= 0x01;  // last type letter (stays a letter or not, never accepted)
    CHECK(status_of(type) != PngStatus::Ok);
  }
}

TEST_CASE("BadHeader: at least two triggers", "[image][png]") {
  const Spec s = base_spec();
  auto with_ihdr = [&](const Bytes& body) { return file_of({chunk("IHDR", body), good_idat(s), chunk("IEND")}); };
  SECTION("zero size") {
    CHECK(status_of(with_ihdr(ihdr_body(0, 2, 8, 6))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 0, 8, 6))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(0, 0, 8, 6))) == PngStatus::BadHeader);
  }
  SECTION("colour type") {
    for (int ct : {1, 5, 7, 255}) CHECK(status_of(with_ihdr(ihdr_body(3, 2, 8, ct))) == PngStatus::BadHeader);
  }
  SECTION("bit depth and colour type combinations") {
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 3, 6))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 0, 0))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 4, 6))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 2, 2))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 16, 3))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 1, 4))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 9, 0))) == PngStatus::BadHeader);
  }
  SECTION("compression, filter and interlace methods") {
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 8, 6, 1))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 8, 6, 0, 1))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 8, 6, 0, 0, 2))) == PngStatus::BadHeader);
    CHECK(status_of(with_ihdr(ihdr_body(3, 2, 8, 6, 0, 0, 255))) == PngStatus::BadHeader);
  }
  SECTION("dimension above 2^31-1") {
    CHECK(status_of(with_ihdr(ihdr_body(0x80000000u, 1, 8, 6))) == PngStatus::BadHeader);
  }
}

TEST_CASE("BadZlib: at least two triggers per rule", "[image][png]") {
  const Spec s = base_spec();
  const Bytes raw = raw_data(s);
  const Bytes good_z = zlib_stored(raw);
  auto with_z = [&](const Bytes& z) { return file_of({good_ihdr(s), chunk("IDAT", z), chunk("IEND")}); };
  SECTION("header") {
    Bytes a = good_z;
    a[0] = 0x79;  // method 9
    a[1] = static_cast<std::uint8_t>(a[1] + (31 - ((a[0] << 8) + a[1]) % 31) % 31);
    CHECK(status_of(with_z(a)) == PngStatus::BadZlib);
    Bytes b = good_z;
    b[0] = 0x88;  // window 64 KB, method 8
    b[1] = 0;
    b[1] = static_cast<std::uint8_t>((31 - (b[0] << 8) % 31) % 31);
    CHECK(status_of(with_z(b)) == PngStatus::BadZlib);
    Bytes c = good_z;
    c[1] ^= 0x01;  // check bits wrong
    CHECK(status_of(with_z(c)) == PngStatus::BadZlib);
  }
  SECTION("FDICT") {
    Bytes a = good_z;
    a[1] = 0x20;
    a[1] = static_cast<std::uint8_t>(a[1] + (31 - ((a[0] << 8) + a[1]) % 31) % 31);
    REQUIRE((a[1] & 0x20) != 0);
    CHECK(status_of(with_z(a)) == PngStatus::BadZlib);
    Bytes dict = a;
    dict.insert(dict.begin() + 2, {0, 0, 0, 1});
    CHECK(status_of(with_z(dict)) == PngStatus::BadZlib);
  }
  SECTION("Adler-32 mismatch") {
    Bytes a = good_z;
    a.back() ^= 0x01;
    CHECK(status_of(with_z(a)) == PngStatus::BadZlib);
    Bytes b = good_z;
    b[b.size() - 4] ^= 0x80;
    CHECK(status_of(with_z(b)) == PngStatus::BadZlib);
  }
  SECTION("malformed DEFLATE stream") {
    CHECK(status_of(with_z({0x78, 0x01, 0x07, 0x00, 0x00, 0x00, 0x01})) == PngStatus::BadZlib);  // block type 3
    Bytes a = good_z;
    a[2] = 0x01;  // final stored block with a corrupt NLEN
    a[5] ^= 0xFF;
    CHECK(status_of(with_z(a)) == PngStatus::BadZlib);
  }
  SECTION("too short, or no data at all") {
    CHECK(status_of(with_z({})) == PngStatus::BadZlib);
    CHECK(status_of(with_z({0x78})) == PngStatus::BadZlib);
    CHECK(status_of(with_z({0x78, 0x01, 0x03, 0x00})) == PngStatus::BadZlib);
  }
  SECTION("the DEFLATE stream must end exactly at the Adler-32") {
    Bytes a = good_z;
    a.insert(a.end() - 4, {0, 0});  // two bytes between the last block and the Adler-32
    CHECK(status_of(with_z(a)) == PngStatus::BadZlib);
    Bytes b = good_z;
    b.insert(b.end(), {0});  // a byte after the Adler-32
    CHECK(status_of(with_z(b)) == PngStatus::BadZlib);
    Bytes c = good_z;
    c.erase(c.end() - 5);  // Adler-32 starts one byte early
    CHECK(status_of(with_z(c)) == PngStatus::BadZlib);
  }
}

TEST_CASE("BadFilter: filter byte above 4 and wrong inflated size", "[image][png]") {
  const Spec s = base_spec();
  const Bytes ihdr = good_ihdr(s), iend = chunk("IEND");
  auto with_raw = [&](const Bytes& raw) { return file_of({ihdr, chunk("IDAT", zlib_stored(raw)), iend}); };
  const Bytes raw = raw_data(s);
  SECTION("filter type") {
    for (std::uint8_t f : {std::uint8_t{5}, std::uint8_t{6}, std::uint8_t{255}}) {
      Bytes r = raw;
      r[0] = f;
      CHECK(status_of(with_raw(r)) == PngStatus::BadFilter);
    }
    Bytes late = raw;
    late[raw.size() / 2] = 9;  // second row
    CHECK(status_of(with_raw(late)) == PngStatus::BadFilter);
  }
  SECTION("inflated data too short") {
    CHECK(status_of(with_raw(Bytes(raw.begin(), raw.end() - 1))) == PngStatus::BadFilter);
    CHECK(status_of(with_raw(Bytes(raw.begin(), raw.begin() + 5))) == PngStatus::BadFilter);
    CHECK(status_of(with_raw({})) == PngStatus::BadFilter);
  }
  SECTION("inflated data too long") {
    Bytes longer = raw;
    longer.push_back(0);
    CHECK(status_of(with_raw(longer)) == PngStatus::BadFilter);
    Bytes twice = raw;
    append(twice, raw);
    CHECK(status_of(with_raw(twice)) == PngStatus::BadFilter);
  }
  SECTION("interlaced data in a non-interlaced file and the reverse") {
    Spec il = s;
    il.interlaced = true;
    CHECK(status_of(file_of({ihdr, chunk("IDAT", zlib_stored(raw_data(il))), iend})) == PngStatus::BadFilter);
    Spec ni = make_spec(5, 5, 6, 8, false, 0);
    Spec as_il = ni;
    as_il.interlaced = true;
    const Bytes wrong = file_of({good_ihdr(as_il), good_idat(ni), iend});
    CHECK(status_of(wrong) == PngStatus::BadFilter);
  }
}

TEST_CASE("TooLarge: limits at the boundary and one above", "[image][png]") {
  PngLimits lim;
  lim.max_dim = 16;
  lim.max_pixels = 100;
  SECTION("max_dim") {
    CHECK(status_of(build(make_spec(16, 1, 6, 8, false, 0)), lim) == PngStatus::Ok);
    CHECK(status_of(build(make_spec(1, 16, 6, 8, false, 0)), lim) == PngStatus::Ok);
    CHECK(status_of(build(make_spec(17, 1, 6, 8, false, 0)), lim) == PngStatus::TooLarge);
    CHECK(status_of(build(make_spec(1, 17, 6, 8, false, 0)), lim) == PngStatus::TooLarge);
  }
  SECTION("max_pixels") {
    CHECK(status_of(build(make_spec(10, 10, 6, 8, false, 0)), lim) == PngStatus::Ok);
    CHECK(status_of(build(make_spec(10, 11, 6, 8, false, 0)), lim) == PngStatus::TooLarge);
    CHECK(status_of(build(make_spec(11, 10, 0, 1, true, 0)), lim) == PngStatus::TooLarge);
    lim.max_pixels = 99;
    CHECK(status_of(build(make_spec(10, 10, 6, 8, false, 0)), lim) == PngStatus::TooLarge);
  }
  SECTION("default limits") {
    const PngLimits def;
    CHECK(def.max_dim == 8192);
    CHECK(def.max_pixels == (16ull << 20));
    // 8193 wide is rejected from the header alone: no pixel data is needed.
    const Bytes tiny = file_of({chunk("IHDR", ihdr_body(8193, 1, 8, 0)), chunk("IEND")});
    CHECK(status_of(tiny) == PngStatus::TooLarge);
    const Bytes square = file_of({chunk("IHDR", ihdr_body(8192, 8192, 8, 0)), chunk("IEND")});
    CHECK(status_of(square) == PngStatus::TooLarge);  // 64 Mi pixels above the 16 Mi default
  }
}

TEST_CASE("huge claimed sizes in tiny files fail fast without allocating", "[image][png]") {
  const auto start = std::chrono::steady_clock::now();
  const Bytes idat = chunk("IDAT", zlib_stored(Bytes(16, 0)));
  const std::uint32_t big = 0x7FFFFFFFu;
  CHECK(status_of(file_of({chunk("IHDR", ihdr_body(big, big, 16, 6)), idat, chunk("IEND")})) == PngStatus::TooLarge);
  PngLimits huge;
  huge.max_dim = 0xFFFFFFFFu;
  huge.max_pixels = ~0ull;
  CHECK(status_of(file_of({chunk("IHDR", ihdr_body(big, big, 16, 6, 0, 0, 1)), idat, chunk("IEND")}), huge) == PngStatus::TooLarge);
  // Generous limits and a size that is addressable: the tiny stream ends long before any big buffer would be needed.
  huge.max_pixels = 1ull << 40;
  const PngStatus st = status_of(file_of({chunk("IHDR", ihdr_body(30000, 30000, 16, 6)), idat, chunk("IEND")}), huge);
  CHECK(st != PngStatus::Ok);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  CHECK(ms < 2000);
}

TEST_CASE("ancillary chunks are CRC-checked and ignored", "[image][png]") {
  const Spec s = make_spec(3, 3, 2, 8, false, 1, 5);
  const Bytes ihdr = good_ihdr(s), idat = good_idat(s);
  const Bytes f = file_of({ihdr, chunk("gAMA", Bytes{0, 1, 0x86, 0xA0}), chunk("sRGB", Bytes{0}), chunk("cHRM", Bytes(32, 1)),
                           chunk("iCCP", Bytes{'x', 0, 0, 1, 2, 3}), chunk("pHYs", Bytes(9, 0)), chunk("tEXt", Bytes{'a', 0, 'b'}),
                           chunk("zzZz", Bytes(5, 7)), idat, chunk("tIME", Bytes(7, 1)), chunk("IEND")});
  const PngResult r = decode(f);
  REQUIRE(r.ok());
  CHECK(r.image.rgba == expected_rgba(s));
}

TEST_CASE("APNG decodes as its default image", "[image][png]") {
  const Spec s = make_spec(4, 3, 6, 8, false, 4, 8);
  Bytes fctl(26, 0);
  fctl[3] = 0;
  fctl[7] = 4;
  fctl[11] = 3;
  const Spec s2 = make_spec(4, 3, 6, 8, false, 0, 99);
  Bytes fdat = {0, 0, 0, 1};
  append(fdat, zlib_stored(raw_data(s2)));
  SECTION("fcTL before IDAT: the default image is the animation's first frame") {
    const Bytes f = file_of({good_ihdr(s), chunk("acTL", Bytes{0, 0, 0, 2, 0, 0, 0, 0}), chunk("fcTL", fctl), good_idat(s),
                             chunk("fcTL", fctl), chunk("fdAT", fdat), chunk("IEND")});
    const PngResult r = decode(f);
    REQUIRE(r.ok());
    CHECK(r.image.rgba == expected_rgba(s));
  }
  SECTION("no fcTL before IDAT: the IDAT image is not part of the animation but is still the image") {
    const Bytes f = file_of({good_ihdr(s), chunk("acTL", Bytes{0, 0, 0, 1, 0, 0, 0, 0}), good_idat(s), chunk("fcTL", fctl),
                             chunk("fdAT", fdat), chunk("IEND")});
    const PngResult r = decode(f);
    REQUIRE(r.ok());
    CHECK(r.image.rgba == expected_rgba(s));
  }
  SECTION("a bad CRC in a fdAT chunk is still an error") {
    Bytes f = file_of({good_ihdr(s), good_idat(s), chunk("fcTL", fctl), chunk("fdAT", fdat), chunk("IEND")});
    flip_crc(f, 3);
    CHECK(status_of(f) == PngStatus::BadCrc);
  }
}

TEST_CASE("IDAT edge cases: split anywhere, empty chunks, one byte chunks", "[image][png]") {
  const Spec s = make_spec(6, 5, 6, 8, false, 2, 3);
  const Bytes z = zlib_stored(raw_data(s));
  const Bytes ihdr = good_ihdr(s), iend = chunk("IEND");
  SECTION("split at every byte") {
    for (std::size_t cut = 0; cut <= z.size(); ++cut) {
      const Bytes a(z.begin(), z.begin() + static_cast<std::ptrdiff_t>(cut)), b(z.begin() + static_cast<std::ptrdiff_t>(cut), z.end());
      const PngResult r = decode(file_of({ihdr, chunk("IDAT", a), chunk("IDAT", b), iend}));
      INFO("cut " << cut);
      REQUIRE(r.ok());
      CHECK(r.image.rgba == expected_rgba(s));
    }
  }
  SECTION("empty IDAT chunks around the data") {
    const PngResult r = decode(file_of({ihdr, chunk("IDAT"), chunk("IDAT", z), chunk("IDAT"), iend}));
    REQUIRE(r.ok());
    CHECK(r.image.rgba == expected_rgba(s));
  }
  SECTION("only empty IDAT chunks is a BadZlib") {
    CHECK(status_of(file_of({ihdr, chunk("IDAT"), iend})) == PngStatus::BadZlib);
    CHECK(status_of(file_of({ihdr, chunk("IDAT"), chunk("IDAT"), iend})) == PngStatus::BadZlib);
  }
  SECTION("one byte per chunk") {
    std::vector<Bytes> chunks = {ihdr};
    for (std::uint8_t b : z) chunks.push_back(chunk("IDAT", Bytes{b}));
    chunks.push_back(iend);
    const PngResult r = decode(file_of(chunks));
    REQUIRE(r.ok());
    CHECK(r.image.rgba == expected_rgba(s));
  }
}

TEST_CASE("error messages are short and never contain input bytes", "[image][png]") {
  const Spec s = base_spec();
  const PngResult r = decode(file_of({good_ihdr(s), chunk("QQQQ", Bytes{'S', 'E', 'C', 'R', 'E', 'T'}), good_idat(s), chunk("IEND")}));
  REQUIRE(r.status == PngStatus::BadChunk);
  CHECK(!r.message.empty());
  CHECK(r.message.size() < 100);
  CHECK(r.message.find("QQQQ") == std::string::npos);
  CHECK(r.message.find("SECRET") == std::string::npos);
  CHECK(r.image.rgba.empty());
  CHECK(r.image.width == 0);
}

TEST_CASE("every status is reachable and a failure never returns an image", "[image][png]") {
  const Spec s = base_spec();
  std::vector<PngStatus> seen;
  const Bytes ok = build(s);
  Bytes crc = ok;
  flip_crc(crc, 1);
  const Bytes bytes[] = {ok, Bytes{}, Bytes(ok.begin(), ok.end() - 3), file_of({good_idat(s)}), crc,
                         file_of({chunk("IHDR", ihdr_body(0, 1, 8, 6)), chunk("IEND")}),
                         file_of({good_ihdr(make_spec(2, 2, 3, 8, false, 0)), good_idat(make_spec(2, 2, 3, 8, false, 0)), chunk("IEND")}),
                         file_of({good_ihdr(s), chunk("IDAT", Bytes(8, 0)), chunk("IEND")}),
                         file_of({good_ihdr(s), chunk("IDAT", zlib_stored(Bytes(3, 9))), chunk("IEND")}),
                         file_of({chunk("IHDR", ihdr_body(9000, 1, 8, 6)), chunk("IEND")})};
  for (const Bytes& b : bytes) {
    const PngResult r = decode(b);
    seen.push_back(r.status);
    if (!r.ok()) {
      CHECK(r.image.rgba.empty());
      CHECK_FALSE(r.image.valid());
    }
  }
  const PngStatus all[] = {PngStatus::Ok, PngStatus::NotPng, PngStatus::Truncated, PngStatus::BadChunk, PngStatus::BadCrc,
                           PngStatus::BadHeader, PngStatus::BadPalette, PngStatus::BadZlib, PngStatus::BadFilter, PngStatus::TooLarge};
  for (const PngStatus st : all) {
    bool found = false;
    for (const PngStatus x : seen) found = found || x == st;
    CHECK(found);
  }
}

TEST_CASE("encode_png returns an empty vector for an invalid image", "[image][png][encode]") {
  Rgba8 a;
  CHECK(encode_png(a).empty());
  Rgba8 b;
  b.width = 2;
  b.height = 2;
  b.rgba.assign(15, 0);
  CHECK(encode_png(b).empty());
  b.rgba.assign(17, 0);
  CHECK(encode_png(b).empty());
  Rgba8 c;
  c.width = 0;
  c.height = 3;
  CHECK(encode_png(c).empty());
  Rgba8 d;
  d.width = 3;
  d.height = 0;
  d.rgba.assign(12, 1);
  CHECK(encode_png(d).empty());
}

namespace {
Rgba8 random_image(std::uint32_t w, std::uint32_t h, std::uint32_t seed, int mode) {
  Rgba8 im;
  im.width = w;
  im.height = h;
  im.rgba.resize(std::size_t{w} * h * 4);
  std::uint32_t x = seed * 747796405u + 2891336453u;
  for (std::size_t i = 0; i < im.rgba.size(); ++i) {
    x = x * 1664525u + 1013904223u;
    const std::uint8_t noise = static_cast<std::uint8_t>(x >> 24);
    switch (mode) {
      case 0: im.rgba[i] = noise; break;                                           // incompressible
      case 1: im.rgba[i] = static_cast<std::uint8_t>((i / 4) % 256);               // smooth horizontal ramp
              break;
      case 2: im.rgba[i] = static_cast<std::uint8_t>((noise & 3) ? 200 : noise);  // mostly flat
              break;
      default: im.rgba[i] = (i % 4 == 3) ? static_cast<std::uint8_t>(i / 4) : noise; break;
    }
  }
  return im;
}
}  // namespace

TEST_CASE("encode then decode is pixel-exact for fixed-seed random images at every level", "[image][png][encode]") {
  const std::uint32_t sizes[][2] = {{1, 1}, {1, 9}, {9, 1}, {13, 7}, {64, 33}, {100, 3}};
  for (int level = 0; level <= 9; ++level) {
    for (int mode = 0; mode < 4; ++mode) {
      for (const auto& sz : sizes) {
        const Rgba8 im = random_image(sz[0], sz[1], static_cast<std::uint32_t>(level * 31 + mode), mode);
        const Bytes png = encode_png(im, level);
        INFO("level " << level << " mode " << mode << " size " << sz[0] << "x" << sz[1]);
        REQUIRE(!png.empty());
        const PngResult r = decode(png);
        REQUIRE(r.status == PngStatus::Ok);
        CHECK(r.image.width == im.width);
        CHECK(r.image.height == im.height);
        CHECK(r.image.rgba == im.rgba);
      }
    }
  }
}

TEST_CASE("encode_png is deterministic and clamps the level", "[image][png][encode]") {
  const Rgba8 im = random_image(40, 30, 5, 2);
  for (int level = 0; level <= 9; ++level) CHECK(encode_png(im, level) == encode_png(im, level));
  CHECK(encode_png(im, -5) == encode_png(im, 0));
  CHECK(encode_png(im, 99) == encode_png(im, 9));
  CHECK(encode_png(im) == encode_png(im, 6));
}

TEST_CASE("encode_png writes IHDR, one IDAT and IEND: colour type 6, depth 8, no interlace", "[image][png][encode]") {
  const Rgba8 im = random_image(5, 4, 2, 0);
  const Bytes png = encode_png(im, 6);
  REQUIRE(png.size() > 45);
  CHECK(Bytes(png.begin(), png.begin() + 8) == signature());
  const auto spans = chunk_spans(png);
  REQUIRE(spans.size() == 3);
  CHECK(std::string(png.begin() + 12, png.begin() + 16) == "IHDR");
  CHECK(png[16 + 3] == 5);
  CHECK(png[20 + 3] == 4);
  CHECK(png[24] == 8);
  CHECK(png[25] == 6);
  CHECK(png[26] == 0);
  CHECK(png[27] == 0);
  CHECK(png[28] == 0);
  CHECK(std::string(png.begin() + static_cast<std::ptrdiff_t>(spans[1].first) + 4, png.begin() + static_cast<std::ptrdiff_t>(spans[1].first) + 8) == "IDAT");
  CHECK(std::string(png.end() - 8, png.end() - 4) == "IEND");
  // The zlib header is the 32 KB window deflate header with valid check bits.
  const std::size_t z = spans[1].first + 8;
  CHECK(png[z] == 0x78);
  CHECK(((png[z] << 8) | png[z + 1]) % 31 == 0);
  CHECK((png[z + 1] & 0x20) == 0);
}

namespace {
// Filter bytes the encoder chose, read back through the DEFLATE codec.
Bytes chosen_filters(const Bytes& png, std::uint32_t w, std::uint32_t h) {
  const auto spans = chunk_spans(png);
  const std::size_t z = spans[1].first + 8;
  const std::size_t zlen = spans[1].second - 12;
  const auto inf = lumora::package::inflate_raw(png.data() + z + 2, zlen - 2, std::uint64_t{h} * (w * 4 + 1));
  REQUIRE(inf.status == lumora::package::InflateStatus::Ok);
  Bytes f;
  for (std::uint32_t y = 0; y < h; ++y) f.push_back(inf.data[std::size_t{y} * (w * 4 + 1)]);
  return f;
}
}  // namespace

TEST_CASE("encoder picks the filter with the smallest sum of absolute differences", "[image][png][encode]") {
  SECTION("a horizontal ramp prefers Sub") {
    Rgba8 im;
    im.width = 32;
    im.height = 4;
    for (std::uint32_t y = 0; y < 4; ++y)
      for (std::uint32_t x = 0; x < 32; ++x) {
        const std::uint8_t v = static_cast<std::uint8_t>(10 + x * 2);
        for (int c = 0; c < 4; ++c) im.rgba.push_back(v);
      }
    const Bytes f = chosen_filters(encode_png(im, 6), 32, 4);
    CHECK(f[0] == 1);
  }
  SECTION("identical noisy rows prefer Up after the first row") {
    Rgba8 row = random_image(32, 1, 4, 0);
    Rgba8 im;
    im.width = 32;
    im.height = 6;
    for (int y = 0; y < 6; ++y) im.rgba.insert(im.rgba.end(), row.rgba.begin(), row.rgba.end());
    const Bytes f = chosen_filters(encode_png(im, 6), 32, 6);
    for (std::size_t y = 1; y < 6; ++y) CHECK(f[y] == 2);
  }
  SECTION("a flat image ties on every filter and takes the lowest, None") {
    Rgba8 im;
    im.width = 8;
    im.height = 3;
    im.rgba.assign(8 * 3 * 4, 0);
    const Bytes f = chosen_filters(encode_png(im, 6), 8, 3);
    for (const std::uint8_t v : f) CHECK(v == 0);
  }
}

TEST_CASE("a 2048x2048 image encodes and decodes within a few seconds", "[image][png][encode][perf]") {
  Rgba8 im;
  im.width = 2048;
  im.height = 2048;
  im.rgba.resize(std::size_t{2048} * 2048 * 4);
  std::uint32_t x = 1;
  for (std::uint32_t y = 0; y < 2048; ++y)
    for (std::uint32_t px = 0; px < 2048; ++px) {
      x = x * 1664525u + 1013904223u;
      std::uint8_t* p = &im.rgba[(std::size_t{y} * 2048 + px) * 4];
      p[0] = static_cast<std::uint8_t>(px / 8);
      p[1] = static_cast<std::uint8_t>(y / 8);
      p[2] = static_cast<std::uint8_t>((px ^ y) & 0xFF);
      p[3] = static_cast<std::uint8_t>(255 - ((x >> 30) & 1u));
    }
  const auto start = std::chrono::steady_clock::now();
  const Bytes png = encode_png(im, 6);
  const PngResult r = decode(png);
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  REQUIRE(!png.empty());
  REQUIRE(r.status == PngStatus::Ok);
  CHECK(r.image.rgba == im.rgba);
  INFO("encode + decode took " << seconds << " s");
  CHECK(seconds < 30.0);
}
