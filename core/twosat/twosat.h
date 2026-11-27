// 2-SAT: satisfiability of a conjunction of two-literal clauses, with a satisfying assignment.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion
// (the implication graph can be 131072 nodes deep), no global state, no large automatic arrays (working memory is caller scratch). The number of steps
// must be linear in n + m (for example strongly connected components); a slower method is not acceptable for the largest input. Evidence label:
// unit tests only.
#pragma once
#include <cstdint>
namespace lumora::twosat {
constexpr std::uint32_t kMaxVars = 65536;      // largest number of variables
constexpr std::uint32_t kMaxClauses = 262144;  // largest number of clauses
enum class TwoSatResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadLiteral = 4 };
// Variables are numbered 0 .. n-1. A literal is a u32: 2 * v is the variable v, 2 * v + 1 is its negation, so a valid literal is below 2 * n. Clause i
// is (la[i] OR lb[i]); la[i] and lb[i] may be equal, may be the two polarities of one variable (a tautology), and clauses may repeat.
//
// twosat_scratch_words: *out_words = the scratch size in u32 words that twosat_solve needs: 16 * n + 2 * m + 2 (the algorithm may use less; scratch is
// working memory with unspecified contents afterwards, not tested). Check order: a null out_words gives NullArgument; then n above kMaxVars or m
// above kMaxClauses gives TooLarge (nothing written).
TwoSatResult twosat_scratch_words(std::uint32_t n, std::uint32_t m, std::uint32_t* out_words);
// twosat_solve: on Ok, *out_sat is 1 when a satisfying assignment exists and 0 otherwise.
//   If *out_sat is 1, value[0 .. n) holds one satisfying assignment, each entry 0 (false) or 1 (true). Which assignment is returned is NOT specified:
//   a test checks only that every clause holds.
//   If *out_sat is 0, *out_conflict = the smallest variable index v such that v and its negation lie in the same strongly connected component of the
//   implication graph (clause (a OR b) gives the edges not a -> b and not b -> a); that is, the smallest v for which both v -> not v and not v -> v are
//   implied. value is untouched. When *out_sat is 1, *out_conflict is untouched.
// With n = 0 there is no valid literal, so any m above 0 gives BadLiteral (after the checks below); n = 0 and m = 0
// gives Ok, *out_sat = 1 and nothing written to value or *out_conflict.
// Check order: a null pointer argument gives NullArgument (la and lb only while m is above 0; value, scratch only while n is above 0; out_sat and
// out_conflict always); then n above kMaxVars or m above kMaxClauses gives TooLarge (the condition uses the given values even when above the
// limits); then, only while n is above 0, scratch_words below 16 * n + 2 * m + 2 gives ScratchTooSmall; then a literal at or above 2 * n in la or lb
// gives BadLiteral (one pass over all clauses; nothing is written on any failure).
TwoSatResult twosat_solve(std::uint32_t n, std::uint32_t m, const std::uint32_t* la, const std::uint32_t* lb, std::uint32_t* scratch,
                          std::uint32_t scratch_words, std::uint8_t* value, std::uint8_t* out_sat, std::uint32_t* out_conflict);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::twosat
