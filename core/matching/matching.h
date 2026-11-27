// Maximum matching of a bipartite graph given as an edge list: the size and, for every vertex, whether it is matched in every, in some or in no
// maximum matching. In caller storage. Pure integer: no floating point, no libm, no 128-bit types, no compiler builtins or intrinsics (MSVC), no
// allocation, no recursion (explicit stacks in scratch), no global state. The number of steps must be polynomial in the sizes (for example
// Hopcroft-Karp plus alternating-path reachability, iterative). Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::matching {
constexpr std::uint32_t kMaxSide = 65536;     // largest number of vertices on one side
constexpr std::uint32_t kMaxEdges = 262144;   // largest edge count
enum class MatchResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadNode = 4 };
// Graph: left vertices 0 .. left-1, right vertices 0 .. right-1. Edge i joins the left vertex eu[i] and the right vertex ev[i]. Parallel edges are
// allowed (they count once). The caller must not make any two arrays overlap.
//
// RESULT: *out_size = the number of edges in a maximum matching (a set of edges that share no vertex, as large as possible). cl[v] for the left
// vertex v in 0 .. left-1 and cr[w] for the right vertex w in 0 .. right-1 are 2 if the vertex is matched in EVERY maximum matching, 0 if it is
// matched in NO maximum matching (this happens exactly for a vertex without edges), and 1 otherwise (matched in some maximum matchings but not in
// others). The individual edges of a matching are not part of the result, so the result is the same for every correct method.
//
// *out_words = the scratch size in u32 words that matching_run needs: 8 * left + 8 * right + 2 * edges + 2 (the algorithm may use less). Check
// order of matching_scratch_words: a null out_words gives NullArgument; then left or right above kMaxSide, or edges above kMaxEdges, gives
// TooLarge (nothing written).
MatchResult matching_scratch_words(std::uint32_t left, std::uint32_t right, std::uint32_t edges, std::uint32_t* out_words);
// Computes the result. Check order: a null pointer argument (eu and ev only while edges is above 0; scratch only while left + right is above 0; cl
// only while left is above 0; cr only while right is above 0; out_size always; the conditions use the given sizes even when they are above the
// limits) gives NullArgument; then left or right above kMaxSide, or edges above kMaxEdges, gives TooLarge; then, only while left + right is above
// 0, scratch_words below 8 * left + 8 * right + 2 * edges + 2 gives ScratchTooSmall; then BadNode when any edge has eu not below left or ev not
// below right (every edge is checked, in one pass, before any write; with left 0 or right 0 any edge is a BadNode). On any failure all outputs are
// untouched. On success only *out_size, cl[0 .. left) and cr[0 .. right) are written, and scratch is used as working memory with unspecified
// contents afterwards (not tested). With no vertices at all the result is size 0 and nothing else is written.
MatchResult matching_run(std::uint32_t left, std::uint32_t right, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev,
                         std::uint32_t* scratch, std::uint32_t scratch_words, std::uint8_t* cl, std::uint8_t* cr, std::uint32_t* out_size);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::matching
