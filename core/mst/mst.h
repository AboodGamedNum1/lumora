// Minimum spanning forest of an undirected weighted graph given as an edge list (Kruskal order with a fixed tie rule), in caller storage.
// Pure integer: no floating point, no libm, no 128-bit types, no compiler builtins or intrinsics (MSVC), no allocation, no recursion, no global
// state. The sort and the component handling must have a worst case of O((n + m) log m) or better: no quadratic method. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::mst {
constexpr std::uint32_t kMaxNodes = 65536;   // largest node count
constexpr std::uint32_t kMaxEdges = 262144;  // largest edge count
enum class MstResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutputTooSmall = 4, BadEdge = 5 };
// Graph: nodes are 0 .. nodes-1. Edge i joins eu[i] and ev[i] with the weight ew[i] (u32), for i in 0 .. edges-1. Edges are undirected; parallel
// edges and self-loops (eu[i] == ev[i]) are allowed; the graph need not be connected. The caller must not make any two arrays overlap.
//
// RESULT: the edge indices of a minimum spanning FOREST selected by this rule: process the edges ordered by (weight, then edge index), both
// ascending; take an edge if and only if its endpoints are in different components of the edges taken so far (a self-loop is never taken).
// The result is unique under that rule. Output order: the edges in that processing order (so the weights never decrease along the output).
// *out_count = the number of edges taken (nodes minus the number of components, so 0 for nodes 0 or 1 or when only self-loops exist).
// *out_weight = the sum of their weights as u64 (it cannot overflow: at most 65535 edges of at most 2^32 - 1).
//
// Sizes: *out_scratch_words = nodes + edges (u32 words); the forest holds at most nodes - 1 edges, so *out_forest_words = nodes - 1 for
// nodes above 0 and 0 for nodes 0 (the output capacity every call must offer; it does not depend on the graph). Check order of mst_sizes: a null
// out pointer gives NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge (nothing written).
MstResult mst_sizes(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_scratch_words, std::uint32_t* out_forest_words);
// Computes the forest. Check order: a null pointer argument (eu, ev, ew only while edges is above 0; scratch only while nodes + edges is above 0;
// out_idx only while nodes is above 1; out_count and out_weight always; these conditions use the given nodes and edges even when they are
// above the limits) gives NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge; then scratch_words below
// nodes + edges gives ScratchTooSmall; then out_capacity below the forest words gives OutputTooSmall; then any edge with eu or ev not below
// nodes gives BadEdge (so every edge is a BadEdge when nodes is 0 and edges is above 0). On any failure out_idx, *out_count and *out_weight are
// untouched. On success only out_idx[0 .. *out_count), *out_count and *out_weight are written (out_idx beyond the count keeps its old values),
// and scratch is used as working memory with unspecified contents afterwards (not tested). No other memory is written.
MstResult mst_forest(std::uint32_t nodes, std::uint32_t edges, const std::uint32_t* eu, const std::uint32_t* ev, const std::uint32_t* ew,
                     std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_idx, std::uint32_t out_capacity,
                     std::uint32_t* out_count, std::uint64_t* out_weight);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.
}  // namespace lumora::mst
