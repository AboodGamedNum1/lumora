// Tests for core/graycode. Expected values come from the header's examples, from test-only oracles of a different method than the library (the code table built
// level by level from the header's definition with arrays, for every (k, n) with k^n up to 65536; a bit-loop Gray decode; k^n by squaring with an overflow test;
// validity by taking nibbles with division; the one-digit-by-exactly-1 test digit by digit) and from FNV-1a 64-bit digests made by tools/graycode_ref.py (Python
// standard library only: the explicit lists built from the header's definition for every walk, and the same definition as a recursion on the index for the large
// sizes; see docs/graycode.md). Extents (see docs/graycode.md): the header examples; the check-order matrix; binary codes on a structured set, 200000 seeded
// values and every value below 2^22; the counts for every (k, n); the boundary bits of the code; the whole code walk of every (k, n) with k^n up to 2^22, each
// step checked for one digit changing by exactly 1; edge, block-boundary and seeded ranks for every (k, n); every code below 2^16 for n up to 4; sampled invalid codes.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "graycode.h"

namespace {
using lumora::graycode::GrResult;
using lumora::graycode::gray_decode;
using lumora::graycode::gray_encode;
using lumora::graycode::kgray_count;
using lumora::graycode::kgray_next;
using lumora::graycode::kgray_rank;
using lumora::graycode::kgray_unrank;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr u64 kGuard = 0xDEADBEEFDEADBEEFull;
constexpr u64 kCell = 0x1234567812345678ull;
constexpr u64 kMax64 = ~0ull;

constexpr u64 kG1Digest = 0x937973A67CBCB740ull;
constexpr u64 kG1Count = 204292ull;
constexpr u64 kG2Digest = 0x7AA95F4CC572DD41ull;
constexpr u64 kG2Count = 240ull;
constexpr u64 kG4aDigest = 0x7AA84EF1CECAC5ACull;
constexpr u64 kG4aCount = 8114957ull;
constexpr u64 kG4bDigest = 0x622D04CFC2FC4831ull;
constexpr u64 kG4bCount = 8412356ull;
constexpr u64 kG4cDigest = 0x6351570061D9C3FBull;
constexpr u64 kG4cCount = 9230784ull;
constexpr u64 kG5aDigest = 0x8B00CF6474DE13A5ull;
constexpr u64 kG5aCount = 161520ull;
constexpr u64 kG5bDigest = 0xC0F816EAD018317Full;
constexpr u64 kG5bCount = 130080ull;
constexpr u64 kG5cDigest = 0x9D9AEEE5EF328A50ull;
constexpr u64 kG5cCount = 196558ull;
constexpr u64 kG6bDigest = 0x701031E541F79FD3ull;
constexpr u64 kG6bCount = 189480ull;
constexpr u64 kG6aDigest = 0x9D8E8AD496FF66D5ull;
constexpr u64 kG6aCount = 3932160ull;

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
u64 resf(u64 h, GrResult r, u64 v) {
  h = fold(h, static_cast<u64>(r));
  if (r == GrResult::Ok) h = fold(h, v);
  return h;
}

// ---- library calls between guard words; a failure must leave the cell alone ----
struct Out {
  GrResult r;
  u64 v;
  bool guards;
};
Out finish(GrResult r, const u64* cell) {
  Out x{r, cell[1], cell[0] == kGuard && cell[2] == kGuard};
  if (r != GrResult::Ok) {
    x.guards = x.guards && cell[1] == kCell;
    x.v = 0u;
  }
  return x;
}
Out c_count(u32 k, u32 n) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const GrResult r = kgray_count(k, n, &cell[1]);
  return finish(r, cell);
}
Out c_unrank(u32 k, u32 n, u64 rank) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const GrResult r = kgray_unrank(k, n, rank, &cell[1]);
  return finish(r, cell);
}
Out c_rank(u32 k, u32 n, u64 code) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const GrResult r = kgray_rank(k, n, code, &cell[1]);
  return finish(r, cell);
}
Out c_next(u32 k, u32 n, u64 code) {
  u64 cell[3] = {kGuard, kCell, kGuard};
  const GrResult r = kgray_next(k, n, code, &cell[1]);
  return finish(r, cell);
}

// ---- oracles ----
// k^n by squaring; big is set when it is 2^64 or more (the value is then meaningless)
u64 o_count(u32 k, u32 n, bool* big) {
  u64 result = 1u;
  u64 base = k;
  u32 e = n;
  bool ov = false;
  while (e > 0u) {
    if ((e & 1u) != 0u) {
      if (base != 0u && result > kMax64 / base) ov = true;
      result *= base;
    }
    e >>= 1u;
    if (e > 0u) {
      if (base != 0u && base > kMax64 / base) ov = true;
      base *= base;
    }
  }
  if (big != nullptr) *big = ov;
  return result;
}
// validity by taking nibbles with division
bool o_valid(u32 k, u32 n, u64 code) {
  u64 rest = code;
  for (u32 i = 0u; i < n; ++i) {
    if (rest % 16u >= k) return false;
    rest /= 16u;
  }
  return rest == 0u;
}
bool one_digit_by_one(u64 a, u64 b, u32 n) {
  u32 diff = 0u;
  for (u32 i = 0u; i < n; ++i) {
    const u32 x = static_cast<u32>((a >> (4u * i)) & 15u);
    const u32 y = static_cast<u32>((b >> (4u * i)) & 15u);
    if (x != y) {
      if (x + 1u != y && y + 1u != x) return false;
      ++diff;
    }
  }
  return diff == 1u;
}
u64 o_decode(u64 g) {
  u64 x = 0u;
  u64 p = 0u;
  for (u32 b = 64u; b > 0u; --b) {
    p ^= (g >> (b - 1u)) & 1u;
    x |= p << (b - 1u);
  }
  return x;
}
u32 popcount64(u64 x) {
  u32 c = 0u;
  while (x != 0u) {
    c += static_cast<u32>(x & 1u);
    x >>= 1u;
  }
  return c;
}
// the code table of every (k, n) with k^n up to 65536 from the header's definition: the top digit d runs 0..k-1 and the lower digits follow the (n-1)-digit table
// forward for even d, backward for odd d
u64 g_tab_a[65536];
u64 g_tab_b[65536];
const u64* o_table(u32 k, u32 n) {
  u64 len = 1u;
  g_tab_a[0] = 0u;
  for (u32 lvl = 0u; lvl < n; ++lvl) {
    u64 w = 0u;
    for (u32 d = 0u; d < k; ++d) {
      for (u64 i = 0u; i < len; ++i) {
        const u64 src = (d % 2u == 0u) ? g_tab_a[i] : g_tab_a[len - 1u - i];
        g_tab_b[w++] = (static_cast<u64>(d) << (4u * lvl)) | src;
      }
    }
    len *= k;
    for (u64 i = 0u; i < len; ++i) g_tab_a[i] = g_tab_b[i];
  }
  return g_tab_a;
}

// ---- digests (mirrors tools/graycode_ref.py) ----
constexpr u64 kWalkLimit = 1ull << 22;

u64 g1_digest(u64& cnt) {
  u64 h = kFnv0;
  cnt = 0u;
  auto one = [&](u64 x) {
    h = fold(fold(fold(h, x), gray_encode(x)), gray_decode(x));
    ++cnt;
  };
  one(0u);
  one(0x5555555555555555ull);
  one(0xAAAAAAAAAAAAAAAAull);
  one(kMax64);
  for (u32 i = 0u; i < 64u; ++i) {
    one(1ull << i);
    one((1ull << i) - 1u);
    one((1ull << i) + 1u);
  }
  for (u32 i = 0u; i < 64u; ++i) {
    for (u32 j = i; j < 64u; ++j) {
      const u64 hi = j == 63u ? kMax64 : (1ull << (j + 1u)) - 1u;
      one(hi ^ ((1ull << i) - 1u));
    }
  }
  for (u32 i = 0u; i < 64u; ++i) {
    for (u32 j = i + 1u; j < 64u; ++j) one((1ull << i) | (1ull << j));
  }
  for (u32 i = 0u; i < 200000u; ++i) one(splitmix_at((1ull << 40) + i));
  return h;
}

// every valid walk property for one (k, n) with k^n up to 2^20
bool walk(u32 k, u32 n, u64& h, u64& cnt) {
  bool big = false;
  const u64 c = o_count(k, n, &big);
  bool ok = !big;
  h = fold(fold(fold(h, k), n), c);
  const bool tab = c <= 65536u;
  const u64* t = tab ? o_table(k, n) : nullptr;
  u64 prev = 0u;
  u64 prev_next = 0u;
  for (u64 r = 0u; r < c; ++r) {
    const Out u = c_unrank(k, n, r);
    const u64 code = u.v;
    h = fold(h, code);
    if (u.r != GrResult::Ok || !u.guards || !o_valid(k, n, code)) ok = false;
    if (tab && code != t[r]) ok = false;
    const Out rk = c_rank(k, n, code);
    if (rk.r != GrResult::Ok || rk.v != r || !rk.guards) ok = false;
    const Out nx = c_next(k, n, code);
    if (r + 1u < c) {
      if (nx.r != GrResult::Ok || !nx.guards) ok = false;
    } else if (nx.r != GrResult::NoNext || !nx.guards) {
      ok = false;
    }
    if (r > 0u) {
      if (!one_digit_by_one(prev, code, n) || prev_next != code) ok = false;
    }
    prev = code;
    prev_next = nx.v;
  }
  cnt += c;
  return ok;
}
bool walks(const u32* ks, u32 nk, u64 digest, u64 count) {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 q = 0u; q < nk; ++q) {
    for (u32 n = 1u; n <= 16u; ++n) {
      bool big = false;
      const u64 c = o_count(ks[q], n, &big);
      if (big || c > kWalkLimit) break;
      if (!walk(ks[q], n, h, cnt)) ok = false;
    }
  }
  return ok && h == digest && cnt == count;
}

// edge, block-boundary and seeded ranks
bool ranks_case(u32 k, u32 n, u64& h, u64& cnt) {
  bool ok = true;
  bool big = false;
  const u64 c = o_count(k, n, &big);
  bool big2 = false;
  const u64 pw = o_count(k, n - 1u, &big2);
  u64 list[2048 + 64];
  u32 m = 0u;
  list[m++] = 0u;
  list[m++] = 1u;
  list[m++] = 2u;
  list[m++] = k - 1u;
  list[m++] = k;
  list[m++] = c - 2u;
  list[m++] = c - 1u;
  if (!big) {
    list[m++] = c;
    list[m++] = c + 1u;
  }
  list[m++] = kMax64;
  for (u32 j = 1u; j < k; ++j) {
    list[m++] = j * pw - 1u;
    list[m++] = j * pw;
    list[m++] = j * pw + 1u;
  }
  const u64 base = (1ull << 44) + (static_cast<u64>(k) * 16u + n) * 1000000u;
  h = fold(fold(h, k), n);
  for (u32 q = 0u; q < m + 2000u; ++q) {
    u64 r = 0u;
    if (q < m) {
      r = list[q];
    } else {
      const u64 s = splitmix_at(base + (q - m));
      r = big ? s : s % c;
    }
    const Out u = c_unrank(k, n, r);
    h = resf(fold(h, r), u.r, u.v);
    const bool in_range = big || r < c;
    if (!u.guards || (u.r == GrResult::Ok) != in_range) ok = false;
    if (u.r == GrResult::Ok) {
      if (!o_valid(k, n, u.v)) ok = false;
      const Out rk = c_rank(k, n, u.v);
      h = resf(h, rk.r, rk.v);
      if (rk.r != GrResult::Ok || rk.v != r || !rk.guards) ok = false;
      const Out nx = c_next(k, n, u.v);
      h = resf(h, nx.r, nx.v);
      const bool last = big ? r == kMax64 : r + 1u == c;
      if (!nx.guards) ok = false;
      if (last) {
        if (nx.r != GrResult::NoNext) ok = false;
      } else {
        const Out u2 = c_unrank(k, n, r + 1u);
        if (nx.r != GrResult::Ok || u2.r != GrResult::Ok || nx.v != u2.v || !one_digit_by_one(u.v, nx.v, n)) ok = false;
      }
    } else if (u.r != GrResult::RankTooLarge) {
      ok = false;
    }
    ++cnt;
  }
  return ok;
}
bool ranks_group(const u32* ks, u32 nk, u64 digest, u64 count) {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 q = 0u; q < nk; ++q) {
    for (u32 n = 1u; n <= 16u; ++n) {
      if (!ranks_case(ks[q], n, h, cnt)) ok = false;
    }
  }
  return ok && h == digest && cnt == count;
}

// sampled and invalid codes
bool codes_case(u32 k, u32 n, u64& h, u64& cnt) {
  bool ok = true;
  bool big = false;
  const u64 c = o_count(k, n, &big);
  const u64 base = (1ull << 45) + (static_cast<u64>(k) * 16u + n) * 1000000u;
  const u64 s0 = splitmix_at(base);
  const u64 w = c_unrank(k, n, big ? s0 : s0 % c).v;
  h = fold(fold(h, k), n);
  auto one = [&](u64 code) {
    const Out rk = c_rank(k, n, code);
    const Out nx = c_next(k, n, code);
    h = resf(resf(fold(h, code), rk.r, rk.v), nx.r, nx.v);
    const bool v = o_valid(k, n, code);
    if (!rk.guards || !nx.guards) ok = false;
    if (!v) {
      if (rk.r != GrResult::BadCode || nx.r != GrResult::BadCode) ok = false;
    } else {
      if (rk.r != GrResult::Ok) ok = false;
      const Out back = c_unrank(k, n, rk.v);
      if (back.r != GrResult::Ok || back.v != code) ok = false;
      if (nx.r == GrResult::Ok) {
        const Out rn = c_rank(k, n, nx.v);
        if (rn.r != GrResult::Ok || rn.v != rk.v + 1u) ok = false;
      } else if (nx.r != GrResult::NoNext) {
        ok = false;
      }
    }
    ++cnt;
  };
  for (u32 p = 0u; p < n; ++p) {
    for (u32 v = k; v < 16u; ++v) one((w & ~(15ull << (4u * p))) | (static_cast<u64>(v) << (4u * p)));
  }
  for (u32 b = 4u * n; b < 64u; ++b) one(w | (1ull << b));
  for (u32 i = 1u; i <= 500u; ++i) one(splitmix_at(base + i));
  for (u32 i = 501u; i <= 700u; ++i) {
    u64 x = 0u;
    for (u32 p = 0u; p < n; ++p) x |= (splitmix_at(base + static_cast<u64>(i) * 16u + p) % 16u) << (4u * p);
    one(x);
  }
  return ok;
}

u32 g_all[15] = {2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
}  // namespace

TEST_CASE("graycode: header examples", "[graycode]") {
  bool ok = true;
  ok = ok && gray_encode(0u) == 0u && gray_encode(1u) == 1u && gray_encode(2u) == 3u && gray_encode(3u) == 2u;
  ok = ok && gray_encode(kMax64) == 0x8000000000000000ull && gray_encode(0x123456789ABCDEF0ull) == 0x1B2E7D44D7E2B188ull;
  ok = ok && gray_decode(0x123456789ABCDEF0ull) == 0x1C279BAF132894A0ull && gray_decode(kMax64) == 0xAAAAAAAAAAAAAAAAull;
  ok = ok && gray_decode(0x8000000000000000ull) == kMax64 && gray_decode(3u) == 2u;
  const u32 ck[6][2] = {{2u, 1u}, {3u, 2u}, {10u, 16u}, {15u, 16u}, {16u, 15u}, {16u, 16u}};
  const u64 cv[6] = {2u, 9u, 10000000000000000ull, 6568408355712890625ull, 1ull << 60, 0u};
  for (u32 i = 0u; i < 6u; ++i) {
    const Out o = c_count(ck[i][0], ck[i][1]);
    const GrResult want = i == 5u ? GrResult::Overflow : GrResult::Ok;
    if (o.r != want || o.v != cv[i] || !o.guards) ok = false;
  }
  const u64 tern[9] = {0x00u, 0x01u, 0x02u, 0x12u, 0x11u, 0x10u, 0x20u, 0x21u, 0x22u};
  for (u32 r = 0u; r < 9u; ++r) {
    const Out o = c_unrank(3u, 2u, r);
    if (o.r != GrResult::Ok || o.v != tern[r] || !o.guards) ok = false;
    const Out q = c_rank(3u, 2u, tern[r]);
    if (q.r != GrResult::Ok || q.v != r) ok = false;
  }
  struct U {
    u32 k, n;
    u64 rank, code;
  };
  const U us[] = {{4u, 2u, 15u, 0x30u},
                  {2u, 16u, 40000u, 0x1101001001100000ull},
                  {10u, 3u, 999u, 0x900u},
                  {10u, 16u, 9999999999999999ull, 0x9000000000000000ull},
                  {10u, 16u, 12345678901234ull, 0x17355371991735ull},
                  {3u, 16u, 43046720ull, 0x2222222222222222ull},
                  {16u, 16u, kMax64, 0xF000000000000000ull},
                  {16u, 16u, 0x123456789ABCDEF0ull, 0x1D3B597795B3D1FFull}};
  for (const U& e : us) {
    const Out o = c_unrank(e.k, e.n, e.rank);
    if (o.r != GrResult::Ok || o.v != e.code || !o.guards) ok = false;
    const Out q = c_rank(e.k, e.n, e.code);
    if (q.r != GrResult::Ok || q.v != e.rank) ok = false;
  }
  if (c_unrank(3u, 2u, 9u).r != GrResult::RankTooLarge || c_unrank(10u, 16u, 10000000000000000ull).r != GrResult::RankTooLarge) ok = false;
  if (c_unrank(16u, 16u, kMax64).r != GrResult::Ok) ok = false;
  struct B {
    u32 k, n;
    u64 code;
  };
  const B bad[] = {{3u, 2u, 0x03u}, {3u, 2u, 0x100u}, {2u, 3u, 0x2u}, {10u, 1u, 0xAu}, {4u, 2u, 0x40u}};
  for (const B& b : bad) {
    if (c_rank(b.k, b.n, b.code).r != GrResult::BadCode || c_next(b.k, b.n, b.code).r != GrResult::BadCode) ok = false;
  }
  const Out n1 = c_next(3u, 2u, 0x02u);
  const Out n2 = c_next(4u, 2u, 0x03u);
  const Out n3 = c_next(3u, 2u, 0x20u);
  if (n1.r != GrResult::Ok || n1.v != 0x12u || n2.r != GrResult::Ok || n2.v != 0x13u || n3.r != GrResult::Ok || n3.v != 0x21u) ok = false;
  if (c_next(3u, 2u, 0x22u).r != GrResult::NoNext || c_next(2u, 3u, 0x100u).r != GrResult::NoNext) ok = false;
  if (c_next(16u, 16u, 0xF000000000000000ull).r != GrResult::NoNext || c_next(10u, 16u, 0x9000000000000000ull).r != GrResult::NoNext) ok = false;
  CHECK(ok);
}

TEST_CASE("graycode: check order and no write on failure", "[graycode]") {
  bool ok = true;
  const u32 ks[8] = {0u, 1u, 2u, 3u, 16u, 17u, 100u, 0xFFFFFFFFu};
  const u32 ns[7] = {0u, 1u, 2u, 16u, 17u, 100u, 0xFFFFFFFFu};
  const u64 vs[5] = {0u, 3u, 0x100u, 0x23u, kMax64};
  for (u32 a = 0u; a < 8u; ++a) {
    for (u32 b = 0u; b < 7u; ++b) {
      for (u32 q = 0u; q < 5u; ++q) {
        const u32 k = ks[a];
        const u32 n = ns[b];
        const u64 v = vs[q];
        const bool badarg = k < 2u || k > 16u || n < 1u || n > 16u;
        if (kgray_count(k, n, nullptr) != GrResult::NullArgument || kgray_unrank(k, n, v, nullptr) != GrResult::NullArgument) ok = false;
        if (kgray_rank(k, n, v, nullptr) != GrResult::NullArgument || kgray_next(k, n, v, nullptr) != GrResult::NullArgument) ok = false;
        if (badarg) {
          const Out o1 = c_count(k, n);
          const Out o2 = c_unrank(k, n, v);
          const Out o3 = c_rank(k, n, v);
          const Out o4 = c_next(k, n, v);
          const bool all = o1.r == GrResult::BadArgument && o2.r == GrResult::BadArgument && o3.r == GrResult::BadArgument && o4.r == GrResult::BadArgument;
          if (!all || !o1.guards || !o2.guards || !o3.guards || !o4.guards) ok = false;
        }
      }
    }
  }
  // a bad code comes before NoNext and before everything else the code could decide
  const Out x1 = c_next(2u, 3u, 0x1100u);
  const Out x2 = c_next(3u, 2u, 0x23u);
  const Out x3 = c_rank(3u, 2u, 0x23u);
  const Out x4 = c_next(2u, 3u, 0x100u);
  if (x1.r != GrResult::BadCode || x2.r != GrResult::BadCode || x3.r != GrResult::BadCode || x4.r != GrResult::NoNext) ok = false;
  if (!x1.guards || !x2.guards || !x3.guards || !x4.guards) ok = false;
  // RankTooLarge and Overflow are the only value failures of count and unrank
  const Out y1 = c_unrank(2u, 1u, 2u);
  const Out y2 = c_count(16u, 16u);
  if (y1.r != GrResult::RankTooLarge || y2.r != GrResult::Overflow || !y1.guards || !y2.guards) ok = false;
  bool never = true;
  for (u32 k = 2u; k <= 16u; ++k) {
    for (u32 n = 1u; n <= 16u; ++n) {
      const Out ct = c_count(k, n);
      const bool should_overflow = k == 16u && n == 16u;
      if ((ct.r == GrResult::Overflow) != should_overflow || !ct.guards) never = false;
    }
  }
  CHECK(ok);
  CHECK(never);
}

TEST_CASE("graycode: G1 binary structured set and seeded values", "[graycode]") {
  u64 cnt = 0u;
  const u64 h = g1_digest(cnt);
  bool ok = true;
  auto prop = [&](u64 x) {
    const u64 g = gray_encode(x);
    if (gray_decode(g) != x || o_decode(g) != x || gray_encode(gray_decode(x)) != x || gray_decode(x) != o_decode(x)) ok = false;
    if (popcount64(gray_encode(x) ^ gray_encode(x + 1u)) != 1u && x != kMax64) ok = false;
  };
  for (u32 i = 0u; i < 64u; ++i) {
    prop(1ull << i);
    prop((1ull << i) - 1u);
    prop((1ull << i) + 1u);
    for (u32 j = i; j < 64u; ++j) {
      const u64 hi = j == 63u ? kMax64 : (1ull << (j + 1u)) - 1u;
      prop(hi ^ ((1ull << i) - 1u));
      if (j > i) prop((1ull << i) | (1ull << j));
    }
  }
  prop(0u);
  prop(0x5555555555555555ull);
  prop(0xAAAAAAAAAAAAAAAAull);
  prop(kMax64);
  for (u32 i = 0u; i < 200000u; ++i) prop(splitmix_at((1ull << 40) + i));
  CHECK(ok);
  CHECK(cnt == kG1Count);
  CHECK(h == kG1Digest);
}

TEST_CASE("graycode: G1b every value below 2^22", "[graycode]") {
  bool ok = true;
  for (u64 x = 0u; x < (1ull << 22); ++x) {
    const u64 g = gray_encode(x);
    if (gray_decode(g) != x || gray_encode(gray_decode(x)) != x) ok = false;
    if (popcount64(g ^ gray_encode(x + 1u)) != 1u) ok = false;
  }
  CHECK(ok);
}

TEST_CASE("graycode: G2 counts for every base and digit count", "[graycode]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 k = 2u; k <= 16u; ++k) {
    for (u32 n = 1u; n <= 16u; ++n) {
      const Out o = c_count(k, n);
      bool big = false;
      const u64 c = o_count(k, n, &big);
      if (!o.guards || (o.r == GrResult::Overflow) != big || (!big && o.v != c)) ok = false;
      h = resf(fold(fold(h, k), n), o.r, o.v);
      ++cnt;
    }
  }
  CHECK(ok);
  CHECK(cnt == kG2Count);
  CHECK(h == kG2Digest);
}

TEST_CASE("graycode: boundary bits, (16, 16) and digits k to 15", "[graycode]") {
  bool ok = true;
  bool high = true;
  bool digits = true;
  bool all16 = true;
  // n = 15: each of bits 60..63 set on a valid code is BadCode (rank and next); n = 16 has no high bits
  for (u32 k = 2u; k <= 16u; ++k) {
    bool big = false;
    const u64 c = o_count(k, 15u, &big);
    const u64 picks[3] = {0u, c - 1u, c / 2u};
    for (u32 q = 0u; q < 3u; ++q) {
      const u64 w = c_unrank(k, 15u, picks[q]).v;
      for (u32 b = 60u; b < 64u; ++b) {
        const Out r = c_rank(k, 15u, w | (1ull << b));
        const Out x = c_next(k, 15u, w | (1ull << b));
        if (r.r != GrResult::BadCode || x.r != GrResult::BadCode || !r.guards || !x.guards) high = false;
      }
    }
  }
  // n = 16 with k < 16: the digit values k..15 in every position are BadCode
  for (u32 k = 2u; k < 16u; ++k) {
    const u64 w = c_unrank(k, 16u, splitmix_at(k) % o_count(k, 16u, nullptr)).v;
    for (u32 p = 0u; p < 16u; ++p) {
      for (u32 v = k; v < 16u; ++v) {
        const u64 code = (w & ~(15ull << (4u * p))) | (static_cast<u64>(v) << (4u * p));
        const Out r = c_rank(k, 16u, code);
        const Out x = c_next(k, 16u, code);
        if (r.r != GrResult::BadCode || x.r != GrResult::BadCode || !r.guards || !x.guards) digits = false;
      }
    }
  }
  // (16, 16): every code is valid and every rank is valid; rank and unrank are inverse on seeded values and the ends
  for (u32 i = 0u; i < 20000u; ++i) {
    const u64 v = splitmix_at((1ull << 46) + i);
    const Out r = c_rank(16u, 16u, v);
    const Out u = c_unrank(16u, 16u, v);
    if (r.r != GrResult::Ok || u.r != GrResult::Ok || !r.guards || !u.guards) all16 = false;
    if (c_unrank(16u, 16u, r.v).v != v || c_rank(16u, 16u, u.v).v != v) all16 = false;
  }
  const Out e1 = c_rank(16u, 16u, 0u);
  const Out e2 = c_rank(16u, 16u, kMax64);
  const Out e3 = c_unrank(16u, 16u, 0u);
  const Out e4 = c_unrank(16u, 16u, kMax64);
  if (e1.r != GrResult::Ok || e1.v != 0u || e2.r != GrResult::Ok || e3.v != 0u || e4.v != 0xF000000000000000ull) ok = false;
  CHECK(ok);
  CHECK(high);
  CHECK(digits);
  CHECK(all16);
}

TEST_CASE("graycode: one digit changes by 1 across every block boundary", "[graycode]") {
  bool ok = true;
  for (u32 k = 2u; k <= 16u; ++k) {
    for (u32 n = 1u; n <= 16u; ++n) {
      const u64 pw = o_count(k, n - 1u, nullptr);
      for (u32 j = 1u; j < k; ++j) {
        const u64 b = j * pw;
        const Out a0 = c_unrank(k, n, b - 1u);
        const Out a1 = c_unrank(k, n, b);
        const Out a2 = c_unrank(k, n, b + 1u);
        // b + 1 is the last rank of nothing only when n = 1 and j = k - 1 (then b + 1 = k is RankTooLarge and a1 is the last code)
        const bool has2 = !(pw == 1u && j == k - 1u);
        if (a0.r != GrResult::Ok || a1.r != GrResult::Ok || (a2.r == GrResult::Ok) != has2) ok = false;
        if (!one_digit_by_one(a0.v, a1.v, n) || c_next(k, n, a0.v).v != a1.v) ok = false;
        if (has2 && (!one_digit_by_one(a1.v, a2.v, n) || c_next(k, n, a1.v).v != a2.v)) ok = false;
        if (!has2 && c_next(k, n, a1.v).r != GrResult::NoNext) ok = false;
        // the top digit changes from j - 1 to j at the boundary and nothing else does
        const u64 top0 = (a0.v >> (4u * (n - 1u))) & 15u;
        const u64 top1 = (a1.v >> (4u * (n - 1u))) & 15u;
        if (top0 + 1u != top1 || top1 != j) ok = false;
      }
    }
  }
  CHECK(ok);
}

TEST_CASE("graycode: G5a ranks, bases 2 to 6", "[graycode]") {
  const bool ok = ranks_group(g_all, 5u, kG5aDigest, kG5aCount);
  CHECK(ok);
}
TEST_CASE("graycode: G5b ranks, bases 7 to 10", "[graycode]") {
  const bool ok = ranks_group(g_all + 5, 4u, kG5bDigest, kG5bCount);
  CHECK(ok);
}
TEST_CASE("graycode: G5c ranks, bases 11 to 16", "[graycode]") {
  const bool ok = ranks_group(g_all + 9, 6u, kG5cDigest, kG5cCount);
  CHECK(ok);
}

TEST_CASE("graycode: G6b sampled and invalid codes", "[graycode]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 k = 2u; k <= 16u; ++k) {
    for (u32 n = 1u; n <= 16u; ++n) {
      if (!codes_case(k, n, h, cnt)) ok = false;
    }
  }
  CHECK(ok);
  CHECK(cnt == kG6bCount);
  CHECK(h == kG6bDigest);
}

TEST_CASE("graycode: G6a every code below 2^16, n = 1..4", "[graycode]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 k = 2u; k <= 16u; ++k) {
    for (u32 n = 1u; n <= 4u; ++n) {
      for (u64 code = 0u; code < 65536u; ++code) {
        const Out rk = c_rank(k, n, code);
        const Out nx = c_next(k, n, code);
        const bool v = o_valid(k, n, code);
        h = fold(fold(fold(h, k), n), code);
        if (v) {
          h = resf(resf(h, rk.r, rk.v), nx.r, nx.v);
          if (rk.r != GrResult::Ok || (nx.r != GrResult::Ok && nx.r != GrResult::NoNext)) ok = false;
        } else {
          h = fold(h, static_cast<u64>(GrResult::BadCode));
          if (rk.r != GrResult::BadCode || nx.r != GrResult::BadCode) ok = false;
        }
        ++cnt;
      }
    }
  }
  CHECK(ok);
  CHECK(cnt == kG6aCount);
  CHECK(h == kG6aDigest);
}

TEST_CASE("graycode: G4a walks, bases 2 to 4", "[graycode]") {
  const bool ok = walks(g_all, 3u, kG4aDigest, kG4aCount);
  CHECK(ok);
}
TEST_CASE("graycode: G4b walks, bases 5 to 9", "[graycode]") {
  const bool ok = walks(g_all + 3, 5u, kG4bDigest, kG4bCount);
  CHECK(ok);
}
TEST_CASE("graycode: G4c walks, bases 10 to 16", "[graycode]") {
  const bool ok = walks(g_all + 8, 7u, kG4cDigest, kG4cCount);
  CHECK(ok);
}
