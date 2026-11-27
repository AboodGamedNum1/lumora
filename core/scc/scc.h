// Strongly connected components of a directed graph given as an edge list, in caller storage. Pure integer: no floating point, no libm, no
// 128-bit types, no compiler builtins or intrinsics (MSVC), no allocation, no recursion (an explicit stack in scratch), no global state. The number
// of steps must be linear in nodes plus edges (for example Tarjan's or Kosaraju's method, iterative). Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::scc {
constexpr std::uint32_t kMaxNodes = 65536;    // largest node count
constexpr std::uint32_t kMaxEdges = 262144;   // largest edge count
enum class SccResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadNode = 4 };
// Graph: nodes 0 .. nodes-1. Directed edge i goes from eu[i] to ev[i]. Parallel edges, antiparallel edges and self-loops are allowed (a self-loop
// does not change any component). The caller must not make any two arrays overlap.
//
// RESULT: comp[v] for v in 0 .. nodes-1 is the component number of node v, and *out_count is the number of components. Two nodes get the same
// number exactly when each is reachable from the other (a node is always reachable from itself, so an isolated node is a component of its own).
// The numbering is CANONICAL, so it is fully determined by the graph: components are numbered 0, 1, 2, ... in the order of their SMALLEST member
// node (the component holding node 0 is 0; the next number goes to the component of the smallest node not yet numbered; and so on). Nodes 0 gives
// *out_count = 0 and writes nothing to comp.
//
// *out_words = the scratch size in u32 words that scc_run needs: 6 * nodes + edges + 1 (the algorithm may use less). Check order of
// scc_scratch_words: a null out_words gives NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge (nothing written).
SccResult scc_scratch_words(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_words);
// Computes the components. Check order: a null pointer argument (eu and ev only while edges is above 0; scratch only while nodes is above 0; comp
// only while nodes is above 0; out_count always; the conditions use the given sizes even when they are above the limits) gives NullArgument; then
// nodes above kMaxNodes or edges above kMaxEdges gives TooLarge; then, only while nodes is above 0, scratch_words below 6 * nodes + edges + 1
// gives ScratchTooSmall; then BadNode when any edge has eu or ev not below nodes (every edge is checked, in one pass, before any write; with
// nodes 0 any edge is a BadNode). On any failure *out_count and comp are untouched. On success only *out_count and comp[0 .. nodes) are written,
// and scratch is used as working memory with unspecified contents afterwards (not tested).
SccResult scc_run(std::uint32_t nodes, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev, std::uint32_t* scratch,
                  std::uint32_t scratch_words, std::uint32_t* comp, std::uint32_t* out_count);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::scc
