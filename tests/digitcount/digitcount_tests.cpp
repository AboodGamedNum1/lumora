// Tests for core/digitcount. Evidence label: unit tests only.
// Expected values come from an in-test scan (running counts of the digit sum of every x, computed by repeated division; not the method of the
// implementation), from digests made by tools/digitcount_ref.py (standard library only: the scan is THE oracle on the small extents; at large n the
// big-integer generating-function oracle is the oracle, because the scan is infeasible at 2^40; the reference compares the two on the small extents) and from
// identities that do not use the implementation's method. Extents, named in the test names and in docs/digitcount.md:
//   exhaustive vs the in-test scan: count_digit_sum for every base 2..16 with every n 0..2000 and every s 0..cap+1 (cap = (base - 1) * digits of n), and for
//               every base 17..64 with every n 0..300 and every s 0..cap+1, with the sum of the counts, the sum of s * count and the largest s with a non-zero
//               count checked against the scan; digit_sum_total for every base 2..64 and every n 0..2^18;
//   boundaries: every base 2..64, n in {b^k - 1, b^k, b^k + 1} up to 2^40 and n in {2^40 - 1, 2^40}, every s 0..cap+1, as reference digests, plus the identities
//               (sum of counts = n + 1, sum of s * count = the total, the difference with n - 1, the symmetry at b^k - 1, s = 0 gives 1);
//   sampled: forty n up to 2^40 per base from xorshift64, every s 0..cap+1, as reference digests and with the same identities; n = 0..200 per base as
//               reference digests of the scan.
// Every call passes a cell between guard words, pre-filled with a sentinel. No test asserts anything about time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "digitcount.h"

using lumora::digitcount::count_digit_sum;
using lumora::digitcount::digit_sum_total;
using lumora::digitcount::DigitResult;
using lumora::digitcount::kMaxN;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr std::uint64_t kSmallDigest[63] = {  // per base 2..64
    0xd98fb022e28821dbull, 0x58fc8c8417ea7676ull, 0xe2ae9b3516fcaaedull,
    0x6335aeacd01e4970ull, 0xe6de3f5c4bd1c3a3ull, 0xb254754ceabb264cull,
    0x606eb6c29e8eff8eull, 0x918352898269d548ull, 0x50b62d0820310312ull,
    0xc705120f7cce13c8ull, 0x2b4b74b461736ba9ull, 0xed4f00755a157640ull,
    0xe80cb91aa415523aull, 0x9b331425826dc764ull, 0x19a6c5df26a3f500ull,
    0x88e3fe4c6fe9bd86ull, 0x9e4954d4b56af4e5ull, 0x1bbacc17dad22c30ull,
    0xe75fb0dadac62b46ull, 0xcca6eb4e15755754ull, 0xbaec03ada7cfce90ull,
    0x1ab75f87ef724f54ull, 0xe230fee420f8a6fcull, 0x82abbccf35d19b34ull,
    0xe4664ebdcd67f7dbull, 0xb2288de2b2f74370ull, 0x2cef1e8fdc72248full,
    0x596bbeb3759b3630ull, 0xda26cdd8fbaeeb8full, 0x1f9c6bda2efcacc4ull,
    0x0fefb0c175b5161cull, 0xc8a14e0ecd8bfeecull, 0x74a19cfeef134b0eull,
    0x06a173dfa8df8a80ull, 0xa7c7144efa07947dull, 0x9dfcf6ee1c99b578ull,
    0x845b629f6cdacd42ull, 0x57498a5c5ab05224ull, 0xcb1feb37a710fe2dull,
    0x8d48cf6b0ac8a75cull, 0x4db5f5a40a353966ull, 0x52a06e675ce971c4ull,
    0xe44fa13088acf550ull, 0x364307ea46b011c4ull, 0x74cdbea58fe66628ull,
    0xe8c44ca08237a71cull, 0x81c8ef6a1d956a96ull, 0x9300c49e1dd662e4ull,
    0x1523a7051dc9365eull, 0x437acf6d6d0801a0ull, 0x3144de5157e243fdull,
    0xd1d8ec572b7f2db2ull, 0xe2ec95ee12e7540full, 0xe30845727116ca7cull,
    0x5ee7efb59c53653full, 0x01e30c5d5a481ca6ull, 0x5d9184b9121b48b5ull,
    0xee4ef2b46e5efc08ull, 0xdf54b79f76057705ull, 0x784b0ad00f2d943aull,
    0x8beeba2c9816541full, 0xdb485c50893b1c5cull, 0x0701ddbd0352370full,
};
constexpr std::uint64_t kBoundaryDigest[63] = {  // per base 2..64
    0x7f3ea93b66af9a70ull, 0x4a299b33755e279dull, 0xf30cb7d17dbbc996ull,
    0x8cd8b51f96d08eaaull, 0xef3e4dee82d53041ull, 0xe53758d8f299a11bull,
    0x3875dc44476daed5ull, 0x18e446190682be3eull, 0x7d04e39185334414ull,
    0x18b9a3abb8cd624aull, 0x314cd4c19c757e06ull, 0x85f71106e0d0dc6eull,
    0x845e8bf492e41473ull, 0x595577937f9bcf03ull, 0x03969e90ab1269b8ull,
    0x3829b842970c6262ull, 0x7a77fd1b0b6ccd35ull, 0x2ee388ed218979bdull,
    0xfdb2cd2e49e41e78ull, 0x3b99264eef7c6720ull, 0xbe4177332a4b9578ull,
    0x442c2ada6372b244ull, 0xd52fbeab3c1e5afcull, 0x5421563080848612ull,
    0xf38888f4a570c967ull, 0x02ee7a613cbe1aacull, 0x7a97a09fa6e68832ull,
    0xd1d1cfc752f8c560ull, 0xb29bd5b9108110c8ull, 0x8b4603c3edf0be9eull,
    0xdbf7f4f6020fa838ull, 0x0c5caa2360465d92ull, 0xeceed558533e4e58ull,
    0x229b6f8be5272da8ull, 0x9e90bb47bca615d4ull, 0x2323bc700fc4a73cull,
    0x33bb2dcf6f70334full, 0xf303e1b6cb831526ull, 0x36823ecfd3e88934ull,
    0xb995dd6cd9203f12ull, 0x1b7e664254176492ull, 0x8468f02a832ea2f4ull,
    0xb4001ddf373f4377ull, 0xa0c7d8b03da6c948ull, 0xe4eebef12f67613full,
    0x530554a1a3195b0aull, 0x1742b70456af0bc4ull, 0x89059f97e4c0c77cull,
    0xfe48d7e17c434ac5ull, 0x5f170185d2c6da7aull, 0xf4c1e77e8a671b46ull,
    0x681e415a1d7e51c6ull, 0x26ccd595df05ce04ull, 0x003f898bbb4d0957ull,
    0x332913e5f4538ae8ull, 0x67ab8aa1953179d4ull, 0x2a439b77d7aed57cull,
    0xffdb1c410045121dull, 0x8d950c9e03bce623ull, 0x7a0d4abe244143b6ull,
    0xc459009ec57e68fdull, 0x78ec7d0c41cc7d8dull, 0x25f76ad8000d2c90ull,
};
constexpr std::uint64_t kRandomDigest[63] = {  // per base 2..64
    0xe269fcc9b32385bcull, 0x037b842928312ebfull, 0xe3281855060c3c25ull,
    0x11983382ab252debull, 0x3a5644cd3175df2full, 0xbf43a6a16ec7cdd3ull,
    0x0132f276ead50159ull, 0xeb05732b80e31a18ull, 0x184d6b044937d49dull,
    0x624af4a46e7df119ull, 0xbf359c5332aa7912ull, 0x392add8dfc3679ffull,
    0xf6fa5fd2193941bcull, 0x0bcfc581c65a6b61ull, 0x2fd98d7e22dcdf0eull,
    0xde30d2bc663b8976ull, 0x301caabbfb9785afull, 0x7497d1b1a0e8d557ull,
    0x443fee5658485f9eull, 0x0283c836e169c8a0ull, 0x138d3244d620cda1ull,
    0xe54a2d2fc2979e31ull, 0x4773fc5154450488ull, 0xd45dbfad36e92253ull,
    0xd522ddf13f07a97dull, 0xefae548c2a9cc076ull, 0x78e0913bb0b8dcdfull,
    0x424591e1ab95971full, 0x0bbfed53151bb655ull, 0x0db608506ffc6101ull,
    0xa5fb7deb05d385b2ull, 0x304243b6b27220c1ull, 0x3392e5c54a72603eull,
    0x27a5ecff60bdc31bull, 0xad2f64f6e27ead69ull, 0xa777a0518ff80cfdull,
    0xba0ff029a3252c8cull, 0xba787f0222019d8cull, 0x5e00d731cbaa3da5ull,
    0xee50321f36bb0ef7ull, 0xc6d0aaa4c963e3f2ull, 0xf4189b884ffaa195ull,
    0xd20ec5d89ab87ca3ull, 0x6a9a86ac203d0a28ull, 0x2ce450c497ad772aull,
    0xbe54526e37376c24ull, 0x64ba86c1c88be0f8ull, 0xce6160206db02d4aull,
    0xe60364e23ea433b7ull, 0x1777c28e37e715e0ull, 0x5772455c596e8407ull,
    0x496ff4b7042ada1eull, 0xf2c96922640e7a1bull, 0xec9dd3c318021401ull,
    0x40077a2cae4fefd2ull, 0xab10309ddb62a2deull, 0xe56b46ec8911edccull,
    0x74cd0ac5303a23f5ull, 0x145329cb5b3bfc97ull, 0x1e59ad437e9692a3ull,
    0xb32238fc76df916bull, 0x24b8e312214c37b1ull, 0x53cb4b9a097f7921ull,
};

constexpr u64 kSent = 0x5A5A5A5A5A5A5A5Aull;
constexpr u64 kInit = 0xcbf29ce484222325ull;
u64 fnv(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }

u64 digit_sum(u64 x, u32 b) {
  u64 s = 0;
  while (x > 0u) {
    s += x % b;
    x /= b;
  }
  return s;
}
u32 ndigits(u64 n, u32 b) {
  u32 k = 0;
  while (n > 0u) {
    ++k;
    n /= b;
  }
  return k;
}

// Calls through a cell between guard words; returns false when the result is not Ok or a guard changed.
bool impl_count(u64 n, u32 b, u64 s, u64& res) {
  u64 cell[5] = {kSent, kSent, kSent, kSent, kSent};
  const DigitResult r = count_digit_sum(n, b, s, cell + 2);
  res = cell[2];
  return r == DigitResult::Ok && cell[0] == kSent && cell[1] == kSent && cell[3] == kSent && cell[4] == kSent;
}
bool impl_total(u64 n, u32 b, u64& res) {
  u64 cell[5] = {kSent, kSent, kSent, kSent, kSent};
  const DigitResult r = digit_sum_total(n, b, cell + 2);
  res = cell[2];
  return r == DigitResult::Ok && cell[0] == kSent && cell[1] == kSent && cell[3] == kSent && cell[4] == kSent;
}

// The result words of one (n, b): counts for s = 0..cap+1, then the total.
bool impl_words(u64 n, u32 b, std::vector<u64>& w) {
  const u64 cap = static_cast<u64>(b - 1u) * ndigits(n, b);
  w.assign(cap + 2u, 0u);
  bool ok = true;
  for (u64 s = 0; s < cap + 2u; ++s) ok = ok && impl_count(n, b, s, w[s]);
  u64 t = 0;
  ok = ok && impl_total(n, b, t);
  w.push_back(t);
  return ok;
}
u64 fold_words(u64 h, const std::vector<u64>& w) {
  for (u64 x : w) h = fnv(h, x);
  return h;
}

u64 pow_cap(u32 b, u32 k) {  // b^k, saturating above 2^41
  u64 p = 1;
  for (u32 i = 0; i < k; ++i) {
    p *= b;
    if (p > (kMaxN << 1u)) return kMaxN << 1u;
  }
  return p;
}

std::vector<u64> boundary_ns(u32 b) {
  std::vector<u64> v;
  auto add = [&](u64 n) {
    if (n <= kMaxN) {
      for (u64 x : v) {
        if (x == n) return;
      }
      v.push_back(n);
    }
  };
  u64 p = 1;
  while (p <= kMaxN) {
    add(p - 1u);
    add(p);
    add(p + 1u);
    p *= b;
  }
  add(kMaxN - 1u);
  add(kMaxN);
  // sort ascending (the reference lists them in ascending order)
  for (std::size_t i = 1; i < v.size(); ++i) {
    for (std::size_t j = i; j > 0 && v[j - 1] > v[j]; --j) {
      const u64 t = v[j];
      v[j] = v[j - 1];
      v[j - 1] = t;
    }
  }
  return v;
}
std::vector<u64> random_ns(u32 b) {
  std::vector<u64> v;
  u64 x = 0x9E3779B97F4A7C15ull + b;
  for (int i = 0; i < 40; ++i) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    v.push_back(x % (kMaxN + 1u));
  }
  return v;
}

// Identities that do not use the method of the implementation. Returns the number of failed identities.
u32 check_identities(u64 n, u32 b) {
  u32 bad = 0;
  std::vector<u64> w, w1;
  if (!impl_words(n, b, w)) ++bad;
  const std::size_t m = w.size() - 1u;  // counts are w[0..m-1], the total is w[m]
  u64 sum = 0, weighted = 0;
  for (std::size_t s = 0; s < m; ++s) {
    sum += w[s];
    weighted += static_cast<u64>(s) * w[s];
  }
  if (sum != n + 1u) ++bad;
  if (weighted != w[m]) ++bad;
  const u64 ds = digit_sum(n, b);
  if (n > 0u) {
    if (!impl_words(n - 1u, b, w1)) ++bad;
    const std::size_t m1 = w1.size() - 1u;
    for (std::size_t s = 0; s < m; ++s) {
      const u64 prev = (s < m1) ? w1[s] : 0u;
      if (w[s] - prev != ((s == ds) ? 1u : 0u)) ++bad;
    }
    if (w[m] - w1[m1] != ds) ++bad;
  }
  // s = 0 counts only x = 0; at n = 0 and at n = b^k it is exactly 1
  if (w[0] != 1u) ++bad;
  // n = b^k - 1: the counts are symmetric under s -> k (b - 1) - s
  u64 p = 1;
  while (p <= n) p *= b;
  if (p - 1u == n && n > 0u) {
    const u64 top = static_cast<u64>(b - 1u) * ndigits(n, b);
    for (u64 s = 0; s <= top; ++s) {
      if (w[s] != w[top - s]) ++bad;
    }
  }
  return bad;
}

// count_digit_sum for every n 0..nmax and every s 0..cap+1 against running scan counts; the totals too (when with_total)
u64 scan_check(u32 b, u64 nmax, u64& bad) {
  std::vector<u64> cnt(512, 0u);
  u64 total = 0, maxds = 0, calls = 0;
  for (u64 n = 0; n <= nmax; ++n) {
    const u64 ds = digit_sum(n, b);
    ++cnt[ds];
    total += ds;
    if (ds > maxds) maxds = ds;
    const u64 cap = static_cast<u64>(b - 1u) * ndigits(n, b);
    u64 sum = 0, weighted = 0, top_nonzero = 0, got = 0;
    for (u64 s = 0; s <= cap + 1u; ++s) {
      ++calls;
      if (!impl_count(n, b, s, got) || got != cnt[s]) ++bad;
      sum += got;
      weighted += s * got;
      if (got != 0u) top_nonzero = s;
    }
    u64 t = 0;
    if (!impl_total(n, b, t) || t != total || sum != n + 1u || weighted != total || top_nonzero != maxds) ++bad;
  }
  return calls;
}

TEST_CASE("digitcount: hand-checked values and closed forms", "[digitcount]") {
  u64 r = 0;
  // base 10, n = 99: the digit sums of 0..99
  CHECK((impl_count(99, 10, 0, r) && r == 1u));
  CHECK((impl_count(99, 10, 1, r) && r == 2u));  // 1, 10
  CHECK((impl_count(99, 10, 9, r) && r == 10u));  // 9, 18, ..., 90
  CHECK((impl_count(99, 10, 10, r) && r == 9u));  // 19, 28, ..., 91
  CHECK((impl_count(99, 10, 18, r) && r == 1u));
  CHECK((impl_count(99, 10, 19, r) && r == 0u));
  CHECK((impl_total(99, 10, r) && r == 900u));
  // base 2, n = 7: popcounts 0,1,1,2,1,2,2,3
  CHECK((impl_count(7, 2, 0, r) && r == 1u));
  CHECK((impl_count(7, 2, 1, r) && r == 3u));
  CHECK((impl_count(7, 2, 2, r) && r == 3u));
  CHECK((impl_count(7, 2, 3, r) && r == 1u));
  CHECK((impl_total(7, 2, r) && r == 12u));
  // n = 0
  for (u32 b = 2; b <= 64u; ++b) {
    CHECK((impl_count(0, b, 0, r) && r == 1u));
    CHECK((impl_count(0, b, 1, r) && r == 0u));
    CHECK((impl_count(0, b, 0xFFFFFFFFFFFFFFFFull, r) && r == 0u));
    CHECK((impl_total(0, b, r) && r == 0u));
  }
  // n = 2^40 in base 2: the powers of two give s = 1 (41 of them), the total is 40 * 2^39 + 1
  CHECK((impl_count(kMaxN, 2, 1, r) && r == 41u));
  CHECK((impl_count(kMaxN, 2, 0, r) && r == 1u));
  CHECK((impl_count(kMaxN, 2, 40, r) && r == 1u));  // 2^40 - 1
  CHECK((impl_count(kMaxN, 2, 41, r) && r == 0u));
  CHECK((impl_total(kMaxN, 2, r) && r == 40u * (1ull << 39) + 1u));
  // total(b^k - 1) = k * b^(k-1) * b (b - 1) / 2 for every base and power below 2^40
  for (u32 b = 2; b <= 64u; ++b) {
    for (u32 k = 1; pow_cap(b, k) <= kMaxN; ++k) {
      const u64 want = static_cast<u64>(k) * pow_cap(b, k - 1u) * (static_cast<u64>(b) * (b - 1u) / 2u);
      CHECK((impl_total(pow_cap(b, k) - 1u, b, r) && r == want));
    }
  }
}

TEST_CASE("digitcount: failure cases, check order, nothing written (sentinel in *out)", "[digitcount]") {
  const u32 bad_bases[] = {0u, 1u, 65u, 66u, 128u, 0xFFFFFFFFu};
  const u64 big_ns[] = {kMaxN + 1u, kMaxN + 2u, 0xFFFFFFFFFFFFFFFFull};
  const u64 ok_ns[] = {0u, 1u, 5u, 0xFFFFFFFFull, kMaxN - 1u, kMaxN};
  const u64 all_ns[] = {0u, 1u, 5u, 0xFFFFFFFFull, kMaxN - 1u, kMaxN, kMaxN + 1u, kMaxN + 2u, 0xFFFFFFFFFFFFFFFFull};
  const u64 ss[] = {0u, 1u, 7u, 441u, 442u, 0xFFFFFFFFull, 1ull << 63, 0xFFFFFFFFFFFFFFFFull};
  // null out: always NullArgument, whatever the base, n and s (null first)
  for (u32 b : {0u, 1u, 2u, 10u, 64u, 65u, 0xFFFFFFFFu}) {
    for (u64 n : all_ns) {
      for (u64 s : ss) CHECK(count_digit_sum(n, b, s, nullptr) == DigitResult::NullArgument);
      CHECK(digit_sum_total(n, b, nullptr) == DigitResult::NullArgument);
    }
  }
  // bad base with a valid or an oversize n: BadBase (base before size)
  for (u32 b : bad_bases) {
    for (u64 n : all_ns) {
      for (u64 s : ss) {
        u64 cell = kSent;
        CHECK(count_digit_sum(n, b, s, &cell) == DigitResult::BadBase);
        CHECK(cell == kSent);
      }
      u64 cell = kSent;
      CHECK(digit_sum_total(n, b, &cell) == DigitResult::BadBase);
      CHECK(cell == kSent);
    }
  }
  // oversize n with every valid base: TooLarge
  for (u32 b = 2; b <= 64u; ++b) {
    for (u64 n : big_ns) {
      for (u64 s : ss) {
        u64 cell = kSent;
        CHECK(count_digit_sum(n, b, s, &cell) == DigitResult::TooLarge);
        CHECK(cell == kSent);
      }
      u64 cell = kSent;
      CHECK(digit_sum_total(n, b, &cell) == DigitResult::TooLarge);
      CHECK(cell == kSent);
    }
  }
  // 2^40 and UINT32_MAX are legal; any s is accepted, a huge or unattainable s gives 0
  for (u32 b = 2; b <= 64u; ++b) {
    for (u64 n : ok_ns) {
      u64 t = 0;
      CHECK(impl_total(n, b, t));
      for (u64 s : ss) {
        u64 r = kSent;
        const u64 cap = static_cast<u64>(b - 1u) * ndigits(n, b);
        CHECK(impl_count(n, b, s, r));
        if (s > cap) CHECK(r == 0u);
      }
    }
  }
}

TEST_CASE("digitcount: exhaustive count_digit_sum for bases 2 to 16 with n 0 to 2000 and bases 17 to 64 with n 0 to 300, every s, against the scan", "[digitcount][exhaustive][slow]") {
  u64 bad = 0, calls = 0;
  for (u32 b = 2; b <= 16u; ++b) calls += scan_check(b, 2000u, bad);
  for (u32 b = 17; b <= 64u; ++b) calls += scan_check(b, 300u, bad);
  CHECK(bad == 0u);
  CHECK(calls > 0u);
}

TEST_CASE("digitcount: exhaustive digit_sum_total for every base 2 to 64 and every n 0 to 2^18 against a running scan", "[digitcount][exhaustive][slow]") {
  u64 bad = 0;
  for (u32 b = 2; b <= 64u; ++b) {
    u64 total = 0, t = 0;
    for (u64 n = 0; n <= (1ull << 18); ++n) {
      total += digit_sum(n, b);
      if (!impl_total(n, b, t) || t != total) ++bad;
    }
  }
  CHECK(bad == 0u);
}

TEST_CASE("digitcount: reference digests of the scan for n 0 to 200 in every base", "[digitcount][exhaustive]") {
  for (u32 b = 2; b <= 64u; ++b) {
    u64 h = kInit;
    std::vector<u64> w;
    bool ok = true;
    for (u64 n = 0; n <= 200u; ++n) {
      ok = ok && impl_words(n, b, w);
      h = fold_words(h, w);
    }
    CHECK(ok);
    CHECK(h == kSmallDigest[b - 2u]);
  }
}

TEST_CASE("digitcount: boundaries b^k - 1, b^k, b^k + 1 up to 2^40 and 2^40 - 1, 2^40 in every base: reference digests and identities", "[digitcount][sampled]") {
  u32 bad = 0;
  for (u32 b = 2; b <= 64u; ++b) {
    u64 h = kInit;
    std::vector<u64> w;
    for (u64 n : boundary_ns(b)) {
      if (!impl_words(n, b, w)) ++bad;
      h = fold_words(h, w);
      bad += check_identities(n, b);
    }
    CHECK(h == kBoundaryDigest[b - 2u]);
  }
  CHECK(bad == 0u);
}

TEST_CASE("digitcount: forty random n up to 2^40 per base: reference digests and identities", "[digitcount][sampled]") {
  u32 bad = 0;
  for (u32 b = 2; b <= 64u; ++b) {
    u64 h = kInit;
    std::vector<u64> w;
    for (u64 n : random_ns(b)) {
      if (!impl_words(n, b, w)) ++bad;
      h = fold_words(h, w);
      bad += check_identities(n, b);
    }
    CHECK(h == kRandomDigest[b - 2u]);
  }
  CHECK(bad == 0u);
}

}  // namespace
