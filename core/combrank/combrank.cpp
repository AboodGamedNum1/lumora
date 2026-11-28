// Ranking of k-subsets of {0, ..., n - 1} as u64 masks in the combinatorial number system (contract in combrank.h).
// Integer only; no tables, arrays, heap, recursion, mutable statics, 128-bit types, builtins or intrinsics. Every binomial is built or updated by an exact
// division before the multiplication (after cancelling a gcd), so no product is larger than the binomial it produces, and that is at most C(64, 32) < 2^61.
#include "combrank.h"

namespace lumora::combrank {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// The only multiplication of the file: every product in the binomial updates goes through here.
u64 mul(u64 a, u64 b) { return a * b; }

u64 gcd(u64 a, u64 b) {
  while (b != 0u) {
    const u64 t = a % b;
    a = b;
    b = t;
  }
  return a;
}

// C(n, k) for k at most n (n at most 64), by c_i = C(n - k + i, i) = c_{i-1} * (n - k + i) / i with k replaced by min(k, n - k). With g = gcd(c_{i-1}, i) the
// quotient (n - k + i) / (i / g) is exact, so c_i = (c_{i-1} / g) * ((n - k + i) / (i / g)) and the product is c_i itself.
u64 binom(u32 n, u32 k) {
  if (k > n) return 0u;
  if (k > n - k) k = n - k;
  u64 c = 1u;
  for (u32 i = 1u; i <= k; ++i) {
    const u64 m = static_cast<u64>(n - k) + i;
    const u64 g = gcd(c, i);
    c = mul(c / g, m / (i / g));
  }
  return c;
}

// From b = C(c, j) (c at least 1) to C(c - 1, j): C(c - 1, j) * c = C(c, j) * (c - j), so with g = gcd(b, c) the quotient (c - j) / (c / g) is exact.
u64 down(u64 b, u32 c, u32 j) {
  const u64 g = gcd(b, c);
  return mul(b / g, static_cast<u64>(c - j) / (c / g));
}

// From b = C(c, j) (c at least 1, j at least 1) to C(c - 1, j - 1) = C(c, j) * j / c.
u64 diag(u64 b, u32 c, u32 j) {
  const u64 g = gcd(b, c);
  return mul(b / g, static_cast<u64>(j) / (c / g));
}

u32 popcount(u64 m) {
  u32 n = 0u;
  while (m != 0u) {
    m &= m - 1u;
    ++n;
  }
  return n;
}

bool mask_ok(u64 mask, u32 n, u32 k) {
  if (n < 64u && (mask >> n) != 0u) return false;
  return popcount(mask) == k;
}
}  // namespace

CbResult combo_count(std::uint32_t n, std::uint32_t k, std::uint64_t* out) {
  if (out == nullptr) return CbResult::NullArgument;
  if (n > kMaxN || k > n) return CbResult::BadArgument;
  *out = binom(n, k);
  return CbResult::Ok;
}

CbResult combo_rank(std::uint64_t mask, std::uint32_t n, std::uint32_t k, std::uint64_t* out) {
  if (out == nullptr) return CbResult::NullArgument;
  if (n > kMaxN || k > n) return CbResult::BadArgument;
  if (!mask_ok(mask, n, k)) return CbResult::BadMask;
  u64 sum = 0u;
  if (k > 0u) {
    u32 c = n - 1u;
    u32 j = k;
    u64 b = binom(c, j);  // C(c, j)
    for (;;) {
      if (((mask >> c) & 1u) != 0u) {
        sum += b;
        if (j == 1u) break;
        b = diag(b, c, j);
        --j;
      } else {
        b = down(b, c, j);
      }
      --c;
    }
  }
  *out = sum;
  return CbResult::Ok;
}

CbResult combo_unrank(std::uint32_t n, std::uint32_t k, std::uint64_t rank, std::uint64_t* out) {
  if (out == nullptr) return CbResult::NullArgument;
  if (n > kMaxN || k > n) return CbResult::BadArgument;
  if (rank >= binom(n, k)) return CbResult::RankTooLarge;
  u64 mask = 0u;
  if (k > 0u) {
    u32 c = n - 1u;
    u32 j = k;
    u64 b = binom(c, j);  // C(c, j)
    for (;;) {
      while (b > rank) {
        b = down(b, c, j);
        --c;
      }
      mask |= u64{1} << c;
      rank -= b;
      if (j == 1u) break;
      b = diag(b, c, j);
      --c;
      --j;
    }
  }
  *out = mask;
  return CbResult::Ok;
}

CbResult combo_next(std::uint64_t mask, std::uint32_t n, std::uint32_t k, std::uint64_t* out) {
  if (out == nullptr) return CbResult::NullArgument;
  if (n > kMaxN || k > n) return CbResult::BadArgument;
  if (!mask_ok(mask, n, k)) return CbResult::BadMask;
  if (k == 0u) return CbResult::NoNext;
  const u64 last = ((~u64{0}) >> (64u - k)) << (n - k);  // the top k bits of n: the last mask
  if (mask == last) return CbResult::NoNext;
  const u64 low = mask & (~mask + 1u);
  const u64 r = mask + low;  // cannot wrap: the lowest run of ones would have to reach bit 63, and then mask is last
  *out = r | (((mask ^ r) >> 2) / low);
  return CbResult::Ok;
}
}  // namespace lumora::combrank
