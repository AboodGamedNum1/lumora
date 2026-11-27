// Set of disjoint half-open u32 intervals in canonical form (non-empty, sorted, separated by a gap). See ivset.h for the contract.
// Integer only: no allocation, no recursion, no global state. No +1 is ever added to a bound, so an end of 0xFFFFFFFF cannot overflow.
#include "ivset.h"

namespace lumora::ivset {

namespace {

using u32 = std::uint32_t;

// Moves the entries [from, n) to start at index `to`, in the order that does not overwrite entries still to be read.
void shift_tail(u32* starts, u32* ends, u32 from, u32 n, u32 to) {
  if (to > from) {
    for (u32 i = n; i > from; --i) {
      starts[to + (i - 1 - from)] = starts[i - 1];
      ends[to + (i - 1 - from)] = ends[i - 1];
    }
  } else if (to < from) {
    for (u32 i = from; i < n; ++i) {
      starts[to + (i - from)] = starts[i];
      ends[to + (i - from)] = ends[i];
    }
  }
}

// Common argument checks. Returns Ok when the arguments are valid.
IvResult check_state(const u32* starts, const u32* ends, const u32* count, u32 capacity) {
  if (starts == nullptr || ends == nullptr || count == nullptr) return IvResult::NullArgument;
  if (capacity > kMaxCapacity) return IvResult::TooLarge;
  if (*count > capacity) return IvResult::BadCount;
  return IvResult::Ok;
}

}  // namespace

IvResult ivset_insert(u32* starts, u32* ends, u32* count, u32 capacity, u32 lo, u32 hi) {
  const IvResult chk = check_state(starts, ends, count, capacity);
  if (chk != IvResult::Ok) return chk;
  if (lo > hi) return IvResult::InvalidRange;
  if (lo == hi) return IvResult::Ok;
  const u32 n = *count;
  // i0: first interval that overlaps or touches [lo, hi) from the left side (its end reaches lo).
  u32 i0 = 0;
  while (i0 < n && ends[i0] < lo) ++i0;
  // i1: first interval that starts after hi (a start equal to hi touches and is merged).
  u32 i1 = i0;
  while (i1 < n && starts[i1] <= hi) ++i1;
  const u32 merged = i1 - i0;
  const u32 new_count = n - merged + 1u;
  if (new_count > capacity) return IvResult::Full;
  u32 new_lo = lo;
  u32 new_hi = hi;
  if (merged > 0) {
    if (starts[i0] < new_lo) new_lo = starts[i0];
    if (ends[i1 - 1] > new_hi) new_hi = ends[i1 - 1];
  }
  shift_tail(starts, ends, i1, n, i0 + 1u);
  starts[i0] = new_lo;
  ends[i0] = new_hi;
  *count = new_count;
  return IvResult::Ok;
}

IvResult ivset_remove(u32* starts, u32* ends, u32* count, u32 capacity, u32 lo, u32 hi) {
  const IvResult chk = check_state(starts, ends, count, capacity);
  if (chk != IvResult::Ok) return chk;
  if (lo > hi) return IvResult::InvalidRange;
  if (lo == hi) return IvResult::Ok;
  const u32 n = *count;
  // i0: first interval with a member at or above lo (its end is above lo). i1: first interval that starts at or above hi.
  u32 i0 = 0;
  while (i0 < n && ends[i0] <= lo) ++i0;
  u32 i1 = i0;
  while (i1 < n && starts[i1] < hi) ++i1;
  if (i1 == i0) return IvResult::Ok;  // nothing in [lo, hi) is a member
  const u32 affected = i1 - i0;
  const bool keep_left = starts[i0] < lo;
  const bool keep_right = ends[i1 - 1] > hi;
  const u32 pieces = (keep_left ? 1u : 0u) + (keep_right ? 1u : 0u);
  const u32 new_count = n - affected + pieces;
  if (new_count > capacity) return IvResult::Full;
  const u32 left_start = starts[i0];
  const u32 right_end = ends[i1 - 1];
  shift_tail(starts, ends, i1, n, i0 + pieces);
  u32 w = i0;
  if (keep_left) {
    starts[w] = left_start;
    ends[w] = lo;
    ++w;
  }
  if (keep_right) {
    starts[w] = hi;
    ends[w] = right_end;
  }
  *count = new_count;
  return IvResult::Ok;
}

IvResult ivset_contains(const u32* starts, const u32* ends, const u32* count, u32 capacity, u32 x, bool* out) {
  if (out == nullptr) return IvResult::NullArgument;
  const IvResult chk = check_state(starts, ends, count, capacity);
  if (chk != IvResult::Ok) return chk;
  // Binary search for the number of intervals whose start is at or below x.
  u32 lo = 0;
  u32 hi = *count;
  while (lo < hi) {
    const u32 mid = lo + (hi - lo) / 2u;
    if (starts[mid] <= x) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  *out = (lo > 0 && x < ends[lo - 1]);
  return IvResult::Ok;
}

}  // namespace lumora::ivset
