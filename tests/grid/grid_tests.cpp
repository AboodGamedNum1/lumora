// Tests for core/grid. Pure CPU, deterministic (own LCG, no std distributions). Golden scene digests are also written in
// docs/grid.md and must match on Linux and Windows.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

#include "grid.h"

// Scene generators must not be fused into multiply-add, or their inputs would change with the compiler flags.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

using namespace lumora::grid;

namespace {

float from_bits(std::uint32_t b) {
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}
const float kNaN = from_bits(0x7fc00000u);
const float kInf = from_bits(0x7f800000u);
const float kBig = from_bits(0x7f7fffffu);  // largest finite float

struct Lcg {
  std::uint32_t s;
  std::uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
  float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
};

bool hit(const Aabb& a, const Aabb& b) {
  return a.min_x <= b.max_x && b.min_x <= a.max_x && a.min_y <= b.max_y && b.min_y <= a.max_y;
}

SpatialGrid* make(float ox, float oy, float cs, std::uint32_t nx, std::uint32_t ny) {
  SpatialGrid* g = new SpatialGrid();
  REQUIRE(g->init(ox, oy, cs, nx, ny) == GridStatus::Ok);
  return g;
}

std::uint32_t add(SpatialGrid& g, const Aabb& b) {
  std::uint32_t id = 0xffffffffu;
  REQUIRE(g.insert(b, id) == GridStatus::Ok);
  return id;
}

std::vector<std::uint32_t> q(const SpatialGrid& g, const Aabb& b) {
  std::vector<std::uint32_t> out(70000);
  const std::size_t n = g.query(b, out.data(), out.size());
  out.resize(n);
  return out;
}

struct Pairs {
  std::vector<std::uint32_t> a, b;
};
Pairs all_pairs(const SpatialGrid& g) {
  Pairs p;
  const std::size_t n = g.pairs(nullptr, nullptr, 0);
  p.a.resize(n);
  p.b.resize(n);
  REQUIRE(g.pairs(p.a.data(), p.b.data(), n) == n);
  return p;
}

// Brute force reference over a mirror of the live items.
struct Mirror {
  std::vector<std::uint32_t> id;
  std::vector<Aabb> box;
  void put(std::uint32_t i, const Aabb& b) {
    id.push_back(i);
    box.push_back(b);
  }
  std::size_t at(std::uint32_t i) const {
    for (std::size_t k = 0; k < id.size(); ++k) {
      if (id[k] == i) return k;
    }
    return id.size();
  }
  void drop(std::uint32_t i) {
    const std::size_t k = at(i);
    id.erase(id.begin() + static_cast<std::ptrdiff_t>(k));
    box.erase(box.begin() + static_cast<std::ptrdiff_t>(k));
  }
  std::vector<std::uint32_t> query(const Aabb& b) const {
    std::vector<std::uint32_t> r;
    for (std::size_t k = 0; k < id.size(); ++k) {
      if (hit(box[k], b)) r.push_back(id[k]);
    }
    return sorted(r);
  }
  static std::vector<std::uint32_t> sorted(std::vector<std::uint32_t> v) {
    for (std::size_t i = 1; i < v.size(); ++i) {
      for (std::size_t j = i; j > 0 && v[j - 1] > v[j]; --j) std::swap(v[j - 1], v[j]);
    }
    return v;
  }
  Pairs pairs() const {
    std::vector<std::uint32_t> order = sorted(id);
    Pairs p;
    for (std::size_t x = 0; x < order.size(); ++x) {
      for (std::size_t y = x + 1; y < order.size(); ++y) {
        if (hit(box[at(order[x])], box[at(order[y])])) {
          p.a.push_back(order[x]);
          p.b.push_back(order[y]);
        }
      }
    }
    return p;
  }
};

Aabb random_box(Lcg& g, float world, float max_size) {
  const float w = g.unit() < 0.1f ? 0.0f : g.range(0, max_size), h = g.unit() < 0.1f ? 0.0f : g.range(0, max_size);
  const float x = g.range(-world * 0.2f, world * 1.2f), y = g.range(-world * 0.2f, world * 1.2f);
  return Aabb{x, y, x + w, y + h};
}

constexpr std::uint64_t kGoldenScatter = 0x32a1586075426a3aull;
constexpr std::uint64_t kGoldenChurn = 0xa877de3e9622cfb0ull;
constexpr std::uint64_t kGoldenStress = 0x90343d8556c3cfdeull;

std::uint64_t scene_digest(const SpatialGrid& g, Lcg& rng, float world) {
  const Pairs p = all_pairs(g);
  std::vector<std::uint32_t> pv;
  for (std::size_t i = 0; i < p.a.size(); ++i) {
    pv.push_back(p.a[i]);
    pv.push_back(p.b[i]);
  }
  std::vector<std::uint32_t> qv;
  for (int i = 0; i < 100; ++i) {
    const Aabb b = random_box(rng, world, world * 0.2f);
    const std::vector<std::uint32_t> r = q(g, b);
    qv.push_back(static_cast<std::uint32_t>(r.size()));
    qv.insert(qv.end(), r.begin(), r.end());
  }
  const std::uint64_t d0 = g.digest(), d1 = digest_u32(pv.data(), pv.size()), d2 = digest_u32(qv.data(), qv.size());
  const std::uint32_t all[8] = {static_cast<std::uint32_t>(d0), static_cast<std::uint32_t>(d0 >> 32),
                                static_cast<std::uint32_t>(d1), static_cast<std::uint32_t>(d1 >> 32),
                                static_cast<std::uint32_t>(d2), static_cast<std::uint32_t>(d2 >> 32),
                                static_cast<std::uint32_t>(p.a.size()), static_cast<std::uint32_t>(g.live_count())};
  return digest_u32(all, 8);
}

}  // namespace

TEST_CASE("grid_status_text has a distinct text per status", "[grid]") {
  const GridStatus all[] = {GridStatus::Ok,      GridStatus::BadCellSize, GridStatus::BadOrigin,   GridStatus::BadCells,
                            GridStatus::BadBox,  GridStatus::TooManyItems, GridStatus::UnknownItem};
  for (std::size_t i = 0; i < 7; ++i) {
    REQUIRE(std::strlen(grid_status_text(all[i])) > 0);
    for (std::size_t j = i + 1; j < 7; ++j) REQUIRE(std::strcmp(grid_status_text(all[i]), grid_status_text(all[j])) != 0);
  }
}

TEST_CASE("digests: FNV-1a 64 over little-endian 32-bit values", "[grid][digest]") {
  REQUIRE(digest_u32(nullptr, 0) == 0xcbf29ce484222325ull);
  const std::uint32_t zero = 0, two[2] = {1, 2};
  REQUIRE(digest_u32(&zero, 1) == 0x4d25767f9dce13f5ull);
  REQUIRE(digest_u32(two, 2) == 0xc9c28939c99668c6ull);
  SpatialGrid g;
  REQUIRE(g.init(0, 0, 1, 4, 4) == GridStatus::Ok);
  REQUIRE(g.digest() == 0x4d25767f9dce13f5ull);  // count 0
  std::uint32_t id = 0;
  REQUIRE(g.insert(Aabb{0, 0, 1, 1}, id) == GridStatus::Ok);
  REQUIRE(g.digest() == 0xfcea119d5f96b0d4ull);  // 1, id 0, box bits 0 0 0x3f800000 0x3f800000
  for (int i = 1; i < 6; ++i) REQUIRE(g.insert(Aabb{5, 5, 6, 6}, id) == GridStatus::Ok);
  for (std::uint32_t k = 1; k <= 4; ++k) REQUIRE(g.remove(k) == GridStatus::Ok);
  REQUIRE(g.update(5, Aabb{-2.0f, 0.5f, 3.0f, 0.5f}) == GridStatus::Ok);
  REQUIRE(g.digest() == 0xb6462fac1139fbb2ull);  // 2 live: ids 0 and 5, ascending
}

TEST_CASE("init: statuses and order", "[grid][init]") {
  SpatialGrid g;
  REQUIRE(g.init(0, 0, 0.0f, 4, 4) == GridStatus::BadCellSize);
  REQUIRE(g.init(0, 0, -1.0f, 4, 4) == GridStatus::BadCellSize);
  REQUIRE(g.init(0, 0, kNaN, 4, 4) == GridStatus::BadCellSize);
  REQUIRE(g.init(0, 0, kInf, 4, 4) == GridStatus::BadCellSize);
  REQUIRE(g.init(kNaN, 0, 1, 4, 4) == GridStatus::BadOrigin);
  REQUIRE(g.init(0, -kInf, 1, 4, 4) == GridStatus::BadOrigin);
  REQUIRE(g.init(1e9f + 64.0f, 0, 1, 4, 4) == GridStatus::BadOrigin);  // the next float above 1e9
  REQUIRE(g.init(0, -(1e9f + 64.0f), 1, 4, 4) == GridStatus::BadOrigin);
  REQUIRE(g.init(1e9f, -1e9f, 1, 4, 4) == GridStatus::Ok);
  REQUIRE(g.init(0, 0, 1, 0, 4) == GridStatus::BadCells);
  REQUIRE(g.init(0, 0, 1, 4, 0) == GridStatus::BadCells);
  REQUIRE(g.init(0, 0, 1, kMaxCellsPerAxis + 1, 4) == GridStatus::BadCells);
  REQUIRE(g.init(0, 0, 1, 4, kMaxCellsPerAxis + 1) == GridStatus::BadCells);
  REQUIRE(g.init(0, 0, 1, kMaxCellsPerAxis, kMaxCellsPerAxis) == GridStatus::Ok);
  REQUIRE(g.init(0, 0, from_bits(1u), 1, 1) == GridStatus::Ok);  // smallest positive cell size
  REQUIRE(g.init(0, 0, kBig, 2, 2) == GridStatus::Ok);
  // Check order: cell size, then origin, then cells.
  REQUIRE(g.init(kNaN, 0, 0.0f, 0, 0) == GridStatus::BadCellSize);
  REQUIRE(g.init(kNaN, 0, 1.0f, 0, 0) == GridStatus::BadOrigin);
  // A failed init keeps the previous state; a successful one clears it and restarts the ids.
  REQUIRE(g.init(0, 0, 1, 8, 8) == GridStatus::Ok);
  std::uint32_t id = 99;
  REQUIRE(g.insert(Aabb{1, 1, 2, 2}, id) == GridStatus::Ok);
  REQUIRE(id == 0);
  REQUIRE(g.init(0, 0, -1, 8, 8) == GridStatus::BadCellSize);
  REQUIRE(g.live_count() == 1);
  REQUIRE(g.init(0, 0, 1, 8, 8) == GridStatus::Ok);
  REQUIRE(g.live_count() == 0);
  REQUIRE(g.insert(Aabb{1, 1, 2, 2}, id) == GridStatus::Ok);
  REQUIRE(id == 0);
}

TEST_CASE("an uninitialized grid is empty and rejects inserts", "[grid][init]") {
  SpatialGrid g;
  std::uint32_t id = 77;
  REQUIRE(g.insert(Aabb{0, 0, 1, 1}, id) == GridStatus::BadCells);
  REQUIRE(id == 77);
  REQUIRE(g.remove(0) == GridStatus::UnknownItem);
  REQUIRE(g.update(0, Aabb{0, 0, 1, 1}) == GridStatus::UnknownItem);
  REQUIRE(g.live_count() == 0);
  std::uint32_t out[2];
  REQUIRE(g.query(Aabb{0, 0, 1, 1}, out, 2) == 0);
  REQUIRE(g.pairs(out, out, 2) == 0);
  REQUIRE(g.digest() == 0x4d25767f9dce13f5ull);
}

TEST_CASE("insert, remove, update: statuses and ids never reused", "[grid][items]") {
  SpatialGrid g;
  REQUIRE(g.init(0, 0, 10, 10, 10) == GridStatus::Ok);
  std::uint32_t id = 55;
  const Aabb bad[] = {{kNaN, 0, 1, 1}, {0, kNaN, 1, 1}, {0, 0, kInf, 1}, {0, 0, 1, -kInf}, {2, 0, 1, 1}, {0, 2, 1, 1}};
  for (const Aabb& b : bad) {
    REQUIRE(g.insert(b, id) == GridStatus::BadBox);
    REQUIRE(id == 55);
  }
  REQUIRE(g.live_count() == 0);
  REQUIRE(add(g, Aabb{0, 0, 1, 1}) == 0);
  REQUIRE(add(g, Aabb{2, 2, 3, 3}) == 1);
  REQUIRE(add(g, Aabb{4, 4, 4, 4}) == 2);  // a point box is valid
  REQUIRE(g.live_count() == 3);
  REQUIRE(g.remove(2) == GridStatus::Ok);
  REQUIRE(g.remove(2) == GridStatus::UnknownItem);  // twice
  REQUIRE(g.remove(1000) == GridStatus::UnknownItem);
  REQUIRE(g.live_count() == 2);
  REQUIRE(add(g, Aabb{4, 4, 4, 4}) == 3);  // id 2 is not reused
  REQUIRE(g.remove(3) == GridStatus::Ok);
  REQUIRE(g.remove(1) == GridStatus::Ok);
  REQUIRE(add(g, Aabb{0, 0, 1, 1}) == 4);
  REQUIRE(g.update(1, Aabb{0, 0, 1, 1}) == GridStatus::UnknownItem);  // removed
  REQUIRE(g.update(9, Aabb{0, 0, 1, 1}) == GridStatus::UnknownItem);
  REQUIRE(g.update(0, Aabb{kNaN, 0, 1, 1}) == GridStatus::BadBox);
  REQUIRE(g.update(9, Aabb{kNaN, 0, 1, 1}) == GridStatus::BadBox);  // BadBox is checked before the id
  REQUIRE(g.update(0, Aabb{3, 3, 2, 3}) == GridStatus::BadBox);
  REQUIRE(q(*&g, Aabb{0, 0, 1, 1}) == std::vector<std::uint32_t>({0, 4}));  // a failed update changed nothing
  REQUIRE(g.update(0, Aabb{50, 50, 60, 60}) == GridStatus::Ok);
  REQUIRE(q(g, Aabb{0, 0, 1, 1}) == std::vector<std::uint32_t>({4}));
  REQUIRE(q(g, Aabb{55, 55, 55, 55}) == std::vector<std::uint32_t>({0}));
  // Many removes and inserts (compaction) keep ids rising and results right.
  std::uint32_t last = 4;
  for (int i = 0; i < 5000; ++i) {
    const std::uint32_t n = add(g, Aabb{1, 1, 2, 2});
    REQUIRE(n == last + 1);
    last = n;
    REQUIRE(g.remove(n) == GridStatus::Ok);
  }
  REQUIRE(g.live_count() == 2);
  REQUIRE(q(g, Aabb{0, 0, 100, 100}) == std::vector<std::uint32_t>({0, 4}));
}

TEST_CASE("kMaxItems live items", "[grid][items]") {
  SpatialGrid* g = make(0, 0, 4, 64, 64);
  std::uint32_t id = 0;
  for (std::size_t i = 0; i < kMaxItems; ++i) {
    const float x = static_cast<float>(i % 256), y = static_cast<float>(i / 256);
    REQUIRE(g->insert(Aabb{x, y, x, y}, id) == GridStatus::Ok);
    REQUIRE(id == i);
  }
  REQUIRE(g->live_count() == kMaxItems);
  REQUIRE(g->insert(Aabb{0, 0, 1, 1}, id) == GridStatus::TooManyItems);
  REQUIRE(g->live_count() == kMaxItems);
  REQUIRE(g->remove(100) == GridStatus::Ok);
  REQUIRE(g->insert(Aabb{0, 0, 1, 1}, id) == GridStatus::Ok);
  REQUIRE(id == kMaxItems);  // ids are never reused, so they can pass kMaxItems
  REQUIRE(g->insert(Aabb{0, 0, 1, 1}, id) == GridStatus::TooManyItems);
  delete g;
}

TEST_CASE("query: points, zero-width, touching, whole grid, far outside", "[grid][query]") {
  SpatialGrid* g = make(0, 0, 10, 10, 10);  // world 0..100
  const std::uint32_t a = add(*g, Aabb{12, 12, 18, 18});
  const std::uint32_t b = add(*g, Aabb{20, 20, 30, 30});  // starts exactly on a cell border
  const std::uint32_t c = add(*g, Aabb{25, 5, 25, 95});   // zero-width vertical line
  const std::uint32_t d = add(*g, Aabb{50, 50, 50, 50});  // a point on a cell corner
  REQUIRE(q(*g, Aabb{15, 15, 15, 15}) == std::vector<std::uint32_t>({a}));  // point query inside
  REQUIRE(q(*g, Aabb{18, 18, 18, 18}) == std::vector<std::uint32_t>({a}));  // on the max corner: closed
  REQUIRE(q(*g, Aabb{19, 19, 19.9f, 19.9f}).empty());
  REQUIRE(q(*g, Aabb{18, 18, 20, 20}) == std::vector<std::uint32_t>({a, b}));  // touches both
  REQUIRE(q(*g, Aabb{20, 20, 20, 20}) == std::vector<std::uint32_t>({b}));     // exactly the cell border corner
  REQUIRE(q(*g, Aabb{25, 50, 25, 50}) == std::vector<std::uint32_t>({b == 0 ? 0u : c}));  // a point on the line
  REQUIRE(q(*g, Aabb{24.999f, 40, 24.999f, 41}).empty());
  REQUIRE(q(*g, Aabb{25, 30, 25, 30}) == std::vector<std::uint32_t>({b, c}));  // the line touches b's max y corner
  REQUIRE(q(*g, Aabb{50, 50, 50, 50}) == std::vector<std::uint32_t>({d}));
  REQUIRE(q(*g, Aabb{0, 0, 100, 100}) == std::vector<std::uint32_t>({a, b, c, d}));  // whole grid
  REQUIRE(q(*g, Aabb{-kBig, -kBig, kBig, kBig}) == std::vector<std::uint32_t>({a, b, c, d}));
  REQUIRE(q(*g, Aabb{-1e30f, 0, -1e29f, 100}).empty());  // far outside misses everything
  REQUIRE(q(*g, Aabb{200, 200, 300, 300}).empty());
  REQUIRE(q(*g, Aabb{-5, -5, -1, -1}).empty());
  // Invalid query boxes return 0 and write nothing.
  std::uint32_t out[4] = {7, 7, 7, 7};
  REQUIRE(g->query(Aabb{kNaN, 0, 1, 1}, out, 4) == 0);
  REQUIRE(g->query(Aabb{5, 0, 1, 1}, out, 4) == 0);
  REQUIRE(g->query(Aabb{0, 0, kInf, 1}, out, 4) == 0);
  REQUIRE(out[0] == 7);
  delete g;
}

TEST_CASE("query: truncation reports the total", "[grid][query]") {
  SpatialGrid* g = make(0, 0, 10, 8, 8);
  for (int i = 0; i < 10; ++i) add(*g, Aabb{1, 1, 2, 2});
  std::uint32_t out[16];
  std::memset(out, 0xff, sizeof out);
  REQUIRE(g->query(Aabb{0, 0, 5, 5}, out, 4) == 10);
  for (std::uint32_t i = 0; i < 4; ++i) REQUIRE(out[i] == i);
  REQUIRE(out[4] == 0xffffffffu);  // nothing written beyond the capacity
  REQUIRE(g->query(Aabb{0, 0, 5, 5}, out, 10) == 10);
  REQUIRE(out[9] == 9);
  REQUIRE(g->query(Aabb{0, 0, 5, 5}, nullptr, 0) == 10);
  REQUIRE(g->query(Aabb{0, 0, 5, 5}, nullptr, 5) == 10);  // a null pointer is never written
  REQUIRE(g->query(Aabb{0, 0, 5, 5}, out, 0) == 10);
  delete g;
}

TEST_CASE("boxes outside the grid land in the border cells and still match exactly", "[grid][outside]") {
  SpatialGrid* g = make(0, 0, 10, 4, 4);  // world 0..40
  const std::uint32_t left = add(*g, Aabb{-3e9f, 5, -1e9f, 6});
  const std::uint32_t left2 = add(*g, Aabb{-5e9f, 5, -4e9f, 6});  // same border cell, does not touch `left`
  const std::uint32_t right = add(*g, Aabb{1e9f, 5, kBig, 6});
  const std::uint32_t tiny = add(*g, Aabb{-1e-30f, -1e-30f, 1e-30f, 1e-30f});
  const std::uint32_t huge = add(*g, Aabb{-kBig, -kBig, kBig, kBig});
  REQUIRE(q(*g, Aabb{-2e9f, 0, -1.5e9f, 10}) == std::vector<std::uint32_t>({left, huge}));
  REQUIRE(q(*g, Aabb{-4.5e9f, 0, -4.1e9f, 10}) == std::vector<std::uint32_t>({left2, huge}));
  REQUIRE(q(*g, Aabb{-3.9e9f, 0, -3.2e9f, 10}) == std::vector<std::uint32_t>({huge}));  // between: only the huge box
  REQUIRE(q(*g, Aabb{3e38f, 0, kBig, 10}) == std::vector<std::uint32_t>({right, huge}));
  REQUIRE(q(*g, Aabb{0, 0, 0, 0}) == std::vector<std::uint32_t>({tiny, huge}));
  const Pairs p = all_pairs(*g);
  // left-left2 do not touch; huge touches all four others.
  REQUIRE(p.a == std::vector<std::uint32_t>({0, 1, 2, 3}));
  REQUIRE(p.b == std::vector<std::uint32_t>({4, 4, 4, 4}));
  (void)left;
  (void)left2;
  (void)right;
  delete g;
}

TEST_CASE("cell borders: edges exactly on a border occupy both cells", "[grid][borders]") {
  SpatialGrid* g = make(0, 0, 10, 10, 10);
  // Boxes ending or starting exactly at 10, 20, ... and a query that touches only from the far side.
  const std::uint32_t a = add(*g, Aabb{0, 0, 10, 10});
  const std::uint32_t b = add(*g, Aabb{10, 10, 20, 20});
  const std::uint32_t c = add(*g, Aabb{20, 0, 30, 10});
  REQUIRE(q(*g, Aabb{10, 10, 10, 10}) == std::vector<std::uint32_t>({a, b}));
  REQUIRE(q(*g, Aabb{10, 0, 10, 0}) == std::vector<std::uint32_t>({a}));
  REQUIRE(q(*g, Aabb{9.999f, 9.999f, 9.999f, 9.999f}) == std::vector<std::uint32_t>({a}));
  REQUIRE(q(*g, Aabb{10.001f, 10.001f, 10.001f, 10.001f}) == std::vector<std::uint32_t>({b}));
  REQUIRE(q(*g, Aabb{20, 10, 20, 10}) == std::vector<std::uint32_t>({b, c}));
  const Pairs p = all_pairs(*g);
  REQUIRE(p.a == std::vector<std::uint32_t>({0, 1}));
  REQUIRE(p.b == std::vector<std::uint32_t>({1, 2}));
  delete g;
}

TEST_CASE("pairs: sorted, once each, against a brute force reference", "[grid][pairs]") {
  struct Cfg {
    float ox, oy, cs;
    std::uint32_t nx, ny;
    float world, size;
    int items;
  };
  const Cfg cfgs[] = {
      {0, 0, 10, 10, 10, 100, 12, 150},    {-50, 20, 3.5f, 40, 25, 100, 8, 200},  {0, 0, 1, 1, 1, 100, 30, 80},
      {0, 0, 100, 3, 3, 300, 400, 60},     {1000, -1000, 0.25f, 64, 64, 16, 3, 250}, {0, 0, 7, 4096, 4096, 50, 4, 120},
  };
  std::uint32_t seed = 1;
  for (const Cfg& c : cfgs) {
    Lcg rng{seed++ * 7919u};
    SpatialGrid* g = make(c.ox, c.oy, c.cs, c.nx, c.ny);
    Mirror m;
    for (int i = 0; i < c.items; ++i) {
      const Aabb b = random_box(rng, c.world, c.size);
      const Aabb shifted = {b.min_x + c.ox, b.min_y + c.oy, b.max_x + c.ox, b.max_y + c.oy};
      m.put(add(*g, shifted), shifted);
    }
    for (int round = 0; round < 3; ++round) {
      const Pairs got = all_pairs(*g), want = m.pairs();
      REQUIRE(got.a == want.a);
      REQUIRE(got.b == want.b);
      for (int i = 1; i < static_cast<int>(got.a.size()); ++i) {
        REQUIRE((got.a[i - 1] < got.a[i] || (got.a[i - 1] == got.a[i] && got.b[i - 1] < got.b[i])));  // sorted, no repeats
      }
      for (std::size_t i = 0; i < got.a.size(); ++i) REQUIRE(got.a[i] < got.b[i]);
      for (int i = 0; i < 40; ++i) {
        Aabb b = random_box(rng, c.world, c.size * 2);
        b = Aabb{b.min_x + c.ox, b.min_y + c.oy, b.max_x + c.ox, b.max_y + c.oy};
        REQUIRE(q(*g, b) == m.query(b));
      }
      // Churn: remove a third, update a third, insert some more.
      for (int i = 0; i < c.items / 3; ++i) {
        const std::uint32_t victim = m.id[rng.next() % m.id.size()];
        REQUIRE(g->remove(victim) == GridStatus::Ok);
        m.drop(victim);
      }
      for (int i = 0; i < c.items / 3; ++i) {
        const std::uint32_t target = m.id[rng.next() % m.id.size()];
        Aabb b = random_box(rng, c.world, c.size);
        b = Aabb{b.min_x + c.ox, b.min_y + c.oy, b.max_x + c.ox, b.max_y + c.oy};
        REQUIRE(g->update(target, b) == GridStatus::Ok);
        m.box[m.at(target)] = b;
      }
      for (int i = 0; i < c.items / 4; ++i) {
        Aabb b = random_box(rng, c.world, c.size);
        b = Aabb{b.min_x + c.ox, b.min_y + c.oy, b.max_x + c.ox, b.max_y + c.oy};
        m.put(add(*g, b), b);
      }
      REQUIRE(g->live_count() == m.id.size());
    }
    delete g;
  }
}

TEST_CASE("pairs: truncation reports the total and a sorted prefix", "[grid][pairs]") {
  SpatialGrid* g = make(0, 0, 10, 4, 4);
  for (int i = 0; i < 6; ++i) add(*g, Aabb{1, 1, 2, 2});  // all overlap: 15 pairs
  std::uint32_t a[32], b[32];
  std::memset(a, 0xff, sizeof a);
  std::memset(b, 0xff, sizeof b);
  REQUIRE(g->pairs(a, b, 4) == 15);
  const std::uint32_t ea[4] = {0, 0, 0, 0}, eb[4] = {1, 2, 3, 4};
  for (int i = 0; i < 4; ++i) {
    REQUIRE(a[i] == ea[i]);
    REQUIRE(b[i] == eb[i]);
  }
  REQUIRE(a[4] == 0xffffffffu);
  REQUIRE(g->pairs(nullptr, nullptr, 0) == 15);
  REQUIRE(g->pairs(a, nullptr, 8) == 15);
  REQUIRE(g->pairs(a, b, 0) == 15);
  REQUIRE(g->pairs(a, b, 32) == 15);
  REQUIRE(a[14] == 4);
  REQUIRE(b[14] == 5);
  // Two items sharing many cells are reported once.
  SpatialGrid* h = make(0, 0, 1, 16, 16);
  add(*h, Aabb{0, 0, 16, 16});
  add(*h, Aabb{0, 0, 16, 16});
  REQUIRE(h->pairs(a, b, 32) == 1);
  delete h;
  delete g;
}

TEST_CASE("golden scene digests", "[grid][digest]") {
  // Scatter: small boxes over a grid.
  Lcg r1{101};
  SpatialGrid* g1 = make(0, 0, 8, 32, 32);
  for (int i = 0; i < 600; ++i) add(*g1, random_box(r1, 256, 12));
  const std::uint64_t d1 = scene_digest(*g1, r1, 256);
  // Churn: inserts, removes and updates, with a large box.
  Lcg r2{202};
  SpatialGrid* g2 = make(-100, -100, 5, 50, 50);
  std::vector<std::uint32_t> ids;
  for (int i = 0; i < 400; ++i) ids.push_back(add(*g2, random_box(r2, 200, 30)));
  add(*g2, Aabb{-100, -100, 150, 150});
  for (int i = 0; i < 150; ++i) {
    const std::uint32_t id = ids[r2.next() % ids.size()];
    if (r2.next() % 2) {
      g2->remove(id);
    } else {
      g2->update(id, random_box(r2, 200, 30));
    }
  }
  const std::uint64_t d2 = scene_digest(*g2, r2, 200);
  // Stress: border cells, tiny and huge boxes, far outside.
  Lcg r3{303};
  SpatialGrid* g3 = make(0, 0, 4, 8, 8);
  for (int i = 0; i < 150; ++i) add(*g3, random_box(r3, 32, 6));
  add(*g3, Aabb{-1e10f, 0, -1e9f, 32});
  add(*g3, Aabb{1e9f, 0, kBig, 32});
  add(*g3, Aabb{-kBig, -kBig, kBig, kBig});
  add(*g3, Aabb{4, 4, 4, 4});
  add(*g3, Aabb{8, 8, 8, 8});
  const std::uint64_t d3 = scene_digest(*g3, r3, 32);
  INFO("scatter " << std::hex << d1 << " churn " << d2 << " stress " << d3);
  REQUIRE(d1 == kGoldenScatter);
  REQUIRE(d2 == kGoldenChurn);
  REQUIRE(d3 == kGoldenStress);
  delete g1;
  delete g2;
  delete g3;
}

TEST_CASE("performance: 65536 items", "[grid][perf]") {
  const auto t0 = std::chrono::steady_clock::now();
  SpatialGrid* g = make(0, 0, 8, 256, 256);  // world 2048 x 2048
  Lcg rng{5};
  for (std::size_t i = 0; i < kMaxItems; ++i) {
    const float x = rng.range(0, 2040), y = rng.range(0, 2040);
    add(*g, Aabb{x, y, x + rng.range(0, 6), y + rng.range(0, 6)});
  }
  REQUIRE(g->live_count() == kMaxItems);
  std::uint64_t acc = 0;
  for (int i = 0; i < 2000; ++i) {
    const float x = rng.range(0, 2000), y = rng.range(0, 2000);
    acc += g->query(Aabb{x, y, x + 40, y + 40}, nullptr, 0);
  }
  for (int i = 0; i < 20000; ++i) {
    const std::uint32_t id = rng.next() % kMaxItems;
    const float x = rng.range(0, 2040), y = rng.range(0, 2040);
    REQUIRE(g->update(id, Aabb{x, y, x + 3, y + 3}) == GridStatus::Ok);
  }
  const std::size_t n = g->pairs(nullptr, nullptr, 0);
  REQUIRE(n > 0);
  acc += n + g->digest();
  REQUIRE(acc != 0);
  delete g;
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  INFO("seconds " << secs);
  REQUIRE(secs < 10.0);
}

TEST_CASE("exact border, border-cell and far-outside boxes match the brute force reference", "[grid][pairs][borders]") {
  // Coordinates drawn from values that sit exactly on cell borders, just beside them, outside the grid and extreme.
  const float vals[] = {-kBig, -1e10f, -1e9f, -50.0f, -10.0f, -1e-30f, 0.0f, 1e-30f, 9.999999f, 10.0f, 10.000001f, 20.0f,
                        29.999998f, 30.0f, 40.0f, 49.999996f, 50.0f, 50.000004f, 60.0f, 1e9f, 1e10f, kBig};
  const std::uint32_t n_vals = sizeof vals / sizeof vals[0];
  Lcg rng{9001};
  SpatialGrid* g = make(0, 0, 10, 5, 5);  // world 0..50
  Mirror m;
  auto pick = [&](float& lo, float& hi) {
    float a = vals[rng.next() % n_vals], b = vals[rng.next() % n_vals];
    if (a > b) std::swap(a, b);
    lo = a;
    hi = b;
  };
  for (int i = 0; i < 400; ++i) {
    Aabb b;
    pick(b.min_x, b.max_x);
    pick(b.min_y, b.max_y);
    m.put(add(*g, b), b);
  }
  for (int round = 0; round < 3; ++round) {
    const Pairs got = all_pairs(*g), want = m.pairs();
    REQUIRE(got.a == want.a);
    REQUIRE(got.b == want.b);
    for (int i = 0; i < 300; ++i) {
      Aabb b;
      pick(b.min_x, b.max_x);
      pick(b.min_y, b.max_y);
      REQUIRE(q(*g, b) == m.query(b));
    }
    for (int i = 0; i < 100; ++i) {
      const std::uint32_t id = m.id[rng.next() % m.id.size()];
      Aabb b;
      pick(b.min_x, b.max_x);
      pick(b.min_y, b.max_y);
      REQUIRE(g->update(id, b) == GridStatus::Ok);
      m.box[m.at(id)] = b;
    }
  }
  delete g;
  // The same with a grid whose origin and cell size are not round, and a tiny one-cell grid.
  const float cell = 0.1f;
  SpatialGrid* h = make(-0.3f, 0.7f, cell, 7, 9);
  Mirror mh;
  for (int i = 0; i < 300; ++i) {
    const float x = -0.3f + static_cast<float>(rng.next() % 9) * cell, y = 0.7f + static_cast<float>(rng.next() % 11) * cell;
    const float w = static_cast<float>(rng.next() % 3) * cell, hgt = static_cast<float>(rng.next() % 3) * cell;
    const Aabb b = {x, y, x + w, y + hgt};  // edges land on (or one rounding from) cell borders
    mh.put(add(*h, b), b);
  }
  const Pairs got = all_pairs(*h), want = mh.pairs();
  REQUIRE(got.a == want.a);
  REQUIRE(got.b == want.b);
  for (int i = 0; i < 200; ++i) {
    const float x = -0.5f + static_cast<float>(rng.next() % 12) * cell, y = 0.5f + static_cast<float>(rng.next() % 14) * cell;
    const Aabb b = {x, y, x + cell * static_cast<float>(rng.next() % 3), y};
    REQUIRE(q(*h, b) == mh.query(b));
  }
  delete h;
}
