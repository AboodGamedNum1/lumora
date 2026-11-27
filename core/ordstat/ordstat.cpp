// Order statistics: k-th smallest value and inversion count. See ordstat.h for the contract. Integer only: no allocation, no recursion, no
// global state. Selection is an iterative quickselect with a three-way partition and a pseudo-random pivot from a local generator, so it is
// linear in expectation on every input and does not degrade on sorted, reversed or all-equal arrays. It is not worst-case linear against an
// adversary who knows the generator seed. The inversion count is a bottom-up merge sort over a copy of the input held in the scratch.
#include "ordstat.h"

namespace lumora::ordstat {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

u32 median3(u32 x, u32 y, u32 z) {
  if (x < y) {
    if (y < z) return y;
    return (x < z) ? z : x;
  }
  if (x < z) return x;
  return (y < z) ? z : y;
}

}  // namespace

OrdResult select_kth_u32(u32* a, u32 n, u32 k, u32* out) {
  if (out == nullptr) return OrdResult::NullArgument;
  if (a == nullptr && n > 0) return OrdResult::NullArgument;
  if (n > kMaxLen) return OrdResult::TooLarge;
  if (k >= n) return OrdResult::OutOfRange;
  u32 rng = 0x9E3779B9u;
  u32 lo = 0;
  u32 hi = n - 1;
  while (lo < hi) {
    const u32 span = hi - lo + 1;
    u32 pick[3];
    for (u32 t = 0; t < 3; ++t) {
      rng ^= rng << 13;
      rng ^= rng >> 17;
      rng ^= rng << 5;
      pick[t] = a[lo + rng % span];
    }
    const u32 pivot = median3(pick[0], pick[1], pick[2]);
    // Three-way partition of a[lo .. hi]: [lo, lt) below the pivot, [lt, gt] equal, (gt, hi] above.
    u32 lt = lo;
    u32 i = lo;
    u32 gt = hi;
    while (i <= gt) {
      if (a[i] < pivot) {
        const u32 t = a[lt];
        a[lt] = a[i];
        a[i] = t;
        ++lt;
        ++i;
      } else if (a[i] > pivot) {
        const u32 t = a[gt];
        a[gt] = a[i];
        a[i] = t;
        --gt;
      } else {
        ++i;
      }
    }
    if (k < lt) {
      hi = lt - 1;
    } else if (k > gt) {
      lo = gt + 1;
    } else {
      break;
    }
  }
  *out = a[k];
  return OrdResult::Ok;
}

OrdResult inversion_count_u32(const u32* a, u32 n, u32* scratch, u32 scratch_words, u64* out) {
  if (out == nullptr) return OrdResult::NullArgument;
  if (a == nullptr && n > 0) return OrdResult::NullArgument;
  const u64 need = (n > 1) ? 2 * static_cast<u64>(n) : 0;
  if (scratch == nullptr && need > 0) return OrdResult::NullArgument;
  if (n > kMaxLen) return OrdResult::TooLarge;
  if (scratch_words < need) return OrdResult::ScratchTooSmall;
  if (n < 2) {
    *out = 0;
    return OrdResult::Ok;
  }
  u32* src = scratch;
  u32* buf = scratch + n;
  for (u32 i = 0; i < n; ++i) src[i] = a[i];
  u64 count = 0;
  for (u32 width = 1; width < n; width *= 2) {
    for (u32 start = 0; start < n; start += 2 * width) {
      const u32 mid = (start + width < n) ? start + width : n;
      const u32 end = (start + 2 * width < n) ? start + 2 * width : n;
      u32 i = start;
      u32 j = mid;
      u32 t = start;
      while (i < mid && j < end) {
        if (src[j] < src[i]) {
          count += mid - i;  // src[j] jumps over every remaining element of the left run
          buf[t++] = src[j++];
        } else {
          buf[t++] = src[i++];
        }
      }
      while (i < mid) buf[t++] = src[i++];
      while (j < end) buf[t++] = src[j++];
    }
    u32* tmp = src;
    src = buf;
    buf = tmp;
  }
  *out = count;
  return OrdResult::Ok;
}

}  // namespace lumora::ordstat
