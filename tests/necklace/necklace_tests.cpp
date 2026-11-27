// Tests for core/necklace. Evidence label: unit tests only.
// Expected values come from tools/necklace_ref.py (standard library only: brute force enumeration of all strings, exact big-integer Burnside with checked
// division, and a modular third source from gcd counting) as FNV-1a digests over 32-bit words and explicit values, and from in-test checks of a different
// shape (the gcd form of Burnside, the Gauss identities for Lyndon words, and the dihedral average built from explicit permutations). Extents, named in the
// test names and in docs/necklace.md:
//   exhaustive: enumeration extent, every (n, k) with n 0..24, k 0..1100 and k^n <= 30000 (2484 pairs), all three functions, against the digest of the
//               brute-force counts, plus seven named heavier pairs against explicit enumeration counts;
//               exact extent, every n 0..2000 times the 50 values of kKs (k 0..40 and nine large values), all three functions, against the digest of the
//               exact big-integer values;
//               in-test gcd form for necklaces and the Gauss identities for every n 1..2000 times kKs; the dihedral average from explicit permutations
//               for every n 1..400 times kKs;
//   sampled:    the 16 values of kLargeN (up to 2^20, primes, highly composite) times the 20 values of kLargeK, against digests from the reference (exact
//               big integers for k <= 16, the modular third source for larger k, which the reference checks against each other where both exist), and the
//               gcd form and Gauss identities at those n.
// Every success writes through a guarded cell; no test measures time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "necklace.h"

using lumora::necklace::bracelet_count;
using lumora::necklace::kMaxN;
using lumora::necklace::kMod;
using lumora::necklace::lyndon_count;
using lumora::necklace::necklace_count;
using lumora::necklace::NeckResult;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr std::uint64_t kDigestEnum = 0x66ddc434afee2422ull;  // 2484 pairs, n 0..24, k 0..1100, k^n <= 30000 (n = 0 for every k)
struct Heavy { std::uint32_t n, k, necklace, bracelet, lyndon; };
constexpr Heavy kHeavy[] = {
    {10u, 3u, 5934u, 3210u, 5880u},
    {12u, 3u, 44368u, 22913u, 44220u},
    {16u, 2u, 4116u, 2250u, 4080u},
    {18u, 2u, 14602u, 7685u, 14532u},
    {8u, 4u, 8230u, 4435u, 8160u},
    {7u, 5u, 11165u, 5895u, 11160u},
    {6u, 8u, 43800u, 23052u, 43596u},
};
constexpr std::uint64_t kDigestExact = 0x97aaa612c22b9888ull;  // n 0..2000 times the 50 values of kKs
constexpr std::uint64_t kDigestLarge = 0x2e2d3d5c14549b49ull;  // the 16 values of kLargeN times the 20 values of kLargeK
// per-n digests of the large group (all 20 k values, triples in order)
constexpr std::uint64_t kDigestLargeByN[] = {
    0xc4abd8675809d440ull,  // n = 1024
    0x7c41a3ae24bb1db9ull,  // n = 4096
    0xc70c25ad819a63e2ull,  // n = 65536
    0x37996dda3b80ce40ull,  // n = 131072
    0x16a7f6217aa19e1dull,  // n = 262144
    0xfa0a48aba6643fb9ull,  // n = 524288
    0x326074265d6e93ddull,  // n = 524289
    0x409d42fa9e404ccbull,  // n = 786432
    0x7afa7251f907d249ull,  // n = 1000000
    0x79906d6c5faa7cfbull,  // n = 720720
    0x1baabc54490e122aull,  // n = 997920
    0xca6cc5d9de1f2a9full,  // n = 65537
    0xb8ae4e36e677b147ull,  // n = 7919
    0x57961553facba283ull,  // n = 1048573
    0x490650899e42ed49ull,  // n = 1048575
    0xf73e425a9f64aab7ull,  // n = 1048576
};

constexpr u32 kKs[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u,
                       25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u,
                       255u, 256u, 998244351u, 998244352u, 998244353u, 998244354u, 1996488706u, 2147483648u, 4294967295u};
constexpr u32 kLargeN[] = {1024u, 4096u, 65536u, 131072u, 262144u, 524288u, 524289u, 786432u, 1000000u, 720720u, 997920u, 65537u, 7919u, 1048573u,
                           1048575u, 1048576u};
constexpr u32 kLargeK[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 10u, 11u, 12u, 16u, 255u, 256u, 998244352u, 998244353u, 998244354u, 1996488706u, 2147483648u,
                           4294967295u};
constexpr u32 kPeerK[] = {0u, 1u, 2u, 998244352u, 998244353u, 998244354u, 1996488706u, 4294967295u};

u64 fnv(u64 h, u32 w) { return (h ^ static_cast<u64>(w)) * 0x100000001b3ull; }
constexpr u64 kFnvInit = 0xcbf29ce484222325ull;

struct Triple {
  u32 nec, bra, lyn;
  bool ok;
};
Triple three(u32 n, u32 k) {
  Triple t{0u, 0u, 0u, true};
  t.ok = (necklace_count(n, k, &t.nec) == NeckResult::Ok) && (bracelet_count(n, k, &t.bra) == NeckResult::Ok) && (lyndon_count(n, k, &t.lyn) == NeckResult::Ok);
  if (t.nec >= kMod || t.bra >= kMod || t.lyn >= kMod) t.ok = false;
  return t;
}
u64 add(u64 a, u64 b) { return (a + b) % kMod; }
u64 sub(u64 a, u64 b) { return (a + kMod - b) % kMod; }
u64 mul(u64 a, u64 b) { return (a % kMod) * (b % kMod) % kMod; }
u64 power(u64 b, u64 e) {
  u64 r = 1;
  b %= kMod;
  while (e != 0) {
    if ((e & 1u) != 0u) r = r * b % kMod;
    b = b * b % kMod;
    e >>= 1;
  }
  return r;
}
u64 inverse(u64 a) { return power(a, kMod - 2u); }
u32 gcd32(u32 a, u32 b) {
  while (b != 0u) {
    const u32 t = a % b;
    a = b;
    b = t;
  }
  return a;
}
std::uint64_t kpow(u32 k, u64 e) { return power(k, e); }

// (g, number of j in 0 .. n-1 with gcd(j, n) = g), counted by explicit gcd of every j (a different route from the totient).
struct GcdCount {
  u32 g;
  u64 count;
};
std::vector<GcdCount> gcd_counts(u32 n) {
  std::vector<GcdCount> v;
  std::vector<u32> seen(n + 1u, 0u);
  for (u32 j = 0; j < n; ++j) ++seen[gcd32(j, n)];  // gcd(0, n) = n
  for (u32 g = 1; g <= n; ++g) {
    if (seen[g] != 0u) v.push_back({g, seen[g]});
  }
  return v;
}
std::vector<u32> divisors_of(u32 n) {
  std::vector<u32> d;
  for (u64 i = 1; i * i <= n; ++i) {
    if (n % i == 0u) {
      d.push_back(static_cast<u32>(i));
      if (i != n / i) d.push_back(static_cast<u32>(n / i));
    }
  }
  return d;
}
u64 necklace_gcd_form(const std::vector<GcdCount>& gc, u32 n, u32 k) {
  u64 s = 0;
  for (const GcdCount& c : gc) s = add(s, mul(c.count, kpow(k, c.g)));
  return mul(s, inverse(n));
}
}  // namespace

TEST_CASE("necklace: failure cases, check order, nothing written", "[necklace]") {
  const u32 ns[] = {0u, 5u, kMaxN, kMaxN + 1u, 0xFFFFFFFFu};
  for (u32 n : ns) {
    CHECK(necklace_count(n, 2u, nullptr) == NeckResult::NullArgument);  // null before size
    CHECK(bracelet_count(n, 2u, nullptr) == NeckResult::NullArgument);
    CHECK(lyndon_count(n, 2u, nullptr) == NeckResult::NullArgument);
  }
  for (u32 k : kKs) {
    const u32 bad_ns[] = {kMaxN + 1u, kMaxN + 2u, 0x80000000u, 0xFFFFFFFFu};
    for (u32 n : bad_ns) {
      u32 a = 0xA5A5A5A5u, b = 0xA5A5A5A5u, c = 0xA5A5A5A5u;
      CHECK(necklace_count(n, k, &a) == NeckResult::TooLarge);
      CHECK(bracelet_count(n, k, &b) == NeckResult::TooLarge);
      CHECK(lyndon_count(n, k, &c) == NeckResult::TooLarge);
      CHECK((a == 0xA5A5A5A5u && b == 0xA5A5A5A5u && c == 0xA5A5A5A5u));
    }
  }
  // Success writes exactly one word and nothing beside it.
  u32 cells[5] = {0xA5A5A5A5u, 0xA5A5A5A5u, 0xA5A5A5A5u, 0xA5A5A5A5u, 0xA5A5A5A5u};
  REQUIRE(necklace_count(kMaxN, 2u, &cells[2]) == NeckResult::Ok);
  bool guards = (cells[0] == 0xA5A5A5A5u) && (cells[1] == 0xA5A5A5A5u) && (cells[3] == 0xA5A5A5A5u) && (cells[4] == 0xA5A5A5A5u);
  CHECK(guards);
  CHECK(cells[2] != 0xA5A5A5A5u);
  cells[2] = 0xA5A5A5A5u;
  REQUIRE(bracelet_count(kMaxN, 2u, &cells[2]) == NeckResult::Ok);
  REQUIRE(lyndon_count(kMaxN, 2u, &cells[2]) == NeckResult::Ok);
  guards = (cells[0] == 0xA5A5A5A5u) && (cells[1] == 0xA5A5A5A5u) && (cells[3] == 0xA5A5A5A5u) && (cells[4] == 0xA5A5A5A5u);
  CHECK(guards);
}

TEST_CASE("necklace: edge cases n = 0, k = 0, k = 1, k as a residue, bracelet n = 1 and n = 2", "[necklace]") {
  for (u32 k : kKs) {
    const Triple t = three(0u, k);
    CHECK(t.ok);
    CHECK((t.nec == 1u && t.bra == 1u && t.lyn == 0u));  // n = 0, for every k
  }
  for (u32 n : {1u, 2u, 3u, 4u, 5u, 6u, 12u, 97u, 1024u, 720720u, kMaxN}) {
    const Triple z = three(n, 0u);
    CHECK(z.ok);
    CHECK((z.nec == 0u && z.bra == 0u && z.lyn == 0u));  // k = 0, n > 0
    const Triple o = three(n, 1u);
    CHECK(o.ok);
    CHECK((o.nec == 1u && o.bra == 1u));
    CHECK(o.lyn == ((n == 1u) ? 1u : 0u));
    // k = p, 2p, p + 1 behave as residues 0, 0, 1
    const Triple p0 = three(n, kMod);
    const Triple p2 = three(n, 2u * kMod);
    const Triple p1 = three(n, kMod + 1u);
    CHECK((p0.nec == z.nec && p0.bra == z.bra && p0.lyn == z.lyn));
    CHECK((p2.nec == z.nec && p2.bra == z.bra && p2.lyn == z.lyn));
    CHECK((p1.nec == o.nec && p1.bra == o.bra && p1.lyn == o.lyn));
  }
  const u64 inv2 = inverse(2u);
  for (u32 k : kKs) {
    const Triple a = three(1u, k);
    const Triple b = three(2u, k);
    const u64 kr = static_cast<u64>(k) % kMod;
    CHECK((a.nec == kr && a.bra == kr && a.lyn == kr));  // n = 1: k of everything
    CHECK(b.bra == mul(mul(kr, add(kr, 1u)), inv2));      // n = 2: k (k + 1) / 2 bracelets
    CHECK(b.nec == mul(mul(kr, add(kr, 1u)), inv2));      // and as many necklaces
    CHECK(b.lyn == mul(mul(kr, sub(kr, 1u)), inv2));      // k (k - 1) / 2 Lyndon words
  }
  // Textbook values.
  const struct {
    u32 n, k, nec, bra, lyn;
  } known[] = {{6, 2, 14, 13, 9}, {4, 3, 24, 21, 18}, {5, 2, 8, 8, 6}, {3, 2, 4, 4, 2}, {4, 2, 6, 6, 3}, {8, 2, 36, 30, 30}, {10, 2, 108, 78, 99}};
  for (const auto& c : known) {
    const Triple t = three(c.n, c.k);
    CHECK(t.ok);
    CHECK((t.nec == c.nec && t.bra == c.bra && t.lyn == c.lyn));
  }
}

TEST_CASE("necklace: enumeration extent every (n, k) with n 0 to 24, k 0 to 1100 and k^n up to 30000, 2484 pairs", "[necklace][exhaustive]") {
  u64 h = kFnvInit;
  u32 pairs = 0;
  bool all_ok = true;
  for (u32 n = 0; n <= 24u; ++n) {
    for (u32 k = 0; k <= 1100u; ++k) {
      bool in = (n == 0u);
      if (!in) {
        u64 p = 1;
        in = true;
        for (u32 i = 0; i < n && in; ++i) {
          p *= k;
          if (p > 30000u) in = false;
        }
      }
      if (!in) continue;
      const Triple t = three(n, k);
      all_ok = all_ok && t.ok;
      h = fnv(fnv(fnv(h, t.nec), t.bra), t.lyn);
      ++pairs;
    }
  }
  CHECK(all_ok);
  CHECK(pairs == 2484u);
  CHECK(h == kDigestEnum);
  for (const Heavy& e : kHeavy) {
    const Triple t = three(e.n, e.k);
    CHECK(t.ok);
    CHECK((t.nec == e.necklace && t.bra == e.bracelet && t.lyn == e.lyndon));
  }
}

TEST_CASE("necklace: exact extent every n 0 to 2000 times 50 values of k against exact big-integer digest", "[necklace][exhaustive]") {
  u64 h = kFnvInit;
  bool all_ok = true;
  for (u32 n = 0; n <= 2000u; ++n) {
    for (u32 k : kKs) {
      const Triple t = three(n, k);
      all_ok = all_ok && t.ok;
      h = fnv(fnv(fnv(h, t.nec), t.bra), t.lyn);
    }
  }
  CHECK(all_ok);
  CHECK(h == kDigestExact);
}

TEST_CASE("necklace: gcd form and Gauss identities for every n 1 to 2000 times 50 values of k", "[necklace][exhaustive]") {
  u32 bad_gcd = 0, bad_sumL = 0, bad_gauss = 0;
  for (u32 n = 1; n <= 2000u; ++n) {
    const std::vector<GcdCount> gc = gcd_counts(n);
    const std::vector<u32> ds = divisors_of(n);
    for (u32 k : kKs) {
      const Triple t = three(n, k);
      if (t.nec != necklace_gcd_form(gc, n, k)) ++bad_gcd;
      u64 sum_l = 0, gauss = 0;
      for (u32 d : ds) {
        u32 l = 0;
        (void)lyndon_count(d, k, &l);
        sum_l = add(sum_l, l);
        gauss = add(gauss, mul(d, l));
      }
      if (sum_l != t.nec) ++bad_sumL;                // sum over d | n of L(d) = N(n)
      if (gauss != kpow(k, n)) ++bad_gauss;          // sum over d | n of d L(d) = k^n
    }
  }
  CHECK(bad_gcd == 0u);
  CHECK(bad_sumL == 0u);
  CHECK(bad_gauss == 0u);
  // The same sum-of-Lyndon identity on the peer's list of residues for every n 1 to 200, stated separately.
  u32 bad = 0;
  for (u32 n = 1; n <= 200u; ++n) {
    const std::vector<u32> ds = divisors_of(n);
    for (u32 k : kPeerK) {
      u64 s = 0;
      for (u32 d : ds) {
        u32 l = 0;
        (void)lyndon_count(d, k, &l);
        s = add(s, l);
      }
      u32 nec = 0;
      (void)necklace_count(n, k, &nec);
      if (s != nec) ++bad;
    }
  }
  CHECK(bad == 0u);
}

TEST_CASE("necklace: bracelets equal the dihedral average from explicit permutations for every n 1 to 400 times 50 values of k", "[necklace][exhaustive]") {
  u32 bad = 0;
  for (u32 n = 1; n <= 400u; ++n) {
    // rotations: fixed strings k^(cycles) with cycles = gcd(j, n); reflections s: i -> (s - i) mod n, cycles counted on the permutation itself.
    std::vector<u32> rot_cycles(n), refl_cycles(n);
    for (u32 j = 0; j < n; ++j) rot_cycles[j] = gcd32(j, n);
    for (u32 s = 0; s < n; ++s) {
      std::vector<char> seen(n, 0);
      u32 cyc = 0;
      for (u32 i = 0; i < n; ++i) {
        if (seen[i] != 0) continue;
        ++cyc;
        u32 x = i;
        while (seen[x] == 0) {
          seen[x] = 1;
          x = static_cast<u32>((static_cast<u64>(s) + n - x) % n);
        }
      }
      refl_cycles[s] = cyc;
    }
    for (u32 k : kKs) {
      u64 total = 0;
      for (u32 j = 0; j < n; ++j) total = add(total, kpow(k, rot_cycles[j]));
      for (u32 s = 0; s < n; ++s) total = add(total, kpow(k, refl_cycles[s]));
      const u64 want = mul(total, inverse(2u * static_cast<u64>(n)));
      u32 got = 0;
      (void)bracelet_count(n, k, &got);
      if (got != want) ++bad;
    }
  }
  CHECK(bad == 0u);
}

TEST_CASE("necklace: sampled large n 1024 to 2^20 (primes, highly composite) times 20 values of k against reference digests", "[necklace][sampled]") {
  u64 all = kFnvInit;
  std::size_t idx = 0;
  for (u32 n : kLargeN) {
    u64 h = kFnvInit;
    for (u32 k : kLargeK) {
      const Triple t = three(n, k);
      CHECK(t.ok);
      h = fnv(fnv(fnv(h, t.nec), t.bra), t.lyn);
      all = fnv(fnv(fnv(all, t.nec), t.bra), t.lyn);
    }
    CHECK(h == kDigestLargeByN[idx]);
    ++idx;
  }
  CHECK(all == kDigestLarge);
}

TEST_CASE("necklace: sampled gcd form and Gauss identities at the large n", "[necklace][sampled]") {
  for (u32 n : kLargeN) {
    const std::vector<GcdCount> gc = gcd_counts(n);
    const std::vector<u32> ds = divisors_of(n);
    u32 bad = 0;
    for (u32 k : kLargeK) {
      u32 nec = 0;
      (void)necklace_count(n, k, &nec);
      if (nec != necklace_gcd_form(gc, n, k)) ++bad;
      u64 sum_l = 0, gauss = 0;
      for (u32 d : ds) {
        u32 l = 0;
        (void)lyndon_count(d, k, &l);
        sum_l = add(sum_l, l);
        gauss = add(gauss, mul(d, l));
      }
      if (sum_l != nec) ++bad;
      if (gauss != kpow(k, n)) ++bad;
    }
    CHECK(bad == 0u);
  }
  // n = 2^20 with k = 1 and k = 0 explicitly (the largest length).
  const Triple one = three(kMaxN, 1u);
  CHECK((one.nec == 1u && one.bra == 1u && one.lyn == 0u));
  const Triple two = three(kMaxN, 2u);
  CHECK(two.ok);
}
