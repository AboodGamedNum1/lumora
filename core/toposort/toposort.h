// Deterministic topological order of a small directed graph given as an edge list. Pure integer: no floating point, no libm, no 128-bit types
// or builtins (MSVC), no allocation, no recursion, no global state. All working memory is caller scratch. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::toposort {

constexpr std::uint32_t kMaxNodes = 1u << 16;  // largest node count
constexpr std::uint32_t kMaxEdges = 1u << 20;  // largest edge count

enum class TopoResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BadEdge = 3, ScratchTooSmall = 4, Cycle = 5 };

// Scratch size in u32 words that toposort_u32 must work within for this graph size: *out_words = 3 * nodes + edges + 1 (the algorithm may use
// less). NullArgument when out_words is null; TooLarge when nodes is above kMaxNodes or edges above kMaxEdges (nothing written).
TopoResult toposort_scratch_words(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_words);

// Nodes are 0 .. nodes-1. Edge k is from_to[2k] -> from_to[2k+1] (so from_to holds 2 * edges words); self-loops and duplicate edges are
// allowed (a self-loop is a cycle; duplicates count once for ordering). Writes into order[0 .. nodes) the order in which, at each step, the
// SMALLEST-numbered node whose predecessors are all already placed comes next (the lexicographically smallest topological order).
// Check order: a null pointer argument (from_to counts only while edges is above 0, order only while nodes is above 0, scratch only while nodes is above 0) gives
// NullArgument; then nodes above kMaxNodes or edges above kMaxEdges gives TooLarge; then an edge endpoint that is not below nodes gives
// BadEdge; then, only when nodes is above 0, scratch_words below the value of toposort_scratch_words gives ScratchTooSmall (nodes 0 needs no scratch; edges are then still range-checked: any edge is a BadEdge); then a graph with a cycle gives Cycle. On any
// failure order is left untouched (scratch contents are unspecified). nodes 0 succeeds with nothing written.
TopoResult toposort_u32(std::uint32_t nodes, const std::uint32_t* from_to, std::uint32_t edges, std::uint32_t* order,
                        std::uint32_t* scratch, std::uint32_t scratch_words);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::toposort
