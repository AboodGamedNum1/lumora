// Bridges and articulation points of an undirected graph given as an edge list, in caller storage. Pure integer: no floating point, no libm, no
// 128-bit types, no compiler builtins or intrinsics (MSVC), no allocation, no recursion (explicit stacks in scratch), no global state. The number of
// steps must be linear in nodes plus edges. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::bridges {
constexpr std::uint32_t kMaxNodes = 65536;    // largest node count
constexpr std::uint32_t kMaxEdges = 262144;   // largest edge count
enum class BridgeResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadNode = 4 };
// Graph: nodes 0 .. nodes-1. Undirected edge i joins eu[i] and ev[i] (the order of the two ends does not matter). Parallel edges and self-loops are
// allowed. The caller must not make any two arrays overlap.
//
// DEFINITIONS (exact, so the result is fully determined by the graph). An edge is a BRIDGE when removing that one edge (and only it) increases the
// number of connected components. A self-loop is never a bridge. Two parallel edges are never bridges (each one keeps the other's ends connected),
// while a single edge with no parallel twin can be. A node is an ARTICULATION POINT when removing that node (and every edge at it, loops included)
// increases the number of connected components among the REMAINING nodes. An isolated node, and a node whose only edges are loops, is never one.
//
// RESULT: is_bridge[i] for i in 0 .. edges-1 is 1 if edge i is a bridge, else 0. is_cut[v] for v in 0 .. nodes-1 is 1 if node v is an articulation
// point, else 0. *out_bridges = the number of bridges and *out_cuts = the number of articulation points. With edges 0 nothing is written to is_bridge
// and with nodes 0 nothing is written to is_cut.
//
// *out_words = the scratch size in u32 words that bridges_run needs: 8 * nodes + 2 * edges + 1 (the algorithm may use less). Check order of
// bridges_scratch_words: a null out_words gives NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge (nothing written).
BridgeResult bridges_scratch_words(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_words);
// Computes the result. Check order: a null pointer argument (eu and ev only while edges is above 0; is_bridge only while edges is above 0; scratch
// and is_cut only while nodes is above 0; out_bridges and out_cuts always; the conditions use the given sizes even when they are above the limits)
// gives NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge; then, only while nodes is above 0, scratch_words below
// 8 * nodes + 2 * edges + 1 gives ScratchTooSmall; then BadNode when any edge has eu or ev not below nodes (every edge is checked, in one pass,
// before any write; with nodes 0 any edge is a BadNode). On any failure all outputs are untouched. On success only is_bridge[0 .. edges),
// is_cut[0 .. nodes), *out_bridges and *out_cuts are written, and scratch is used as working memory with unspecified contents afterwards (not tested).
BridgeResult bridges_run(std::uint32_t nodes, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev, std::uint32_t* scratch,
                         std::uint32_t scratch_words, std::uint8_t* is_bridge, std::uint8_t* is_cut, std::uint32_t* out_bridges,
                         std::uint32_t* out_cuts);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::bridges
