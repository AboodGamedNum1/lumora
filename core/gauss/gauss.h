// Gaussian elimination over the field of residues modulo the prime 998244353: determinant, rank and one canonical solution of a linear system.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). The number of field operations must be O(rows * cols * min(rows, cols));
// a factorial or exponential method is not acceptable for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::gauss {
constexpr std::uint32_t kMod = 998244353u;  // prime
constexpr std::uint32_t kMaxDim = 256u;     // largest number of rows and of columns
enum class GaussResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, ValueOutOfRange = 4 };
// Solution kinds of gauss_solve.
enum class SolveKind : std::uint8_t { Unique = 0, Many = 1, Inconsistent = 2 };
// A matrix a has rows * cols entries, row-major: a[i * cols + j]. Every entry (and every entry of b) is a residue in 0 .. kMod - 1; an entry not below
// kMod gives ValueOutOfRange. All arithmetic is exact in the field (add, subtract, multiply and divide by a nonzero element modulo kMod). Scratch sizes
// are in u32 words; scratch is working memory with unspecified contents afterwards (not tested) and needs only the alignment of u32. Output arrays
// and scratch must not overlap the input arrays or each other (outside the contract, not tested). Nothing is written on any failure.
//
// gauss_scratch_words: *out_words = rows * cols when with_rhs is false, rows * (cols + 1) when it is true (gauss_det and gauss_rank use the first, with
// rows = cols = n for gauss_det; gauss_solve uses the second). Check order: a null out_words gives NullArgument; then rows or cols above kMaxDim gives
// TooLarge (nothing written).
GaussResult gauss_scratch_words(std::uint32_t rows, std::uint32_t cols, bool with_rhs, std::uint32_t* out_words);
// gauss_det: the determinant of the n x n matrix a, as a residue (the empty matrix, n = 0, has determinant 1; a singular matrix has determinant 0).
// Check order: a null out_det (always), or a or scratch (each only while n is above 0), gives NullArgument; then n above kMaxDim gives TooLarge;
// then, only while n is above 0, scratch_words below n * n gives ScratchTooSmall; then an entry not below kMod gives ValueOutOfRange. On Ok *out_det
// is written.
GaussResult gauss_det(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_det);
// gauss_rank: the rank of the rows x cols matrix a (0 when rows or cols is 0). Check order: a null out_rank (always), or a or scratch (each only
// while rows and cols are both above 0), gives NullArgument; then rows or cols above kMaxDim gives TooLarge (even when the other one is 0); then, only
// while rows and cols are both above 0, scratch_words below rows * cols gives ScratchTooSmall; then an entry not below kMod gives ValueOutOfRange.
GaussResult gauss_rank(const std::uint32_t* a, std::uint32_t rows, std::uint32_t cols, std::uint32_t* scratch, std::uint32_t scratch_words,
                       std::uint32_t* out_rank);
// gauss_solve: the system a * x = b with a of size rows x cols (cols unknowns, rows equations) and b of rows entries. On Ok, *out_rank = the rank of a
// and *out_kind is:
//   Inconsistent: no solution (x is not written);
//   Unique: exactly one solution (rank = cols; this includes cols = 0 with every entry of b equal to 0): x[0 .. cols) holds it;
//   Many: more than one solution (rank < cols; this includes rows = 0 with cols above 0): x[0 .. cols) holds THE canonical one, the solution of the
//   reduced row echelon form in which every free variable (a column without a pivot) is 0.
// A system with cols = 0 and a nonzero entry of b is Inconsistent. Check order: a null out_rank or out_kind (always), or a (only while rows and cols
// are both above 0), or b and scratch (each only while rows is above 0), or x (only while cols is above 0), gives NullArgument; then rows or cols above
// kMaxDim gives TooLarge; then, only while rows is above 0, scratch_words below rows * (cols + 1) gives ScratchTooSmall; then an entry of a or b not
// below kMod gives ValueOutOfRange.
GaussResult gauss_solve(const std::uint32_t* a, std::uint32_t rows, std::uint32_t cols, const std::uint32_t* b, std::uint32_t* scratch,
                        std::uint32_t scratch_words, std::uint32_t* x, std::uint32_t* out_rank, SolveKind* out_kind);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::gauss
