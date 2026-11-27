// Number-theoretic convolution modulo 998244353 (contract in ntt.h). Iterative radix-2 transform with the primitive root 3, in caller scratch.
// Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler builtins. See docs/ntt.md.
#include "ntt.h"

namespace lumora::ntt {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kRoot = 3u;

u32 mul_mod(u32 x, u32 y) { return static_cast<u32>((static_cast<u64>(x) * static_cast<u64>(y)) % kMod); }

u32 pow_mod(u32 base, u32 e) {
  u32 r = 1u;
  u32 b = base;
  while (e != 0u) {
    if ((e & 1u) != 0u) r = mul_mod(r, b);
    b = mul_mod(b, b);
    e >>= 1u;
  }
  return r;
}

// r = na + nb - 1 in 64 bits, 0 when na or nb is 0.
u64 result_len(u32 na, u32 nb) {
  if (na == 0u || nb == 0u) return 0u;
  return static_cast<u64>(na) + static_cast<u64>(nb) - 1u;
}

// Smallest power of two that is at least r (r from 1 to 2^19 - 1 here).
u32 padded_size(u64 r) {
  u32 m = 1u;
  while (static_cast<u64>(m) < r) m <<= 1u;
  return m;
}

// In-place forward transform of v[0 .. m), m a power of two, m <= 2^19.
void transform(u32* v, u32 m) {
  // bit reversal permutation
  u32 j = 0u;
  for (u32 i = 1u; i < m; ++i) {
    u32 bit = m >> 1u;
    while ((j & bit) != 0u) {
      j ^= bit;
      bit >>= 1u;
    }
    j ^= bit;
    if (i < j) {
      const u32 t = v[i];
      v[i] = v[j];
      v[j] = t;
    }
  }
  for (u32 len = 2u; len <= m; len <<= 1u) {
    const u32 wl = pow_mod(kRoot, (kMod - 1u) / len);
    const u32 half = len >> 1u;
    for (u32 start = 0u; start < m; start += len) {
      u32 w = 1u;
      for (u32 k = 0u; k < half; ++k) {
        const u32 x = v[start + k];
        const u32 y = mul_mod(v[start + k + half], w);
        u32 s = x + y;
        if (s >= kMod) s -= kMod;
        v[start + k] = s;
        v[start + k + half] = (x >= y) ? (x - y) : (x + kMod - y);
        w = mul_mod(w, wl);
      }
    }
  }
}

}  // namespace

NttResult ntt_scratch_words(std::uint32_t na, std::uint32_t nb, std::uint32_t* out_words) {
  if (out_words == nullptr) return NttResult::NullArgument;
  if (na > kMaxLen || nb > kMaxLen) return NttResult::TooLarge;
  const u64 r = result_len(na, nb);
  *out_words = (r == 0u) ? 0u : static_cast<u32>(2u * static_cast<u64>(padded_size(r)));
  return NttResult::Ok;
}

NttResult ntt_convolve(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* scratch,
                       std::uint32_t scratch_words, std::uint32_t* out, std::uint32_t out_cap, std::uint32_t* out_len) {
  if (out_len == nullptr) return NttResult::NullArgument;
  const u64 r = result_len(na, nb);
  if (na > 0u && a == nullptr) return NttResult::NullArgument;
  if (nb > 0u && b == nullptr) return NttResult::NullArgument;
  if (r > 0u && (scratch == nullptr || out == nullptr)) return NttResult::NullArgument;
  if (na > kMaxLen || nb > kMaxLen) return NttResult::TooLarge;
  u32 m = 0u;
  if (r > 0u) {
    m = padded_size(r);
    if (static_cast<u64>(scratch_words) < 2u * static_cast<u64>(m)) return NttResult::ScratchTooSmall;
    if (static_cast<u64>(out_cap) < r) return NttResult::OutTooSmall;
  }
  bool bad = false;
  for (u32 i = 0u; i < na; ++i) {
    if (a[i] >= kMod) bad = true;
  }
  for (u32 i = 0u; i < nb; ++i) {
    if (b[i] >= kMod) bad = true;
  }
  if (bad) return NttResult::ValueOutOfRange;
  if (r == 0u) {
    *out_len = 0u;
    return NttResult::Ok;
  }
  // Both inputs are copied into scratch before anything is written to out, so any overlap among a, b and out is harmless.
  u32* fa = scratch;
  u32* fb = scratch + m;
  for (u32 i = 0u; i < m; ++i) {
    fa[i] = (i < na) ? a[i] : 0u;
    fb[i] = (i < nb) ? b[i] : 0u;
  }
  transform(fa, m);
  transform(fb, m);
  for (u32 i = 0u; i < m; ++i) fa[i] = mul_mod(fa[i], fb[i]);
  transform(fa, m);
  // Inverse: the forward transform again, then the entries 1 .. m-1 are read in reverse order and scaled by 1/m.
  const u32 inv_m = pow_mod(m, kMod - 2u);
  const u32 rr = static_cast<u32>(r);
  for (u32 k = 0u; k < rr; ++k) {
    const u32 src = (k == 0u) ? 0u : (m - k);
    out[k] = mul_mod(fa[src], inv_m);
  }
  *out_len = rr;
  return NttResult::Ok;
}

}  // namespace lumora::ntt
