// Tests for core/stirling. Evidence label: unit tests only.
// Expected values come from tools/stirling_ref.py (standard library only: exact big-integer triangles, inclusion-exclusion, divide-and-conquer rising
// factorial, modular Bell recurrence) as FNV-1a digests over 32-bit words, from explicit textbook values, and from in-test full two-dimensional triangles
// (a different shape from the in-place row sweeps of the implementation). Extents, named in the test names and in docs/stirling.md:
//   exhaustive: every row n = 0..300 of both kinds, every entry, against the in-test triangles and the reference digest of all rows concatenated;
//               every Bell table n = 0..300, every entry (against the row sums of the second-kind triangle and the binomial recurrence);
//               the signed first-kind matrix times the second-kind matrix and the reverse product, every (n, m) with n, m = 0..300;
//   sampled:    rows n in {512, 1000, 1024, 2047, 2048, 2049, 3000, 4095, 4096} of both kinds as reference digests, Bell tables n in
//               {300, 1024, 2048, 4095, 4096} as reference digests, row sums and falling/rising factorial identities at a list of n up to 4096,
//               and for n in {1024, 2048, 4095, 4096} the orthogonality sums for m = 0..520 (the second-kind column is generated in the test).
// Every success run uses output arrays with guard words on both sides and, for bell_table, scratch of exactly n + 1 words at an odd u32 offset with guard
// words on both sides; scratch contents after a call are never asserted (only the guard words); overlap of scratch and out is outside the contract and not
// tested; time is not measured.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stirling.h"

using lumora::stirling::bell_table;
using lumora::stirling::kMaxN;
using lumora::stirling::kMod;
using lumora::stirling::stirling1_row;
using lumora::stirling::stirling2_row;
using lumora::stirling::StirResult;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Vec = std::vector<u32>;

constexpr std::uint64_t kDigestAllS2 = 0x3cff9db73056284cull;  // rows 0..300 concatenated
constexpr std::uint64_t kDigestAllC1 = 0x2620a16a6d1712e5ull;  // rows 0..300 concatenated
struct RowDigest { std::uint32_t n; std::uint64_t digest; };
constexpr RowDigest kS2Digests[] = {
    {512u, 0xa9019eb820211f7dull},
    {1000u, 0x44fc690d1ec007e4ull},
    {1024u, 0xf1800ee4e4c2ff15ull},
    {2047u, 0x7ed0ab535992a025ull},
    {2048u, 0x6d198b94e27faf2bull},
    {2049u, 0x4dd1c57649e25fe0ull},
    {3000u, 0x4a5cf30025dc67a8ull},
    {4095u, 0x9be8223517d5134bull},
    {4096u, 0xcdedd4405c4381a9ull},
};
constexpr RowDigest kC1Digests[] = {
    {512u, 0xe8382a69f53825eaull},
    {1000u, 0xff197342bc125322ull},
    {1024u, 0x8f236df52801b267ull},
    {2047u, 0x41654adfbdc5a60bull},
    {2048u, 0xe9a6ce88e67656f5ull},
    {2049u, 0xd313061576aea1f5ull},
    {3000u, 0xc8092788d9be682full},
    {4095u, 0x6feec59cc07e97ddull},
    {4096u, 0x9d87132d98a77c4full},
};
constexpr RowDigest kBellDigests[] = {  // digest of B(0) .. B(n)
    {300u, 0xba4fd5260b6432a9ull},
    {1024u, 0x019af0fe134a0f91ull},
    {2048u, 0x4d1a2d6664debdb3ull},
    {4095u, 0x9b4d7877b5cc2160ull},
    {4096u, 0xabb968411949e28full},
};

constexpr u32 kGuard = 3u;
constexpr u32 kSentinel = 0xA5A5A5A5u;
constexpr u32 kJunk = 0x5A5A5A5Au;

u64 fnv(const u32* p, std::size_t count, u64 h = 0xcbf29ce484222325ull) {
  for (std::size_t i = 0; i < count; ++i) h = (h ^ static_cast<u64>(p[i])) * 0x100000001b3ull;
  return h;
}
u64 fnv(const Vec& v, u64 h = 0xcbf29ce484222325ull) { return fnv(v.data(), v.size(), h); }

u64 mulmod(u64 a, u64 b) { return (a % kMod) * (b % kMod) % kMod; }
u64 powmod(u64 b, u64 e) {
  u64 r = 1;
  b %= kMod;
  while (e != 0) {
    if ((e & 1u) != 0u) r = r * b % kMod;
    b = b * b % kMod;
    e >>= 1;
  }
  return r;
}

enum class Kind { S2, C1 };

// Runs a row function into an array with guard words on both sides. Returns the result code; row receives n + 1 words.
StirResult run_row(Kind kind, u32 n, Vec& row, bool& guards_ok) {
  Vec buf(static_cast<std::size_t>(n) + 1u + 2u * kGuard, kSentinel);
  const StirResult r = (kind == Kind::S2) ? stirling2_row(n, buf.data() + kGuard) : stirling1_row(n, buf.data() + kGuard);
  guards_ok = true;
  for (u32 i = 0; i < kGuard; ++i) {
    if (buf[i] != kSentinel) guards_ok = false;
    if (buf[static_cast<std::size_t>(n) + 1u + kGuard + i] != kSentinel) guards_ok = false;
  }
  row.assign(buf.begin() + kGuard, buf.begin() + kGuard + n + 1u);
  return r;
}

// Runs bell_table with scratch of exactly `words` u32 words (at least n + 1) filled with `fill`; checks the guards of both arrays.
StirResult run_bell(u32 n, u32 words, u32 fill, Vec& table, bool& guards_ok) {
  Vec sbuf(static_cast<std::size_t>(words) + 2u * kGuard, kSentinel);
  for (u32 i = 0; i < words; ++i) sbuf[kGuard + i] = fill;
  Vec obuf(static_cast<std::size_t>(n) + 1u + 2u * kGuard, kSentinel);
  const StirResult r = bell_table(n, sbuf.data() + kGuard, words, obuf.data() + kGuard);
  guards_ok = true;
  for (u32 i = 0; i < kGuard; ++i) {
    if (sbuf[i] != kSentinel) guards_ok = false;
    if (sbuf[static_cast<std::size_t>(words) + kGuard + i] != kSentinel) guards_ok = false;
    if (obuf[i] != kSentinel) guards_ok = false;
    if (obuf[static_cast<std::size_t>(n) + 1u + kGuard + i] != kSentinel) guards_ok = false;
  }
  table.assign(obuf.begin() + kGuard, obuf.begin() + kGuard + n + 1u);
  return r;
}

// Full two-dimensional triangles to n = 300, built part by part from the defining recurrences (not in place).
struct Tri {
  std::vector<Vec> s2, c1;
};
const Tri& triangles() {
  static const Tri t = [] {
    Tri x;
    x.s2.assign(301, Vec(301, 0u));
    x.c1.assign(301, Vec(301, 0u));
    x.s2[0][0] = 1u;
    x.c1[0][0] = 1u;
    for (std::size_t m = 1; m <= 300; ++m) {
      for (std::size_t k = 1; k <= m; ++k) {
        x.s2[m][k] = static_cast<u32>((static_cast<u64>(k) * x.s2[m - 1][k] + x.s2[m - 1][k - 1]) % kMod);
        x.c1[m][k] = static_cast<u32>((static_cast<u64>(m - 1) * x.c1[m - 1][k] + x.c1[m - 1][k - 1]) % kMod);
      }
    }
    return x;
  }();
  return t;
}

// Rows of the implementation for n = 0..300, kept for the orthogonality products.
struct Rows {
  std::vector<Vec> s2, c1;
};
const Rows& impl_rows() {
  static const Rows r = [] {
    Rows x;
    for (u32 n = 0; n <= 300; ++n) {
      Vec a, b;
      bool g = true;
      (void)run_row(Kind::S2, n, a, g);
      (void)run_row(Kind::C1, n, b, g);
      x.s2.push_back(a);
      x.c1.push_back(b);
    }
    return x;
  }();
  return r;
}

u64 factorial(u32 n) {
  u64 f = 1;
  for (u32 i = 2; i <= n; ++i) f = f * i % kMod;
  return f;
}
u64 sum_row(const Vec& v) {
  u64 s = 0;
  for (u32 x : v) s = (s + x) % kMod;
  return s;
}
bool all_reduced(const Vec& v) {
  for (u32 x : v) {
    if (x >= kMod) return false;
  }
  return true;
}
u64 choose2(u64 n) { return n * (n - 1u) / 2u; }
u64 choose3(u64 n) { return n * (n - 1u) * (n - 2u) / 6u; }
u64 choose4(u64 n) { return n * (n - 1u) * (n - 2u) * (n - 3u) / 24u; }

// Near-diagonal and edge identities of one row pair for n >= 4.
void check_near_diagonal(u32 n, const Vec& s2, const Vec& c1) {
  const u64 N = n;
  bool ok = true;
  ok = ok && (s2[0] == 0u) && (c1[0] == 0u) && (s2[n] == 1u) && (c1[n] == 1u);
  ok = ok && (s2[1] == 1u);
  ok = ok && (s2[2] == (powmod(2, n - 1u) + kMod - 1u) % kMod);
  ok = ok && (c1[1] == factorial(n - 1u));
  ok = ok && (s2[n - 1u] == choose2(N) % kMod) && (c1[n - 1u] == choose2(N) % kMod);
  ok = ok && (s2[n - 2u] == (choose3(N) + 3u * choose4(N)) % kMod);
  ok = ok && (c1[n - 2u] == ((3u * N - 1u) * choose3(N) / 4u) % kMod);
  ok = ok && (c1[n - 3u] == mulmod(choose2(N), choose4(N)));
  CHECK(ok);
}
}  // namespace

TEST_CASE("stirling: failure cases, check order, nothing written", "[stirling]") {
  Vec row(8, kSentinel);
  CHECK(stirling2_row(3u, nullptr) == StirResult::NullArgument);
  CHECK(stirling1_row(3u, nullptr) == StirResult::NullArgument);
  CHECK(stirling2_row(kMaxN + 1u, nullptr) == StirResult::NullArgument);  // null before size
  CHECK(stirling1_row(0xFFFFFFFFu, nullptr) == StirResult::NullArgument);
  CHECK(stirling2_row(kMaxN + 1u, row.data()) == StirResult::TooLarge);
  CHECK(stirling1_row(kMaxN + 1u, row.data()) == StirResult::TooLarge);
  CHECK(stirling2_row(0xFFFFFFFFu, row.data()) == StirResult::TooLarge);
  CHECK(stirling1_row(0xFFFFFFFFu, row.data()) == StirResult::TooLarge);
  bool untouched = true;
  for (u32 v : row) untouched = untouched && (v == kSentinel);
  CHECK(untouched);

  Vec out(8, kSentinel), scr(8, kJunk);
  CHECK(bell_table(3u, scr.data(), 8u, nullptr) == StirResult::NullArgument);
  CHECK(bell_table(3u, nullptr, 8u, out.data()) == StirResult::NullArgument);
  CHECK(bell_table(0u, nullptr, 0u, out.data()) == StirResult::NullArgument);  // null scratch always, even for n = 0
  CHECK(bell_table(0u, nullptr, 1u, out.data()) == StirResult::NullArgument);
  CHECK(bell_table(kMaxN + 1u, nullptr, 0u, nullptr) == StirResult::NullArgument);
  CHECK(bell_table(kMaxN + 1u, scr.data(), 8u, nullptr) == StirResult::NullArgument);  // null out before size
  CHECK(bell_table(kMaxN + 1u, nullptr, 8u, out.data()) == StirResult::NullArgument);  // null scratch before size
  CHECK(bell_table(kMaxN + 1u, scr.data(), 8u, out.data()) == StirResult::TooLarge);
  CHECK(bell_table(0xFFFFFFFFu, scr.data(), 0xFFFFFFFFu, out.data()) == StirResult::TooLarge);
  CHECK(bell_table(kMaxN + 1u, scr.data(), 0u, out.data()) == StirResult::TooLarge);  // size before scratch
  CHECK(bell_table(0xFFFFFFFFu, scr.data(), 0u, out.data()) == StirResult::TooLarge);
  CHECK(bell_table(0u, scr.data(), 0u, out.data()) == StirResult::ScratchTooSmall);
  CHECK(bell_table(1u, scr.data(), 1u, out.data()) == StirResult::ScratchTooSmall);
  CHECK(bell_table(5u, scr.data(), 5u, out.data()) == StirResult::ScratchTooSmall);
  CHECK(bell_table(5u, scr.data(), 0u, out.data()) == StirResult::ScratchTooSmall);
  CHECK(bell_table(kMaxN, scr.data(), kMaxN, out.data()) == StirResult::ScratchTooSmall);  // n words, one short
  CHECK(bell_table(kMaxN, scr.data(), 0u, out.data()) == StirResult::ScratchTooSmall);
  untouched = true;
  for (u32 v : out) untouched = untouched && (v == kSentinel);
  for (u32 v : scr) untouched = untouched && (v == kJunk);
  CHECK(untouched);  // no failure wrote to out or to scratch
}

TEST_CASE("stirling: small known values", "[stirling]") {
  const u32 s2_6[] = {0, 1, 31, 90, 65, 15, 1};
  const u32 c1_6[] = {0, 120, 274, 225, 85, 15, 1};
  Vec a, b;
  bool g = false;
  REQUIRE(run_row(Kind::S2, 6u, a, g) == StirResult::Ok);
  CHECK(g);
  REQUIRE(run_row(Kind::C1, 6u, b, g) == StirResult::Ok);
  CHECK(g);
  bool ok = (a.size() == 7u) && (b.size() == 7u);
  for (std::size_t i = 0; ok && i < 7u; ++i) ok = (a[i] == s2_6[i]) && (b[i] == c1_6[i]);
  CHECK(ok);
  REQUIRE(run_row(Kind::S2, 0u, a, g) == StirResult::Ok);
  CHECK((a.size() == 1u && a[0] == 1u));
  CHECK(g);
  REQUIRE(run_row(Kind::C1, 0u, b, g) == StirResult::Ok);
  CHECK((b.size() == 1u && b[0] == 1u));
  CHECK(g);
  REQUIRE(run_row(Kind::S2, 1u, a, g) == StirResult::Ok);
  CHECK((a.size() == 2u && a[0] == 0u && a[1] == 1u));
  REQUIRE(run_row(Kind::C1, 1u, b, g) == StirResult::Ok);
  CHECK((b.size() == 2u && b[0] == 0u && b[1] == 1u));
  REQUIRE(run_row(Kind::S2, 2u, a, g) == StirResult::Ok);
  CHECK((a.size() == 3u && a[0] == 0u && a[1] == 1u && a[2] == 1u));
  REQUIRE(run_row(Kind::C1, 2u, b, g) == StirResult::Ok);
  CHECK((b.size() == 3u && b[0] == 0u && b[1] == 1u && b[2] == 1u));
  REQUIRE(run_row(Kind::S2, 10u, a, g) == StirResult::Ok);
  REQUIRE(run_row(Kind::C1, 10u, b, g) == StirResult::Ok);
  CHECK(a[5] == 42525u);
  CHECK(b[5] == 269325u);
  // Bell numbers (all below the modulus up to B(14); B(15) = 1382958545 reduces to 384714192).
  const u32 bell_known[] = {1u, 1u, 2u, 5u, 15u, 52u, 203u, 877u, 4140u, 21147u, 115975u, 678570u, 4213597u, 27644437u, 190899322u, 384714192u};
  Vec t;
  REQUIRE(run_bell(15u, 16u, kJunk, t, g) == StirResult::Ok);
  CHECK(g);
  ok = (t.size() == 16u);
  for (std::size_t i = 0; ok && i < 16u; ++i) ok = (t[i] == bell_known[i]);
  CHECK(ok);
  REQUIRE(run_bell(0u, 1u, 0u, t, g) == StirResult::Ok);
  CHECK(g);
  CHECK((t.size() == 1u && t[0] == 1u));
  REQUIRE(run_bell(1u, 2u, 0xFFFFFFFFu, t, g) == StirResult::Ok);
  CHECK((t.size() == 2u && t[0] == 1u && t[1] == 1u));
}

TEST_CASE("stirling: exhaustive rows n = 0..300 of both kinds, every entry", "[stirling][exhaustive]") {
  const Tri& tri = triangles();
  u64 hs = 0xcbf29ce484222325ull, hc = 0xcbf29ce484222325ull;
  u32 bad = 0, bad_guard = 0, bad_range = 0, bad_code = 0;
  for (u32 n = 0; n <= 300; ++n) {
    Vec a, b;
    bool g1 = false, g2 = false;
    if (run_row(Kind::S2, n, a, g1) != StirResult::Ok) ++bad_code;
    if (run_row(Kind::C1, n, b, g2) != StirResult::Ok) ++bad_code;
    if (!g1 || !g2) ++bad_guard;
    if (!all_reduced(a) || !all_reduced(b)) ++bad_range;
    for (u32 k = 0; k <= n; ++k) {
      if (a[k] != tri.s2[n][k]) ++bad;
      if (b[k] != tri.c1[n][k]) ++bad;
    }
    hs = fnv(a, hs);
    hc = fnv(b, hc);
    CHECK(sum_row(b) == factorial(n));
    u64 want = 0;
    for (u32 k = 0; k <= n; ++k) want = (want + tri.s2[n][k]) % kMod;
    CHECK(sum_row(a) == want);
  }
  CHECK(bad == 0u);
  CHECK(bad_guard == 0u);
  CHECK(bad_range == 0u);
  CHECK(bad_code == 0u);
  CHECK(hs == kDigestAllS2);
  CHECK(hc == kDigestAllC1);
}

TEST_CASE("stirling: exhaustive near-diagonal identities n = 4..300", "[stirling][exhaustive]") {
  const Rows& r = impl_rows();
  for (u32 n = 4; n <= 300; ++n) check_near_diagonal(n, r.s2[n], r.c1[n]);
}

TEST_CASE("stirling: exhaustive Bell tables n = 0..300, every entry", "[stirling][exhaustive]") {
  const Tri& tri = triangles();
  // Binomial recurrence B(m+1) = sum_k C(m, k) B(k) with a Pascal triangle built in the test; the second source is the row sums of the S2 triangle.
  std::vector<Vec> binom(301, Vec(301, 0u));
  for (std::size_t m = 0; m <= 300; ++m) {
    binom[m][0] = 1u;
    for (std::size_t k = 1; k <= m; ++k) binom[m][k] = static_cast<u32>((binom[m - 1][k - 1] + (k < m ? binom[m - 1][k] : 0u)) % kMod);
  }
  Vec bs(301, 0u), bb(301, 0u);
  for (u32 n = 0; n <= 300; ++n) {
    u64 s = 0;
    for (u32 k = 0; k <= n; ++k) s = (s + tri.s2[n][k]) % kMod;
    bs[n] = static_cast<u32>(s);
  }
  bb[0] = 1u;
  for (u32 m = 0; m < 300; ++m) {
    u64 s = 0;
    for (u32 k = 0; k <= m; ++k) s = (s + static_cast<u64>(binom[m][k]) * bb[k]) % kMod;
    bb[m + 1] = static_cast<u32>(s);
  }
  CHECK(bs == bb);
  u32 bad = 0, bad_guard = 0, bad_code = 0, bad_range = 0;
  const u32 fills[] = {0u, 0xFFFFFFFFu, kJunk};
  for (u32 n = 0; n <= 300; ++n) {
    for (u32 f = 0; f < 3u; ++f) {
      Vec t;
      bool g = false;
      if (run_bell(n, n + 1u, fills[f], t, g) != StirResult::Ok) ++bad_code;
      if (!g) ++bad_guard;
      if (!all_reduced(t)) ++bad_range;
      for (u32 v = 0; v <= n; ++v) {
        if (t[v] != bs[v]) ++bad;
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(bad_guard == 0u);
  CHECK(bad_code == 0u);
  CHECK(bad_range == 0u);
  Vec t;
  bool g = false;
  REQUIRE(run_bell(300u, 300u + 1u, kJunk, t, g) == StirResult::Ok);
  CHECK(fnv(t) == kBellDigests[0].digest);
  // More scratch than needed is accepted and gives the same table; the extra words are guard-checked as part of the scratch array.
  Vec t2;
  REQUIRE(run_bell(300u, 5000u, 0x12345678u, t2, g) == StirResult::Ok);
  CHECK(g);
  CHECK(t == t2);
}

TEST_CASE("stirling: exhaustive orthogonality of signed first-kind and second-kind matrices, n, m = 0..300", "[stirling][exhaustive]") {
  const Rows& r = impl_rows();
  u32 bad_a = 0, bad_b = 0;
  for (u32 n = 0; n <= 300; ++n) {
    for (u32 m = 0; m <= 300; ++m) {
      // sum_k (-1)^(n-k) c(n, k) S(k, m) and sum_k S(n, k) (-1)^(k-m) c(k, m), both equal to [n = m]
      u64 a = 0, b = 0;
      for (u32 k = m; k <= n; ++k) {
        const u64 pa = mulmod(r.c1[n][k], r.s2[k][m]);
        a = ((n - k) % 2u == 0u) ? (a + pa) % kMod : (a + kMod - pa) % kMod;
        const u64 pb = mulmod(r.s2[n][k], r.c1[k][m]);
        b = ((k - m) % 2u == 0u) ? (b + pb) % kMod : (b + kMod - pb) % kMod;
      }
      const u64 want = (n == m) ? 1u : 0u;
      if (a != want) ++bad_a;
      if (b != want) ++bad_b;
    }
  }
  CHECK(bad_a == 0u);
  CHECK(bad_b == 0u);
}

TEST_CASE("stirling: sampled rows 512..4096 against reference digests, both kinds", "[stirling][sampled]") {
  for (const RowDigest& d : kS2Digests) {
    Vec a;
    bool g = false;
    REQUIRE(run_row(Kind::S2, d.n, a, g) == StirResult::Ok);
    CHECK(g);
    CHECK(fnv(a) == d.digest);
    CHECK(all_reduced(a));
  }
  for (const RowDigest& d : kC1Digests) {
    Vec a;
    bool g = false;
    REQUIRE(run_row(Kind::C1, d.n, a, g) == StirResult::Ok);
    CHECK(g);
    CHECK(fnv(a) == d.digest);
    CHECK(all_reduced(a));
  }
}

TEST_CASE("stirling: sampled Bell tables n = 1024, 2048, 4095, 4096 against reference digests, prefix property", "[stirling][sampled]") {
  Vec full;
  bool g = false;
  REQUIRE(run_bell(kMaxN, kMaxN + 1u, kJunk, full, g) == StirResult::Ok);
  CHECK(g);
  CHECK(full.size() == 4097u);
  CHECK(all_reduced(full));
  for (const RowDigest& d : kBellDigests) {
    Vec t;
    REQUIRE(run_bell(d.n, d.n + 1u, 0xFFFFFFFFu, t, g) == StirResult::Ok);
    CHECK(g);
    CHECK(fnv(t) == d.digest);
    CHECK(fnv(full.data(), static_cast<std::size_t>(d.n) + 1u) == d.digest);  // table n is the prefix of table 4096
  }
  // B(n) equals the sum of the second-kind row n at the sampled rows.
  for (const RowDigest& d : kS2Digests) {
    Vec a;
    REQUIRE(run_row(Kind::S2, d.n, a, g) == StirResult::Ok);
    CHECK(sum_row(a) == full[d.n]);
  }
}

TEST_CASE("stirling: sampled row sums, near-diagonal, falling and rising factorial identities up to n = 4096", "[stirling][sampled]") {
  const u32 ns[] = {301u, 400u, 511u, 512u, 777u, 1000u, 1024u, 1500u, 2047u, 2048u, 2049u, 3000u, 3500u, 4000u, 4095u, 4096u};
  const u64 xs[] = {2u, 3u, 5u, 12345u, 998244352u, 777777777u};
  for (u32 n : ns) {
    Vec a, b;
    bool g1 = false, g2 = false;
    REQUIRE(run_row(Kind::S2, n, a, g1) == StirResult::Ok);
    REQUIRE(run_row(Kind::C1, n, b, g2) == StirResult::Ok);
    CHECK((g1 && g2));
    CHECK(sum_row(b) == factorial(n));
    check_near_diagonal(n, a, b);
    for (u64 x : xs) {
      // sum_k S(n, k) x (x-1) ... (x-k+1) = x^n and sum_k c(n, k) x^k = x (x+1) ... (x+n-1)
      u64 fall = 1, lhs = 0, rhs = 1, pw = 1, lhs1 = 0;
      for (u32 k = 0; k <= n; ++k) {
        lhs = (lhs + mulmod(a[k], fall)) % kMod;
        fall = mulmod(fall, (x + kMod - k % kMod) % kMod);
        lhs1 = (lhs1 + mulmod(b[k], pw)) % kMod;
        pw = mulmod(pw, x);
      }
      for (u32 t = 0; t < n; ++t) rhs = mulmod(rhs, x + t);
      CHECK(lhs == powmod(x, n));
      CHECK(lhs1 == rhs);
    }
  }
}

TEST_CASE("stirling: sampled orthogonality for rows 1024, 2048, 4095, 4096 against m = 0..520", "[stirling][sampled]") {
  const u32 ns[] = {1024u, 2048u, 4095u, 4096u};
  constexpr u32 kM = 520u;
  for (u32 n : ns) {
    Vec c;
    bool g = false;
    REQUIRE(run_row(Kind::C1, n, c, g) == StirResult::Ok);
    // Second-kind column vector col[m] = S(k, m) for m = 0..kM, advanced one k at a time (a two-dimensional recurrence, not the implementation).
    Vec col(kM + 1u, 0u);
    col[0] = 1u;  // k = 0
    std::vector<u64> acc(kM + 1u, 0u);
    for (u32 k = 0; k <= n; ++k) {
      if (k > 0u) {
        for (u32 m = kM; m >= 1u; --m) col[m] = static_cast<u32>((static_cast<u64>(m) * col[m] + col[m - 1u]) % kMod);
        col[0] = 0u;
      }
      const bool plus = ((n - k) % 2u == 0u);
      for (u32 m = 0; m <= kM; ++m) {
        const u64 p = mulmod(c[k], col[m]);
        acc[m] = plus ? (acc[m] + p) % kMod : (acc[m] + kMod - p) % kMod;
      }
    }
    u32 bad = 0;
    for (u32 m = 0; m <= kM; ++m) {
      const u64 want = (m == n) ? 1u : 0u;
      if (acc[m] != want) ++bad;
    }
    CHECK(bad == 0u);
  }
}
