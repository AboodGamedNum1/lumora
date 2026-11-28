// Tests for core/combrank. Expected values come from the header's examples, from a test-only Pascal triangle built by addition (a different method from the
// library's gcd updates: the oracle ranks by summing triangle entries and unranks by scanning them), from the definition (the k-subsets of n elements in numeric
// order of the masks get the ranks 0, 1, 2, ...), and from FNV-1a 64-bit digests made by tools/combrank_ref.py (Python standard library only, bigint math.comb).
// Extents (see docs/combrank.md): every n = 0..20 with every mask (rank, unrank, next and the order property); every (n, k) pair for the count; every mask of
// every (n, k) for n up to 16 against the oracle verdict; edge ranks and masks and next chains at every (n, k) up to 64; 200000 sampled (n, k, rank) triples
// and 200000 sampled masks; every subset of n = 64 for k = 1, 2, 3, 61, 62, 63, 64; the full check-order matrix; the bound of the binomial updates.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "combrank.h"

namespace {
using lumora::combrank::CbResult;
using lumora::combrank::combo_count;
using lumora::combrank::combo_next;
using lumora::combrank::combo_rank;
using lumora::combrank::combo_unrank;
using lumora::combrank::kMaxN;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kM64 = 18446744073709551615ull;
constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr u64 kGuard = 0xDEADBEEFCAFEF00Dull;
constexpr u64 kCell = 0x1234567812345678ull;
constexpr u64 kC6432 = 1832624140942590534ull;

constexpr u64 kCountDigest = 0x23525ba2d8ba160eull;
constexpr u64 kCountCount = 4233u;
constexpr u64 kExhADigest = 0x5ef7ed8c243340f2ull;
constexpr u64 kExhACount = 262143u;
constexpr u64 kExhBDigest = 0x4ab1ed230527e451ull;
constexpr u64 kExhBCount = 262144u;
constexpr u64 kExhCDigest = 0xa9462646adb60a4dull;
constexpr u64 kExhCCount = 524288u;
constexpr u64 kExhDDigest = 0xfa18fc557cb43af5ull;
constexpr u64 kExhDCount = 1048576u;
constexpr u64 kEdgeDigest = 0xcbd1d6933e1637a8ull;
constexpr u64 kEdgeCount = 17031u;
constexpr u64 kSamp0Digest = 0x35155a75e2e7783full;
constexpr u64 kSamp0Count = 50000u;
constexpr u64 kSamp1Digest = 0x941f2423c1ab7f6eull;
constexpr u64 kSamp1Count = 50000u;
constexpr u64 kSamp2Digest = 0xad6b237c5872f26dull;
constexpr u64 kSamp2Count = 50000u;
constexpr u64 kSamp3Digest = 0x0fad71338d970682ull;
constexpr u64 kSamp3Count = 50000u;
constexpr u64 kMasks0Digest = 0x2aac591e41310a51ull;
constexpr u64 kMasks0Count = 50000u;
constexpr u64 kMasks1Digest = 0xc16883922ce64b90ull;
constexpr u64 kMasks1Count = 50000u;
constexpr u64 kMasks2Digest = 0x74a364b631311603ull;
constexpr u64 kMasks2Count = 50000u;
constexpr u64 kMasks3Digest = 0xbbb0538c10271ec1ull;
constexpr u64 kMasks3Count = 50000u;
constexpr u64 kWrapDigest = 0x13b4c51901a2117cull;
constexpr u64 kWrapCount = 87489u;

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }
u64 code_of(CbResult r) { return static_cast<u64>(r); }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

u32 pop(u64 m) {
  u32 c = 0u;
  for (u32 i = 0u; i < 64u; ++i) c += static_cast<u32>((m >> i) & 1u);
  return c;
}
u64 lowbits(u32 n) { return n >= 64u ? kM64 : ((u64{1} << n) - 1u); }

// Test-only Pascal triangle by addition: P[n][k] = C(n, k), zero for k > n.
struct Pascal {
  u64 p[65][65];
  Pascal() {
    for (u32 n = 0u; n <= 64u; ++n)
      for (u32 k = 0u; k <= 64u; ++k) p[n][k] = 0u;
    for (u32 n = 0u; n <= 64u; ++n) {
      p[n][0] = 1u;
      for (u32 k = 1u; k <= n; ++k) p[n][k] = p[n - 1u][k - 1u] + p[n - 1u][k];
    }
  }
};
const Pascal& pas() {
  static const Pascal t;
  return t;
}

struct Or {
  CbResult r;
  u64 v;
};
Or o_count(u32 n, u32 k) {
  if (n > 64u || k > n) return {CbResult::BadArgument, 0u};
  return {CbResult::Ok, pas().p[n][k]};
}
bool o_mask_ok(u64 mask, u32 n, u32 k) { return (n >= 64u || (mask >> n) == 0u) && pop(mask) == k; }
Or o_rank(u64 mask, u32 n, u32 k) {
  if (n > 64u || k > n) return {CbResult::BadArgument, 0u};
  if (!o_mask_ok(mask, n, k)) return {CbResult::BadMask, 0u};
  u64 s = 0u;
  u32 j = 0u;
  for (u32 c = 0u; c < n; ++c) {
    if (((mask >> c) & 1u) != 0u) {
      ++j;
      s += pas().p[c][j];
    }
  }
  return {CbResult::Ok, s};
}
Or o_unrank(u32 n, u32 k, u64 rank) {
  if (n > 64u || k > n) return {CbResult::BadArgument, 0u};
  if (rank >= pas().p[n][k]) return {CbResult::RankTooLarge, 0u};
  u64 m = 0u;
  for (u32 j = k; j >= 1u; --j) {
    u32 c = j - 1u;
    while (c < 63u && pas().p[c + 1u][j] <= rank) ++c;
    rank -= pas().p[c][j];
    m |= u64{1} << c;
  }
  return {CbResult::Ok, m};
}
Or o_next(u64 mask, u32 n, u32 k) {
  const Or r = o_rank(mask, n, k);
  if (r.r != CbResult::Ok) return r;
  if (k == 0u || r.v + 1u == pas().p[n][k]) return {CbResult::NoNext, 0u};
  return o_unrank(n, k, r.v + 1u);
}

// Library calls through a result cell between guard words; v is 0 for a failure, and guards is false when a guard word moved or a failure wrote the cell.
struct Lr {
  CbResult r;
  u64 v;
  bool guards;
};
Lr finish(CbResult r, const u64* cell) {
  Lr x{r, 0u, cell[0] == kGuard && cell[2] == kGuard};
  if (r == CbResult::Ok) {
    x.v = cell[1];
  } else {
    x.guards = x.guards && cell[1] == kCell;
  }
  return x;
}
Lr l_count(u32 n, u32 k) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const CbResult r = combo_count(n, k, &cell[1]);
  return finish(r, cell);
}
Lr l_rank(u64 mask, u32 n, u32 k) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const CbResult r = combo_rank(mask, n, k, &cell[1]);
  return finish(r, cell);
}
Lr l_unrank(u32 n, u32 k, u64 rank) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const CbResult r = combo_unrank(n, k, rank, &cell[1]);
  return finish(r, cell);
}
Lr l_next(u64 mask, u32 n, u32 k) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const CbResult r = combo_next(mask, n, k, &cell[1]);
  return finish(r, cell);
}
bool same(const Lr& l, const Or& o) { return l.guards && l.r == o.r && l.v == o.v; }

// ---- digest families (the folds match tools/combrank_ref.py exactly) ----

void count_pair(u32 n, u32 k, u64& h, bool& ok) {
  const Lr l = l_count(n, k);
  const Or o = o_count(n, k);
  if (!same(l, o)) ok = false;
  h = fold(fold(h, code_of(l.r)), l.v);
}

void run_count(u64 want, u64 want_count) {
  u64 h = kFnv0;
  bool ok = true;
  u64 cnt = 0u;
  for (u32 n = 0u; n <= 64u; ++n)
    for (u32 k = 0u; k <= 64u; ++k) {
      count_pair(n, k, h, ok);
      ++cnt;
    }
  const u32 ex[8][2] = {{65u, 0u}, {65u, 65u}, {70u, 3u}, {255u, 1u}, {4294967295u, 0u}, {64u, 65u}, {0u, 1u}, {4294967295u, 4294967295u}};
  for (const auto& e : ex) {
    count_pair(e[0], e[1], h, ok);
    ++cnt;
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want);
}

// Every mask of n bits for n = lo..hi, in numeric order. The k-th class is visited in order, so the index of a mask in its class is its rank (the order
// property); rank, unrank and next are compared with that index, with the previous mask of the class and with the oracle.
void run_exhaustive(u32 lo, u32 hi, u64 want, u64 want_count) {
  u64 h = kFnv0;
  bool ok = true;
  u64 cnt = 0u;
  for (u32 n = lo; n <= hi; ++n) {
    u64 idx[21];
    u64 prev[21];
    for (u32 k = 0u; k <= 20u; ++k) {
      idx[k] = 0u;
      prev[k] = 0u;
    }
    const u64 total = u64{1} << n;
    for (u64 mask = 0u; mask < total; ++mask) {
      const u32 k = pop(mask);
      const u64 i = idx[k];
      const Lr rk = l_rank(mask, n, k);
      const Lr un = l_unrank(n, k, i);
      if (!(rk.guards && rk.r == CbResult::Ok && rk.v == i)) ok = false;
      if (!(un.guards && un.r == CbResult::Ok && un.v == mask)) ok = false;
      if (i > 0u) {
        const Lr nx = l_next(prev[k], n, k);
        if (!(nx.guards && nx.r == CbResult::Ok && nx.v == mask)) ok = false;
      }
      h = fold(fold(fold(h, n), mask), rk.v);
      ++cnt;
      prev[k] = mask;
      idx[k] = i + 1u;
    }
    for (u32 k = 0u; k <= n; ++k) {
      if (idx[k] != pas().p[n][k]) ok = false;
      const Lr nx = l_next(prev[k], n, k);
      if (!(nx.guards && nx.r == CbResult::NoNext)) ok = false;
      const Lr big = l_unrank(n, k, idx[k]);
      if (!(big.guards && big.r == CbResult::RankTooLarge)) ok = false;
      const Lr huge = l_unrank(n, k, kM64);
      if (!(huge.guards && huge.r == CbResult::RankTooLarge)) ok = false;
    }
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want);
}

u64 first_mask(u32 k) { return lowbits(k); }
u64 last_mask(u32 n, u32 k) { return k == 0u ? 0u : (lowbits(k) << (n - k)); }

void edge_rank_fold(u64 mask, u32 n, u32 k, u64& h, bool& ok) {
  const Lr rk = l_rank(mask, n, k);
  if (!same(rk, o_rank(mask, n, k))) ok = false;
  h = fold(fold(h, code_of(rk.r)), rk.v);
  const Lr nx = l_next(mask, n, k);
  if (!same(nx, o_next(mask, n, k))) ok = false;
  h = fold(fold(h, code_of(nx.r)), nx.v);
}

void run_edge(u64 want, u64 want_count) {
  u64 h = kFnv0;
  bool ok = true;
  u64 cnt = 0u;
  for (u32 n = 0u; n <= 64u; ++n)
    for (u32 k = 0u; k <= n; ++k) {
      const u64 c = pas().p[n][k];
      edge_rank_fold(first_mask(k), n, k, h, ok);
      edge_rank_fold(last_mask(n, k), n, k, h, ok);
      const u64 ranks[8] = {0u, 1u, 2u, c - 2u, c - 1u, c, c + 1u, kM64};
      for (u32 t = 0u; t < 8u; ++t) {
        if (t == 3u && c < 2u) continue;
        const u64 r = ranks[t];
        const Lr un = l_unrank(n, k, r);
        if (!same(un, o_unrank(n, k, r))) ok = false;
        h = fold(fold(h, code_of(un.r)), un.v);
        if (un.r == CbResult::Ok) {
          const Lr rk = l_rank(un.v, n, k);
          if (!(rk.guards && rk.r == CbResult::Ok && rk.v == r)) ok = false;
          h = fold(h, rk.v);
        }
        ++cnt;
      }
      const u64 back = c > 65u ? c - 65u : 0u;
      u64 starts[2] = {first_mask(k), 0u};
      const Lr s2 = l_unrank(n, k, back);
      if (!(s2.guards && s2.r == CbResult::Ok)) ok = false;
      starts[1] = s2.v;
      for (u32 s = 0u; s < 2u; ++s) {
        u64 m = starts[s];
        for (u32 step = 0u; step < 130u; ++step) {
          const Lr nx = l_next(m, n, k);
          if (!same(nx, o_next(m, n, k))) ok = false;
          h = fold(fold(h, code_of(nx.r)), nx.v);
          if (nx.r != CbResult::Ok) break;
          m = nx.v;
        }
      }
    }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want);
}

void run_samp(u32 s, u64 want, u64 want_count) {
  u64 h = kFnv0;
  bool ok = true;
  u64 cnt = 0u;
  for (u64 i = 0u; i < 50000u; ++i) {
    const u64 p = (u64{1} << 40) + u64{s} * 100000u + 2u * i;
    const u64 w0 = splitmix_at(p);
    const u64 w1 = splitmix_at(p + 1u);
    const u32 n = static_cast<u32>(w0 % 65u);
    const u32 k = static_cast<u32>((w0 >> 8) % (n + 1u));
    const u64 t = (w0 >> 16) & 3u;
    const u64 c = pas().p[n][k];
    const u64 rank = t == 0u ? w1 : w1 % c;
    const Lr un = l_unrank(n, k, rank);
    if (!same(un, o_unrank(n, k, rank))) ok = false;
    h = fold(fold(fold(fold(fold(h, n), k), rank), code_of(un.r)), un.v);
    if (un.r == CbResult::Ok) {
      const Lr rk = l_rank(un.v, n, k);
      if (!(rk.guards && rk.r == CbResult::Ok && rk.v == rank)) ok = false;
      h = fold(h, rk.v);
      const Lr nx = l_next(un.v, n, k);
      if (!same(nx, o_next(un.v, n, k))) ok = false;
      h = fold(fold(h, code_of(nx.r)), nx.v);
    }
    ++cnt;
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want);
}

void run_masks(u32 s, u64 want, u64 want_count) {
  u64 h = kFnv0;
  bool ok = true;
  u64 cnt = 0u;
  for (u64 i = 0u; i < 50000u; ++i) {
    const u64 p = (u64{1} << 41) + u64{s} * 100000u + 2u * i;
    const u64 w0 = splitmix_at(p);
    const u64 w1 = splitmix_at(p + 1u);
    const u32 n = 1u + static_cast<u32>(w0 % 64u);
    const u32 k = static_cast<u32>((w0 >> 8) % (n + 1u));
    u64 m = w1 & lowbits(n);
    while (pop(m) > k) m &= m - 1u;
    while (pop(m) < k) {
      const u64 z = ~m & lowbits(n);
      m |= z & (~z + 1u);
    }
    const Lr rk = l_rank(m, n, k);
    if (!same(rk, o_rank(m, n, k))) ok = false;
    h = fold(fold(fold(fold(h, n), m), code_of(rk.r)), rk.v);
    if (rk.r == CbResult::Ok) {
      const Lr un = l_unrank(n, k, rk.v);
      if (!(un.guards && un.r == CbResult::Ok && un.v == m)) ok = false;
    }
    const Lr nx = l_next(m, n, k);
    if (!same(nx, o_next(m, n, k))) ok = false;
    h = fold(fold(h, code_of(nx.r)), nx.v);
    ++cnt;
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want);
}

// Every subset of n = 64 for these k, in numeric order produced by the oracle's unrank: rank, unrank and next agree with the index.
void run_wrap(u64 want, u64 want_count) {
  const u32 ks[7] = {1u, 2u, 3u, 61u, 62u, 63u, 64u};
  u64 h = kFnv0;
  bool ok = true;
  u64 cnt = 0u;
  for (u32 t = 0u; t < 7u; ++t) {
    const u32 k = ks[t];
    const u64 c = pas().p[64][k];
    u64 prev = 0u;
    for (u64 i = 0u; i < c; ++i) {
      const Or m = o_unrank(64u, k, i);
      if (m.r != CbResult::Ok) ok = false;
      h = fold(h, m.v);
      ++cnt;
      const Lr rk = l_rank(m.v, 64u, k);
      if (!(rk.guards && rk.r == CbResult::Ok && rk.v == i)) ok = false;
      const Lr un = l_unrank(64u, k, i);
      if (!(un.guards && un.r == CbResult::Ok && un.v == m.v)) ok = false;
      if (i > 0u) {
        if (!(prev < m.v)) ok = false;
        const Lr nx = l_next(prev, 64u, k);
        if (!(nx.guards && nx.r == CbResult::Ok && nx.v == m.v)) ok = false;
      }
      prev = m.v;
    }
    const Lr last = l_next(prev, 64u, k);
    if (!(last.guards && last.r == CbResult::NoNext)) ok = false;
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want);
}

// The bound of the binomial updates: a test-only copy of the library's update recipe with checked arithmetic. Every division must be exact, every product must
// not overflow and no value may exceed C(64, 32); the largest value over all (n, k) must be exactly C(64, 32).
struct Meter {
  u64 maxv = 0u;
  bool exact = true;
  bool noover = true;
  void see(u64 v) {
    if (v > maxv) maxv = v;
  }
  u64 gcd(u64 a, u64 b) const {
    while (b != 0u) {
      const u64 t = a % b;
      a = b;
      b = t;
    }
    return a;
  }
  u64 mul(u64 a, u64 b) {
    if (a != 0u && ((a * b) / a != b)) noover = false;
    const u64 p = a * b;
    see(p);
    return p;
  }
  u64 quo(u64 a, u64 b) {
    if (a % b != 0u) exact = false;
    return a / b;
  }
  u64 binom(u32 n, u32 k) {
    if (k > n) return 0u;
    if (k > n - k) k = n - k;
    u64 c = 1u;
    for (u32 i = 1u; i <= k; ++i) {
      const u64 m = u64{n - k} + i;
      const u64 g = gcd(c, i);
      c = mul(c / g, quo(m, i / g));
    }
    return c;
  }
  u64 down(u64 b, u32 c, u32 j) {
    const u64 g = gcd(b, c);
    return mul(b / g, quo(u64{c - j}, c / g));
  }
  u64 diag(u64 b, u32 c, u32 j) {
    const u64 g = gcd(b, c);
    return mul(b / g, quo(u64{j}, c / g));
  }
  // the rank walk along a mask
  void walk(u64 mask, u32 n, u32 k) {
    if (k == 0u) return;
    u32 c = n - 1u;
    u32 j = k;
    u64 b = binom(c, j);
    for (;;) {
      if (((mask >> c) & 1u) != 0u) {
        if (j == 1u) break;
        b = diag(b, c, j);
        --j;
      } else {
        b = down(b, c, j);
      }
      --c;
    }
  }
};

}  // namespace

TEST_CASE("header examples", "[combrank]") {
  CHECK(same(l_count(0u, 0u), Or{CbResult::Ok, 1u}));
  CHECK(same(l_count(5u, 2u), Or{CbResult::Ok, 10u}));
  CHECK(same(l_count(10u, 4u), Or{CbResult::Ok, 210u}));
  CHECK(same(l_count(64u, 0u), Or{CbResult::Ok, 1u}));
  CHECK(same(l_count(64u, 1u), Or{CbResult::Ok, 64u}));
  CHECK(same(l_count(64u, 2u), Or{CbResult::Ok, 2016u}));
  CHECK(same(l_count(64u, 16u), Or{CbResult::Ok, 488526937079580ull}));
  CHECK(same(l_count(64u, 32u), Or{CbResult::Ok, kC6432}));
  CHECK(same(l_count(63u, 31u), Or{CbResult::Ok, 916312070471295267ull}));
  CHECK(same(l_count(64u, 64u), Or{CbResult::Ok, 1u}));
  const u64 five[10] = {0x03u, 0x05u, 0x06u, 0x09u, 0x0Au, 0x0Cu, 0x11u, 0x12u, 0x14u, 0x18u};
  for (u32 i = 0u; i < 10u; ++i) {
    CHECK(same(l_rank(five[i], 5u, 2u), Or{CbResult::Ok, i}));
    CHECK(same(l_unrank(5u, 2u, i), Or{CbResult::Ok, five[i]}));
    if (i < 9u) CHECK(same(l_next(five[i], 5u, 2u), Or{CbResult::Ok, five[i + 1u]}));
  }
  CHECK(same(l_next(0x18u, 5u, 2u), Or{CbResult::NoNext, 0u}));
  CHECK(same(l_rank(0x2Du, 6u, 4u), Or{CbResult::Ok, 7u}));
  CHECK(same(l_rank(u64{1} << 63, 64u, 1u), Or{CbResult::Ok, 63u}));
  CHECK(same(l_rank(0xFFFFFFFF00000000ull, 64u, 32u), Or{CbResult::Ok, kC6432 - 1u}));
  CHECK(same(l_rank(0u, 7u, 0u), Or{CbResult::Ok, 0u}));
  CHECK(same(l_rank(kM64, 64u, 64u), Or{CbResult::Ok, 0u}));
  CHECK(same(l_unrank(10u, 4u, 100u), Or{CbResult::Ok, 353u}));
  CHECK(same(l_unrank(64u, 2u, 2015u), Or{CbResult::Ok, 0xC000000000000000ull}));
  CHECK(same(l_unrank(64u, 32u, kC6432 - 1u), Or{CbResult::Ok, 0xFFFFFFFF00000000ull}));
  CHECK(same(l_unrank(64u, 10u, 100000000000ull), Or{CbResult::Ok, 0x2211000002221101ull}));
  CHECK(same(l_unrank(5u, 2u, 10u), Or{CbResult::RankTooLarge, 0u}));
  CHECK(same(l_next(0x03u, 5u, 2u), Or{CbResult::Ok, 0x05u}));
  CHECK(same(l_next(0x0Cu, 5u, 2u), Or{CbResult::Ok, 0x11u}));
  CHECK(same(l_next(u64{1} << 62, 64u, 1u), Or{CbResult::Ok, u64{1} << 63}));
  CHECK(same(l_next(u64{1} << 63, 64u, 1u), Or{CbResult::NoNext, 0u}));
  CHECK(same(l_next(0x7FFFFFFFFFFFFFFFull, 64u, 63u), Or{CbResult::Ok, 0xBFFFFFFFFFFFFFFFull}));
  CHECK(same(l_next(0xFFFFFFFF00000000ull, 64u, 32u), Or{CbResult::NoNext, 0u}));
  CHECK(same(l_next(0xFFFFFFFFull, 64u, 32u), Or{CbResult::Ok, 0x17FFFFFFFull}));
  CHECK(kMaxN == 64u);
}

TEST_CASE("count: every (n, k) pair and larger n", "[combrank]") { run_count(kCountDigest, kCountCount); }

TEST_CASE("count: Pascal identity, symmetry and the bound", "[combrank]") {
  bool ok = true;
  u64 top = 0u;
  for (u32 n = 0u; n <= 64u; ++n)
    for (u32 k = 0u; k <= n; ++k) {
      const Lr a = l_count(n, k);
      const Lr b = l_count(n, n - k);
      if (!(a.guards && a.r == CbResult::Ok && b.guards && b.r == CbResult::Ok && a.v == b.v)) ok = false;
      if (a.v > top) top = a.v;
      if (n > 0u && k > 0u && k < n) {
        const Lr x = l_count(n - 1u, k - 1u);
        const Lr y = l_count(n - 1u, k);
        if (a.v != x.v + y.v) ok = false;
      }
      if (k == 0u || k == n) {
        if (a.v != 1u) ok = false;
      }
    }
  CHECK(ok);
  CHECK(top == kC6432);
  CHECK(top < (u64{1} << 61));
}

TEST_CASE("every mask of n = 0..17", "[combrank]") { run_exhaustive(0u, 17u, kExhADigest, kExhACount); }
TEST_CASE("every mask of n = 18", "[combrank]") { run_exhaustive(18u, 18u, kExhBDigest, kExhBCount); }
TEST_CASE("every mask of n = 19", "[combrank]") { run_exhaustive(19u, 19u, kExhCDigest, kExhCCount); }
TEST_CASE("every mask of n = 20", "[combrank]") { run_exhaustive(20u, 20u, kExhDDigest, kExhDCount); }

TEST_CASE("bad popcount: every mask of n up to 16 with every k", "[combrank]") {
  bool ok = true;
  for (u32 n = 0u; n <= 16u; ++n)
    for (u32 k = 0u; k <= n; ++k)
      for (u64 m = 0u; m < (u64{1} << n); ++m) {
        if (pop(m) == k) continue;
        const Lr a = l_rank(m, n, k);
        const Lr b = l_next(m, n, k);
        if (!(a.guards && a.r == CbResult::BadMask && b.guards && b.r == CbResult::BadMask)) ok = false;
      }
  CHECK(ok);
}

TEST_CASE("bad masks: a bit at n or above, the wrong popcount, at every (n, k)", "[combrank]") {
  bool ok = true;
  for (u32 n = 0u; n <= 64u; ++n)
    for (u32 k = 0u; k <= n; ++k) {
      const u64 good = first_mask(k);
      u64 bad[6] = {0u, kM64, good | (u64{1} << (n % 64u)), good ^ 1u, good << 1, (k > 0u ? good >> 1 : u64{1})};
      for (u32 t = 0u; t < 6u; ++t) {
        const Lr a = l_rank(bad[t], n, k);
        const Lr b = l_next(bad[t], n, k);
        const Or oa = o_rank(bad[t], n, k);
        const Or ob = o_next(bad[t], n, k);
        if (!(same(a, oa) && same(b, ob))) ok = false;
      }
      if (n < 64u) {
        // a single extra bit exactly at position n, with the right popcount (one bit moved out of range)
        if (k > 0u) {
          const u64 high = (good >> 1) | (u64{1} << n);
          const Lr a = l_rank(high, n, k);
          const Lr b = l_next(high, n, k);
          if (!(a.guards && a.r == CbResult::BadMask && b.guards && b.r == CbResult::BadMask)) ok = false;
        }
        const Lr c = l_rank(u64{1} << n, n, k);
        if (!(c.guards && c.r == CbResult::BadMask)) ok = false;
      }
    }
  CHECK(ok);
}

TEST_CASE("check order matrix: null, n, k, mask, rank, last", "[combrank]") {
  const u32 ns[7] = {0u, 1u, 10u, 64u, 65u, 255u, 4294967295u};
  const u32 ks[8] = {0u, 1u, 4u, 10u, 11u, 64u, 65u, 4294967295u};
  const u64 ms[9] = {0u, 1u, 0xFu, 0x3FFu, 0x400u, 0x3C0u, 0x8000000000000000ull, kM64, 0x7FFFFFFFFFFFFFFFull};
  bool ok = true;
  for (u32 a = 0u; a < 7u; ++a)
    for (u32 b = 0u; b < 8u; ++b) {
      const u32 n = ns[a];
      const u32 k = ks[b];
      const u64 c = (n <= 64u && k <= n) ? pas().p[n][k] : 0u;
      const u64 rks[6] = {0u, 1u, c - 1u, c, c + 1u, kM64};
      for (u32 t = 0u; t < 6u; ++t) {
        // null output pointer: NullArgument first, whatever the other arguments are
        if (combo_count(n, k, nullptr) != CbResult::NullArgument) ok = false;
        if (combo_unrank(n, k, rks[t], nullptr) != CbResult::NullArgument) ok = false;
        const Lr u = l_unrank(n, k, rks[t]);
        if (!same(u, o_unrank(n, k, rks[t]))) ok = false;
      }
      const Lr cnt = l_count(n, k);
      if (!same(cnt, o_count(n, k))) ok = false;
      for (u32 t = 0u; t < 9u; ++t) {
        if (combo_rank(ms[t], n, k, nullptr) != CbResult::NullArgument) ok = false;
        if (combo_next(ms[t], n, k, nullptr) != CbResult::NullArgument) ok = false;
        if (!same(l_rank(ms[t], n, k), o_rank(ms[t], n, k))) ok = false;
        if (!same(l_next(ms[t], n, k), o_next(ms[t], n, k))) ok = false;
      }
    }
  CHECK(ok);
  // the failure order spelled out: n bad beats a bad mask and a bad rank, a bad mask beats a rank, the rank beats nothing else
  CHECK(l_rank(kM64, 65u, 3u).r == CbResult::BadArgument);
  CHECK(l_next(kM64, 10u, 11u).r == CbResult::BadArgument);
  CHECK(l_unrank(65u, 0u, kM64).r == CbResult::BadArgument);
  CHECK(l_unrank(10u, 11u, 0u).r == CbResult::BadArgument);
  CHECK(l_rank(0x400u, 10u, 1u).r == CbResult::BadMask);
  CHECK(l_next(0x400u, 10u, 1u).r == CbResult::BadMask);
  CHECK(l_next(0x1u, 10u, 0u).r == CbResult::BadMask);
  CHECK(l_next(0u, 10u, 0u).r == CbResult::NoNext);
  CHECK(l_unrank(10u, 0u, 0u).r == CbResult::Ok);
  CHECK(l_unrank(10u, 0u, 1u).r == CbResult::RankTooLarge);
  CHECK(l_unrank(0u, 0u, 0u).v == 0u);
  CHECK(l_unrank(64u, 32u, kC6432).r == CbResult::RankTooLarge);
  CHECK(l_unrank(64u, 32u, kC6432 - 1u).r == CbResult::Ok);
  CHECK(l_unrank(64u, 64u, 0u).v == kM64);
  CHECK(l_unrank(64u, 64u, 1u).r == CbResult::RankTooLarge);
  CHECK(l_unrank(64u, 0u, kM64).r == CbResult::RankTooLarge);
  CHECK(l_rank(0u, 0u, 0u).r == CbResult::Ok);
  CHECK(l_next(kM64, 64u, 64u).r == CbResult::NoNext);
  CHECK(l_next(lowbits(7u), 7u, 7u).r == CbResult::NoNext);
}

TEST_CASE("edges at every (n, k): first and last masks, edge ranks, next chains", "[combrank]") { run_edge(kEdgeDigest, kEdgeCount); }

TEST_CASE("sampled triples 0", "[combrank]") { run_samp(0u, kSamp0Digest, kSamp0Count); }
TEST_CASE("sampled triples 1", "[combrank]") { run_samp(1u, kSamp1Digest, kSamp1Count); }
TEST_CASE("sampled triples 2", "[combrank]") { run_samp(2u, kSamp2Digest, kSamp2Count); }
TEST_CASE("sampled triples 3", "[combrank]") { run_samp(3u, kSamp3Digest, kSamp3Count); }
TEST_CASE("sampled masks 0", "[combrank]") { run_masks(0u, kMasks0Digest, kMasks0Count); }
TEST_CASE("sampled masks 1", "[combrank]") { run_masks(1u, kMasks1Digest, kMasks1Count); }
TEST_CASE("sampled masks 2", "[combrank]") { run_masks(2u, kMasks2Digest, kMasks2Count); }
TEST_CASE("sampled masks 3", "[combrank]") { run_masks(3u, kMasks3Digest, kMasks3Count); }

TEST_CASE("n = 64: every subset for k = 1, 2, 3, 61, 62, 63, 64, and the step that adds the lowest bit", "[combrank]") { run_wrap(kWrapDigest, kWrapCount); }

TEST_CASE("the update recipe: exact divisions, no overflow, every value at most C(64, 32)", "[combrank]") {
  Meter m;
  for (u32 n = 0u; n <= 64u; ++n)
    for (u32 k = 0u; k <= n; ++k) {
      const u64 b = m.binom(n, k);
      if (b != pas().p[n][k]) m.exact = false;
      m.see(b);
      if (n == 0u) continue;
      const u64 c = pas().p[n][k];
      const u64 rs[4] = {0u, c - 1u, c / 2u, c / 3u};
      for (u32 t = 0u; t < 4u; ++t) {
        const Or u = o_unrank(n, k, rs[t]);
        if (u.r == CbResult::Ok) m.walk(u.v, n, k);
      }
    }
  CHECK(m.exact);
  CHECK(m.noover);
  CHECK(m.maxv == kC6432);
}
