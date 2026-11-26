// Lumora CPU spatial index (scope T). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/spatial/spatial.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no
// floats, no unordered containers, no static mutable state, no heap allocation, no pointer values in any decision. Purpose:
// hit testing, culling and nearest queries over many 2D boxes (sprites, particles, UI) for the wallpaper engine.
// Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::spatial {

constexpr std::uint32_t kMaxItems = 1u << 20;
constexpr std::uint32_t kMaxLeaf = 16;
constexpr std::int32_t kMaxCoord = 1 << 28;  // |coordinate| <= kMaxCoord for every box and query value

// Closed box: contains every point with x0 <= x <= x1 and y0 <= y <= y1. A box with x0 > x1 or y0 > y1 is invalid.
struct Box {
  std::int32_t x0, y0, x1, y1;
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,       // a required pointer is null while its count is > 0 (items, nodes, order, out, result pointers)
  BadCount,         // item_count > kMaxItems, or leaf_size is 0 or > kMaxLeaf
  BadBox,           // a box is invalid or has a coordinate beyond kMaxCoord in magnitude
  BufferTooSmall,   // node_capacity < node_bound(item_count, leaf_size) (the bound is exact), or a query output buffer is too small
  BadIndex          // the index passed to a query was not produced by build for these items (checked cheaply, see docs)
};
// Lower-case spaced enum names ("ok", "null buffer", "bad count", "bad box", "buffer too small", "bad index"), "unknown" otherwise.
const char* status_text(Status s);

struct Node {
  Box box;               // tight bounds of every item below this node
  std::uint32_t first;   // leaf: first position in the order array; inner: index of the left child
  std::uint32_t count;   // leaf: number of items (1..leaf_size); inner: 0
  std::uint32_t right;   // inner: index of the right child; leaf: 0
};

// Exact number of nodes build writes for these parameters, defined by the split rule below: a range of n items is a leaf when
// n <= leaf_size, otherwise it has 1 + nodes(n / 2) + nodes(n - n / 2) nodes. Returns 0 for item_count 0 or an invalid
// leaf_size (item_count > kMaxItems, leaf_size 0 or > kMaxLeaf). It must be computed without recursion and without heap.
std::uint32_t node_bound(std::uint32_t item_count, std::uint32_t leaf_size);

// Builds a binary tree over the boxes in caller memory. Deterministic rule (fixed here, details in docs/spatial.md):
//  - order[0 .. n) starts as 0 .. n - 1. A range with count <= leaf_size becomes a leaf.
//  - Otherwise the tight bounds of the range are computed; the axis is the one with the larger extent (x if equal); the range
//    is sorted by the key (x0 + x1 for the x axis, y0 + y1 for y, as int64) then by item index, and split at count / 2
//    (left gets count / 2 items, the right gets the rest).
//  - Nodes are numbered in depth first order, parent before children, left subtree before right.
// Checks in enum order: NullBuffer, BadCount, BadBox (items in index order), BufferTooSmall. Nothing is written on error.
// `items` must stay unchanged and alive for the queries. Writes node_count (may be null only if the caller does not need it).
Status build(const Box* items, std::uint32_t item_count, std::uint32_t leaf_size, Node* nodes, std::uint32_t node_capacity,
             std::uint32_t* order, std::uint32_t* node_count);

// Items whose box intersects the closed query box q (touching counts). Writes their indices in ASCENDING index order to out
// (capacity out_capacity) and the total number of matches to *found, even when out is too small (then BufferTooSmall and out
// holds the first out_capacity matches in ascending order). Checks: NullBuffer, BadBox (q invalid), BadIndex, BufferTooSmall.
Status query_box(const Box* items, std::uint32_t item_count, const Node* nodes, std::uint32_t node_count, const std::uint32_t* order,
                 const Box& q, std::uint32_t* out, std::uint32_t out_capacity, std::uint32_t* found);

// Items whose box contains the point (touching counts), same output rules as query_box.
Status query_point(const Box* items, std::uint32_t item_count, const Node* nodes, std::uint32_t node_count, const std::uint32_t* order,
                   std::int32_t px, std::int32_t py, std::uint32_t* out, std::uint32_t out_capacity, std::uint32_t* found);

// Nearest item to the point: the smallest squared distance (exact, int64) from the point to the box (0 inside or touching);
// ties go to the lowest item index. Writes *index and *dist2. Returns Ok with *found false when there are no items.
// Checks: NullBuffer, BadIndex.
Status nearest(const Box* items, std::uint32_t item_count, const Node* nodes, std::uint32_t node_count, const std::uint32_t* order,
               std::int32_t px, std::int32_t py, std::uint32_t* index, std::uint64_t* dist2, bool* found);

// FNV-1a 64 over the 32-bit little-endian bytes of each value in order, for golden tests. Null or zero count hashes zero bytes.
std::uint64_t digest_indices(const std::uint32_t* values, std::size_t count);

}  // namespace lumora::spatial
