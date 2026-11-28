// Domino tilings of an h by w rectangle modulo 998244353 (contract in tilings.h).
//
// Method: the profile transfer matrix. Column by column, let s be the set of rows of the current column already covered by horizontal dominoes that
// started in the previous column, and t the set of rows in which a horizontal domino starts in this column and ends in the next one. A column can be
// completed from s to t exactly when s and t are disjoint and the remaining free rows (those in neither s nor t) can be paired by vertical dominoes, which
// is the case exactly when every maximal run of consecutive free rows has even length. T[s][t] is 1 in that case and 0 otherwise. The number of tilings of
// the grid with h rows and w columns is the number of ways to go from the empty profile through w columns back to the empty profile: (T^w)[0][0].
// T^w is applied to a row vector: for each bit of w from the lowest, the vector is multiplied by the current power of T when the bit is set, and the power
// is squared. That is at most 31 squarings and 32 vector products, about 32 * 64^3 multiply-adds for h = 6, whatever w is, with no loop over the columns.
// Arithmetic: all entries are residues below kMod; a product of two is below 2^60 and is reduced at once, and the partial sum plus one product stays below
// 2^61, so a u64 never overflows and no 128-bit type is used.
// Working memory is automatic and of fixed size: in the matrix power, two 64 x 64 u32 matrices (the power and the product being formed, 4096 words each) and
// two 64-word vectors, 2 * 4096 + 2 * 64 = 8320 words, below the 16384 words of the contract; build_profile writes into the first matrix and has no
// array of its own. Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no static table.
#include "tilings.h"

namespace lumora::tilings {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr u32 kDim = 64u;  // 2^kMaxH: the matrices are always stored 64 x 64, the used part is size x size

// m[s * kDim + t] = 1 when a column with the profile s coming in can leave with the profile t (rows 0 .. h - 1), 0 otherwise.
void build_profile(u32 h, u32* m) {
  const u32 size = 1u << h;
  for (u32 i = 0; i < kDim * kDim; ++i) m[i] = 0u;
  for (u32 s = 0; s < size; ++s) {
    for (u32 t = 0; t < size; ++t) {
      if ((s & t) != 0u) continue;
      const u32 used = s | t;
      bool ok = true;
      u32 run = 0u;  // length of the current run of free rows
      for (u32 r = 0; r < h; ++r) {
        if (((used >> r) & 1u) == 0u) {
          ++run;
        } else {
          if ((run & 1u) != 0u) ok = false;
          run = 0u;
        }
      }
      if ((run & 1u) != 0u) ok = false;
      if (ok) m[s * kDim + t] = 1u;
    }
  }
}

// sq = m * m on the used size x size part; then m = sq.
void square(u32 size, u32* m, u32* sq) {
  for (u32 i = 0; i < size; ++i) {
    for (u32 j = 0; j < size; ++j) sq[i * kDim + j] = 0u;
    for (u32 k = 0; k < size; ++k) {
      const u64 a = m[i * kDim + k];
      if (a == 0u) continue;
      for (u32 j = 0; j < size; ++j) sq[i * kDim + j] = static_cast<u32>((sq[i * kDim + j] + a * m[k * kDim + j]) % kMod);
    }
  }
  for (u32 i = 0; i < size; ++i) {
    for (u32 j = 0; j < size; ++j) m[i * kDim + j] = sq[i * kDim + j];
  }
}

// v = v * m on the used size part, through the scratch vector nv.
void apply(u32 size, const u32* m, u32* v, u32* nv) {
  for (u32 j = 0; j < size; ++j) {
    u64 acc = 0u;
    for (u32 i = 0; i < size; ++i) acc = (acc + static_cast<u64>(v[i]) * m[i * kDim + j]) % kMod;
    nv[j] = static_cast<u32>(acc);
  }
  for (u32 j = 0; j < size; ++j) v[j] = nv[j];
}
}  // namespace

TileResult domino_tilings(std::uint32_t h, std::uint32_t w, std::uint32_t* out) {
  if (out == nullptr) return TileResult::NullArgument;
  if (h < 1u || h > kMaxH) return TileResult::BadHeight;
  u32 m[kDim * kDim];   // the current power of the profile matrix
  u32 sq[kDim * kDim];  // the product being formed
  u32 v[kDim];          // the row vector, starting as the empty profile
  u32 nv[kDim];         // scratch for one vector product
  const u32 size = 1u << h;
  build_profile(h, m);
  for (u32 j = 0; j < size; ++j) v[j] = 0u;
  v[0] = 1u;
  u32 e = w;
  while (e > 0u) {
    if ((e & 1u) != 0u) apply(size, m, v, nv);
    e >>= 1;
    if (e > 0u) square(size, m, sq);
  }
  *out = v[0];
  return TileResult::Ok;
}
}  // namespace lumora::tilings
