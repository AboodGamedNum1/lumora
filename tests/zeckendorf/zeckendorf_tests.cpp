// Tests for core/zeckendorf. Expected values come from the header's examples, from a test-only Fibonacci list used by a different method (the library walks a pair
// (F(k), F(k - 1)); the test subtracts from the list), from the definition (a list is valid and its sum is n), and from FNV-1a digests made by tools/zeckendorf_ref.py
// (Python bigints, standard library only). Extents (see docs/zeckendorf.md):
//   every n = 0..2^22 (4 chunks); 200000 sampled u64 (4 shards); F(k) - 1, F(k), F(k) + 1 for every k, 2^k - 1, 2^k, 2^k + 1; every arithmetic index list;
//   100000 random valid index lists (2 shards); every list of length 0..4 over 16 boundary indices; every pair 0..255; every triple 0..12; planted errors;
//   every single-term change of the lists of 2^64 - 1 and 2^64; fib for every k; the cap matrix; the failure cases.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "zeckendorf.h"

namespace {
using lumora::zeckendorf::fib;
using lumora::zeckendorf::kMaxIndex;
using lumora::zeckendorf::kMaxTerms;
using lumora::zeckendorf::zeckendorf;
using lumora::zeckendorf::zeckendorf_value;
using lumora::zeckendorf::ZkResult;
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using List = std::vector<u8>;

constexpr u64 kM64 = 18446744073709551615ull;
constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr u64 kGuard = 0xDEADBEEFCAFEF00Dull;
constexpr u64 kCell = 0x1234567812345678ull;

constexpr std::uint64_t kChunkDigest[4] = {0xeb802c80e6f693e4ull, 0x2a7d5fbe3a415ad1ull, 0x1b0f569a9648a1c6ull, 0xb023d44713da28e2ull};
constexpr std::uint64_t kSampDigest[4] = {0x73bd15d36262797aull, 0x500fbbe5a3076e5dull, 0x2c4a270ccfa0af26ull, 0xcd4a9bd3150a9c10ull};
constexpr std::uint64_t kRlistDigest[2] = {0xc718b814fa4de5d8ull, 0xce75c4e3d0836e16ull};
constexpr std::size_t kStructCount = 470;
constexpr std::uint64_t kStructDigest = 0xd84e9108d48c318full;
constexpr std::size_t kArithCount = 6368;
constexpr std::uint64_t kArithDigest = 0xe30a6b40849aa44cull;
constexpr std::size_t kAlphaCount = 69905;
constexpr std::uint64_t kAlphaDigest = 0x648956b7ba9d23a9ull;
constexpr std::uint64_t kPairsDigest = 0x3b301a6dc2b75fb9ull;
constexpr std::uint64_t kTriplesDigest = 0x24313ed160714607ull;
constexpr std::size_t kPlantedCount = 1573;
constexpr std::uint64_t kPlantedDigest = 0x863703b56c7755ccull;
constexpr std::size_t kAroundCount = 10035;
constexpr std::uint64_t kAroundDigest = 0x1b227aad09ed9b6dull;
constexpr std::uint64_t kFibkDigest = 0x3c1018983f59bc0eull;
constexpr std::uint32_t kRlistTooLarge[2] = {2889u, 2890u};

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }
u64 code_of(ZkResult r) { return static_cast<u64>(r); }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// Test-only Fibonacci list F[0..93] (built by addition) and a list F[94] that wraps is not kept: the table stops at 93.
struct Tab {
  u64 f[94];
  Tab() {
    f[0] = 0u;
    f[1] = 1u;
    for (int i = 2; i < 94; ++i) f[i] = f[i - 1] + f[i - 2];
  }
};
const Tab& tab() {
  static const Tab t;
  return t;
}

// The expected indices of n by subtraction from the table (largest first). Returns the number of terms.
u32 oracle_zeck(u64 n, u8* out) {
  const u64* f = tab().f;
  u32 k = 2u;
  while (k < 93u && f[k + 1u] <= n) ++k;
  u32 terms = 0u;
  u64 rem = n;
  for (; k >= 2u && rem != 0u; --k) {
    if (f[k] <= rem) {
      out[terms++] = static_cast<u8>(k);
      rem -= f[k];
    }
  }
  return terms;
}

struct Vo {
  ZkResult r;
  u64 v;
};
// The expected verdict of an index list straight from the header: the whole list is checked, then the sum.
Vo oracle_value(const List& l) {
  const u64* f = tab().f;
  for (std::size_t i = 0u; i < l.size(); ++i) {
    if (l[i] < 2u || l[i] > 93u) return {ZkResult::BadArgument, 0u};
    if (i > 0u && static_cast<u32>(l[i]) + 2u > l[i - 1u]) return {ZkResult::BadArgument, 0u};
  }
  u64 sum = 0u;
  for (std::size_t i = 0u; i < l.size(); ++i) {
    if (f[l[i]] > kM64 - sum) return {ZkResult::TooLarge, 0u};
    sum += f[l[i]];
  }
  return {ZkResult::Ok, sum};
}

struct Vr {
  ZkResult r;
  u64 v;
  bool guards;  // the cell was between guard words and nothing was written on a failure
};
Vr call_value(const List& l) {
  const u32 n = static_cast<u32>(l.size());
  std::unique_ptr<u8[]> a(new u8[n == 0u ? 1u : n]);  // exactly n bytes: an overread is an ASan error
  for (u32 i = 0u; i < n; ++i) a[i] = l[i];
  u64 c[3] = {kGuard, kCell, kGuard};
  const ZkResult r = zeckendorf_value(a.get(), n, &c[1]);
  bool g = c[0] == kGuard && c[2] == kGuard;
  if (r != ZkResult::Ok) g = g && c[1] == kCell;
  return {r, c[1], g};
}

// Checks one list against the oracle and folds the words of the reference digest. Returns false on a mismatch.
bool check_list(const List& l, u64& h) {
  const Vr x = call_value(l);
  const Vo o = oracle_value(l);
  bool good = x.guards && x.r == o.r;
  if (o.r == ZkResult::Ok) good = good && x.v == o.v;
  h = fold(h, l.size());
  for (std::size_t i = 0u; i < l.size(); ++i) h = fold(h, l[i]);
  h = fold(h, code_of(x.r));
  h = fold(h, x.r == ZkResult::Ok ? x.v : 0u);
  // an Ok list is the Zeckendorf list of its own sum
  if (o.r == ZkResult::Ok && good) {
    u8 back[64];
    const u32 t = oracle_zeck(o.v, back);
    good = good && t == l.size();
    for (std::size_t i = 0u; good && i < l.size(); ++i) good = back[i] == l[i];
  }
  return good;
}

// One n: the library against the oracle, the definition (valid list, sum n), and the buffer rules. heap = also a call with a buffer of exactly the term count.
void check_n(u64 n, u64& h, bool& ok, bool heap) {
  u8 want[64];
  const u32 wt = oracle_zeck(n, want);
  u8 buf[64];
  for (u8& b : buf) b = 0xEE;
  u32 cnt = 0xFFFFFFFFu;
  const ZkResult r = zeckendorf(n, buf, 64u, &cnt);
  bool good = r == ZkResult::Ok && cnt == wt && wt <= kMaxTerms;
  for (u32 i = 0u; i < 64u; ++i) {
    if (i < wt) {
      good = good && buf[i] == want[i];
    } else {
      good = good && buf[i] == 0xEE;
    }
  }
  u64 v = kCell;
  const ZkResult rv = zeckendorf_value(buf, cnt > 64u ? 0u : cnt, &v);
  good = good && rv == ZkResult::Ok && v == n;
  if (wt >= 1u) {
    u8 b2[64];
    for (u8& b : b2) b = 0xEE;
    u32 c2 = 0xABCDu;
    const ZkResult r2 = zeckendorf(n, b2, wt - 1u, &c2);
    good = good && r2 == ZkResult::BufferTooSmall && c2 == 0xABCDu;
    for (u8 b : b2) good = good && b == 0xEE;
  }
  if (heap) {
    std::unique_ptr<u8[]> hb(new u8[wt == 0u ? 1u : wt]);
    u32 c3 = 0xFFFFFFFFu;
    const ZkResult r3 = zeckendorf(n, hb.get(), wt, &c3);
    good = good && r3 == ZkResult::Ok && c3 == wt;
    for (u32 i = 0u; i < wt; ++i) good = good && hb[i] == want[i];
  }
  if (!good) ok = false;
  h = fold(fold(h, n), cnt);
  for (u32 i = 0u; i < cnt && i < 64u; ++i) h = fold(h, buf[i]);
}

void chunk(unsigned k) {
  u64 h = kFnv0;
  bool ok = true;
  const u64 lo = u64{k} << 20;
  const u64 hi = ((u64{k} + 1u) << 20) + (k == 3u ? 1u : 0u);
  for (u64 n = lo; n < hi; ++n) check_n(n, h, ok, false);
  CHECK(ok);
  CHECK(h == kChunkDigest[k]);
}

void samp(unsigned k) {
  u64 h = kFnv0;
  bool ok = true;
  u32 maxterms = 0u;
  for (u64 i = u64{k} * 50000u; i < (u64{k} + 1u) * 50000u; ++i) {
    const u64 n = splitmix_at(i);
    check_n(n, h, ok, true);
    u8 w[64];
    const u32 t = oracle_zeck(n, w);
    if (t > maxterms) maxterms = t;
  }
  CHECK(ok);
  CHECK(maxterms <= kMaxTerms);
  CHECK(h == kSampDigest[k]);
}

List rlist(u64 i) {
  const u64 base = 1ull << 43;
  const u64 r0 = splitmix_at(4u * i + base);
  const u64 r1 = splitmix_at(4u * i + 1u + base);
  const u64 r2 = splitmix_at(4u * i + 2u + base);
  int cur = 93 - static_cast<int>(r0 % 6u);
  const u64 length = 1u + r1 % 46u;
  List out;
  for (u64 j = 0u; j < length; ++j) {
    if (cur < 2) break;
    out.push_back(static_cast<u8>(cur));
    cur -= 2 + static_cast<int>((r2 >> j) & 1u);
  }
  return out;
}

void rlists(unsigned k) {
  u64 h = kFnv0;
  bool ok = true;
  u32 big = 0u;
  for (u64 i = u64{k} * 50000u; i < (u64{k} + 1u) * 50000u; ++i) {
    const List l = rlist(i);
    if (!check_list(l, h)) ok = false;
    if (oracle_value(l).r == ZkResult::TooLarge) ++big;
  }
  CHECK(ok);
  CHECK(big == kRlistTooLarge[k]);
  CHECK(h == kRlistDigest[k]);
}

const List& b1() {
  static const List l = {93, 91, 88, 86, 82, 76, 74, 70, 66, 63, 59, 56, 47, 44, 40, 37, 33, 31, 29, 25, 19, 17, 13, 11, 5, 3};  // 2^64 - 1
  return l;
}
const List& b4() {
  static const List l = {93, 91, 88, 86, 82, 76, 74, 70, 66, 63, 59, 56, 47, 44, 40, 37, 33, 31, 29, 25, 19, 17, 13, 11, 6};  // 2^64
  return l;
}
List evens() {
  List l;
  for (int k = 92; k >= 2; k -= 2) l.push_back(static_cast<u8>(k));
  return l;
}
List odds() {
  List l;
  for (int k = 93; k >= 3; k -= 2) l.push_back(static_cast<u8>(k));
  return l;
}

template <class F>
void each_arith(F&& f) {
  for (int d = 2; d <= 6; ++d) {
    for (int a = 2; a <= 93; ++a) {
      List l;
      for (int cur = a; cur >= 2; cur -= d) {
        l.push_back(static_cast<u8>(cur));
        f(l);
      }
    }
  }
}

template <class F>
void each_alpha(F&& f) {
  static const u8 alpha[16] = {0, 1, 2, 3, 4, 5, 6, 7, 90, 91, 92, 93, 94, 95, 96, 255};
  for (u32 len = 0u; len <= 4u; ++len) {
    u32 total = 1u;
    for (u32 i = 0u; i < len; ++i) total *= 16u;
    for (u32 t = 0u; t < total; ++t) {
      List l(len);
      u32 rest = t;
      for (u32 i = len; i > 0u; --i) {
        l[i - 1u] = alpha[rest % 16u];
        rest /= 16u;
      }
      f(l);
    }
  }
}

template <class F>
void each_planted(F&& f) {
  const List bases[4] = {b1(), evens(), odds(), b4()};
  for (const List& base : bases) {
    const std::size_t n = base.size();
    for (std::size_t i = 0u; i < n; ++i) {
      if (i > 0u) {
        List a = base;
        a[i] = base[i - 1u];
        f(a);
        List b = base;
        b[i] = static_cast<u8>(base[i - 1u] - 1u);
        f(b);
      }
      const u8 vs[5] = {0, 1, 94, 95, 255};
      for (u8 v : vs) {
        List c = base;
        c[i] = v;
        f(c);
      }
      List d = base;
      d.insert(d.begin() + static_cast<std::ptrdiff_t>(i) + 1, base[i]);
      f(d);
      List e = base;
      e.insert(e.begin() + static_cast<std::ptrdiff_t>(i) + 1, static_cast<u8>(base[i] - 1u));
      f(e);
      List g = base;
      g.erase(g.begin() + static_cast<std::ptrdiff_t>(i));
      f(g);
      if (i + 1u < n) {
        List s = base;
        s[i] = base[i + 1u];
        s[i + 1u] = base[i];
        f(s);
      }
    }
    List t1 = base;
    t1.push_back(94);
    f(t1);
    List t2 = base;
    t2.push_back(1);
    f(t2);
    List t3 = base;
    t3.push_back(0);
    f(t3);
  }
}

template <class F>
void each_around(F&& f) {
  const List bases[2] = {b1(), b4()};
  for (const List& base : bases) {
    const std::size_t n = base.size();
    for (std::size_t i = 0u; i < n; ++i) {
      for (u32 v = 0u; v < 96u; ++v) {
        List a = base;
        a[i] = static_cast<u8>(v);
        f(a);
      }
      List b = base;
      b.erase(b.begin() + static_cast<std::ptrdiff_t>(i));
      f(b);
    }
    for (std::size_t i = 0u; i <= n; ++i) {
      for (u32 v = 0u; v < 96u; ++v) {
        List a = base;
        a.insert(a.begin() + static_cast<std::ptrdiff_t>(i), static_cast<u8>(v));
        f(a);
      }
    }
  }
}

template <class G>
void run_lists(G&& gen, std::size_t want_count, u64 want_digest) {
  u64 h = kFnv0;
  bool ok = true;
  std::size_t count = 0u;
  u32 ok_n = 0u;
  u32 bad_n = 0u;
  u32 big_n = 0u;
  gen([&](const List& l) {
    ++count;
    if (!check_list(l, h)) ok = false;
    const ZkResult r = oracle_value(l).r;
    if (r == ZkResult::Ok) ++ok_n;
    if (r == ZkResult::BadArgument) ++bad_n;
    if (r == ZkResult::TooLarge) ++big_n;
  });
  CHECK(ok);
  if (want_count != 0u) CHECK(count == want_count);
  CHECK(h == want_digest);
  CHECK(ok_n + bad_n + big_n == count);
}

// The cap rule for n: Ok with the oracle indices when cap >= terms, else BufferTooSmall with the buffer and the count untouched. The buffer is exactly cap bytes plus a sentinel.
bool check_cap(u64 n, u32 cap) {
  u8 want[64];
  const u32 wt = oracle_zeck(n, want);
  std::unique_ptr<u8[]> b(new u8[cap + 1u]);
  for (u32 i = 0u; i <= cap; ++i) b[i] = 0xEE;
  u32 c = 0xABCDu;
  const ZkResult r = zeckendorf(n, b.get(), cap, &c);
  bool good = b[cap] == 0xEE;
  if (cap >= wt) {
    good = good && r == ZkResult::Ok && c == wt;
    for (u32 i = 0u; i < wt; ++i) good = good && b[i] == want[i];
    for (u32 i = wt; i < cap; ++i) good = good && b[i] == 0xEE;
  } else {
    good = good && r == ZkResult::BufferTooSmall && c == 0xABCDu;
    for (u32 i = 0u; i < cap; ++i) good = good && b[i] == 0xEE;
  }
  return good;
}
}  // namespace

TEST_CASE("header examples", "[zeckendorf]") {
  const Tab& t = tab();
  CHECK(kMaxIndex == 93u);
  CHECK(kMaxTerms == 46u);
  const u32 ks[10] = {0u, 1u, 2u, 10u, 40u, 70u, 80u, 90u, 92u, 93u};
  const u64 fs[10] = {0ull, 1ull, 1ull, 55ull, 102334155ull, 190392490709135ull, 23416728348467685ull, 2880067194370816120ull, 7540113804746346429ull, 12200160415121876738ull};
  bool fok = true;
  for (int i = 0; i < 10; ++i) {
    u64 v = kCell;
    const ZkResult r = fib(ks[i], &v);
    if (r != ZkResult::Ok || v != fs[i] || v != t.f[ks[i]]) fok = false;
  }
  CHECK(fok);
  u64 v94 = kCell;
  CHECK(fib(94u, &v94) == ZkResult::TooLarge);
  CHECK(v94 == kCell);
  struct Ex {
    u64 n;
    List l;
  };
  const Ex ex[] = {{0u, {}},
                   {1u, {2}},
                   {2u, {3}},
                   {3u, {4}},
                   {4u, {4, 2}},
                   {10u, {6, 3}},
                   {100u, {11, 6, 4}},
                   {1000u, {16, 7}},
                   {1000000u, {30, 26, 24, 12, 10}},
                   {1ull << 63, {92, 88, 86, 84, 75, 71, 67, 63, 61, 54, 51, 49, 42, 40, 38, 34, 31, 27, 25, 22, 20, 17, 12, 8}},
                   {12200160415121876738ull, {93}},
                   {12200160415121876737ull, evens()},
                   {kM64, b1()}};
  bool zok = true;
  for (const Ex& e : ex) {
    u8 buf[64];
    u32 cnt = 0xFFFFu;
    const ZkResult r = zeckendorf(e.n, buf, 64u, &cnt);
    if (r != ZkResult::Ok || cnt != e.l.size()) {
      zok = false;
      continue;
    }
    for (std::size_t i = 0u; i < e.l.size(); ++i) zok = zok && buf[i] == e.l[i];
    const Vr x = call_value(e.l);
    zok = zok && x.r == ZkResult::Ok && x.v == e.n && x.guards;
  }
  CHECK(zok);
  const Vr a = call_value({93, 91});
  CHECK(a.r == ZkResult::Ok);
  CHECK(a.v == 16860207025497407047ull);
  CHECK(call_value({93, 91, 89}).r == ZkResult::TooLarge);
  CHECK(call_value(b4()).r == ZkResult::TooLarge);
  CHECK(call_value(b1()).r == ZkResult::Ok);
  CHECK(call_value({3, 3}).r == ZkResult::BadArgument);
  CHECK(call_value({4, 3}).r == ZkResult::BadArgument);
  CHECK(call_value({1}).r == ZkResult::BadArgument);
  CHECK(call_value({94}).r == ZkResult::BadArgument);
  CHECK(call_value({}).r == ZkResult::Ok);
  CHECK(call_value({}).v == 0u);
}

TEST_CASE("failure cases: nulls first, then the list or buffer, then the size; nothing written", "[zeckendorf]") {
  // fib
  u64 v = kCell;
  CHECK(fib(5u, nullptr) == ZkResult::NullArgument);
  CHECK(fib(94u, nullptr) == ZkResult::NullArgument);
  CHECK(fib(94u, &v) == ZkResult::TooLarge);
  CHECK(fib(0xFFFFFFFFu, &v) == ZkResult::TooLarge);
  CHECK(v == kCell);
  // zeckendorf
  u8 buf[8] = {0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE};
  u32 cnt = 77u;
  CHECK(zeckendorf(5u, nullptr, 8u, &cnt) == ZkResult::NullArgument);
  CHECK(zeckendorf(0u, nullptr, 8u, &cnt) == ZkResult::NullArgument);
  CHECK(zeckendorf(5u, buf, 8u, nullptr) == ZkResult::NullArgument);
  CHECK(zeckendorf(5u, nullptr, 8u, nullptr) == ZkResult::NullArgument);
  CHECK(zeckendorf(4u, nullptr, 0u, &cnt) == ZkResult::NullArgument);  // nulls before the buffer rule
  CHECK(zeckendorf(4u, buf, 0u, nullptr) == ZkResult::NullArgument);
  CHECK(cnt == 77u);
  CHECK(zeckendorf(4u, buf, 1u, &cnt) == ZkResult::BufferTooSmall);
  CHECK(cnt == 77u);
  bool untouched = true;
  for (u8 b : buf) untouched = untouched && b == 0xEE;
  CHECK(untouched);
  CHECK(zeckendorf(4u, buf, 2u, &cnt) == ZkResult::Ok);
  CHECK(cnt == 2u);
  CHECK(buf[0] == 4u);
  CHECK(buf[1] == 2u);
  CHECK(buf[2] == 0xEE);
  buf[0] = 0xEE;
  buf[1] = 0xEE;
  cnt = 77u;
  CHECK(zeckendorf(0u, buf, 0u, &cnt) == ZkResult::Ok);  // n = 0 with cap 0 and a non-null idx
  CHECK(cnt == 0u);
  CHECK(buf[0] == 0xEE);
  CHECK(zeckendorf(1u, buf, 0u, &cnt) == ZkResult::BufferTooSmall);
  CHECK(cnt == 0u);
  // zeckendorf_value
  const u8 good[2] = {4, 2};
  const u8 bad[2] = {4, 3};
  u64 out = kCell;
  CHECK(zeckendorf_value(good, 2u, nullptr) == ZkResult::NullArgument);
  CHECK(zeckendorf_value(bad, 2u, nullptr) == ZkResult::NullArgument);  // nulls before the list
  CHECK(zeckendorf_value(nullptr, 2u, &out) == ZkResult::NullArgument);
  CHECK(zeckendorf_value(nullptr, 1u, &out) == ZkResult::NullArgument);
  CHECK(zeckendorf_value(nullptr, 0u, nullptr) == ZkResult::NullArgument);
  CHECK(out == kCell);
  CHECK(zeckendorf_value(nullptr, 0u, &out) == ZkResult::Ok);
  CHECK(out == 0u);
  out = kCell;
  CHECK(zeckendorf_value(bad, 2u, &out) == ZkResult::BadArgument);
  CHECK(out == kCell);
  // the whole list is checked before the size: a prefix that already overflows must not hide a later bad index
  const u8 over_dup[4] = {93, 91, 89, 89};
  const u8 over_big[4] = {93, 91, 89, 94};
  const u8 over_adj[4] = {93, 91, 89, 88};
  const u8 over_low[4] = {93, 91, 89, 1};
  const u8 over_ok[3] = {93, 91, 89};
  CHECK(zeckendorf_value(over_dup, 4u, &out) == ZkResult::BadArgument);
  CHECK(zeckendorf_value(over_big, 4u, &out) == ZkResult::BadArgument);
  CHECK(zeckendorf_value(over_adj, 4u, &out) == ZkResult::BadArgument);
  CHECK(zeckendorf_value(over_low, 4u, &out) == ZkResult::BadArgument);
  CHECK(zeckendorf_value(over_ok, 3u, &out) == ZkResult::TooLarge);
  CHECK(out == kCell);
}

TEST_CASE("long counts stop at the first violation and never read past it", "[zeckendorf]") {
  // 46 valid terms then a repeat of 3: the 47th element is bad, so counts 47..255 give BadArgument without reading element 47 or later (the array is exactly 47 bytes).
  List l = odds();
  l.push_back(3);
  std::unique_ptr<u8[]> a(new u8[47]);
  for (u32 i = 0u; i < 47u; ++i) a[i] = l[i];
  bool ok = true;
  for (u32 c = 47u; c <= 255u; ++c) {
    u64 out = kCell;
    if (zeckendorf_value(a.get(), c, &out) != ZkResult::BadArgument || out != kCell) ok = false;
  }
  CHECK(ok);
  u64 out = kCell;
  CHECK(zeckendorf_value(a.get(), 1000000u, &out) == ZkResult::BadArgument);
  // 47 distinct descending indices cannot be valid: the densest list 94, 92, ..., 2 is out of range and 93, 91, ..., 1 reaches index 1.
  std::unique_ptr<u8[]> d1(new u8[47]);
  std::unique_ptr<u8[]> d2(new u8[47]);
  for (u32 i = 0u; i < 47u; ++i) {
    d1[i] = static_cast<u8>(94u - 2u * i);
    d2[i] = static_cast<u8>(93u - 2u * i);
  }
  CHECK(zeckendorf_value(d1.get(), 47u, &out) == ZkResult::BadArgument);
  CHECK(zeckendorf_value(d2.get(), 47u, &out) == ZkResult::BadArgument);
  CHECK(out == kCell);
  // a bad first element with a huge count reads only that element
  const u8 one_bad[1] = {94};
  const u8 one_low[1] = {0};
  CHECK(zeckendorf_value(one_bad, 4000000000u, &out) == ZkResult::BadArgument);
  CHECK(zeckendorf_value(one_low, 4000000000u, &out) == ZkResult::BadArgument);
  // every valid prefix of the odd list of length 1..46 against the oracle
  bool pok = true;
  const List full = odds();
  for (std::size_t n = 1u; n <= full.size(); ++n) {
    const List p(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n));
    const Vr x = call_value(p);
    const Vo o = oracle_value(p);
    if (x.r != o.r || !x.guards || (o.r == ZkResult::Ok && x.v != o.v)) pok = false;
  }
  CHECK(pok);
}

TEST_CASE("fib: every k = 0..93 and the identity F(k) = F(k - 1) + F(k - 2)", "[zeckendorf]") {
  const Tab& t = tab();
  u64 h = kFnv0;
  bool ok = true;
  u64 prev2 = 0u;
  u64 prev1 = 0u;
  for (u32 k = 0u; k < 94u; ++k) {
    u64 v = kCell;
    const ZkResult r = fib(k, &v);
    if (r != ZkResult::Ok || v != t.f[k]) ok = false;
    if (k >= 2u && v != prev1 + prev2) ok = false;
    if (k >= 2u && v < prev1) ok = false;
    h = fold(fold(h, k), v);
    prev2 = prev1;
    prev1 = v;
  }
  CHECK(ok);
  CHECK(h == kFibkDigest);
  CHECK(t.f[93] == 12200160415121876738ull);
  CHECK(t.f[93] > kM64 - t.f[92]);  // F(94) = F(93) + F(92) does not fit a u64
  const u32 big[6] = {94u, 95u, 100u, 255u, 65536u, 0xFFFFFFFFu};
  bool bok = true;
  for (u32 k : big) {
    u64 v = kCell;
    if (fib(k, &v) != ZkResult::TooLarge || v != kCell) bok = false;
  }
  CHECK(bok);
}

TEST_CASE("chunk 0 (n = 0 .. 2^20 - 1)", "[zeckendorf][chunk]") { chunk(0u); }
TEST_CASE("chunk 1 (n = 2^20 .. 2^21 - 1)", "[zeckendorf][chunk]") { chunk(1u); }
TEST_CASE("chunk 2 (n = 2^21 .. 3 * 2^20 - 1)", "[zeckendorf][chunk]") { chunk(2u); }
TEST_CASE("chunk 3 (n = 3 * 2^20 .. 2^22)", "[zeckendorf][chunk]") { chunk(3u); }

TEST_CASE("structured: F(k) - 1, F(k), F(k) + 1, 2^k - 1, 2^k, 2^k + 1 and the top values", "[zeckendorf]") {
  const Tab& t = tab();
  u64 h = kFnv0;
  bool ok = true;
  std::size_t count = 0u;
  for (u32 k = 2u; k < 94u; ++k) {
    const u64 vs[3] = {t.f[k] - 1u, t.f[k], t.f[k] + 1u};
    for (u64 n : vs) {
      check_n(n, h, ok, true);
      ++count;
    }
    // F(k) is the single index k; F(k) - 1 has (k - 1) / 2 terms, every other index from k - 1 down
    u8 w[64];
    if (oracle_zeck(t.f[k], w) != 1u || w[0] != k) ok = false;
    const u32 terms = oracle_zeck(t.f[k] - 1u, w);
    if (terms != (k - 1u) / 2u) ok = false;
    for (u32 i = 0u; i < terms; ++i) {
      if (w[i] != k - 1u - 2u * i) ok = false;
    }
    u8 buf[64];
    u32 c = 0u;
    if (zeckendorf(t.f[k] - 1u, buf, 64u, &c) != ZkResult::Ok || c != terms) ok = false;
  }
  for (u32 k = 0u; k < 64u; ++k) {
    const u64 p = 1ull << k;
    const u64 vs[3] = {p - 1u, p, p + 1u};
    for (u64 n : vs) {
      check_n(n, h, ok, true);
      ++count;
    }
  }
  check_n(kM64, h, ok, true);
  check_n(kM64 - 1u, h, ok, true);
  count += 2u;
  CHECK(ok);
  CHECK(count == kStructCount);
  CHECK(h == kStructDigest);
}

TEST_CASE("the all-even and all-odd lists: F(2m + 1) - 1 and F(2m + 2) - 1", "[zeckendorf]") {
  const Tab& t = tab();
  bool ok = true;
  for (u32 m = 1u; 2u * m <= 92u; ++m) {
    List ev;  // 2m, 2m - 2, ..., 2: sum F(2m + 1) - 1, m terms
    for (u32 k = 2u * m; k >= 2u; k -= 2u) ev.push_back(static_cast<u8>(k));
    const Vr a = call_value(ev);
    if (a.r != ZkResult::Ok || a.v != t.f[2u * m + 1u] - 1u || !a.guards) ok = false;
    u8 buf[64];
    u32 c = 0u;
    if (zeckendorf(a.v, buf, 64u, &c) != ZkResult::Ok || c != m) ok = false;
    for (u32 i = 0u; i < c && i < 64u; ++i) {
      if (buf[i] != ev[i]) ok = false;
    }
  }
  for (u32 m = 1u; 2u * m + 1u <= 93u; ++m) {
    List od;  // 2m + 1, 2m - 1, ..., 3: sum F(2m + 2) - 1, m terms
    for (u32 k = 2u * m + 1u; k >= 3u; k -= 2u) od.push_back(static_cast<u8>(k));
    const Vr a = call_value(od);
    const bool fits = 2u * m + 2u <= 93u;
    if (fits) {
      if (a.r != ZkResult::Ok || a.v != t.f[2u * m + 2u] - 1u || !a.guards) ok = false;
    } else {
      if (a.r != ZkResult::TooLarge || !a.guards) ok = false;  // the full odd list 93..3 sums F(94) - 1, above u64
    }
  }
  CHECK(ok);
  CHECK(call_value(odds()).r == ZkResult::TooLarge);
  CHECK(call_value(evens()).v == 12200160415121876737ull);
}

TEST_CASE("the 46-term maximum, caps 0, 1, 25, 26, 45, 46, 255 for values of 0, 1, 25, 26 and 46 terms", "[zeckendorf]") {
  const u64 ns[7] = {0u, 1u, kM64 - 2u, kM64, 12200160415121876737ull, 12200160415121876736ull, 4u};
  const u32 caps[7] = {0u, 1u, 25u, 26u, 45u, 46u, 255u};
  bool ok = true;
  for (u64 n : ns) {
    for (u32 cap : caps) {
      if (!check_cap(n, cap)) ok = false;
    }
  }
  CHECK(ok);
  u8 w[64];
  CHECK(oracle_zeck(0u, w) == 0u);
  CHECK(oracle_zeck(kM64 - 2u, w) == 25u);
  CHECK(oracle_zeck(kM64, w) == 26u);
  CHECK(oracle_zeck(12200160415121876737ull, w) == 46u);
  // no u64 needs more than 46 terms
  u8 buf[64];
  u32 c = 0u;
  CHECK(zeckendorf(12200160415121876737ull, buf, 45u, &c) == ZkResult::BufferTooSmall);
  CHECK(zeckendorf(12200160415121876737ull, buf, 46u, &c) == ZkResult::Ok);
  CHECK(c == 46u);
}

TEST_CASE("arithmetic lists: a, a - d, a - 2d, ... for d = 2..6, every start, every length", "[zeckendorf]") {
  run_lists([](auto&& f) { each_arith(f); }, kArithCount, kArithDigest);
}

TEST_CASE("samples 0", "[zeckendorf][sample]") { samp(0u); }
TEST_CASE("samples 1", "[zeckendorf][sample]") { samp(1u); }
TEST_CASE("samples 2", "[zeckendorf][sample]") { samp(2u); }
TEST_CASE("samples 3", "[zeckendorf][sample]") { samp(3u); }

TEST_CASE("random valid lists 0", "[zeckendorf][sample]") { rlists(0u); }
TEST_CASE("random valid lists 1", "[zeckendorf][sample]") { rlists(1u); }

TEST_CASE("invalid lists: every list of length 0..4 over 0 1 2 3 4 5 6 7 90 91 92 93 94 95 96 255", "[zeckendorf]") {
  run_lists([](auto&& f) { each_alpha(f); }, kAlphaCount, kAlphaDigest);
}

TEST_CASE("invalid lists: every pair p, q in 0..255", "[zeckendorf]") {
  run_lists(
      [](auto&& f) {
        for (u32 p = 0u; p < 256u; ++p) {
          for (u32 q = 0u; q < 256u; ++q) f(List{static_cast<u8>(p), static_cast<u8>(q)});
        }
      },
      65536u, kPairsDigest);
}

TEST_CASE("invalid lists: every triple a, b, c in 0..12", "[zeckendorf]") {
  run_lists(
      [](auto&& f) {
        for (u32 a = 0u; a < 13u; ++a) {
          for (u32 b = 0u; b < 13u; ++b) {
            for (u32 c = 0u; c < 13u; ++c) f(List{static_cast<u8>(a), static_cast<u8>(b), static_cast<u8>(c)});
          }
        }
      },
      2197u, kTriplesDigest);
}

TEST_CASE("planted errors: a duplicate, an adjacent pair, a swap or an out-of-range index at every position of four long lists", "[zeckendorf]") {
  run_lists([](auto&& f) { each_planted(f); }, kPlantedCount, kPlantedDigest);
}

TEST_CASE("around 2^64 - 1 and 2^64: every single-term change, deletion and insertion", "[zeckendorf]") {
  run_lists([](auto&& f) { each_around(f); }, kAroundCount, kAroundDigest);
  // the lists themselves: 2^64 - 1 is Ok and 2^64 is TooLarge (valid, one more than a u64 holds)
  const Vr a = call_value(b1());
  CHECK(a.r == ZkResult::Ok);
  CHECK(a.v == kM64);
  CHECK(call_value(b4()).r == ZkResult::TooLarge);
  const Tab& t = tab();
  bool sum_ok = true;
  u64 s = 0u;
  for (u8 k : b4()) {
    if (t.f[k] > kM64 - s) {
      sum_ok = false;
      break;
    }
    s += t.f[k];
  }
  CHECK_FALSE(sum_ok);  // the sum of b4 does not fit
}
