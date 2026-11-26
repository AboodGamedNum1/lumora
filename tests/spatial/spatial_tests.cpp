// Tests for the integer spatial tree (core/spatial).
//
// Expected digests in kGolden were produced by an independent Python reference of the rules in docs/spatial.md (recursive build
// with a library sort on the (key, index) order, brute-force queries, its own FNV-1a) and pasted here as constants. The relation
// tests compare the implementation with a naive reference written in this file, not with itself.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

#include "spatial.h"

using namespace lumora::spatial;

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Boxes = std::vector<Box>;

constexpr std::int32_t kK = kMaxCoord;
constexpr u32 kSentinel = 0xA5A5A5A5u;

struct Lcg {
  u64 s;
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  std::int32_t rng(std::int32_t lo, std::int32_t hi) {  // inclusive, range below 2^30
    return static_cast<std::int32_t>(lo + static_cast<i64>(next() % static_cast<u32>(hi - lo + 1)));
  }
};

// Dataset kinds (mirrored by the Python reference): 0 random, 1 identical, 2 nested, 3 touching grid, 4 points, 5 line of
// touching flat boxes, 6 extreme coordinates.
Boxes gen(int kind, u32 n, u64 seed) {
  Lcg r{seed};
  Boxes b;
  b.reserve(n);
  for (u32 i = 0; i < n; ++i) {
    const std::int32_t ii = static_cast<std::int32_t>(i);
    switch (kind) {
      case 0: {
        const std::int32_t x = r.rng(-1000, 1000), y = r.rng(-1000, 1000), w = r.rng(0, 50), h = r.rng(0, 50);
        b.push_back({x, y, x + w, y + h});
        break;
      }
      case 1: b.push_back({5, 5, 10, 10}); break;
      case 2: b.push_back({-ii, -ii, ii, ii}); break;
      case 3: {
        const std::int32_t cx = ii % 32, cy = ii / 32;
        b.push_back({cx * 10, cy * 10, cx * 10 + 10, cy * 10 + 10});
        break;
      }
      case 4: {
        const std::int32_t x = r.rng(-100, 100), y = r.rng(-100, 100);
        b.push_back({x, y, x, y});
        break;
      }
      case 5: b.push_back({ii * 3, 0, ii * 3 + 3, 0}); break;
      default: {
        const std::int32_t c3 = r.rng(-1000, 1000), c4 = r.rng(-kK, kK);
        const std::int32_t c[5] = {-kK, kK, 0, c3, c4};
        const std::int32_t xa = c[r.next() % 5], xb = c[r.next() % 5], ya = c[r.next() % 5], yb = c[r.next() % 5];
        b.push_back({std::min(xa, xb), std::min(ya, yb), std::max(xa, xb), std::max(ya, yb)});
        break;
      }
    }
  }
  return b;
}

struct QuerySet {
  std::vector<Box> boxes;
  std::vector<std::pair<std::int32_t, std::int32_t>> points;
};

QuerySet make_queries(u64 seed) {
  Lcg r{seed ^ 0x5151u};
  QuerySet q;
  for (int i = 0; i < 48; ++i) {
    const std::int32_t x = r.rng(-1100, 1100), y = r.rng(-1100, 1100), w = r.rng(0, 400), h = r.rng(0, 400);
    q.boxes.push_back({x, y, x + w, y + h});
  }
  q.boxes.push_back({INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX});
  q.boxes.push_back({kK, kK, kK, kK});
  q.boxes.push_back({-kK, -kK, -kK, -kK});
  q.boxes.push_back({0, 0, 0, 0});
  q.boxes.push_back({-kK, -kK, kK, kK});
  for (int i = 0; i < 32; ++i) {
    const std::int32_t x = r.rng(-1100, 1100), y = r.rng(-1100, 1100);
    q.points.push_back({x, y});
  }
  q.points.push_back({INT32_MIN, INT32_MIN});
  q.points.push_back({INT32_MAX, INT32_MAX});
  q.points.push_back({kK, -kK});
  q.points.push_back({0, 0});
  return q;
}

bool inter(const Box& a, const Box& b) { return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1; }

u64 ref_d2(const Box& b, std::int32_t px, std::int32_t py) {
  const i64 dx = std::max<i64>({static_cast<i64>(b.x0) - px, 0, static_cast<i64>(px) - b.x1});
  const i64 dy = std::max<i64>({static_cast<i64>(b.y0) - py, 0, static_cast<i64>(py) - b.y1});
  return static_cast<u64>(dx) * static_cast<u64>(dx) + static_cast<u64>(dy) * static_cast<u64>(dy);
}

std::vector<u32> brute_box(const Boxes& items, const Box& q) {
  std::vector<u32> r;
  for (u32 i = 0; i < items.size(); ++i)
    if (inter(items[i], q)) r.push_back(i);
  return r;
}

struct Near {
  bool found;
  u32 index;
  u64 d2;
};
Near brute_nearest(const Boxes& items, std::int32_t px, std::int32_t py) {
  Near n{false, 0, 0};
  for (u32 i = 0; i < items.size(); ++i) {
    const u64 d = ref_d2(items[i], px, py);
    if (!n.found || d < n.d2) n = {true, i, d};
  }
  return n;
}

// Reference build: recursive, library sort on the (key, index) order, written independently of the module.
struct RefTree {
  std::vector<Node> nodes;
  std::vector<u32> order;
};
u32 ref_rec(const Boxes& items, u32 leaf, RefTree& t, u32 lo, u32 hi) {
  const u32 idx = static_cast<u32>(t.nodes.size());
  t.nodes.push_back(Node{});
  Box bb = items[t.order[lo]];
  for (u32 k = lo; k < hi; ++k) {
    const Box& b = items[t.order[k]];
    bb = {std::min(bb.x0, b.x0), std::min(bb.y0, b.y0), std::max(bb.x1, b.x1), std::max(bb.y1, b.y1)};
  }
  Node nd{bb, lo, hi - lo, 0};
  if (hi - lo > leaf) {
    const bool y_axis = static_cast<i64>(bb.y1) - bb.y0 > static_cast<i64>(bb.x1) - bb.x0;
    std::sort(t.order.begin() + lo, t.order.begin() + hi, [&](u32 a, u32 b) {
      const i64 ka = y_axis ? static_cast<i64>(items[a].y0) + items[a].y1 : static_cast<i64>(items[a].x0) + items[a].x1;
      const i64 kb = y_axis ? static_cast<i64>(items[b].y0) + items[b].y1 : static_cast<i64>(items[b].x0) + items[b].x1;
      return ka != kb ? ka < kb : a < b;
    });
    const u32 mid = lo + (hi - lo) / 2;
    nd.count = 0;
    nd.first = ref_rec(items, leaf, t, lo, mid);
    nd.right = ref_rec(items, leaf, t, mid, hi);
  }
  t.nodes[idx] = nd;
  return idx;
}
RefTree ref_build(const Boxes& items, u32 leaf) {
  RefTree t;
  t.order.resize(items.size());
  for (u32 i = 0; i < items.size(); ++i) t.order[i] = i;
  if (!items.empty()) ref_rec(items, leaf, t, 0, static_cast<u32>(items.size()));
  return t;
}

u32 ref_nodes(u32 n, u32 leaf) { return n <= leaf ? 1 : 1 + ref_nodes(n / 2, leaf) + ref_nodes(n - n / 2, leaf); }

struct Built {
  Boxes items;
  std::vector<Node> nodes;
  std::vector<u32> order;
  u32 node_count = 0;
  u32 leaf = 0;
  Status st = Status::Ok;
};
Built make(Boxes items, u32 leaf) {
  Built b;
  b.items = std::move(items);
  b.leaf = leaf;
  const u32 n = static_cast<u32>(b.items.size());
  b.nodes.resize(node_bound(n, leaf) + 1);
  b.order.resize(n + 1);
  b.st = build(b.items.data(), n, leaf, b.nodes.data(), static_cast<u32>(b.nodes.size()), b.order.data(), &b.node_count);
  return b;
}

bool same_node(const Node& a, const Node& b) {
  return a.box.x0 == b.box.x0 && a.box.y0 == b.box.y0 && a.box.x1 == b.box.x1 && a.box.y1 == b.box.y1 && a.first == b.first &&
         a.count == b.count && a.right == b.right;
}

// Checks every structural rule of the header on a built tree. Recursion is fine here (test code, depth <= 21).
bool check_node(const Built& b, u32 idx, u32 lo, u32 hi, u32& next) {
  if (idx != next) return false;  // depth first numbering
  ++next;
  const Node& nd = b.nodes[idx];
  Box bb = b.items[b.order[lo]];
  for (u32 k = lo; k < hi; ++k) {
    const Box& x = b.items[b.order[k]];
    bb = {std::min(bb.x0, x.x0), std::min(bb.y0, x.y0), std::max(bb.x1, x.x1), std::max(bb.y1, x.y1)};
  }
  if (nd.box.x0 != bb.x0 || nd.box.y0 != bb.y0 || nd.box.x1 != bb.x1 || nd.box.y1 != bb.y1) return false;  // tight, contains all
  if (hi - lo <= b.leaf) return nd.count == hi - lo && nd.count >= 1 && nd.count <= b.leaf && nd.first == lo && nd.right == 0;
  if (nd.count != 0 || nd.first != idx + 1) return false;
  const u32 mid = lo + (hi - lo) / 2;
  if (!check_node(b, nd.first, lo, mid, next)) return false;
  if (nd.right != next) return false;
  return check_node(b, nd.right, mid, hi, next);
}
bool check_tree(const Built& b) {
  const u32 n = static_cast<u32>(b.items.size());
  std::vector<char> seen(n, 0);
  for (u32 i = 0; i < n; ++i) {
    if (b.order[i] >= n || seen[b.order[i]]) return false;
    seen[b.order[i]] = 1;
  }
  if (n == 0) return b.node_count == 0;
  u32 next = 0;
  if (!check_node(b, 0, 0, n, next)) return false;
  return next == b.node_count && next == node_bound(n, b.leaf);
}

std::vector<u32> node_words(const Node* nodes, u32 count) {
  std::vector<u32> w;
  for (u32 i = 0; i < count; ++i) {
    const Node& n = nodes[i];
    w.insert(w.end(), {static_cast<u32>(n.box.x0), static_cast<u32>(n.box.y0), static_cast<u32>(n.box.x1), static_cast<u32>(n.box.y1),
                       n.first, n.count, n.right});
  }
  return w;
}

struct Digests {
  u32 nodes;
  u64 order, node, query, near;
};

Digests run_digests(const Built& b, u64 seed) {
  const u32 n = static_cast<u32>(b.items.size());
  Digests d{};
  d.nodes = b.node_count;
  d.order = digest_indices(b.order.data(), n);
  const std::vector<u32> nw = node_words(b.nodes.data(), b.node_count);
  d.node = digest_indices(nw.data(), nw.size());
  const QuerySet qs = make_queries(seed);
  std::vector<u32> out(n + 1), qv, nv;
  for (const Box& q : qs.boxes) {
    u32 found = 0;
    REQUIRE(query_box(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), q, out.data(), n + 1, &found) == Status::Ok);
    qv.push_back(found);
    qv.insert(qv.end(), out.begin(), out.begin() + found);
  }
  for (const auto& p : qs.points) {
    u32 found = 0;
    REQUIRE(query_point(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), p.first, p.second, out.data(), n + 1, &found) ==
            Status::Ok);
    qv.push_back(found);
    qv.insert(qv.end(), out.begin(), out.begin() + found);
    u32 idx = 0;
    u64 d2 = 0;
    bool f = false;
    REQUIRE(nearest(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), p.first, p.second, &idx, &d2, &f) == Status::Ok);
    nv.insert(nv.end(), {f ? 1u : 0u, idx, static_cast<u32>(d2 & 0xFFFFFFFFu), static_cast<u32>(d2 >> 32)});
  }
  d.query = digest_indices(qv.data(), qv.size());
  d.near = digest_indices(nv.data(), nv.size());
  return d;
}

struct Golden {
  int kind;
  u32 n, leaf;
  u64 seed;
  u32 nodes;
  u64 order, node, query, near;
};
constexpr Golden kGolden[] = {
{0, 1u, 1u, 1u, 1, 0x4d25767f9dce13f5ull, 0x7db3df6131d8e44eull, 0x263cfe494e959145ull, 0x0bb0e308a5f5f0eaull},
  {0, 2u, 1u, 2u, 3, 0x08cd4c29d1e47d34ull, 0xa991b3106024af39ull, 0xb2e29e0742fed244ull, 0x52925d85845900eaull},
  {0, 17u, 1u, 3u, 33, 0x7d62e82676e3a685ull, 0xa7b43786c818f5e6ull, 0x7714aec9517a7480ull, 0xbd64b93c654a2735ull},
  {0, 100u, 16u, 4u, 15, 0x89ead70d726c6f55ull, 0x6239634f491997aaull, 0x896e220921fb9fe5ull, 0x19a3c78b67ea1c0bull},
  {0, 1000u, 1u, 5u, 1999, 0x9c261768ff31c385ull, 0xf15873fc36a35d07ull, 0xdb2f036af0aca6bdull, 0xba9cedfe34bf5f83ull},
  {0, 1000u, 5u, 6u, 511, 0xeee6fb9595d92301ull, 0xcf06eda57d55ca03ull, 0x1f2ef01788e7c2a7ull, 0x05cde6f83bd96691ull},
  {0, 3000u, 16u, 7u, 511, 0xb5dcf3449fe96fb1ull, 0x25c9cd5cf9095375ull, 0xe15203a3e1c1366dull, 0x93e1b3e667d5adafull},
  {1, 300u, 16u, 8u, 63, 0xed6a8b24f7b464f1ull, 0xa0d6c2a557a1536eull, 0x1f870b6e71fe11c5ull, 0xbe6530adb35da733ull},
  {1, 40u, 1u, 9u, 79, 0xf60decece00f7b25ull, 0xf6a5e47ce902052aull, 0xf042d82c50283335ull, 0x7d52b95c5f77d3dfull},
  {2, 200u, 3u, 10u, 143, 0x40d0f1d3d90d9325ull, 0x015751b13493f0faull, 0x5638f1a80e684c58ull, 0xe6ded271462ad815ull},
  {3, 512u, 16u, 11u, 63, 0xb225f94dd46e1125ull, 0xdacf837b0ae230a1ull, 0x5cfc78608bae9c90ull, 0x679505958265b248ull},
  {3, 100u, 2u, 12u, 127, 0xf97d7fb9d73ebca5ull, 0x4cc1664da70e10f5ull, 0x23c0841a136d9f75ull, 0x5a166fa015e73e25ull},
  {4, 700u, 4u, 13u, 511, 0x3d4d80504a020c21ull, 0xccd45e45ea44a59dull, 0xae4c5699f460ff41ull, 0xefcbdc1ec2ae62a3ull},
  {4, 1000u, 16u, 14u, 127, 0x8b94598208c897c9ull, 0x65552297203dcd7full, 0x0dbe52afb0c7cec5ull, 0x82ed56c32b08d807ull},
  {5, 333u, 7u, 15u, 127, 0xd8b8d5e74a9f5dc2ull, 0xf915ad2157f3b22bull, 0xfa07aa3bdf131e49ull, 0xa983de2269c0538full},
  {6, 400u, 16u, 16u, 63, 0xc80c63ae09173571ull, 0x8cf0aad53d2e6bf6ull, 0xb69d4c3080421509ull, 0x8490466dc8c84839ull},
  {6, 64u, 1u, 17u, 127, 0x0deb0cfcf3dd62c5ull, 0xfd98672cfb225338ull, 0xd0631d6e2fe97128ull, 0xcf3c52a9d7ceef42ull},
  {0, 65536u, 16u, 18u, 8191, 0x01585832053091ddull, 0x5eeed3c3017cc501ull, 0xae8cb2e8cfc58098ull, 0x80eb61754c72b425ull},
};

// Same as the table rows but at 2^20 items; not run under sanitizers (see the performance test).
constexpr Golden kGoldenBig = {0, 1048576u, 16u, 19u, 131071, 0xa1baf36686f39e3dull, 0x63149bfed58c4689ull, 0x7b91cd966e8d4fa2ull, 0xaccad719a3de4d56ull};

bool expect_digests(const Golden& g, const Built& b) {
  const Digests d = run_digests(b, g.seed);
  return d.nodes == g.nodes && d.order == g.order && d.node == g.node && d.query == g.query && d.near == g.near;
}

}  // namespace

TEST_CASE("status_text names", "[spatial]") {
  REQUIRE(std::string_view(status_text(Status::Ok)) == "ok");
  REQUIRE(std::string_view(status_text(Status::NullBuffer)) == "null buffer");
  REQUIRE(std::string_view(status_text(Status::BadCount)) == "bad count");
  REQUIRE(std::string_view(status_text(Status::BadBox)) == "bad box");
  REQUIRE(std::string_view(status_text(Status::BufferTooSmall)) == "buffer too small");
  REQUIRE(std::string_view(status_text(Status::BadIndex)) == "bad index");
  REQUIRE(std::string_view(status_text(static_cast<Status>(200))) == "unknown");
}

TEST_CASE("digest_indices is FNV-1a 64 over little-endian words", "[spatial]") {
  REQUIRE(digest_indices(nullptr, 5) == 0xcbf29ce484222325ull);
  const u32 v[1] = {1};
  REQUIRE(digest_indices(v, 0) == 0xcbf29ce484222325ull);
  // Independent: FNV-1a of the bytes 01 00 00 00.
  u64 h = 0xcbf29ce484222325ull;
  const std::uint8_t bytes[4] = {1, 0, 0, 0};
  for (std::uint8_t c : bytes) {
    h ^= c;
    h *= 0x100000001b3ull;
  }
  REQUIRE(digest_indices(v, 1) == h);
}

TEST_CASE("node_bound matches the recursive definition", "[spatial]") {
  REQUIRE(node_bound(0, 4) == 0);
  REQUIRE(node_bound(5, 0) == 0);
  REQUIRE(node_bound(5, kMaxLeaf + 1) == 0);
  REQUIRE(node_bound(kMaxItems + 1, 4) == 0);
  const u32 leaves[] = {1, 2, 3, 4, 5, 7, 8, 15, 16};
  for (u32 leaf : leaves)
    for (u32 n = 1; n <= 3000; ++n) REQUIRE(node_bound(n, leaf) == ref_nodes(n, leaf));
  const u32 big[] = {kMaxItems, kMaxItems - 1, (1u << 16) + 1, 1000000u, 999999u, (1u << 19) + 12345u};
  for (u32 leaf : leaves)
    for (u32 n : big) REQUIRE(node_bound(n, leaf) == ref_nodes(n, leaf));
  REQUIRE(node_bound(1, 1) == 1);
  REQUIRE(node_bound(2, 1) == 3);
  REQUIRE(node_bound(16, 16) == 1);
  REQUIRE(node_bound(17, 16) == 3);
}

TEST_CASE("build statuses, order of checks and no writes on error", "[spatial]") {
  Boxes items = gen(0, 10, 99);
  std::vector<Node> nodes(64);
  std::vector<u32> order(10, kSentinel);
  u32 count = kSentinel;
  for (Node& n : nodes) n = Node{{static_cast<std::int32_t>(kSentinel), 0, 0, 0}, kSentinel, kSentinel, kSentinel};
  const auto untouched = [&] {
    for (const Node& n : nodes)
      if (n.first != kSentinel || n.count != kSentinel || n.right != kSentinel) return false;
    for (u32 o : order)
      if (o != kSentinel) return false;
    return count == kSentinel;
  };
  const u32 cap = static_cast<u32>(nodes.size());
  REQUIRE(build(nullptr, 10, 4, nodes.data(), cap, order.data(), &count) == Status::NullBuffer);
  REQUIRE(build(items.data(), 10, 4, nullptr, cap, order.data(), &count) == Status::NullBuffer);
  REQUIRE(build(items.data(), 10, 4, nodes.data(), cap, nullptr, &count) == Status::NullBuffer);
  REQUIRE(build(nullptr, kMaxItems + 1, 4, nodes.data(), cap, order.data(), &count) == Status::NullBuffer);  // before BadCount
  REQUIRE(build(items.data(), kMaxItems + 1, 4, nodes.data(), cap, order.data(), &count) == Status::BadCount);
  REQUIRE(build(items.data(), 10, 0, nodes.data(), cap, order.data(), &count) == Status::BadCount);
  REQUIRE(build(items.data(), 10, kMaxLeaf + 1, nodes.data(), cap, order.data(), &count) == Status::BadCount);
  Boxes bad = items;
  bad[7] = {3, 0, 2, 0};  // reversed x
  bad[3] = {0, 5, 1, 4};  // reversed y (earlier index)
  REQUIRE(build(bad.data(), 10, 0, nodes.data(), cap, order.data(), &count) == Status::BadCount);  // before BadBox
  REQUIRE(build(bad.data(), 10, 4, nodes.data(), 1, order.data(), &count) == Status::BadBox);      // before BufferTooSmall
  bad = items;
  bad[9] = {0, 0, kK + 1, 0};
  REQUIRE(build(bad.data(), 10, 4, nodes.data(), cap, order.data(), &count) == Status::BadBox);
  bad[9] = {-kK - 1, 0, 0, 0};
  REQUIRE(build(bad.data(), 10, 4, nodes.data(), cap, order.data(), &count) == Status::BadBox);
  bad[9] = {0, INT32_MIN, 0, 0};
  REQUIRE(build(bad.data(), 10, 4, nodes.data(), cap, order.data(), &count) == Status::BadBox);
  const u32 need = node_bound(10, 4);
  REQUIRE(build(items.data(), 10, 4, nodes.data(), need - 1, order.data(), &count) == Status::BufferTooSmall);
  REQUIRE(build(items.data(), 10, 4, nodes.data(), 0, order.data(), &count) == Status::BufferTooSmall);
  REQUIRE(untouched());

  // Exactly the bound is enough, and nothing past the bound is written.
  REQUIRE(build(items.data(), 10, 4, nodes.data(), need, order.data(), nullptr) == Status::Ok);
  for (u32 i = need; i < cap; ++i) REQUIRE(nodes[i].first == kSentinel);
  REQUIRE(build(items.data(), 10, 4, nodes.data(), need, order.data(), &count) == Status::Ok);
  REQUIRE(count == need);

  // Coordinates exactly at the limit are valid.
  Boxes lim = {{-kK, -kK, kK, kK}, {kK, kK, kK, kK}, {-kK, kK, -kK, kK}};
  Built b = make(lim, 1);
  REQUIRE(b.st == Status::Ok);
  REQUIRE(check_tree(b));

  // Empty set: Ok, zero nodes, null pointers allowed, nothing written.
  count = kSentinel;
  REQUIRE(build(nullptr, 0, 4, nullptr, 0, nullptr, &count) == Status::Ok);
  REQUIRE(count == 0);
  REQUIRE(build(nullptr, 0, 4, nullptr, 0, nullptr, nullptr) == Status::Ok);
  REQUIRE(build(nullptr, 0, 0, nullptr, 0, nullptr, &count) == Status::BadCount);
}

TEST_CASE("build equals the independent reference and obeys the structural rules", "[spatial]") {
  const u32 sizes[] = {1, 2, 3, 4, 15, 16, 17, 31, 33, 100, 255, 1000, 3000};
  const u32 leaves[] = {1, 2, 5, 16};
  for (int kind = 0; kind <= 6; ++kind)
    for (u32 leaf : leaves)
      for (u32 n : sizes) {
        const Boxes items = gen(kind, n, 1000u + static_cast<u64>(kind) * 17u + n);
        const Built b = make(items, leaf);
        REQUIRE(b.st == Status::Ok);
        const RefTree ref = ref_build(items, leaf);
        REQUIRE(b.node_count == ref.nodes.size());
        REQUIRE(b.node_count == ref_nodes(n, leaf));
        for (u32 i = 0; i < b.node_count; ++i) REQUIRE(same_node(b.nodes[i], ref.nodes[i]));
        for (u32 i = 0; i < n; ++i) REQUIRE(b.order[i] == ref.order[i]);
        REQUIRE(check_tree(b));
      }
}

TEST_CASE("node_bound equals the actual node count for every n and several leaf sizes", "[spatial]") {
  const u32 leaves[] = {1, 3, 16};
  for (u32 leaf : leaves)
    for (u32 n = 1; n <= 2500; ++n) {
      const Built b = make(gen(1, n, 1), leaf);  // identical boxes: the tree shape depends on counts only
      REQUIRE(b.st == Status::Ok);
      REQUIRE(b.node_count == node_bound(n, leaf));
    }
}

TEST_CASE("repeated builds are byte identical and ignore dirty buffers", "[spatial]") {
  const Boxes items = gen(0, 2000, 5);
  Built a = make(items, 4);
  Built b;
  b.items = items;
  b.leaf = 4;
  b.nodes.assign(a.nodes.size(), Node{{7, 7, 7, 7}, 7, 7, 7});
  b.order.assign(a.order.size(), 0xFFFFFFFFu);
  b.st = build(b.items.data(), 2000, 4, b.nodes.data(), static_cast<u32>(b.nodes.size()), b.order.data(), &b.node_count);
  REQUIRE(b.st == Status::Ok);
  REQUIRE(a.node_count == b.node_count);
  for (u32 i = 0; i < a.node_count; ++i) REQUIRE(same_node(a.nodes[i], b.nodes[i]));
  for (u32 i = 0; i < 2000; ++i) REQUIRE(a.order[i] == b.order[i]);
  REQUIRE(std::memcmp(a.order.data(), b.order.data(), 2000 * sizeof(u32)) == 0);
}

TEST_CASE("query_box and query_point match brute force with ordered outputs", "[spatial]") {
  const u32 leaves[] = {1, 4, 16};
  for (int kind = 0; kind <= 6; ++kind)
    for (u32 leaf : leaves) {
      const u32 n = kind == 2 ? 300 : 1500;
      const Built b = make(gen(kind, n, 40u + static_cast<u64>(kind)), leaf);
      REQUIRE(b.st == Status::Ok);
      const QuerySet qs = make_queries(40u + static_cast<u64>(kind));
      std::vector<u32> out(n + 1);
      for (const Box& q : qs.boxes) {
        const std::vector<u32> want = brute_box(b.items, q);
        u32 found = 77;
        REQUIRE(query_box(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), q, out.data(), n + 1, &found) == Status::Ok);
        REQUIRE(found == want.size());
        for (u32 i = 0; i < found; ++i) REQUIRE(out[i] == want[i]);
      }
      for (const auto& p : qs.points) {
        const std::vector<u32> want = brute_box(b.items, Box{p.first, p.second, p.first, p.second});
        u32 found = 77;
        REQUIRE(query_point(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), p.first, p.second, out.data(), n + 1,
                            &found) == Status::Ok);
        REQUIRE(found == want.size());
        for (u32 i = 0; i < found; ++i) REQUIRE(out[i] == want[i]);
      }
    }
}

TEST_CASE("queries: touching counts, degenerate boxes and empty or single sets", "[spatial]") {
  // Grid of touching cells: a point on a shared corner hits all four cells, an edge point hits two.
  Built g = make(gen(3, 64, 0), 1);
  std::vector<u32> out(64);
  u32 found = 0;
  REQUIRE(query_point(g.items.data(), 64, g.nodes.data(), g.node_count, g.order.data(), 10, 10, out.data(), 64, &found) == Status::Ok);
  REQUIRE(found == 4);
  REQUIRE(out[0] == 0);
  REQUIRE(out[1] == 1);
  REQUIRE(out[2] == 32);
  REQUIRE(out[3] == 33);
  REQUIRE(query_point(g.items.data(), 64, g.nodes.data(), g.node_count, g.order.data(), 10, 5, out.data(), 64, &found) == Status::Ok);
  REQUIRE(found == 2);
  REQUIRE(out[0] == 0);
  REQUIRE(out[1] == 1);
  REQUIRE(query_point(g.items.data(), 64, g.nodes.data(), g.node_count, g.order.data(), 11, -1, out.data(), 64, &found) == Status::Ok);
  REQUIRE(found == 0);
  // A query box that only touches the outside edge of the grid.
  REQUIRE(query_box(g.items.data(), 64, g.nodes.data(), g.node_count, g.order.data(), Box{-5, -5, 0, 0}, out.data(), 64, &found) ==
          Status::Ok);
  REQUIRE(found == 1);
  REQUIRE(out[0] == 0);
  REQUIRE(query_box(g.items.data(), 64, g.nodes.data(), g.node_count, g.order.data(), Box{-5, -5, -1, 100}, out.data(), 64, &found) ==
          Status::Ok);
  REQUIRE(found == 0);

  // Identical and nested sets.
  Built same = make(gen(1, 40, 0), 3);
  std::vector<u32> o2(40);
  REQUIRE(query_point(same.items.data(), 40, same.nodes.data(), same.node_count, same.order.data(), 5, 10, o2.data(), 40, &found) ==
          Status::Ok);
  REQUIRE(found == 40);
  for (u32 i = 0; i < 40; ++i) REQUIRE(o2[i] == i);
  Built nest = make(gen(2, 50, 0), 2);
  std::vector<u32> o3(50);
  REQUIRE(query_point(nest.items.data(), 50, nest.nodes.data(), nest.node_count, nest.order.data(), 0, 0, o3.data(), 50, &found) ==
          Status::Ok);
  REQUIRE(found == 50);
  REQUIRE(query_point(nest.items.data(), 50, nest.nodes.data(), nest.node_count, nest.order.data(), 49, 49, o3.data(), 50, &found) ==
          Status::Ok);
  REQUIRE(found == 1);
  REQUIRE(o3[0] == 49);

  // Single item and empty set (null pointers are fine with zero items).
  Built one = make(Boxes{{1, 2, 3, 4}}, 16);
  u32 o1[1];
  REQUIRE(query_box(one.items.data(), 1, one.nodes.data(), one.node_count, one.order.data(), Box{3, 4, 9, 9}, o1, 1, &found) == Status::Ok);
  REQUIRE(found == 1);
  REQUIRE(o1[0] == 0);
  found = 9;
  REQUIRE(query_box(nullptr, 0, nullptr, 0, nullptr, Box{0, 0, 1, 1}, nullptr, 0, &found) == Status::Ok);
  REQUIRE(found == 0);
  found = 9;
  REQUIRE(query_point(nullptr, 0, nullptr, 0, nullptr, 0, 0, nullptr, 0, &found) == Status::Ok);
  REQUIRE(found == 0);
}

TEST_CASE("truncated outputs return the full total and the smallest indices ascending", "[spatial]") {
  const u32 n = 2000;
  const Built b = make(gen(0, n, 321), 4);
  const Boxes& items = b.items;
  const Box qs[] = {{-1100, -1100, 1100, 1100}, {-300, -300, 300, 300}, {-200, -200, 200, 200}};
  for (const Box& q : qs) {
    const std::vector<u32> want = brute_box(items, q);
    const u32 total = static_cast<u32>(want.size());
    REQUIRE(total > 10);
    const u32 caps[] = {0, 1, 2, 3, 7, total / 2, total - 1, total, total + 5};
    for (u32 cap : caps) {
      std::vector<u32> out(cap + 4, kSentinel);
      u32 found = 0;
      const Status st = query_box(items.data(), n, b.nodes.data(), b.node_count, b.order.data(), q, cap ? out.data() : nullptr, cap, &found);
      REQUIRE(found == total);
      REQUIRE(st == (cap < total ? Status::BufferTooSmall : Status::Ok));
      const u32 filled = std::min(cap, total);
      for (u32 i = 0; i < filled; ++i) REQUIRE(out[i] == want[i]);
      for (u32 i = filled; i < out.size() && cap > 0; ++i) REQUIRE(out[i] == kSentinel);  // nothing written past the capacity
    }
  }
  // query_point truncation: all identical boxes contain the point.
  Built same = make(gen(1, 100, 0), 4);
  std::vector<u32> out(5, kSentinel);
  u32 found = 0;
  REQUIRE(query_point(same.items.data(), 100, same.nodes.data(), same.node_count, same.order.data(), 7, 7, out.data(), 4, &found) ==
          Status::BufferTooSmall);
  REQUIRE(found == 100);
  for (u32 i = 0; i < 4; ++i) REQUIRE(out[i] == i);
  REQUIRE(out[4] == kSentinel);
}

TEST_CASE("query argument checks and their order", "[spatial]") {
  const Built b = make(gen(0, 100, 3), 4);
  std::vector<u32> out(100, kSentinel);
  u32 found = 55;
  const Box ok{0, 0, 10, 10};
  const Box reversed{5, 0, 4, 10};
  const Box reversed_y{0, 5, 10, 4};
  const u32 n = 100;
  const Node* nd = b.nodes.data();
  // NullBuffer: required pointers while counts are positive; found is always required.
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, b.order.data(), ok, out.data(), 100, nullptr) == Status::NullBuffer);
  REQUIRE(query_box(nullptr, n, nd, b.node_count, b.order.data(), ok, out.data(), 100, &found) == Status::NullBuffer);
  REQUIRE(found == 0);
  REQUIRE(query_box(b.items.data(), n, nullptr, b.node_count, b.order.data(), ok, out.data(), 100, &found) == Status::NullBuffer);
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, nullptr, ok, out.data(), 100, &found) == Status::NullBuffer);
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, b.order.data(), ok, nullptr, 5, &found) == Status::NullBuffer);
  REQUIRE(query_point(b.items.data(), n, nd, b.node_count, b.order.data(), 0, 0, nullptr, 5, &found) == Status::NullBuffer);
  REQUIRE(query_point(b.items.data(), n, nd, b.node_count, b.order.data(), 0, 0, out.data(), 5, nullptr) == Status::NullBuffer);
  // A null out with capacity 0 is fine.
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, b.order.data(), Box{-5000, -5000, 5000, 5000}, nullptr, 0, &found) ==
          Status::BufferTooSmall);
  REQUIRE(found == 100);
  // BadBox only for reversed bounds; magnitude is not checked for queries.
  found = 55;
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, b.order.data(), reversed, out.data(), 100, &found) == Status::BadBox);
  REQUIRE(found == 0);
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, b.order.data(), reversed_y, out.data(), 100, &found) == Status::BadBox);
  REQUIRE(query_box(b.items.data(), n, nd, b.node_count, b.order.data(), Box{INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX}, out.data(), 100,
                    &found) == Status::Ok);
  REQUIRE(found == 100);
  // BadBox comes before BadIndex, NullBuffer before BadBox.
  REQUIRE(query_box(b.items.data(), n, nd, 0, b.order.data(), reversed, out.data(), 100, &found) == Status::BadBox);
  REQUIRE(query_box(b.items.data(), n, nd, 0, b.order.data(), ok, out.data(), 100, &found) == Status::BadIndex);
  REQUIRE(query_box(nullptr, n, nd, b.node_count, b.order.data(), reversed, out.data(), 100, &found) == Status::NullBuffer);
  // BadIndex comes before BufferTooSmall.
  REQUIRE(query_box(b.items.data(), n, nd, 0, b.order.data(), Box{-5000, -5000, 5000, 5000}, out.data(), 1, &found) == Status::BadIndex);
  REQUIRE(found == 0);
}

TEST_CASE("nearest matches brute force, with ties to the lowest index", "[spatial]") {
  const u32 leaves[] = {1, 4, 16};
  for (int kind = 0; kind <= 6; ++kind)
    for (u32 leaf : leaves) {
      const u32 n = kind == 2 ? 300 : 1500;
      const Built b = make(gen(kind, n, 60u + static_cast<u64>(kind)), leaf);
      const QuerySet qs = make_queries(60u + static_cast<u64>(kind));
      std::vector<std::pair<std::int32_t, std::int32_t>> pts = qs.points;
      Lcg r{9};
      for (int i = 0; i < 300; ++i) pts.push_back({r.rng(-1300, 1300), r.rng(-1300, 1300)});
      for (const auto& p : pts) {
        const Near want = brute_nearest(b.items, p.first, p.second);
        u32 idx = 12345;
        u64 d2 = 12345;
        bool f = false;
        REQUIRE(nearest(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), p.first, p.second, &idx, &d2, &f) == Status::Ok);
        REQUIRE(f == want.found);
        REQUIRE(idx == want.index);
        REQUIRE(d2 == want.d2);
      }
    }
  // Ties: identical boxes always give index 0; a point inside nested boxes gives distance 0 and the lowest index.
  Built same = make(gen(1, 77, 0), 4);
  u32 idx = 9;
  u64 d2 = 9;
  bool f = false;
  REQUIRE(nearest(same.items.data(), 77, same.nodes.data(), same.node_count, same.order.data(), 100, 100, &idx, &d2, &f) == Status::Ok);
  REQUIRE(f);
  REQUIRE(idx == 0);
  REQUIRE(d2 == 90u * 90u * 2u);
  Built nest = make(gen(2, 60, 0), 2);
  REQUIRE(nearest(nest.items.data(), 60, nest.nodes.data(), nest.node_count, nest.order.data(), 3, 3, &idx, &d2, &f) == Status::Ok);
  REQUIRE(idx == 3);  // box 3 is (-3,-3,3,3) and contains (3,3); box 2 is at distance 1 per axis; box 3 is the lowest containing it
  REQUIRE(d2 == 0);
  // Touching gives zero. Two boxes at the same distance on opposite sides: the lower index wins whatever the tree shape.
  Built two = make(Boxes{{10, 0, 10, 0}, {-10, 0, -10, 0}, {0, 10, 0, 10}, {0, -10, 0, -10}, {50, 50, 60, 60}}, 1);
  REQUIRE(nearest(two.items.data(), 5, two.nodes.data(), two.node_count, two.order.data(), 0, 0, &idx, &d2, &f) == Status::Ok);
  REQUIRE(idx == 0);
  REQUIRE(d2 == 100);
  // Empty set: Ok, found false.
  idx = 9;
  d2 = 9;
  f = true;
  REQUIRE(nearest(nullptr, 0, nullptr, 0, nullptr, 1, 1, &idx, &d2, &f) == Status::Ok);
  REQUIRE_FALSE(f);
  REQUIRE(idx == 0);
  REQUIRE(d2 == 0);
}

TEST_CASE("nearest distance bound: int32 points against boxes at the coordinate limit", "[spatial]") {
  // dx = dy = 2^31 + 2^28 = 2415919104, so d2 = 2 * dx^2 = 11673330234144325632 < 2^64 (constant from the Python reference).
  Built b = make(Boxes{{kK, kK, kK, kK}}, 1);
  u32 idx = 0;
  u64 d2 = 0;
  bool f = false;
  REQUIRE(nearest(b.items.data(), 1, b.nodes.data(), b.node_count, b.order.data(), INT32_MIN, INT32_MIN, &idx, &d2, &f) == Status::Ok);
  REQUIRE(f);
  REQUIRE(d2 == 11673330234144325632ull);
  // Largest possible per-axis gap on one axis: INT32_MAX against -2^28.
  Built c = make(Boxes{{-kK, 0, -kK, 0}}, 1);
  REQUIRE(nearest(c.items.data(), 1, c.nodes.data(), c.node_count, c.order.data(), INT32_MAX, 0, &idx, &d2, &f) == Status::Ok);
  REQUIRE(d2 == 2415919103ull * 2415919103ull);
  // Argument checks: result pointers are always required; nothing is written on error.
  idx = 7;
  d2 = 7;
  REQUIRE(nearest(b.items.data(), 1, b.nodes.data(), b.node_count, b.order.data(), 0, 0, nullptr, &d2, &f) == Status::NullBuffer);
  REQUIRE(nearest(b.items.data(), 1, b.nodes.data(), b.node_count, b.order.data(), 0, 0, &idx, nullptr, &f) == Status::NullBuffer);
  REQUIRE(nearest(b.items.data(), 1, b.nodes.data(), b.node_count, b.order.data(), 0, 0, &idx, &d2, nullptr) == Status::NullBuffer);
  REQUIRE(nearest(nullptr, 1, b.nodes.data(), b.node_count, b.order.data(), 0, 0, &idx, &d2, &f) == Status::NullBuffer);
  REQUIRE(nearest(b.items.data(), 1, b.nodes.data(), 0, b.order.data(), 0, 0, &idx, &d2, &f) == Status::BadIndex);
  REQUIRE(idx == 7);
  REQUIRE(d2 == 7);
}

TEST_CASE("BadIndex: cheap guards, lazy checks and visit budgets", "[spatial]") {
  const u32 n = 100;
  const Built good = make(gen(0, n, 8), 4);
  const Box all{-5000, -5000, 5000, 5000};
  std::vector<u32> out(n + 1);
  const auto qbox = [&](const std::vector<Node>& nodes, u32 nc, const std::vector<u32>& order, u32* found) {
    return query_box(good.items.data(), n, nodes.data(), nc, order.data(), all, out.data(), n + 1, found);
  };
  u32 found = 5;
  REQUIRE(qbox(good.nodes, good.node_count, good.order, &found) == Status::Ok);
  REQUIRE(found == n);

  // Guards on counts and on the root.
  REQUIRE(qbox(good.nodes, 0, good.order, &found) == Status::BadIndex);
  REQUIRE(found == 0);
  REQUIRE(qbox(good.nodes, 2 * n, good.order, &found) == Status::BadIndex);
  REQUIRE(qbox(good.nodes, 1, good.order, &found) == Status::BadIndex);  // inner root with a single node
  {
    std::vector<Node> nd = good.nodes;
    nd[0].first = 2;
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    nd = good.nodes;
    nd[0].right = 0;
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    nd = good.nodes;
    nd[0].right = good.node_count;
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
  }
  // Lazy checks while walking.
  u32 inner = 0, leaf_node = 0;
  for (u32 i = 1; i < good.node_count; ++i) {
    if (good.nodes[i].count == 0 && inner == 0) inner = i;
    if (good.nodes[i].count > 0 && leaf_node == 0) leaf_node = i;
  }
  REQUIRE(inner > 0);
  REQUIRE(leaf_node > 0);
  {
    std::vector<Node> nd = good.nodes;
    nd[inner].right = inner;  // a back edge would loop forever without the checks
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    REQUIRE(found == 0);
    nd = good.nodes;
    nd[inner].first = 0;
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    nd = good.nodes;
    nd[inner].right = good.node_count + 3;
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    nd = good.nodes;
    nd[leaf_node].first = n;  // first + count beyond the items
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    nd = good.nodes;
    nd[leaf_node].count = n + 1;
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
    // Sharing a subtree twice (a DAG) exceeds the visit budget.
    nd = good.nodes;
    nd[1].right = nd[0].right;
    REQUIRE(nd[1].count == 0);
    REQUIRE(qbox(nd, good.node_count, good.order, &found) == Status::BadIndex);
  }
  {
    std::vector<u32> od = good.order;
    od[0] = n;  // index outside the items
    REQUIRE(qbox(good.nodes, good.node_count, od, &found) == Status::BadIndex);
    od = good.order;
    for (u32& v : od) v = n + 7;
    u32 idx = 0;
    u64 d2 = 0;
    bool f = false;
    REQUIRE(nearest(good.items.data(), n, good.nodes.data(), good.node_count, od.data(), 0, 0, &idx, &d2, &f) == Status::BadIndex);
  }
  {
    // nearest sees the same damage: a root whose right child points back.
    std::vector<Node> nd = good.nodes;
    nd[0].right = 1;
    u32 idx = 0;
    u64 d2 = 0;
    bool f = false;
    REQUIRE(nearest(good.items.data(), n, nd.data(), good.node_count, good.order.data(), 0, 0, &idx, &d2, &f) == Status::BadIndex);
    REQUIRE(nearest(good.items.data(), n, good.nodes.data(), 0, good.order.data(), 0, 0, &idx, &d2, &f) == Status::BadIndex);
    REQUIRE(nearest(good.items.data(), n, good.nodes.data(), 2 * n, good.order.data(), 0, 0, &idx, &d2, &f) == Status::BadIndex);
  }
  // An empty index must have node_count 0.
  REQUIRE(query_box(nullptr, 0, nullptr, 1, nullptr, all, nullptr, 0, &found) == Status::BadIndex);
  // Limitation: an index that is internally consistent but was built from other items is accepted (no crash, plausible result).
  const Boxes other = gen(0, n, 777);
  REQUIRE(query_box(other.data(), n, good.nodes.data(), good.node_count, good.order.data(), all, out.data(), n + 1, &found) == Status::Ok);
  REQUIRE(found == n);
}

TEST_CASE("golden digests from the independent reference", "[spatial]") {
  for (const Golden& g : kGolden) {
    INFO("kind " << g.kind << " n " << g.n << " leaf " << g.leaf);
    const Built b = make(gen(g.kind, g.n, g.seed), g.leaf);
    REQUIRE(b.st == Status::Ok);
    REQUIRE(expect_digests(g, b));
  }
}

TEST_CASE("performance: 2^16 boxes build and many queries (sanitizer safe size)", "[spatial][perf]") {
  const u32 n = 1u << 16;
  const Built b = make(gen(0, n, 18), 16);
  REQUIRE(b.st == Status::Ok);
  REQUIRE(check_tree(b));
  Lcg r{3};
  std::vector<u32> out(n);
  for (int i = 0; i < 20000; ++i) {
    const std::int32_t x = r.rng(-1100, 1100), y = r.rng(-1100, 1100);
    const Box q{x, y, x + r.rng(0, 60), y + r.rng(0, 60)};
    u32 found = 0;
    REQUIRE(query_box(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), q, out.data(), 64, &found) <= Status::BufferTooSmall);
    if (i < 100) REQUIRE(found == brute_box(b.items, q).size());
    u32 idx = 0;
    u64 d2 = 0;
    bool f = false;
    REQUIRE(nearest(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), x, y, &idx, &d2, &f) == Status::Ok);
    if (i < 50) {
      const Near want = brute_nearest(b.items, x, y);
      REQUIRE(idx == want.index);
      REQUIRE(d2 == want.d2);
    }
  }
  // Worst case for the output heap: a query that matches everything with a small and a large capacity.
  u32 found = 0;
  std::vector<u32> small(5);
  REQUIRE(query_box(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), Box{-2000, -2000, 2000, 2000}, small.data(), 5, &found) ==
          Status::BufferTooSmall);
  REQUIRE(found == n);
  for (u32 i = 0; i < 5; ++i) REQUIRE(small[i] == i);
  REQUIRE(query_box(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), Box{-2000, -2000, 2000, 2000}, out.data(), n, &found) ==
          Status::Ok);
  for (u32 i = 0; i < n; ++i) REQUIRE(out[i] == i);
}

#if !defined(__SANITIZE_ADDRESS__)
TEST_CASE("performance: 2^20 boxes (unsanitized builds only)", "[spatial][perf]") {
  const u32 n = kMaxItems;
  const Built b = make(gen(0, n, kGoldenBig.seed), kGoldenBig.leaf);
  REQUIRE(b.st == Status::Ok);
  REQUIRE(b.node_count == node_bound(n, kGoldenBig.leaf));
  REQUIRE(check_tree(b));
  REQUIRE(expect_digests(kGoldenBig, b));
  Lcg r{4};
  std::vector<u32> out(n);
  for (int i = 0; i < 5000; ++i) {
    const std::int32_t x = r.rng(-1100, 1100), y = r.rng(-1100, 1100);
    const Box q{x, y, x + r.rng(0, 30), y + r.rng(0, 30)};
    u32 found = 0;
    REQUIRE(query_box(b.items.data(), n, b.nodes.data(), b.node_count, b.order.data(), q, out.data(), 256, &found) <= Status::BufferTooSmall);
    if (i < 20) REQUIRE(found == brute_box(b.items, q).size());
  }
}
#endif
