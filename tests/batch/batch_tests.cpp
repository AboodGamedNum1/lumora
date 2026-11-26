// Tests for core/batch. Pure CPU, deterministic (own LCG, no std distributions). Golden scene digests are also written in
// docs/batch.md and must match on Linux and Windows.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "batch.h"

// The scene generators below must not be fused into multiply-add, or their inputs would change with the compiler flags.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

using namespace lumora::batch;
namespace xf = lumora::xform;

namespace {

float from_bits(std::uint32_t b) {
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}
std::uint32_t to_bits(float f) {
  std::uint32_t b;
  std::memcpy(&b, &f, sizeof b);
  return b;
}
const float kNaN = from_bits(0x7fc00000u);
const float kInf = from_bits(0x7f800000u);
float up(float f) { return from_bits(to_bits(f) + 1); }  // next float above a positive finite f

struct Lcg {
  std::uint32_t s;
  std::uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
  float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
};

// A 1 x 1 sprite at (x, y); the red tint byte carries an id so output order can be read back.
Sprite spr(std::uint8_t id, std::int32_t layer, Blend blend, std::uint32_t tex, float x = 0, float y = 0) {
  Sprite s;
  s.transform = xf::translation(x, y);
  s.layer = layer;
  s.blend = blend;
  s.texture = tex;
  s.tint[0] = id;
  return s;
}

struct Out {
  std::vector<Vertex> v;
  std::vector<std::uint32_t> i;
  std::vector<DrawBatch> b;
  BatchResult r;
};

Out run(const std::vector<Sprite>& s, const Rect* cull = nullptr) {
  Out o;
  o.v.resize(s.size() * 4 + 4);
  o.i.resize(s.size() * 6 + 6);
  o.b.resize(s.size() + 1);
  o.r = build_batches(s.data(), s.size(), cull, o.v.data(), o.v.size(), o.i.data(), o.i.size(), o.b.data(), o.b.size());
  return o;
}

std::vector<int> ids(const Out& o) {
  std::vector<int> r;
  for (std::size_t k = 0; k < o.r.sprites_drawn; ++k) r.push_back(o.v[4 * k].rgba[0]);
  return r;
}

std::uint64_t digest_of(const Out& o) {
  return digest_buffers(o.v.data(), o.r.vertex_count, o.i.data(), o.r.index_count, o.b.data(), o.r.batch_count);
}

constexpr std::uint64_t kGoldenMixed = 0x7bf77c4738ae3857ull;
constexpr std::uint64_t kGoldenRotatedCulled = 0xd5ccb04c626f574bull;
constexpr std::uint64_t kGoldenUi = 0x6840ba61d77a156cull;

std::vector<Sprite> scene_mixed() {
  Lcg g{42};
  std::vector<Sprite> s;
  for (int i = 0; i < 300; ++i) {
    Sprite p;
    p.transform = xf::multiply(xf::translation(g.range(-100, 100), g.range(-100, 100)), xf::scaling(g.range(0.5f, 3), g.range(0.5f, 3)));
    p.size = {g.range(1, 20), g.range(1, 20)};
    p.uv = {g.unit(), g.unit(), g.unit() + 1, g.unit() + 1};
    for (int c = 0; c < 4; ++c) p.tint[c] = static_cast<std::uint8_t>(g.next() >> 24);
    p.texture = g.next() % 5;
    p.layer = static_cast<std::int32_t>(g.next() % 4) - 1;
    p.blend = static_cast<Blend>(g.next() % 3);
    s.push_back(p);
  }
  return s;
}

std::vector<Sprite> scene_rotated() {
  Lcg g{7};
  std::vector<Sprite> s;
  for (int i = 0; i < 200; ++i) {
    Sprite p;
    p.transform = xf::compose_trs({g.range(-300, 300), g.range(-300, 300)}, g.range(-6, 6), {g.range(0.25f, 2), g.range(0.25f, 2)});
    p.size = {g.range(2, 30), g.range(2, 30)};
    p.texture = g.next() % 3;
    p.layer = static_cast<std::int32_t>(g.next() % 3);
    p.blend = static_cast<Blend>(g.next() % 3);
    p.tint[0] = static_cast<std::uint8_t>(i);
    s.push_back(p);
  }
  return s;
}

std::vector<Sprite> scene_ui() {
  std::vector<Sprite> s;
  const Affine quarter{0, 1, -1, 0, 100, 0};  // exact 90 degree turn plus a shift
  for (int i = 0; i < 40; ++i) {
    Sprite p;
    p.transform = (i % 2) ? quarter : xf::translation(static_cast<float>(i * 8), static_cast<float>(i * 4));
    p.transform.tx += static_cast<float>(i);
    p.size = {static_cast<float>(4 + i % 5), static_cast<float>(2 + i % 3)};
    p.texture = static_cast<std::uint32_t>(i % 4);
    p.layer = (i % 7) - 3;
    p.blend = (i % 5 == 0) ? Blend::Opaque : ((i % 3 == 0) ? Blend::Additive : Blend::Alpha);
    p.uv = {0, 0, 0.5f, 0.25f};
    p.tint[3] = static_cast<std::uint8_t>(255 - 5 * i);
    s.push_back(p);
  }
  return s;
}

}  // namespace

TEST_CASE("batch_status_text has a distinct text per status", "[batch]") {
  const BatchStatus all[] = {BatchStatus::Ok, BatchStatus::TooManySprites, BatchStatus::BadSprite, BatchStatus::BadViewBounds,
                             BatchStatus::BufferTooSmall};
  for (std::size_t i = 0; i < 5; ++i) {
    REQUIRE(std::strlen(batch_status_text(all[i])) > 0);
    for (std::size_t j = i + 1; j < 5; ++j) REQUIRE(std::strcmp(batch_status_text(all[i]), batch_status_text(all[j])) != 0);
  }
}

TEST_CASE("digest_buffers is FNV-1a 64 over vertices, indices and batches", "[batch][digest]") {
  REQUIRE(digest_buffers(nullptr, 0, nullptr, 0, nullptr, 0) == 0xcbf29ce484222325ull);
  Vertex v{};
  v.x = 1.0f;
  v.y = 2.0f;
  v.u = 0.5f;
  v.v = 0.25f;
  v.rgba[0] = 1;
  v.rgba[1] = 2;
  v.rgba[2] = 3;
  v.rgba[3] = 4;
  const std::uint32_t idx[3] = {0, 1, 2};
  const DrawBatch b = {7, Blend::Opaque, 0, 6};
  REQUIRE(digest_buffers(&v, 1, idx, 3, &b, 1) == 0x23d38fec211036c3ull);
}

TEST_CASE("validation: statuses, order, and nothing is written on error", "[batch][validate]") {
  std::vector<Sprite> ok = {spr(1, 0, Blend::Alpha, 0)};
  std::vector<Vertex> vs(8);
  std::vector<std::uint32_t> is(12, 0xdeadbeefu);
  std::vector<DrawBatch> bs(2);
  std::memset(vs.data(), 0xab, vs.size() * sizeof(Vertex));
  std::memset(bs.data(), 0xcd, bs.size() * sizeof(DrawBatch));
  auto sentinel_ok = [&]() {
    for (const Vertex& v : vs) {
      if (v.rgba[0] != 0xab) return false;
    }
    for (std::uint32_t i : is) {
      if (i != 0xdeadbeefu) return false;
    }
    const unsigned char* p = reinterpret_cast<const unsigned char*>(bs.data());
    for (std::size_t k = 0; k < bs.size() * sizeof(DrawBatch); ++k) {
      if (p[k] != 0xcd) return false;
    }
    return true;
  };
  auto call = [&](const std::vector<Sprite>& s, const Rect* cull, std::size_t vcap, std::size_t icap, std::size_t bcap) {
    return build_batches(s.data(), s.size(), cull, vs.data(), vcap, is.data(), icap, bs.data(), bcap);
  };
  REQUIRE(call(ok, nullptr, 8, 12, 2).status == BatchStatus::Ok);
  std::memset(vs.data(), 0xab, vs.size() * sizeof(Vertex));
  std::fill(is.begin(), is.end(), 0xdeadbeefu);
  std::memset(bs.data(), 0xcd, bs.size() * sizeof(DrawBatch));

  struct Case {
    const char* name;
    void (*mutate)(Sprite&);
  };
  const Case bad[] = {
      {"transform a nan", [](Sprite& s) { s.transform.a = from_bits(0x7fc00000u); }},
      {"transform b inf", [](Sprite& s) { s.transform.b = from_bits(0x7f800000u); }},
      {"transform c -inf", [](Sprite& s) { s.transform.c = from_bits(0xff800000u); }},
      {"transform d nan", [](Sprite& s) { s.transform.d = from_bits(0x7fc00000u); }},
      {"transform tx inf", [](Sprite& s) { s.transform.tx = from_bits(0x7f800000u); }},
      {"transform ty nan", [](Sprite& s) { s.transform.ty = from_bits(0x7fc00000u); }},
      {"size x zero", [](Sprite& s) { s.size.x = 0; }},
      {"size y negative", [](Sprite& s) { s.size.y = -1; }},
      {"size x nan", [](Sprite& s) { s.size.x = from_bits(0x7fc00000u); }},
      {"size y inf", [](Sprite& s) { s.size.y = from_bits(0x7f800000u); }},
      {"uv min_x nan", [](Sprite& s) { s.uv.min_x = from_bits(0x7fc00000u); }},
      {"uv min_y inf", [](Sprite& s) { s.uv.min_y = from_bits(0x7f800000u); }},
      {"uv max_x inf", [](Sprite& s) { s.uv.max_x = from_bits(0xff800000u); }},
      {"uv max_y nan", [](Sprite& s) { s.uv.max_y = from_bits(0x7fc00000u); }},
      {"blend 3", [](Sprite& s) { s.blend = static_cast<Blend>(3); }},
      {"blend 255", [](Sprite& s) { s.blend = static_cast<Blend>(255); }},
  };
  for (const Case& c : bad) {
    INFO(c.name);
    std::vector<Sprite> s = {spr(1, 0, Blend::Alpha, 0), spr(2, 0, Blend::Alpha, 0)};
    c.mutate(s[1]);  // the second sprite is bad: the first bad sprite decides
    const BatchResult r = call(s, nullptr, 8, 12, 2);
    REQUIRE(r.status == BatchStatus::BadSprite);
    REQUIRE(r.sprites_drawn == 0);
    REQUIRE(r.vertex_count == 0);
    REQUIRE(sentinel_ok());
  }
  // Small and large finite values are valid, also negative uv and uv with min > max.
  {
    std::vector<Sprite> s = {spr(1, 0, Blend::Alpha, 0)};
    s[0].size = {from_bits(1u), 3.0e38f};
    s[0].uv = {-5, 7, -9, 1e30f};
    REQUIRE(call(s, nullptr, 8, 12, 2).status == BatchStatus::Ok);
    std::fill(is.begin(), is.end(), 0xdeadbeefu);
    std::memset(vs.data(), 0xab, vs.size() * sizeof(Vertex));
    std::memset(bs.data(), 0xcd, bs.size() * sizeof(DrawBatch));
  }
  // Null sprites with a count is a bad sprite; count 0 with nulls is fine.
  REQUIRE(build_batches(nullptr, 3, nullptr, vs.data(), 8, is.data(), 12, bs.data(), 2).status == BatchStatus::BadSprite);
  const BatchResult empty = build_batches(nullptr, 0, nullptr, nullptr, 0, nullptr, 0, nullptr, 0);
  REQUIRE(empty.status == BatchStatus::Ok);
  REQUIRE(empty.sprites_drawn == 0);
  REQUIRE(empty.batch_count == 0);

  // View bounds.
  const Rect bad_rects[] = {{2, 0, 1, 1}, {0, 2, 1, 1}, {kNaN, 0, 1, 1}, {0, kNaN, 1, 1}, {0, 0, kInf, 1}, {0, 0, 1, kInf},
                            {-kInf, 0, 1, 1}};
  for (const Rect& r : bad_rects) {
    REQUIRE(call(ok, &r, 8, 12, 2).status == BatchStatus::BadViewBounds);
    REQUIRE(sentinel_ok());
  }
  const Rect point = {1, 1, 1, 1};  // min == max is a valid (degenerate) rectangle
  REQUIRE(call(ok, &point, 8, 12, 2).status == BatchStatus::Ok);
  std::fill(is.begin(), is.end(), 0xdeadbeefu);
  std::memset(vs.data(), 0xab, vs.size() * sizeof(Vertex));
  std::memset(bs.data(), 0xcd, bs.size() * sizeof(DrawBatch));

  // Order of checks: sprites before view bounds before capacity.
  std::vector<Sprite> both = {spr(1, 0, Blend::Alpha, 0)};
  both[0].size.x = 0;
  REQUIRE(call(both, &bad_rects[0], 0, 0, 0).status == BatchStatus::BadSprite);
  REQUIRE(call(ok, &bad_rects[0], 0, 0, 0).status == BatchStatus::BadViewBounds);
  REQUIRE(call(ok, nullptr, 0, 0, 0).status == BatchStatus::BufferTooSmall);
  REQUIRE(sentinel_ok());

  // Too many sprites: the count alone decides, even before the sprites are read.
  std::vector<Sprite> many(kMaxSprites + 1, spr(1, 0, Blend::Alpha, 0));
  many[0].size.x = 0;
  const BatchResult tm = build_batches(many.data(), many.size(), nullptr, vs.data(), 8, is.data(), 12, bs.data(), 2);
  REQUIRE(tm.status == BatchStatus::TooManySprites);
  REQUIRE(sentinel_ok());
}

TEST_CASE("kMaxSprites and kMaxSprites + 1", "[batch][limits]") {
  std::vector<Sprite> s(kMaxSprites, spr(1, 0, Blend::Alpha, 3));
  Out o;
  o.v.resize(4 * kMaxSprites);
  o.i.resize(6 * kMaxSprites);
  o.b.resize(kMaxSprites);
  o.r = build_batches(s.data(), s.size(), nullptr, o.v.data(), o.v.size(), o.i.data(), o.i.size(), o.b.data(), o.b.size());
  REQUIRE(o.r.status == BatchStatus::Ok);
  REQUIRE(o.r.sprites_drawn == kMaxSprites);
  REQUIRE(o.r.vertex_count == 4 * kMaxSprites);
  REQUIRE(o.r.index_count == 6 * kMaxSprites);
  REQUIRE(o.r.batch_count == 1);
  REQUIRE(o.b[0].index_count == 6 * kMaxSprites);
  REQUIRE(o.i[6 * (kMaxSprites - 1) + 5] == 4 * (kMaxSprites - 1) + 3);
  s.push_back(spr(1, 0, Blend::Alpha, 3));
  REQUIRE(build_batches(s.data(), s.size(), nullptr, o.v.data(), o.v.size(), o.i.data(), o.i.size(), o.b.data(), o.b.size()).status ==
          BatchStatus::TooManySprites);
}

TEST_CASE("culling: closed rectangles, touching, outside, rotated", "[batch][cull]") {
  std::vector<Sprite> s = {spr(1, 0, Blend::Alpha, 0)};  // covers (0,0)-(1,1)
  auto count = [&](const Rect& r) { return run(s, &r).r.sprites_drawn; };
  REQUIRE(count(Rect{0.5f, 0.5f, 0.6f, 0.6f}) == 1);  // inside
  REQUIRE(count(Rect{-5, -5, 5, 5}) == 1);            // contains the sprite
  REQUIRE(count(Rect{1, 1, 2, 2}) == 1);              // touching at a corner
  REQUIRE(count(Rect{1, 0, 2, 1}) == 1);              // touching along an edge
  REQUIRE(count(Rect{-1, -1, 0, 0}) == 1);            // touching at the other corner
  REQUIRE(count(Rect{up(1.0f), 0, 2, 1}) == 0);       // one ulp beyond the right edge
  REQUIRE(count(Rect{0, up(1.0f), 1, 2}) == 0);
  REQUIRE(count(Rect{-2, 0, -from_bits(0x00000001u), 1}) == 0);  // smallest gap on the left
  REQUIRE(count(Rect{5, 5, 6, 6}) == 0);
  const Out o = run(s, &(const Rect&)Rect{5, 5, 6, 6});
  REQUIRE(o.r.status == BatchStatus::Ok);
  REQUIRE(o.r.sprites_culled == 1);
  REQUIRE(o.r.vertex_count == 0);
  REQUIRE(o.r.batch_count == 0);
  // Exact 90 degree matrix: (0,0)-(2,1) maps to x in [-1, 0], y in [0, 2].
  std::vector<Sprite> q = {spr(1, 0, Blend::Alpha, 0)};
  q[0].transform = Affine{0, 1, -1, 0, 0, 0};
  q[0].size = {2, 1};
  auto qcount = [&](const Rect& r) { return run(q, &r).r.sprites_drawn; };
  REQUIRE(qcount(Rect{0, -5, 5, 0}) == 1);       // touches the corner (0, 0)
  REQUIRE(qcount(Rect{-3, 2, 0, 3}) == 1);       // touches at (0, 2)
  REQUIRE(qcount(Rect{0.0f, 2.5f, 5, 3}) == 0);  // above
  REQUIRE(qcount(Rect{0.5f, 0, 5, 2}) == 0);     // to the right of x = 0
  // The bounds are axis aligned: a 45 degree diamond is kept by a rectangle in the empty corner of its bounds.
  std::vector<Sprite> d = {spr(1, 0, Blend::Alpha, 0)};
  d[0].transform = Affine{0.5f, 0.5f, -0.5f, 0.5f, 0, 0};  // exact entries
  d[0].size = {2, 2};                                      // bounds x in [-1, 1], y in [0, 2]
  const Rect corner = {0.9f, 1.9f, 5, 5};
  REQUIRE(run(d, &corner).r.sprites_drawn == 1);
  const Rect outside = {1.5f, 0, 5, 2};
  REQUIRE(run(d, &outside).r.sprites_drawn == 0);
  // Culling keeps the order and the culled count adds up.
  std::vector<Sprite> m;
  for (int i = 0; i < 10; ++i) m.push_back(spr(static_cast<std::uint8_t>(i), 0, Blend::Alpha, 0, static_cast<float>(i * 3), 0));
  const Rect view = {5, 0, 13, 1};  // sprites at x = 6, 9, 12 and the one at 3 does not reach, 15 is outside
  const Out mo = run(m, &view);
  REQUIRE(mo.r.sprites_drawn == 3);
  REQUIRE(mo.r.sprites_culled == 7);
  REQUIRE(ids(mo) == std::vector<int>({2, 3, 4}));
}

TEST_CASE("ordering across layers, blends and textures", "[batch][order]") {
  const std::vector<Sprite> s = {
      spr(0, 0, Blend::Alpha, 5),    spr(1, 0, Blend::Opaque, 2),   spr(2, 0, Blend::Additive, 1), spr(3, 0, Blend::Alpha, 3),
      spr(4, 0, Blend::Opaque, 1),   spr(5, 0, Blend::Opaque, 2),   spr(6, 0, Blend::Additive, 1), spr(7, -1, Blend::Alpha, 9),
  };
  const Out o = run(s);
  REQUIRE(o.r.status == BatchStatus::Ok);
  // layer -1 first; in layer 0: Opaque by texture (1, then 2 with ids 1, 5), Additive tex 1 (2, 6), Alpha in input order.
  REQUIRE(ids(o) == std::vector<int>({7, 4, 1, 5, 2, 6, 0, 3}));
  REQUIRE(o.r.batch_count == 6);
  REQUIRE(o.b[0].texture == 9);
  REQUIRE(o.b[0].blend == Blend::Alpha);
  REQUIRE(o.b[0].first_index == 0);
  REQUIRE(o.b[0].index_count == 6);
  REQUIRE(o.b[1].texture == 1);
  REQUIRE(o.b[1].blend == Blend::Opaque);
  REQUIRE(o.b[1].first_index == 6);
  REQUIRE(o.b[1].index_count == 6);
  REQUIRE(o.b[2].texture == 2);
  REQUIRE(o.b[2].blend == Blend::Opaque);
  REQUIRE(o.b[2].index_count == 12);
  REQUIRE(o.b[3].texture == 1);
  REQUIRE(o.b[3].blend == Blend::Additive);
  REQUIRE(o.b[3].index_count == 12);
  REQUIRE(o.b[4].texture == 5);
  REQUIRE(o.b[5].texture == 3);
  REQUIRE(o.b[5].first_index == 7 * 6);
  // Alpha sprites keep strict input order whatever the texture, so alternating textures give one batch each.
  const std::vector<Sprite> a = {spr(0, 0, Blend::Alpha, 3), spr(1, 0, Blend::Alpha, 1), spr(2, 0, Blend::Alpha, 3),
                                 spr(3, 0, Blend::Alpha, 1)};
  const Out ao = run(a);
  REQUIRE(ids(ao) == std::vector<int>({0, 1, 2, 3}));
  REQUIRE(ao.r.batch_count == 4);
  // Opaque sprites with alternating textures are grouped: textures 3 and 1 become two batches.
  const std::vector<Sprite> p = {spr(0, 0, Blend::Opaque, 3), spr(1, 0, Blend::Opaque, 1), spr(2, 0, Blend::Opaque, 3),
                                 spr(3, 0, Blend::Opaque, 1)};
  const Out po = run(p);
  REQUIRE(ids(po) == std::vector<int>({1, 3, 0, 2}));
  REQUIRE(po.r.batch_count == 2);
  // Layers sort as signed integers, lowest first, including the extremes.
  const std::int32_t lo = std::numeric_limits<std::int32_t>::min(), hi = std::numeric_limits<std::int32_t>::max();
  const std::vector<Sprite> l = {spr(0, hi, Blend::Opaque, 0), spr(1, 0, Blend::Alpha, 0), spr(2, lo, Blend::Additive, 0),
                                 spr(3, -1, Blend::Opaque, 0), spr(4, 1, Blend::Opaque, 0)};
  REQUIRE(ids(run(l)) == std::vector<int>({2, 3, 1, 4, 0}));
  // Equal sprites keep their input order (stability), also for a larger set.
  std::vector<Sprite> st;
  for (int i = 0; i < 200; ++i) st.push_back(spr(static_cast<std::uint8_t>(i), i % 3, Blend::Additive, 4));
  const Out so = run(st);
  int last_layer = -1, last_id = -1;
  for (std::size_t k = 0; k < so.r.sprites_drawn; ++k) {
    const int id = so.v[4 * k].rgba[0], layer = id % 3;
    if (layer == last_layer) REQUIRE(id > last_id);
    else REQUIRE(layer > last_layer);
    last_layer = layer;
    last_id = id;
  }
  // A layer change does not break a batch when texture and blend are equal (merge on consecutive output order).
  const std::vector<Sprite> ml = {spr(0, 0, Blend::Alpha, 2), spr(1, 1, Blend::Alpha, 2), spr(2, 2, Blend::Alpha, 2)};
  REQUIRE(run(ml).r.batch_count == 1);
  // Different blend with the same texture breaks the batch.
  const std::vector<Sprite> mb = {spr(0, 0, Blend::Opaque, 2), spr(1, 0, Blend::Additive, 2), spr(2, 0, Blend::Alpha, 2)};
  REQUIRE(run(mb).r.batch_count == 3);
}

TEST_CASE("vertices, uvs, tint and index patterns", "[batch][vertices]") {
  std::vector<Sprite> s = {spr(9, 0, Blend::Alpha, 4, 10, 20), spr(8, 0, Blend::Alpha, 4, -3, 0.5f)};
  s[0].size = {2, 3};
  s[0].uv = {0.25f, 0.5f, 0.75f, 1.5f};
  s[0].tint[0] = 11;
  s[0].tint[1] = 22;
  s[0].tint[2] = 33;
  s[0].tint[3] = 44;
  s[1].uv = {1, 1, 0, 0};  // reversed uv is allowed
  const Out o = run(s);
  REQUIRE(o.r.status == BatchStatus::Ok);
  REQUIRE(o.r.vertex_count == 8);
  REQUIRE(o.r.index_count == 12);
  REQUIRE(o.r.batch_count == 1);
  const float ex[4] = {10, 12, 12, 10}, ey[4] = {20, 20, 23, 23}, eu[4] = {0.25f, 0.75f, 0.75f, 0.25f},
              ev[4] = {0.5f, 0.5f, 1.5f, 1.5f};
  for (int j = 0; j < 4; ++j) {
    REQUIRE(o.v[j].x == ex[j]);
    REQUIRE(o.v[j].y == ey[j]);
    REQUIRE(o.v[j].u == eu[j]);
    REQUIRE(o.v[j].v == ev[j]);
    REQUIRE(o.v[j].rgba[0] == 11);
    REQUIRE(o.v[j].rgba[1] == 22);
    REQUIRE(o.v[j].rgba[2] == 33);
    REQUIRE(o.v[j].rgba[3] == 44);
  }
  REQUIRE(o.v[4].x == -3);
  REQUIRE(o.v[5].x == -2);
  REQUIRE(o.v[6].y == 1.5f);
  REQUIRE(o.v[4].u == 1);
  REQUIRE(o.v[4].v == 1);
  REQUIRE(o.v[6].u == 0);
  REQUIRE(o.v[6].v == 0);
  const std::uint32_t pattern[6] = {0, 1, 2, 0, 2, 3};
  for (std::uint32_t k = 0; k < 2; ++k) {
    for (int j = 0; j < 6; ++j) REQUIRE(o.i[6 * k + j] == 4 * k + pattern[j]);
  }
  REQUIRE(o.b[0].first_index == 0);
  REQUIRE(o.b[0].index_count == 12);
  REQUIRE(o.b[0].texture == 4);
}

TEST_CASE("exact positions with exact-value matrices", "[batch][vertices]") {
  std::vector<Sprite> s(3);
  s[0].transform = xf::scaling(2, 3);
  s[0].size = {4, 5};
  s[1].transform = Affine{0, 1, -1, 0, 0, 0};  // rotate by 90 degrees counter-clockwise
  s[1].size = {4, 5};
  s[2].transform = Affine{2, 0, 0, -2, 10, 10};  // scale with a y flip, then translate
  s[2].size = {3, 1};
  for (int i = 0; i < 3; ++i) s[i].layer = i;
  const Out o = run(s);
  REQUIRE(o.r.status == BatchStatus::Ok);
  const float ex0[4] = {0, 8, 8, 0}, ey0[4] = {0, 0, 15, 15};
  const float ex1[4] = {0, 0, -5, -5}, ey1[4] = {0, 4, 4, 0};
  const float ex2[4] = {10, 16, 16, 10}, ey2[4] = {10, 10, 8, 8};
  for (int j = 0; j < 4; ++j) {
    REQUIRE(o.v[j].x == ex0[j]);
    REQUIRE(o.v[j].y == ey0[j]);
    REQUIRE(o.v[4 + j].x == ex1[j]);
    REQUIRE(o.v[4 + j].y == ey1[j]);
    REQUIRE(o.v[8 + j].x == ex2[j]);
    REQUIRE(o.v[8 + j].y == ey2[j]);
  }
  // A zero coordinate is always +0, never a negative zero.
  for (const Vertex& v : o.v) {
    if (v.x == 0.0f) REQUIRE(to_bits(v.x) == 0u);
    if (v.y == 0.0f) REQUIRE(to_bits(v.y) == 0u);
  }
  // One rounding: a double evaluation with an exact float product and sum agrees with xform::apply.
  Sprite r;
  r.transform = Affine{0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
  r.size = {7.7f, 3.3f};
  const Out ro = run({r});
  const Vec2 corners[4] = {{0, 0}, {7.7f, 0}, {7.7f, 3.3f}, {0, 3.3f}};
  for (int j = 0; j < 4; ++j) {
    const xf::Vec2 e = xf::apply(r.transform, corners[j]);
    REQUIRE(to_bits(ro.v[j].x) == to_bits(e.x));
    REQUIRE(to_bits(ro.v[j].y) == to_bits(e.y));
    const double x = static_cast<double>(r.transform.a) * corners[j].x + static_cast<double>(r.transform.c) * corners[j].y +
                     static_cast<double>(r.transform.tx);
    REQUIRE(to_bits(ro.v[j].x) == to_bits(static_cast<float>(x)));
  }
}

TEST_CASE("capacity: exact fit, one short, nothing written", "[batch][capacity]") {
  const std::vector<Sprite> s = {spr(0, 0, Blend::Alpha, 1), spr(1, 0, Blend::Alpha, 2), spr(2, 0, Blend::Alpha, 2),
                                 spr(3, 0, Blend::Alpha, 1, 100, 100)};
  // 4 drawn, 3 batches (1, 2+2, 1).
  std::vector<Vertex> v(16);
  std::vector<std::uint32_t> i(24);
  std::vector<DrawBatch> b(3);
  BatchResult r = build_batches(s.data(), 4, nullptr, v.data(), 16, i.data(), 24, b.data(), 3);
  REQUIRE(r.status == BatchStatus::Ok);
  REQUIRE(r.batch_count == 3);
  REQUIRE(b[1].index_count == 12);
  auto fill = [&]() {
    std::memset(v.data(), 0xab, v.size() * sizeof(Vertex));
    std::fill(i.begin(), i.end(), 0xdeadbeefu);
    std::memset(b.data(), 0xcd, b.size() * sizeof(DrawBatch));
  };
  auto untouched = [&]() {
    for (const Vertex& x : v) {
      if (x.rgba[0] != 0xab) return false;
    }
    for (std::uint32_t x : i) {
      if (x != 0xdeadbeefu) return false;
    }
    return reinterpret_cast<const unsigned char*>(b.data())[0] == 0xcd;
  };
  const std::size_t caps[3][3] = {{15, 24, 3}, {16, 23, 3}, {16, 24, 2}};
  for (const auto& c : caps) {
    fill();
    r = build_batches(s.data(), 4, nullptr, v.data(), c[0], i.data(), c[1], b.data(), c[2]);
    REQUIRE(r.status == BatchStatus::BufferTooSmall);
    REQUIRE(untouched());
    REQUIRE(r.sprites_drawn == 4);  // the required sizes are reported
    REQUIRE(r.vertex_count == 16);
    REQUIRE(r.index_count == 24);
    REQUIRE(r.batch_count == 3);
  }
  // Null buffers with a requirement are too small too.
  r = build_batches(s.data(), 4, nullptr, nullptr, 16, i.data(), 24, b.data(), 3);
  REQUIRE(r.status == BatchStatus::BufferTooSmall);
  // Culled sprites need no room: only the two sprites in view count.
  const Rect view = {-1, -1, 2, 2};
  r = build_batches(s.data(), 4, &view, v.data(), 12, i.data(), 18, b.data(), 2);
  REQUIRE(r.status == BatchStatus::Ok);
  REQUIRE(r.sprites_drawn == 3);
  REQUIRE(r.sprites_culled == 1);
  REQUIRE(r.batch_count == 2);
  // Zero sprites (or all culled) need no buffers.
  const Rect none = {500, 500, 600, 600};
  r = build_batches(s.data(), 4, &none, nullptr, 0, nullptr, 0, nullptr, 0);
  REQUIRE(r.status == BatchStatus::Ok);
  REQUIRE(r.sprites_culled == 4);
}

TEST_CASE("golden scene digests", "[batch][digest]") {
  const Out mixed = run(scene_mixed());
  REQUIRE(mixed.r.status == BatchStatus::Ok);
  const Rect view = {-150, -150, 150, 150};
  const Out rot = run(scene_rotated(), &view);
  REQUIRE(rot.r.status == BatchStatus::Ok);
  REQUIRE(rot.r.sprites_culled > 0);
  REQUIRE(rot.r.sprites_drawn > 0);
  const Out ui = run(scene_ui());
  REQUIRE(ui.r.status == BatchStatus::Ok);
  INFO("mixed " << std::hex << digest_of(mixed) << " rotated " << digest_of(rot) << " ui " << digest_of(ui));
  INFO("counts " << std::dec << mixed.r.batch_count << " " << rot.r.sprites_drawn << " " << rot.r.batch_count << " " << ui.r.batch_count);
  REQUIRE(digest_of(mixed) == kGoldenMixed);
  REQUIRE(digest_of(rot) == kGoldenRotatedCulled);
  REQUIRE(digest_of(ui) == kGoldenUi);
}

TEST_CASE("performance: 16384 sprites", "[batch][perf]") {
  Lcg g{3};
  std::vector<Sprite> s(kMaxSprites);
  for (Sprite& p : s) {
    p.transform = xf::compose_trs({g.range(-500, 500), g.range(-500, 500)}, g.range(-3, 3), {g.range(0.5f, 2), g.range(0.5f, 2)});
    p.size = {g.range(1, 16), g.range(1, 16)};
    p.texture = g.next() % 16;
    p.layer = static_cast<std::int32_t>(g.next() % 8);
    p.blend = static_cast<Blend>(g.next() % 3);
  }
  Out o;
  o.v.resize(4 * kMaxSprites);
  o.i.resize(6 * kMaxSprites);
  o.b.resize(kMaxSprites);
  const Rect view = {-400, -400, 400, 400};
  const auto t0 = std::chrono::steady_clock::now();
  std::uint64_t acc = 0;
  for (int rep = 0; rep < 20; ++rep) {
    o.r = build_batches(s.data(), s.size(), rep % 2 ? &view : nullptr, o.v.data(), o.v.size(), o.i.data(), o.i.size(), o.b.data(),
                        o.b.size());
    REQUIRE(o.r.status == BatchStatus::Ok);
    acc += digest_of(o);
  }
  REQUIRE(acc != 0);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  INFO("seconds " << secs);
  REQUIRE(secs < 10.0);
}
