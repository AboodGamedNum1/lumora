// Gaussian elimination modulo 998244353 (contract in gauss.h). Gauss-Jordan elimination in caller scratch.
// Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler builtins. See docs/gauss.md.
#include "gauss.h"

namespace lumora::gauss {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

u32 mul_mod(u32 x, u32 y) { return static_cast<u32>((static_cast<u64>(x) * static_cast<u64>(y)) % kMod); }

u32 sub_mod(u32 x, u32 y) { return (x >= y) ? (x - y) : (x + kMod - y); }

// x^(kMod - 2): the inverse of a nonzero x.
u32 inv_mod(u32 x) {
  u32 r = 1u;
  u32 b = x;
  u32 e = kMod - 2u;
  while (e != 0u) {
    if ((e & 1u) != 0u) r = mul_mod(r, b);
    b = mul_mod(b, b);
    e >>= 1u;
  }
  return r;
}

// True when every entry of v[0 .. count) is below kMod.
bool all_below_mod(const u32* v, u64 count) {
  bool ok = true;
  for (u64 i = 0u; i < count; ++i) {
    if (v[i] >= kMod) ok = false;
  }
  return ok;
}

// Gauss-Jordan elimination of w (rows x width, row-major), pivots searched only in the first cols columns (cols <= width). Pivot rows end at
// rows 0 .. rank - 1 in reduced row echelon form (leading entry 1 at the leftmost possible column of every pivot row). *det_acc is multiplied by
// every pivot value and negated for every row swap, so it is the determinant when the matrix is square and the rank is full.
u32 reduce(u32* w, u32 rows, u32 cols, u32 width, u32* det_acc) {
  u32 rank = 0u;
  for (u32 c = 0u; c < cols && rank < rows; ++c) {
    u32 p = rank;
    while (p < rows && w[static_cast<u64>(p) * width + c] == 0u) ++p;
    if (p == rows) continue;
    if (p != rank) {
      for (u32 j = 0u; j < width; ++j) {
        const u32 t = w[static_cast<u64>(p) * width + j];
        w[static_cast<u64>(p) * width + j] = w[static_cast<u64>(rank) * width + j];
        w[static_cast<u64>(rank) * width + j] = t;
      }
      *det_acc = sub_mod(0u, *det_acc);
    }
    u32* prow = w + static_cast<u64>(rank) * width;
    const u32 pivot = prow[c];
    *det_acc = mul_mod(*det_acc, pivot);
    const u32 inv = inv_mod(pivot);
    for (u32 j = c; j < width; ++j) prow[j] = mul_mod(prow[j], inv);
    for (u32 i = 0u; i < rows; ++i) {
      if (i == rank) continue;
      u32* row = w + static_cast<u64>(i) * width;
      const u32 f = row[c];
      if (f == 0u) continue;
      for (u32 j = c; j < width; ++j) row[j] = sub_mod(row[j], mul_mod(f, prow[j]));
    }
    ++rank;
  }
  return rank;
}

}  // namespace

GaussResult gauss_scratch_words(std::uint32_t rows, std::uint32_t cols, bool with_rhs, std::uint32_t* out_words) {
  if (out_words == nullptr) return GaussResult::NullArgument;
  if (rows > kMaxDim || cols > kMaxDim) return GaussResult::TooLarge;
  const u64 width = with_rhs ? static_cast<u64>(cols) + 1u : static_cast<u64>(cols);
  *out_words = static_cast<u32>(static_cast<u64>(rows) * width);
  return GaussResult::Ok;
}

GaussResult gauss_det(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_det) {
  if (out_det == nullptr) return GaussResult::NullArgument;
  if (n > 0u && (a == nullptr || scratch == nullptr)) return GaussResult::NullArgument;
  if (n > kMaxDim) return GaussResult::TooLarge;
  const u64 cells = static_cast<u64>(n) * n;
  if (n > 0u && static_cast<u64>(scratch_words) < cells) return GaussResult::ScratchTooSmall;
  if (!all_below_mod(a, cells)) return GaussResult::ValueOutOfRange;
  if (n == 0u) {
    *out_det = 1u;
    return GaussResult::Ok;
  }
  for (u64 i = 0u; i < cells; ++i) scratch[i] = a[i];
  u32 det = 1u;
  const u32 rank = reduce(scratch, n, n, n, &det);
  *out_det = (rank == n) ? det : 0u;
  return GaussResult::Ok;
}

GaussResult gauss_rank(const std::uint32_t* a, std::uint32_t rows, std::uint32_t cols, std::uint32_t* scratch, std::uint32_t scratch_words,
                       std::uint32_t* out_rank) {
  if (out_rank == nullptr) return GaussResult::NullArgument;
  const bool nonempty = rows > 0u && cols > 0u;
  if (nonempty && (a == nullptr || scratch == nullptr)) return GaussResult::NullArgument;
  if (rows > kMaxDim || cols > kMaxDim) return GaussResult::TooLarge;
  const u64 cells = static_cast<u64>(rows) * cols;
  if (nonempty && static_cast<u64>(scratch_words) < cells) return GaussResult::ScratchTooSmall;
  if (!all_below_mod(a, cells)) return GaussResult::ValueOutOfRange;
  if (!nonempty) {
    *out_rank = 0u;
    return GaussResult::Ok;
  }
  for (u64 i = 0u; i < cells; ++i) scratch[i] = a[i];
  u32 unused = 1u;
  *out_rank = reduce(scratch, rows, cols, cols, &unused);
  return GaussResult::Ok;
}

GaussResult gauss_solve(const std::uint32_t* a, std::uint32_t rows, std::uint32_t cols, const std::uint32_t* b, std::uint32_t* scratch,
                        std::uint32_t scratch_words, std::uint32_t* x, std::uint32_t* out_rank, SolveKind* out_kind) {
  if (out_rank == nullptr || out_kind == nullptr) return GaussResult::NullArgument;
  if (rows > 0u && cols > 0u && a == nullptr) return GaussResult::NullArgument;
  if (rows > 0u && (b == nullptr || scratch == nullptr)) return GaussResult::NullArgument;
  if (cols > 0u && x == nullptr) return GaussResult::NullArgument;
  if (rows > kMaxDim || cols > kMaxDim) return GaussResult::TooLarge;
  const u64 width = static_cast<u64>(cols) + 1u;
  if (rows > 0u && static_cast<u64>(scratch_words) < static_cast<u64>(rows) * width) return GaussResult::ScratchTooSmall;
  if (!all_below_mod(a, static_cast<u64>(rows) * cols)) return GaussResult::ValueOutOfRange;
  if (!all_below_mod(b, rows)) return GaussResult::ValueOutOfRange;
  if (rows == 0u) {
    // No equations: the zero vector solves it; it is the only solution only when there are no unknowns.
    for (u32 j = 0u; j < cols; ++j) x[j] = 0u;
    *out_rank = 0u;
    *out_kind = (cols == 0u) ? SolveKind::Unique : SolveKind::Many;
    return GaussResult::Ok;
  }
  const u32 w = static_cast<u32>(width);
  for (u32 i = 0u; i < rows; ++i) {
    for (u32 j = 0u; j < cols; ++j) scratch[static_cast<u64>(i) * w + j] = a[static_cast<u64>(i) * cols + j];
    scratch[static_cast<u64>(i) * w + cols] = b[i];
  }
  u32 unused = 1u;
  const u32 rank = reduce(scratch, rows, cols, w, &unused);
  // Rows rank .. rows - 1 are zero in the first cols columns; a nonzero right-hand side there is a contradiction.
  for (u32 i = rank; i < rows; ++i) {
    if (scratch[static_cast<u64>(i) * w + cols] != 0u) {
      *out_rank = rank;
      *out_kind = SolveKind::Inconsistent;
      return GaussResult::Ok;
    }
  }
  for (u32 j = 0u; j < cols; ++j) x[j] = 0u;
  for (u32 k = 0u; k < rank; ++k) {
    u32 c = 0u;
    while (scratch[static_cast<u64>(k) * w + c] == 0u) ++c;
    x[c] = scratch[static_cast<u64>(k) * w + cols];
  }
  *out_rank = rank;
  *out_kind = (rank == cols) ? SolveKind::Unique : SolveKind::Many;
  return GaussResult::Ok;
}

}  // namespace lumora::gauss
