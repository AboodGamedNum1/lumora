// Tests for core/cartree. Evidence label: unit tests only.
// Expected trees come from an in-test definitional brute force (the leftmost minimum of each range found by a linear scan, ranges kept on an explicit
// stack; not the loop of the implementation), from an in-test nearest-smaller-value construction (monotonic stacks, a different shape again), and from
// tools/cartree_ref.py (standard library only: the same definition as THE oracle, the nearest-smaller shape as the second method, and validators) as
// FNV-1a digests over 32-bit words (kNone as 0xFFFFFFFF; parent[0..n-1], left[0..n-1], right[0..n-1], root per tree). Extents, named in the test names and in
// docs/cartree.md:
//   exhaustive vs the in-test brute: every array of length 0..8 with values below its length (17650829 arrays; any array is order isomorphic to one of these
//               for its length, so every tie pattern up to length 8 occurs); every array over {0,1,2} of length 0..13 (2391484); every array over {0,1} of
//               length 0..20 (2097151); every permutation of length 0..9 (409114);
//   exhaustive vs reference digests: every array of length 0..6 with values below its length (50070), every array over {0,1,2} of length 0..10 (88573),
//               every permutation of length 0..8 (46234), each as one digest in the stated enumeration order (last position fastest);
//   sampled (large): 9 generators times n in {1, 2, 3, 1000, 65535, 65536, 524289, 1048575, 1048576}, each validated in the test (O(n): consistency, heap order
//               with the tie rule, in-order walk 0..n-1, one root), compared with the in-test nearest-smaller tree, and with a reference digest.
// Every success run uses output arrays with guard words before and after each array and a garbage fill, and the input is compared with a copy afterwards.
// No test asserts anything about time.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "cartree.h"

using lumora::cartree::cartesian_tree;
using lumora::cartree::CartResult;
using lumora::cartree::kMaxN;
using lumora::cartree::kNone;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;

constexpr std::uint64_t kDigest_all_below_n_len_0_6 = 0x3af28b9764c8acacull;  // 50070 arrays
constexpr std::uint64_t kDigest_alphabet3_len_0_10 = 0x637b344507a74471ull;  // 88573 arrays
constexpr std::uint64_t kDigest_permutations_0_8 = 0xac8477bbd7a92f84ull;  // 46234 arrays
constexpr std::uint64_t kLargeDigest[9][9] = {  // [kind][n index]: kinds equal asc desc saw two extremes perm organ full; n 1 2 3 1000 65535 65536 524289 1048575 1048576
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0x51b4af8ffb710cecull, 0xc42ca313e8b022f0ull, 0x1d2d290243c19b9cull, 0xb109ccf461cf64f8ull, 0x8f5a1c5fd2a94afcull, 0x9f360d86f67b9b9cull, 0xac07fb6080fb64f8ull},  // equal
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0x51b4af8ffb710cecull, 0xc42ca313e8b022f0ull, 0x1d2d290243c19b9cull, 0xb109ccf461cf64f8ull, 0x8f5a1c5fd2a94afcull, 0x9f360d86f67b9b9cull, 0xac07fb6080fb64f8ull},  // asc
    {0xe0387a8510e14afcull, 0x3beb4606da3b10d7ull, 0x34215d05b167c078ull, 0x1236a8397f7e8fe5ull, 0x7a5c5390010d62b0ull, 0x4bdb7d8e80109955ull, 0x6b3f5a5197794afcull, 0xa57ac2b3e41462b0ull, 0x001bd0513d249955ull},  // desc
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0x51b4af8ffb710cecull, 0xc42ca313e8b022f0ull, 0x180614ef6674b9b1ull, 0x04f12904923e1ad7ull, 0xb71b31612a7145acull, 0x68552e7a0f6c083cull, 0xb9bcb4757300a818ull},  // saw
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0x51b4af8ffb710cecull, 0x78fdf00b60969b02ull, 0xdcc61a4e9618fe28ull, 0xd9e9dbb8a054d262ull, 0x286564125170ae1aull, 0xc3dfbadbd9e6d518ull, 0xa66d980047118757ull},  // two
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0x51b4af8ffb710cecull, 0x78fdf00b60969b02ull, 0xdcc61a4e9618fe28ull, 0xd9e9dbb8a054d262ull, 0x286564125170ae1aull, 0xc3dfbadbd9e6d518ull, 0xa66d980047118757ull},  // extremes
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0xd9825904c5050dadull, 0x3d401554603b3391ull, 0x7af56a5a3b28ef25ull, 0x80846187137973a9ull, 0x2f45d1968a2ea4a6ull, 0xb876d6ee251a6a2full, 0x5e5416d9deaaf026ull},  // perm
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0xd9825904c5050dadull, 0xdab9dc98beb0a0ebull, 0xcb5c8155ffc92f65ull, 0x1ca86a22c4c7b7dfull, 0x99336e58bd554afcull, 0x2dcc8ff7b2322f65ull, 0x29c1e03991b1b7dfull},  // organ
    {0xe0387a8510e14afcull, 0xb7bfae44dc56addcull, 0xd9825904c5050dadull, 0xac73d3a8a76596e4ull, 0x37adfa9357d05cc7ull, 0x5b4aedd3c2924de0ull, 0x80b544d7193fab01ull, 0x736fd416c6a080fdull, 0x75dec4f64424485eull},  // full
};

constexpr u32 kG = 3u;  // guard words before and after each output array
constexpr i32 kSent = 0x5A5A5A5A;

u64 fnv(u64 h, u32 w) { return (h ^ static_cast<u64>(w)) * 0x100000001b3ull; }
constexpr u64 kInit = 0xcbf29ce484222325ull;

struct Tree {
  std::vector<i32> p, l, r;
  i32 root = kNone;
};
bool same(const Tree& x, const Tree& y) { return x.root == y.root && x.p == y.p && x.l == y.l && x.r == y.r; }
u64 fold(u64 h, const Tree& t) {
  for (i32 v : t.p) h = fnv(h, static_cast<u32>(v));
  for (i32 v : t.l) h = fnv(h, static_cast<u32>(v));
  for (i32 v : t.r) h = fnv(h, static_cast<u32>(v));
  return fnv(h, static_cast<u32>(t.root));
}

// Runs the implementation on a[0 .. n-1]: outputs get a garbage fill and guard words; reports false on a guard hit, a changed input or a bad result code.
bool run_impl(const std::vector<u32>& a, i32 fill, Tree& out) {
  const u32 n = static_cast<u32>(a.size());
  std::vector<i32> pb(n + 2u * kG, kSent), lb(n + 2u * kG, kSent), rb(n + 2u * kG, kSent);
  for (u32 i = 0; i < n; ++i) pb[kG + i] = lb[kG + i] = rb[kG + i] = fill;
  i32 root = fill;
  const std::vector<u32> copy = a;
  const CartResult r = cartesian_tree(a.empty() ? nullptr : a.data(), n, pb.data() + kG, lb.data() + kG, rb.data() + kG, &root);
  bool ok = (r == CartResult::Ok) && (copy == a);
  for (u32 g = 0; g < kG; ++g) {
    ok = ok && (pb[g] == kSent) && (lb[g] == kSent) && (rb[g] == kSent);
    ok = ok && (pb[kG + n + g] == kSent) && (lb[kG + n + g] == kSent) && (rb[kG + n + g] == kSent);
  }
  out.p.assign(pb.begin() + kG, pb.begin() + kG + n);
  out.l.assign(lb.begin() + kG, lb.begin() + kG + n);
  out.r.assign(rb.begin() + kG, rb.begin() + kG + n);
  out.root = root;
  return ok;
}

// Definition: the root of a range is its leftmost minimum; the two parts of the range on either side form the subtrees. Explicit stack of ranges.
void brute(const u32* a, u32 n, Tree& t) {
  t.p.assign(n, kNone);
  t.l.assign(n, kNone);
  t.r.assign(n, kNone);
  t.root = kNone;
  if (n == 0u) return;
  struct Range {
    i32 lo, hi, par;
    bool is_left;
  };
  std::vector<Range> todo;
  todo.push_back({0, static_cast<i32>(n) - 1, kNone, false});
  while (!todo.empty()) {
    const Range g = todo.back();
    todo.pop_back();
    i32 m = g.lo;
    for (i32 i = g.lo + 1; i <= g.hi; ++i) {
      if (a[i] < a[m]) m = i;  // strict: the leftmost minimum stays
    }
    t.p[m] = g.par;
    if (g.par == kNone) {
      t.root = m;
    } else if (g.is_left) {
      t.l[g.par] = m;
    } else {
      t.r[g.par] = m;
    }
    if (g.lo <= m - 1) todo.push_back({g.lo, m - 1, m, true});
    if (m + 1 <= g.hi) todo.push_back({m + 1, g.hi, m, false});
  }
}

// Nearest-smaller shape: L(i) = nearest j < i with a[j] <= a[i], R(i) = nearest j > i with a[j] < a[i]; the parent is the one with the larger value
// (R when the values are equal) or the only one that exists.
void nearest_smaller(const std::vector<u32>& a, Tree& t) {
  const u32 n = static_cast<u32>(a.size());
  std::vector<i32> L(n, kNone), R(n, kNone), st;
  st.reserve(n);
  for (u32 i = 0; i < n; ++i) {
    while (!st.empty() && a[st.back()] > a[i]) st.pop_back();
    L[i] = st.empty() ? kNone : st.back();
    st.push_back(static_cast<i32>(i));
  }
  st.clear();
  for (u32 k = n; k > 0u; --k) {
    const u32 i = k - 1u;
    while (!st.empty() && a[st.back()] >= a[i]) st.pop_back();
    R[i] = st.empty() ? kNone : st.back();
    st.push_back(static_cast<i32>(i));
  }
  t.p.assign(n, kNone);
  t.l.assign(n, kNone);
  t.r.assign(n, kNone);
  t.root = kNone;
  for (u32 i = 0; i < n; ++i) {
    i32 par = kNone;
    if (L[i] != kNone && R[i] != kNone) {
      par = (a[L[i]] > a[R[i]]) ? L[i] : R[i];
    } else if (L[i] != kNone) {
      par = L[i];
    } else {
      par = R[i];
    }
    t.p[i] = par;
    if (par == kNone) {
      t.root = static_cast<i32>(i);
    } else if (par < static_cast<i32>(i)) {
      t.r[par] = static_cast<i32>(i);
    } else {
      t.l[par] = static_cast<i32>(i);
    }
  }
}

// O(n) validator: one root without a parent, parent/left/right agree, children on the correct side, heap order with the tie rule (a left child is strictly
// greater than its parent, a right child greater or equal), and the in-order walk visits 0 .. n-1 once each.
bool validate(const std::vector<u32>& a, const Tree& t) {
  const i32 n = static_cast<i32>(a.size());
  if (n == 0) return t.root == kNone;
  if (t.root < 0 || t.root >= n || t.p[t.root] != kNone) return false;
  i32 roots = 0;
  for (i32 i = 0; i < n; ++i) {
    const i32 par = t.p[i];
    if (par == kNone) {
      ++roots;
    } else {
      if (par < 0 || par >= n) return false;
      const bool is_l = (t.l[par] == i), is_r = (t.r[par] == i);
      if (is_l == is_r) return false;
      if (is_l && !(i < par && a[i] > a[par])) return false;
      if (is_r && !(i > par && a[i] >= a[par])) return false;
    }
    if (t.l[i] != kNone && !(t.l[i] >= 0 && t.l[i] < i && t.p[t.l[i]] == i)) return false;
    if (t.r[i] != kNone && !(t.r[i] > i && t.r[i] < n && t.p[t.r[i]] == i)) return false;
  }
  if (roots != 1) return false;
  std::vector<i32> st;
  i32 cur = t.root, next = 0;
  while (!st.empty() || cur != kNone) {
    while (cur != kNone) {
      st.push_back(cur);
      cur = t.l[cur];
      if (st.size() > static_cast<std::size_t>(n)) return false;
    }
    cur = st.back();
    st.pop_back();
    if (cur != next) return false;
    ++next;
    cur = t.r[cur];
  }
  return next == n;
}

// Compare the implementation with the brute force on one array (fill varies with the call).
bool check_one(const std::vector<u32>& a, i32 fill) {
  Tree got, want;
  const bool ok = run_impl(a, fill, got);
  brute(a.data(), static_cast<u32>(a.size()), want);
  return ok && same(got, want);
}


// Allocation-free variant for the exhaustive loops (n <= 21): the same checks as run_impl and brute, on fixed arrays.
constexpr u32 kSmall = 24u;
struct SmallTree {
  i32 p[kSmall], l[kSmall], r[kSmall];
  i32 root;
};
void brute_small(const u32* a, u32 n, SmallTree& t) {
  for (u32 i = 0; i < n; ++i) t.p[i] = t.l[i] = t.r[i] = kNone;
  t.root = kNone;
  if (n == 0u) return;
  struct Range {
    i32 lo, hi, par;
    bool is_left;
  };
  Range todo[2u * kSmall];
  u32 top = 0;
  todo[top++] = {0, static_cast<i32>(n) - 1, kNone, false};
  while (top > 0u) {
    const Range g = todo[--top];
    i32 m = g.lo;
    for (i32 i = g.lo + 1; i <= g.hi; ++i) {
      if (a[i] < a[m]) m = i;
    }
    t.p[m] = g.par;
    if (g.par == kNone) {
      t.root = m;
    } else if (g.is_left) {
      t.l[g.par] = m;
    } else {
      t.r[g.par] = m;
    }
    if (g.lo <= m - 1) todo[top++] = {g.lo, m - 1, m, true};
    if (m + 1 <= g.hi) todo[top++] = {m + 1, g.hi, m, false};
  }
}
bool check_small(const std::vector<u32>& a, i32 fill) {
  const u32 n = static_cast<u32>(a.size());
  i32 pb[kSmall + 2u * kG], lb[kSmall + 2u * kG], rb[kSmall + 2u * kG];
  for (u32 i = 0; i < kSmall + 2u * kG; ++i) pb[i] = lb[i] = rb[i] = kSent;
  for (u32 i = 0; i < n; ++i) pb[kG + i] = lb[kG + i] = rb[kG + i] = fill;
  u32 copy[kSmall];
  for (u32 i = 0; i < n; ++i) copy[i] = a[i];
  i32 root = fill;
  const CartResult r = cartesian_tree(a.data(), n, pb + kG, lb + kG, rb + kG, &root);
  bool ok = (r == CartResult::Ok);
  for (u32 i = 0; i < n; ++i) ok = ok && (copy[i] == a[i]);
  for (u32 g = 0; g < kG; ++g) {
    ok = ok && (pb[g] == kSent) && (lb[g] == kSent) && (rb[g] == kSent);
    ok = ok && (pb[kG + n + g] == kSent) && (lb[kG + n + g] == kSent) && (rb[kG + n + g] == kSent);
  }
  SmallTree want;
  brute_small(a.data(), n, want);
  ok = ok && (root == want.root);
  for (u32 i = 0; i < n; ++i) ok = ok && (pb[kG + i] == want.p[i]) && (lb[kG + i] == want.l[i]) && (rb[kG + i] == want.r[i]);
  return ok;
}

struct Gen32 {
  u32 x;
  explicit Gen32(u32 seed) : x(seed == 0u ? 1u : seed) {}
  u32 next() {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }
};
enum Kind { kEqual, kAsc, kDesc, kSaw, kTwo, kExtremes, kPerm, kOrgan, kFull, kKinds };
const char* const kKindNames[] = {"equal", "asc", "desc", "saw", "two", "extremes", "perm", "organ", "full"};
constexpr u32 kLargeN[9] = {1u, 2u, 3u, 1000u, 65535u, 65536u, 524289u, 1048575u, 1048576u};

// Generators documented in tools/cartree_ref.py (seed 0x9E3779B9 + n).
std::vector<u32> generate(int kind, u32 n) {
  std::vector<u32> a(n);
  Gen32 g(0x9E3779B9u + n);
  switch (kind) {
    case kEqual:
      for (u32 i = 0; i < n; ++i) a[i] = 7u;
      break;
    case kAsc:
      for (u32 i = 0; i < n; ++i) a[i] = i;
      break;
    case kDesc:
      for (u32 i = 0; i < n; ++i) a[i] = n - 1u - i;
      break;
    case kSaw:
      for (u32 i = 0; i < n; ++i) a[i] = i % 1000u;
      break;
    case kTwo:
      for (u32 i = 0; i < n; ++i) a[i] = g.next() >> 31;
      break;
    case kExtremes:
      for (u32 i = 0; i < n; ++i) a[i] = ((g.next() >> 31) != 0u) ? 0xFFFFFFFFu : 0u;
      break;
    case kPerm:
      for (u32 i = 0; i < n; ++i) a[i] = i;
      for (u32 i = n; i > 1u; --i) {
        const u32 top = i - 1u;
        const u32 j = g.next() % (top + 1u);
        std::swap(a[top], a[j]);
      }
      break;
    case kOrgan:
      for (u32 i = 0; i < n; ++i) a[i] = std::min(i, n - 1u - i);
      break;
    default:
      for (u32 i = 0; i < n; ++i) a[i] = g.next();
      break;
  }
  return a;
}

// Odometer over arrays of length n with digits below base (last position fastest). Returns false when it wraps.
bool bump(std::vector<u32>& a, u32 base) {
  for (std::size_t k = a.size(); k > 0; --k) {
    if (++a[k - 1] < base) return true;
    a[k - 1] = 0u;
  }
  return false;
}
}  // namespace

TEST_CASE("cartree: failure cases, check order, nothing written (every output pre-filled with a sentinel)", "[cartree]") {
  const u32 data[4] = {3u, 1u, 2u, 1u};
  const u32 bad_ns[] = {kMaxN + 1u, kMaxN + 2u, 0x80000000u, 0xFFFFFFFFu};
  // Pointers handed to failing calls may be tiny: a failing call must not touch them.
  for (int mask = 0; mask < 32; ++mask) {  // bit 0: a null, 1: parent null, 2: left null, 3: right null, 4: root null
    for (u32 n : {0u, 1u, 2u, 4u, kMaxN, kMaxN + 1u, 0xFFFFFFFFu}) {
      i32 pb[4] = {kSent, kSent, kSent, kSent}, lb[4] = {kSent, kSent, kSent, kSent}, rb[4] = {kSent, kSent, kSent, kSent};
      i32 root = kSent;
      const u32 call_n = n;
      const u32* pa = ((mask & 1) != 0) ? nullptr : data;
      i32* pp = ((mask & 2) != 0) ? nullptr : pb;
      i32* pl = ((mask & 4) != 0) ? nullptr : lb;
      i32* pr = ((mask & 8) != 0) ? nullptr : rb;
      i32* proot = ((mask & 16) != 0) ? nullptr : &root;
      CartResult want = CartResult::Ok;
      if (proot == nullptr) {
        want = CartResult::NullArgument;
      } else if (call_n > kMaxN) {
        want = CartResult::TooLarge;
      } else if (call_n > 0u && (pa == nullptr || pp == nullptr || pl == nullptr || pr == nullptr)) {
        want = CartResult::NullArgument;
      }
      if (want == CartResult::Ok && call_n > 4u) continue;  // a successful call needs real arrays: covered by the large cases
      const CartResult got = cartesian_tree(pa, call_n, pp, pl, pr, proot);
      CHECK(got == want);
      bool untouched = true;
      for (int i = 0; i < 4; ++i) untouched = untouched && (pb[i] == kSent) && (lb[i] == kSent) && (rb[i] == kSent);
      if (want == CartResult::Ok && call_n == 0u) {
        CHECK(root == kNone);  // n = 0: only the root is written
        CHECK(untouched);
      } else if (want == CartResult::Ok) {
        CHECK((root >= 0 && root < static_cast<i32>(call_n)));  // a real tree was written
      } else {
        CHECK(root == kSent);
        CHECK(untouched);
      }
    }
  }
  (void)bad_ns;
  // TooLarge with null and with non-null arrays, and with a real a and real outputs.
  for (u32 n : bad_ns) {
    i32 pb[4] = {kSent, kSent, kSent, kSent}, lb[4] = {kSent, kSent, kSent, kSent}, rb[4] = {kSent, kSent, kSent, kSent};
    i32 root = kSent;
    CHECK(cartesian_tree(data, n, pb, lb, rb, &root) == CartResult::TooLarge);
    CHECK(cartesian_tree(nullptr, n, nullptr, nullptr, nullptr, &root) == CartResult::TooLarge);
    CHECK(cartesian_tree(data, n, pb, lb, rb, nullptr) == CartResult::NullArgument);
    bool untouched = (root == kSent);
    for (int i = 0; i < 4; ++i) untouched = untouched && (pb[i] == kSent) && (lb[i] == kSent) && (rb[i] == kSent);
    CHECK(untouched);
  }
}

TEST_CASE("cartree: hand-checked trees and ties (leftmost minimum is the root)", "[cartree]") {
  struct Case {
    std::vector<u32> a;
    std::vector<i32> p, l, r;
    i32 root;
  };
  const std::vector<Case> cases = {
      {{2, 1, 1, 3}, {1, -1, 1, 2}, {-1, 0, -1, -1}, {-1, 2, 3, -1}, 1},
      {{5, 5, 5, 5, 5}, {-1, 0, 1, 2, 3}, {-1, -1, -1, -1, -1}, {1, 2, 3, 4, -1}, 0},
      {{1, 1, 0, 1, 1}, {2, 0, -1, 2, 3}, {-1, -1, 0, -1, -1}, {1, -1, 3, 4, -1}, 2},
      {{0, 1, 1, 1}, {-1, 0, 1, 2}, {-1, -1, -1, -1}, {1, 2, 3, -1}, 0},
      {{1, 1, 1, 0}, {3, 0, 1, -1}, {-1, -1, -1, 0}, {1, 2, -1, -1}, 3},
      {{5, 5, 5, 4, 5}, {3, 0, 1, -1, 3}, {-1, -1, -1, 0, -1}, {1, 2, -1, 4, -1}, 3},
      {{0, 1, 2}, {-1, 0, 1}, {-1, -1, -1}, {1, 2, -1}, 0},
      {{2, 1, 0}, {1, 2, -1}, {-1, 0, 1}, {-1, -1, -1}, 2},
      {{3, 1, 2}, {1, -1, 1}, {-1, 0, -1}, {-1, 2, -1}, 1},
      {{9}, {-1}, {-1}, {-1}, 0},
      {{1, 1}, {-1, 0}, {-1, -1}, {1, -1}, 0},
      {{2, 1}, {1, -1}, {-1, 0}, {-1, -1}, 1},
      {{0xFFFFFFFFu, 0u, 0xFFFFFFFFu}, {1, -1, 1}, {-1, 0, -1}, {-1, 2, -1}, 1},
  };
  for (const Case& c : cases) {
    for (i32 fill : {0, -1, 0x7FFFFFFF, 12345}) {
      Tree t;
      CHECK(run_impl(c.a, fill, t));
      CHECK((t.p == c.p && t.l == c.l && t.r == c.r && t.root == c.root));
    }
  }
  // n = 0: success, root = kNone, arrays not touched.
  i32 root = kSent;
  i32 cell = kSent;
  CHECK(cartesian_tree(nullptr, 0u, nullptr, nullptr, nullptr, &root) == CartResult::Ok);
  CHECK(root == kNone);
  root = kSent;
  CHECK(cartesian_tree(nullptr, 0u, &cell, &cell, &cell, &root) == CartResult::Ok);
  CHECK((root == kNone && cell == kSent));
}

TEST_CASE("cartree: garbage fills, input unchanged, guard words (random arrays n 1 to 40)", "[cartree]") {
  Gen32 g(12345u);
  u32 bad = 0;
  for (u32 round = 0; round < 3000u; ++round) {
    const u32 n = 1u + g.next() % 40u;
    const u32 base = 1u + g.next() % 6u;
    std::vector<u32> a(n);
    for (u32 i = 0; i < n; ++i) a[i] = g.next() % base;
    for (i32 fill : {0, -1, 0x7FFFFFFF, 0x12345678, kSent}) {
      if (!check_one(a, fill)) ++bad;
    }
  }
  CHECK(bad == 0u);
}

TEST_CASE("cartree: exhaustive every array of length 0 to 8 with values below its length against the brute force (17650829 arrays)", "[cartree][exhaustive][slow]") {
  u64 count = 0, bad = 0;
  for (u32 n = 0; n <= 8u; ++n) {
    std::vector<u32> a(n, 0u);
    do {
      ++count;
      if (!check_small(a, -1)) ++bad;
    } while (n > 0u && bump(a, n));
  }
  CHECK(count == 17650829u);
  CHECK(bad == 0u);
}

TEST_CASE("cartree: exhaustive every array over {0,1,2} of length 0 to 13 and over {0,1} of length 0 to 20 against the brute force", "[cartree][exhaustive][slow]") {
  u64 count3 = 0, count2 = 0, bad = 0;
  for (u32 n = 0; n <= 13u; ++n) {
    std::vector<u32> a(n, 0u);
    do {
      ++count3;
      if (!check_small(a, 0)) ++bad;
    } while (n > 0u && bump(a, 3u));
  }
  for (u32 n = 0; n <= 20u; ++n) {
    std::vector<u32> a(n, 0u);
    do {
      ++count2;
      if (!check_small(a, -1)) ++bad;
    } while (n > 0u && bump(a, 2u));
  }
  CHECK(count3 == 2391484u);
  CHECK(count2 == 2097151u);
  CHECK(bad == 0u);
}

TEST_CASE("cartree: exhaustive every permutation of length 0 to 9 against the brute force (409114 permutations)", "[cartree][exhaustive]") {
  u64 count = 0, bad = 0;
  for (u32 n = 0; n <= 9u; ++n) {
    std::vector<u32> a(n);
    for (u32 i = 0; i < n; ++i) a[i] = i;
    do {
      ++count;
      if (!check_small(a, 0x7FFFFFFF)) ++bad;
    } while (std::next_permutation(a.begin(), a.end()));
  }
  CHECK(count == 409114u);
  CHECK(bad == 0u);
}

TEST_CASE("cartree: exhaustive reference digests: arrays of length 0 to 6 below their length, {0,1,2} length 0 to 10, permutations 0 to 8", "[cartree][exhaustive]") {
  u64 h1 = kInit, h2 = kInit, h3 = kInit, c1 = 0, c2 = 0, c3 = 0;
  bool ok = true;
  for (u32 n = 0; n <= 6u; ++n) {
    std::vector<u32> a(n, 0u);
    do {
      Tree t;
      ok = run_impl(a, -1, t) && ok;
      h1 = fold(h1, t);
      ++c1;
    } while (n > 0u && bump(a, n));
  }
  for (u32 n = 0; n <= 10u; ++n) {
    std::vector<u32> a(n, 0u);
    do {
      Tree t;
      ok = run_impl(a, 0, t) && ok;
      h2 = fold(h2, t);
      ++c2;
    } while (n > 0u && bump(a, 3u));
  }
  for (u32 n = 0; n <= 8u; ++n) {
    std::vector<u32> a(n);
    for (u32 i = 0; i < n; ++i) a[i] = i;
    do {
      Tree t;
      ok = run_impl(a, 7, t) && ok;
      h3 = fold(h3, t);
      ++c3;
    } while (std::next_permutation(a.begin(), a.end()));
  }
  CHECK(ok);
  CHECK((c1 == 50070u && c2 == 88573u && c3 == 46234u));
  CHECK(h1 == kDigest_all_below_n_len_0_6);
  CHECK(h2 == kDigest_alphabet3_len_0_10);
  CHECK(h3 == kDigest_permutations_0_8);
}

TEST_CASE("cartree: sampled large n 1 to 2^20, 9 generators, validator, nearest-smaller tree and reference digest", "[cartree][sampled]") {
  for (int kind = 0; kind < kKinds; ++kind) {
    for (u32 ni = 0; ni < 9u; ++ni) {
      const u32 n = kLargeN[ni];
      const std::vector<u32> a = generate(kind, n);
      Tree got, ns;
      const bool ok = run_impl(a, -1, got);
      nearest_smaller(a, ns);
      INFO(kKindNames[kind] << " n=" << n);
      CHECK(ok);
      CHECK(validate(a, got));
      CHECK(same(got, ns));
      CHECK(fold(kInit, got) == kLargeDigest[kind][ni]);
    }
  }
}

TEST_CASE("cartree: n = 2^20 exact, ties and extremes, 2^19 + 1", "[cartree][sampled]") {
  // all equal: a right chain; all but one equal (the minimum last, first and in the middle).
  const u32 n = kMaxN;
  std::vector<u32> a(n, 5u);
  Tree t;
  REQUIRE(run_impl(a, -1, t));
  CHECK(t.root == 0);
  bool chain = (t.p[0] == kNone);
  for (u32 i = 1; i < n; ++i) chain = chain && (t.p[i] == static_cast<i32>(i) - 1) && (t.l[i] == kNone);
  for (u32 i = 0; i + 1u < n; ++i) chain = chain && (t.r[i] == static_cast<i32>(i) + 1);
  CHECK(chain);
  CHECK(t.r[n - 1u] == kNone);
  for (u32 pos : {0u, n / 2u, n - 1u}) {
    std::vector<u32> b(n, 5u);
    b[pos] = 4u;
    Tree u, w;
    REQUIRE(run_impl(b, 0, u));
    nearest_smaller(b, w);
    CHECK(same(u, w));
    CHECK(validate(b, u));
    CHECK(u.root == static_cast<i32>(pos));
  }
  std::vector<u32> c(524289u);
  for (u32 i = 0; i < c.size(); ++i) c[i] = (i % 2u == 0u) ? 0u : 0xFFFFFFFFu;
  Tree x, y;
  REQUIRE(run_impl(c, 123, x));
  nearest_smaller(c, y);
  CHECK(same(x, y));
  CHECK(validate(c, x));
}
