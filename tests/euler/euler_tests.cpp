// Tests for core/euler. Evidence label: unit tests only. Expected kinds and required path starts come from tools/euler_ref.py (brute force over edge
// orders for every small graph, no degree rule and no Hierholzer step; the 15 max-size patterns are known by construction and confirmed by degree
// counts and union-find). Every success test uses a scratch array of exactly 6n + 3m + 8 words (filled with garbage) and exact-size outputs pre-filled
// with sentinels. Which trail is returned is not specified, so no test compares it with anything but the graph: every returned trail is checked edge
// by edge. Tiny or null buffers appear only for failures and empty graphs. No test asserts any scratch value after a call. The max-size cases are
// sampled patterns, not exhaustive. Time is not measured.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "euler.h"

using lumora::euler::euler_scratch_words;
using lumora::euler::euler_trail;
using lumora::euler::EulerKind;
using lumora::euler::EulerResult;
using lumora::euler::kMaxEdges;
using lumora::euler::kMaxNodes;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

namespace {

// groups: instances, digest lo, digest hi
const u32 kGroups[24] = {
    7u, 3428071969u, 3436248539u,
    210u, 686822747u, 1351594459u,
    2002u, 3625683423u, 2146852271u,
    4845u, 1392314591u, 599159457u,
    5461u, 1197036907u, 3184995852u,
    7381u, 4272276229u, 216068789u,
    4369u, 823705289u, 656794491u,
    4096u, 221426653u, 4210308784u,
};
const u32 kBattery[5] = {3983036438u, 3910091260u, 2602u, 5981u, 11417u};
const u32 kMid[5] = {3286790792u, 3684196958u, 67u, 53u, 80u};
// max-size patterns: pid, n, m, kind, start
const u32 kBig[75] = {
    0u, 65536u, 0u, 2u, 4294967295u,
    1u, 65536u, 65535u, 1u, 0u,
    2u, 65536u, 262144u, 2u, 4294967295u,
    3u, 65536u, 262144u, 2u, 4294967295u,
    4u, 1u, 262144u, 2u, 4294967295u,
    5u, 2u, 262144u, 0u, 4294967295u,
    6u, 2u, 262144u, 2u, 4294967295u,
    7u, 2u, 262144u, 1u, 0u,
    8u, 65536u, 65536u, 0u, 4294967295u,
    9u, 65536u, 1050u, 0u, 4294967295u,
    10u, 65536u, 262144u, 1u, 44776u,
    11u, 512u, 262144u, 2u, 4294967295u,
    12u, 65536u, 131070u, 2u, 4294967295u,
    13u, 65536u, 1001u, 1u, 0u,
    14u, 65536u, 262144u, 0u, 4294967295u,
};

const u32 kGarbage = 0xA5A5A5A5u;
const u32 kSentinel = 0xDEADBEEFu;
const EulerKind kKindSentinel = static_cast<EulerKind>(0x7Eu);
const u32 kNone = 0xFFFFFFFFu;
const u64 kFnvOff = 0xCBF29CE484222325ull;
const u64 kFnvPrime = 0x100000001B3ull;

u64 fold_bytes(u64 h, u64 v, int nbytes) {
  for (int i = 0; i < nbytes; ++i) {
    h ^= (v & 0xFFu);
    v >>= 8;
    h *= kFnvPrime;
  }
  return h;
}

struct Graph {
  u32 n = 0;
  std::vector<u32> eu, ev;
  u32 edges() const { return static_cast<u32>(eu.size()); }
  void add(u32 a, u32 b) {
    eu.push_back(a);
    ev.push_back(b);
  }
};

struct Run {
  EulerResult res;
  EulerKind kind;
  u32 len;
  std::vector<u32> out;
};

Run run(const Graph& g, u32 fill = kGarbage) {
  const u32 m = g.edges();
  std::vector<u32> scratch(6ull * g.n + 3ull * m + 8ull, fill);
  Run r;
  r.out.assign(m, kSentinel);
  r.kind = kKindSentinel;
  r.len = kSentinel;
  r.res = euler_trail(g.n, m, g.eu.data(), g.ev.data(), scratch.data(), static_cast<u32>(scratch.size()), r.out.data(), &r.len, &r.kind);
  return r;
}

u32 kind_code(EulerKind k) { return static_cast<u32>(k); }

struct Lcg {
  u64 s;
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 33);
  }
  u32 rnd(u32 k) { return next() % k; }
};

// Independent classification used where no reference digest exists (relabelled copies): in/out degree counts and union-find. It is not a trail search.
struct Class {
  u32 kind;   // 0 None, 1 Path, 2 Circuit
  u32 start;  // required start of a Path, else kNone
};

Class classify(const Graph& g) {
  const u32 m = g.edges();
  if (m == 0) return {2, kNone};
  std::vector<u32> par(g.n), od(g.n, 0), id(g.n, 0);
  for (u32 i = 0; i < g.n; ++i) par[i] = i;
  auto find = [&](u32 x) {
    while (par[x] != x) {
      par[x] = par[par[x]];
      x = par[x];
    }
    return x;
  };
  for (u32 i = 0; i < m; ++i) {
    ++od[g.eu[i]];
    ++id[g.ev[i]];
    par[find(g.eu[i])] = find(g.ev[i]);
  }
  const u32 root = find(g.eu[0]);
  for (u32 i = 0; i < m; ++i) {
    if (find(g.eu[i]) != root) return {0, kNone};
  }
  u32 src = 0, nsrc = 0, nsnk = 0;
  for (u32 v = 0; v < g.n; ++v) {
    if (od[v] == id[v]) continue;
    if (od[v] == id[v] + 1u) {
      ++nsrc;
      src = v;
    } else if (id[v] == od[v] + 1u) {
      ++nsnk;
    } else {
      return {0, kNone};
    }
  }
  if (nsrc == 0 && nsnk == 0) return {2, kNone};
  if (nsrc == 1 && nsnk == 1) return {1, src};
  return {0, kNone};
}

// Certificate checker. Circuit or Path: out_len == m, out_edges[0 .. m) uses every edge exactly once, the head of each edge is the tail of the next, a
// Circuit ends where it starts, a Path ends elsewhere and starts at `start`. None: len 0 and out_edges untouched.
const char* certify(const Graph& g, const Run& r, u32 kind, u32 start) {
  const u32 m = g.edges();
  if (r.res != EulerResult::Ok) return "result not Ok";
  if (kind_code(r.kind) != kind) return "wrong kind";
  if (kind == 0) {
    if (r.len != 0) return "len on None";
    for (u32 x : r.out) {
      if (x != kSentinel) return "edges written on None";
    }
    return "";
  }
  if (r.len != m) return "len != m";
  std::vector<unsigned char> seen(m, 0);
  for (u32 i = 0; i < m; ++i) {
    if (r.out[i] >= m || seen[r.out[i]]) return "edge repeated or out of range";
    seen[r.out[i]] = 1;
  }
  if (m == 0) return "";
  for (u32 i = 0; i + 1u < m; ++i) {
    if (g.ev[r.out[i]] != g.eu[r.out[i + 1u]]) return "trail broken";
  }
  const u32 first = g.eu[r.out[0]];
  const u32 last = g.ev[r.out[m - 1u]];
  if (kind == 2) return first == last ? "" : "circuit not closed";
  if (first == last) return "path is closed";
  return first == start ? "" : "path starts at the wrong node";
}

bool certified(const Graph& g, const Run& r, u32 kind, u32 start) { return certify(g, r, kind, start)[0] == '\0'; }

u64 fold_expected(u64 h, u32 kind, u32 start) {
  h = fold_bytes(h, kind, 1);
  return fold_bytes(h, start, 4);
}

// Generators identical to tools/euler_ref.py.
std::vector<std::pair<u32, u32>> walk(Lcg& r, u32 lo, u32 cnt, u32 m, bool closed) {
  std::vector<std::pair<u32, u32>> e;
  const u32 v0 = lo + r.rnd(cnt);
  u32 cur = v0;
  for (u32 i = 0; i < m; ++i) {
    u32 nxt = lo + r.rnd(cnt);
    if (closed && i + 1u == m) nxt = v0;
    e.push_back({cur, nxt});
    cur = nxt;
  }
  return e;
}

void shuffle(Lcg& r, std::vector<std::pair<u32, u32>>& e) {
  for (u32 i = static_cast<u32>(e.size()); i-- > 1;) {
    const u32 j = r.rnd(i + 1u);
    std::swap(e[i], e[j]);
  }
}

Graph from_pairs(u32 n, const std::vector<std::pair<u32, u32>>& e) {
  Graph g;
  g.n = n;
  for (const auto& p : e) g.add(p.first, p.second);
  return g;
}

Graph gen(Lcg& r, u32 n, u32 m, u32 mode) {
  std::vector<std::pair<u32, u32>> e;
  if (mode == 0) {
    for (u32 i = 0; i < m; ++i) {
      const u32 a = r.rnd(n);
      const u32 b = r.rnd(n);
      e.push_back({a, b});
    }
  } else if (mode == 1 || mode == 2) {
    e = walk(r, 0, n, m, mode == 2);
    shuffle(r, e);
  } else if (mode == 3) {
    const bool closed = r.rnd(2) == 1;
    e = walk(r, 0, n, m, closed);
    shuffle(r, e);
    if (m > 0) {
      const u32 i = r.rnd(m);
      e[i].second = r.rnd(n);
    }
  } else {
    const u32 a = r.rnd(m + 1u);
    const u32 h = n / 2u;
    e = walk(r, 0, h, a, true);
    const auto e2 = walk(r, h, n - h, m - a, true);
    e.insert(e.end(), e2.begin(), e2.end());
    shuffle(r, e);
  }
  return from_pairs(n, e);
}

// Enumerations identical to tools/euler_ref.py: multisets (combinations with replacement over the n*n edge kinds) or ordered lists.
u64 enumerate_group(u32 n, u32 mmax, bool ordered, u32& count) {
  const u32 KN = n * n;
  u64 h = kFnvOff;
  count = 0;
  for (u32 m = 0; m <= mmax; ++m) {
    std::vector<u32> idx(m, 0);
    for (;;) {
      Graph g;
      g.n = n;
      for (u32 i = 0; i < m; ++i) g.add(idx[i] / n, idx[i] % n);
      ++count;
      const Class c = classify(g);
      const Run r = run(g);
      REQUIRE(r.res == EulerResult::Ok);
      REQUIRE(certified(g, r, c.kind, c.start));
      h = fold_expected(h, kind_code(r.kind), r.kind == EulerKind::Path ? g.eu[r.out[0]] : kNone);
      if (ordered) {
        u32 i = m;
        while (i > 0 && idx[i - 1u] == KN - 1u) {
          idx[i - 1u] = 0;
          --i;
        }
        if (i == 0) break;
        ++idx[i - 1u];
      } else {
        u32 i = m;
        while (i > 0 && idx[i - 1u] == KN - 1u) --i;
        if (i == 0) break;
        const u32 val = idx[i - 1u] + 1u;
        for (u32 j = i - 1u; j < m; ++j) idx[j] = val;
      }
    }
  }
  return h;
}

}  // namespace

namespace {

void group_check(const char*, u32 n, u32 mmax, bool ordered, u32 gi) {
  u32 count = 0;
  const u64 h = enumerate_group(n, mmax, ordered, count);
  CHECK(count == kGroups[3u * gi]);
  CHECK(static_cast<u32>(h) == kGroups[3u * gi + 1u]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[3u * gi + 2u]);
}

Graph big_graph(u32 pid) {
  Lcg r{9000ull + pid};
  const u32 NN = kMaxNodes, MM = kMaxEdges;
  std::vector<std::pair<u32, u32>> e;
  u32 n = NN;
  switch (pid) {
    case 0: break;
    case 1:
      for (u32 i = 0; i + 1u < NN; ++i) e.push_back({i, i + 1u});
      shuffle(r, e);
      break;
    case 2:
      for (u32 i = 0; i < NN; ++i) e.push_back({i, (i + 1u) % NN});
      for (u32 i = 0; i < MM - NN; ++i) {
        const u32 x = r.rnd(NN);
        e.push_back({x, x});
      }
      shuffle(r, e);
      break;
    case 3:
      e = walk(r, 0, NN, MM, true);
      shuffle(r, e);
      break;
    case 4:
      n = 1;
      e.assign(MM, {0u, 0u});
      break;
    case 5:
      n = 2;
      e.assign(131073u, {0u, 1u});
      e.insert(e.end(), 131071u, {1u, 0u});
      break;
    case 6:
      n = 2;
      e.assign(131072u, {0u, 1u});
      e.insert(e.end(), 131072u, {1u, 0u});
      shuffle(r, e);
      break;
    case 7:
      n = 2;
      e.assign(131072u, {0u, 1u});
      e.insert(e.end(), 131071u, {1u, 0u});
      e.push_back({0u, 0u});
      shuffle(r, e);
      break;
    case 8: {
      const u32 h = NN / 2u;
      for (u32 i = 0; i < h; ++i) e.push_back({i, (i + 1u) % h});
      for (u32 i = 0; i < h; ++i) e.push_back({h + i, h + (i + 1u) % h});
      shuffle(r, e);
      break;
    }
    case 9:
      for (u32 i = 0; i < 1000; ++i) e.push_back({i, i + 1u});
      for (u32 i = 0; i < 50; ++i) e.push_back({2000u + i, 2000u + (i + 1u) % 50u});
      shuffle(r, e);
      break;
    case 10:
      e = walk(r, 0, NN, MM, false);
      shuffle(r, e);
      break;
    case 11:
      n = 512;
      for (u32 a = 0; a < 512; ++a) {
        for (u32 b = 0; b < 512; ++b) e.push_back({a, b});
      }
      break;
    case 12:
      for (u32 i = 1; i < NN; ++i) e.push_back({0u, i});
      for (u32 i = 1; i < NN; ++i) e.push_back({i, 0u});
      shuffle(r, e);
      break;
    case 13:
      for (u32 i = 0; i < 1000; ++i) e.push_back({i, (i + 1u) % 1000u});
      e.push_back({0u, 1u});
      shuffle(r, e);
      break;
    default:
      for (u32 i = 0; i < MM; ++i) {
        const u32 a = r.rnd(NN);
        const u32 b = r.rnd(NN);
        e.push_back({a, b});
      }
      break;
  }
  return from_pairs(n, e);
}

}  // namespace

TEST_CASE("euler_scratch_words", "[euler]") {
  u32 w = 7;
  CHECK(euler_scratch_words(0, 0, &w) == EulerResult::Ok);
  CHECK(w == 8u);
  CHECK(euler_scratch_words(1, 0, &w) == EulerResult::Ok);
  CHECK(w == 14u);
  CHECK(euler_scratch_words(3, 5, &w) == EulerResult::Ok);
  CHECK(w == 41u);
  CHECK(euler_scratch_words(kMaxNodes, kMaxEdges, &w) == EulerResult::Ok);
  CHECK(w == 6u * 65536u + 3u * 262144u + 8u);
  CHECK(euler_scratch_words(0, kMaxEdges, &w) == EulerResult::Ok);
  CHECK(w == 3u * 262144u + 8u);
  w = 99;
  CHECK(euler_scratch_words(kMaxNodes + 1u, 0, &w) == EulerResult::TooLarge);
  CHECK(w == 99u);
  CHECK(euler_scratch_words(0, kMaxEdges + 1u, &w) == EulerResult::TooLarge);
  CHECK(w == 99u);
  CHECK(euler_scratch_words(0xFFFFFFFFu, 0xFFFFFFFFu, &w) == EulerResult::TooLarge);
  CHECK(w == 99u);
  CHECK(euler_scratch_words(1, 1, nullptr) == EulerResult::NullArgument);
  CHECK(euler_scratch_words(kMaxNodes + 1u, 0, nullptr) == EulerResult::NullArgument);
}

TEST_CASE("every multiset of 0 to 6 self-loops on 1 node (7 graphs): digest group", "[euler]") { group_check("n1", 1, 6, false, 0); }
TEST_CASE("every multiset of 0 to 6 edges on 2 nodes (4 edge kinds (u, v), 210 graphs): digest group", "[euler]") { group_check("n2", 2, 6, false, 1); }
TEST_CASE("every multiset of 0 to 5 edges on 3 nodes (9 edge kinds (u, v), 2002 graphs): digest group", "[euler]") { group_check("n3", 3, 5, false, 2); }
TEST_CASE("every multiset of 0 to 4 edges on 4 nodes (16 edge kinds (u, v), 4845 graphs): digest group", "[euler]") { group_check("n4", 4, 4, false, 3); }
TEST_CASE("every ordered edge list of 0 to 6 edges on 2 nodes (5461 lists): digest group", "[euler]") { group_check("o2", 2, 6, true, 4); }
TEST_CASE("every ordered edge list of 0 to 4 edges on 3 nodes (7381 lists): digest group", "[euler]") { group_check("o3", 3, 4, true, 5); }
TEST_CASE("every ordered edge list of 0 to 3 edges on 4 nodes (4369 lists): digest group", "[euler]") { group_check("o4", 4, 3, true, 6); }

TEST_CASE("every set of the 12 non-loop edges on 4 nodes (all 2^12 = 4096 subsets, edges in (u, v) order): digest group", "[euler]") {
  std::vector<std::pair<u32, u32>> K;
  for (u32 a = 0; a < 4; ++a) {
    for (u32 b = 0; b < 4; ++b) {
      if (a != b) K.push_back({a, b});
    }
  }
  u64 h = kFnvOff;
  for (u32 mask = 0; mask < 4096u; ++mask) {
    Graph g;
    g.n = 4;
    for (u32 i = 0; i < 12; ++i) {
      if ((mask >> i) & 1u) g.add(K[i].first, K[i].second);
    }
    const Class c = classify(g);
    const Run r = run(g);
    REQUIRE(r.res == EulerResult::Ok);
    REQUIRE(certified(g, r, c.kind, c.start));
    h = fold_expected(h, kind_code(r.kind), r.kind == EulerKind::Path ? g.eu[r.out[0]] : kNone);
  }
  CHECK(kGroups[21] == 4096u);
  CHECK(static_cast<u32>(h) == kGroups[22]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[23]);
}

TEST_CASE("random battery: 20000 small graphs by digest", "[euler]") {
  Lcg r{7171};
  u64 h = kFnvOff;
  u32 kc[3] = {0, 0, 0};
  for (u32 k = 0; k < 20000u; ++k) {
    const u32 n = 1u + r.rnd(6);
    const u32 m = r.rnd(8);
    const u32 mode = r.rnd(4);
    const Graph g = gen(r, n, m, mode);
    const Class c = classify(g);
    const Run run1 = run(g);
    REQUIRE(run1.res == EulerResult::Ok);
    REQUIRE(certified(g, run1, c.kind, c.start));
    ++kc[kind_code(run1.kind)];
    h = fold_expected(h, kind_code(run1.kind), run1.kind == EulerKind::Path ? g.eu[run1.out[0]] : kNone);
  }
  CHECK(static_cast<u32>(h) == kBattery[0]);
  CHECK(static_cast<u32>(h >> 32) == kBattery[1]);
  CHECK(kc[0] == kBattery[2]);
  CHECK(kc[1] == kBattery[3]);
  CHECK(kc[2] == kBattery[4]);
}

TEST_CASE("mid-size graphs (200 graphs, 2 to 9 nodes, 0 to 12 edges, including two disconnected circuits): digest and certificate on every graph", "[euler]") {
  Lcg r{7272};
  u64 h = kFnvOff;
  u32 kc[3] = {0, 0, 0};
  for (u32 k = 0; k < 200u; ++k) {
    const u32 n = 2u + r.rnd(8);
    const u32 m = r.rnd(13);
    const u32 mode = r.rnd(5);
    const Graph g = gen(r, n, m, mode);
    const Class c = classify(g);
    const Run run1 = run(g);
    REQUIRE(run1.res == EulerResult::Ok);
    REQUIRE(certified(g, run1, c.kind, c.start));
    ++kc[kind_code(run1.kind)];
    h = fold_expected(h, kind_code(run1.kind), run1.kind == EulerKind::Path ? g.eu[run1.out[0]] : kNone);
  }
  CHECK(static_cast<u32>(h) == kMid[0]);
  CHECK(static_cast<u32>(h >> 32) == kMid[1]);
  CHECK(kc[0] == kMid[2]);
  CHECK(kc[1] == kMid[3]);
  CHECK(kc[2] == kMid[4]);
}

TEST_CASE("max-size graphs: 15 patterns by kind and required start, each checked by the certificate checker", "[euler]") {
  bool seen[3] = {false, false, false};
  for (u32 pid = 0; pid < 15u; ++pid) {
    const u32* row = &kBig[5u * pid];
    CAPTURE(pid);
    REQUIRE(row[0] == pid);
    const Graph g = big_graph(pid);
    REQUIRE(g.n == row[1]);
    REQUIRE(g.edges() == row[2]);
    const Run r = run(g);
    REQUIRE(r.res == EulerResult::Ok);
    REQUIRE(certified(g, r, row[3], row[4]));
    seen[row[3]] = true;
  }
  CHECK(seen[0]);
  CHECK(seen[1]);
  CHECK(seen[2]);
}

TEST_CASE("hand-checked graphs", "[euler]") {
  auto go = [](u32 n, std::vector<std::pair<u32, u32>> e, u32 kind, u32 start) {
    const Graph g = from_pairs(n, e);
    const Run r = run(g);
    CHECK(certified(g, r, kind, start));
  };
  go(1, {{0, 0}}, 2, kNone);                                   // one self-loop
  go(1, {{0, 0}, {0, 0}, {0, 0}}, 2, kNone);                   // repeated self-loops
  go(5, {}, 2, kNone);                                         // no edges, isolated nodes only
  go(2, {{0, 1}}, 1, 0);                                       // single edge is a path
  go(2, {{0, 1}, {1, 0}}, 2, kNone);                           // two-cycle
  go(2, {{0, 1}, {0, 1}}, 0, kNone);                           // parallel edges one way: source off by 2
  go(2, {{0, 1}, {0, 1}, {1, 0}}, 1, 0);                       // parallel edges, path from 0
  go(3, {{0, 1}, {1, 2}}, 1, 0);                               // chain
  go(3, {{1, 2}, {0, 1}}, 1, 0);                               // chain, edges listed backwards
  go(3, {{0, 1}, {1, 2}, {2, 0}}, 2, kNone);                   // triangle
  go(4, {{0, 1}, {1, 2}, {2, 0}}, 2, kNone);                   // triangle plus an isolated node
  go(4, {{0, 1}, {1, 0}, {2, 3}, {3, 2}}, 0, kNone);           // two circuits, not connected
  go(4, {{0, 1}, {2, 3}}, 0, kNone);                           // two paths, balanced counts but not connected
  go(4, {{0, 1}, {1, 2}, {3, 3}}, 0, kNone);                   // path plus a separate self-loop
  go(3, {{0, 1}, {0, 2}}, 0, kNone);                           // two sinks
  go(3, {{0, 2}, {1, 2}}, 0, kNone);                           // two sources
  go(3, {{0, 1}, {1, 1}, {1, 2}}, 1, 0);                       // path through a self-loop
  go(3, {{0, 0}, {0, 1}, {1, 0}}, 2, kNone);                   // loop at a node of a two-cycle
  go(3, {{0, 1}, {1, 2}, {2, 1}, {1, 0}}, 2, kNone);           // figure of two cycles through node 1
  go(3, {{2, 1}, {1, 2}, {1, 0}}, 1, 1);                       // path starts in the middle of the node numbers
  go(2, {{1, 0}}, 1, 1);                                       // path whose start is the higher node
  go(2, {{0, 0}, {1, 1}}, 0, kNone);                           // two loops on different nodes
  go(3, {{0, 1}, {1, 0}, {1, 2}}, 1, 1);                       // path starting at 1, going round through 0
}

TEST_CASE("argument rules and check order", "[euler]") {
  const u32 eu[2] = {0, 1}, ev[2] = {1, 0};
  std::vector<u32> sc(6 * 2 + 3 * 2 + 8, kGarbage);
  u32 out[2] = {kSentinel, kSentinel};
  u32 len = kSentinel;
  EulerKind kind = kKindSentinel;
  auto untouched = [&]() { return out[0] == kSentinel && out[1] == kSentinel && len == kSentinel && kind == kKindSentinel; };
  auto reset = [&]() {
    out[0] = out[1] = kSentinel;
    len = kSentinel;
    kind = kKindSentinel;
  };
  const u32 W = static_cast<u32>(sc.size());
  // baseline
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), W, out, &len, &kind) == EulerResult::Ok);
  CHECK(kind == EulerKind::Circuit);
  CHECK(len == 2u);
  reset();
  // each pointer alone
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), W, out, nullptr, &kind) == EulerResult::NullArgument);
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), W, out, &len, nullptr) == EulerResult::NullArgument);
  CHECK(euler_trail(2, 2, nullptr, ev, sc.data(), W, out, &len, &kind) == EulerResult::NullArgument);
  CHECK(euler_trail(2, 2, eu, nullptr, sc.data(), W, out, &len, &kind) == EulerResult::NullArgument);
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), W, nullptr, &len, &kind) == EulerResult::NullArgument);
  CHECK(euler_trail(2, 2, eu, ev, nullptr, W, out, &len, &kind) == EulerResult::NullArgument);
  CHECK(untouched());
  // pointers are needed for any m above 0 and any n above 0, also for m = 1 and n = 1
  {
    const u32 e1[1] = {0};
    std::vector<u32> sc1(6 + 3 + 8, kGarbage);
    u32 o1[1] = {kSentinel};
    const u32 W1 = static_cast<u32>(sc1.size());
    CHECK(euler_trail(1, 1, e1, e1, sc1.data(), W1, o1, &len, &kind) == EulerResult::Ok);
    reset();
    CHECK(euler_trail(1, 1, nullptr, e1, sc1.data(), W1, o1, &len, &kind) == EulerResult::NullArgument);
    CHECK(euler_trail(1, 1, e1, nullptr, sc1.data(), W1, o1, &len, &kind) == EulerResult::NullArgument);
    CHECK(euler_trail(1, 1, e1, e1, sc1.data(), W1, nullptr, &len, &kind) == EulerResult::NullArgument);
    CHECK(euler_trail(1, 1, e1, e1, nullptr, W1, o1, &len, &kind) == EulerResult::NullArgument);
    CHECK(euler_trail(1, 0, nullptr, nullptr, nullptr, W1, nullptr, &len, &kind) == EulerResult::NullArgument);
    CHECK(euler_trail(1, 0, nullptr, nullptr, sc1.data(), 13, nullptr, &len, &kind) == EulerResult::ScratchTooSmall);
    CHECK(euler_trail(1, 0, nullptr, nullptr, sc1.data(), 14, nullptr, &len, &kind) == EulerResult::Ok);
    reset();
    o1[0] = kSentinel;
    // endpoint equal to n is BadNode in either array (one node, one edge)
    const u32 one[1] = {1};
    CHECK(euler_trail(1, 1, one, e1, sc1.data(), W1, o1, &len, &kind) == EulerResult::BadNode);
    CHECK(euler_trail(1, 1, e1, one, sc1.data(), W1, o1, &len, &kind) == EulerResult::BadNode);
    CHECK(euler_trail(1, 1, one, one, sc1.data(), W1, o1, &len, &kind) == EulerResult::BadNode);
    CHECK(untouched());
    CHECK(o1[0] == kSentinel);
  }
  // pointers not needed: edge pointers and out_edges while m = 0, scratch while n = 0
  CHECK(euler_trail(2, 0, nullptr, nullptr, sc.data(), W, nullptr, &len, &kind) == EulerResult::Ok);
  CHECK(kind == EulerKind::Circuit);
  CHECK(len == 0u);
  reset();
  CHECK(euler_trail(0, 0, nullptr, nullptr, nullptr, 0, nullptr, &len, &kind) == EulerResult::Ok);
  CHECK(kind == EulerKind::Circuit);
  CHECK(len == 0u);
  reset();
  // out_kind and out_len are always required
  CHECK(euler_trail(0, 0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &kind) == EulerResult::NullArgument);
  CHECK(euler_trail(0, 0, nullptr, nullptr, nullptr, 0, nullptr, &len, nullptr) == EulerResult::NullArgument);
  CHECK(untouched());
  // null beats TooLarge, TooLarge beats ScratchTooSmall and BadNode
  CHECK(euler_trail(kMaxNodes + 1u, 0, nullptr, nullptr, nullptr, 0, nullptr, &len, &kind) == EulerResult::NullArgument);
  CHECK(euler_trail(kMaxNodes + 1u, 0, nullptr, nullptr, sc.data(), 0, nullptr, &len, &kind) == EulerResult::TooLarge);
  CHECK(euler_trail(1, kMaxEdges + 1u, eu, ev, sc.data(), 0, out, &len, &kind) == EulerResult::TooLarge);
  CHECK(euler_trail(0xFFFFFFFFu, 0xFFFFFFFFu, eu, ev, sc.data(), 0, out, &len, &kind) == EulerResult::TooLarge);
  CHECK(untouched());
  // ScratchTooSmall by exactly one word; with n = 0 the scratch size is ignored; BadNode comes after ScratchTooSmall
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), W - 1u, out, &len, &kind) == EulerResult::ScratchTooSmall);
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), 0, out, &len, &kind) == EulerResult::ScratchTooSmall);
  const u32 bad_u[2] = {0, 2};
  CHECK(euler_trail(2, 2, bad_u, ev, sc.data(), W - 1u, out, &len, &kind) == EulerResult::ScratchTooSmall);
  CHECK(untouched());
  CHECK(euler_trail(2, 2, eu, ev, sc.data(), W, out, &len, &kind) == EulerResult::Ok);
  reset();
  // BadNode: any endpoint at or above n, in either array, at any position; n = 0 with m > 0
  CHECK(euler_trail(2, 2, bad_u, ev, sc.data(), W, out, &len, &kind) == EulerResult::BadNode);
  const u32 bad_v[2] = {1, 5};
  CHECK(euler_trail(2, 2, eu, bad_v, sc.data(), W, out, &len, &kind) == EulerResult::BadNode);
  const u32 first_bad[2] = {2, 1};
  CHECK(euler_trail(2, 2, first_bad, ev, sc.data(), W, out, &len, &kind) == EulerResult::BadNode);
  const u32 huge[2] = {0, 0xFFFFFFFFu};
  CHECK(euler_trail(2, 2, eu, huge, sc.data(), W, out, &len, &kind) == EulerResult::BadNode);
  CHECK(euler_trail(0, 2, eu, ev, nullptr, 0, out, &len, &kind) == EulerResult::BadNode);
  CHECK(untouched());
  // BadNode wins over a graph with no trail, whatever the edge order
  const u32 a_u[3] = {0, 0, 1}, a_v[3] = {1, 1, 9};
  const u32 b_u[3] = {1, 0, 0}, b_v[3] = {9, 1, 1};
  std::vector<u32> sc3(6 * 2 + 3 * 3 + 8, kGarbage);
  u32 out3[3] = {kSentinel, kSentinel, kSentinel};
  CHECK(euler_trail(2, 3, a_u, a_v, sc3.data(), static_cast<u32>(sc3.size()), out3, &len, &kind) == EulerResult::BadNode);
  CHECK(euler_trail(2, 3, b_u, b_v, sc3.data(), static_cast<u32>(sc3.size()), out3, &len, &kind) == EulerResult::BadNode);
  CHECK(untouched());
  CHECK(out3[0] == kSentinel);
}

TEST_CASE("a graph with no trail writes the kind and a zero length, and leaves out_edges untouched; a trail writes exactly m edges", "[euler]") {
  const Graph none = from_pairs(3, {{0, 1}, {0, 2}});
  const Run r0 = run(none);
  CHECK(r0.res == EulerResult::Ok);
  CHECK(r0.kind == EulerKind::None);
  CHECK(r0.len == 0u);
  CHECK(r0.out[0] == kSentinel);
  CHECK(r0.out[1] == kSentinel);
  // a larger out_edges array keeps its tail
  const Graph g = from_pairs(3, {{0, 1}, {1, 2}});
  std::vector<u32> sc(6 * 3 + 3 * 2 + 8, kGarbage);
  std::vector<u32> out(5, kSentinel);
  u32 len = kSentinel;
  EulerKind kind = kKindSentinel;
  CHECK(euler_trail(3, 2, g.eu.data(), g.ev.data(), sc.data(), static_cast<u32>(sc.size()), out.data(), &len, &kind) == EulerResult::Ok);
  CHECK(kind == EulerKind::Path);
  CHECK(len == 2u);
  CHECK(out[0] == 0u);
  CHECK(out[1] == 1u);
  CHECK(out[2] == kSentinel);
  CHECK(out[3] == kSentinel);
  CHECK(out[4] == kSentinel);
  // a larger scratch and different garbage give the same kinds
  for (u32 fill : {0u, 0xFFFFFFFFu, 0x12345678u}) {
    const Run r = run(g, fill);
    CHECK(certified(g, r, 1, 0));
  }
}

TEST_CASE("relabelled copies: renamed nodes and shuffled edges keep the kind, the start follows the node, and the trail is certified", "[euler]") {
  Lcg r{4242};
  for (u32 k = 0; k < 3000u; ++k) {
    const u32 n = 1u + r.rnd(8);
    const u32 m = r.rnd(14);
    const Graph g = gen(r, n, m, r.rnd(4));
    std::vector<u32> perm(n);
    for (u32 i = 0; i < n; ++i) perm[i] = i;
    for (u32 i = n; i-- > 1;) std::swap(perm[i], perm[r.rnd(i + 1u)]);
    std::vector<std::pair<u32, u32>> e;
    for (u32 i = 0; i < m; ++i) e.push_back({perm[g.eu[i]], perm[g.ev[i]]});
    shuffle(r, e);
    const Graph h = from_pairs(n, e);
    const Class c = classify(g);
    const Class d = classify(h);
    REQUIRE(c.kind == d.kind);
    if (c.kind == 1u) REQUIRE(d.start == perm[c.start]);
    const Run a = run(g);
    const Run b = run(h);
    REQUIRE(a.res == EulerResult::Ok);
    REQUIRE(b.res == EulerResult::Ok);
    REQUIRE(a.kind == b.kind);
    REQUIRE(certified(g, a, c.kind, c.start));
    REQUIRE(certified(h, b, d.kind, d.start));
  }
  // nine max-size copies of the patterns that are not empty: reversed edge order keeps the kind
  for (u32 pid = 1; pid < 15u; pid += 2u) {
    Graph g = big_graph(pid);
    std::reverse(g.eu.begin(), g.eu.end());
    std::reverse(g.ev.begin(), g.ev.end());
    const u32* row = &kBig[5u * pid];
    const Run a = run(g);
    REQUIRE(a.res == EulerResult::Ok);
    REQUIRE(certified(g, a, row[3], row[4]));
  }
}

TEST_CASE("smallest sizes: one node", "[euler]") {
  for (u32 m = 0; m <= 4u; ++m) {
    Graph g;
    g.n = 1;
    for (u32 i = 0; i < m; ++i) g.add(0, 0);
    const Run r = run(g);
    CHECK(r.res == EulerResult::Ok);
    CHECK(certified(g, r, 2, kNone));
    CHECK(r.len == m);
  }
}
