// Stable LSD radix sort of 32-bit keys (4 passes of 8 bits) with optional values, and a sortedness check. See sortu.h for the
// contract. Integer only: the 256-entry count table is on the stack, no allocation, no recursion, no global state.
#include "sortu.h"

namespace lumora::sortu {

namespace {

using u32 = std::uint32_t;

// A pass is skipped when every key has the same byte there (the stable result is unchanged). Returns true when the pass moved data.
// The count table is filled from src, turned into start offsets, and the elements are scattered to dst in order (stable).
bool pass_keys(const u32* src, u32* dst, u32 n, u32 shift) {
  u32 count[256];
  for (u32 b = 0; b < 256; ++b) count[b] = 0;
  for (u32 i = 0; i < n; ++i) ++count[(src[i] >> shift) & 255u];
  if (count[(src[0] >> shift) & 255u] == n) return false;
  u32 sum = 0;
  for (u32 b = 0; b < 256; ++b) {
    const u32 c = count[b];
    count[b] = sum;
    sum += c;
  }
  for (u32 i = 0; i < n; ++i) dst[count[(src[i] >> shift) & 255u]++] = src[i];
  return true;
}

bool pass_pairs(const u32* sk, const u32* sv, u32* dk, u32* dv, u32 n, u32 shift) {
  u32 count[256];
  for (u32 b = 0; b < 256; ++b) count[b] = 0;
  for (u32 i = 0; i < n; ++i) ++count[(sk[i] >> shift) & 255u];
  if (count[(sk[0] >> shift) & 255u] == n) return false;
  u32 sum = 0;
  for (u32 b = 0; b < 256; ++b) {
    const u32 c = count[b];
    count[b] = sum;
    sum += c;
  }
  for (u32 i = 0; i < n; ++i) {
    const u32 pos = count[(sk[i] >> shift) & 255u]++;
    dk[pos] = sk[i];
    dv[pos] = sv[i];
  }
  return true;
}

}  // namespace

SortResult radix_sort_u32(u32* keys, u32 n, u32* scratch) {
  if (n > 0 && (keys == nullptr || scratch == nullptr)) return SortResult::NullArgument;
  if (n > kMaxCount) return SortResult::TooLarge;
  if (n < 2) return SortResult::Ok;
  u32* src = keys;
  u32* dst = scratch;
  for (u32 shift = 0; shift < 32; shift += 8) {
    if (pass_keys(src, dst, n, shift)) {
      u32* t = src;
      src = dst;
      dst = t;
    }
  }
  if (src != keys) {
    for (u32 i = 0; i < n; ++i) keys[i] = src[i];
  }
  return SortResult::Ok;
}

SortResult radix_sort_pairs_u32(u32* keys, u32* values, u32 n, u32* scratch_keys, u32* scratch_values) {
  if (n > 0 && (keys == nullptr || values == nullptr || scratch_keys == nullptr || scratch_values == nullptr)) return SortResult::NullArgument;
  if (n > kMaxCount) return SortResult::TooLarge;
  if (n < 2) return SortResult::Ok;
  u32* sk = keys;
  u32* sv = values;
  u32* dk = scratch_keys;
  u32* dv = scratch_values;
  for (u32 shift = 0; shift < 32; shift += 8) {
    if (pass_pairs(sk, sv, dk, dv, n, shift)) {
      u32* tk = sk;
      u32* tv = sv;
      sk = dk;
      sv = dv;
      dk = tk;
      dv = tv;
    }
  }
  if (sk != keys) {
    for (u32 i = 0; i < n; ++i) {
      keys[i] = sk[i];
      values[i] = sv[i];
    }
  }
  return SortResult::Ok;
}

SortResult is_sorted_u32(const u32* keys, u32 n, bool* out) {
  if (out == nullptr || (n > 0 && keys == nullptr)) return SortResult::NullArgument;
  if (n > kMaxCount) return SortResult::TooLarge;
  bool sorted = true;
  for (u32 i = 1; i < n; ++i) {
    if (keys[i - 1] > keys[i]) {
      sorted = false;
      break;
    }
  }
  *out = sorted;
  return SortResult::Ok;
}

}  // namespace lumora::sortu
