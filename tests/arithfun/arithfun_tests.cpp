// Tests for core/arithfun. Evidence label: unit tests only.
// phi, d, sigma, rad, omega and Omega of one u32 by trial division. Expected values come from the header's examples, from an in-test sieve to 2^22 (phi by the
// prime sieve phi(m) -= phi(m) / p, d and sigma by marking the multiples of every divisor, rad and omega by the prime sieve, Omega by marking the multiples of every
// prime power: no factorisation), from the definitions (phi as the count of k <= n with gcd(k, n) = 1; d and sigma by listing the divisors), from the closed forms for
// prime powers, from relations that hold for every n (listed at `relations`), and from digests made by tools/arithfun_ref.py (standard library only: a smallest-prime-factor
// sieve for the exhaustive range, Miller-Rabin with Pollard rho for the large values). Extents, named in the test names and in docs/arithfun.md:
//   exhaustive vs the sieve and the digests: every n = 1..2^22 (8 chunk cases of 2^19; the last includes 2^22), all five functions;
//   exhaustive vs the definitions: every n = 1..2^16 for phi (gcd count) and d and sigma (divisor listing), in 8 cases of about equal work;
//   structured sets, each vs a digest: all prime powers p^k < 2^32 for every prime p < 2^16, the 4096 largest u32 values, the semiprimes and squares of the 16 largest primes
//     below 2^16 and the 16 smallest above, the multiples j * P_k (j <= 64) of the primorials P_1..P_9, every n < 2^32 with 9 distinct prime factors, and the edge values;
//   sampled: 1000000 values in (2^22, 2^32) from a fixed splitmix64 stream, in 8 shards of 125000, each vs a digest and the relations.
// Every call passes its result cells between guard words. No test asserts anything about time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "arithfun.h"

using lumora::arithfun::AfResult;
using lumora::arithfun::divisor_count;
using lumora::arithfun::divisor_sigma;
using lumora::arithfun::euler_phi;
using lumora::arithfun::kMaxN;
using lumora::arithfun::prime_factor_counts;
using lumora::arithfun::radical;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kLim = 4294967296ull;
constexpr u64 kTop = 4194304ull;  // 2^22
constexpr u64 kChunkDigest[8] = {0xf919b1a034e9702dull, 0x0c9d0dcb5a889dc4ull, 0x54123e1f19855fcfull, 0xa8e4f67f68a36e4aull, 0x703ebc5d625314f1ull, 0x6a41ab91c153907bull, 0xa5d4733cea8832e6ull, 0xa8e54ae8d56cf10cull};
constexpr u64 kSampleDigest[8] = {0x0abc83936bfc051aull, 0x8ae63f7fbbf96154ull, 0xcbe72469617caaf8ull, 0x99b6013c84abfeecull, 0x230c42897378aba0ull, 0x2de39c522273aae0ull, 0x8f70b317ea5d4725ull, 0x04db97a704bb8329ull};
constexpr u64 kSetDigest[6] = {0x0e4b339ed9b569f0ull, 0x477888db0e4d6382ull, 0xd378d3397bfdf6d9ull, 0x8a6d8ea714cbb5f3ull, 0x41a6c776c6d4a42cull, 0x7e07739443fdf454ull};
constexpr std::size_t kSetCount[6] = {13489u, 4096u, 340u, 472u, 4221u, 234u};
// Work-balanced bounds of the 8 definition cases: n in (kDefBound[k], kDefBound[k + 1]] (about 65536 * sqrt(k / 8)).
constexpr u32 kDefBound[9] = {0u, 23170u, 32768u, 40132u, 46341u, 51810u, 56755u, 61305u, 65536u};

constexpr u32 kGuard = 0xDEADBEEFu;
constexpr u64 kGuard64 = 0xDEADBEEFCAFEF00Dull;

// The six values of one n, from the five functions, each result cell between guard words.
struct All {
  bool ok;
  u32 phi;
  u32 d;
  u64 sigma;
  u32 rad;
  u32 omega;
  u32 big;
};

All run_all(u32 n) {
  All a{true, 0u, 0u, 0u, 0u, 0u, 0u};
  u32 c32[3] = {kGuard, 0u, kGuard};
  u64 c64[3] = {kGuard64, 0u, kGuard64};
  u32 e32[4] = {kGuard, 0u, 0u, kGuard};
  if (euler_phi(n, &c32[1]) != AfResult::Ok) a.ok = false;
  a.phi = c32[1];
  c32[1] = 0u;
  if (divisor_count(n, &c32[1]) != AfResult::Ok) a.ok = false;
  a.d = c32[1];
  c32[1] = 0u;
  if (radical(n, &c32[1]) != AfResult::Ok) a.ok = false;
  a.rad = c32[1];
  if (divisor_sigma(n, &c64[1]) != AfResult::Ok) a.ok = false;
  a.sigma = c64[1];
  if (prime_factor_counts(n, &e32[1], &e32[2]) != AfResult::Ok) a.ok = false;
  a.omega = e32[1];
  a.big = e32[2];
  if (c32[0] != kGuard || c32[2] != kGuard || c64[0] != kGuard64 || c64[2] != kGuard64 || e32[0] != kGuard || e32[3] != kGuard) a.ok = false;
  return a;
}

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }
constexpr u64 kFnv0 = 0xcbf29ce484222325ull;

u64 fold_all(u64 h, u32 n, const All& a) {
  h = fold(h, n);
  h = fold(h, a.phi);
  h = fold(h, a.d);
  h = fold(h, a.sigma);
  h = fold(h, a.rad);
  h = fold(h, a.omega);
  return fold(h, a.big);
}

// Relations that hold for every n >= 1 (checked on every large value; not a proof of the values): rad | n; omega <= 9 and omega <= Omega <= 31; 2^omega <= d; phi <= n;
// sigma >= n; phi * sigma <= n^2 (so the product fits u64); sigma / n < 676039 / 110592 (the product of p / (p - 1) over the first nine primes, whose product bound is in
// docs/arithfun.md), which gives sigma < 2^35.
bool relations(u32 n, const All& a) {
  const bool rad_ok = a.rad != 0u && n % a.rad == 0u;
  const bool counts_ok = a.omega <= 9u && a.omega <= a.big && a.big <= 31u && (1u << a.omega) <= a.d;
  const bool size_ok = a.phi >= 1u && a.phi <= n && a.sigma >= n;
  const bool prod_ok = static_cast<u64>(a.phi) * a.sigma <= static_cast<u64>(n) * n;
  const bool ratio_ok = a.sigma * 110592ull < static_cast<u64>(n) * 676039ull + 1ull && a.sigma < (1ull << 35);
  return rad_ok && counts_ok && size_ok && prod_ok && ratio_ok;
}

// The in-test sieve to 2^22 (built once; arrays are allowed in the test, not in the source).
struct Sieve {
  std::vector<u32> phi, d, rad;
  std::vector<u64> sigma;
  std::vector<std::uint8_t> omega, big;
  Sieve()
      : phi(kTop + 1), d(kTop + 1, 0u), rad(kTop + 1, 1u), sigma(kTop + 1, 0u), omega(kTop + 1, 0u), big(kTop + 1, 0u) {
    for (u32 i = 0u; i <= kTop; ++i) phi[i] = i;
    for (u32 v = 1u; v <= kTop; ++v) {
      for (u32 m = v; m <= kTop; m += v) {
        d[m] += 1u;
        sigma[m] += v;
        if (m > kTop - v) break;
      }
    }
    for (u32 p = 2u; p <= kTop; ++p) {
      if (phi[p] != p) continue;  // p is prime exactly when no smaller prime has touched phi[p]
      for (u32 m = p; m <= kTop; m += p) {
        phi[m] -= phi[m] / p;
        rad[m] *= p;
        omega[m] = static_cast<std::uint8_t>(omega[m] + 1u);
        if (m > kTop - p) break;
      }
      for (u64 q = p; q <= kTop; q *= p) {
        for (u64 m = q; m <= kTop; m += q) big[m] = static_cast<std::uint8_t>(big[m] + 1u);
      }
    }
  }
};

const Sieve& sieve() {
  static const Sieve s;
  return s;
}

std::vector<u32> primes_below_65536() {
  std::vector<char> comp(65536, 0);
  std::vector<u32> pr;
  for (u32 i = 2u; i < 65536u; ++i) {
    if (comp[i] != 0) continue;
    pr.push_back(i);
    for (u32 j = i * i; j < 65536u; j += i) comp[j] = 1;
  }
  return pr;
}

bool is_prime_td(u64 n, const std::vector<u32>& pr) {
  if (n < 2u) return false;
  for (u32 p : pr) {
    if (static_cast<u64>(p) * p > n) break;
    if (n % p == 0u) return false;
  }
  return true;
}

void normalise(std::vector<u32>& v) {
  // bottom-up merge sort, then drop the repeats
  std::vector<u32> tmp(v.size());
  for (std::size_t w = 1; w < v.size(); w *= 2) {
    for (std::size_t lo = 0; lo < v.size(); lo += 2 * w) {
      const std::size_t mid = (lo + w < v.size()) ? lo + w : v.size();
      const std::size_t hi = (lo + 2 * w < v.size()) ? lo + 2 * w : v.size();
      std::size_t i = lo, j = mid, k = lo;
      while (i < mid && j < hi) tmp[k++] = (v[j] < v[i]) ? v[j++] : v[i++];
      while (i < mid) tmp[k++] = v[i++];
      while (j < hi) tmp[k++] = v[j++];
    }
    v.swap(tmp);
  }
  std::size_t out = 0;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (out == 0 || v[i] != v[out - 1]) v[out++] = v[i];
  }
  v.resize(out);
}

struct SetCheck {
  bool values_ok;
  bool relations_ok;
  u64 digest;
  std::size_t count;
};

SetCheck check_set(std::vector<u32> v) {
  normalise(v);
  SetCheck r{true, true, kFnv0, v.size()};
  for (u32 n : v) {
    const All a = run_all(n);
    if (!a.ok) r.values_ok = false;
    if (!relations(n, a)) r.relations_ok = false;
    r.digest = fold_all(r.digest, n, a);
  }
  return r;
}

void nine_go(const std::vector<u32>& ps, std::size_t i, u64 n, u32 cnt, std::vector<u32>& out) {
  if (cnt == 9u) {
    out.push_back(static_cast<u32>(n));
    return;
  }
  for (std::size_t j = i; j < ps.size(); ++j) {
    const u32 p = ps[j];
    u64 m = n * p;
    u64 need = m;
    bool enough = true;
    for (u32 t = 1u; t < 9u - cnt; ++t) {
      if (j + t >= ps.size()) {
        enough = false;
        break;
      }
      need *= ps[j + t];
    }
    if (!enough || need >= kLim) break;
    while (m < kLim) {
      nine_go(ps, j + 1, m, cnt + 1u, out);
      m *= p;
    }
  }
}

void check_chunk(unsigned k) {
  const Sieve& s = sieve();
  const u32 lo = (k == 0u) ? 1u : k * (1u << 19);
  const u32 hi = (k + 1u) * (1u << 19) - 1u;
  u64 h = kFnv0;
  u64 bad = 0;
  u32 first_bad = 0;
  bool rel = true;
  auto one = [&](u32 n) {
    const All a = run_all(n);
    const bool same = a.ok && a.phi == s.phi[n] && a.d == s.d[n] && a.sigma == s.sigma[n] && a.rad == s.rad[n] && a.omega == s.omega[n] && a.big == s.big[n];
    if (!same) {
      if (bad == 0u) first_bad = n;
      ++bad;
    }
    if (!relations(n, a)) rel = false;
    h = fold_all(h, n, a);
  };
  for (u32 n = lo; n <= hi; ++n) one(n);
  if (k == 7u) one(static_cast<u32>(kTop));
  CHECK(bad == 0u);
  CHECK(first_bad == 0u);
  CHECK(rel);
  CHECK(h == kChunkDigest[k]);
}

void check_def(unsigned k) {
  u64 bad = 0;
  for (u32 n = kDefBound[k] + 1u; n <= kDefBound[k + 1]; ++n) {
    u32 coprime = 0u;
    for (u32 j = 1u; j <= n; ++j) {
      u32 a = j, b = n;
      while (b != 0u) {
        const u32 t = a % b;
        a = b;
        b = t;
      }
      if (a == 1u) ++coprime;
    }
    u32 dc = 0u;
    u64 ds = 0u;
    for (u32 i = 1u; static_cast<u64>(i) * i <= n; ++i) {
      if (n % i != 0u) continue;
      dc += 1u;
      ds += i;
      if (n / i != i) {
        dc += 1u;
        ds += n / i;
      }
    }
    const All a = run_all(n);
    if (!a.ok || a.phi != coprime || a.d != dc || a.sigma != ds) ++bad;
  }
  CHECK(bad == 0u);
}

void check_sample(unsigned k) {
  u64 state = 0x9E3779B97F4A7C15ull + 0x9E3779B97F4A7C15ull * (static_cast<u64>(k) * 125000ull);
  u64 h = kFnv0;
  bool ok = true;
  bool rel = true;
  for (u32 i = 0u; i < 125000u; ++i) {
    state += 0x9E3779B97F4A7C15ull;
    u64 z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    const u32 n = static_cast<u32>(kTop + 1u + z % (kLim - 1u - kTop));
    const All a = run_all(n);
    if (!a.ok) ok = false;
    if (!relations(n, a)) rel = false;
    h = fold_all(h, n, a);
  }
  CHECK(ok);
  CHECK(rel);
  CHECK(h == kSampleDigest[k]);
}
}  // namespace

TEST_CASE("header examples", "[arithfun]") {
  CHECK(kMaxN == 4294967295u);
  struct P { u32 n; u32 v; };
  const P phis[] = {{1u, 1u}, {2u, 1u}, {6u, 2u}, {12u, 4u}, {36u, 12u}, {60u, 16u}, {97u, 96u}, {100u, 40u}, {360u, 96u}, {1000u, 400u}, {720720u, 138240u}, {2147483648u, 1073741824u}, {4294967295u, 2147483648u}, {4294967291u, 4294967290u}};
  const P ds[] = {{1u, 1u}, {2u, 2u}, {6u, 4u}, {12u, 6u}, {36u, 9u}, {60u, 12u}, {97u, 2u}, {100u, 9u}, {360u, 24u}, {1000u, 16u}, {720720u, 240u}, {2147483648u, 32u}, {3234846615u, 512u}};
  struct Q { u32 n; u64 v; };
  const Q sg[] = {{1u, 1ull}, {2u, 3ull}, {6u, 12ull}, {12u, 28ull}, {36u, 91ull}, {60u, 168ull}, {97u, 98ull}, {100u, 217ull}, {360u, 1170ull}, {1000u, 2340ull}, {720720u, 3249792ull}, {2147483648u, 4294967295ull}, {4294967295u, 7304603328ull}, {3234846615u, 8360755200ull}};
  const P rd[] = {{2u, 2u}, {12u, 6u}, {36u, 6u}, {60u, 30u}, {100u, 10u}, {1000u, 10u}, {720720u, 30030u}, {2147483648u, 2u}, {3486784401u, 3u}, {4294967295u, 4294967295u}};
  struct C { u32 n; u32 w; u32 o; };
  const C cs[] = {{1u, 0u, 0u}, {2u, 1u, 1u}, {12u, 2u, 3u}, {36u, 2u, 4u}, {60u, 3u, 4u}, {360u, 3u, 6u}, {1000u, 2u, 6u}, {720720u, 6u, 10u}, {2147483648u, 1u, 31u}, {3486784401u, 1u, 20u}, {4294967295u, 5u, 5u}, {3234846615u, 9u, 9u}};
  bool ok = true;
  for (const P& e : phis) { u32 v = 0u; ok = ok && euler_phi(e.n, &v) == AfResult::Ok && v == e.v; }
  CHECK(ok);
  ok = true;
  for (const P& e : ds) { u32 v = 0u; ok = ok && divisor_count(e.n, &v) == AfResult::Ok && v == e.v; }
  CHECK(ok);
  ok = true;
  for (const Q& e : sg) { u64 v = 0u; ok = ok && divisor_sigma(e.n, &v) == AfResult::Ok && v == e.v; }
  CHECK(ok);
  ok = true;
  for (const P& e : rd) { u32 v = 0u; ok = ok && radical(e.n, &v) == AfResult::Ok && v == e.v; }
  CHECK(ok);
  ok = true;
  for (const C& e : cs) { u32 w = 99u, o = 99u; ok = ok && prime_factor_counts(e.n, &w, &o) == AfResult::Ok && w == e.w && o == e.o; }
  CHECK(ok);
}

TEST_CASE("failure cases: null first, then n = 0, nothing written", "[arithfun]") {
  const u32 ns[] = {0u, 1u, 2u, 4294967295u};
  bool ok = true;
  for (u32 n : ns) {
    ok = ok && euler_phi(n, nullptr) == AfResult::NullArgument;
    ok = ok && divisor_count(n, nullptr) == AfResult::NullArgument;
    ok = ok && divisor_sigma(n, nullptr) == AfResult::NullArgument;
    ok = ok && radical(n, nullptr) == AfResult::NullArgument;
    u32 a = 111u, b = 222u;
    ok = ok && prime_factor_counts(n, nullptr, nullptr) == AfResult::NullArgument;
    ok = ok && prime_factor_counts(n, &a, nullptr) == AfResult::NullArgument;
    ok = ok && prime_factor_counts(n, nullptr, &b) == AfResult::NullArgument;
    ok = ok && a == 111u && b == 222u;
  }
  CHECK(ok);
  // n = 0 with valid cells: BadArgument, every cell keeps its sentinel and its guards
  u32 c32[3] = {kGuard, 0x5A5A5A5Au, kGuard};
  u64 c64[3] = {kGuard64, 0x5A5A5A5A5A5A5A5Aull, kGuard64};
  u32 e32[4] = {kGuard, 0x11111111u, 0x22222222u, kGuard};
  CHECK(euler_phi(0u, &c32[1]) == AfResult::BadArgument);
  CHECK(divisor_count(0u, &c32[1]) == AfResult::BadArgument);
  CHECK(radical(0u, &c32[1]) == AfResult::BadArgument);
  CHECK(divisor_sigma(0u, &c64[1]) == AfResult::BadArgument);
  CHECK(prime_factor_counts(0u, &e32[1], &e32[2]) == AfResult::BadArgument);
  const bool intact = c32[0] == kGuard && c32[1] == 0x5A5A5A5Au && c32[2] == kGuard && c64[0] == kGuard64 && c64[1] == 0x5A5A5A5A5A5A5A5Aull && c64[2] == kGuard64 &&
                      e32[0] == kGuard && e32[1] == 0x11111111u && e32[2] == 0x22222222u && e32[3] == kGuard;
  CHECK(intact);
  // a failed null check writes nothing through the other pointer (both-null and one-null cases above), and a success writes only its own cells
  u32 w = 7u, o = 8u;
  CHECK(prime_factor_counts(1u, &w, &o) == AfResult::Ok);
  CHECK(w == 0u);
  CHECK(o == 0u);
}

TEST_CASE("n = 1 and the smallest values", "[arithfun]") {
  const All one = run_all(1u);
  CHECK(one.ok);
  CHECK(one.phi == 1u);
  CHECK(one.d == 1u);
  CHECK(one.sigma == 1u);
  CHECK(one.rad == 1u);
  CHECK(one.omega == 0u);
  CHECK(one.big == 0u);
  const All two = run_all(2u);
  CHECK(two.phi == 1u);
  CHECK(two.d == 2u);
  CHECK(two.sigma == 3u);
  CHECK(two.rad == 2u);
  CHECK(two.omega == 1u);
  CHECK(two.big == 1u);
  const All four = run_all(4u);
  CHECK(four.phi == 2u);
  CHECK(four.d == 3u);
  CHECK(four.sigma == 7u);
  CHECK(four.rad == 2u);
  CHECK(four.big == 2u);
  const All nine = run_all(9u);
  CHECK(nine.phi == 6u);
  CHECK(nine.sigma == 13u);
  CHECK(nine.rad == 3u);
}

TEST_CASE("maxima over the u32 range: the values at the maximisers", "[arithfun]") {
  // sigma max 22318571520 at 4227022800 and d max 1920 at 3491888400 are measured (full-range local check, docs/arithfun.md); here the functions return them.
  const All s = run_all(4227022800u);
  CHECK(s.ok);
  CHECK(s.sigma == 22318571520ull);
  const All d = run_all(3491888400u);
  CHECK(d.ok);
  CHECK(d.d == 1920u);
  const All w = run_all(3234846615u);
  CHECK(w.omega == 9u);
  const All o = run_all(2147483648u);
  CHECK(o.big == 31u);
  const bool bounds = s.sigma < (1ull << 35) && 22318571520ull * 110592ull < 4227022800ull * 676039ull;
  CHECK(bounds);
}

TEST_CASE("chunk 0 vs sieve and digest", "[arithfun][chunk]") { check_chunk(0u); }
TEST_CASE("chunk 1 vs sieve and digest", "[arithfun][chunk]") { check_chunk(1u); }
TEST_CASE("chunk 2 vs sieve and digest", "[arithfun][chunk]") { check_chunk(2u); }
TEST_CASE("chunk 3 vs sieve and digest", "[arithfun][chunk]") { check_chunk(3u); }
TEST_CASE("chunk 4 vs sieve and digest", "[arithfun][chunk]") { check_chunk(4u); }
TEST_CASE("chunk 5 vs sieve and digest", "[arithfun][chunk]") { check_chunk(5u); }
TEST_CASE("chunk 6 vs sieve and digest", "[arithfun][chunk]") { check_chunk(6u); }
TEST_CASE("chunk 7 vs sieve and digest", "[arithfun][chunk]") { check_chunk(7u); }

TEST_CASE("definitions 0 (n up to 23170)", "[arithfun][def]") { check_def(0u); }
TEST_CASE("definitions 1 (n up to 32768)", "[arithfun][def]") { check_def(1u); }
TEST_CASE("definitions 2 (n up to 40132)", "[arithfun][def]") { check_def(2u); }
TEST_CASE("definitions 3 (n up to 46341)", "[arithfun][def]") { check_def(3u); }
TEST_CASE("definitions 4 (n up to 51810)", "[arithfun][def]") { check_def(4u); }
TEST_CASE("definitions 5 (n up to 56755)", "[arithfun][def]") { check_def(5u); }
TEST_CASE("definitions 6 (n up to 61305)", "[arithfun][def]") { check_def(6u); }
TEST_CASE("definitions 7 (n up to 65536)", "[arithfun][def]") { check_def(7u); }

TEST_CASE("set: prime powers p^k below 2^32 for every prime p below 2^16", "[arithfun][set]") {
  const std::vector<u32> pr = primes_below_65536();
  std::vector<u32> v;
  bool formula = true;
  for (u32 p : pr) {
    u64 q = p;
    u32 k = 1u;
    u64 lower = 1u;  // p^(k - 1)
    u64 sg = 1u + p;
    while (q < kLim) {
      v.push_back(static_cast<u32>(q));
      const All a = run_all(static_cast<u32>(q));
      const bool fits = a.ok && a.phi == lower * (p - 1u) && a.d == k + 1u && a.sigma == sg && a.rad == p && a.omega == 1u && a.big == k;
      if (!fits) formula = false;
      lower = q;
      q *= p;
      sg += q;
      ++k;
    }
  }
  CHECK(formula);
  const SetCheck r = check_set(v);
  CHECK(r.values_ok);
  CHECK(r.relations_ok);
  CHECK(r.count == kSetCount[0]);
  CHECK(r.digest == kSetDigest[0]);
}

TEST_CASE("set: the 4096 largest u32 values (includes the 100 largest primes)", "[arithfun][set]") {
  std::vector<u32> v;
  for (u64 n = kLim - 4096u; n < kLim; ++n) v.push_back(static_cast<u32>(n));
  const SetCheck r = check_set(v);
  CHECK(r.values_ok);
  CHECK(r.relations_ok);
  CHECK(r.count == kSetCount[1]);
  CHECK(r.digest == kSetDigest[1]);
  // at least the 100 largest primes below 2^32 lie in the window: count them with trial division and check each has phi = n - 1, d = 2, sigma = n + 1, rad = n
  const std::vector<u32> pr = primes_below_65536();
  u32 primes_seen = 0u;
  bool prime_ok = true;
  for (u64 n = kLim - 1u; n >= kLim - 4096u; --n) {
    if (!is_prime_td(n, pr)) continue;
    ++primes_seen;
    const All a = run_all(static_cast<u32>(n));
    if (!(a.ok && a.phi == n - 1u && a.d == 2u && a.sigma == n + 1u && a.rad == n && a.omega == 1u && a.big == 1u)) prime_ok = false;
  }
  CHECK(prime_ok);
  CHECK(primes_seen >= 100u);
}

TEST_CASE("set: semiprimes and squares near 2^16", "[arithfun][set]") {
  const std::vector<u32> pr = primes_below_65536();
  std::vector<u32> both;
  for (std::size_t i = pr.size() - 16u; i < pr.size(); ++i) both.push_back(pr[i]);
  for (u32 q = 65537u; both.size() < 32u; q += 2u) {
    if (is_prime_td(q, pr)) both.push_back(q);
  }
  std::vector<u32> v;
  bool formula = true;
  for (std::size_t i = 0; i < both.size(); ++i) {
    for (std::size_t j = i; j < both.size(); ++j) {
      const u64 n = static_cast<u64>(both[i]) * both[j];
      if (n >= kLim) continue;
      v.push_back(static_cast<u32>(n));
      const All a = run_all(static_cast<u32>(n));
      const u64 p = both[i], q = both[j];
      bool fits = false;
      if (p == q) fits = a.ok && a.phi == p * (p - 1u) && a.d == 3u && a.sigma == 1u + p + p * p && a.rad == p && a.omega == 1u && a.big == 2u;
      else fits = a.ok && a.phi == (p - 1u) * (q - 1u) && a.d == 4u && a.sigma == (p + 1u) * (q + 1u) && a.rad == n && a.omega == 2u && a.big == 2u;
      if (!fits) formula = false;
    }
  }
  CHECK(formula);
  const SetCheck r = check_set(v);
  CHECK(r.values_ok);
  CHECK(r.relations_ok);
  CHECK(r.count == kSetCount[2]);
  CHECK(r.digest == kSetDigest[2]);
}

TEST_CASE("set: multiples j * P_k (j <= 64) of the primorials P_1 to P_9", "[arithfun][set]") {
  const u32 first[9] = {2u, 3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u};
  std::vector<u32> v;
  u64 pk = 1u;
  for (u32 p : first) {
    pk *= p;
    for (u64 j = 1u; j <= 64u; ++j) {
      if (pk * j < kLim) v.push_back(static_cast<u32>(pk * j));
    }
  }
  const SetCheck r = check_set(v);
  CHECK(r.values_ok);
  CHECK(r.relations_ok);
  CHECK(r.count == kSetCount[3]);
  CHECK(r.digest == kSetDigest[3]);
}

TEST_CASE("set: every n below 2^32 with 9 distinct prime factors", "[arithfun][set]") {
  std::vector<u32> pr = primes_below_65536();
  std::vector<u32> ps;
  for (u32 p : pr) {
    if (p < 450u) ps.push_back(p);
  }
  std::vector<u32> v;
  nine_go(ps, 0u, 1u, 0u, v);
  const SetCheck r = check_set(v);
  CHECK(r.values_ok);
  CHECK(r.relations_ok);
  CHECK(r.count == kSetCount[4]);
  CHECK(r.digest == kSetDigest[4]);
  bool nine = true;
  for (u32 n : v) {
    const All a = run_all(n);
    if (a.omega != 9u) nine = false;
  }
  CHECK(nine);
}

TEST_CASE("set: edge values", "[arithfun][set]") {
  std::vector<u32> v;
  for (u32 k = 0u; k < 32u; ++k) v.push_back(1u << k);
  for (u32 k = 1u; k <= 32u; ++k) {
    v.push_back(static_cast<u32>((1ull << k) - 1u));
    if (k < 32u) v.push_back((1u << k) + 1u);
  }
  u64 pw = 1u;
  for (u32 k = 0u; k < 10u; ++k) {
    v.push_back(static_cast<u32>(pw));
    pw *= 10u;
  }
  for (u32 n = 2147483648u - 64u; n <= 2147483648u + 64u; ++n) v.push_back(n);
  const u32 extra[] = {3491888400u, 4227022800u, 4294967295u, 4294967294u, 4294967291u, 3234846615u, 4190266080u};
  for (u32 n : extra) v.push_back(n);
  const SetCheck r = check_set(v);
  CHECK(r.values_ok);
  CHECK(r.relations_ok);
  CHECK(r.count == kSetCount[5]);
  CHECK(r.digest == kSetDigest[5]);
}

TEST_CASE("sample 0 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(0u); }
TEST_CASE("sample 1 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(1u); }
TEST_CASE("sample 2 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(2u); }
TEST_CASE("sample 3 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(3u); }
TEST_CASE("sample 4 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(4u); }
TEST_CASE("sample 5 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(5u); }
TEST_CASE("sample 6 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(6u); }
TEST_CASE("sample 7 (125000 values in (2^22, 2^32))", "[arithfun][sample]") { check_sample(7u); }
