// Tests for core/sortu (scope AO). Expected values come from an independent Python reference (tools/sortu_ref.py: an LSD sort over
// bucket lists in base 256, sorted() only as a second check, and its own FNV) and are pasted below: seeded scenarios (the key
// generator is mirrored here, values are the original index times an odd constant) whose digest folds the sortedness of the input, the
// sorted keys, the permuted values and the sortedness of the result. Direct cases cover the argument rules, stability with many equal
// keys against a naive stable oracle, and is_sorted. Every array is an exact-size heap vector, so ASan sees any access past it.
// Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "sortu.h"

using namespace lumora::sortu;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kVMul = 2654435761u;

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int k = 0; k < 4; ++k) {
      h ^= (v >> (8 * k)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

struct Lcg {
  u64 s;
  explicit Lcg(u64 seed) : s(seed) {}
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  u32 rng(u32 lo, u32 hi) { return static_cast<u32>(lo + next() % (static_cast<u64>(hi) - lo + 1)); }
};

struct ScCase {
  u32 n;
  u64 seed;
  int mode;
  u64 digest;
};

const ScCase kScCases[] = {{0, 501, 0, 3010618080379317301ull}, {0, 502, 1, 3010618080379317301ull}, {0, 503, 2, 3010618080379317301ull}, {0, 504, 3, 3010618080379317301ull}, {0, 505, 4, 3010618080379317301ull}, {0, 506, 5, 3010618080379317301ull}, {0, 507, 6, 3010618080379317301ull}, {1, 508, 0, 8030289004156646504ull}, {1, 509, 1, 14014804997892777523ull}, {1, 510, 2, 10262262680828168260ull}, {1, 511, 3, 10262262680828168260ull}, {1, 512, 4, 374786180202609187ull}, {1, 513, 5, 15489371977748742413ull}, {1, 514, 6, 11450931886620250094ull}, {2, 515, 0, 16428863999349033511ull}, {2, 516, 1, 10294078399589287753ull}, {2, 517, 2, 2217427212804159167ull}, {2, 518, 3, 3420909619144797855ull}, {2, 519, 4, 12387133162482846118ull}, {2, 520, 5, 17003350615805387960ull}, {2, 521, 6, 10137890815548947571ull}, {3, 522, 0, 4866519139064481657ull}, {3, 523, 1, 11119146924959634029ull}, {3, 524, 2, 14000590360219278286ull}, {3, 525, 3, 16439442471246634543ull}, {3, 526, 4, 15580735737429738284ull}, {3, 527, 5, 367241486018105471ull}, {3, 528, 6, 13084327195417380004ull}, {4, 529, 0, 16348139542930832158ull}, {4, 530, 1, 15396582825439131952ull}, {4, 531, 2, 2324517685261621081ull}, {4, 532, 3, 2145449593715641108ull}, {4, 533, 4, 6149168955360823835ull}, {4, 534, 5, 2396716307015783980ull}, {4, 535, 6, 9677671912141513259ull}, {5, 536, 0, 15206515524860001543ull}, {5, 537, 1, 12767824859670956344ull}, {5, 538, 2, 4913033711264702263ull}, {5, 539, 3, 9029362229520796255ull}, {5, 540, 4, 9445004296035456605ull}, {5, 541, 5, 1476518382345100314ull}, {5, 542, 6, 7739146849956189593ull}, {16, 543, 0, 18222286291789899315ull}, {16, 544, 1, 18221633280365119760ull}, {16, 545, 2, 4497212897819254932ull}, {16, 546, 3, 12664751107298319174ull}, {16, 547, 4, 13367231827031954280ull}, {16, 548, 5, 4873052134759513071ull}, {16, 549, 6, 8545256195938848499ull}, {255, 550, 0, 15023698484574966900ull}, {255, 551, 1, 1726725290388714327ull}, {255, 552, 2, 11508982340626216335ull}, {255, 553, 3, 4675224616959862957ull}, {255, 554, 4, 9130426690928374443ull}, {255, 555, 5, 14450141539153330261ull}, {255, 556, 6, 1054266641327798483ull}, {256, 557, 0, 10468805183892510268ull}, {256, 558, 1, 13981121406366433120ull}, {256, 559, 2, 1325248337159181328ull}, {256, 560, 3, 468847410982578888ull}, {256, 561, 4, 4404281959931908929ull}, {256, 562, 5, 6844679210644547196ull}, {256, 563, 6, 17704965386743858597ull}, {257, 564, 0, 15662167064171248764ull}, {257, 565, 1, 13403173162233078569ull}, {257, 566, 2, 8849371387966177511ull}, {257, 567, 3, 12950556025639456800ull}, {257, 568, 4, 13552601830595913298ull}, {257, 569, 5, 12383026004344881704ull}, {257, 570, 6, 3655330768677119265ull}, {1000, 571, 0, 10074300598905188871ull}, {1000, 572, 1, 16465626897620930467ull}, {1000, 573, 2, 11124972966329991708ull}, {1000, 574, 3, 535951426651281310ull}, {1000, 575, 4, 15061429084307359153ull}, {1000, 576, 5, 3696429902722538544ull}, {1000, 577, 6, 9832385212723935735ull}, {4096, 578, 0, 14924420393498300395ull}, {4096, 579, 1, 8423675267324380573ull}, {4096, 580, 2, 148855541191055562ull}, {4096, 581, 3, 16919685082422957721ull}, {4096, 582, 4, 15379785692877315306ull}, {4096, 583, 5, 14126582424676513902ull}, {4096, 584, 6, 1168839486691274805ull}, {5000, 585, 0, 9749839304641159381ull}, {5000, 586, 1, 16171797950668528061ull}, {5000, 587, 2, 14226406175354209256ull}, {5000, 588, 3, 17224049298946422977ull}, {5000, 589, 4, 7363310962997062628ull}, {5000, 590, 5, 2721819360946411389ull}, {5000, 591, 6, 1170412619408238174ull}};

std::vector<u32> gen_keys(u32 n, u64 seed, int mode) {
  Lcg r(seed);
  u32 pool[5];
  for (u32& p : pool) p = r.next();
  std::vector<u32> out;
  for (u32 i = 0; i < n; ++i) {
    u32 k;
    if (mode == 0) k = r.next();
    else if (mode == 1) k = pool[r.rng(0, 4)];
    else if (mode == 2) k = i * 3u + r.rng(0, 2);
    else if (mode == 3) k = (n - 1 - i) * 3u + r.rng(0, 2);
    else if (mode == 4) k = pool[0];
    else if (mode == 5) k = 0xABCDEF00u | r.rng(0, 255);
    else k = (r.rng(0, 255) << 24) | 0x00123456u;
    out.push_back(k);
  }
  return out;
}

// naive stable oracle: insertion sort on (key, value), shares nothing with the radix code
void oracle(std::vector<u32>& k, std::vector<u32>& v) {
  for (std::size_t i = 1; i < k.size(); ++i) {
    const u32 ck = k[i], cv = v[i];
    std::size_t j = i;
    while (j > 0 && k[j - 1] > ck) {
      k[j] = k[j - 1];
      v[j] = v[j - 1];
      --j;
    }
    k[j] = ck;
    v[j] = cv;
  }
}

// Fills a block of stack with a nonzero pattern so that a count table that is not fully cleared would hold garbage.
u32 dirty_stack(u32 salt) {
  volatile u32 block[4096];
  for (u32 i = 0; i < 4096; ++i) block[i] = 0x01010101u * (salt & 255u) + i;
  return block[salt & 4095u];
}

}  // namespace

TEST_CASE("sortu: scenarios match the independent reference") {
  for (const ScCase& c : kScCases) {
    std::vector<u32> keys = gen_keys(c.n, c.seed, c.mode);
    std::vector<u32> values(c.n);
    for (u32 i = 0; i < c.n; ++i) values[i] = i * kVMul;
    std::vector<u32> keys_only = keys;
    Fnv f;
    bool before = false;
    REQUIRE(is_sorted_u32(keys.data(), c.n, &before) == SortResult::Ok);
    f.word(before ? 1u : 0u);
    std::vector<u32> sk(c.n), sv(c.n), s1(c.n);
    REQUIRE(radix_sort_pairs_u32(keys.data(), values.data(), c.n, sk.data(), sv.data()) == SortResult::Ok);
    REQUIRE(radix_sort_u32(keys_only.data(), c.n, s1.data()) == SortResult::Ok);
    CHECK(keys_only == keys);
    for (u32 k : keys) f.word(k);
    for (u32 v : values) f.word(v);
    bool after = false;
    REQUIRE(is_sorted_u32(keys.data(), c.n, &after) == SortResult::Ok);
    f.word(after ? 1u : 0u);
    CHECK(f.h == c.digest);
    // the naive stable oracle agrees
    std::vector<u32> ok = gen_keys(c.n, c.seed, c.mode);
    std::vector<u32> ov(c.n);
    for (u32 i = 0; i < c.n; ++i) ov[i] = i * kVMul;
    if (c.n <= 1000) {
      oracle(ok, ov);
      CHECK(ok == keys);
      CHECK(ov == values);
    }
  }
}

TEST_CASE("sortu: stability with many equal keys") {
  Lcg r(77);
  for (int round = 0; round < 60; ++round) {
    const u32 n = r.rng(1, 700);
    const u32 distinct = r.rng(1, 6);
    u32 pool[6];
    for (u32& p : pool) p = r.next();
    std::vector<u32> keys(n), values(n);
    for (u32 i = 0; i < n; ++i) {
      keys[i] = pool[r.rng(0, distinct - 1)];
      values[i] = i;  // the original position
    }
    std::vector<u32> ek = keys, ev = values;
    oracle(ek, ev);
    std::vector<u32> sk(n), sv(n);
    REQUIRE(radix_sort_pairs_u32(keys.data(), values.data(), n, sk.data(), sv.data()) == SortResult::Ok);
    REQUIRE(keys == ek);
    REQUIRE(values == ev);
    // within every run of equal keys the original positions are strictly increasing
    for (u32 i = 1; i < n; ++i)
      if (keys[i - 1] == keys[i]) REQUIRE(values[i - 1] < values[i]);
  }
  // all equal keys: values keep their order exactly
  std::vector<u32> k(500, 0xFFFFFFFFu), v(500), sk(500), sv(500);
  for (u32 i = 0; i < 500; ++i) v[i] = 500 - i;
  const std::vector<u32> v0 = v;
  REQUIRE(radix_sort_pairs_u32(k.data(), v.data(), 500, sk.data(), sv.data()) == SortResult::Ok);
  CHECK(v == v0);
  // keys that differ only in one byte position at a time, with equal-key runs
  for (u32 shift = 0; shift < 32; shift += 8) {
    std::vector<u32> kk(300), vv(300), a(300), b(300);
    for (u32 i = 0; i < 300; ++i) {
      kk[i] = (((i * 7u) % 5u) << shift);
      vv[i] = i;
    }
    std::vector<u32> ek = kk, ev = vv;
    oracle(ek, ev);
    REQUIRE(radix_sort_pairs_u32(kk.data(), vv.data(), 300, a.data(), b.data()) == SortResult::Ok);
    CHECK(kk == ek);
    CHECK(vv == ev);
  }
}

TEST_CASE("sortu: every byte position and the extreme keys") {
  // keys that need each of the four passes, and the values 0 and 0xFFFFFFFF
  const u32 raw[] = {0xFFFFFFFFu, 0u, 0x80000000u, 0x7FFFFFFFu, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u,
                     1u, 0x100u, 0x10000u, 0x1000000u, 0xFFFFFFFEu, 0x80000001u, 0x01000000u, 0x00010000u};
  std::vector<u32> keys(raw, raw + 16), values(16), sk(16), sv(16);
  for (u32 i = 0; i < 16; ++i) values[i] = 100 + i;
  std::vector<u32> ek = keys, ev = values;
  oracle(ek, ev);
  REQUIRE(radix_sort_pairs_u32(keys.data(), values.data(), 16, sk.data(), sv.data()) == SortResult::Ok);
  CHECK(keys == ek);
  CHECK(values == ev);
  CHECK(keys.front() == 0u);
  CHECK(keys.back() == 0xFFFFFFFFu);
  // already sorted, reversed, and a single byte pass
  std::vector<u32> asc(256), desc(256), tmp(256);
  for (u32 i = 0; i < 256; ++i) {
    asc[i] = i << 8;
    desc[i] = (255 - i) << 8;
  }
  REQUIRE(radix_sort_u32(desc.data(), 256, tmp.data()) == SortResult::Ok);
  CHECK(desc == asc);
  REQUIRE(radix_sort_u32(asc.data(), 256, tmp.data()) == SortResult::Ok);
  CHECK(asc == desc);
}

TEST_CASE("sortu: argument rules") {
  u32 k[4] = {3, 1, 2, 0}, v[4] = {10, 11, 12, 13}, sk[4] = {7, 7, 7, 7}, sv[4] = {8, 8, 8, 8};
  const u32 k0[4] = {3, 1, 2, 0}, v0[4] = {10, 11, 12, 13};
  auto same = [&]() {
    for (int i = 0; i < 4; ++i) {
      CHECK(k[i] == k0[i]);
      CHECK(v[i] == v0[i]);
      CHECK(sk[i] == 7u);
      CHECK(sv[i] == 8u);
    }
  };
  // null pointers with n above 0
  CHECK(radix_sort_u32(nullptr, 4, sk) == SortResult::NullArgument);
  CHECK(radix_sort_u32(k, 4, nullptr) == SortResult::NullArgument);
  CHECK(radix_sort_u32(nullptr, 1, sk) == SortResult::NullArgument);
  CHECK(radix_sort_u32(k, 1, nullptr) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(nullptr, v, 4, sk, sv) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(k, nullptr, 4, sk, sv) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(k, v, 4, nullptr, sv) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(k, v, 4, sk, nullptr) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(k, v, 1, sk, nullptr) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(k, nullptr, 1, sk, sv) == SortResult::NullArgument);
  // null is checked before the size
  CHECK(radix_sort_u32(nullptr, kMaxCount + 1, sk) == SortResult::NullArgument);
  CHECK(radix_sort_u32(k, 0xFFFFFFFFu, nullptr) == SortResult::NullArgument);
  CHECK(radix_sort_pairs_u32(k, v, kMaxCount + 1, sk, nullptr) == SortResult::NullArgument);
  // too large, small dummy buffers, nothing read or written
  CHECK(radix_sort_u32(k, kMaxCount + 1, sk) == SortResult::TooLarge);
  CHECK(radix_sort_u32(k, 0xFFFFFFFFu, sk) == SortResult::TooLarge);
  CHECK(radix_sort_pairs_u32(k, v, kMaxCount + 1, sk, sv) == SortResult::TooLarge);
  CHECK(radix_sort_pairs_u32(k, v, 0xFFFFFFFFu, sk, sv) == SortResult::TooLarge);
  same();
  // n 0 with every pointer null
  CHECK(radix_sort_u32(nullptr, 0, nullptr) == SortResult::Ok);
  CHECK(radix_sort_pairs_u32(nullptr, nullptr, 0, nullptr, nullptr) == SortResult::Ok);
  CHECK(radix_sort_u32(k, 0, sk) == SortResult::Ok);
  CHECK(radix_sort_pairs_u32(k, v, 0, sk, sv) == SortResult::Ok);
  same();
  // n 1 changes nothing, scratch untouched
  u32 one = 0xDEADBEEFu, onev = 5, os = 9, osv = 9;
  CHECK(radix_sort_u32(&one, 1, &os) == SortResult::Ok);
  CHECK(radix_sort_pairs_u32(&one, &onev, 1, &os, &osv) == SortResult::Ok);
  CHECK(one == 0xDEADBEEFu);
  CHECK(onev == 5u);
  CHECK(os == 9u);
  CHECK(osv == 9u);
  // a real sort of the four
  CHECK(radix_sort_pairs_u32(k, v, 4, sk, sv) == SortResult::Ok);
  CHECK(k[0] == 0u);
  CHECK(k[1] == 1u);
  CHECK(k[2] == 2u);
  CHECK(k[3] == 3u);
  CHECK(v[0] == 13u);
  CHECK(v[1] == 11u);
  CHECK(v[2] == 12u);
  CHECK(v[3] == 10u);
}

TEST_CASE("sortu: is_sorted") {
  bool out = false;
  const std::vector<u32> asc = {1, 2, 2, 3, 0xFFFFFFFFu};
  CHECK(is_sorted_u32(asc.data(), 5, &out) == SortResult::Ok);
  CHECK(out);
  const std::vector<u32> bad = {1, 3, 2};
  CHECK(is_sorted_u32(bad.data(), 3, &out) == SortResult::Ok);
  CHECK_FALSE(out);
  const std::vector<u32> last = {1, 2, 3, 4, 3};
  CHECK(is_sorted_u32(last.data(), 5, &out) == SortResult::Ok);
  CHECK_FALSE(out);
  const std::vector<u32> first = {2, 1, 3, 4, 5};
  CHECK(is_sorted_u32(first.data(), 5, &out) == SortResult::Ok);
  CHECK_FALSE(out);
  const std::vector<u32> eq = {7, 7, 7, 7};
  CHECK(is_sorted_u32(eq.data(), 4, &out) == SortResult::Ok);
  CHECK(out);
  const std::vector<u32> big = {0xFFFFFFFFu, 0};
  CHECK(is_sorted_u32(big.data(), 2, &out) == SortResult::Ok);
  CHECK_FALSE(out);
  const std::vector<u32> two = {5, 5};
  CHECK(is_sorted_u32(two.data(), 2, &out) == SortResult::Ok);
  CHECK(out);
  out = false;
  CHECK(is_sorted_u32(nullptr, 0, &out) == SortResult::Ok);
  CHECK(out);
  out = false;
  CHECK(is_sorted_u32(asc.data(), 0, &out) == SortResult::Ok);
  CHECK(out);
  out = false;
  CHECK(is_sorted_u32(asc.data(), 1, &out) == SortResult::Ok);
  CHECK(out);
  out = true;
  const std::vector<u32> one = {9};
  CHECK(is_sorted_u32(one.data(), 1, &out) == SortResult::Ok);
  CHECK(out);
  // failures leave *out alone
  out = true;
  CHECK(is_sorted_u32(nullptr, 3, &out) == SortResult::NullArgument);
  CHECK(is_sorted_u32(nullptr, 1, &out) == SortResult::NullArgument);
  CHECK(is_sorted_u32(asc.data(), 5, nullptr) == SortResult::NullArgument);
  CHECK(is_sorted_u32(nullptr, 0, nullptr) == SortResult::NullArgument);
  CHECK(is_sorted_u32(asc.data(), 0, nullptr) == SortResult::NullArgument);
  CHECK(is_sorted_u32(nullptr, kMaxCount + 1, &out) == SortResult::NullArgument);
  CHECK(is_sorted_u32(asc.data(), kMaxCount + 1, &out) == SortResult::TooLarge);
  CHECK(is_sorted_u32(asc.data(), 0xFFFFFFFFu, &out) == SortResult::TooLarge);
  CHECK(out);
  out = false;
  CHECK(is_sorted_u32(bad.data(), kMaxCount + 1, &out) == SortResult::TooLarge);
  CHECK_FALSE(out);
}

TEST_CASE("sortu: random sizes against the stable oracle") {
  Lcg r(0x50127);
  for (int k = 0; k < 400; ++k) {
    const u32 n = r.rng(0, 300);
    const u32 bits = r.rng(0, 32);
    std::vector<u32> keys(n), values(n);
    for (u32 i = 0; i < n; ++i) {
      keys[i] = bits == 32 ? r.next() : (bits == 0 ? 0u : (r.next() >> (32 - bits)));
      values[i] = r.next();
    }
    std::vector<u32> ek = keys, ev = values;
    oracle(ek, ev);
    std::vector<u32> sk(n), sv(n), s1(n), k1 = keys;
    REQUIRE(radix_sort_pairs_u32(keys.data(), values.data(), n, sk.data(), sv.data()) == SortResult::Ok);
    REQUIRE(keys == ek);
    REQUIRE(values == ev);
    REQUIRE(radix_sort_u32(k1.data(), n, s1.data()) == SortResult::Ok);
    REQUIRE(k1 == ek);
    bool sorted = false;
    REQUIRE(is_sorted_u32(keys.data(), n, &sorted) == SortResult::Ok);
    REQUIRE(sorted);
  }
}

TEST_CASE("sortu: odd and even numbers of executed passes end in the caller's arrays") {
  // keys that vary in a chosen set of byte positions; passes over uniform bytes are skipped, so the number executed is the number
  // of varying positions (1 and 3 are the odd cases that need the copy back, 2 and 4 end in place)
  const u32 masks[] = {0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u, 0x0000FFFFu, 0x00FF00FFu, 0xFF0000FFu, 0x00FFFF00u,
                       0x00FFFFFFu, 0xFFFFFF00u, 0xFF00FFFFu, 0xFFFF00FFu, 0xFFFFFFFFu};
  Lcg r(31);
  for (u32 mask : masks) {
    for (u32 n : {2u, 3u, 40u, 257u}) {
      std::vector<u32> keys(n), values(n);
      for (u32 i = 0; i < n; ++i) {
        keys[i] = (r.next() & mask) | (0x5A5A5A5Au & ~mask);
        values[i] = i;
      }
      std::vector<u32> ek = keys, ev = values;
      oracle(ek, ev);
      std::vector<u32> k1 = keys, sk(n, 0xAAAAAAAAu), sv(n, 0xBBBBBBBBu), s1(n, 0xCCCCCCCCu);
      dirty_stack(mask);
      REQUIRE(radix_sort_pairs_u32(keys.data(), values.data(), n, sk.data(), sv.data()) == SortResult::Ok);
      REQUIRE(keys == ek);
      REQUIRE(values == ev);
      dirty_stack(mask + 1);
      REQUIRE(radix_sort_u32(k1.data(), n, s1.data()) == SortResult::Ok);
      REQUIRE(k1 == ek);
    }
  }
}

TEST_CASE("sortu: the count table is cleared on every call") {
  // keys whose top byte is 255 and others, sorted right after the stack was filled with garbage
  for (u32 salt = 1; salt < 40; ++salt) {
    const u32 n = 50;
    std::vector<u32> keys(n), values(n), sk(n), sv(n);
    Lcg r(salt);
    for (u32 i = 0; i < n; ++i) {
      keys[i] = (r.rng(0, 1) ? 0xFFu : r.rng(0, 255)) * 0x01010101u;
      values[i] = i;
    }
    std::vector<u32> ek = keys, ev = values, k1 = keys, s1(n);
    oracle(ek, ev);
    dirty_stack(salt);
    REQUIRE(radix_sort_pairs_u32(keys.data(), values.data(), n, sk.data(), sv.data()) == SortResult::Ok);
    REQUIRE(keys == ek);
    REQUIRE(values == ev);
    dirty_stack(salt * 3u);
    REQUIRE(radix_sort_u32(k1.data(), n, s1.data()) == SortResult::Ok);
    REQUIRE(k1 == ek);
  }
}
