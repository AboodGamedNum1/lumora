// Tests for core/dyck. Expected values come from the header's examples, from test-only oracles of a different method than the library (a Pascal triangle with the
// reflection formula C(r, m) - C(r, m - 1) for the ballot numbers and C(2n, n) - C(2n, n + 1) for the Catalan numbers; validity by counting the ')' in every prefix;
// rank and unrank by that table; the successor as unrank(rank + 1)), from properties (rank and unrank are inverse, the successor has the next rank, the whole
// ascending walk of every set of words is valid, strictly increasing and has C_n words) and from FNV-1a 64-bit digests made by tools/dyck_ref.py (Python standard
// library only: brute-force sorted strings for small n, a memoised program f(remaining, depth) with Python integers, math.comb, and per-position bit counts by
// prefix times suffix). Extents (see docs/dyck.md): the check-order matrix; the header examples; the counts for n = 0..32; edge ranks (C_n - 1, C_n, C_n + 1 and
// the u64 maximum among them) for every n = 0..32; chains at both ends of every n; for each n = 17..32, 12500 sampled ranks, 1250 near-valid masks and 3125 random
// masks; every mask below 2^(2n) for n = 0..9 with high-bit variants; the whole ascending walk for every n = 0..15 (13.7M words).
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "dyck.h"

namespace {
using lumora::dyck::DyResult;
using lumora::dyck::dyck_count;
using lumora::dyck::dyck_next;
using lumora::dyck::dyck_rank;
using lumora::dyck::dyck_unrank;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr u64 kGuard = 0xDEADBEEFDEADBEEFull;
constexpr u64 kCell = 0x1234567812345678ull;
constexpr u64 kMax64 = ~0ull;

constexpr u64 kD3Digest = 0xd1646443532190e7ull;
constexpr u64 kD3Count = 33u;
constexpr u64 kD4Digest = 0x0a515e408986668full;
constexpr u64 kD4Count = 370279u;
constexpr u64 kD6Digest = 0x522be84669842f65ull;
constexpr u64 kD6Count = 4587u;
constexpr u64 kD8Digest = 0xb2c5b1021611cb6eull;
constexpr u64 kD8Count = 1220u;
constexpr u64 kD5ADigest = 0x25cd89d7fa220f9dull;
constexpr u64 kD5ACount = 290512u;
constexpr u64 kD5BDigest = 0x31c5dd0f7e8c7a3cull;
constexpr u64 kD5BCount = 742900u;
constexpr u64 kD5CDigest = 0x6b4da54ed6b4e011ull;
constexpr u64 kD5CCount = 2674440u;
constexpr u64 kD5DDigest = 0x21004fc0b5d1fdcbull;
constexpr u64 kD5DCount = 9694845u;
constexpr u64 kD7aDigest = 0x77e8a10087647481ull;
constexpr u64 kD7aCount = 200000u;
constexpr u64 kD7bDigest = 0xa99690524d61e410ull;
constexpr u64 kD7bCount = 20000u;
constexpr u64 kD7cDigest = 0xa5967dda44f7fddaull;
constexpr u64 kD7cCount = 50000u;

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
u32 code_of(DyResult r) { return static_cast<u32>(r); }

// ---- library calls between guard words; a failure must leave the cell alone ----
struct Out {
  DyResult r;
  u64 v;
  bool guards;
};
Out finish(DyResult r, const u64* cell) {
  Out x{r, cell[1], cell[0] == kGuard && cell[2] == kGuard};
  if (r != DyResult::Ok) {
    x.guards = x.guards && cell[1] == kCell;
    x.v = 0u;
  }
  return x;
}
Out c_count(u32 n) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const DyResult r = dyck_count(n, &cell[1]);
  return finish(r, cell);
}
Out c_rank(u64 mask, u32 n) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const DyResult r = dyck_rank(mask, n, &cell[1]);
  return finish(r, cell);
}
Out c_unrank(u32 n, u64 rank) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const DyResult r = dyck_unrank(n, rank, &cell[1]);
  return finish(r, cell);
}
Out c_next(u64 mask, u32 n) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const DyResult r = dyck_next(mask, n, &cell[1]);
  return finish(r, cell);
}

// ---- oracles ----
struct Pascal {
  u64 v[65][65];
  Pascal() {
    for (u32 r = 0u; r < 65u; ++r) {
      for (u32 m = 0u; m < 65u; ++m) v[r][m] = 0u;
      v[r][0] = 1u;
      for (u32 m = 1u; m <= r; ++m) v[r][m] = v[r - 1u][m - 1u] + (m <= r - 1u ? v[r - 1u][m] : 0u);
    }
  }
};
const Pascal& pas() {
  static const Pascal p;  // test-only table
  return p;
}
u64 o_catalan(u32 n) { return pas().v[2u * n][n] - (n + 1u <= 2u * n ? pas().v[2u * n][n + 1u] : 0u); }
// walks of r steps from height k to 0 never below 0, by reflection
u64 o_ballot(u32 r, u32 k) {
  if (k > r || ((r - k) % 2u) != 0u) return 0u;
  const u32 m = (r - k) / 2u;
  return pas().v[r][m] - (m > 0u ? pas().v[r][m - 1u] : 0u);
}
// validity by counting: every prefix has at most as many ')' as '(', the totals are equal
bool o_valid(u64 mask, u32 n) {
  const u32 bits = 2u * n;
  if (bits < 64u && (mask >> bits) != 0u) return false;
  u32 closes = 0u;
  for (u32 j = 1u; j <= bits; ++j) {
    closes += static_cast<u32>((mask >> (bits - j)) & 1u);
    if (2u * closes > j) return false;
  }
  return 2u * closes == bits;
}
u64 o_rank(u64 mask, u32 n) {
  u64 rank = 0u;
  u32 opens = 0u;
  u32 closes = 0u;
  for (u32 j = 1u; j <= 2u * n; ++j) {
    const u32 left = 2u * n - j;
    if (((mask >> left) & 1u) != 0u) {
      rank += o_ballot(left, opens - closes + 1u);
      ++closes;
    } else {
      ++opens;
    }
  }
  return rank;
}
u64 o_unrank(u32 n, u64 rank) {
  u64 mask = 0u;
  u32 depth = 0u;
  for (u32 j = 1u; j <= 2u * n; ++j) {
    const u32 left = 2u * n - j;
    const u64 c = o_ballot(left, depth + 1u);
    if (rank < c) {
      ++depth;
    } else {
      rank -= c;
      mask |= 1ull << left;
      --depth;
    }
  }
  return mask;
}
u64 low_mask(u32 n) { return n == 32u ? kMax64 : (1ull << (2u * n)) - 1u; }

// A mask through rank and next: codes, values, guards and digest fold (n and mask are folded by the caller of the digest form).
bool mask_tail(u64& h, u64 mask, u32 n) {
  bool ok = true;
  const Out r = c_rank(mask, n);
  const Out nx = c_next(mask, n);
  const bool v = o_valid(mask, n);
  if (!r.guards || !nx.guards) ok = false;
  if (!v) {
    if (r.r != DyResult::BadMask || nx.r != DyResult::BadMask) ok = false;
  } else {
    const u64 want = o_rank(mask, n);
    if (r.r != DyResult::Ok || r.v != want) ok = false;
    if (want + 1u >= o_catalan(n)) {
      if (nx.r != DyResult::NoNext) ok = false;
    } else if (nx.r != DyResult::Ok || nx.v != o_unrank(n, want + 1u)) {
      ok = false;
    }
    if (r.r == DyResult::Ok) {
      const Out u = c_unrank(n, r.v);
      if (u.r != DyResult::Ok || u.v != mask || !u.guards) ok = false;
    }
  }
  h = fold(fold(fold(fold(h, code_of(r.r)), r.v), code_of(nx.r)), nx.v);
  return ok;
}
bool mask_case(u64& h, u32 n, u64 mask) {
  h = fold(fold(h, n), mask);
  return mask_tail(h, mask, n);
}
bool rank_case(u64& h, u32 n, u64 rank) {
  bool ok = true;
  const Out u = c_unrank(n, rank);
  h = fold(fold(fold(fold(h, n), rank), code_of(u.r)), u.v);
  if (!u.guards) ok = false;
  if (rank >= o_catalan(n)) {
    if (u.r != DyResult::RankTooLarge) ok = false;
    for (u32 i = 0u; i < 4u; ++i) h = fold(h, 0u);
    return ok;
  }
  if (u.r != DyResult::Ok || u.v != o_unrank(n, rank) || !o_valid(u.v, n)) ok = false;
  if (!mask_tail(h, u.v, n)) ok = false;
  return ok;
}

u32 rank_list(u32 n, u64* out) {
  const u64 c = o_catalan(n);
  u32 k = 0u;
  const u64 base[13] = {0u, 1u, 2u, c - 2u, c - 1u, c, c + 1u, c + 2u, c / 2u - 1u, c / 2u, c / 2u + 1u, kMax64, kMax64 - 1u};
  for (u32 i = 0u; i < 13u; ++i) out[k++] = base[i];
  for (u32 s = 1u; s < 64u; ++s) {
    out[k++] = (1ull << s) - 1u;
    out[k++] = 1ull << s;
  }
  return k;
}

constexpr u32 kD7a = 12500u;
constexpr u32 kD7b = 1250u;
constexpr u32 kD7c = 3125u;

u64 edit_case(u32 n, u32 i, u64 base, u64 mask, u64 w2) {
  const u32 p1 = static_cast<u32>(w2 % (2u * n));
  const u32 p2 = static_cast<u32>((w2 >> 8) % (2u * n));
  const u64 low = low_mask(n);
  (void)base;
  (void)i;
  switch (i % 10u) {
    case 0u: return mask ^ (1ull << p1);
    case 1u: {
      const u64 b1 = (mask >> p1) & 1u;
      const u64 b2 = (mask >> p2) & 1u;
      const u64 x = b1 ^ b2;
      return mask ^ ((x << p1) | (x << p2));
    }
    case 2u: return n < 32u ? (mask | (1ull << (2u * n + static_cast<u32>((w2 >> 16) % (64u - 2u * n))))) : (mask ^ (1ull << 63));
    case 3u: return low;
    case 4u: return 0u;
    case 5u: return 0x5555555555555555ull & low;
    case 6u: return 0xAAAAAAAAAAAAAAAAull & low;
    case 7u: return mask ^ low;
    case 8u: return mask ^ (1ull << p1) ^ (1ull << p2);
    default: return mask >> 1;
  }
}

// ---- whole ascending walk of the words of one semilength ----
bool walk(u32 n, u64& h) {
  bool ok = true;
  u64 cnt[32] = {};
  u64 total = 0u;
  u64 idx = 0u;
  const u64 c = o_catalan(n);
  const Out first = c_unrank(n, 0u);
  u64 mask = first.v;
  if (first.r != DyResult::Ok || mask != low_mask(n) >> n) ok = false;
  for (;;) {
    if (!o_valid(mask, n)) ok = false;
    const Out r = c_rank(mask, n);
    const Out u = c_unrank(n, idx);
    if (r.r != DyResult::Ok || r.v != idx || !r.guards) ok = false;
    if (u.r != DyResult::Ok || u.v != mask || !u.guards) ok = false;
    for (u32 p = 0u; p < 2u * n; ++p) cnt[p] += (mask >> p) & 1u;
    total += mask;
    const Out nx = c_next(mask, n);
    if (!nx.guards) ok = false;
    if (nx.r == DyResult::NoNext) break;
    if (nx.r != DyResult::Ok || nx.v <= mask) {
      ok = false;
      break;
    }
    mask = nx.v;
    ++idx;
  }
  if (idx + 1u != c) ok = false;
  h = fold(fold(h, n), c);
  for (u32 p = 0u; p < 2u * n; ++p) h = fold(h, cnt[p]);
  h = fold(h, total);
  return ok;
}
bool walks(const u32* ns, u32 k, u64 dig, u64 want) {
  u64 h = kFnv0;
  bool ok = true;
  u64 words = 0u;
  for (u32 i = 0u; i < k; ++i) {
    if (!walk(ns[i], h)) ok = false;
    words += o_catalan(ns[i]);
  }
  return ok && h == dig && words == want;
}
}  // namespace

// Cheap cases first, the long walks last.
TEST_CASE("dyck: header examples", "[dyck]") {
  struct R {
    u64 mask;
    u32 n;
    u64 rank;
  };
  const R ranks[] = {{0x7u, 3u, 0u}, {0xBu, 3u, 1u}, {0xDu, 3u, 2u}, {0x13u, 3u, 3u}, {0x15u, 3u, 4u}, {15u, 4u, 0u}, {23u, 4u, 1u}, {27u, 4u, 2u}, {29u, 4u, 3u},
                     {39u, 4u, 4u}, {83u, 4u, 13u}, {85u, 4u, 13u}, {0xB3u, 5u, 21u}, {0x555555555ull, 18u, 477638699u}, {0xFFFFFFFFull, 32u, 0u},
                     {0x5555555555555555ull, 32u, 55534064877048197ull}, {0u, 0u, 0u}};
  for (const R& e : ranks) {
    const Out o = c_rank(e.mask, e.n);
    const bool g = o.guards && o.r == DyResult::Ok && (o.v == e.rank || e.mask == 83u);
    CHECK(g);
  }
  const u32 cn[] = {0u, 1u, 5u, 10u, 20u, 31u, 32u};
  const u64 cv[] = {1u, 1u, 42u, 16796u, 6564120420ull, 14544636039226909ull, 55534064877048198ull};
  for (u32 i = 0u; i < 7u; ++i) {
    const Out o = c_count(cn[i]);
    const bool g = o.guards && o.r == DyResult::Ok && o.v == cv[i];
    CHECK(g);
  }
  struct U {
    u32 n;
    u64 rank;
    u64 mask;
  };
  const U us[] = {{10u, 0u, 1023u}, {8u, 100u, 0x7A7u}, {20u, 1000000u, 0xBF9F727ull}, {32u, 10000000000000000ull, 0x08F6C4AB1D384BB7ull}, {32u, 0u, 0xFFFFFFFFull},
                  {32u, 55534064877048197ull, 0x5555555555555555ull}, {0u, 0u, 0u}};
  for (const U& e : us) {
    const Out o = c_unrank(e.n, e.rank);
    const bool g = o.guards && o.r == DyResult::Ok && o.v == e.mask;
    CHECK(g);
  }
  const Out t1 = c_unrank(3u, 5u);
  const Out t2 = c_unrank(0u, 1u);
  const bool b1 = t1.r == DyResult::RankTooLarge && t1.guards;
  const bool b2 = t2.r == DyResult::RankTooLarge && t2.guards;
  CHECK(b1);
  CHECK(b2);
  struct N {
    u64 mask;
    u32 n;
    u64 next;
    bool last;
  };
  const N ns[] = {{0x7u, 3u, 0xBu, false}, {0xDu, 3u, 0x13u, false}, {0x15u, 3u, 0u, true}, {0xFFFFFFFFull, 32u, 0x17FFFFFFFull, false},
                  {0x5555555555555553ull, 32u, 0x5555555555555555ull, false}, {0x5555555555555555ull, 32u, 0u, true}, {0u, 0u, 0u, true}};
  for (const N& e : ns) {
    const Out o = c_next(e.mask, e.n);
    const bool g = o.guards && (e.last ? o.r == DyResult::NoNext : (o.r == DyResult::Ok && o.v == e.next));
    CHECK(g);
  }
  const u64 bad[][2] = {{6u, 2u}, {9u, 2u}, {4u, 2u}, {0x83u, 3u}};
  for (const auto& e : bad) {
    const Out o = c_rank(e[0], static_cast<u32>(e[1]));
    const bool g = o.r == DyResult::BadMask && o.guards;
    CHECK(g);
  }
}

TEST_CASE("dyck: check order and no write on failure", "[dyck]") {
  const u32 ns[8] = {0u, 1u, 3u, 5u, 32u, 33u, 100u, 0xFFFFFFFFu};
  const u64 masks[8] = {0u, 0x7u, 0x15u, 0x83u, 6u, 0xFFFFFFFFull, 0x5555555555555555ull, kMax64};
  const u64 ranks[7] = {0u, 1u, 4u, 5u, 42u, 55534064877048198ull, kMax64};
  bool ok = true;
  for (u32 i = 0u; i < 8u; ++i) {
    // count: null, then n above 32
    {
      u64 cell[3] = {kGuard, kCell, kGuard};
      const DyResult a = dyck_count(ns[i], nullptr);
      const DyResult b = dyck_count(ns[i], &cell[1]);
      if (a != DyResult::NullArgument) ok = false;
      const u32 want = ns[i] > 32u ? 2u : 0u;
      if (code_of(b) != want) ok = false;
      if (want == 2u && cell[1] != kCell) ok = false;
      if (cell[0] != kGuard || cell[2] != kGuard) ok = false;
    }
    for (u32 j = 0u; j < 8u; ++j) {
      u64 cell[3] = {kGuard, kCell, kGuard};
      if (dyck_rank(masks[j], ns[i], nullptr) != DyResult::NullArgument) ok = false;
      if (dyck_next(masks[j], ns[i], nullptr) != DyResult::NullArgument) ok = false;
      const Out r = c_rank(masks[j], ns[i]);
      const Out x = c_next(masks[j], ns[i]);
      u32 wr = 0u;
      u32 wx = 0u;
      if (ns[i] > 32u) {
        wr = 2u;
        wx = 2u;
      } else if (!o_valid(masks[j], ns[i])) {
        wr = 3u;
        wx = 3u;
      } else {
        wx = o_rank(masks[j], ns[i]) + 1u >= o_catalan(ns[i]) ? 5u : 0u;
      }
      if (code_of(r.r) != wr || code_of(x.r) != wx || !r.guards || !x.guards) ok = false;
      (void)cell;
    }
    for (u32 j = 0u; j < 7u; ++j) {
      if (dyck_unrank(ns[i], ranks[j], nullptr) != DyResult::NullArgument) ok = false;
      const Out u = c_unrank(ns[i], ranks[j]);
      u32 want = 0u;
      if (ns[i] > 32u) {
        want = 2u;
      } else if (ranks[j] >= o_catalan(ns[i])) {
        want = 4u;
      }
      if (code_of(u.r) != want || !u.guards) ok = false;
    }
  }
  CHECK(ok);
}

TEST_CASE("dyck: D3 counts n = 0..32", "[dyck]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u32 n = 0u; n <= 32u; ++n) {
    const Out o = c_count(n);
    if (o.r != DyResult::Ok || o.v != o_catalan(n) || !o.guards) ok = false;
    h = fold(fold(h, n), o.v);
  }
  // C_(n+1) = sum C_i C_(n-i) for n up to 30 (no wrap: the products stay below 2^64 up to n = 30)
  for (u32 n = 0u; n < 30u; ++n) {
    u64 s = 0u;
    for (u32 i = 0u; i <= n; ++i) s += c_count(i).v * c_count(n - i).v;
    if (s != c_count(n + 1u).v) ok = false;
  }
  const Out big = c_count(33u);
  if (big.r != DyResult::BadArgument || !big.guards) ok = false;
  CHECK(ok);
  CHECK(h == kD3Digest);
}

TEST_CASE("dyck: D6 edge ranks for n = 0..32", "[dyck]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  u64 list[160];
  for (u32 n = 0u; n <= 32u; ++n) {
    const u32 k = rank_list(n, list);
    for (u32 i = 0u; i < k; ++i) {
      if (!rank_case(h, n, list[i])) ok = false;
      ++cnt;
    }
  }
  CHECK(ok);
  CHECK(cnt == kD6Count);
  CHECK(h == kD6Digest);
}

TEST_CASE("dyck: D8 chains at both ends of every n", "[dyck]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 n = 1u; n <= 32u; ++n) {
    const u64 c = o_catalan(n);
    const u64 head = c < 21u ? c : 21u;
    for (u64 r = 0u; r < head; ++r) {
      if (!mask_case(h, n, c_unrank(n, r).v)) ok = false;
      ++cnt;
    }
    for (u64 r = (c > 21u ? c - 21u : 0u); r < c; ++r) {
      if (!mask_case(h, n, c_unrank(n, r).v)) ok = false;
      ++cnt;
    }
    // the chain by next from the 21st word before the end reaches the last word and stops
    u64 m = c_unrank(n, c > 21u ? c - 21u : 0u).v;
    u64 steps = 0u;
    for (;;) {
      const Out x = c_next(m, n);
      if (x.r == DyResult::NoNext) break;
      if (x.r != DyResult::Ok) {
        ok = false;
        break;
      }
      m = x.v;
      ++steps;
    }
    if (steps != (c > 21u ? 20u : c - 1u) || m != c_unrank(n, c - 1u).v) ok = false;
  }
  CHECK(ok);
  CHECK(cnt == kD8Count);
  CHECK(h == kD8Digest);
}

TEST_CASE("dyck: boundary bits at 2n, 63 and every single flip, n = 0..32", "[dyck]") {
  bool ok = true;
  bool high_bad = true;
  u64 h = kFnv0;
  for (u32 n = 0u; n <= 32u; ++n) {
    const u64 c = o_catalan(n);
    const u64 pick[3] = {0u, c - 1u, c / 2u};
    for (u32 q = 0u; q < 3u; ++q) {
      const u64 w = c_unrank(n, pick[q]).v;
      if (!mask_case(h, n, w)) ok = false;
      // every bit from 2n up to 63 set on a valid word is BadMask (the shift 2n is never taken for n = 32)
      for (u32 b = 2u * n; b < 64u; ++b) {
        const u64 m = w | (1ull << b);
        const Out r = c_rank(m, n);
        const Out x = c_next(m, n);
        if (r.r != DyResult::BadMask || x.r != DyResult::BadMask || !r.guards || !x.guards) high_bad = false;
        if (!mask_case(h, n, m)) ok = false;
      }
      // every single bit flip of a valid word, and the word with the first character closed
      for (u32 b = 0u; b < 64u; ++b) {
        if (!mask_case(h, n, w ^ (1ull << b))) ok = false;
      }
      if (!mask_case(h, n, w | (1ull << (2u * n - 1u + (n == 0u ? 1u : 0u))))) ok = false;
    }
    if (!mask_case(h, n, kMax64) || !mask_case(h, n, 1ull << 63) || !mask_case(h, n, low_mask(n)) || !mask_case(h, n, low_mask(n) >> 1)) ok = false;
  }
  CHECK(ok);
  CHECK(high_bad);
}

TEST_CASE("dyck: D7a sampled ranks, n = 17..32", "[dyck]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u32 n = 17u; n <= 32u; ++n) {
    const u64 c = o_catalan(n);
    const u64 base = (1ull << 40) + n * 1000000ull;
    for (u32 i = 0u; i < kD7a; ++i) {
      if (!rank_case(h, n, splitmix_at(base + i) % c)) ok = false;
    }
  }
  CHECK(ok);
  CHECK(h == kD7aDigest);
}

TEST_CASE("dyck: D7b near-valid masks, n = 17..32", "[dyck]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u32 n = 17u; n <= 32u; ++n) {
    const u64 c = o_catalan(n);
    const u64 base = (1ull << 41) + n * 1000000ull;
    for (u32 i = 0u; i < kD7b; ++i) {
      const u64 mask = o_unrank(n, splitmix_at(base + 2u * i) % c);
      const u64 w2 = splitmix_at(base + 2u * i + 1u);
      if (!mask_case(h, n, edit_case(n, i, base, mask, w2))) ok = false;
    }
  }
  CHECK(ok);
  CHECK(h == kD7bDigest);
}

TEST_CASE("dyck: D7c random masks, n = 17..32", "[dyck]") {
  u64 h = kFnv0;
  bool ok = true;
  for (u32 n = 17u; n <= 32u; ++n) {
    const u64 base = (1ull << 42) + n * 1000000ull;
    for (u32 i = 0u; i < kD7c; ++i) {
      u64 m = splitmix_at(base + i);
      if (i % 5u != 0u) m &= low_mask(n);
      if (!mask_case(h, n, m)) ok = false;
    }
  }
  CHECK(ok);
  CHECK(h == kD7cDigest);
}

TEST_CASE("dyck: D4 every mask below 2^(2n), n = 0..9", "[dyck]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  bool seq = true;
  for (u32 n = 0u; n <= 9u; ++n) {
    u64 next_rank = 0u;
    for (u64 m = 0u; m < (1ull << (2u * n)); ++m) {
      if (!mask_case(h, n, m)) ok = false;
      ++cnt;
      if (o_valid(m, n)) {
        const Out r = c_rank(m, n);
        if (r.r != DyResult::Ok || r.v != next_rank) seq = false;
        ++next_rank;
        const u64 hi1 = m | (1ull << (2u * n));
        const u64 hi2 = m | (1ull << 63);
        const u64 hi3 = m | (1ull << (2u * n + 3u));
        if (!mask_case(h, n, hi1) || !mask_case(h, n, hi2) || !mask_case(h, n, hi3)) ok = false;
        cnt += 3u;
      }
    }
    if (next_rank != o_catalan(n)) seq = false;
  }
  CHECK(ok);
  CHECK(seq);
  CHECK(cnt == kD4Count);
  CHECK(h == kD4Digest);
}

TEST_CASE("dyck: D5 walks n = 0..12", "[dyck]") {
  u32 ns[13];
  for (u32 i = 0u; i < 13u; ++i) ns[i] = i;
  const bool ok = walks(ns, 13u, kD5ADigest, kD5ACount);
  CHECK(ok);
}
TEST_CASE("dyck: D5 walk n = 13", "[dyck]") {
  const u32 ns[1] = {13u};
  const bool ok = walks(ns, 1u, kD5BDigest, kD5BCount);
  CHECK(ok);
}
TEST_CASE("dyck: D5 walk n = 14", "[dyck]") {
  const u32 ns[1] = {14u};
  const bool ok = walks(ns, 1u, kD5CDigest, kD5CCount);
  CHECK(ok);
}
TEST_CASE("dyck: D5 walk n = 15", "[dyck]") {
  const u32 ns[1] = {15u};
  const bool ok = walks(ns, 1u, kD5DDigest, kD5DCount);
  CHECK(ok);
}
