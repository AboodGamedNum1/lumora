// Tests for core/assign. Evidence label: unit tests only. Expected minimum totals come from tools/assign_ref.py: brute force over all permutations for
// every small matrix, exact subset dynamic programming for the mid ones, and optima known by construction (cost = r[i] + c[j] + d[i][j], d >= 0 and
// zero along a permutation) for the max-size ones. The small groups are also checked against an in-test brute force. Every success test uses a scratch
// array of exactly 16n + 16 words (filled with garbage) and an exact-size out_col pre-filled with a sentinel. Which optimal assignment is returned is
// not specified, so no test compares it with anything but the matrix: every returned assignment is checked to be a permutation whose cost equals the
// reported total. Tiny or null buffers appear only for failures and the empty matrix. No test asserts any scratch value after a call. The max-size
// cases are sampled patterns, not exhaustive. Time is not measured.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "assign.h"

using lumora::assign::assign_run;
using lumora::assign::assign_scratch_words;
using lumora::assign::AssignResult;
using lumora::assign::kMaxN;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

namespace {

// groups: instances, digest lo, digest hi
const u32 kGroups[18] = {
    5u, 1595787903u, 1598566600u,
    1296u, 260538823u, 4125150041u,
    262144u, 2483792997u, 2047066592u,
    65536u, 3649239809u, 1326872951u,
    256u, 1476307669u, 215121795u,
    262144u, 1013160253u, 1211265517u,
};
const u32 kBattery[2] = {3687323428u, 291289103u};
const u32 kMid[2] = {2314873766u, 2208337263u};
// max-size patterns: pid, n, total lo, total hi
const u32 kBig[48] = {
    0u, 512u, 0u, 0u,
    1u, 512u, 4294966784u, 511u,
    2u, 512u, 632098304u, 0u,
    3u, 512u, 0u, 0u,
    4u, 512u, 0u, 0u,
    5u, 512u, 684733643u, 129u,
    6u, 512u, 4293441067u, 511u,
    7u, 512u, 1558u, 0u,
    8u, 512u, 4219736806u, 251u,
    9u, 512u, 3843967724u, 239u,
    10u, 512u, 0u, 0u,
    11u, 300u, 4152914084u, 71u,
};

const u32 kGarbage = 0xA5A5A5A5u;
const u32 kSentinel = 0xDEADBEEFu;
const u64 kSentinel64 = 0xDEADBEEFCAFEF00Dull;
const u32 kMaxV = 0xFFFFFFFFu;
const u64 kFnvOff = 0xCBF29CE484222325ull;
const u64 kFnvPrime = 0x100000001B3ull;

u64 fold_bytes(u64 h, u64 v, int nbytes) {
  for (int i = 0; i < nbytes; ++i) {
    h ^= (v & 0xFFu);
    v >>= 8;
    h *= kFnvPrime;
  }
  return h;
}

struct Mat {
  u32 n = 0;
  std::vector<u32> a;
};

struct Run {
  AssignResult res;
  u64 total;
  std::vector<u32> col;
};

Run run(const Mat& m, u32 fill = kGarbage) {
  std::vector<u32> scratch(16ull * m.n + 16ull, fill);
  Run r;
  r.col.assign(m.n, kSentinel);
  r.total = kSentinel64;
  r.res = assign_run(m.n, m.a.data(), scratch.data(), static_cast<u32>(scratch.size()), &r.total, r.col.data());
  return r;
}

struct Lcg {
  u64 s;
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 33);
  }
  u32 rnd(u32 k) { return next() % k; }
};

// In-test brute force over all permutations (small n only).
u64 brute(const Mat& m) {
  std::vector<u32> p(m.n);
  for (u32 i = 0; i < m.n; ++i) p[i] = i;
  u64 best = ~0ull;
  do {
    u64 s = 0;
    for (u32 i = 0; i < m.n; ++i) s += m.a[static_cast<u64>(i) * m.n + p[i]];
    best = std::min(best, s);
  } while (std::next_permutation(p.begin(), p.end()));
  return best;
}

// Certificate checker: out_col is a permutation, its cost equals the total, and the matrix was not modified (compared by the caller).
const char* certify(const Mat& m, const Run& r, u64 expected) {
  if (r.res != AssignResult::Ok) return "result not Ok";
  if (r.total != expected) return "total differs from the expected minimum";
  std::vector<unsigned char> seen(m.n, 0);
  u64 cost = 0;
  for (u32 i = 0; i < m.n; ++i) {
    if (r.col[i] >= m.n || seen[r.col[i]]) return "not a permutation";
    seen[r.col[i]] = 1;
    cost += m.a[static_cast<u64>(i) * m.n + r.col[i]];
  }
  if (cost != r.total) return "cost of the assignment differs from the total";
  return "";
}

bool certified(const Mat& m, const Run& r, u64 expected) { return certify(m, r, expected)[0] == '\0'; }

u32 rnd32(Lcg& r) {
  const u32 hi = r.rnd(65536);
  const u32 lo = r.rnd(65536);
  return (hi << 16) | lo;
}

u32 entry(Lcg& r, u32 mode) {
  if (mode == 0) return r.rnd(4);
  if (mode == 1) return r.rnd(100);
  if (mode == 2) return rnd32(r);
  return kMaxV - r.rnd(8);
}

void shuffle(Lcg& r, std::vector<u32>& e) {
  for (u32 i = static_cast<u32>(e.size()); i-- > 1;) std::swap(e[i], e[r.rnd(i + 1u)]);
}

// Every matrix with entries from `vals` (row-major odometer, last entry fastest), identical to tools/assign_ref.py.
u64 enumerate_group(u32 n, const std::vector<u32>& vals, u32& count) {
  const u32 cells = n * n;
  const u32 V = static_cast<u32>(vals.size());
  std::vector<u32> idx(cells, 0);
  Mat m;
  m.n = n;
  m.a.assign(cells, 0);
  u64 h = kFnvOff;
  count = 0;
  for (;;) {
    for (u32 i = 0; i < cells; ++i) m.a[i] = vals[idx[i]];
    ++count;
    const u64 want = brute(m);
    const Run r = run(m);
    REQUIRE(certified(m, r, want));
    h = fold_bytes(h, r.total, 8);
    u32 i = cells;
    while (i > 0 && idx[i - 1u] == V - 1u) {
      idx[i - 1u] = 0;
      --i;
    }
    if (i == 0) break;
    ++idx[i - 1u];
  }
  return h;
}

}  // namespace

namespace {

void group_check(u32 n, const std::vector<u32>& vals, u32 gi) {
  u32 count = 0;
  const u64 h = enumerate_group(n, vals, count);
  CHECK(count == kGroups[3u * gi]);
  CHECK(static_cast<u32>(h) == kGroups[3u * gi + 1u]);
  CHECK(static_cast<u32>(h >> 32) == kGroups[3u * gi + 2u]);
}

// cost = r[i] + c[j] + d[i][j], d zero along a random permutation; identical to tools/assign_ref.py.
Mat planted(Lcg& r, u32 n, u32 rmax, u32 cmax, u32 dmax) {
  std::vector<u32> perm(n);
  for (u32 i = 0; i < n; ++i) perm[i] = i;
  shuffle(r, perm);
  std::vector<u32> rr(n), cc(n);
  for (u32 i = 0; i < n; ++i) rr[i] = r.rnd(rmax);
  for (u32 j = 0; j < n; ++j) cc[j] = r.rnd(cmax);
  Mat m;
  m.n = n;
  for (u32 i = 0; i < n; ++i) {
    for (u32 j = 0; j < n; ++j) {
      const u32 d = (perm[i] == j) ? 0u : r.rnd(dmax + 1u);
      m.a.push_back(rr[i] + cc[j] + d);
    }
  }
  return m;
}

Mat big_matrix(u32 pid) {
  Lcg r{9100ull + pid};
  const u32 N = 512;
  Mat m;
  m.n = N;
  switch (pid) {
    case 0: m.a.assign(N * N, 0u); break;
    case 1: m.a.assign(N * N, kMaxV); break;
    case 2: m.a.assign(N * N, 1234567u); break;
    case 3:
      for (u32 i = 0; i < N; ++i) {
        for (u32 j = 0; j < N; ++j) m.a.push_back(i == j ? 0u : kMaxV);
      }
      break;
    case 4: {
      std::vector<u32> perm(N);
      for (u32 i = 0; i < N; ++i) perm[i] = i;
      shuffle(r, perm);
      for (u32 i = 0; i < N; ++i) {
        for (u32 j = 0; j < N; ++j) m.a.push_back(perm[i] == j ? 0u : rnd32(r));
      }
      break;
    }
    case 5: m = planted(r, N, 1u << 30, 1u << 30, (1u << 31) - 1u); break;
    case 6: {
      std::vector<u32> perm(N);
      for (u32 i = 0; i < N; ++i) perm[i] = i;
      shuffle(r, perm);
      std::vector<u32> rr(N), cc(N);
      for (u32 i = 0; i < N; ++i) rr[i] = (1u << 31) - 2000u + r.rnd(1000);
      for (u32 j = 0; j < N; ++j) cc[j] = (1u << 31) - 2000u + r.rnd(1000);
      for (u32 i = 0; i < N; ++i) {
        for (u32 j = 0; j < N; ++j) {
          const u32 d = (perm[i] == j) ? 0u : r.rnd(1000);
          m.a.push_back(rr[i] + cc[j] + d);
        }
      }
      break;
    }
    case 7: m = planted(r, N, 4, 4, 1); break;
    case 8: {
      std::vector<u32> rr(N);
      for (u32 i = 0; i < N; ++i) rr[i] = rnd32(r);
      for (u32 i = 0; i < N; ++i) {
        for (u32 j = 0; j < N; ++j) m.a.push_back(rr[i]);
      }
      break;
    }
    case 9: {
      std::vector<u32> cc(N);
      for (u32 j = 0; j < N; ++j) cc[j] = rnd32(r);
      for (u32 i = 0; i < N; ++i) {
        for (u32 j = 0; j < N; ++j) m.a.push_back(cc[j]);
      }
      break;
    }
    case 10:
      for (u32 i = 0; i < N; ++i) {
        for (u32 j = 0; j < N; ++j) m.a.push_back(j > i ? kMaxV : 0u);
      }
      break;
    default: m = planted(r, 300, 1u << 30, 1u << 30, (1u << 31) - 1u); break;
  }
  return m;
}

}  // namespace

TEST_CASE("assign_scratch_words", "[assign]") {
  u32 w = 7;
  CHECK(assign_scratch_words(0, &w) == AssignResult::Ok);
  CHECK(w == 16u);
  CHECK(assign_scratch_words(1, &w) == AssignResult::Ok);
  CHECK(w == 32u);
  CHECK(assign_scratch_words(5, &w) == AssignResult::Ok);
  CHECK(w == 96u);
  CHECK(assign_scratch_words(kMaxN, &w) == AssignResult::Ok);
  CHECK(w == 16u * 512u + 16u);
  w = 99;
  CHECK(assign_scratch_words(kMaxN + 1u, &w) == AssignResult::TooLarge);
  CHECK(w == 99u);
  CHECK(assign_scratch_words(0xFFFFFFFFu, &w) == AssignResult::TooLarge);
  CHECK(w == 99u);
  CHECK(assign_scratch_words(1, nullptr) == AssignResult::NullArgument);
  CHECK(assign_scratch_words(kMaxN + 1u, nullptr) == AssignResult::NullArgument);
}

TEST_CASE("every 1 x 1 matrix with an entry from 0, 1, 2, 4294967294, 4294967295 (5 matrices): digest group", "[assign]") {
  group_check(1, {0u, 1u, 2u, kMaxV - 1u, kMaxV}, 0);
}
TEST_CASE("every 2 x 2 matrix with entries from 0 to 5 (1296 matrices): digest group", "[assign]") { group_check(2, {0u, 1u, 2u, 3u, 4u, 5u}, 1); }
TEST_CASE("every 3 x 3 matrix with entries from 0 to 3 (262144 matrices): digest group", "[assign]") { group_check(3, {0u, 1u, 2u, 3u}, 2); }
TEST_CASE("every 4 x 4 matrix with entries from 0 and 1 (65536 matrices): digest group", "[assign]") { group_check(4, {0u, 1u}, 3); }
TEST_CASE("every 2 x 2 matrix with entries from 0, 1, 4294967294, 4294967295 (256 matrices): digest group", "[assign]") {
  group_check(2, {0u, 1u, kMaxV - 1u, kMaxV}, 4);
}
TEST_CASE("every 3 x 3 matrix with entries from 0, 1, 4294967294, 4294967295 (262144 matrices): digest group", "[assign]") {
  group_check(3, {0u, 1u, kMaxV - 1u, kMaxV}, 5);
}

TEST_CASE("random battery: 8000 matrices of side 1 to 6 by digest, also against the in-test brute force", "[assign]") {
  Lcg r{8181};
  u64 h = kFnvOff;
  for (u32 k = 0; k < 8000u; ++k) {
    Mat m;
    m.n = 1u + r.rnd(6);
    const u32 mode = r.rnd(4);
    for (u32 i = 0; i < m.n * m.n; ++i) m.a.push_back(entry(r, mode));
    const Run run1 = run(m);
    REQUIRE(certified(m, run1, brute(m)));
    h = fold_bytes(h, run1.total, 8);
  }
  CHECK(static_cast<u32>(h) == kBattery[0]);
  CHECK(static_cast<u32>(h >> 32) == kBattery[1]);
}

TEST_CASE("mid-size matrices (300 matrices of side 7 to 12): digest and certificate on every matrix", "[assign]") {
  Lcg r{8282};
  u64 h = kFnvOff;
  for (u32 k = 0; k < 300u; ++k) {
    Mat m;
    m.n = 7u + r.rnd(6);
    const u32 mode = r.rnd(4);
    for (u32 i = 0; i < m.n * m.n; ++i) m.a.push_back(entry(r, mode));
    const Run run1 = run(m);
    REQUIRE(run1.res == AssignResult::Ok);
    const u64 got = run1.total;
    REQUIRE(certified(m, run1, got));
    h = fold_bytes(h, got, 8);
  }
  CHECK(static_cast<u32>(h) == kMid[0]);
  CHECK(static_cast<u32>(h >> 32) == kMid[1]);
}

TEST_CASE("max-size matrices: 12 patterns (side 512, and 300) by the optimum known by construction, each checked by the certificate checker", "[assign]") {
  for (u32 pid = 0; pid < 12u; ++pid) {
    const u32* row = &kBig[4u * pid];
    CAPTURE(pid);
    REQUIRE(row[0] == pid);
    const Mat m = big_matrix(pid);
    REQUIRE(m.n == row[1]);
    const std::vector<u32> before = m.a;
    const Run r = run(m);
    REQUIRE(certified(m, r, (static_cast<u64>(row[3]) << 32) | row[2]));
    REQUIRE(m.a == before);
  }
}

TEST_CASE("hand-checked matrices", "[assign]") {
  auto go = [](u32 n, std::vector<u32> a, u64 want) {
    Mat m;
    m.n = n;
    m.a = a;
    const Run r = run(m);
    CHECK(certified(m, r, want));
  };
  go(1, {7}, 7);
  go(1, {0}, 0);
  go(1, {kMaxV}, kMaxV);
  go(2, {1, 2, 3, 4}, 5);                       // 1 + 4 = 5 and 2 + 3 = 5 tie
  go(2, {4, 1, 2, 3}, 3);                       // anti-diagonal wins
  go(2, {1, 100, 100, 1}, 2);
  go(2, {100, 1, 1, 100}, 2);
  go(2, {kMaxV, kMaxV, kMaxV, kMaxV}, 2ull * kMaxV);
  go(2, {0, kMaxV, kMaxV, 0}, 0);
  go(2, {kMaxV, 0, 0, kMaxV}, 0);
  go(3, {9, 2, 7, 6, 4, 3, 5, 8, 1}, 9ull);      // classic: 2 + 6 + 1 = 9
  go(3, {4, 1, 3, 2, 0, 5, 3, 2, 2}, 5ull);      // 1 + 2 + 2 = 5 (rows 0->1, 1->0, 2->2)
  go(3, {1, 1, 1, 1, 1, 1, 1, 1, 1}, 3);
  go(3, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0);
  go(3, {kMaxV, 0, kMaxV, kMaxV, kMaxV, 0, 0, kMaxV, kMaxV}, 0);   // a cyclic permutation of zeros
  go(3, {kMaxV, kMaxV, kMaxV, kMaxV, kMaxV, kMaxV, kMaxV, kMaxV, kMaxV}, 3ull * kMaxV);
  go(3, {5, 5, 5, 5, 5, 0, 5, 5, 5}, 10);          // one cheap cell, the other two rows pay 5 each
  go(4, {82, 83, 69, 92, 77, 37, 49, 92, 11, 69, 5, 86, 8, 9, 98, 23}, 140);
  go(4, {0, 1, 2, 3, 1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2}, 0);   // a Latin square with a permutation of zeros
}

TEST_CASE("argument rules and check order", "[assign]") {
  const u32 a[4] = {1, 2, 3, 4};
  std::vector<u32> sc(16 * 2 + 16, kGarbage);
  u32 col[2] = {kSentinel, kSentinel};
  u64 total = kSentinel64;
  const u32 W = static_cast<u32>(sc.size());
  auto untouched = [&]() { return col[0] == kSentinel && col[1] == kSentinel && total == kSentinel64; };
  auto reset = [&]() {
    col[0] = col[1] = kSentinel;
    total = kSentinel64;
  };
  CHECK(assign_run(2, a, sc.data(), W, &total, col) == AssignResult::Ok);
  CHECK(total == 5u);
  reset();
  // each pointer alone
  CHECK(assign_run(2, a, sc.data(), W, nullptr, col) == AssignResult::NullArgument);
  CHECK(assign_run(2, nullptr, sc.data(), W, &total, col) == AssignResult::NullArgument);
  CHECK(assign_run(2, a, nullptr, W, &total, col) == AssignResult::NullArgument);
  CHECK(assign_run(2, a, sc.data(), W, &total, nullptr) == AssignResult::NullArgument);
  CHECK(untouched());
  // n = 1 needs the same pointers
  u32 one[1] = {9};
  u32 c1[1] = {kSentinel};
  std::vector<u32> sc1(32, kGarbage);
  CHECK(assign_run(1, nullptr, sc1.data(), 32, &total, c1) == AssignResult::NullArgument);
  CHECK(assign_run(1, one, nullptr, 32, &total, c1) == AssignResult::NullArgument);
  CHECK(assign_run(1, one, sc1.data(), 32, &total, nullptr) == AssignResult::NullArgument);
  CHECK(assign_run(1, one, sc1.data(), 31, &total, c1) == AssignResult::ScratchTooSmall);
  CHECK(untouched());
  CHECK(c1[0] == kSentinel);
  CHECK(assign_run(1, one, sc1.data(), 32, &total, c1) == AssignResult::Ok);
  CHECK(total == 9u);
  CHECK(c1[0] == 0u);
  reset();
  // n = 0: only out_total is needed
  CHECK(assign_run(0, nullptr, nullptr, 0, nullptr, nullptr) == AssignResult::NullArgument);
  CHECK(assign_run(0, nullptr, nullptr, 0, &total, nullptr) == AssignResult::Ok);
  CHECK(total == 0u);
  total = kSentinel64;
  CHECK(assign_run(0, a, sc.data(), 0, &total, col) == AssignResult::Ok);
  CHECK(total == 0u);
  CHECK(col[0] == kSentinel);
  CHECK(col[1] == kSentinel);
  reset();
  // null beats TooLarge, TooLarge beats ScratchTooSmall; the given n is used above the limit
  CHECK(assign_run(kMaxN + 1u, nullptr, sc.data(), W, &total, col) == AssignResult::NullArgument);
  CHECK(assign_run(kMaxN + 1u, a, sc.data(), W, &total, col) == AssignResult::TooLarge);
  CHECK(assign_run(kMaxN + 1u, a, sc.data(), 0, &total, col) == AssignResult::TooLarge);
  CHECK(assign_run(0xFFFFFFFFu, a, sc.data(), 0, &total, col) == AssignResult::TooLarge);
  CHECK(untouched());
  // ScratchTooSmall by exactly one word, and with a size that does not fit 32 bits when multiplied
  CHECK(assign_run(2, a, sc.data(), W - 1u, &total, col) == AssignResult::ScratchTooSmall);
  CHECK(assign_run(2, a, sc.data(), 0, &total, col) == AssignResult::ScratchTooSmall);
  CHECK(untouched());
  // a scratch larger than needed gives the same result
  std::vector<u32> big_scratch(1000, 0xFFFFFFFFu);
  CHECK(assign_run(2, a, big_scratch.data(), 1000, &total, col) == AssignResult::Ok);
  CHECK(total == 5u);
}

TEST_CASE("a successful run writes only the total and out_col[0 .. n), and does not modify the matrix", "[assign]") {
  Mat m;
  m.n = 3;
  m.a = {9, 2, 7, 6, 4, 3, 5, 8, 1};
  const std::vector<u32> before = m.a;
  std::vector<u32> sc(16 * 3 + 16, kGarbage);
  std::vector<u32> col(6, kSentinel);
  u64 total = kSentinel64;
  CHECK(assign_run(3, m.a.data(), sc.data(), static_cast<u32>(sc.size()), &total, col.data()) == AssignResult::Ok);
  CHECK(total == 9u);
  CHECK(col[3] == kSentinel);
  CHECK(col[4] == kSentinel);
  CHECK(col[5] == kSentinel);
  CHECK(m.a == before);
  for (u32 fill : {0u, 0xFFFFFFFFu, 0x12345678u}) {
    const Run r = run(m, fill);
    CHECK(certified(m, r, 9u));
  }
}

TEST_CASE("relabelled matrices: permuted rows and columns, transposes and added row and column constants keep the minimum", "[assign]") {
  Lcg r{4343};
  for (u32 k = 0; k < 2000u; ++k) {
    Mat m;
    m.n = 1u + r.rnd(8);
    const u32 n = m.n;
    const u32 lim = 1u << 20;
    for (u32 i = 0; i < n * n; ++i) m.a.push_back(r.rnd(lim));
    const Run base = run(m);
    REQUIRE(base.res == AssignResult::Ok);
    const u64 want = base.total;
    REQUIRE(certified(m, base, want));
    std::vector<u32> pr(n), pc(n);
    for (u32 i = 0; i < n; ++i) pr[i] = pc[i] = i;
    shuffle(r, pr);
    shuffle(r, pc);
    Mat p;
    p.n = n;
    p.a.assign(n * n, 0);
    for (u32 i = 0; i < n; ++i) {
      for (u32 j = 0; j < n; ++j) p.a[pr[i] * n + pc[j]] = m.a[i * n + j];
    }
    REQUIRE(certified(p, run(p), want));
    Mat t;
    t.n = n;
    t.a.assign(n * n, 0);
    for (u32 i = 0; i < n; ++i) {
      for (u32 j = 0; j < n; ++j) t.a[j * n + i] = m.a[i * n + j];
    }
    REQUIRE(certified(t, run(t), want));
    Mat q = m;
    u64 add = 0;
    std::vector<u32> rr(n), cc(n);
    for (u32 i = 0; i < n; ++i) {
      rr[i] = r.rnd(lim);
      cc[i] = r.rnd(lim);
      add += rr[i];
      add += cc[i];
    }
    for (u32 i = 0; i < n; ++i) {
      for (u32 j = 0; j < n; ++j) q.a[i * n + j] += rr[i] + cc[j];
    }
    REQUIRE(certified(q, run(q), want + add));
  }
  // a max-size pattern with its rows reversed keeps its optimum
  Mat m = big_matrix(5);
  const u32 N = m.n;
  for (u32 i = 0; i < N / 2u; ++i) {
    for (u32 j = 0; j < N; ++j) std::swap(m.a[i * N + j], m.a[(N - 1u - i) * N + j]);
  }
  const u32* row = &kBig[4u * 5u];
  REQUIRE(certified(m, run(m), (static_cast<u64>(row[3]) << 32) | row[2]));
}

TEST_CASE("smallest sizes: sides 1 and 2 with largest entries", "[assign]") {
  Mat m;
  m.n = 1;
  m.a = {kMaxV};
  const Run r1 = run(m);
  CHECK(certified(m, r1, kMaxV));
  CHECK(r1.col[0] == 0u);
  Mat q;
  q.n = 2;
  q.a = {kMaxV, kMaxV - 1u, kMaxV - 1u, kMaxV};
  CHECK(certified(q, run(q), 2ull * (kMaxV - 1u)));
}

TEST_CASE("scratch that starts at an odd u32 boundary of a larger buffer, with exactly 16n + 16 words", "[assign]") {
  Lcg r{5151};
  for (u32 k = 0; k < 200u; ++k) {
    Mat m;
    m.n = 1u + r.rnd(9);
    for (u32 i = 0; i < m.n * m.n; ++i) m.a.push_back(r.rnd(1000));
    const u64 want = brute(m.n <= 6 ? m : Mat{1, {0}}) * (m.n <= 6 ? 1u : 0u);
    std::vector<u32> buf(16ull * m.n + 16ull + 1ull, kGarbage);
    std::vector<u32> col(m.n, kSentinel);
    u64 total = kSentinel64;
    REQUIRE(assign_run(m.n, m.a.data(), buf.data() + 1, 16u * m.n + 16u, &total, col.data()) == AssignResult::Ok);
    Run rr;
    rr.res = AssignResult::Ok;
    rr.total = total;
    rr.col = col;
    const Run ref = run(m);
    REQUIRE(total == ref.total);
    if (m.n <= 6) REQUIRE(total == want);
    REQUIRE(certified(m, rr, total));
  }
}

TEST_CASE("scratch filled with different random words (full 32-bit words, and per 64-bit pair either small, random or near 2^64, so potentials start unequal): same totals as the brute force and the garbage-fill run", "[assign]") {
  Lcg r{6262};
  for (u32 k = 0; k < 3000u; ++k) {
    Mat m;
    m.n = 1u + r.rnd(8);
    const u32 mode = r.rnd(4);
    for (u32 i = 0; i < m.n * m.n; ++i) m.a.push_back(entry(r, mode));
    std::vector<u32> sc(16ull * m.n + 16ull);
    const bool small = (k % 2u) == 1u;
    for (u32 q = 0; q + 1u < sc.size() + 1u; q += 2u) {
      u32 lo = rnd32(r), hi = rnd32(r);
      if (small) {
        const u32 t = r.rnd(3);
        if (t == 0) {
          lo = r.rnd(60);
          hi = 0;
        } else if (t == 2) {
          lo = 0xFFFFFFFFu - r.rnd(4);
          hi = 0xFFFFFFFFu - r.rnd(2);
        }
      }
      sc[q] = lo;
      if (q + 1u < sc.size()) sc[q + 1u] = hi;
    }
    std::vector<u32> col(m.n, kSentinel);
    u64 total = kSentinel64;
    REQUIRE(assign_run(m.n, m.a.data(), sc.data(), static_cast<u32>(sc.size()), &total, col.data()) == AssignResult::Ok);
    Run rr;
    rr.res = AssignResult::Ok;
    rr.total = total;
    rr.col = col;
    const Run ref = run(m);
    REQUIRE(total == ref.total);
    if (m.n <= 6) REQUIRE(certified(m, rr, brute(m)));
    REQUIRE(certified(m, rr, total));
  }
}
