// Single-source shortest paths in a directed graph with positive u32 edge weights, given as an edge list: the distance to every node, the number of
// shortest paths and a canonical predecessor. In caller storage. Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler
// builtins or intrinsics: MSVC), no allocation, no recursion, no global state. The worst case must be O((n + m) log n) or better (a binary heap in
// scratch, for example): no quadratic method for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::sssp {
constexpr std::uint32_t kMaxNodes = 65536;   // largest node count
constexpr std::uint32_t kMaxEdges = 262144;  // largest edge count
constexpr std::uint64_t kInf = 0xFFFFFFFFFFFFFFFFull;  // distance of an unreachable node
constexpr std::uint32_t kNoNode = 0xFFFFFFFFu;         // predecessor of the source and of an unreachable node
enum class SsspResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadNode = 4, BadWeight = 5 };
// Graph: nodes 0 .. n-1; edge i goes from eu[i] to ev[i] with weight w[i], which must be at least 1 (a weight of 0 is BadWeight). Parallel edges and
// self-loops are allowed (a self-loop never helps; parallel edges count as separate paths). The caller must not make any two arrays overlap.
//
// RESULT for the source s: dist[v] = the length of a shortest path from s to v (a sum of u64 weights; the largest possible value is below 2^48, so
// it never overflows), 0 for v = s, kInf when v is not reachable. count[v] = the number of different shortest paths from s to v, taken modulo 2^64
// (a path is a sequence of EDGES, so two parallel edges give two paths; count[s] = 1; count is 0 exactly for an unreachable node). parent[v] = for
// a reachable v other than s, the smallest node index u such that some edge from u to v with weight x has dist[u] + x = dist[v] (a node on some
// shortest path to v, one step before v; the smallest index among all of them); kNoNode for s and for unreachable nodes.
//
// *out_words = the scratch size in u32 words: 8 * n + 3 * m + 2 (the algorithm may use less). Check order of sssp_scratch_words: a null out_words gives
// NullArgument; then n above kMaxNodes or m above kMaxEdges gives TooLarge (nothing written).
SsspResult sssp_scratch_words(std::uint32_t n, std::uint32_t m, std::uint32_t* out_words);
// Computes the result. Check order: a null pointer argument (eu, ev and w only while m is above 0; scratch, dist, count and parent only while n is
// above 0; the conditions use the given sizes even when above the limits) gives NullArgument; then n above kMaxNodes or m above kMaxEdges gives
// TooLarge; then, only while n is above 0, scratch_words below 8 * n + 3 * m + 2 gives ScratchTooSmall; then BadNode when source is not below n
// (with n = 0 any source is a BadNode) or any edge has eu or ev not below n; then BadWeight when any weight is 0 (every edge is checked, in one
// pass, before any write; BadNode wins over BadWeight). On any failure all outputs are untouched. On success only dist[0 .. n), count[0 .. n) and
// parent[0 .. n) are written; scratch holds unspecified contents afterwards (not tested).
SsspResult sssp_run(std::uint32_t n, std::uint32_t m, const std::uint32_t* eu, const std::uint32_t* ev, const std::uint32_t* w, std::uint32_t source,
                    std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* dist, std::uint64_t* count, std::uint32_t* parent);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::sssp
