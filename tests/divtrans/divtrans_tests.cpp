// Tests for core/divtrans. Evidence label: unit tests only.
// Expected results come from an in-test scan brute force (the definition: for every m, every d in 1..m with m % d == 0; mu by trial division; not the
// loops of the implementation), from in-test second constructions of a different shape (a per-prime pass for the transforms, the divisors of every m from
// its smallest-prime-factor factorisation for the convolution), from closed forms on the full array, and from FNV-1a digests over 32-bit words made by
// tools/divtrans_ref.py (standard library only; its scan oracle is THE oracle on the small extents; the large digests come from its O(n log n) pair
// oracle, because the scan is infeasible at n = 2^20; the pair oracle is checked against the scan on the small extents and n = 1..300).
// Extents, named in the test names and in docs/divtrans.md:
//   exhaustive vs the in-test scan (transforms, with both round trips): every array over {0,1,p-1} of length 0..12 (797161) and over {0,1,2,p-1} of length 0..8
//               (87381); convolution pairs over {0,1,p-1} of length 0..6 (597871) and over {0,1,2} of length 0..7 (5380840), each also with a and b the same array;
//   exhaustive vs reference digests: transforms over {0,1,p-1} length 0..8 and {0,1,2,p-1} length 0..6, convolution pairs over {0,1,p-1} length 0..4 and
//               {0,1,2} length 0..5, each as one digest in the stated enumeration order (last position fastest);
//   sampled: random arrays for every n = 1..2000 (all n against the second constructions and the digest; n up to 300 also against the scan); large n =
//               65536, 65537, 524289, 720720, 1048573 (prime), 1048575, 1048576 with 4 kinds of inputs each against reference digests, with the second
//               constructions on random arrays, with both round trips, and with closed forms on the full array.
// Every run goes through guard words before and after each array, a garbage fill of the output, and a byte comparison of the inputs afterwards.
// No test asserts anything about time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "divtrans.h"

using lumora::divtrans::divisor_convolve;
using lumora::divtrans::divisor_mobius;
using lumora::divtrans::divisor_zeta;
using lumora::divtrans::DivResult;
using lumora::divtrans::kMaxN;
using lumora::divtrans::kMod;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr std::uint64_t kDigest_zeta_01m_0_8 = 0x2b3f1f424b9ee04aull;  // 9841 arrays
constexpr std::uint64_t kDigest_mobius_01m_0_8 = 0xbfe212bddb11709dull;  // 9841 arrays
constexpr std::uint64_t kDigest_zeta_012m_0_6 = 0xab90496efb8d0f3full;  // 5461 arrays
constexpr std::uint64_t kDigest_mobius_012m_0_6 = 0x11fa9f4780e2a154ull;  // 5461 arrays
constexpr std::uint64_t kDigest_conv_01m_0_4 = 0xba011fdcde27d689ull;  // 7381 pairs
constexpr std::uint64_t kDigest_conv_012_0_5 = 0x31dac55a70ff7a98ull;  // 66430 pairs
constexpr std::uint64_t kDigest_random_1_2000 = 0x737af003638665e3ull;  // n = 1..2000: zeta, mobius, convolve of rand arrays
constexpr std::uint64_t kLargeDigest[7][12] = {  // [n index][op]: n 65536 65537 524289 720720 1048573 1048575 1048576; ops zeta_rand mobius_rand zeta_sparse mobius_sparse zeta_ones mobius_ones zeta_max mobius_max conv_rand conv_sparse conv_ones conv_max
    {0x1a7a8ae4f5e364eaull, 0x57abf89df3a98c34ull, 0x49d1c97d60a627d4ull, 0xb5a0803f59567d71ull, 0x2fab5d3672071fdfull, 0x105ebd1cf4b22324ull, 0xe7db30ee15d637e7ull, 0x07a9732ee1362325ull, 0x2c5c9a0d051190dfull, 0x18bd48ed82ac4009ull, 0x2fab5d3672071fdfull, 0x2fab5d3672071fdfull},  // n = 65536
    {0x84710a10bb00d427ull, 0x97e9b9da3b4563adull, 0x3372bbef9a05a445ull, 0xd19e02fc3daa3b47ull, 0x074f4083c21b2487ull, 0x831e7c33cab1b62cull, 0xa33c3cb94a7f00c8ull, 0x3b11dda8aefdb7dfull, 0xcc082555855fb9d9ull, 0x1e04861a522178b7ull, 0x074f4083c21b2487ull, 0x074f4083c21b2487ull},  // n = 65537
    {0x4ce076ff8a397159ull, 0xc398db4693906373ull, 0x3a333616730a5393ull, 0xae99d99c0772f393ull, 0xf0d4f4ba80bed59bull, 0x97670886ab81b62cull, 0x1277f4ae8f46d8bcull, 0x85e3a3aa1461b7dfull, 0xe11a5a286a578a1cull, 0x30d655bb2d8b4190ull, 0xf0d4f4ba80bed59bull, 0xf0d4f4ba80bed59bull},  // n = 524289
    {0x15ff2ad512e0b146ull, 0x5dbe2c0d829aef05ull, 0xef1eb5b23d47e6e2ull, 0x64d40803d3f86644ull, 0x686137c4369370e1ull, 0x5befdf70c82ce024ull, 0x920fd9e3692494fdull, 0x3a230f119e14f565ull, 0x3fdf14dd2e5bdca1ull, 0x9fa39964f3f561a3ull, 0x686137c4369370e1ull, 0x686137c4369370e1ull},  // n = 720720
    {0x2ace61dae09e7b7bull, 0xd188bd59a69d17b0ull, 0xf08e34ba5b5f00dfull, 0x21abf1fcb633cc9bull, 0xfb5577bcc789ee7cull, 0x8c777ad52a2c196cull, 0x5c0f3c45accb4dfbull, 0xd4b6ee136174256full, 0x0467f137c7596d79ull, 0x0a46aad36bedd78bull, 0xfb5577bcc789ee7cull, 0xfb5577bcc789ee7cull},  // n = 1048573
    {0x4be0f565216a1290ull, 0x0b8f570caa5ec776ull, 0x5c0cc3d72fb56814ull, 0x708c307e5976d959ull, 0xb9c1e13f207b7e08ull, 0x451d0ce10e72d64cull, 0x396da15db0a84eb9ull, 0xd9c54d6ce3a96bc7ull, 0xf8a582beea226db0ull, 0x732cb35bbb319b3cull, 0xb9c1e13f207b7e08ull, 0xb9c1e13f207b7e08ull},  // n = 1048575
    {0x9ddc718800362d5aull, 0xac45c4fc54e3e275ull, 0xc38b429a9a9e05d3ull, 0x5965a3715ba4d825ull, 0x1fefdb4431d74b47ull, 0xe3332e6b8d222324ull, 0x6cfa86f49f82536full, 0xb3ae5706d8e22325ull, 0x0ea6c4c0706b1dc9ull, 0x45f09ef6230fa476ull, 0x1fefdb4431d74b47ull, 0x1fefdb4431d74b47ull},  // n = 1048576
};

constexpr u32 kG = 3u;  // guard words before and after each array
constexpr u32 kSent = 0x5A5A5A5Au;
constexpr u32 kP = kMod;
constexpr u64 kInit = 0xcbf29ce484222325ull;

u64 fnv(u64 h, u32 w) { return (h ^ static_cast<u64>(w)) * 0x100000001b3ull; }
u64 fold(u64 h, const std::vector<u32>& v) {
  for (u32 w : v) h = fnv(h, w);
  return h;
}
u32 xs(u32 x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
enum Kind { kRand = 0, kSparse = 1, kOnes = 2, kMax = 3 };
std::vector<u32> gen(int kind, u32 n, u32 seed) {
  std::vector<u32> v(n);
  u32 x = 0x9E3779B9u + n + seed;
  for (u32 i = 0; i < n; ++i) {
    if (kind == kOnes) {
      v[i] = 1u;
    } else if (kind == kMax) {
      v[i] = kP - 1u;
    } else if (kind == kRand) {
      x = xs(x);
      v[i] = x % kP;
    } else {
      x = xs(x);
      const u32 r1 = x;
      x = xs(x);
      v[i] = ((r1 & 7u) == 0u) ? (x % kP) : 0u;
    }
  }
  return v;
}

// ---- calls through guard words and a garbage fill ----
std::vector<u32> g_ba, g_bb, g_bo;
bool guards_ok(const std::vector<u32>& b, u32 n) {
  bool ok = true;
  for (u32 g = 0; g < kG; ++g) ok = ok && (b[g] == kSent) && (b[kG + n + g] == kSent);
  return ok;
}
void load(std::vector<u32>& b, const u32* a, u32 n) {
  b.assign(n + 2u * kG, kSent);
  for (u32 i = 0; i < n; ++i) b[kG + i] = a[i];
}
bool same_range(const std::vector<u32>& b, const u32* a, u32 n) {
  bool ok = true;
  for (u32 i = 0; i < n; ++i) ok = ok && (b[kG + i] == a[i]);
  return ok;
}
// zeta (mob false) or mobius (mob true) of a[0..n-1] into res
bool impl_transform(bool mob, const u32* a, u32 n, std::vector<u32>& res) {
  load(g_ba, a, n);
  const DivResult r = mob ? divisor_mobius(g_ba.data() + kG, n) : divisor_zeta(g_ba.data() + kG, n);
  const bool ok = (r == DivResult::Ok) && guards_ok(g_ba, n);
  res.assign(g_ba.begin() + kG, g_ba.begin() + kG + n);
  return ok;
}
u32 g_fill_counter = 0;
bool impl_conv(const u32* a, const u32* b, u32 n, bool alias, std::vector<u32>& res) {
  static const u32 fills[5] = {0u, 0xFFFFFFFFu, kP, 0x12345678u, 0xFFFFFFFEu};
  const u32 fill = fills[g_fill_counter++ % 5u];
  load(g_ba, a, n);
  load(g_bb, b, n);
  g_bo.assign(n + 2u * kG, kSent);
  for (u32 i = 0; i < n; ++i) g_bo[kG + i] = fill;
  const u32* pb = alias ? g_ba.data() + kG : g_bb.data() + kG;
  const DivResult r = divisor_convolve(g_ba.data() + kG, pb, n, g_bo.data() + kG);
  bool ok = (r == DivResult::Ok) && guards_ok(g_ba, n) && guards_ok(g_bb, n) && guards_ok(g_bo, n);
  ok = ok && same_range(g_ba, a, n) && same_range(g_bb, b, n);
  res.assign(g_bo.begin() + kG, g_bo.begin() + kG + n);
  return ok;
}

// ---- scan brute force: the definition, O(n^2) ----
int mu_of(u32 k) {
  int r = 1;
  for (u32 p = 2; p * p <= k; ++p) {
    if (k % p == 0) {
      k /= p;
      if (k % p == 0) return 0;
      r = -r;
    }
  }
  if (k > 1) r = -r;
  return r;
}
void zeta_scan(const u32* a, u32 n, std::vector<u32>& out) {
  out.assign(n, 0u);
  for (u32 m = 1; m <= n; ++m) {
    u64 s = 0;
    for (u32 d = 1; d <= m; ++d) {
      if (m % d == 0) s += a[d - 1];
    }
    out[m - 1] = static_cast<u32>(s % kP);
  }
}
void mobius_scan(const u32* A, u32 n, std::vector<u32>& out) {
  out.assign(n, 0u);
  for (u32 m = 1; m <= n; ++m) {
    i64 s = 0;
    for (u32 d = 1; d <= m; ++d) {
      if (m % d == 0) s += static_cast<i64>(mu_of(m / d)) * static_cast<i64>(A[d - 1]);
    }
    out[m - 1] = static_cast<u32>(((s % static_cast<i64>(kP)) + static_cast<i64>(kP)) % static_cast<i64>(kP));
  }
}
void conv_scan(const u32* a, const u32* b, u32 n, std::vector<u32>& out) {
  out.assign(n, 0u);
  for (u32 m = 1; m <= n; ++m) {
    u64 s = 0;
    for (u32 d = 1; d <= m; ++d) {
      if (m % d == 0) s += (static_cast<u64>(a[d - 1]) * static_cast<u64>(b[m / d - 1])) % kP;
    }
    out[m - 1] = static_cast<u32>(s % kP);
  }
}

// ---- second constructions (different shape from the implementation) ----
std::vector<u32> primes_upto(u32 n) {
  std::vector<char> comp(n + 1u, 0);
  std::vector<u32> ps;
  for (u32 i = 2; i <= n; ++i) {
    if (comp[i] == 0) {
      ps.push_back(i);
      for (u64 j = static_cast<u64>(i) * i; j <= n; j += i) comp[j] = 1;
    }
  }
  return ps;
}
u32 add_m(u32 x, u32 y) { return static_cast<u32>((static_cast<u64>(x) + y) % kP); }
u32 sub_m(u32 x, u32 y) { return static_cast<u32>((static_cast<u64>(x) + kP - y) % kP); }
// per prime p: zeta adds a[i] into a[i p] for i ascending; mobius undoes it for i descending
void zeta_pp(std::vector<u32>& a, const std::vector<u32>& ps) {
  const u32 n = static_cast<u32>(a.size());
  for (u32 p : ps) {
    for (u32 i = 1; i <= n / p; ++i) a[i * p - 1] = add_m(a[i * p - 1], a[i - 1]);
  }
}
void mobius_pp(std::vector<u32>& a, const std::vector<u32>& ps) {
  const u32 n = static_cast<u32>(a.size());
  for (u32 p : ps) {
    for (u32 i = n / p; i >= 1; --i) a[i * p - 1] = sub_m(a[i * p - 1], a[i - 1]);
  }
}
std::vector<u32> spf_table(u32 n) {
  std::vector<u32> spf(n + 1u);
  for (u32 i = 0; i <= n; ++i) spf[i] = i;
  for (u32 i = 2; static_cast<u64>(i) * i <= n; ++i) {
    if (spf[i] == i) {
      for (u32 j = i * i; j <= n; j += i) {
        if (spf[j] == j) spf[j] = i;
      }
    }
  }
  return spf;
}
// the divisors of every m from its factorisation, then the sum
void conv_spf(const u32* a, const u32* b, u32 n, const std::vector<u32>& spf, std::vector<u32>& out) {
  out.assign(n, 0u);
  u32 divs[512];
  for (u32 m = 1; m <= n; ++m) {
    u32 cnt = 1;
    divs[0] = 1u;
    u32 r = m;
    while (r > 1u) {
      const u32 p = spf[r];
      u32 e = 0;
      while (r % p == 0u) {
        r /= p;
        ++e;
      }
      const u32 base = cnt;
      u32 pw = 1;
      for (u32 k = 0; k < e; ++k) {
        pw *= p;
        for (u32 j = 0; j < base; ++j) divs[cnt++] = divs[j] * pw;
      }
    }
    u64 s = 0;
    for (u32 j = 0; j < cnt; ++j) s += (static_cast<u64>(a[divs[j] - 1u]) * b[m / divs[j] - 1u]) % kP;
    out[m - 1u] = static_cast<u32>(s % kP);
  }
}

// ---- enumeration, last position fastest ----
template <class F>
void for_arrays(const std::vector<u32>& alpha, u32 n, F f) {
  std::vector<u32> idx(n, 0u), a(n, alpha[0]);
  while (true) {
    f(a);
    u32 pos = n;
    while (pos > 0u) {
      --pos;
      if (++idx[pos] < alpha.size()) {
        a[pos] = alpha[idx[pos]];
        break;
      }
      idx[pos] = 0u;
      a[pos] = alpha[0];
      if (pos == 0u) return;
    }
    if (n == 0u) return;
  }
}

constexpr u64 kCount01m = 797161u, kCount012m = 87381u;

// transforms vs the scan and the two round trips on every array of the alphabet, lengths 0..nmax
u64 exhaustive_transforms(const std::vector<u32>& alpha, u32 nmax, u64& bad) {
  u64 count = 0;
  std::vector<u32> z, m, zs, ms, back;
  for (u32 n = 0; n <= nmax; ++n) {
    for_arrays(alpha, n, [&](const std::vector<u32>& a) {
      ++count;
      bool ok = impl_transform(false, a.data(), n, z) && impl_transform(true, a.data(), n, m);
      zeta_scan(a.data(), n, zs);
      mobius_scan(a.data(), n, ms);
      ok = ok && (z == zs) && (m == ms);
      ok = ok && impl_transform(false, m.data(), n, back) && (back == a);  // zeta(mobius(a)) == a
      ok = ok && impl_transform(true, z.data(), n, back) && (back == a);   // mobius(zeta(a)) == a
      if (!ok) ++bad;
    });
  }
  return count;
}
u64 exhaustive_conv(const std::vector<u32>& alpha, u32 nmax, u64& bad) {
  u64 count = 0;
  std::vector<u32> c, cs, ca, cas;
  for (u32 n = 0; n <= nmax; ++n) {
    std::vector<std::vector<u32>> all;
    for_arrays(alpha, n, [&](const std::vector<u32>& a) { all.push_back(a); });
    for (const std::vector<u32>& a : all) {
      bool alias_ok = impl_conv(a.data(), a.data(), n, true, ca);
      conv_scan(a.data(), a.data(), n, cas);
      if (!alias_ok || ca != cas) ++bad;
      for (const std::vector<u32>& b : all) {
        ++count;
        const bool ok = impl_conv(a.data(), b.data(), n, false, c);
        conv_scan(a.data(), b.data(), n, cs);
        if (!ok || c != cs) ++bad;
      }
    }
  }
  return count;
}

u64 digest_transforms(const std::vector<u32>& alpha, u32 nmax, bool mob) {
  u64 h = kInit;
  std::vector<u32> r;
  for (u32 n = 0; n <= nmax; ++n) {
    for_arrays(alpha, n, [&](const std::vector<u32>& a) {
      impl_transform(mob, a.data(), n, r);
      h = fold(h, r);
    });
  }
  return h;
}
u64 digest_conv(const std::vector<u32>& alpha, u32 nmax) {
  u64 h = kInit;
  std::vector<u32> r;
  for (u32 n = 0; n <= nmax; ++n) {
    std::vector<std::vector<u32>> all;
    for_arrays(alpha, n, [&](const std::vector<u32>& a) { all.push_back(a); });
    for (const std::vector<u32>& a : all) {
      for (const std::vector<u32>& b : all) {
        impl_conv(a.data(), b.data(), n, false, r);
        h = fold(h, r);
      }
    }
  }
  return h;
}

const std::vector<u32> kAlpha01m = {0u, 1u, kP - 1u};
const std::vector<u32> kAlpha012m = {0u, 1u, 2u, kP - 1u};
const std::vector<u32> kAlpha012 = {0u, 1u, 2u};

TEST_CASE("divtrans: hand-checked values and wrong-order patterns", "[divtrans]") {
  std::vector<u32> r;
  {  // unit at 1: every integer gets it exactly once (a wrong order counts 4 twice, 8 four times)
    const u32 a[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    CHECK(impl_transform(false, a, 8, r));
    CHECK(r == std::vector<u32>(8, 1u));
    CHECK(impl_transform(true, r.data(), 8, r));
    CHECK(r == std::vector<u32>({1, 0, 0, 0, 0, 0, 0, 0}));
  }
  {
    const u32 a[4] = {0, 1, 0, 0};
    CHECK(impl_transform(false, a, 4, r));
    CHECK(r == std::vector<u32>({0, 1, 0, 1}));
    const u32 b[4] = {1, 1, 0, 0};
    CHECK(impl_transform(false, b, 4, r));
    CHECK(r == std::vector<u32>({1, 2, 1, 2}));
    const u32 c[4] = {1, 1, 1, 1};
    CHECK(impl_transform(false, c, 4, r));
    CHECK(r == std::vector<u32>({1, 2, 2, 3}));
    CHECK(impl_transform(true, r.data(), 4, r));
    CHECK(r == std::vector<u32>({1, 1, 1, 1}));
  }
  {  // ones: the divisor counts; their inverse is ones; mobius of ones is e_1
    const std::vector<u32> ones(12, 1u);
    CHECK(impl_transform(false, ones.data(), 12, r));
    CHECK(r == std::vector<u32>({1, 2, 2, 3, 2, 4, 2, 4, 3, 4, 2, 6}));
    CHECK(impl_transform(true, r.data(), 12, r));
    CHECK(r == ones);
    CHECK(impl_transform(true, ones.data(), 12, r));
    CHECK(r == std::vector<u32>({1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));  // mobius(1) = e_1
    std::vector<u32> e1(12, 0u);
    e1[0] = 1u;
    CHECK(impl_transform(true, e1.data(), 12, r));  // mobius(e_1) = mu
    CHECK(r == std::vector<u32>({1, kP - 1, kP - 1, 0, kP - 1, 1, kP - 1, 0, 0, 1, kP - 1, 0}));
    CHECK(impl_transform(false, r.data(), 12, r));  // zeta(mu) = e_1
    CHECK(r == e1);
  }
  {  // zeta of Euler's phi is the identity
    const u32 phi[12] = {1, 1, 2, 2, 4, 2, 6, 4, 6, 4, 10, 4};
    CHECK(impl_transform(false, phi, 12, r));
    CHECK(r == std::vector<u32>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}));
  }
  {  // convolution: identity, ones * ones, phi * ones, commutative on a sample
    const u32 e1[6] = {1, 0, 0, 0, 0, 0}, b[6] = {5, 4, 3, 2, 1, 7};
    CHECK(impl_conv(e1, b, 6, false, r));
    CHECK(r == std::vector<u32>({5, 4, 3, 2, 1, 7}));
    CHECK(impl_conv(b, e1, 6, false, r));
    CHECK(r == std::vector<u32>({5, 4, 3, 2, 1, 7}));
    const std::vector<u32> ones(12, 1u);
    CHECK(impl_conv(ones.data(), ones.data(), 12, false, r));
    CHECK(r == std::vector<u32>({1, 2, 2, 3, 2, 4, 2, 4, 3, 4, 2, 6}));
    const u32 phi[12] = {1, 1, 2, 2, 4, 2, 6, 4, 6, 4, 10, 4};
    CHECK(impl_conv(phi, ones.data(), 12, false, r));
    CHECK(r == std::vector<u32>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}));
    const u32 x[6] = {3, 1, 4, 1, 5, 9}, y[6] = {2, 7, 1, 8, 2, 8};
    std::vector<u32> r2;
    CHECK(impl_conv(x, y, 6, false, r));
    CHECK(impl_conv(y, x, 6, false, r2));
    CHECK(r == r2);
    CHECK(r == std::vector<u32>({6, 3 * 7 + 1 * 2, 3 * 1 + 4 * 2, 3 * 8 + 1 * 7 + 1 * 2, 3 * 2 + 5 * 2, 3 * 8 + 1 * 1 + 4 * 7 + 9 * 2}));
  }
  {  // n = 0 and n = 1
    std::vector<u32> e;
    CHECK(impl_transform(false, nullptr, 0, e));
    CHECK(e.empty());
    const u32 one[1] = {kP - 1u};
    CHECK(impl_transform(false, one, 1, r));
    CHECK(r == std::vector<u32>({kP - 1u}));
    CHECK(impl_conv(one, one, 1, false, r));
    CHECK(r == std::vector<u32>({1u}));  // (p-1)^2 = 1
  }
}

TEST_CASE("divtrans: failure cases, check order, nothing written (sentinel-filled)", "[divtrans]") {
  const u32 ns[] = {0u, 1u, 2u, kMaxN, kMaxN + 1u, 0xFFFFFFFFu};
  std::vector<u32> bigz(kMaxN, 0u);
  // transforms: a null or not, every n
  for (int mob = 0; mob < 2; ++mob) {
    for (int nul = 0; nul < 2; ++nul) {
      for (u32 n : ns) {
        u32 buf[4] = {7u, 7u, 7u, 7u};  // valid residues, so a successful call is possible
        u32* p = (nul != 0) ? nullptr : buf;
        DivResult want = DivResult::Ok;
        if (n > kMaxN) {
          want = DivResult::TooLarge;
        } else if (n > 0u && p == nullptr) {
          want = DivResult::NullArgument;
        }
        if (want == DivResult::Ok && n > 2u) continue;  // a successful call needs real arrays: covered by the other cases
        const DivResult got = (mob != 0) ? divisor_mobius(p, n) : divisor_zeta(p, n);
        CHECK(got == want);
        bool untouched = true;
        for (u32 i = 0; i < 4u; ++i) untouched = untouched && (buf[i] == 7u);
        if (want != DivResult::Ok || n == 0u) CHECK(untouched);
      }
    }
  }
  // convolution: every null combination
  for (int mask = 0; mask < 8; ++mask) {
    for (u32 n : ns) {
      u32 ba[4] = {1, 2, 3, 4}, bb[4] = {5, 6, 7, 8}, bo[4] = {kSent, kSent, kSent, kSent};
      const u32* pa = ((mask & 1) != 0) ? nullptr : ba;
      const u32* pb = ((mask & 2) != 0) ? nullptr : bb;
      u32* po = ((mask & 4) != 0) ? nullptr : bo;
      DivResult want = DivResult::Ok;
      if (n > kMaxN) {
        want = DivResult::TooLarge;
      } else if (n > 0u && (pa == nullptr || pb == nullptr || po == nullptr)) {
        want = DivResult::NullArgument;
      }
      if (want == DivResult::Ok && n > 2u) continue;
      const DivResult got = divisor_convolve(pa, pb, n, po);
      CHECK(got == want);
      bool untouched = true;
      for (u32 i = 0; i < 4u; ++i) untouched = untouched && (bo[i] == kSent);
      if (want != DivResult::Ok || n == 0u) CHECK(untouched);
      CHECK((ba[0] == 1u && ba[3] == 4u && bb[0] == 5u && bb[3] == 8u));
    }
  }
  // oversize precedes nulls; null precedes a bad entry (n = 12, the bad entry is only reachable with real pointers)
  {
    u32 bad[12] = {kMod, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ok[12] = {0}, out[12];
    for (u32& v : out) v = kSent;
    CHECK(divisor_convolve(bad, ok, 12, nullptr) == DivResult::NullArgument);
    CHECK(divisor_convolve(nullptr, bad, 12, out) == DivResult::NullArgument);
    CHECK(divisor_convolve(bad, ok, kMaxN + 1u, nullptr) == DivResult::TooLarge);
    CHECK(divisor_zeta(nullptr, 12) == DivResult::NullArgument);
    CHECK(divisor_mobius(nullptr, 12) == DivResult::NullArgument);
    bool untouched = true;
    for (u32 v : out) untouched = untouched && (v == kSent);
    CHECK(untouched);
  }
  // invalid residues at the first, middle and last position of a and of b, three bad values, sentinel-filled outputs
  const u32 badvals[] = {kMod, kMod + 1u, 0xFFFFFFFFu};
  const u32 poss[] = {0u, 5u, 11u};
  for (u32 bv : badvals) {
    for (u32 pos : poss) {
      for (int which = 0; which < 3; ++which) {  // 0: a bad, 1: b bad, 2: both bad (a at pos, b at the other end)
        u32 a[12], b[12], out[12];
        for (u32 i = 0; i < 12u; ++i) {
          a[i] = i + 1u;
          b[i] = 100u + i;
          out[i] = kSent;
        }
        if (which == 0 || which == 2) a[pos] = bv;
        if (which == 1 || which == 2) b[11u - pos] = bv;
        u32 ca[12], cb[12];
        for (u32 i = 0; i < 12u; ++i) {
          ca[i] = a[i];
          cb[i] = b[i];
        }
        CHECK(divisor_convolve(a, b, 12, out) == DivResult::ValueOutOfRange);
        bool untouched = true;
        for (u32 i = 0; i < 12u; ++i) untouched = untouched && (out[i] == kSent) && (a[i] == ca[i]) && (b[i] == cb[i]);
        CHECK(untouched);
        // transforms: the same bad array
        for (int mob = 0; mob < 2; ++mob) {
          u32 t[12];
          for (u32 i = 0; i < 12u; ++i) t[i] = a[i];
          if (which == 1) t[pos] = bv;
          const DivResult got = (mob != 0) ? divisor_mobius(t, 12) : divisor_zeta(t, 12);
          CHECK(got == DivResult::ValueOutOfRange);
          bool same = true;
          for (u32 i = 0; i < 12u; ++i) same = same && (t[i] == ((which == 1 && i == pos) ? bv : a[i]));
          CHECK(same);
        }
      }
    }
  }
  // the largest valid residue is accepted, and a bad entry in a on a larger n is found at the last position
  {
    std::vector<u32> big(5000u, 3u), out(5000u, kSent), t = big;
    big[4999] = kMod;
    CHECK(divisor_convolve(big.data(), t.data(), 5000, out.data()) == DivResult::ValueOutOfRange);
    CHECK(divisor_convolve(t.data(), big.data(), 5000, out.data()) == DivResult::ValueOutOfRange);
    CHECK(divisor_zeta(big.data(), 5000) == DivResult::ValueOutOfRange);
    CHECK(divisor_mobius(big.data(), 5000) == DivResult::ValueOutOfRange);
    bool untouched = (big[4999] == kMod) && (big[0] == 3u);
    for (u32 v : out) untouched = untouched && (v == kSent);
    CHECK(untouched);
    t[4999] = kP - 1u;
    CHECK(divisor_zeta(t.data(), 5000) == DivResult::Ok);
  }
  (void)bigz;
}

TEST_CASE("divtrans: exhaustive transforms over {0,1,p-1} length 0 to 12 and {0,1,2,p-1} length 0 to 8 against the scan, both round trips", "[divtrans][exhaustive][slow]") {
  u64 bad = 0;
  const u64 c1 = exhaustive_transforms(kAlpha01m, 12, bad);
  const u64 c2 = exhaustive_transforms(kAlpha012m, 8, bad);
  CHECK(c1 == kCount01m);
  CHECK(c2 == kCount012m);
  CHECK(bad == 0u);
}

TEST_CASE("divtrans: exhaustive convolution pairs over {0,1,p-1} length 0 to 6 and {0,1,2} length 0 to 7 against the scan, also a = b", "[divtrans][exhaustive][slow]") {
  u64 bad = 0;
  const u64 c1 = exhaustive_conv(kAlpha01m, 6, bad);
  const u64 c2 = exhaustive_conv(kAlpha012, 7, bad);
  CHECK(c1 == 597871u);
  CHECK(c2 == 5380840u);
  CHECK(bad == 0u);
}

TEST_CASE("divtrans: exhaustive reference digests (transforms {0,1,p-1} 0..8 and {0,1,2,p-1} 0..6, pairs {0,1,p-1} 0..4 and {0,1,2} 0..5)", "[divtrans][exhaustive]") {
  CHECK(digest_transforms(kAlpha01m, 8, false) == kDigest_zeta_01m_0_8);
  CHECK(digest_transforms(kAlpha01m, 8, true) == kDigest_mobius_01m_0_8);
  CHECK(digest_transforms(kAlpha012m, 6, false) == kDigest_zeta_012m_0_6);
  CHECK(digest_transforms(kAlpha012m, 6, true) == kDigest_mobius_012m_0_6);
  CHECK(digest_conv(kAlpha01m, 4) == kDigest_conv_01m_0_4);
  CHECK(digest_conv(kAlpha012, 5) == kDigest_conv_012_0_5);
}

TEST_CASE("divtrans: random arrays for every n 1 to 2000 (second constructions, scan up to 300, digest, a = b)", "[divtrans][sampled]") {
  const std::vector<u32> ps = primes_upto(2000u);
  const std::vector<u32> spf = spf_table(2000u);
  u64 h = kInit, bad = 0;
  std::vector<u32> z, m, c, r2, ca;
  for (u32 n = 1; n <= 2000u; ++n) {
    const std::vector<u32> a = gen(kRand, n, 0u), b = gen(kRand, n, 77u);
    bool ok = impl_transform(false, a.data(), n, z) && impl_transform(true, a.data(), n, m) && impl_conv(a.data(), b.data(), n, false, c);
    h = fold(fold(fold(h, z), m), c);
    std::vector<u32> w = a;
    zeta_pp(w, ps);
    ok = ok && (w == z);
    w = a;
    mobius_pp(w, ps);
    ok = ok && (w == m);
    conv_spf(a.data(), b.data(), n, spf, r2);
    ok = ok && (c == r2);
    ok = ok && impl_conv(a.data(), a.data(), n, true, ca);
    conv_spf(a.data(), a.data(), n, spf, r2);
    ok = ok && (ca == r2);
    if (n <= 300u) {
      zeta_scan(a.data(), n, w);
      ok = ok && (w == z);
      mobius_scan(a.data(), n, w);
      ok = ok && (w == m);
      conv_scan(a.data(), b.data(), n, w);
      ok = ok && (w == c);
    }
    if (!ok) ++bad;
  }
  CHECK(bad == 0u);
  CHECK(h == kDigest_random_1_2000);
}

const u32 kLargeN[7] = {65536u, 65537u, 524289u, 720720u, 1048573u, 1048575u, 1048576u};

TEST_CASE("divtrans: large n transforms, 4 kinds of inputs, digests, per-prime second construction, round trips", "[divtrans][sampled]") {
  std::vector<u32> z, m, back;
  for (u32 ni = 0; ni < 7u; ++ni) {
    const u32 n = kLargeN[ni];
    const std::vector<u32> ps = primes_upto(n);
    for (int kind = 0; kind < 4; ++kind) {
      const std::vector<u32> a = gen(kind, n, 0u);
      const bool okz = impl_transform(false, a.data(), n, z);
      const bool okm = impl_transform(true, a.data(), n, m);
      CHECK(okz);
      CHECK(okm);
      CHECK(fold(kInit, z) == kLargeDigest[ni][static_cast<u32>(kind) * 2u]);
      CHECK(fold(kInit, m) == kLargeDigest[ni][static_cast<u32>(kind) * 2u + 1u]);
      if (kind == kRand || kind == kSparse) {
        std::vector<u32> w = a;
        zeta_pp(w, ps);
        CHECK(w == z);
        w = a;
        mobius_pp(w, ps);
        CHECK(w == m);
        CHECK((impl_transform(false, m.data(), n, back) && back == a));
        CHECK((impl_transform(true, z.data(), n, back) && back == a));
      }
    }
  }
}

TEST_CASE("divtrans: large n convolution, 4 kinds of inputs, digests, divisor-factorisation second construction", "[divtrans][sampled]") {
  std::vector<u32> c, r2;
  for (u32 ni = 0; ni < 7u; ++ni) {
    const u32 n = kLargeN[ni];
    const std::vector<u32> spf = spf_table(n);
    const std::vector<u32> rb = gen(kRand, n, 77u);
    for (int kind = 0; kind < 4; ++kind) {
      const std::vector<u32> a = gen(kind, n, 0u);
      const std::vector<u32> b = (kind == kOnes) ? gen(kOnes, n, 0u) : (kind == kMax) ? gen(kMax, n, 0u) : rb;
      CHECK(impl_conv(a.data(), b.data(), n, false, c));
      CHECK(fold(kInit, c) == kLargeDigest[ni][8u + static_cast<u32>(kind)]);
      if (kind == kRand) {
        conv_spf(a.data(), b.data(), n, spf, r2);
        CHECK(c == r2);
      }
    }
  }
}

TEST_CASE("divtrans: closed forms on the full array (divisor count, phi, mu, identity), large n", "[divtrans][sampled]") {
  std::vector<u32> r;
  for (u32 ni = 0; ni < 7u; ++ni) {
    const u32 n = kLargeN[ni];
    const std::vector<u32> spf = spf_table(n);
    std::vector<u32> dcount(n, 0u), phi(n), mu(n), ident(n), ones(n, 1u), e1(n, 0u);
    e1[0] = 1u;
    for (u32 d = 1; d <= n; ++d) {
      for (u32 m = d; m <= n; m += d) ++dcount[m - 1];
    }
    std::vector<i64> mui(n + 1u, 0);
    std::vector<u32> ph(n + 1u, 0u);
    mui[1] = 1;
    ph[1] = 1u;
    for (u32 k = 2; k <= n; ++k) {
      const u32 p = spf[k], q = k / p;
      mui[k] = (q % p == 0u) ? 0 : -mui[q];
      ph[k] = (q % p == 0u) ? ph[q] * p : ph[q] * (p - 1u);
    }
    for (u32 k = 1; k <= n; ++k) {
      mu[k - 1] = (mui[k] == 0) ? 0u : (mui[k] == 1) ? 1u : kP - 1u;
      phi[k - 1] = ph[k];
      ident[k - 1] = k;
    }
    CHECK((impl_transform(false, ones.data(), n, r) && r == dcount));       // zeta(1) = d
    CHECK((impl_transform(false, phi.data(), n, r) && r == ident));         // zeta(phi) = id
    CHECK((impl_transform(true, ones.data(), n, r) && r == e1));            // mobius(1) = e_1
    CHECK((impl_transform(true, e1.data(), n, r) && r == mu));              // mobius(e_1) = mu
    CHECK((impl_transform(false, mu.data(), n, r) && r == e1));             // zeta(mu) = e_1
    CHECK((impl_transform(true, dcount.data(), n, r) && r == ones));        // mobius(d) = 1
    CHECK((impl_transform(true, ident.data(), n, r) && r == phi));          // mobius(id) = phi
    CHECK((impl_conv(ones.data(), ones.data(), n, true, r) && r == dcount));  // 1 * 1 = d
    CHECK((impl_conv(phi.data(), ones.data(), n, false, r) && r == ident));   // phi * 1 = id
    CHECK((impl_conv(mu.data(), ones.data(), n, false, r) && r == e1));       // mu * 1 = e_1
  }
}

}  // namespace
