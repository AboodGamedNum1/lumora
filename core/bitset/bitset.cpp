// Fixed-size bit sets over caller 64-bit words. See bitset.h for the contract. Integer only: plain shifts and masks, no builtins, no
// intrinsics, no allocation, no recursion, no global state.
#include "bitset.h"

namespace lumora::bitset {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kAll = ~0ull;

bool valid(const void* words, u32 nbits) { return nbits <= kMaxBits && (nbits == 0 || words != nullptr); }

u32 count_bits(u64 x) {
  u32 c = 0;
  while (x != 0) {
    x &= x - 1;
    ++c;
  }
  return c;
}

u32 trailing_zeros(u64 x) {  // x != 0
  u32 k = 0;
  while ((x & 1u) == 0) {
    x >>= 1;
    ++k;
  }
  return k;
}

}  // namespace

u32 words_for(u32 nbits) { return nbits > kMaxBits ? 0u : (nbits + 63u) / 64u; }

bool set(u64* words, u32 nbits, u32 i, bool value) {
  if (!valid(words, nbits) || i >= nbits) return false;
  const u64 mask = 1ull << (i & 63u);
  if (value) words[i >> 6] |= mask;
  else words[i >> 6] &= ~mask;
  return true;
}

bool test(const u64* words, u32 nbits, u32 i, bool* out) {
  if (!valid(words, nbits) || out == nullptr || i >= nbits) return false;
  *out = ((words[i >> 6] >> (i & 63u)) & 1u) != 0;
  return true;
}

bool fill(u64* words, u32 nbits, bool value) {
  if (!valid(words, nbits)) return false;
  const u32 full = nbits >> 6;
  for (u32 w = 0; w < full; ++w) words[w] = value ? kAll : 0ull;
  const u32 rem = nbits & 63u;
  if (rem != 0) {
    const u64 mask = (1ull << rem) - 1u;
    if (value) words[full] |= mask;
    else words[full] &= ~mask;
  }
  return true;
}

bool popcount(const u64* words, u32 nbits, u32* out) {
  if (!valid(words, nbits) || out == nullptr) return false;
  const u32 full = nbits >> 6;
  u32 total = 0;
  for (u32 w = 0; w < full; ++w) total += count_bits(words[w]);
  const u32 rem = nbits & 63u;
  if (rem != 0) total += count_bits(words[full] & ((1ull << rem) - 1u));
  *out = total;
  return true;
}

bool find_next(const u64* words, u32 nbits, u32 from, bool value, u32* out) {
  if (!valid(words, nbits) || out == nullptr || from > nbits) return false;
  if (from == nbits) {
    *out = nbits;
    return true;
  }
  const u32 last = (nbits - 1u) >> 6;
  u32 w = from >> 6;
  u64 x = (value ? words[w] : ~words[w]) & (kAll << (from & 63u));
  for (;;) {
    if (x != 0) {
      const u64 j = (static_cast<u64>(w) << 6) + trailing_zeros(x);
      *out = j < nbits ? static_cast<u32>(j) : nbits;  // a hit in the padding is no hit
      return true;
    }
    if (w == last) break;
    ++w;
    x = value ? words[w] : ~words[w];
  }
  *out = nbits;
  return true;
}

}  // namespace lumora::bitset
