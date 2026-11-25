// Tests for the atlas packer. Every check against pixels uses its own reference code in this file.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "atlas.h"
#include "json_value.h"

using namespace lumora::image;

namespace {

Rgba8 sprite(std::uint32_t w, std::uint32_t h, std::uint32_t seed) {
  Rgba8 im;
  im.width = w;
  im.height = h;
  im.rgba.resize(std::size_t{w} * h * 4);
  std::uint32_t x = seed * 2654435761u + 977u;
  for (auto& b : im.rgba) {
    x = x * 1664525u + 1013904223u;
    b = static_cast<std::uint8_t>(x >> 24);
  }
  return im;
}

AtlasInput input(const std::string& name, std::uint32_t w, std::uint32_t h, std::uint32_t seed = 1) {
  AtlasInput in;
  in.name = name;
  in.image = sprite(w, h, seed);
  return in;
}

std::uint32_t floor_pow2(std::uint32_t v) {
  std::uint32_t p = 1;
  while ((p << 1) <= v) p <<= 1;
  return p;
}
bool is_pow2(std::uint32_t v) { return v != 0 && (v & (v - 1)) == 0; }

const std::uint8_t* px(const Rgba8& im, std::uint32_t x, std::uint32_t y) { return &im.rgba[(std::size_t{y} * im.width + x) * 4]; }

bool same_pixel(const std::uint8_t* a, const std::uint8_t* b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }

// Full invariant check of an Ok result against its inputs and limits.
void check_result(const std::vector<AtlasInput>& in, const AtlasLimits& lim, const AtlasResult& r) {
  REQUIRE(r.status == AtlasStatus::Ok);
  REQUIRE(r.image.valid());
  const std::uint32_t cap = lim.power_of_two ? floor_pow2(lim.max_size) : lim.max_size;
  CHECK(r.image.width <= cap);
  CHECK(r.image.height <= cap);
  if (lim.power_of_two) {
    CHECK(is_pow2(r.image.width));
    CHECK(is_pow2(r.image.height));
  }
  REQUIRE(r.entries.size() == in.size());
  const std::uint32_t p = lim.padding;
  std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>> cells;
  for (std::size_t i = 0; i < in.size(); ++i) {
    const AtlasEntry& e = r.entries[i];
    CHECK(e.name == in[i].name);
    REQUIRE(e.w == in[i].image.width);
    REQUIRE(e.h == in[i].image.height);
    // The padded cell is inside the atlas.
    REQUIRE(e.x >= p);
    REQUIRE(e.y >= p);
    REQUIRE(std::uint64_t{e.x} + e.w + p <= r.image.width);
    REQUIRE(std::uint64_t{e.y} + e.h + p <= r.image.height);
    cells.emplace_back(e.x - p, e.y - p, e.w + 2 * p, e.h + 2 * p);
    // Sprite pixels are exact.
    const Rgba8& s = in[i].image;
    for (std::uint32_t y = 0; y < e.h; ++y)
      for (std::uint32_t x = 0; x < e.w; ++x) REQUIRE(same_pixel(px(r.image, e.x + x, e.y + y), px(s, x, y)));
    // The ring is the extruded border: every cell pixel equals the clamped sprite pixel.
    for (std::uint32_t cy = 0; cy < e.h + 2 * p; ++cy)
      for (std::uint32_t cx = 0; cx < e.w + 2 * p; ++cx) {
        const std::uint32_t sx = cx < p ? 0 : std::min(cx - p, e.w - 1);
        const std::uint32_t sy = cy < p ? 0 : std::min(cy - p, e.h - 1);
        REQUIRE(same_pixel(px(r.image, e.x - p + cx, e.y - p + cy), px(s, sx, sy)));
      }
  }
  // Padded cells never overlap.
  for (std::size_t a = 0; a < cells.size(); ++a)
    for (std::size_t b = a + 1; b < cells.size(); ++b) {
      const auto [ax, ay, aw, ah] = cells[a];
      const auto [bx, by, bw, bh] = cells[b];
      const bool apart = ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay;
      REQUIRE(apart);
    }
}

void check_failed(const AtlasResult& r, AtlasStatus status) {
  CHECK(r.status == status);
  CHECK_FALSE(r.ok());
  CHECK(!r.message.empty());
  CHECK(r.message.size() < 100);
  CHECK(r.image.rgba.empty());
  CHECK(r.image.width == 0);
  CHECK(r.image.height == 0);
  CHECK(r.entries.empty());
  CHECK(atlas_index_json(r).empty());
}

std::uint32_t lcg(std::uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s >> 8;
}

}  // namespace

TEST_CASE("a single sprite: no padding, and with padding the ring is extruded", "[atlas]") {
  AtlasLimits lim;
  lim.padding = 0;
  auto r = pack_atlas({input("a", 1, 1)}, lim);
  REQUIRE(r.ok());
  CHECK(r.image.width == 1);
  CHECK(r.image.height == 1);
  CHECK(r.entries[0].x == 0);
  CHECK(r.entries[0].y == 0);
  lim.padding = 2;
  const std::vector<AtlasInput> in = {input("a", 1, 1, 5)};
  r = pack_atlas(in, lim);
  REQUIRE(r.ok());
  CHECK(r.image.width == 5);
  CHECK(r.image.height == 5);
  CHECK(r.entries[0].x == 2);
  CHECK(r.entries[0].y == 2);
  check_result(in, lim, r);
  for (std::uint32_t y = 0; y < 5; ++y)
    for (std::uint32_t x = 0; x < 5; ++x) CHECK(same_pixel(px(r.image, x, y), px(in[0].image, 0, 0)));
}

TEST_CASE("padding extrudes edges and corners of a 3x2 sprite", "[atlas]") {
  AtlasLimits lim;
  lim.padding = 1;
  Rgba8 s;
  s.width = 3;
  s.height = 2;
  s.rgba = {1, 0, 0, 255, 2, 0, 0, 255, 3, 0, 0, 255, 4, 0, 0, 255, 5, 0, 0, 255, 6, 0, 0, 255};
  AtlasInput in;
  in.name = "s";
  in.image = s;
  const auto r = pack_atlas({in}, lim);
  REQUIRE(r.ok());
  REQUIRE(r.image.width == 5);
  REQUIRE(r.image.height == 4);
  const std::uint8_t expect_red[4][5] = {{1, 1, 2, 3, 3}, {1, 1, 2, 3, 3}, {4, 4, 5, 6, 6}, {4, 4, 5, 6, 6}};
  for (std::uint32_t y = 0; y < 4; ++y)
    for (std::uint32_t x = 0; x < 5; ++x) {
      CHECK(px(r.image, x, y)[0] == expect_red[y][x]);
      CHECK(px(r.image, x, y)[3] == 255);
    }
}

TEST_CASE("no-overlap, pixel-exact and extrusion invariants over 240 fixed-seed random sets", "[atlas][property]") {
  std::uint32_t seed = 20261126;
  for (int set = 0; set < 240; ++set) {
    const std::uint32_t count = 1 + lcg(seed) % 24;
    AtlasLimits lim;
    lim.padding = lcg(seed) % 5;
    lim.power_of_two = (lcg(seed) % 3) == 0;
    lim.max_size = 256 + lcg(seed) % 700;
    std::vector<AtlasInput> in;
    for (std::uint32_t i = 0; i < count; ++i) {
      const std::uint32_t w = 1 + lcg(seed) % (set % 2 ? 60 : 24);
      const std::uint32_t h = 1 + lcg(seed) % (set % 3 ? 40 : 12);
      in.push_back(input("s" + std::to_string(i), w, h, static_cast<std::uint32_t>(set * 100) + i));
    }
    const auto r = pack_atlas(in, lim);
    INFO("set " << set << " count " << count << " padding " << lim.padding << " pot " << lim.power_of_two << " max " << lim.max_size);
    check_result(in, lim, r);
  }
}

TEST_CASE("repeated packing is byte-identical", "[atlas]") {
  std::vector<AtlasInput> in;
  for (std::uint32_t i = 0; i < 40; ++i) in.push_back(input("n" + std::to_string(i), 3 + (i * 7) % 29, 2 + (i * 11) % 31, i));
  const auto a = pack_atlas(in);
  const auto b = pack_atlas(in);
  const auto c = pack_atlas(std::vector<AtlasInput>(in));
  REQUIRE(a.ok());
  CHECK(a.image.rgba == b.image.rgba);
  CHECK(a.image.rgba == c.image.rgba);
  CHECK(a.image.width == b.image.width);
  CHECK(atlas_index_json(a) == atlas_index_json(b));
  CHECK(atlas_index_json(a) == atlas_index_json(c));
}

TEST_CASE("input permutations: the set of placed rectangles depends only on the multiset of sizes", "[atlas]") {
  // Inputs are sorted by height, then width, then input index. Equal-size sprites therefore take the slots in input
  // order, and any shuffle of the inputs produces the same set of rectangles. Names follow their sprites.
  std::vector<AtlasInput> in;
  const std::uint32_t sizes[][2] = {{16, 16}, {16, 16}, {16, 16}, {16, 16}, {8, 24}, {8, 24}, {30, 5}, {30, 5}, {7, 7}, {7, 7}};
  for (std::uint32_t i = 0; i < 10; ++i) in.push_back(input("s" + std::to_string(i), sizes[i][0], sizes[i][1], i));
  AtlasLimits lim;
  lim.padding = 1;
  auto rects = [](const AtlasResult& r) {
    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>> s;
    for (const auto& e : r.entries) s.insert({e.x, e.y, e.w, e.h});
    return s;
  };
  const auto base = pack_atlas(in, lim);
  REQUIRE(base.ok());
  std::uint32_t seed = 7;
  for (int round = 0; round < 25; ++round) {
    std::vector<AtlasInput> shuffled = in;
    for (std::size_t i = shuffled.size() - 1; i > 0; --i) std::swap(shuffled[i], shuffled[lcg(seed) % (i + 1)]);
    const auto r = pack_atlas(shuffled, lim);
    REQUIRE(r.ok());
    CHECK(r.image.width == base.image.width);
    CHECK(r.image.height == base.image.height);
    CHECK(rects(r) == rects(base));
    check_result(shuffled, lim, r);
  }
  // Two equal-size sprites swapped: same rectangles, swapped names.
  std::vector<AtlasInput> swapped = in;
  std::swap(swapped[0], swapped[1]);
  const auto r = pack_atlas(swapped, lim);
  REQUIRE(r.ok());
  CHECK(r.entries[0].x == base.entries[0].x);
  CHECK(r.entries[0].y == base.entries[0].y);
  CHECK(r.entries[1].x == base.entries[1].x);
  CHECK(r.entries[0].name == "s1");
}

TEST_CASE("limits: BadLimits has several triggers", "[atlas][status]") {
  const std::vector<AtlasInput> in = {input("a", 4, 4)};
  AtlasLimits lim;
  lim.max_size = 0;
  check_failed(pack_atlas(in, lim), AtlasStatus::BadLimits);
  lim.max_size = 16385;
  check_failed(pack_atlas(in, lim), AtlasStatus::BadLimits);
  lim.max_size = 0xFFFFFFFFu;
  check_failed(pack_atlas(in, lim), AtlasStatus::BadLimits);
  lim = AtlasLimits{};
  lim.padding = 17;
  check_failed(pack_atlas(in, lim), AtlasStatus::BadLimits);
  lim.padding = 0xFFFFFFFFu;
  check_failed(pack_atlas(in, lim), AtlasStatus::BadLimits);
  lim = AtlasLimits{};
  lim.max_inputs = 0;
  check_failed(pack_atlas(in, lim), AtlasStatus::BadLimits);
  // The boundaries themselves are accepted.
  lim = AtlasLimits{};
  lim.max_size = 1;
  lim.padding = 0;
  CHECK(pack_atlas({input("a", 1, 1)}, lim).ok());
  lim = AtlasLimits{};
  lim.max_size = 16384;
  lim.padding = 16;
  CHECK(pack_atlas(in, lim).ok());
}

TEST_CASE("NoInputs and TooManyInputs", "[atlas][status]") {
  check_failed(pack_atlas({}), AtlasStatus::NoInputs);
  check_failed(pack_atlas(std::vector<AtlasInput>{}, AtlasLimits{}), AtlasStatus::NoInputs);
  AtlasLimits lim;
  lim.max_inputs = 3;
  std::vector<AtlasInput> in;
  for (int i = 0; i < 3; ++i) in.push_back(input("s" + std::to_string(i), 2, 2));
  CHECK(pack_atlas(in, lim).ok());
  in.push_back(input("s3", 2, 2));
  check_failed(pack_atlas(in, lim), AtlasStatus::TooManyInputs);
  lim.max_inputs = 1;
  check_failed(pack_atlas(in, lim), AtlasStatus::TooManyInputs);
  lim.max_inputs = 1;
  CHECK(pack_atlas({input("only", 2, 2)}, lim).ok());
}

TEST_CASE("BadInput: names", "[atlas][status]") {
  auto with_name = [](const std::string& n) { return std::vector<AtlasInput>{input(n, 2, 2)}; };
  check_failed(pack_atlas(with_name("")), AtlasStatus::BadInput);
  check_failed(pack_atlas(with_name(std::string(129, 'a'))), AtlasStatus::BadInput);
  CHECK(pack_atlas(with_name(std::string(128, 'a'))).ok());
  CHECK(pack_atlas(with_name("a")).ok());
  for (const char c : {'\x01', '\n', '\t', '\x1f', '\x7f'}) check_failed(pack_atlas(with_name(std::string("a") + c + "b")), AtlasStatus::BadInput);
  check_failed(pack_atlas(with_name(std::string("a\0b", 3))), AtlasStatus::BadInput);
  // Invalid UTF-8 is rejected so that the JSON index is always valid text.
  check_failed(pack_atlas(with_name("\xC0\x80")), AtlasStatus::BadInput);
  check_failed(pack_atlas(with_name("a\xFF")), AtlasStatus::BadInput);
  check_failed(pack_atlas(with_name("\xE2\x82")), AtlasStatus::BadInput);
  check_failed(pack_atlas(with_name("\xED\xA0\x80")), AtlasStatus::BadInput);
  check_failed(pack_atlas(with_name("\xF4\x90\x80\x80")), AtlasStatus::BadInput);
  CHECK(pack_atlas(with_name("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80")).ok());
  CHECK(pack_atlas(with_name("sp ace/with\\odd\"chars")).ok());
}

TEST_CASE("BadInput: duplicate names and invalid images", "[atlas][status]") {
  check_failed(pack_atlas({input("a", 2, 2), input("a", 3, 3)}), AtlasStatus::BadInput);
  check_failed(pack_atlas({input("a", 2, 2), input("b", 2, 2), input("c", 2, 2), input("a", 2, 2)}), AtlasStatus::BadInput);
  CHECK(pack_atlas({input("a", 2, 2), input("A", 2, 2)}).ok());  // names are case sensitive
  AtlasInput zero = input("z", 2, 2);
  zero.image.width = 0;
  check_failed(pack_atlas({zero}), AtlasStatus::BadInput);
  AtlasInput empty = input("e", 2, 2);
  empty.image = Rgba8{};
  check_failed(pack_atlas({empty}), AtlasStatus::BadInput);
  AtlasInput short_buf = input("s", 4, 4);
  short_buf.image.rgba.pop_back();
  check_failed(pack_atlas({short_buf}), AtlasStatus::BadInput);
  AtlasInput huge_claim = input("h", 2, 2);
  huge_claim.image.width = 0xFFFFFFFFu;
  huge_claim.image.height = 0xFFFFFFFFu;
  check_failed(pack_atlas({huge_claim}), AtlasStatus::BadInput);
  // A bad input is reported even when another input would not fit.
  check_failed(pack_atlas({input("big", 5000, 1), zero}), AtlasStatus::BadInput);
}

TEST_CASE("DoesNotFit: sprite larger than max_size, with and without padding", "[atlas][status]") {
  AtlasLimits lim;
  lim.max_size = 64;
  lim.padding = 0;
  check_failed(pack_atlas({input("w", 65, 1)}, lim), AtlasStatus::DoesNotFit);
  check_failed(pack_atlas({input("h", 1, 65)}, lim), AtlasStatus::DoesNotFit);
  CHECK(pack_atlas({input("w", 64, 1)}, lim).ok());
  CHECK(pack_atlas({input("h", 1, 64)}, lim).ok());
  lim.padding = 2;
  check_failed(pack_atlas({input("w", 61, 1)}, lim), AtlasStatus::DoesNotFit);
  CHECK(pack_atlas({input("w", 60, 60)}, lim).ok());
  check_failed(pack_atlas({input("w", 60, 61)}, lim), AtlasStatus::DoesNotFit);
  lim.max_size = 16384;
  lim.padding = 0;
  check_failed(pack_atlas({input("w", 16385, 1)}, lim), AtlasStatus::DoesNotFit);
  lim.padding = 16;
  CHECK(pack_atlas({input("edge", 16352, 1)}, lim).ok());
  check_failed(pack_atlas({input("edge", 16353, 1)}, lim), AtlasStatus::DoesNotFit);
}

TEST_CASE("DoesNotFit: total area above max_size squared", "[atlas][status]") {
  AtlasLimits lim;
  lim.max_size = 32;
  lim.padding = 0;
  // Five 16x16 sprites need 1280 pixels; the atlas holds 1024.
  std::vector<AtlasInput> in;
  for (int i = 0; i < 5; ++i) in.push_back(input("s" + std::to_string(i), 16, 16, static_cast<std::uint32_t>(i)));
  check_failed(pack_atlas(in, lim), AtlasStatus::DoesNotFit);
  in.pop_back();
  const auto r = pack_atlas(in, lim);  // exactly four: they tile 32x32
  check_result(in, lim, r);
  CHECK(r.image.width == 32);
  CHECK(r.image.height == 32);
  // Many 1x1 sprites: 33 do not fit 32x1? They do not fit in a 5x5 atlas when there are 26.
  lim.max_size = 5;
  in.clear();
  for (int i = 0; i < 26; ++i) in.push_back(input("p" + std::to_string(i), 1, 1, static_cast<std::uint32_t>(i)));
  check_failed(pack_atlas(in, lim), AtlasStatus::DoesNotFit);
  in.pop_back();
  check_result(in, lim, pack_atlas(in, lim));
}

TEST_CASE("DoesNotFit only happens when nothing fits: sets that stack in one column always fit", "[atlas][status][property]") {
  // Documented bound: if every cell is at most max_size wide and the cell heights add up to at most max_size, the
  // skyline can always place them (worst case on top of each other), so the result must be Ok.
  std::uint32_t seed = 99;
  for (int set = 0; set < 300; ++set) {
    AtlasLimits lim;
    lim.max_size = 40 + lcg(seed) % 200;
    lim.padding = lcg(seed) % 4;
    lim.power_of_two = (set % 4) == 0;
    const std::uint32_t cap = lim.power_of_two ? floor_pow2(lim.max_size) : lim.max_size;
    std::vector<AtlasInput> in;
    std::uint64_t heights = 0;
    while (true) {
      const std::uint32_t w = 1 + lcg(seed) % (cap - 2 * lim.padding);
      const std::uint32_t h = 1 + lcg(seed) % 30;
      if (heights + h + 2 * lim.padding > cap) break;
      heights += h + 2 * lim.padding;
      in.push_back(input("s" + std::to_string(in.size()), w, h, static_cast<std::uint32_t>(set)));
      if (in.size() >= 40) break;
    }
    if (in.empty()) continue;
    const auto r = pack_atlas(in, lim);
    INFO("set " << set << " cap " << cap << " padding " << lim.padding);
    check_result(in, lim, r);
  }
}

TEST_CASE("max_size is never exceeded: every result is Ok inside the limit or DoesNotFit", "[atlas][status][property]") {
  std::uint32_t seed = 4242;
  int ok = 0, no = 0;
  for (int set = 0; set < 300; ++set) {
    AtlasLimits lim;
    lim.max_size = 8 + lcg(seed) % 90;
    lim.padding = lcg(seed) % 3;
    lim.power_of_two = (set % 3) == 0;
    std::vector<AtlasInput> in;
    const std::uint32_t count = 1 + lcg(seed) % 12;
    for (std::uint32_t i = 0; i < count; ++i) in.push_back(input("s" + std::to_string(i), 1 + lcg(seed) % 40, 1 + lcg(seed) % 40, static_cast<std::uint32_t>(set)));
    const auto r = pack_atlas(in, lim);
    if (r.ok()) {
      ++ok;
      check_result(in, lim, r);
    } else {
      ++no;
      check_failed(r, AtlasStatus::DoesNotFit);
    }
  }
  CHECK(ok > 30);
  CHECK(no > 30);
}

TEST_CASE("single-sprite sweep: fits exactly when both padded sides are within the cap", "[atlas][status][property]") {
  for (std::uint32_t max_size = 1; max_size <= 20; ++max_size)
    for (std::uint32_t p = 0; p <= 3; ++p)
      for (int pot = 0; pot < 2; ++pot)
        for (std::uint32_t w = 1; w <= 22; w += (w < 6 ? 1 : 3))
          for (std::uint32_t h = 1; h <= 22; h += (h < 6 ? 1 : 3)) {
            AtlasLimits lim;
            lim.max_size = max_size;
            lim.padding = p;
            lim.power_of_two = pot == 1;
            const std::uint32_t cap = pot ? floor_pow2(max_size) : max_size;
            const auto r = pack_atlas({input("a", w, h)}, lim);
            INFO("max " << max_size << " p " << p << " pot " << pot << " " << w << "x" << h);
            if (w + 2 * p <= cap && h + 2 * p <= cap) {
              REQUIRE(r.ok());
              REQUIRE(r.image.width <= cap);
              REQUIRE(r.image.height <= cap);
            } else {
              REQUIRE(r.status == AtlasStatus::DoesNotFit);
            }
          }
}

TEST_CASE("power_of_two rounds the atlas up to powers of two no larger than max_size", "[atlas]") {
  AtlasLimits lim;
  lim.power_of_two = true;
  lim.padding = 0;
  auto r = pack_atlas({input("a", 33, 5)}, lim);
  REQUIRE(r.ok());
  CHECK(r.image.width == 64);
  CHECK(r.image.height == 8);
  CHECK(r.entries[0].w == 33);
  lim.max_size = 100;  // the largest power of two that fits is 64
  r = pack_atlas({input("a", 64, 64)}, lim);
  REQUIRE(r.ok());
  CHECK(r.image.width == 64);
  check_failed(pack_atlas({input("a", 65, 1)}, lim), AtlasStatus::DoesNotFit);
  lim.power_of_two = false;
  CHECK(pack_atlas({input("a", 65, 1)}, lim).ok());
}

TEST_CASE("smallest atlas search: the result is tight, not max_size", "[atlas]") {
  AtlasLimits lim;
  lim.padding = 0;
  std::vector<AtlasInput> in;
  for (int i = 0; i < 4; ++i) in.push_back(input("s" + std::to_string(i), 16, 16, static_cast<std::uint32_t>(i)));
  auto r = pack_atlas(in, lim);
  REQUIRE(r.ok());
  CHECK(std::uint64_t{r.image.width} * r.image.height == 1024);  // the sprites' own area
  CHECK(r.image.width == 32);  // equal area: the squarer shape wins
  CHECK(r.image.height == 32);
  // One sprite: the atlas is exactly its padded size.
  lim.padding = 3;
  r = pack_atlas({input("one", 10, 7)}, lim);
  REQUIRE(r.ok());
  CHECK(r.image.width == 16);
  CHECK(r.image.height == 13);
  // A tall and a wide sprite: never larger than their bounding stack.
  lim.padding = 0;
  r = pack_atlas({input("tall", 4, 100), input("wide", 100, 4)}, lim);
  REQUIRE(r.ok());
  CHECK(std::uint64_t{r.image.width} * r.image.height <= 104ull * 104ull);
  // Many 1x1 sprites pack into a near-square.
  in.clear();
  for (int i = 0; i < 100; ++i) in.push_back(input("p" + std::to_string(i), 1, 1, static_cast<std::uint32_t>(i)));
  lim.padding = 0;
  r = pack_atlas(in, lim);
  REQUIRE(r.ok());
  CHECK(r.image.width == 10);
  CHECK(r.image.height == 10);
  check_result(in, lim, r);
}

TEST_CASE("JSON index: exact text, key order, escaping", "[atlas][json]") {
  AtlasLimits lim;
  lim.padding = 0;
  lim.max_size = 64;
  const auto r = pack_atlas({input("a", 2, 3), input("b\"c\\d", 2, 3)}, lim);
  REQUIRE(r.ok());
  const std::string json = atlas_index_json(r);
  CHECK(json.rfind("{\"width\":", 0) == 0);
  const std::string expect = "{\"width\":" + std::to_string(r.image.width) + ",\"height\":" + std::to_string(r.image.height) +
                             ",\"entries\":[{\"name\":\"a\",\"x\":" + std::to_string(r.entries[0].x) + ",\"y\":" +
                             std::to_string(r.entries[0].y) + ",\"w\":2,\"h\":3},{\"name\":\"b\\\"c\\\\d\",\"x\":" +
                             std::to_string(r.entries[1].x) + ",\"y\":" + std::to_string(r.entries[1].y) + ",\"w\":2,\"h\":3}]}";
  CHECK(json == expect);
  CHECK(json.find(' ') == std::string::npos);
  CHECK(json.find('\n') == std::string::npos);
}

TEST_CASE("JSON index parses back with the repo parser and matches the result", "[atlas][json]") {
  const std::string names[] = {"plain", "with space", "quote\"inside", "back\\slash", "slash/forward", "caf\xC3\xA9", "\xF0\x9F\x98\x80 emoji",
                               "tab-free", "{}[],:", "\xE2\x80\xA8line-sep"};
  std::vector<AtlasInput> in;
  std::uint32_t i = 0;
  for (const auto& n : names) in.push_back(input(n, 3 + i % 5, 2 + i % 4, i)), ++i;
  const auto r = pack_atlas(in);
  REQUIRE(r.ok());
  const std::string json = atlas_index_json(r);
  const auto parsed = lumora::scene::parse_json(json);
  REQUIRE(parsed.ok);
  const auto& root = parsed.value;
  REQUIRE(root.is_object());
  const auto& members = root.as_object();
  REQUIRE(members.size() == 3);
  CHECK(members[0].first == "width");
  CHECK(members[1].first == "height");
  CHECK(members[2].first == "entries");
  CHECK(root.find("width")->as_number() == r.image.width);
  CHECK(root.find("height")->as_number() == r.image.height);
  const auto& entries = root.find("entries")->as_array();
  REQUIRE(entries.size() == in.size());
  for (std::size_t k = 0; k < entries.size(); ++k) {
    const auto& o = entries[k].as_object();
    REQUIRE(o.size() == 5);
    CHECK(o[0].first == "name");
    CHECK(o[1].first == "x");
    CHECK(o[2].first == "y");
    CHECK(o[3].first == "w");
    CHECK(o[4].first == "h");
    CHECK(o[0].second.as_string() == in[k].name);
    CHECK(o[1].second.as_number() == r.entries[k].x);
    CHECK(o[2].second.as_number() == r.entries[k].y);
    CHECK(o[3].second.as_number() == r.entries[k].w);
    CHECK(o[4].second.as_number() == r.entries[k].h);
  }
}

TEST_CASE("atlas_index_json is empty for a failed or default result", "[atlas][json]") {
  CHECK(atlas_index_json(AtlasResult{}).empty());
  AtlasLimits lim;
  lim.max_size = 0;
  CHECK(atlas_index_json(pack_atlas({input("a", 1, 1)}, lim)).empty());
}

TEST_CASE("1x1 sprites, large padding and tiny atlases", "[atlas]") {
  AtlasLimits lim;
  lim.padding = 16;
  std::vector<AtlasInput> in = {input("a", 1, 1, 1), input("b", 1, 1, 2), input("c", 1, 1, 3)};
  check_result(in, lim, pack_atlas(in, lim));
  lim.max_size = 1;
  lim.padding = 0;
  check_result({input("a", 1, 1)}, lim, pack_atlas({input("a", 1, 1)}, lim));
  check_failed(pack_atlas({input("a", 1, 1), input("b", 1, 1)}, lim), AtlasStatus::DoesNotFit);
  lim.max_size = 2;
  check_result({input("a", 1, 1), input("b", 1, 1), input("c", 1, 1), input("d", 1, 1)}, lim,
               pack_atlas({input("a", 1, 1), input("b", 1, 1), input("c", 1, 1), input("d", 1, 1)}, lim));
}

TEST_CASE("1000 sprites of 16 to 128 pixels pack within the sanitized time cap", "[atlas][perf]") {
  std::vector<AtlasInput> in;
  std::uint32_t seed = 31337;
  for (std::uint32_t i = 0; i < 1000; ++i) in.push_back(input("sprite" + std::to_string(i), 16 + lcg(seed) % 113, 16 + lcg(seed) % 113, i));
  AtlasLimits lim;
  lim.max_size = 8192;
  lim.padding = 2;
  const auto start = std::chrono::steady_clock::now();
  const auto r = pack_atlas(in, lim);
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  REQUIRE(r.ok());
  INFO("pack took " << seconds << " s, atlas " << r.image.width << "x" << r.image.height);
  CHECK(seconds < 30.0);
  // Pixel and overlap checks on the whole result.
  REQUIRE(r.entries.size() == 1000);
  std::uint64_t sprite_area = 0;
  for (std::size_t i = 0; i < in.size(); ++i) sprite_area += std::uint64_t{in[i].image.width + 4} * (in[i].image.height + 4);
  CHECK(sprite_area * 100 >= std::uint64_t{r.image.width} * r.image.height * 70);  // at least 70 percent filled
  std::vector<std::uint8_t> owner(std::size_t{r.image.width} * r.image.height, 0);
  for (std::size_t i = 0; i < in.size(); ++i) {
    const auto& e = r.entries[i];
    for (std::uint32_t y = 0; y < e.h; y += 7)
      for (std::uint32_t x = 0; x < e.w; x += 5) REQUIRE(same_pixel(px(r.image, e.x + x, e.y + y), px(in[i].image, x, y)));
    for (std::uint32_t y = e.y - 2; y < e.y + e.h + 2; ++y)
      for (std::uint32_t x = e.x - 2; x < e.x + e.w + 2; ++x) {
        REQUIRE(owner[std::size_t{y} * r.image.width + x] == 0);
        owner[std::size_t{y} * r.image.width + x] = 1;
      }
  }
  CHECK(r.image.width <= 8192);
  CHECK(r.image.height <= 8192);
  CHECK(pack_atlas(in, lim).image.rgba == r.image.rgba);
}

TEST_CASE("many equal sprites and a worst-case staircase stay valid and deterministic", "[atlas][perf]") {
  std::vector<AtlasInput> in;
  for (std::uint32_t i = 0; i < 400; ++i) in.push_back(input("e" + std::to_string(i), 16, 16, i));
  AtlasLimits lim;
  lim.padding = 0;
  auto r = pack_atlas(in, lim);
  REQUIRE(r.ok());
  CHECK(std::uint64_t{r.image.width} * r.image.height == 400ull * 256ull);  // 20 by 20 tiles of 16 pixels
  in.clear();
  for (std::uint32_t i = 0; i < 200; ++i) in.push_back(input("t" + std::to_string(i), 1 + i, 200 - i, i));
  lim.max_size = 4096;
  r = pack_atlas(in, lim);
  check_result(in, lim, r);
}
