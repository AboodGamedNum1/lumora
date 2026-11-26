// Tests for the integer tessellation module (core/tess).
//
// Expected values in the golden tables were produced by an independent Python reference of the rules in docs/tess.md (big-integer
// shoelace, brute-force segment tests, a naive uncached ear-clipping loop that follows the header rule literally, its own FNV-1a)
// and pasted here as constants. The relation tests compare the implementation with a naive reference written in this file.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "tess.h"

using namespace lumora::tess;

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Pts = std::vector<Point>;

constexpr std::int32_t kK = kMaxCoord;
constexpr u32 kSentinel = 0xA5A5A5A5u;

struct Lcg {
  u64 s;
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  std::int32_t rng(std::int32_t lo, std::int32_t hi) {  // inclusive
    return static_cast<std::int32_t>(lo + static_cast<i64>(next() % static_cast<u32>(hi - lo + 1)));
  }
};

i64 orient(const Point& a, const Point& b, const Point& c) {
  return (static_cast<i64>(b.x) - a.x) * (static_cast<i64>(c.y) - a.y) - (static_cast<i64>(b.y) - a.y) * (static_cast<i64>(c.x) - a.x);
}
int sg(i64 v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

// Shoelace in int64: exact for every polygon built in these tests (all are simple, or have small coordinates).
i64 ref_area2(const Pts& p) {
  i64 a = 0;
  for (std::size_t i = 0; i < p.size(); ++i) {
    const Point& q = p[(i + 1) % p.size()];
    a += static_cast<i64>(p[i].x) * q.y - static_cast<i64>(q.x) * p[i].y;
  }
  return a;
}

bool on_box(const Point& a, const Point& b, const Point& p) {
  return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x) && std::min(a.y, b.y) <= p.y && p.y <= std::max(a.y, b.y);
}

// Reference segment test, formulated with the parametric solution instead of orientation signs (small coordinates only).
bool ref_meet(const Point& p1, const Point& p2, const Point& p3, const Point& p4) {
  const i64 rx = static_cast<i64>(p2.x) - p1.x, ry = static_cast<i64>(p2.y) - p1.y;
  const i64 sx = static_cast<i64>(p4.x) - p3.x, sy = static_cast<i64>(p4.y) - p3.y;
  const i64 qx = static_cast<i64>(p3.x) - p1.x, qy = static_cast<i64>(p3.y) - p1.y;
  const i64 den = rx * sy - ry * sx;
  if (den != 0) {
    i64 tn = qx * sy - qy * sx, un = qx * ry - qy * rx;
    i64 d = den;
    if (d < 0) {
      d = -d;
      tn = -tn;
      un = -un;
    }
    return tn >= 0 && tn <= d && un >= 0 && un <= d;
  }
  if (qx * ry - qy * rx != 0) return false;  // parallel, not collinear
  const i64 rr = rx * rx + ry * ry;
  if (rr == 0) return on_box(p3, p4, p1) && (sx * (-qy) - sy * (-qx)) == 0;  // p1 == p2 degenerate: not used by the tests
  const i64 t0 = qx * rx + qy * ry, t1 = t0 + sx * rx + sy * ry;
  return std::max(t0, t1) >= 0 && std::min(t0, t1) <= rr;
}

bool ref_simple(const Pts& p) {
  const std::size_t n = p.size();
  if (ref_area2(p) == 0) return false;
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i + 1; j < n; ++j)
      if (p[i].x == p[j].x && p[i].y == p[j].y) return false;
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i + 1; j < n; ++j) {
      const bool adj_next = j == i + 1, adj_wrap = (i == 0 && j == n - 1);
      const Point &a = p[i], &b = p[(i + 1) % n], &c = p[j], &d = p[(j + 1) % n];
      if (adj_next || adj_wrap) {
        // Adjacent edges share only the common vertex: forbid a collinear fold.
        const Point &x = adj_next ? a : c, &y = adj_next ? b : d, &z = adj_next ? d : b;  // x - y - z consecutive
        if (orient(x, y, z) == 0) {
          const i64 dot = (static_cast<i64>(x.x) - y.x) * (static_cast<i64>(z.x) - y.x) + (static_cast<i64>(x.y) - y.y) * (static_cast<i64>(z.y) - y.y);
          if (dot > 0) return false;
        }
        continue;
      }
      if (ref_meet(a, b, c, d)) return false;
    }
  return true;
}

// Naive uncached ear clipping that follows the header rule literally (written for clarity, not speed).
struct Naive {
  std::vector<u32> tris;  // index triples
  u32 fallbacks = 0;
  u32 drops = 0;
};
Naive naive_tri(const Pts& p) {
  Naive r;
  const int s = sg(ref_area2(p));
  std::vector<u32> ring(p.size());
  for (u32 i = 0; i < ring.size(); ++i) ring[i] = i;
  const auto closed_in = [&](u32 a, u32 v, u32 b, u32 q) {
    return sg(orient(p[a], p[v], p[q])) * s >= 0 && sg(orient(p[v], p[b], p[q])) * s >= 0 && sg(orient(p[b], p[a], p[q])) * s >= 0;
  };
  const auto open_in = [&](u32 a, u32 v, u32 b, u32 q) {
    return sg(orient(p[a], p[v], p[q])) * s > 0 && sg(orient(p[v], p[b], p[q])) * s > 0 && sg(orient(p[b], p[a], p[q])) * s > 0;
  };
  while (ring.size() >= 3) {
    std::sort(ring.begin(), ring.end());  // the ring order is the index order
    const std::vector<u32> start = ring;
    bool clipped = false, dropped = false;
    for (u32 v : start) {
      const auto it = std::find(ring.begin(), ring.end(), v);
      if (it == ring.end()) continue;
      const std::size_t k = static_cast<std::size_t>(it - ring.begin()), m = ring.size();
      const u32 a = ring[(k + m - 1) % m], b = ring[(k + 1) % m];
      const int t = sg(orient(p[a], p[v], p[b]));
      if (t == 0) {
        ring.erase(it);
        dropped = true;
        ++r.drops;
        continue;
      }
      if (t == s) {
        bool free_of = true;
        for (u32 u : ring)
          if (u != a && u != v && u != b && closed_in(a, v, b, u)) free_of = false;
        if (free_of) {
          if (s > 0) r.tris.insert(r.tris.end(), {a, v, b});
          else r.tris.insert(r.tris.end(), {b, v, a});
          ring.erase(it);
          clipped = true;
          break;
        }
      }
    }
    if (clipped || dropped) continue;
    for (u32 v : ring) {
      const auto it = std::find(ring.begin(), ring.end(), v);
      const std::size_t k = static_cast<std::size_t>(it - ring.begin()), m = ring.size();
      const u32 a = ring[(k + m - 1) % m], b = ring[(k + 1) % m];
      if (sg(orient(p[a], p[v], p[b])) != s) continue;
      bool none = true;
      for (u32 u : ring)
        if (u != a && u != v && u != b && open_in(a, v, b, u)) none = false;
      if (!none) continue;
      if (s > 0) r.tris.insert(r.tris.end(), {a, v, b});
      else r.tris.insert(r.tris.end(), {b, v, a});
      ring.erase(it);
      ++r.fallbacks;
      break;
    }
  }
  return r;
}

// Shapes (mirrored by the Python reference).
Pts rect_collinear(std::int32_t w, std::int32_t h) {
  Pts p;
  for (std::int32_t x = 0; x < w; ++x) p.push_back({x, 0});
  for (std::int32_t y = 0; y < h; ++y) p.push_back({w, y});
  for (std::int32_t x = w; x > 0; --x) p.push_back({x, h});
  for (std::int32_t y = h; y > 0; --y) p.push_back({0, y});
  return p;
}
Pts comb(std::int32_t m) {
  Pts p = {{0, 0}, {2 * m - 1, 0}};
  for (std::int32_t t = m - 1; t >= 0; --t) {
    p.push_back({2 * t + 1, 10});
    p.push_back({2 * t, 10});
    if (t > 0) {
      p.push_back({2 * t, 5});
      p.push_back({2 * t - 1, 5});
    }
  }
  return p;
}
Pts spiral(std::int32_t k) {
  const std::int32_t dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
  std::vector<Point> c = {{0, 0}};
  for (std::int32_t i = 0; i < k; ++i) {
    const std::int32_t l = 4 * (i / 2 + 1);
    c.push_back({c.back().x + dx[i % 4] * l, c.back().y + dy[i % 4] * l});
  }
  Pts left, right;
  for (std::int32_t j = 0; j <= k; ++j) {
    std::int32_t sx = 0, sy = 0;
    if (j > 0) {
      sx += -dy[(j - 1) % 4];
      sy += dx[(j - 1) % 4];
    }
    if (j < k) {
      sx += -dy[j % 4];
      sy += dx[j % 4];
    }
    left.push_back({c[j].x + sx, c[j].y + sy});
    right.push_back({c[j].x - sx, c[j].y - sy});
  }
  std::reverse(right.begin(), right.end());
  left.insert(left.end(), right.begin(), right.end());
  return left;
}
std::int32_t tdiv3(std::int32_t v) { return v / 3; }  // truncation toward zero (C++11)
Pts star(std::int32_t m) {
  const std::int32_t R = 96, step = (8 * R) / (2 * m);
  Pts p;
  for (std::int32_t i = 0; i < 2 * m; ++i) {
    const std::int32_t t = step * i, side = t / (2 * R), off = t % (2 * R);
    std::int32_t x = 0, y = 0;
    switch (side) {
      case 0: x = -R + off; y = -R; break;
      case 1: x = R; y = -R + off; break;
      case 2: x = R - off; y = R; break;
      default: x = -R; y = R - off; break;
    }
    if (i % 2) {
      x = tdiv3(x);
      y = tdiv3(y);
    }
    p.push_back({x, y});
  }
  return p;
}
Pts parabola(std::int32_t m) {
  Pts p;
  for (std::int32_t i = 0; i < m; ++i) p.push_back({i, i * i});
  return p;
}

struct ShapeDef {
  const char* name;
  Pts (*make)();
};
const ShapeDef kShapes[] = {
    {"tri", [] { return Pts{{0, 0}, {10, 0}, {0, 10}}; }},
    {"tri_ext", [] { return Pts{{-kK, -kK}, {kK, -kK}, {0, kK}}; }},
    {"sliver", [] { return Pts{{-kK, -kK}, {kK, kK - 1}, {kK - 1, kK}}; }},
    {"square_ext", [] { return Pts{{-kK, -kK}, {kK, -kK}, {kK, kK}, {-kK, kK}}; }},
    {"L_ext", [] { return Pts{{-kK, -kK}, {kK, -kK}, {kK, 0}, {0, 0}, {0, kK}, {-kK, kK}}; }},
    {"parabola", [] { return parabola(100); }},
    {"star", [] { return star(32); }},
    {"rect_col", [] { return rect_collinear(100, 60); }},
    {"comb", [] { return comb(96); }},
    {"spiral", [] { return spiral(160); }},
};

// Four variants: as is, reversed, rotated by n / 3, reversed then rotated by n / 2.
Pts variant(const Pts& p, int v) {
  const std::size_t n = p.size();
  Pts r = p;
  if (v == 1 || v == 3) std::reverse(r.begin(), r.end());
  const std::size_t rot = v == 2 ? n / 3 : (v == 3 ? n / 2 : 0);
  std::rotate(r.begin(), r.begin() + static_cast<std::ptrdiff_t>(rot), r.end());
  return r;
}

struct Tri {
  Status st;
  std::vector<u32> out;  // 3 * tri_count words
  u32 count = 0;
};
Tri run_tri(const Pts& p) {
  const u32 n = static_cast<u32>(p.size());
  std::vector<u32> out(3 * (n - 2) + 3, kSentinel), scratch(scratch_size(n), kSentinel);
  Tri t;
  u32 cnt = kSentinel;
  t.st = triangulate(p.data(), n, out.data(), n - 2, scratch.data(), scratch_size(n), &cnt);
  if (t.st == Status::Ok) {
    t.count = cnt;
    t.out.assign(out.begin(), out.begin() + 3 * cnt);
    for (std::size_t i = 3 * cnt; i < out.size(); ++i) REQUIRE(out[i] == kSentinel);  // nothing written past the triangles
  }
  return t;
}

// The tiling rules of the header: valid indices, every triangle counter-clockwise with non-zero area, doubled areas sum to |area2|.
bool tiling_ok(const Pts& p, const Tri& t) {
  const u32 n = static_cast<u32>(p.size());
  i64 sum = 0;
  for (u32 k = 0; k < t.count; ++k) {
    const u32 a = t.out[3 * k], b = t.out[3 * k + 1], c = t.out[3 * k + 2];
    if (a >= n || b >= n || c >= n || a == b || b == c || a == c) return false;
    const i64 ar = orient(p[a], p[b], p[c]);
    if (ar <= 0) return false;
    sum += ar;
  }
  const i64 total = ref_area2(p);
  return sum == (total < 0 ? -total : total);
}

std::vector<u32> as_words(const Tri& t) {
  std::vector<u32> w;
  w.push_back(t.count);
  w.insert(w.end(), t.out.begin(), t.out.end());
  return w;
}

struct ShapeGolden {
  const char* name;
  int variant;
  u32 n, tris, drops;
  u64 digest;
  i64 area2;
};
constexpr ShapeGolden kShapeGolden[] = {
  {"tri", 0, 3, 1, 0, 0xc3a3bd3a59bc7a17ull, 100ll},
  {"tri", 1, 3, 1, 0, 0x6e5ae5a25523fdf7ull, -100ll},
  {"tri", 2, 3, 1, 0, 0xc3a3bd3a59bc7a17ull, 100ll},
  {"tri", 3, 3, 1, 0, 0x6e5ae5a25523fdf7ull, -100ll},
  {"tri_ext", 0, 3, 1, 0, 0xc3a3bd3a59bc7a17ull, 288230376151711744ll},
  {"tri_ext", 1, 3, 1, 0, 0x6e5ae5a25523fdf7ull, -288230376151711744ll},
  {"tri_ext", 2, 3, 1, 0, 0xc3a3bd3a59bc7a17ull, 288230376151711744ll},
  {"tri_ext", 3, 3, 1, 0, 0x6e5ae5a25523fdf7ull, -288230376151711744ll},
  {"sliver", 0, 3, 1, 0, 0xc3a3bd3a59bc7a17ull, 1073741823ll},
  {"sliver", 1, 3, 1, 0, 0x6e5ae5a25523fdf7ull, -1073741823ll},
  {"sliver", 2, 3, 1, 0, 0xc3a3bd3a59bc7a17ull, 1073741823ll},
  {"sliver", 3, 3, 1, 0, 0x6e5ae5a25523fdf7ull, -1073741823ll},
  {"square_ext", 0, 4, 2, 0, 0x74abd744285dde85ull, 576460752303423488ll},
  {"square_ext", 1, 4, 2, 0, 0x7b479fc2182ce3c5ull, -576460752303423488ll},
  {"square_ext", 2, 4, 2, 0, 0x74abd744285dde85ull, 576460752303423488ll},
  {"square_ext", 3, 4, 2, 0, 0x7b479fc2182ce3c5ull, -576460752303423488ll},
  {"L_ext", 0, 6, 4, 0, 0xeeba2b673ffa9467ull, 432345564227567616ll},
  {"L_ext", 1, 6, 4, 0, 0x16699e531138d715ull, -432345564227567616ll},
  {"L_ext", 2, 6, 3, 1, 0x2214e560825e8690ull, 432345564227567616ll},
  {"L_ext", 3, 6, 4, 0, 0x16699e531138d715ull, -432345564227567616ll},
  {"parabola", 0, 100, 98, 0, 0x1ba8188157033c85ull, 323400ll},
  {"parabola", 1, 100, 98, 0, 0x4bd2e34295f00985ull, -323400ll},
  {"parabola", 2, 100, 98, 0, 0x1ba8188157033c85ull, 323400ll},
  {"parabola", 3, 100, 98, 0, 0x4bd2e34295f00985ull, -323400ll},
  {"star", 0, 64, 62, 0, 0xb70a5f408aaec9e9ull, 24576ll},
  {"star", 1, 64, 50, 12, 0x09caf32726a86549ull, -24576ll},
  {"star", 2, 64, 54, 8, 0x167432c852e7446dull, 24576ll},
  {"star", 3, 64, 50, 12, 0x09caf32726a86549ull, -24576ll},
  {"rect_col", 0, 320, 318, 0, 0xd313f5936b62467bull, 12000ll},
  {"rect_col", 1, 320, 259, 59, 0xf9bf362a1520db7full, -12000ll},
  {"rect_col", 2, 320, 264, 54, 0x887d5d04ac44c9bdull, 12000ll},
  {"rect_col", 3, 320, 259, 59, 0xf9bf362a1520db7full, -12000ll},
  {"comb", 0, 384, 382, 0, 0x0ad65d9adec21ab3ull, 2870ll},
  {"comb", 1, 384, 382, 0, 0x49f4dcedf5277defull, -2870ll},
  {"comb", 2, 384, 256, 126, 0xe6bc2cd097307652ull, 2870ll},
  {"comb", 3, 384, 289, 93, 0xfba7291e01ff5daeull, -2870ll},
  {"spiral", 0, 322, 320, 0, 0x3b626a4a775293eeull, -103680ll},
  {"spiral", 1, 322, 320, 0, 0x62c046d619723432ull, 103680ll},
  {"spiral", 2, 322, 320, 0, 0x3fbe1dd37fb4c8aeull, -103680ll},
  {"spiral", 3, 322, 320, 0, 0x62c046d619723432ull, 103680ll},
};

struct CorpusResult {
  u32 simple = 0, tris = 0, drops = 0;
  u64 digest = 0;
};

// Records one polygon in the corpus digest the same way as the Python reference. Also cross-checks every answer against the
// references in this file.
void corpus_item(const Pts& p, CorpusResult& r, std::vector<u32>& words, bool with_len) {
  const u32 n = static_cast<u32>(p.size());
  bool simple = false;
  REQUIRE(is_simple(p.data(), n, &simple) == Status::Ok);
  REQUIRE(simple == ref_simple(p));
  i64 a = 0;
  REQUIRE(area2(p.data(), n, &a) == Status::Ok);
  REQUIRE(a == ref_area2(p));
  const Tri t = run_tri(p);
  if (with_len) words.push_back(n);
  else words.push_back(simple ? 1u : 0u);
  words.push_back(static_cast<u32>(static_cast<u64>(a) & 0xFFFFFFFFu));
  words.push_back(static_cast<u32>((static_cast<u64>(a) >> 32) & 0xFFFFFFFFu));
  if (!simple) {
    REQUIRE(t.st == Status::BadPolygon);
    return;
  }
  REQUIRE(t.st == Status::Ok);
  const Naive ref = naive_tri(p);
  REQUIRE(ref.fallbacks == 0);
  REQUIRE(t.out == ref.tris);
  REQUIRE(t.count == n - 2 - ref.drops);
  REQUIRE(tiling_ok(p, t));
  ++r.simple;
  r.tris += t.count;
  r.drops += ref.drops;
  words.push_back(t.count);
  words.insert(words.end(), t.out.begin(), t.out.end());
}

CorpusResult run_grid_corpus(u64 seed, int count, int nlo, int nhi, int rlo, int rhi) {
  Lcg r{seed};
  CorpusResult res;
  std::vector<u32> words;
  for (int i = 0; i < count; ++i) {
    const std::int32_t n = r.rng(nlo, nhi), R = r.rng(rlo, rhi);
    Pts p;
    for (std::int32_t k = 0; k < n; ++k) {
      const std::int32_t x = r.rng(-R, R), y = r.rng(-R, R);
      p.push_back({x, y});
    }
    corpus_item(p, res, words, false);
  }
  res.digest = digest_indices(words.data(), words.size());
  return res;
}

Pts grow(Lcg& r, std::int32_t R, std::size_t target) {
  Pts p = (r.next() & 1u) ? Pts{{-R, -R}, {R, -R}, {R, R}} : Pts{{0, 0}, {R, 0}, {0, R}};
  int fails = 0;
  while (p.size() < target && fails < 200) {
    const std::size_t i = static_cast<std::size_t>(r.rng(0, static_cast<std::int32_t>(p.size()) - 1));
    const std::int32_t qx = r.rng(-R, R), qy = r.rng(-R, R);
    Pts c(p.begin(), p.begin() + static_cast<std::ptrdiff_t>(i) + 1);
    c.push_back({qx, qy});
    c.insert(c.end(), p.begin() + static_cast<std::ptrdiff_t>(i) + 1, p.end());
    bool simple = false;
    REQUIRE(is_simple(c.data(), static_cast<u32>(c.size()), &simple) == Status::Ok);
    if (simple) p = c;
    else ++fails;
  }
  return p;
}

}  // namespace

TEST_CASE("status_text names", "[tess]") {
  REQUIRE(std::string_view(status_text(Status::Ok)) == "ok");
  REQUIRE(std::string_view(status_text(Status::NullBuffer)) == "null buffer");
  REQUIRE(std::string_view(status_text(Status::BadCount)) == "bad count");
  REQUIRE(std::string_view(status_text(Status::BadPoint)) == "bad point");
  REQUIRE(std::string_view(status_text(Status::BadPolygon)) == "bad polygon");
  REQUIRE(std::string_view(status_text(Status::BufferTooSmall)) == "buffer too small");
  REQUIRE(std::string_view(status_text(Status::Overlap)) == "overlap");
  REQUIRE(std::string_view(status_text(static_cast<Status>(99))) == "unknown");
}

TEST_CASE("digest_indices is FNV-1a 64 over little-endian words", "[tess]") {
  REQUIRE(digest_indices(nullptr, 3) == 0xcbf29ce484222325ull);
  const u32 v[2] = {1, 0x01020304u};
  REQUIRE(digest_indices(v, 0) == 0xcbf29ce484222325ull);
  u64 h = 0xcbf29ce484222325ull;
  const std::uint8_t bytes[8] = {1, 0, 0, 0, 4, 3, 2, 1};
  for (std::uint8_t c : bytes) {
    h ^= c;
    h *= 0x100000001b3ull;
  }
  REQUIRE(digest_indices(v, 2) == h);
}

TEST_CASE("scratch_size is 3 * count for valid counts", "[tess]") {
  REQUIRE(scratch_size(0) == 0);
  REQUIRE(scratch_size(2) == 0);
  REQUIRE(scratch_size(3) == 9);
  REQUIRE(scratch_size(4096) == 3u * 4096u);
  REQUIRE(scratch_size(4097) == 0);
}

TEST_CASE("area2: exact values, signs, argument checks and saturation", "[tess]") {
  const Pts sq = {{0, 0}, {4, 0}, {4, 3}, {0, 3}};
  i64 a = 77;
  REQUIRE(area2(sq.data(), 4, &a) == Status::Ok);
  REQUIRE(a == 24);
  const Pts rev(sq.rbegin(), sq.rend());
  REQUIRE(area2(rev.data(), 4, &a) == Status::Ok);
  REQUIRE(a == -24);
  // Extreme square: 2 * (2^29)^2 = 2^59.
  const Pts ext = {{-kK, -kK}, {kK, -kK}, {kK, kK}, {-kK, kK}};
  REQUIRE(area2(ext.data(), 4, &a) == Status::Ok);
  REQUIRE(a == (static_cast<i64>(1) << 59));
  // Errors: result untouched.
  a = 77;
  REQUIRE(area2(nullptr, 4, &a) == Status::NullBuffer);
  REQUIRE(area2(sq.data(), 4, nullptr) == Status::NullBuffer);
  REQUIRE(area2(sq.data(), 2, &a) == Status::BadCount);
  REQUIRE(area2(sq.data(), 4097, &a) == Status::BadCount);
  Pts bad = sq;
  bad[2].x = kK + 1;
  REQUIRE(area2(bad.data(), 4, &a) == Status::BadPoint);
  bad[2] = {0, INT32_MIN};
  REQUIRE(area2(bad.data(), 4, &a) == Status::BadPoint);
  REQUIRE(a == 77);
  // Self-intersecting ring that winds around the extreme square 1000 times: the true sum is 1000 * 2^59, far beyond int64.
  Pts wind;
  for (int k = 0; k < 1000; ++k) wind.insert(wind.end(), ext.begin(), ext.end());
  REQUIRE(wind.size() == 4000);
  REQUIRE(area2(wind.data(), 4000, &a) == Status::Ok);
  REQUIRE(a == INT64_MAX);
  std::reverse(wind.begin(), wind.end());
  REQUIRE(area2(wind.data(), 4000, &a) == Status::Ok);
  REQUIRE(a == INT64_MIN);
  // 16 turns: 16 * 2^59 = 2^63 saturates, 15 turns fits exactly.
  Pts w15, w16;
  for (int k = 0; k < 15; ++k) w15.insert(w15.end(), ext.begin(), ext.end());
  for (int k = 0; k < 16; ++k) w16.insert(w16.end(), ext.begin(), ext.end());
  REQUIRE(area2(w15.data(), 60, &a) == Status::Ok);
  REQUIRE(a == 15 * (static_cast<i64>(1) << 59));
  REQUIRE(area2(w16.data(), 64, &a) == Status::Ok);
  REQUIRE(a == INT64_MAX);
  // Exact sign for a ring whose sum is zero after huge cancellation: 1000 turns forward then 1000 turns backward.
  Pts zero;
  for (int k = 0; k < 500; ++k) zero.insert(zero.end(), ext.begin(), ext.end());
  const Pts back(ext.rbegin(), ext.rend());
  for (int k = 0; k < 500; ++k) zero.insert(zero.end(), back.begin(), back.end());
  REQUIRE(area2(zero.data(), 4000, &a) == Status::Ok);
  REQUIRE(a == 0);
  bool simple = true;
  REQUIRE(is_simple(zero.data(), 4000, &simple) == Status::Ok);
  REQUIRE_FALSE(simple);
}

TEST_CASE("is_simple: argument checks and every rejection rule", "[tess]") {
  const Pts sq = {{0, 0}, {4, 0}, {4, 4}, {0, 4}};
  bool s = false;
  REQUIRE(is_simple(nullptr, 4, &s) == Status::NullBuffer);
  REQUIRE(is_simple(sq.data(), 4, nullptr) == Status::NullBuffer);
  REQUIRE(is_simple(sq.data(), 2, &s) == Status::BadCount);
  REQUIRE(is_simple(sq.data(), 5000, &s) == Status::BadCount);
  Pts bad = sq;
  bad[1].y = -kK - 1;
  s = true;
  REQUIRE(is_simple(bad.data(), 4, &s) == Status::BadPoint);
  REQUIRE(s);  // untouched on error
  REQUIRE(is_simple(sq.data(), 4, &s) == Status::Ok);
  REQUIRE(s);

  const struct {
    const char* what;
    Pts p;
  } cases[] = {
      {"bowtie", {{0, 0}, {4, 4}, {4, 0}, {0, 4}}},
      {"coincident vertices", {{0, 0}, {4, 0}, {4, 4}, {4, 0}, {0, 4}}},
      {"vertex touches a non-adjacent edge", {{0, 0}, {8, 0}, {8, 8}, {4, 0}, {0, 8}}},
      {"edges overlap along a line", {{0, 0}, {6, 0}, {6, 4}, {2, 4}, {2, 0}, {0, 0 + 4}, {0, 4}}},
      {"spike folded onto the previous edge", {{0, 0}, {6, 0}, {3, 0}, {3, 4}, {0, 4}}},
      {"zero area, all collinear", {{0, 0}, {1, 0}, {2, 0}, {3, 0}}},
      {"zero area, three collinear", {{0, 0}, {5, 0}, {2, 0}}},
      {"crossing edges", {{0, 0}, {10, 0}, {10, 10}, {5, -5}, {0, 10}}},
      {"two squares sharing a corner", {{0, 0}, {4, 0}, {4, 4}, {8, 4}, {8, 8}, {4, 8}, {4, 4}, {0, 4}}},
  };
  for (const auto& c : cases) {
    INFO(c.what);
    s = true;
    REQUIRE(is_simple(c.p.data(), static_cast<u32>(c.p.size()), &s) == Status::Ok);
    REQUIRE_FALSE(s);
    REQUIRE(ref_simple(c.p) == false);
  }
  // Accepted: collinear consecutive vertices, both windings, and a reflex vertex that nearly touches.
  const Pts ok_cases[] = {
      {{0, 0}, {2, 0}, {4, 0}, {4, 4}, {0, 4}},
      {{0, 4}, {4, 4}, {4, 0}, {2, 0}, {0, 0}},
      {{0, 0}, {10, 0}, {10, 10}, {5, 1}, {0, 10}},
      {{0, 0}, {10, 0}, {10, 10}, {5, 0 + 1}, {0, 10}},
  };
  for (const Pts& p : ok_cases) {
    s = false;
    REQUIRE(is_simple(p.data(), static_cast<u32>(p.size()), &s) == Status::Ok);
    REQUIRE(s);
    REQUIRE(ref_simple(p));
  }
  // The same ring one unit lower touches the other edge at a vertex-on-edge point.
  const Pts touch = {{0, 0}, {10, 0}, {10, 10}, {5, 0}, {0, 10}};
  REQUIRE(is_simple(touch.data(), 5, &s) == Status::Ok);
  REQUIRE_FALSE(s);
  // Extreme coordinates: the edge cross products reach 2^59 and stay exact.
  const Pts huge = {{-kK, -kK}, {kK, -kK + 1}, {kK, kK}, {-kK, kK - 1}};
  REQUIRE(is_simple(huge.data(), 4, &s) == Status::Ok);
  REQUIRE(s);
  const Pts huge_bow = {{-kK, -kK}, {kK, kK}, {kK, -kK}, {-kK, kK}};
  REQUIRE(is_simple(huge_bow.data(), 4, &s) == Status::Ok);
  REQUIRE_FALSE(s);
}

TEST_CASE("is_simple and area2 match the references on random grid polygons", "[tess]") {
  Lcg r{1234};
  for (int i = 0; i < 4000; ++i) {
    const std::int32_t n = r.rng(3, 10), R = r.rng(1, 5);
    Pts p;
    for (std::int32_t k = 0; k < n; ++k) {
      const std::int32_t x = r.rng(-R, R), y = r.rng(-R, R);
      p.push_back({x, y});
    }
    bool s = false;
    REQUIRE(is_simple(p.data(), static_cast<u32>(n), &s) == Status::Ok);
    REQUIRE(s == ref_simple(p));
  }
}

TEST_CASE("triangulate on the named shapes: reference, tiling, rule counts and golden digests", "[tess]") {
  for (const ShapeGolden& g : kShapeGolden) {
    const ShapeDef* def = nullptr;
    for (const ShapeDef& d : kShapes)
      if (std::string_view(d.name) == g.name) def = &d;
    REQUIRE(def != nullptr);
    INFO(g.name << " variant " << g.variant);
    const Pts p = variant(def->make(), g.variant);
    REQUIRE(p.size() == g.n);
    bool simple = false;
    REQUIRE(is_simple(p.data(), g.n, &simple) == Status::Ok);
    REQUIRE(simple);
    i64 a = 0;
    REQUIRE(area2(p.data(), g.n, &a) == Status::Ok);
    REQUIRE(a == g.area2);
    const Tri t = run_tri(p);
    REQUIRE(t.st == Status::Ok);
    REQUIRE(t.count == g.tris);
    REQUIRE(t.count == g.n - 2 - g.drops);
    REQUIRE(tiling_ok(p, t));
    const std::vector<u32> w = as_words(t);
    REQUIRE(digest_indices(w.data(), w.size()) == g.digest);
    const Naive ref = naive_tri(p);
    REQUIRE(ref.fallbacks == 0);
    REQUIRE(ref.drops == g.drops);
    REQUIRE(t.out == ref.tris);
  }
}

TEST_CASE("triangulate: collinear runs, clockwise output and index rotation", "[tess]") {
  // Square with a collinear midpoint on every side. The first corner is an ear before any midpoint is met, so midpoints can appear
  // in triangles (only dropped vertices never do); the result equals the naive reference.
  const Pts sq = {{0, 0}, {2, 0}, {4, 0}, {4, 2}, {4, 4}, {2, 4}, {0, 4}, {0, 2}};
  const Tri t = run_tri(sq);
  REQUIRE(t.st == Status::Ok);
  REQUIRE(t.out == naive_tri(sq).tris);
  REQUIRE(t.count == 8 - 2 - naive_tri(sq).drops);
  REQUIRE(tiling_ok(sq, t));
  // A ring that starts at a midpoint drops it first: the scan meets vertex 0 (collinear) and drops it before any ear test.
  const Pts sq2 = {{2, 0}, {4, 0}, {4, 2}, {4, 4}, {2, 4}, {0, 4}, {0, 2}, {0, 0}};
  const Tri t3 = run_tri(sq2);
  REQUIRE(t3.st == Status::Ok);
  REQUIRE(t3.out == naive_tri(sq2).tris);
  REQUIRE(naive_tri(sq2).drops >= 1);
  REQUIRE(t3.count == 8 - 2 - naive_tri(sq2).drops);
  REQUIRE(tiling_ok(sq2, t3));
  // Triangle: counter-clockwise emits (0, 1, 2); clockwise emits (next, current, previous) = (2, 1, 0) for the first ear.
  const Pts ccw = {{0, 0}, {4, 0}, {0, 4}};
  const Tri a = run_tri(ccw);
  REQUIRE(a.out == std::vector<u32>({2, 0, 1}));  // lowest vertex 0: previous 2, current 0, next 1
  const Pts cw = {{0, 0}, {0, 4}, {4, 0}};
  const Tri b = run_tri(cw);
  REQUIRE(b.out == std::vector<u32>({1, 0, 2}));  // lowest vertex 0: previous 2, next 1, clockwise: (next, current, previous)
  REQUIRE(tiling_ok(cw, b));
  // Both windings of a concave polygon tile it with the same count.
  const Pts conc = {{0, 0}, {8, 0}, {8, 8}, {4, 2}, {0, 8}};
  Pts conc_r(conc.rbegin(), conc.rend());
  const Tri c1 = run_tri(conc), c2 = run_tri(conc_r);
  REQUIRE(c1.count == 3);
  REQUIRE(c2.count == 3);
  REQUIRE(tiling_ok(conc, c1));
  REQUIRE(tiling_ok(conc_r, c2));
  // Every start index of a polygon with collinear runs gives a valid tiling and the reference result.
  const Pts rc = rect_collinear(7, 5);
  for (std::size_t k = 0; k < rc.size(); ++k) {
    Pts r = rc;
    std::rotate(r.begin(), r.begin() + static_cast<std::ptrdiff_t>(k), r.end());
    const Tri t2 = run_tri(r);
    REQUIRE(t2.st == Status::Ok);
    REQUIRE(t2.count == r.size() - 2 - naive_tri(r).drops);
    REQUIRE(tiling_ok(r, t2));
    REQUIRE(t2.out == naive_tri(r).tris);
  }
}

TEST_CASE("triangulate matches the naive reference and the golden digests on random polygons", "[tess]") {
  struct G {
    u64 seed;
    int count, nlo, nhi, rlo, rhi;
    u32 simple, tris, drops;
    u64 digest;
  };
  const G corpora[] = {
      {7u, 3000, 3, 9, 2, 6, 726, 1280, 20, 0x2898f75a9fc6f8f4ull},
      {8u, 3000, 6, 14, 3, 8, 58, 263, 5, 0xc7b0678d4d964472ull},
  };
  for (const G& g : corpora) {
    const CorpusResult r = run_grid_corpus(g.seed, g.count, g.nlo, g.nhi, g.rlo, g.rhi);
    REQUIRE(r.simple == g.simple);
    REQUIRE(r.tris == g.tris);
    REQUIRE(r.drops == g.drops);
    REQUIRE(r.digest == g.digest);
  }
  // Grown simple polygons (many vertices, many collinear points and reflex corners), both windings, random start index.
  Lcg r{31};
  CorpusResult res;
  std::vector<u32> words;
  for (int i = 0; i < 300; ++i) {
    const std::int32_t R = r.rng(3, 6);
    const std::size_t target = static_cast<std::size_t>(r.rng(8, 24));
    Pts p = grow(r, R, target);
    if (r.next() & 1u) std::reverse(p.begin(), p.end());
    const std::size_t k = static_cast<std::size_t>(r.rng(0, static_cast<std::int32_t>(p.size()) - 1));
    std::rotate(p.begin(), p.begin() + static_cast<std::ptrdiff_t>(k), p.end());
    corpus_item(p, res, words, true);
  }
  REQUIRE(res.simple == 300);
  REQUIRE(res.tris == 3969);
  REQUIRE(res.drops == 253);
  REQUIRE(digest_indices(words.data(), words.size()) == 0x518e7438d84f0e90ull);
}

TEST_CASE("triangulate: statuses, check order and no writes on error", "[tess]") {
  const Pts sq = {{0, 0}, {4, 0}, {4, 4}, {0, 4}};
  std::vector<u32> out(6, kSentinel), scratch(12, kSentinel);
  u32 cnt = kSentinel;
  const auto untouched = [&] {
    for (u32 v : out)
      if (v != kSentinel) return false;
    for (u32 v : scratch)
      if (v != kSentinel) return false;
    return cnt == kSentinel;
  };
  const Point* p = sq.data();
  REQUIRE(triangulate(nullptr, 4, out.data(), 2, scratch.data(), 12, &cnt) == Status::NullBuffer);
  REQUIRE(triangulate(p, 4, nullptr, 2, scratch.data(), 12, &cnt) == Status::NullBuffer);
  REQUIRE(triangulate(p, 4, out.data(), 2, nullptr, 12, &cnt) == Status::NullBuffer);
  REQUIRE(triangulate(p, 4, out.data(), 2, scratch.data(), 12, nullptr) == Status::NullBuffer);
  REQUIRE(triangulate(nullptr, 2, out.data(), 2, scratch.data(), 12, &cnt) == Status::NullBuffer);  // before BadCount
  REQUIRE(triangulate(p, 2, out.data(), 2, scratch.data(), 12, &cnt) == Status::BadCount);
  REQUIRE(triangulate(p, 0, out.data(), 2, scratch.data(), 12, &cnt) == Status::BadCount);
  REQUIRE(triangulate(p, 4097, out.data(), 2, scratch.data(), 12, &cnt) == Status::BadCount);
  Pts bad = sq;
  bad[3].x = kK + 1;
  REQUIRE(triangulate(bad.data(), 4, out.data(), 2, scratch.data(), 12, &cnt) == Status::BadPoint);
  REQUIRE(triangulate(bad.data(), 4, out.data(), 1, scratch.data(), 11, &cnt) == Status::BadPoint);  // before BufferTooSmall
  REQUIRE(triangulate(bad.data(), 2, out.data(), 1, scratch.data(), 11, &cnt) == Status::BadCount);
  REQUIRE(triangulate(p, 4, out.data(), 1, scratch.data(), 12, &cnt) == Status::BufferTooSmall);
  REQUIRE(triangulate(p, 4, out.data(), 2, scratch.data(), 11, &cnt) == Status::BufferTooSmall);
  REQUIRE(triangulate(p, 4, out.data(), 0, scratch.data(), 0, &cnt) == Status::BufferTooSmall);
  const Pts bow = {{0, 0}, {4, 4}, {4, 0}, {0, 4}};
  REQUIRE(triangulate(bow.data(), 4, out.data(), 2, scratch.data(), 12, &cnt) == Status::BadPolygon);
  const Pts line = {{0, 0}, {1, 0}, {2, 0}};
  REQUIRE(triangulate(line.data(), 3, out.data(), 1, scratch.data(), 9, &cnt) == Status::BadPolygon);
  const Pts touch = {{0, 0}, {10, 0}, {10, 10}, {5, 0}, {0, 10}};
  std::vector<u32> out5(9, kSentinel), sc5(15, kSentinel);
  REQUIRE(triangulate(touch.data(), 5, out5.data(), 3, sc5.data(), 15, &cnt) == Status::BadPolygon);
  for (u32 v : out5) REQUIRE(v == kSentinel);
  for (u32 v : sc5) REQUIRE(v == kSentinel);
  REQUIRE(untouched());
  // BufferTooSmall comes before BadPolygon.
  REQUIRE(triangulate(bow.data(), 4, out.data(), 1, scratch.data(), 12, &cnt) == Status::BufferTooSmall);
  // Exactly enough is Ok; extra capacity is allowed and untouched.
  REQUIRE(triangulate(p, 4, out.data(), 2, scratch.data(), 12, &cnt) == Status::Ok);
  REQUIRE(cnt == 2);
  REQUIRE(out[6 - 1] != kSentinel);
  std::vector<u32> big(60, kSentinel);
  REQUIRE(triangulate(p, 4, big.data(), 20, scratch.data(), 12, &cnt) == Status::Ok);
  for (std::size_t i = 6; i < big.size(); ++i) REQUIRE(big[i] == kSentinel);
}

TEST_CASE("triangulate: Overlap among points, scratch, output and tri_count", "[tess]") {
  const Pts pent = parabola(5);  // 5 points = 40 bytes, scratch 15 words, output 9 words, tri_count 1 word
  const u32 n = 5;
  const std::size_t arena_words = 256;
  std::vector<u64> raw(arena_words / 2 + 8);  // 8-byte aligned arena
  u32* base = reinterpret_cast<u32*>(raw.data());
  Point* pp = reinterpret_cast<Point*>(base + 100);  // pts at word 100, bytes [400, 440): words 100..109
  const auto reset = [&] {
    std::fill(base, base + arena_words, kSentinel);
    for (u32 i = 0; i < n; ++i) pp[i] = pent[i];
  };
  struct Layout {
    std::size_t scratch, out, tri;  // word offsets in the arena
    u32 out_capacity;
    bool overlap;
    const char* what;
  };
  const Layout layouts[] = {
      {0, 20, 40, 3, false, "disjoint"},
      {0, 15, 40, 3, false, "output starts right after the scratch end"},
      {0, 14, 40, 3, true, "output shares the last scratch word"},
      {20, 0, 40, 3, false, "scratch after the output"},
      {20, 12, 40, 3, true, "output overlaps the scratch start"},
      {0, 20, 14, 3, true, "tri_count inside the scratch"},
      {0, 20, 15, 3, false, "tri_count right after the scratch"},
      {0, 20, 28, 3, true, "tri_count inside the output"},
      {0, 20, 29, 3, false, "tri_count right after the output"},
      {0, 20, 100, 3, true, "tri_count inside the points"},
      {0, 20, 110, 3, false, "tri_count right after the points"},
      {96, 20, 40, 3, true, "scratch overlaps the points"},
      {86, 20, 40, 3, true, "scratch reaches the first point word"},
      {85, 20, 40, 3, false, "scratch ends exactly before the points"},
      {70, 20, 40, 3, false, "scratch well before the points"},
      {0, 92, 40, 3, true, "output overlaps the points"},
      {0, 91, 40, 3, false, "output ends exactly before the points"},
      {0, 110, 40, 3, false, "output starts exactly after the points"},
      {0, 109, 40, 3, true, "output overlaps the last point word"},
      // The output extent is 3 * min(out_capacity, n - 2) words: a large capacity does not reach the points.
      {0, 91, 40, 50, false, "large capacity, extent is limited to n - 2 triangles"},
      {0, 92, 40, 50, true, "large capacity, overlap inside the limited extent"},
  };
  for (const Layout& l : layouts) {
    INFO(l.what);
    reset();
    const std::vector<u32> before(base, base + arena_words);
    const Status st = triangulate(pp, n, base + l.out, l.out_capacity, base + l.scratch, 15, base + l.tri);
    if (l.overlap) {
      REQUIRE(st == Status::Overlap);
      REQUIRE(std::equal(before.begin(), before.end(), base));  // nothing written on error
    } else {
      REQUIRE(st == Status::Ok);
      REQUIRE(base[l.tri] == 3);
    }
  }
  // Overlap is checked after BufferTooSmall and before BadPolygon.
  reset();
  REQUIRE(triangulate(pp, n, base, 2, base + 100 + 4, 15, base + 40) == Status::BufferTooSmall);
  Point* bow = pp;
  bow[0] = {0, 0};
  bow[1] = {4, 4};
  bow[2] = {4, 0};
  bow[3] = {0, 4};
  REQUIRE(triangulate(bow, 4, base, 2, base + 100 + 2, 12, base + 40) == Status::Overlap);  // scratch inside the points
  REQUIRE(triangulate(bow, 4, base, 2, base + 20, 12, base + 40) == Status::BadPolygon);
}

TEST_CASE("locate: inside, outside, edges, vertices and extreme int32 queries", "[tess]") {
  const Pts sq = {{0, 0}, {4, 0}, {4, 4}, {0, 4}};
  Location w = Location::Outside;
  const struct {
    std::int32_t x, y;
    Location want;
  } cases[] = {
      {2, 2, Location::Inside},      {1, 1, Location::Inside},      {0, 0, Location::Boundary},   {4, 4, Location::Boundary},
      {4, 0, Location::Boundary},    {0, 4, Location::Boundary},    {2, 0, Location::Boundary},   {4, 2, Location::Boundary},
      {2, 4, Location::Boundary},    {0, 2, Location::Boundary},    {-1, 2, Location::Outside},   {5, 2, Location::Outside},
      {2, -1, Location::Outside},    {2, 5, Location::Outside},     {5, 5, Location::Outside},    {-1, -1, Location::Outside},
      {INT32_MIN, INT32_MIN, Location::Outside},                    {INT32_MAX, INT32_MAX, Location::Outside},
      {INT32_MIN, 2, Location::Outside},                            {INT32_MAX, 2, Location::Outside},
      {2, INT32_MIN, Location::Outside},                            {2, INT32_MAX, Location::Outside},
      {INT32_MAX, INT32_MIN, Location::Outside},                    {INT32_MIN, INT32_MAX, Location::Outside},
  };
  for (const auto& c : cases) {
    INFO(c.x << "," << c.y);
    w = Location::Inside;
    REQUIRE(locate(sq.data(), 4, c.x, c.y, &w) == Status::Ok);
    REQUIRE(w == c.want);
    const Pts rev(sq.rbegin(), sq.rend());
    REQUIRE(locate(rev.data(), 4, c.x, c.y, &w) == Status::Ok);
    REQUIRE(w == c.want);
  }
  // Extreme polygon: corners and centre, and the largest int32 gaps.
  const Pts ext = {{-kK, -kK}, {kK, -kK}, {kK, kK}, {-kK, kK}};
  const struct {
    std::int32_t x, y;
    Location want;
  } ecases[] = {
      {0, 0, Location::Inside},           {kK, kK, Location::Boundary},         {-kK, -kK, Location::Boundary},
      {kK, 0, Location::Boundary},        {kK + 1, 0, Location::Outside},       {-kK - 1, 0, Location::Outside},
      {INT32_MAX, INT32_MAX, Location::Outside}, {INT32_MIN, INT32_MIN, Location::Outside},
      {INT32_MIN, 0, Location::Outside},  {0, INT32_MAX, Location::Outside},    {kK - 1, kK - 1, Location::Inside},
  };
  for (const auto& c : ecases) {
    INFO(c.x << "," << c.y);
    REQUIRE(locate(ext.data(), 4, c.x, c.y, &w) == Status::Ok);
    REQUIRE(w == c.want);
  }
  // Argument checks; the result is untouched on error.
  w = Location::Inside;
  REQUIRE(locate(nullptr, 4, 0, 0, &w) == Status::NullBuffer);
  REQUIRE(locate(sq.data(), 4, 0, 0, nullptr) == Status::NullBuffer);
  REQUIRE(locate(sq.data(), 2, 0, 0, &w) == Status::BadCount);
  REQUIRE(locate(sq.data(), 4097, 0, 0, &w) == Status::BadCount);
  Pts bad = sq;
  bad[0].x = kK + 1;
  REQUIRE(locate(bad.data(), 4, 0, 0, &w) == Status::BadPoint);
  REQUIRE(w == Location::Inside);
  // A non-simple polygon does not crash; the answer is unspecified.
  const Pts bow = {{0, 0}, {4, 4}, {4, 0}, {0, 4}};
  REQUIRE(locate(bow.data(), 4, 2, 2, &w) == Status::Ok);
}

TEST_CASE("locate agrees with the tessellation and the edge list on grids", "[tess]") {
  // Reference: a point on an edge is Boundary; otherwise it is Inside exactly when it lies in some triangle of the exact tiling.
  const char* names[] = {"star", "comb", "spiral", "L_ext", "parabola", "rect_col"};
  for (const char* name : names) {
    const ShapeDef* def = nullptr;
    for (const ShapeDef& d : kShapes)
      if (std::string_view(d.name) == name) def = &d;
    REQUIRE(def != nullptr);
    Pts p = def->make();
    const bool big = std::string_view(name) == "L_ext";
    if (big) {
      // Scale the extreme L down to a small grid for the exhaustive scan: the structure (and the collinear points) are kept.
      for (Point& q : p) {
        q.x = q.x / kK * 6;
        q.y = q.y / kK * 6;
      }
    }
    for (int v = 0; v < 2; ++v) {
      const Pts q = variant(p, v == 0 ? 0 : 3);
      const Tri t = run_tri(q);
      REQUIRE(t.st == Status::Ok);
      std::int32_t lox = INT32_MAX, hix = INT32_MIN, loy = INT32_MAX, hiy = INT32_MIN;
      for (const Point& a : q) {
        lox = std::min(lox, a.x);
        hix = std::max(hix, a.x);
        loy = std::min(loy, a.y);
        hiy = std::max(hiy, a.y);
      }
      const std::int32_t stepx = std::max<std::int32_t>(1, (hix - lox) / 60), stepy = std::max<std::int32_t>(1, (hiy - loy) / 60);
      for (std::int32_t y = loy - 2; y <= hiy + 2; y += stepy)
        for (std::int32_t x = lox - 2; x <= hix + 2; x += stepx) {
          bool on_edge = false;
          for (std::size_t i = 0; i < q.size(); ++i) {
            const Point &a = q[i], &b = q[(i + 1) % q.size()];
            if (orient(a, b, Point{x, y}) == 0 && on_box(a, b, Point{x, y})) on_edge = true;
          }
          bool in_tri = false;
          for (u32 k = 0; k < t.count && !in_tri; ++k) {
            const Point &a = q[t.out[3 * k]], &b = q[t.out[3 * k + 1]], &c = q[t.out[3 * k + 2]];
            const Point pt{x, y};
            in_tri = orient(a, b, pt) >= 0 && orient(b, c, pt) >= 0 && orient(c, a, pt) >= 0;  // triangles are counter-clockwise
          }
          const Location want = on_edge ? Location::Boundary : (in_tri ? Location::Inside : Location::Outside);
          Location got = Location::Outside;
          REQUIRE(locate(q.data(), static_cast<u32>(q.size()), x, y, &got) == Status::Ok);
          REQUIRE(got == want);
        }
    }
  }
}

TEST_CASE("every vertex and every edge midpoint of the corpus shapes locate as Boundary", "[tess]") {
  const Pts p = star(32);
  Location w = Location::Outside;
  for (std::size_t i = 0; i < p.size(); ++i) {
    REQUIRE(locate(p.data(), static_cast<u32>(p.size()), p[i].x, p[i].y, &w) == Status::Ok);
    REQUIRE(w == Location::Boundary);
  }
  const Pts r = rect_collinear(30, 20);
  for (std::size_t i = 0; i < r.size(); ++i) {
    REQUIRE(locate(r.data(), static_cast<u32>(r.size()), r[i].x, r[i].y, &w) == Status::Ok);
    REQUIRE(w == Location::Boundary);
  }
}

TEST_CASE("performance: 4096-vertex comb, spiral and collinear rectangle (sanitizer safe)", "[tess][perf]") {
  const Pts shapes[3] = {comb(1024), spiral(2047), rect_collinear(1024, 1024)};
  const std::size_t expect_n[3] = {4096, 4096, 4096};
  const u32 max_tris = 4094;
  for (int i = 0; i < 3; ++i) {
    INFO("shape " << i);
    const Pts& p = shapes[i];
    REQUIRE(p.size() == expect_n[i]);
    const Tri t = run_tri(p);
    REQUIRE(t.st == Status::Ok);
    REQUIRE(t.count <= max_tris);
    if (i < 2) REQUIRE(t.count == max_tris);  // no collinear vertex in the comb and the spiral
    REQUIRE(tiling_ok(p, t));
    bool simple = false;
    REQUIRE(is_simple(p.data(), 4096, &simple) == Status::Ok);
    REQUIRE(simple);
  }
}

TEST_CASE("triangulate equals the naive reference on mid-size comb, spiral and collinear rectangle", "[tess]") {
  const Pts shapes[3] = {comb(128), spiral(255), rect_collinear(128, 128)};
  for (int i = 0; i < 3; ++i)
    for (int v = 0; v < 4; ++v) {
      INFO("shape " << i << " variant " << v);
      const Pts p = variant(shapes[i], v);
      const Tri t = run_tri(p);
      REQUIRE(t.st == Status::Ok);
      const Naive ref = naive_tri(p);
      REQUIRE(ref.fallbacks == 0);
      REQUIRE(t.out == ref.tris);
      REQUIRE(t.count == p.size() - 2 - ref.drops);
      REQUIRE(tiling_ok(p, t));
    }
}
