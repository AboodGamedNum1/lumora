// Maximum flow in a directed graph with u32 edge capacities (value and the source side of the minimum cut), in caller storage. Pure integer: no
// floating point, no libm, no 128-bit types, no compiler builtins or intrinsics (MSVC), no allocation, no recursion, no global state. The number
// of steps must NOT depend on the capacity values (so a capacity of 4294967295 must not cost 4294967295 augmentations): use a method with a
// polynomial worst case in nodes and edges, for example Dinic's or Edmonds-Karp. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::maxflow {
constexpr std::uint32_t kMaxNodes = 4096;    // largest node count
constexpr std::uint32_t kMaxEdges = 65536;   // largest edge count
enum class FlowResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadNode = 4, SameTerminal = 5 };
// Graph: nodes 0 .. nodes-1. Directed edge i goes from eu[i] to ev[i] with the capacity ecap[i] (u32, may be 0). Parallel edges, antiparallel
// edges and self-loops (eu[i] == ev[i], which carry no flow) are allowed. The caller must not make any two arrays overlap.
//
// RESULT: *out_flow = the value of a maximum flow from src to sink, as u64 (at most 65536 * (2^32 - 1), so it cannot overflow). out_side[v] for
// v in 0 .. nodes-1 = 1 if v is reachable from src in the residual graph of a maximum flow, else 0. This set is the SAME for every maximum flow
// (it is the source side of the unique minimum cut closest to src), so it is fully determined by the graph; src always gets 1; the sink always
// gets 0. The individual edge flows are not part of the result.
//
// *out_scratch_words = the scratch size in u32 words that maxflow_run needs: 8 * edges + 6 * nodes. Check order of maxflow_scratch_words: a null
// out_scratch_words gives NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge (nothing written).
FlowResult maxflow_scratch_words(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_scratch_words);
// Computes the maximum flow. Check order: a null pointer argument (eu, ev and ecap only while edges is above 0; scratch only while nodes + edges is
// above 0; out_side only while nodes is above 0; out_flow always; the conditions use the given sizes even when they are above the limits) gives
// NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge; then scratch_words below 8 * edges + 6 * nodes gives
// ScratchTooSmall; then BadNode when src or sink is not below nodes, or any edge has eu or ev not below nodes (every edge is checked, in one pass,
// before any write); then SameTerminal when src equals sink. On any failure *out_flow and out_side are untouched. On success only *out_flow and
// out_side[0 .. nodes) are written, and scratch is used as working memory with unspecified contents afterwards (not tested).
FlowResult maxflow_run(std::uint32_t nodes, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev, const std::uint32_t* ecap,
                       std::uint32_t src, std::uint32_t sink, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_flow,
                       std::uint8_t* out_side);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::maxflow
