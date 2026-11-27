// Tests for core/twosat. Evidence label: unit tests only. Expected verdicts and conflict variables come from tools/twosat_ref.py (brute force over all
// assignments, with a reachability closure for the conflict variable, for the small instances; iterative Kosaraju for the max-size ones). Every
// success test uses a scratch array of exactly 16n + 2m + 2 words (filled with garbage) and exact-size outputs pre-filled with sentinels. The
// assignment that is returned is not specified, so no test compares it with anything but the clauses: every returned assignment is checked clause by
// clause. Tiny or null buffers appear only for failures and empty instances. No test asserts any scratch value after a call. The max-size cases are
// sampled patterns, not exhaustive. Time is not measured.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "twosat.h"

using lumora::twosat::kMaxClauses;
using lumora::twosat::kMaxVars;
using lumora::twosat::twosat_scratch_words;
using lumora::twosat::twosat_solve;
using lumora::twosat::TwoSatResult;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u8 = std::uint8_t;

namespace {

// groups: instances, digest lo, digest hi
const u32 kGroups[21] = {
    84u, 161897508u, 2098215032u,
    74613u, 3266957009u, 3660693984u,
    91390u, 2542636729u, 316277762u,
    9139u, 224701448u, 3889471416u,
    69905u, 3437671530u, 1067610018u,
    47989u, 808480756u, 2990989545u,
    2097152u, 80167269u, 2900108463u,
};
const u32 kBattery[2] = {1600354638u, 4064300430u};
const u32 kMid[3] = {1939591847u, 1343136747u, 149u};
// max-size patterns: pid, n, m, sat, conflict
const u32 kBig[75] = {
    0u, 65536u, 0u, 1u, 4294967295u,
    1u, 65536u, 65537u, 0u, 0u,
    2u, 65536u, 65536u, 1u, 4294967295u,
    3u, 65536u, 65535u, 1u, 4294967295u,
    4u, 65536u, 100002u, 0u, 65535u,
    5u, 65536u, 262144u, 0u, 0u,
    6u, 65536u, 50000u, 1u, 4294967295u,
    7u, 65536u, 65536u, 1u, 4294967295u,
    8u, 65536u, 65536u, 1u, 4294967295u,
    9u, 65536u, 262144u, 1u, 4294967295u,
    10u, 65536u, 131070u, 1u, 4294967295u,
    11u, 65536u, 131072u, 0u, 0u,
    12u, 65536u, 60022u, 0u, 40000u,
    13u, 512u, 262144u, 1u, 4294967295u,
    14u, 65536u, 262144u, 1u, 4294967295u,
};

const u32 kGarbage = 0xA5A5A5A5u;
const u32 kSentinel32 = 0xDEADBEEFu;
const u8 kSentinel8 = 0xEEu;
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

struct Inst {
  u32 n = 0;
  std::vector<u32> la, lb;
  u32 clauses() const { return static_cast<u32>(la.size()); }
  void add(u32 a, u32 b) {
    la.push_back(a);
    lb.push_back(b);
  }
};

struct Run {
  TwoSatResult res;
  u8 sat;
  u32 conflict;
  std::vector<u8> value;
};

Run run(const Inst& g, u32 fill = kGarbage) {
  const u32 m = g.clauses();
  std::vector<u32> scratch(16ull * g.n + 2ull * m + 2ull, fill);
  Run r;
  r.value.assign(g.n, kSentinel8);
  r.sat = kSentinel8;
  r.conflict = kSentinel32;
  r.res = twosat_solve(g.n, m, g.la.data(), g.lb.data(), scratch.data(), static_cast<u32>(scratch.size()), r.value.data(), &r.sat, &r.conflict);
  return r;
}

u64 fold_run(u64 h, const Run& r) {
  h = fold_bytes(h, r.sat, 1);
  return fold_bytes(h, r.sat == 1u ? kNone : r.conflict, 4);
}

struct Lcg {
  u64 s;
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 33);
  }
  u32 rnd(u32 k) { return next() % k; }
};

bool literal_true(const std::vector<u8>& value, u32 lit) { return value[lit >> 1] == ((lit & 1u) ? 0u : 1u); }

// Independent reachability over the implication graph (breadth-first, adjacency built with std::sort; no strongly connected components).
struct Impl {
  u32 N;
  std::vector<u32> first, adj;
  explicit Impl(const Inst& g) : N(2u * g.n), first(2ull * g.n + 1u, 0) {
    std::vector<std::pair<u32, u32>> e;
    for (u32 i = 0; i < g.clauses(); ++i) {
      e.push_back({g.la[i] ^ 1u, g.lb[i]});
      e.push_back({g.lb[i] ^ 1u, g.la[i]});
    }
    std::sort(e.begin(), e.end());
    for (const auto& p : e) {
      ++first[p.first + 1u];
      adj.push_back(p.second);
    }
    for (u32 v = 0; v < N; ++v) first[v + 1u] += first[v];
  }
  bool reaches(u32 from, u32 to) const {
    std::vector<u8> seen(N, 0);
    std::vector<u32> q;
    q.push_back(from);
    seen[from] = 1;
    for (std::size_t h = 0; h < q.size(); ++h) {
      const u32 u = q[h];
      if (u == to) return true;
      for (u32 k = first[u]; k < first[u + 1u]; ++k) {
        if (!seen[adj[k]]) {
          seen[adj[k]] = 1;
          q.push_back(adj[k]);
        }
      }
    }
    return false;
  }
  bool conflict_at(u32 v) const { return reaches(2u * v, 2u * v + 1u) && reaches(2u * v + 1u, 2u * v); }
};

// Certificate checker. Satisfiable: every value is 0 or 1 and every clause holds, conflict untouched. Unsatisfiable: value untouched, and the reported
// variable has both reachabilities (a proof of unsatisfiability); with `minimal` every smaller variable is checked to have no conflict.
const char* certify(const Inst& g, const Run& r, bool minimal) {
  if (r.res != TwoSatResult::Ok) return "result not Ok";
  if (r.sat == 1u) {
    if (r.conflict != kSentinel32) return "conflict written on sat";
    for (u8 x : r.value) {
      if (x > 1u) return "value not 0 or 1";
    }
    for (u32 i = 0; i < g.clauses(); ++i) {
      if (!literal_true(r.value, g.la[i]) && !literal_true(r.value, g.lb[i])) return "clause violated";
    }
    return "";
  }
  if (r.sat != 0u) return "sat flag";
  for (u8 x : r.value) {
    if (x != kSentinel8) return "value written on unsat";
  }
  if (r.conflict >= g.n) return "conflict out of range";
  const Impl im(g);
  if (!im.conflict_at(r.conflict)) return "conflict variable has no two-way path";
  if (minimal) {
    for (u32 v = 0; v < r.conflict; ++v) {
      if (im.conflict_at(v)) return "smaller conflict variable exists";
    }
  }
  return "";
}

bool certified(const Inst& g, const Run& r, bool minimal) { return certify(g, r, minimal)[0] == '\0'; }

// Enumerations identical to tools/twosat_ref.py.
struct Kind {
  u32 a, b;
};

std::vector<Kind> kinds(u32 n, bool unordered) {
  std::vector<Kind> k;
  for (u32 a = 0; a < 2u * n; ++a) {
    for (u32 b = unordered ? a : 0u; b < 2u * n; ++b) k.push_back({a, b});
  }
  return k;
}

u64 enumerate_group(u32 n, u32 mmax, bool ordered, bool unordered_kinds, u32& count) {
  const std::vector<Kind> K = kinds(n, unordered_kinds);
  const u32 KN = static_cast<u32>(K.size());
  u64 h = kFnvOff;
  count = 0;
  for (u32 m = 0; m <= mmax; ++m) {
    std::vector<u32> idx(m, 0);
    for (;;) {
      Inst g;
      g.n = n;
      for (u32 i = 0; i < m; ++i) g.add(K[idx[i]].a, K[idx[i]].b);
      ++count;
      const Run r = run(g);
      REQUIRE(r.res == TwoSatResult::Ok);
      REQUIRE(certified(g, r, true));
      h = fold_run(h, r);
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

void planted(Lcg& r, u32 n, u32 m, Inst& g) {
  std::vector<u32> hid(n);
  for (u32 i = 0; i < n; ++i) hid[i] = r.rnd(2);
  u32 got = 0;
  while (got < m) {
    const u32 a = r.rnd(2u * n);
    const u32 b = r.rnd(2u * n);
    const u32 ta = hid[a >> 1] ^ (a & 1u) ^ 1u;
    const u32 tb = hid[b >> 1] ^ (b & 1u) ^ 1u;
    if (ta != 0 || tb != 0) {
      g.add(a, b);
      ++got;
    }
  }
}

// Mirror of big() in tools/twosat_ref.py.
Inst big_inst(u32 pid) {
  Lcg r = {7000ull + pid};
  const u32 N = 65536u, M = 262144u;
  Inst g;
  g.n = N;
  if (pid == 0) {
  } else if (pid == 1) {
    for (u32 i = 0; i + 1u < N; ++i) g.add(2u * i + 1u, 2u * (i + 1u));
    g.add(0, 0);
    g.add(2u * (N - 1u) + 1u, 2u * (N - 1u) + 1u);
  } else if (pid == 2) {
    for (u32 i = 0; i + 1u < N; ++i) g.add(2u * i + 1u, 2u * (i + 1u));
    g.add(0, 0);
  } else if (pid == 3) {
    std::vector<u32> p(N);
    for (u32 i = 0; i < N; ++i) p[i] = i;
    for (u32 i = N; i > 1; --i) std::swap(p[i - 1u], p[r.rnd(i)]);
    for (u32 i = 0; i + 1u < N; ++i) g.add(2u * p[i] + 1u, 2u * p[i + 1u]);
  } else if (pid == 4) {
    planted(r, N, 100000u, g);
    g.add(2u * (N - 1u), 2u * (N - 1u));
    g.add(2u * (N - 1u) + 1u, 2u * (N - 1u) + 1u);
  } else if (pid == 5) {
    for (u32 i = 0; i < M; ++i) {
      const u32 a = r.rnd(2u * N);
      const u32 b = r.rnd(2u * N);
      g.add(a, b);
    }
  } else if (pid == 6) {
    for (u32 i = 0; i < 50000u; ++i) {
      const u32 a = r.rnd(2u * N);
      const u32 b = r.rnd(2u * N);
      g.add(a, b);
    }
  } else if (pid == 7) {
    for (u32 i = 0; i < N; ++i) {
      if (i % 2u == 0) {
        g.add(2u * i, 2u * i + 1u);
      } else {
        g.add(2u * i + 1u, 2u * i);
      }
    }
  } else if (pid == 8) {
    for (u32 i = 0; i < N; ++i) g.add(2u * i + (i & 1u), 2u * i + (i & 1u));
  } else if (pid == 9) {
    for (u32 i = 0; i < M; ++i) g.add(0, 2);
  } else if (pid == 10) {
    for (u32 i = 0; i + 1u < N; ++i) {
      g.add(2u * i + 1u, 2u * (i + 1u));
      g.add(2u * i, 2u * (i + 1u) + 1u);
    }
  } else if (pid == 11) {
    for (u32 i = 0; i + 1u < N; ++i) {
      g.add(2u * i + 1u, 2u * (i + 1u));
      g.add(2u * i, 2u * (i + 1u) + 1u);
    }
    g.add(2u * (N - 1u), 0);
    g.add(2u * (N - 1u) + 1u, 1);
  } else if (pid == 12) {
    planted(r, N, 60000u, g);
    const u32 base = 40000u;
    for (u32 i = 0; i < 10u; ++i) {
      g.add(2u * (base + i) + 1u, 2u * (base + i + 1u));
      g.add(2u * (base + i), 2u * (base + i + 1u) + 1u);
    }
    g.add(2u * (base + 10u), 2u * base);
    g.add(2u * (base + 10u) + 1u, 2u * base + 1u);
  } else if (pid == 13) {
    g.n = 512u;
    planted(r, 512u, M, g);
  } else if (pid == 14) {
    planted(r, N, M, g);
  }
  return g;
}

}  // namespace
TEST_CASE("twosat_scratch_words", "[twosat]") {
  u32 w = 7;
  CHECK(twosat_scratch_words(0, 0, &w) == TwoSatResult::Ok);
  CHECK(w == 2u);
  CHECK(twosat_scratch_words(1, 0, &w) == TwoSatResult::Ok);
  CHECK(w == 18u);
  CHECK(twosat_scratch_words(3, 5, &w) == TwoSatResult::Ok);
  CHECK(w == 60u);
  CHECK(twosat_scratch_words(kMaxVars, kMaxClauses, &w) == TwoSatResult::Ok);
  CHECK(w == 16u * 65536u + 2u * 262144u + 2u);
  CHECK(twosat_scratch_words(0, kMaxClauses, &w) == TwoSatResult::Ok);
  CHECK(w == 2u * 262144u + 2u);
  w = 99;
  CHECK(twosat_scratch_words(kMaxVars + 1u, 0, &w) == TwoSatResult::TooLarge);
  CHECK(w == 99u);
  CHECK(twosat_scratch_words(0, kMaxClauses + 1u, &w) == TwoSatResult::TooLarge);
  CHECK(w == 99u);
  CHECK(twosat_scratch_words(0xFFFFFFFFu, 0xFFFFFFFFu, &w) == TwoSatResult::TooLarge);
  CHECK(w == 99u);
  CHECK(twosat_scratch_words(1, 1, nullptr) == TwoSatResult::NullArgument);
  CHECK(twosat_scratch_words(kMaxVars + 1u, 0, nullptr) == TwoSatResult::NullArgument);
}

TEST_CASE("every multiset of 0 to 6 clauses on 1 variable (3 clause kinds a <= b, 84 instances): digest group", "[twosat]") {
  u32 count = 0;
  const u64 h = enumerate_group(1, 6, false, true, count);
  CHECK(count == kGroups[0]);
  CHECK(static_cast<u32>(h) == kGroups[1]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[2]);
}

TEST_CASE("every multiset of 0 to 6 clauses on 2 variables (16 ordered literal pairs, 74613 instances): digest group", "[twosat]") {
  u32 count = 0;
  const u64 h = enumerate_group(2, 6, false, false, count);
  CHECK(count == kGroups[3]);
  CHECK(static_cast<u32>(h) == kGroups[4]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[5]);
}

TEST_CASE("every multiset of 0 to 4 clauses on 3 variables (36 ordered literal pairs, 91390 instances): digest group", "[twosat]") {
  u32 count = 0;
  const u64 h = enumerate_group(3, 4, false, false, count);
  CHECK(count == kGroups[6]);
  CHECK(static_cast<u32>(h) == kGroups[7]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[8]);
}

TEST_CASE("every multiset of 0 to 3 clauses on 4 variables (36 literal pairs a <= b, 9139 instances): digest group", "[twosat]") {
  u32 count = 0;
  const u64 h = enumerate_group(4, 3, false, true, count);
  CHECK(count == kGroups[9]);
  CHECK(static_cast<u32>(h) == kGroups[10]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[11]);
}

TEST_CASE("every ordered clause list of 0 to 4 clauses on 2 variables (16 ordered literal pairs, 69905 lists): digest group", "[twosat]") {
  u32 count = 0;
  const u64 h = enumerate_group(2, 4, true, false, count);
  CHECK(count == kGroups[12]);
  CHECK(static_cast<u32>(h) == kGroups[13]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[14]);
}

TEST_CASE("every ordered clause list of 0 to 3 clauses on 3 variables (36 ordered literal pairs, 47989 lists): digest group", "[twosat]") {
  u32 count = 0;
  const u64 h = enumerate_group(3, 3, true, false, count);
  CHECK(count == kGroups[15]);
  CHECK(static_cast<u32>(h) == kGroups[16]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[17]);
}

TEST_CASE("every set of clauses on 3 variables (the 21 literal pairs a <= b, tautologies and unit clauses included, all 2^21 = 2097152 subsets): digest group",
          "[twosat]") {
  std::vector<Kind> K = kinds(3, true);
  REQUIRE(K.size() == 21u);
  u64 h = kFnvOff;
  u32 unsat = 0;
  for (u32 sub = 0; sub < (1u << 21); ++sub) {
    Inst g;
    g.n = 3;
    for (u32 i = 0; i < 21u; ++i) {
      if (((sub >> i) & 1u) != 0) g.add(K[i].a, K[i].b);
    }
    const Run r = run(g);
    REQUIRE(r.res == TwoSatResult::Ok);
    if (r.sat == 1u) {
      for (u32 i = 0; i < g.clauses(); ++i) REQUIRE((literal_true(r.value, g.la[i]) || literal_true(r.value, g.lb[i])));
      for (u8 x : r.value) REQUIRE(x <= 1u);
    } else {
      ++unsat;
      REQUIRE(r.conflict < 3u);
    }
    h = fold_run(h, r);
  }
  CHECK(kGroups[18] == (1u << 21));
  CHECK(static_cast<u32>(h) == kGroups[19]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[20]);
  CHECK(unsat > 0u);
}

TEST_CASE("random battery: 20000 small instances by digest", "[twosat]") {
  Lcg r = {6161};
  u64 h = kFnvOff;
  for (u32 k = 0; k < 20000u; ++k) {
    const u32 n = 1u + r.rnd(9);
    const u32 m = r.rnd(25);
    Inst g;
    g.n = n;
    for (u32 i = 0; i < m; ++i) {
      const u32 a = r.rnd(2u * n);
      const u32 b = r.rnd(2u * n);
      g.add(a, b);
    }
    const Run run1 = run(g);
    REQUIRE(run1.res == TwoSatResult::Ok);
    if (run1.sat == 1u) REQUIRE(certified(g, run1, false));
    h = fold_run(h, run1);
  }
  CHECK(static_cast<u32>(h) == kBattery[0]);
  CHECK(static_cast<u32>(h >> 32) == kBattery[1]);
}

TEST_CASE("mid-size instances (200 instances, up to 16 variables, up to 37 clauses): digest and certificate on every instance", "[twosat]") {
  Lcg r = {6262};
  u64 h = kFnvOff;
  u32 nsat = 0;
  for (u32 k = 0; k < 200u; ++k) {
    const u32 n = 1u + r.rnd(16);
    const u32 m = r.rnd(2u * n + 6u);
    Inst g;
    g.n = n;
    for (u32 i = 0; i < m; ++i) {
      const u32 a = r.rnd(2u * n);
      const u32 b = r.rnd(2u * n);
      g.add(a, b);
    }
    const Run run1 = run(g);
    REQUIRE(run1.res == TwoSatResult::Ok);
    REQUIRE(certified(g, run1, true));
    nsat += run1.sat;
    h = fold_run(h, run1);
  }
  CHECK(static_cast<u32>(h) == kMid[0]);
  CHECK(static_cast<u32>(h >> 32) == kMid[1]);
  CHECK(nsat == kMid[2]);
}

TEST_CASE("max-size instances: 15 patterns by verdict and conflict, each checked by the certificate checker", "[twosat]") {
  for (u32 pid = 0; pid < 15u; ++pid) {
    const u32* row = kBig + 5u * pid;
    REQUIRE(row[0] == pid);
    const Inst g = big_inst(pid);
    REQUIRE(g.n == row[1]);
    REQUIRE(g.clauses() == row[2]);
    const Run r = run(g);
    REQUIRE(r.res == TwoSatResult::Ok);
    CHECK(r.sat == row[3]);
    if (row[3] == 0u) CHECK(r.conflict == row[4]);
    const char* why = certify(g, r, false);
    INFO("pattern " << pid << ": " << why);
    CHECK(why[0] == '\0');
  }
}

TEST_CASE("max-size patterns by closed form", "[twosat]") {
  {  // pattern 0: no clauses, 65536 variables: satisfiable
    const Inst g = big_inst(0);
    const Run r = run(g);
    CHECK(r.sat == 1u);
    CHECK(certified(g, r, false));
  }
  {  // pattern 1: a chain of 65535 implications with x0 forced true and the last variable forced false: one component, conflict 0
    const Run r = run(big_inst(1));
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 0u);
  }
  {  // pattern 2: the same chain with only x0 forced: every variable is true in every solution
    const Inst g = big_inst(2);
    const Run r = run(g);
    REQUIRE(r.sat == 1u);
    for (u32 v = 0; v < 65536u; ++v) REQUIRE(r.value[v] == 1u);
  }
  {  // pattern 8: unit clauses fix every variable: x_i true for even i, false for odd i
    const Run r = run(big_inst(8));
    REQUIRE(r.sat == 1u);
    for (u32 v = 0; v < 65536u; ++v) REQUIRE(r.value[v] == ((v & 1u) ? 0u : 1u));
  }
  {  // pattern 7: tautologies only; pattern 9: one clause 262144 times; both satisfiable
    CHECK(run(big_inst(7)).sat == 1u);
    const Inst g = big_inst(9);
    const Run r = run(g);
    CHECK(r.sat == 1u);
    CHECK(certified(g, r, false));
  }
  {  // pattern 11: a ring of equivalences closed with an odd twist: unsatisfiable, conflict 0
    const Run r = run(big_inst(11));
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 0u);
  }
  {  // pattern 12: the only unsatisfiable part is on variables 40000 to 40010: the smallest conflict variable is 40000
    const Run r = run(big_inst(12));
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 40000u);
  }
  {  // pattern 4: the only unsatisfiable part is the last variable
    const Run r = run(big_inst(4));
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 65535u);
  }
}

TEST_CASE("hand-checked rules", "[twosat]") {
  {  // no variables, no clauses
    Inst g;
    const Run r = run(g);
    CHECK(r.res == TwoSatResult::Ok);
    CHECK(r.sat == 1u);
    CHECK(r.conflict == kSentinel32);
  }
  {  // variables without clauses: satisfiable, all values written as 0 or 1
    Inst g;
    g.n = 5;
    const Run r = run(g);
    REQUIRE(r.sat == 1u);
    CHECK(certified(g, r, false));
  }
  {  // a unit clause (x0 or x0) forces x0; a unit clause on the negation forces it the other way
    Inst g;
    g.n = 1;
    g.add(0, 0);
    const Run r = run(g);
    REQUIRE(r.sat == 1u);
    CHECK(r.value[0] == 1u);
    Inst h;
    h.n = 1;
    h.add(1, 1);
    const Run q = run(h);
    REQUIRE(q.sat == 1u);
    CHECK(q.value[0] == 0u);
    g.add(1, 1);
    const Run z = run(g);
    CHECK(z.sat == 0u);
    CHECK(z.conflict == 0u);
  }
  {  // a tautology (x or not x) never makes an instance unsatisfiable, in either order, and constrains nothing
    Inst g;
    g.n = 2;
    g.add(0, 1);
    g.add(3, 2);
    g.add(2, 2);
    const Run r = run(g);
    REQUIRE(r.sat == 1u);
    CHECK(r.value[1] == 1u);
    CHECK(certified(g, r, false));
  }
  {  // all four clauses on two variables: unsatisfiable, conflict 0
    Inst g;
    g.n = 2;
    g.add(0, 2);
    g.add(1, 2);
    g.add(0, 3);
    g.add(1, 3);
    const Run r = run(g);
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 0u);
    CHECK(certified(g, r, true));
  }
  {  // the smallest conflict variable: variable 0 is free, variables 1 and 2 are in conflict, the smaller one is reported
    Inst g;
    g.n = 3;
    g.add(0, 1);
    g.add(4, 4);   // x2
    g.add(5, 5);   // not x2: a conflict on variable 2
    g.add(2, 2);   // x1
    g.add(3, 3);   // not x1: a conflict on variable 1
    const Run r = run(g);
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 1u);
    CHECK(certified(g, r, true));
  }
  {  // a conflict found only through a long implication path: x0 -> x1 -> x2 -> not x0 and not x0 -> x2 -> ... back to x0
    Inst g;
    g.n = 3;
    g.add(1, 2);   // x0 -> x1
    g.add(3, 4);   // x1 -> x2
    g.add(5, 1);   // x2 -> not x0
    g.add(0, 0);   // x0 forced
    const Run r = run(g);
    CHECK(r.sat == 0u);
    CHECK(r.conflict == 0u);
    CHECK(certified(g, r, true));
  }
  {  // a satisfiable chain: every returned assignment is checked, whichever it is
    Inst g;
    g.n = 4;
    g.add(1, 2);
    g.add(3, 4);
    g.add(5, 6);
    g.add(7, 7);
    const Run r = run(g);
    REQUIRE(r.sat == 1u);
    CHECK(certified(g, r, false));
  }
  {  // the clause order and the literal order do not change the verdict or the conflict variable
    Lcg rr = {4242};
    for (u32 k = 0; k < 300u; ++k) {
      const u32 n = 1u + rr.rnd(6);
      Inst g;
      g.n = n;
      const u32 m = rr.rnd(14);
      for (u32 i = 0; i < m; ++i) {
        const u32 a = rr.rnd(2u * n);
        const u32 b = rr.rnd(2u * n);
        g.add(a, b);
      }
      Inst h;
      h.n = n;
      for (u32 i = m; i > 0; --i) h.add(g.lb[i - 1u], g.la[i - 1u]);
      const Run a = run(g);
      const Run b = run(h);
      REQUIRE(a.sat == b.sat);
      if (a.sat == 0u) REQUIRE(a.conflict == b.conflict);
    }
  }
  {  // different garbage in scratch: same verdict
    Lcg rr = {4343};
    for (u32 k = 0; k < 100u; ++k) {
      const u32 n = 1u + rr.rnd(8);
      Inst g;
      g.n = n;
      const u32 m = rr.rnd(18);
      for (u32 i = 0; i < m; ++i) {
        const u32 a = rr.rnd(2u * n);
        const u32 b = rr.rnd(2u * n);
        g.add(a, b);
      }
      const Run a = run(g, 0u);
      const Run b = run(g, 0xFFFFFFFFu);
      REQUIRE(a.sat == b.sat);
      if (a.sat == 0u) REQUIRE(a.conflict == b.conflict);
      REQUIRE(certified(g, a, true));
      REQUIRE(certified(g, b, true));
    }
  }
}

namespace {
struct Call {
  u32 n = 3, m = 2;
  std::vector<u32> la = {0, 3}, lb = {2, 4};
  std::vector<u32> scratch = std::vector<u32>(16u * 3u + 2u * 2u + 2u, kGarbage);
  u32 scratch_words = 16u * 3u + 2u * 2u + 2u;
  std::vector<u8> value = std::vector<u8>(3, kSentinel8);
  u8 sat = kSentinel8;
  u32 conflict = kSentinel32;
  bool null_la = false, null_lb = false, null_scratch = false, null_value = false, null_sat = false, null_conflict = false;
  TwoSatResult go() {
    return twosat_solve(n, m, null_la ? nullptr : la.data(), null_lb ? nullptr : lb.data(), null_scratch ? nullptr : scratch.data(), scratch_words,
                        null_value ? nullptr : value.data(), null_sat ? nullptr : &sat, null_conflict ? nullptr : &conflict);
  }
  bool untouched() const {
    for (u8 v : value) {
      if (v != kSentinel8) return false;
    }
    return sat == kSentinel8 && conflict == kSentinel32;
  }
};

void expect_fail(Call& c, TwoSatResult want) {
  const TwoSatResult got = c.go();
  CHECK(got == want);
  CHECK(c.untouched());
}
}  // namespace

TEST_CASE("argument rules and check order", "[twosat]") {
  {  // the base call succeeds
    Call c;
    CHECK(c.go() == TwoSatResult::Ok);
    CHECK(c.sat == 1u);
    CHECK(c.conflict == kSentinel32);
  }
  {  // each pointer alone: out_sat and out_conflict always, value and scratch while n > 0, la and lb while m > 0
    Call a;
    a.null_la = true;
    expect_fail(a, TwoSatResult::NullArgument);
    Call b;
    b.null_lb = true;
    expect_fail(b, TwoSatResult::NullArgument);
    Call c;
    c.null_scratch = true;
    expect_fail(c, TwoSatResult::NullArgument);
    Call d;
    d.null_value = true;
    expect_fail(d, TwoSatResult::NullArgument);
    Call e;
    e.null_sat = true;
    CHECK(e.go() == TwoSatResult::NullArgument);
    CHECK(e.conflict == kSentinel32);
    CHECK(e.value[0] == kSentinel8);
    Call f;
    f.null_conflict = true;
    CHECK(f.go() == TwoSatResult::NullArgument);
    CHECK(f.sat == kSentinel8);
    CHECK(f.value[0] == kSentinel8);
  }
  {  // out_sat and out_conflict are needed even for n = 0 and m = 0; la and lb are not needed while m is 0; value and scratch not while n is 0
    Call z;
    z.n = 0;
    z.m = 0;
    z.null_la = z.null_lb = z.null_scratch = z.null_value = true;
    z.scratch_words = 0;
    CHECK(z.go() == TwoSatResult::Ok);
    CHECK(z.sat == 1u);
    CHECK(z.conflict == kSentinel32);
    Call y;
    y.n = 0;
    y.m = 0;
    y.null_sat = true;
    CHECK(y.go() == TwoSatResult::NullArgument);
    Call x;
    x.n = 0;
    x.m = 0;
    x.null_conflict = true;
    CHECK(x.go() == TwoSatResult::NullArgument);
    Call c;
    c.m = 0;
    c.null_la = c.null_lb = true;
    CHECK(c.go() == TwoSatResult::Ok);
    CHECK(c.sat == 1u);
  }
  {  // n = 0 with clauses: BadLiteral (there is no valid literal), scratch_words is ignored; the clause pointers are needed
    Call c;
    c.n = 0;
    c.null_scratch = c.null_value = true;
    c.scratch_words = 0;
    expect_fail(c, TwoSatResult::BadLiteral);
    c.null_la = true;
    expect_fail(c, TwoSatResult::NullArgument);
  }
  {  // null rules use the given sizes even above the limits, and come before TooLarge
    Call a;
    a.n = kMaxVars + 1u;
    a.null_scratch = true;
    expect_fail(a, TwoSatResult::NullArgument);
    Call b;
    b.m = kMaxClauses + 1u;
    b.null_la = true;
    expect_fail(b, TwoSatResult::NullArgument);
    Call c;
    c.n = kMaxVars + 1u;
    expect_fail(c, TwoSatResult::TooLarge);
    Call d;
    d.m = kMaxClauses + 1u;
    expect_fail(d, TwoSatResult::TooLarge);
    Call e;
    e.n = 0xFFFFFFFFu;
    e.m = 0xFFFFFFFFu;
    e.scratch_words = 0;
    expect_fail(e, TwoSatResult::TooLarge);  // TooLarge before ScratchTooSmall and BadLiteral
  }
  {  // ScratchTooSmall: exact size passes, one word less fails; it comes before BadLiteral
    Call a;
    a.scratch_words -= 1u;
    expect_fail(a, TwoSatResult::ScratchTooSmall);
    Call b;
    b.scratch_words = 0;
    expect_fail(b, TwoSatResult::ScratchTooSmall);
    Call c;
    c.scratch_words -= 1u;
    c.la[0] = 99;
    expect_fail(c, TwoSatResult::ScratchTooSmall);
    Call d;
    d.scratch_words = 16u * 3u + 2u * 1u + 2u;  // one clause short
    expect_fail(d, TwoSatResult::ScratchTooSmall);
    Call e;
    e.scratch_words = 16u * 2u + 2u * 2u + 2u;  // one variable short
    expect_fail(e, TwoSatResult::ScratchTooSmall);
  }
  {  // BadLiteral: a literal at or above 2n in la or lb, in every clause position; outputs untouched
    for (u32 pos = 0; pos < 2u; ++pos) {
      for (u32 field = 0; field < 2u; ++field) {
        Call c;
        (field == 0 ? c.la : c.lb)[pos] = 6;
        expect_fail(c, TwoSatResult::BadLiteral);
        Call d;
        (field == 0 ? d.la : d.lb)[pos] = 0xFFFFFFFFu;
        expect_fail(d, TwoSatResult::BadLiteral);
        Call e;
        (field == 0 ? e.la : e.lb)[pos] = 5;  // the largest valid literal: accepted
        CHECK(e.go() == TwoSatResult::Ok);
      }
    }
  }
  {  // limits: 65536 variables with 262144 clauses is accepted, one word less scratch is not (64-bit size check)
    Inst g;
    g.n = kMaxVars;
    for (u32 i = 0; i < kMaxClauses; ++i) g.add(2u * (i % kMaxVars), 2u * ((i * 7u + 1u) % kMaxVars) + 1u);
    u32 words = 0;
    REQUIRE(twosat_scratch_words(g.n, g.clauses(), &words) == TwoSatResult::Ok);
    std::vector<u32> scratch(words, kGarbage);
    std::vector<u8> value(g.n, kSentinel8);
    u8 sat = kSentinel8;
    u32 conflict = kSentinel32;
    CHECK(twosat_solve(g.n, g.clauses(), g.la.data(), g.lb.data(), scratch.data(), words - 1u, value.data(), &sat, &conflict) ==
          TwoSatResult::ScratchTooSmall);
    CHECK(sat == kSentinel8);
    CHECK(twosat_solve(g.n, g.clauses(), g.la.data(), g.lb.data(), scratch.data(), words, value.data(), &sat, &conflict) == TwoSatResult::Ok);
    CHECK(sat == 1u);
  }
}

TEST_CASE("unsatisfiable results leave value untouched, satisfiable results leave out_conflict untouched", "[twosat]") {
  Inst g;
  g.n = 2;
  g.add(0, 0);
  g.add(1, 1);
  const Run r = run(g);
  CHECK(r.sat == 0u);
  CHECK(r.conflict == 0u);
  CHECK(r.value[0] == kSentinel8);
  CHECK(r.value[1] == kSentinel8);
  Inst h;
  h.n = 2;
  h.add(0, 2);
  const Run q = run(h);
  CHECK(q.sat == 1u);
  CHECK(q.conflict == kSentinel32);
  CHECK(q.value[0] <= 1u);
  CHECK(q.value[1] <= 1u);
}

TEST_CASE("relabelled copies: renamed variables, flipped polarities, shuffled clauses give the same verdict and a certified result", "[twosat]") {
  Lcg r = {5151};
  auto check = [&](const Inst& g) {
    const Run a = run(g);
    REQUIRE(a.res == TwoSatResult::Ok);
    std::vector<u32> p(g.n);
    std::vector<u32> flip(g.n);
    for (u32 i = 0; i < g.n; ++i) {
      p[i] = i;
      flip[i] = r.rnd(2);
    }
    for (u32 i = g.n; i > 1; --i) std::swap(p[i - 1u], p[r.rnd(i)]);
    std::vector<u32> order(g.clauses());
    for (u32 i = 0; i < g.clauses(); ++i) order[i] = i;
    for (u32 i = g.clauses(); i > 1; --i) std::swap(order[i - 1u], order[r.rnd(i)]);
    auto map = [&](u32 l) { return 2u * p[l >> 1] + ((l & 1u) ^ flip[l >> 1]); };
    Inst h;
    h.n = g.n;
    for (u32 i : order) h.add(map(g.la[i]), map(g.lb[i]));
    const Run b = run(h);
    REQUIRE(b.res == TwoSatResult::Ok);
    REQUIRE(a.sat == b.sat);
    REQUIRE(certified(h, b, false));
  };
  for (u32 k = 0; k < 3000u; ++k) {
    const u32 n = 1u + r.rnd(12);
    Inst g;
    g.n = n;
    const u32 m = r.rnd(2u * n + 4u);
    for (u32 i = 0; i < m; ++i) {
      const u32 a = r.rnd(2u * n);
      const u32 b = r.rnd(2u * n);
      g.add(a, b);
    }
    check(g);
  }
  for (u32 pid : {2u, 3u, 4u, 6u, 10u, 11u, 12u, 13u, 14u}) check(big_inst(pid));
}

TEST_CASE("smallest sizes: one variable", "[twosat]") {
  auto make = [](u32 m) {
    Call c;
    c.n = 1;
    c.m = m;
    c.la = {0};
    c.lb = {1};
    c.scratch_words = 16u + 2u * m + 2u;
    c.scratch.assign(c.scratch_words, kGarbage);
    c.value.assign(1, kSentinel8);
    return c;
  };
  for (u32 m = 0; m < 2u; ++m) {
    Call ok = make(m);
    CHECK(ok.go() == TwoSatResult::Ok);
    CHECK(ok.sat == 1u);
    CHECK(ok.value[0] <= 1u);
    Call s = make(m);
    s.scratch_words -= 1u;
    expect_fail(s, TwoSatResult::ScratchTooSmall);
    Call b = make(m);
    b.lb[0] = 2;
    if (m == 1u) expect_fail(b, TwoSatResult::BadLiteral);
    Call v = make(m);
    v.null_value = true;
    expect_fail(v, TwoSatResult::NullArgument);
    Call sc = make(m);
    sc.null_scratch = true;
    expect_fail(sc, TwoSatResult::NullArgument);
  }
  Call a = make(1);
  a.null_la = true;
  expect_fail(a, TwoSatResult::NullArgument);
  Call b = make(1);
  b.null_lb = true;
  expect_fail(b, TwoSatResult::NullArgument);
}
