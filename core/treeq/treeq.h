// Queries on a rooted forest given as a parent array: depth, subtree size, lowest common ancestor and the longest path. In caller storage. Pure
// integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion (explicit
// loops over caller scratch), no global state. The number of steps must be polynomial in the sizes. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::treeq {
constexpr std::uint32_t kMaxNodes = 65536;    // largest number of nodes
constexpr std::uint32_t kMaxQueries = 65536;  // largest number of queries
constexpr std::uint32_t kNoParent = 0xFFFFFFFFu;
enum class TreeResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadParent = 4, BadQuery = 5 };
// Forest: nodes 0 .. n-1; parent[v] is the parent of v, or kNoParent for a root. A valid forest has every parent[v] below n or equal to kNoParent,
// no node that is its own parent, and no cycle (every node reaches a root). The parent of a node may have a LARGER or smaller index (no ordering is
// promised). An empty forest (n = 0) is valid. A forest is allowed to have many roots.
//
// *out_words = the scratch size in u32 words: 3 * n + 1 (the algorithm may use less). Check order: a null out_words gives NullArgument; then n above
// kMaxNodes gives TooLarge (nothing written).
TreeResult treeq_scratch_words(std::uint32_t n, std::uint32_t* out_words);
// Depth and size: depth[v] = the number of edges from v up to its root (a root has 0), size[v] = the number of nodes in the subtree of v, v included
// (a leaf has 1). Check order: a null pointer argument (parent, scratch, depth and size only while n is above 0; the condition uses the given n even
// when it is above the limit) gives NullArgument; then n above kMaxNodes gives TooLarge; then, only while n is above 0, scratch_words below 3 * n + 1
// gives ScratchTooSmall; then BadParent when a parent value is neither kNoParent nor below n, a node is its own parent, or a cycle exists (the
// whole array is checked before any write). On any failure all outputs are untouched. On success only depth[0 .. n) and size[0 .. n) are written;
// scratch holds unspecified contents afterwards. With n = 0 nothing is written and the result is Ok.
TreeResult treeq_depth_size(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* depth,
                            std::uint32_t* size);
// Lowest common ancestor: for each query i, (qa[i], qb[i]) is a pair of nodes, and out[i] = the deepest node that is an ancestor of both (a node is
// its own ancestor, so lca(v, v) = v and lca(u, v) = u when u is an ancestor of v), or kNoParent when the two nodes are in different trees of the
// forest. Check order: a null pointer argument (parent, scratch only while n is above 0; qa, qb and out only while q is above 0; the conditions use
// the given sizes even when above the limits) gives NullArgument; then n above kMaxNodes or q above kMaxQueries gives TooLarge; then, only while
// n is above 0, scratch_words below 3 * n + 1 gives ScratchTooSmall; then BadParent as above; then BadQuery when any qa[i] or qb[i] is not below n (every
// query is checked before any write; with n = 0 any query is a BadQuery). On any failure out is untouched. On success only out[0 .. q) is written.
TreeResult treeq_lca(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t q,
                     const std::uint32_t* qa, const std::uint32_t* qb, std::uint32_t* out);
// Diameter: *out_len = the number of edges on the longest path between two nodes of the same tree (0 for a forest in which every tree is a single
// node and for n = 0), and *out_a, *out_b = the endpoints of one such path with a <= b; when several paths have that length, the pair that is
// smallest by (a, then b) is reported. For a forest in which every tree is a single node the pair is (0, 0) when n > 0 and nothing is written for
// n = 0 except *out_len = 0. Check order: as for treeq_depth_size with outputs out_len (always), out_a and out_b (only while n is above 0).
TreeResult treeq_diameter(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_len,
                          std::uint32_t* out_a, std::uint32_t* out_b);
// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::treeq
