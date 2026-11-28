// Tests for core/farey. Expected values come from the header's examples, from test-only oracles of a different method than the library (the neighbour as the exact
// minimum or maximum over every denominator d in 1..n for n up to 120; the Haros recurrence, which uses no gcd, for the whole sequences; a complete property test for
// the sampled and edge cases: b c - a d = +-1, d + b > n, 1 <= d <= n, c <= d and gcd(c, d) = 1 by a binary gcd) and from FNV-1a 64-bit digests made by
// tools/farey_ref.py (Python standard library only: the brute-force sorted Fraction sequence, the scan over every denominator, the window property with pow(a, -1, b),
// and the Haros recurrence). Extents (see docs/farey.md): the check-order matrix; the header examples; every n in 1..120 with every (a, b) in [0, n + 3]^2 for both
// functions against the scan; whole-sequence walks for n = 1..400, 500, 777, 1000, 1500, 2000 and 3000 with the Haros recurrence, the predecessor checks in both
// directions and the term count 1 + sum of phi; 26 edge orders with edge and invalid fractions; 20000 sampled invalid fractions; 4 x 50000 sampled valid terms.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "farey.h"

namespace {
using lumora::farey::FResult;
using lumora::farey::farey_next;
using lumora::farey::farey_prev;
using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr u32 kN32 = 4294967295u;
constexpr u32 kGuardU = 0xDEADBEEFu;
constexpr u32 kCellC = 0x12345678u;
constexpr u32 kCellD = 0x0BADF00Du;

constexpr u64 kF1Digest = 0xf6939cada41c2e11ull;
constexpr u64 kF1Count = 643220u;
constexpr u64 kF3Digest = 0x60c0b34247b54e66ull;
constexpr u64 kF3Count = 2047u;
constexpr u64 kF4bDigest = 0x495726ef01546b82ull;
constexpr u64 kF4bCount = 20000u;
constexpr u64 kF4s0Digest = 0x77cc693fb47016fbull;
constexpr u64 kF4s0Count = 50000u;
constexpr u64 kF4s1Digest = 0x54a2b6a5409a71c7ull;
constexpr u64 kF4s1Count = 50000u;
constexpr u64 kF4s2Digest = 0x2643d4c0ed768d89ull;
constexpr u64 kF4s2Count = 50000u;
constexpr u64 kF4s3Digest = 0x05fb18969469d7e8ull;
constexpr u64 kF4s3Count = 50000u;
constexpr u64 kF2ADigest = 0xbc6dc8dcaefc6a88ull;
constexpr u64 kF2ACount = 6533695u;
constexpr u64 kF2BDigest = 0x8b19c5dd7abff23eull;
constexpr u64 kF2BCount = 1248246u;
constexpr u64 kF2CDigest = 0x9d1fb32373dff99dull;
constexpr u64 kF2CCount = 3952778u;

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

u32 code_of(FResult r) { return static_cast<u32>(r); }

// ---- library calls between guard words; a failure must leave both cells alone ----
struct Out {
  FResult r;
  u32 c;
  u32 d;
  bool guards;
};
Out call(bool next, u32 n, u32 a, u32 b) {
  u32 cc[3] = {kGuardU, kCellC, kGuardU};
  u32 dd[3] = {kGuardU, kCellD, kGuardU};
  const FResult r = next ? farey_next(n, a, b, &cc[1], &dd[1]) : farey_prev(n, a, b, &cc[1], &dd[1]);
  Out x{r, cc[1], dd[1], cc[0] == kGuardU && cc[2] == kGuardU && dd[0] == kGuardU && dd[2] == kGuardU};
  if (r != FResult::Ok) {
    x.guards = x.guards && cc[1] == kCellC && dd[1] == kCellD;
    x.c = 0u;
    x.d = 0u;
  }
  return x;
}

// ---- oracles ----
// Binary gcd (the library uses Euclid with remainders).
u64 o_gcd(u64 x, u64 y) {
  if (x == 0u) return y;
  if (y == 0u) return x;
  u32 sh = 0u;
  while (((x | y) & 1u) == 0u) {
    x >>= 1;
    y >>= 1;
    ++sh;
  }
  while ((x & 1u) == 0u) x >>= 1;
  while (y != 0u) {
    while ((y & 1u) == 0u) y >>= 1;
    if (x > y) {
      const u64 t = x;
      x = y;
      y = t;
    }
    y -= x;
  }
  return x << sh;
}
bool o_valid(u64 n, u64 a, u64 b) { return b != 0u && b <= n && a <= b && o_gcd(a, b) == 1u; }
bool o_end(u64 a, u64 b, bool next) { return next ? a == b : a == 0u; }
u32 o_code(u64 n, u64 a, u64 b, bool next) {
  if (n == 0u) return 2u;
  if (!o_valid(n, a, b)) return 3u;
  return o_end(a, b, next) ? 4u : 0u;
}
// The complete property test for a neighbour c / d of a / b in F_n.
bool o_prop(u64 n, u64 a, u64 b, bool next, u64 c, u64 d) {
  const u64 bc = b * c;
  const u64 ad = a * d;
  const bool det = next ? (bc == ad + 1u) : (ad == bc + 1u);
  const bool win = d >= 1u && d <= n && d + b > n;
  const bool low = c <= d && o_gcd(c, d) == 1u;
  return det && win && low;
}
// The exact definition by a scan over every denominator d in 1..n (n small): the least fraction above (or the greatest below) a / b.
void o_scan(u64 n, u64 a, u64 b, bool next, u64& bc, u64& bd) {
  bool have = false;
  bc = 0u;
  bd = 1u;
  for (u64 d = 1u; d <= n; ++d) {
    u64 c = 0u;
    if (next) {
      c = a * d / b + 1u;
      if (c > d) continue;
    } else {
      c = (a * d + b - 1u) / b;
      if (c == 0u) continue;
      c -= 1u;
    }
    const bool better = !have || (next ? (c * bd < bc * d) : (c * bd > bc * d));
    if (better) {
      bc = c;
      bd = d;
      have = true;
    }
  }
}

// One case through both functions: the code, the guards, the property test and the digest fold.
bool one_case(u64& h, u32 n, u32 a, u32 b) {
  bool ok = true;
  h = fold(fold(fold(h, n), a), b);
  for (u32 k = 0u; k < 2u; ++k) {
    const bool next = k == 0u;
    const Out o = call(next, n, a, b);
    const u32 want = o_code(n, a, b, next);
    if (!o.guards || code_of(o.r) != want) ok = false;
    if (want == 0u && !o_prop(n, a, b, next, o.c, o.d)) ok = false;
    h = fold(fold(fold(h, code_of(o.r)), o.c), o.d);
  }
  return ok;
}

// ---- case generators (the same as tools/farey_ref.py) ----
void vcase(u64 pos, u32& n, u32& a, u32& b) {
  const u64 w0 = splitmix_at(pos);
  const u64 w1 = splitmix_at(pos + 1u);
  const u64 w2 = splitmix_at(pos + 2u);
  const u64 w3 = splitmix_at(pos + 3u);
  u64 nb = 1u + w0 % 32u;
  if (((w0 >> 10) & 1u) != 0u) nb = 32u - ((w0 >> 11) % 4u);
  u64 nn = w1 & ((1ull << nb) - 1u);
  if (nn == 0u) nn = 1u;
  const u64 mode = (w0 >> 8) & 3u;
  const u64 lim = nn < 1000u ? nn : 1000u;
  u64 bb = nn;
  if (mode == 0u) bb = 1u + w2 % nn;
  if (mode == 1u) bb = nn - (w2 % lim);
  if (mode == 2u) bb = 1u + w2 % lim;
  u64 aa = w3 % (bb + 1u);
  while (o_gcd(aa, bb) != 1u) aa = (aa + 1u) % (bb + 1u);
  n = static_cast<u32>(nn);
  a = static_cast<u32>(aa);
  b = static_cast<u32>(bb);
}
u64 min_n32(u64 v) { return v < kN32 ? v : kN32; }
void icase(u64 pos, u32& n, u32& a, u32& b) {
  const u64 w0 = splitmix_at(pos);
  const u64 w1 = splitmix_at(pos + 1u);
  const u64 w2 = splitmix_at(pos + 2u);
  const u64 nb = 1u + w0 % 32u;
  u64 nn = w1 & ((1ull << nb) - 1u);
  if (nn == 0u) nn = 1u;
  const u64 t = (w0 >> 8) % 8u;
  u64 aa = 0u;
  u64 bb = 0u;
  if (t == 0u) {
    aa = w2 % (nn + 1u);
    bb = 0u;
  } else if (t == 1u) {
    aa = w2 % (nn + 1u);
    bb = min_n32(nn + 1u + w2 % 5u);
  } else if (t == 2u) {
    bb = w2 % (nn + 1u);
    aa = min_n32(bb + 1u + (w0 >> 16) % 5u);
  } else if (t == 3u) {
    const u64 m = (nn / 2u) > 2u ? (nn / 2u) : 2u;
    bb = 2u * m <= kN32 ? 2u * (1u + w2 % m) : 2u;
    bb = min_n32(bb);
    aa = (2u * (w0 >> 16)) % (bb + 1u);
    aa -= aa % 2u;
  } else if (t == 4u) {
    aa = kN32;
    bb = kN32;
  } else if (t == 5u) {
    aa = 0u;
    const u64 lim = nn < 1000u ? nn : 1000u;
    bb = 2u + w2 % (lim > 1u ? lim : 1u);
  } else if (t == 6u) {
    bb = 1u + w2 % nn;
    aa = min_n32(bb + 1u);
  } else {
    bb = nn;
    aa = w2 % (nn + 1u);
  }
  n = static_cast<u32>(nn);
  a = static_cast<u32>(aa);
  b = static_cast<u32>(bb);
}

struct Pair {
  i64 a;
  i64 b;
};
// The fractions of F3 for an order, in the order of the reference: edge fractions that are terms, then the invalid list.
u32 f3_list(i64 n, Pair* out) {
  u32 k = 0u;
  Pair c[128];
  u32 m = 0u;
  const i64 mid = (n - 1) / 2;
  const Pair base[12] = {{0, 1}, {1, 1}, {1, 2}, {1, 3}, {2, 3}, {1, n}, {1, n - 1}, {n - 1, n}, {n - 2, n - 1}, {n - 3, n - 2}, {mid, 2 * mid + 1}, {mid + 1, 2 * mid + 1}};
  for (u32 i = 0u; i < 12u; ++i) c[m++] = base[i];
  i64 f0 = 1;
  i64 f1 = 1;
  Pair fp[48];
  u32 nf = 0u;
  while (f1 <= n) {
    fp[nf++] = Pair{f0, f1};
    const i64 t = f0 + f1;
    f0 = f1;
    f1 = t;
  }
  for (u32 i = 0u; i < nf; ++i) c[m++] = fp[i];
  for (u32 i = 0u; i < nf; ++i) c[m++] = Pair{fp[i].b - fp[i].a, fp[i].b};
  for (u32 i = 0u; i < m; ++i) {
    const bool good = c[i].a >= 0 && c[i].b > 0 && o_valid(static_cast<u64>(n), static_cast<u64>(c[i].a), static_cast<u64>(c[i].b));
    if (good) out[k++] = c[i];
  }
  const i64 x = (n - 1) / 2;
  const i64 big = static_cast<i64>(kN32);
  const Pair inv[26] = {{0, 0}, {1, 0}, {0, 2}, {0, 3}, {0, n}, {0, n + 1}, {1, n + 1}, {n, n}, {n, n + 1}, {n + 1, n}, {n + 1, n + 1}, {n - 1, n}, {n, n - 1}, {2, 4},
                        {2, 2}, {4, 6}, {2 * x, 2 * (x + 1)}, {big, big}, {big - 1, big}, {big, big - 1}, {big - 1, big - 2}, {2, 2 * n}, {n, 2 * n}, {n + 2, n + 1}, {1, 1}, {0, 1}};
  for (u32 i = 0u; i < 26u; ++i) {
    if (inv[i].a >= 0 && inv[i].a <= big && inv[i].b >= 0 && inv[i].b <= big) out[k++] = inv[i];
  }
  return k;
}
constexpr u32 kEdgeN[26] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 10u, 100u, 255u, 256u, 257u, 1000u, 65535u, 65536u, 65537u, 1u << 20, (1u << 24) - 1u, 2147483647u, 2147483648u, 2147483649u,
                            4294967291u, 4294967293u, 4294967294u, 4294967295u};

// ---- whole-sequence walks (the Haros recurrence, no gcd) ----
bool walk(u32 n, u64& h, u64& terms) {
  bool ok = true;
  h = fold(h, n);
  u64 pa = 0u;
  u64 pb = 1u;
  u64 a = 1u;
  u64 b = n;
  h = fold(fold(h, 0u), 1u);
  ++terms;
  for (;;) {
    h = fold(fold(h, a), b);
    ++terms;
    const Out nx = call(true, n, static_cast<u32>(pa), static_cast<u32>(pb));
    const Out pv = call(false, n, static_cast<u32>(a), static_cast<u32>(b));
    if (!nx.guards || nx.r != FResult::Ok || nx.c != a || nx.d != b) ok = false;
    if (!pv.guards || pv.r != FResult::Ok || pv.c != pa || pv.d != pb) ok = false;
    if (a == 1u && b == 1u) break;
    const u64 k = (n + pb) / b;
    const u64 na = k * a - pa;
    const u64 nbb = k * b - pb;
    pa = a;
    pb = b;
    a = na;
    b = nbb;
  }
  const Out e1 = call(true, n, 1u, 1u);
  const Out e0 = call(false, n, 0u, 1u);
  if (e1.r != FResult::NoNeighbour || e0.r != FResult::NoNeighbour || !e1.guards || !e0.guards) ok = false;
  return ok;
}
// 1 + sum of phi(1..n) by a sieve.
u64 phi_total(u32 n) {
  static u32 phi[3001];  // test-only scratch
  for (u32 i = 0u; i <= n; ++i) phi[i] = i;
  u64 s = 1u;
  for (u32 i = 2u; i <= n; ++i) {
    if (phi[i] == i) {
      for (u32 j = i; j <= n; j += i) phi[j] -= phi[j] / i;
    }
  }
  for (u32 i = 1u; i <= n; ++i) s += phi[i];
  return s;
}
bool run_walks(const u32* ns, u32 cnt, u64 dig, u64 want_terms) {
  u64 h = kFnv0;
  u64 terms = 0u;
  bool ok = true;
  u64 sum = 0u;
  for (u32 i = 0u; i < cnt; ++i) {
    const u64 before = terms;
    if (!walk(ns[i], h, terms)) ok = false;
    if (terms - before != phi_total(ns[i])) ok = false;
    sum += terms - before;
  }
  return ok && h == dig && terms == want_terms && sum == want_terms;
}
}  // namespace

// Cheap cases first, the long walks last.
TEST_CASE("farey: header examples", "[farey]") {
  struct E {
    u32 n, a, b, nc, nd, pc, pd;
  };
  const E ex[] = {{5u, 1u, 2u, 3u, 5u, 2u, 5u},
                  {10u, 1u, 3u, 3u, 8u, 3u, 10u},
                  {100u, 22u, 67u, 23u, 70u, 21u, 64u},
                  {kN32, 1u, 2u, 2147483648u, kN32, 2147483647u, kN32},
                  {kN32, 65536u, 65537u, 4294836225u, 4294901759u, 4294901759u, 4294967294u},
                  {kN32, 1234567u, 7654321u, 691930712u, 4289973553u, 692018895u, 4290520288u},
                  {kN32, 2147483647u, kN32, 1u, 2u, 2147483646u, 4294967293u}};
  for (const E& e : ex) {
    const Out nx = call(true, e.n, e.a, e.b);
    const Out pv = call(false, e.n, e.a, e.b);
    const bool gn = nx.r == FResult::Ok && nx.guards && nx.c == e.nc && nx.d == e.nd;
    const bool gp = pv.r == FResult::Ok && pv.guards && pv.c == e.pc && pv.d == e.pd;
    CHECK(gn);
    CHECK(gp);
  }
  const Out a1 = call(true, 1u, 0u, 1u);
  const Out a2 = call(false, 1u, 0u, 1u);
  const Out a3 = call(true, 1u, 1u, 1u);
  const Out a4 = call(false, 1u, 1u, 1u);
  const bool b1 = a1.r == FResult::Ok && a1.c == 1u && a1.d == 1u && a1.guards;
  const bool b2 = a2.r == FResult::NoNeighbour && a2.guards;
  const bool b3 = a3.r == FResult::NoNeighbour && a3.guards;
  const bool b4 = a4.r == FResult::Ok && a4.c == 0u && a4.d == 1u && a4.guards;
  CHECK(b1);
  CHECK(b2);
  CHECK(b3);
  CHECK(b4);
}

TEST_CASE("farey: check order and no write on failure", "[farey]") {
  const u32 ns[6] = {0u, 1u, 2u, 5u, 100u, kN32};
  const u32 fa[10] = {0u, 1u, 1u, 0u, 3u, 2u, 4u, kN32, 0u, 7u};
  const u32 fb[10] = {1u, 1u, 2u, 0u, 2u, 4u, 6u, kN32, 3u, 7u};
  bool ok = true;
  for (u32 i = 0u; i < 6u; ++i) {
    for (u32 j = 0u; j < 10u; ++j) {
      for (u32 k = 0u; k < 2u; ++k) {
        const bool next = k == 0u;
        for (u32 nl = 0u; nl < 4u; ++nl) {
          u32 cc[3] = {kGuardU, kCellC, kGuardU};
          u32 dd[3] = {kGuardU, kCellD, kGuardU};
          u32* pc = (nl & 1u) != 0u ? nullptr : &cc[1];
          u32* pd = (nl & 2u) != 0u ? nullptr : &dd[1];
          const FResult r = next ? farey_next(ns[i], fa[j], fb[j], pc, pd) : farey_prev(ns[i], fa[j], fb[j], pc, pd);
          const u32 want = nl != 0u ? 1u : o_code(ns[i], fa[j], fb[j], next);
          if (code_of(r) != want) ok = false;
          const bool g = cc[0] == kGuardU && cc[2] == kGuardU && dd[0] == kGuardU && dd[2] == kGuardU;
          if (!g) ok = false;
          if (want != 0u && (cc[1] != kCellC || dd[1] != kCellD)) ok = false;
          if (want == 0u && !o_prop(ns[i], fa[j], fb[j], next, cc[1], dd[1])) ok = false;
        }
      }
    }
  }
  CHECK(ok);
}

TEST_CASE("farey: F3 edge orders and fractions", "[farey]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  Pair list[160];
  for (u32 i = 0u; i < 26u; ++i) {
    const u32 k = f3_list(static_cast<i64>(kEdgeN[i]), list);
    for (u32 j = 0u; j < k; ++j) {
      if (!one_case(h, kEdgeN[i], static_cast<u32>(list[j].a), static_cast<u32>(list[j].b))) ok = false;
      ++cnt;
    }
  }
  CHECK(ok);
  CHECK(cnt == kF3Count);
  CHECK(h == kF3Digest);
}

TEST_CASE("farey: F4b sampled invalid fractions", "[farey]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u64 i = 0u; i < kF4bCount; ++i) {
    u32 n = 0u;
    u32 a = 0u;
    u32 b = 0u;
    icase((1ull << 44) + 3u * i, n, a, b);
    if (!one_case(h, n, a, b)) ok = false;
  }
  CHECK(ok);
  CHECK(h == kF4bDigest);
}

TEST_CASE("farey: F1 every pair against the scan", "[farey]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  bool zero_ok = true;
  for (u32 n = 1u; n <= 120u; ++n) {
    for (u32 a = 0u; a < n + 4u; ++a) {
      for (u32 b = 0u; b < n + 4u; ++b) {
        ++cnt;
        if (!one_case(h, n, a, b)) ok = false;
        if (o_valid(n, a, b)) {
          for (u32 k = 0u; k < 2u; ++k) {
            const bool next = k == 0u;
            if (o_end(a, b, next)) continue;
            u64 sc = 0u;
            u64 sd = 0u;
            o_scan(n, a, b, next, sc, sd);
            const Out o = call(next, n, a, b);
            if (o.r != FResult::Ok || o.c != sc || o.d != sd) ok = false;
          }
        }
      }
    }
  }
  for (u32 a = 0u; a < 5u; ++a) {
    for (u32 b = 0u; b < 5u; ++b) {
      const Out o = call(true, 0u, a, b);
      const Out p = call(false, 0u, a, b);
      if (o.r != FResult::BadOrder || p.r != FResult::BadOrder || !o.guards || !p.guards) zero_ok = false;
    }
  }
  CHECK(ok);
  CHECK(zero_ok);
  CHECK(cnt == kF1Count);
  CHECK(h == kF1Digest);
}

TEST_CASE("farey: F4 sampled terms 0", "[farey]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u64 i = 0u; i < kF4s0Count; ++i) {
    u32 n = 0u;
    u32 a = 0u;
    u32 b = 0u;
    vcase((1ull << 40) + 0u * 300000u + 4u * i, n, a, b);
    if (!one_case(h, n, a, b)) ok = false;
  }
  CHECK(ok);
  CHECK(h == kF4s0Digest);
}
TEST_CASE("farey: F4 sampled terms 1", "[farey]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u64 i = 0u; i < kF4s1Count; ++i) {
    u32 n = 0u;
    u32 a = 0u;
    u32 b = 0u;
    vcase((1ull << 40) + 1u * 300000u + 4u * i, n, a, b);
    if (!one_case(h, n, a, b)) ok = false;
  }
  CHECK(ok);
  CHECK(h == kF4s1Digest);
}
TEST_CASE("farey: F4 sampled terms 2", "[farey]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u64 i = 0u; i < kF4s2Count; ++i) {
    u32 n = 0u;
    u32 a = 0u;
    u32 b = 0u;
    vcase((1ull << 40) + 2u * 300000u + 4u * i, n, a, b);
    if (!one_case(h, n, a, b)) ok = false;
  }
  CHECK(ok);
  CHECK(h == kF4s2Digest);
}
TEST_CASE("farey: F4 sampled terms 3", "[farey]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u64 i = 0u; i < kF4s3Count; ++i) {
    u32 n = 0u;
    u32 a = 0u;
    u32 b = 0u;
    vcase((1ull << 40) + 3u * 300000u + 4u * i, n, a, b);
    if (!one_case(h, n, a, b)) ok = false;
  }
  CHECK(ok);
  CHECK(h == kF4s3Digest);
}

TEST_CASE("farey: F2 walks n = 1..400", "[farey]") {
  u32 ns[400];
  for (u32 i = 0u; i < 400u; ++i) ns[i] = i + 1u;
  const bool ok = run_walks(ns, 400u, kF2ADigest, kF2ACount);
  CHECK(ok);
}
TEST_CASE("farey: F2 walks n = 500, 777, 1000, 1500", "[farey]") {
  const u32 ns[4] = {500u, 777u, 1000u, 1500u};
  const bool ok = run_walks(ns, 4u, kF2BDigest, kF2BCount);
  CHECK(ok);
}
TEST_CASE("farey: F2 walks n = 2000, 3000", "[farey]") {
  const u32 ns[2] = {2000u, 3000u};
  const bool ok = run_walks(ns, 2u, kF2CDigest, kF2CCount);
  CHECK(ok);
}
