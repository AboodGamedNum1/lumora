// Lumora 2D sprite batcher (scope K). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/batch/batch.cpp only.
// It builds vertex and index buffers for a renderer from a list of sprites. It uses the transform module
// (xform.h) and nothing else from the project. Results must be bit-identical on Linux, Windows and Android:
// only + - * / sqrt, double internally, FMA contraction off, no libm transcendentals, and no constant division by
// zero or constant overflow (MSVC rejects those at compile time).
#pragma once
#include <cstddef>
#include <cstdint>

#include "xform.h"

namespace lumora::batch {

using xform::Affine;
using xform::Rect;
using xform::Vec2;

enum class Blend : std::uint8_t { Alpha = 0, Additive = 1, Opaque = 2 };

struct Sprite {
  Affine transform;          // local to world, applied to the local quad (0,0)-(size.x,size.y)
  Vec2 size{1, 1};           // local quad size, > 0
  Rect uv{0, 0, 1, 1};       // min_x min_y max_x max_y in texture space, any values (wrapping is the shader's job)
  std::uint8_t tint[4]{255, 255, 255, 255};  // straight RGBA8
  std::uint32_t texture = 0; // opaque texture id
  std::int32_t layer = 0;    // lower layers are drawn first
  Blend blend = Blend::Alpha;
};

struct Vertex {  // 20 bytes, no padding
  float x, y;
  float u, v;
  std::uint8_t rgba[4];
};
static_assert(sizeof(Vertex) == 20, "Vertex must be tightly packed");

struct DrawBatch {
  std::uint32_t texture;
  Blend blend;
  std::uint32_t first_index;  // offset into the index buffer
  std::uint32_t index_count;  // multiple of 6
};

constexpr std::size_t kMaxSprites = 16384;

enum class BatchStatus : std::uint8_t {
  Ok = 0,
  TooManySprites,  // count > kMaxSprites
  BadSprite,       // non-finite transform, size not finite or <= 0, non-finite uv, bad blend value
  BadViewBounds,   // cull rectangle given but min > max or non-finite
  BufferTooSmall,  // an output capacity is smaller than required
};
const char* batch_status_text(BatchStatus s);

struct BatchResult {
  BatchStatus status = BatchStatus::Ok;
  std::size_t sprites_drawn = 0;   // after culling
  std::size_t sprites_culled = 0;
  std::size_t vertex_count = 0;    // 4 per drawn sprite
  std::size_t index_count = 0;     // 6 per drawn sprite
  std::size_t batch_count = 0;
};

// Build the buffers. Steps, in this order:
// 1. Validate every sprite (status order as in the enum, first bad sprite decides). On error nothing is written.
// 2. Cull: if `cull` is not null, a sprite whose transformed axis-aligned bounds (xform::transform_rect of its local
//    quad) do not intersect the closed `cull` rectangle is dropped. Touching counts as intersecting.
// 3. Stable order: sort the remaining sprites by (layer, blend, texture), ties keep input order. Alpha blended sprites
//    must keep strict input order within a layer: Alpha sprites are sorted by layer only (stable), not by texture.
//    Additive and Opaque sprites of one layer are grouped by (blend, texture). Within a layer the group order is
//    Opaque first, then Additive, then Alpha.
// 4. Vertices per sprite in order: (0,0), (size.x,0), (size.x,size.y), (0,size.y) transformed by the sprite matrix
//    in double and rounded once to float; uv corners map (min_x,min_y), (max_x,min_y), (max_x,max_y), (min_x,max_y).
//    Indices: base+0, base+1, base+2, base+0, base+2, base+3.
// 5. Merge consecutive sprites with equal (texture, blend) into one DrawBatch, in final output order, also across
//    layer boundaries (a batch carries no layer).
// Capacities are element counts. Required: 4 per drawn sprite (vertices), 6 per drawn sprite (indices) and exactly the
// merged batch count (batches). Validation order: TooManySprites, first BadSprite, BadViewBounds (checked whenever `cull`
// is not null, even if nothing is culled), then BufferTooSmall. sprites == nullptr is allowed only when count == 0.
// On BufferTooSmall nothing is written but the result carries the required counts. After an earlier failure all counts
// are 0. An output pointer may be null only when its required count is 0; otherwise the result is BufferTooSmall.
BatchResult build_batches(const Sprite* sprites, std::size_t count, const Rect* cull, Vertex* out_vertices,
                          std::size_t vertex_capacity, std::uint32_t* out_indices, std::size_t index_capacity,
                          DrawBatch* out_batches, std::size_t batch_capacity);

// FNV-1a 64 over the bytes of the vertices (as laid out), then the indices (little-endian 4 bytes each),
// then for each batch texture, blend, first_index, index_count (4 bytes each, little-endian), for golden tests.
std::uint64_t digest_buffers(const Vertex* v, std::size_t vn, const std::uint32_t* idx, std::size_t in,
                             const DrawBatch* b, std::size_t bn);

}  // namespace lumora::batch
