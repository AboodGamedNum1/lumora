// Minimum-cost assignment (square matrix): match every row to a different column at the least total cost.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). The number of steps must be O(n^3) (for example the Hungarian method with
// potentials); a factorial or exponential method is not acceptable for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::assign {
constexpr std::uint32_t kMaxN = 512;  // largest matrix side
enum class AssignResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3 };
// The matrix is cost[i * n + j] for i, j in 0 .. n-1, row-major, each entry any u32 (0 to 4294967295). An assignment is a permutation: row i takes column
// col[i], and the columns are all different. Its cost is the sum of cost[i * n + col[i]], which is at most 512 * 4294967295 and fits u64.
//
// assign_scratch_words: *out_words = the scratch size in u32 words that assign_run needs: 16 * n + 16 (the algorithm may use less; scratch is working
// memory with unspecified contents afterwards, not tested). Check order: a null out_words gives NullArgument; then n above kMaxN gives TooLarge
// (nothing written).
AssignResult assign_scratch_words(std::uint32_t n, std::uint32_t* out_words);
// assign_run: on Ok, *out_total = the least possible cost over all assignments (0 for n = 0) and out_col[0 .. n) = one assignment with exactly that
// cost. Which optimal assignment is returned is NOT specified: a test checks only that out_col is a permutation whose cost equals *out_total, and that
// *out_total is the true minimum.
// Check order: a null pointer argument gives NullArgument (cost, scratch and out_col only while n is above 0; out_total always); then n above kMaxN
// gives TooLarge (the condition uses the given n even when above the limit); then, only while n is above 0, scratch_words below 16 * n + 16 gives
// ScratchTooSmall. Nothing is written on any failure. n = 0 is Ok with *out_total = 0 and nothing written to out_col.
AssignResult assign_run(std::uint32_t n, const std::uint32_t* cost, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_total,
                        std::uint32_t* out_col);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::assign
