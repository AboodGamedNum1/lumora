// Eulerian path or circuit of a directed multigraph.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion
// (a path can be 262144 edges long), no global state, no large automatic arrays (working memory is caller scratch). The number of steps must be linear
// in n + m (for example Hierholzer's algorithm); a slower method is not acceptable for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::euler {
constexpr std::uint32_t kMaxNodes = 65536;   // largest number of nodes
constexpr std::uint32_t kMaxEdges = 262144;  // largest number of edges
enum class EulerResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadNode = 4 };
enum class EulerKind : std::uint8_t { None = 0, Path = 1, Circuit = 2 };
// A directed multigraph with nodes 0 .. n-1 and edges 0 .. m-1; edge i goes from eu[i] to ev[i]. Self-loops and parallel edges are allowed. An Eulerian
// trail uses every edge exactly once, as a sequence of edges e1 .. em in which the head of each edge is the tail of the next. Nodes without edges are
// ignored. A Path is a trail whose first tail differs from its last head; a Circuit is a trail that ends where it starts.
//
// euler_scratch_words: *out_words = the scratch size in u32 words that euler_trail needs: 6 * n + 3 * m + 8 (the algorithm may use less; scratch is
// working memory with unspecified contents afterwards, not tested). Check order: a null out_words gives NullArgument; then n above kMaxNodes or m above
// kMaxEdges gives TooLarge (nothing written).
EulerResult euler_scratch_words(std::uint32_t n, std::uint32_t m, std::uint32_t* out_words);
// euler_trail: on Ok, *out_kind says what exists and *out_len is the number of edges written.
//   Circuit: a closed trail exists (every node has equal in-degree and out-degree, and all edges lie in one weakly connected component). out_edges[0 .. m)
//   holds the edge numbers of one such trail in order; *out_len = m. Which node it starts at, and which circuit, is NOT specified (a test checks only that
//   it is a valid trail). With m = 0 the kind is Circuit and *out_len = 0 and nothing is written to out_edges.
//   Path: an open trail exists (all edges in one weakly connected component, one node has out-degree minus in-degree equal to 1, one node has in-degree
//   minus out-degree equal to 1, every other node has equal degrees). out_edges[0 .. m) holds one such trail in order, which must start at the node
//   with out-degree above in-degree; *out_len = m. Which trail is NOT specified.
//   None: no trail exists. *out_len = 0 and out_edges is untouched.
// Check order: a null pointer argument gives NullArgument (eu, ev and out_edges only while m is above 0; scratch only while n is above 0; out_kind and
// out_len always); then n above kMaxNodes or m above kMaxEdges gives TooLarge (the condition uses the given values even when above the limits); then,
// only while n is above 0, scratch_words below 6 * n + 3 * m + 8 gives ScratchTooSmall; then an endpoint at or above n in eu or ev gives BadNode (one
// pass over all edges; with n = 0 any m above 0 gives BadNode). Nothing is written on any failure. n = 0 with m = 0 gives Ok, Circuit and *out_len = 0.
EulerResult euler_trail(std::uint32_t n, std::uint32_t m, const std::uint32_t* eu, const std::uint32_t* ev, std::uint32_t* scratch,
                        std::uint32_t scratch_words, std::uint32_t* out_edges, std::uint32_t* out_len, EulerKind* out_kind);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::euler
