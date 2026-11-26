// 2D sprite batcher. Rules, ordering, merging and golden digests: docs/batch.md.
// Determinism: positions come from xform::apply (double evaluation, one float rounding); sorting is a stable sort on
// an integer key; no libm function is called. Compilers are asked not to fuse multiply-add.
#include "batch.h"

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::batch {
namespace {

inline bool is_finite(float v) { return v - v == 0.0f; }  // false for NaN and infinity

bool valid_blend(Blend b) {
  const std::uint8_t v = static_cast<std::uint8_t>(b);
  return v <= 2;
}

bool bad_sprite(const Sprite& s) {
  const Affine& m = s.transform;
  if (!is_finite(m.a) || !is_finite(m.b) || !is_finite(m.c) || !is_finite(m.d) || !is_finite(m.tx) || !is_finite(m.ty)) {
    return true;
  }
  if (!is_finite(s.size.x) || !is_finite(s.size.y) || !(s.size.x > 0.0f) || !(s.size.y > 0.0f)) return true;
  if (!is_finite(s.uv.min_x) || !is_finite(s.uv.min_y) || !is_finite(s.uv.max_x) || !is_finite(s.uv.max_y)) return true;
  return !valid_blend(s.blend);
}

// Group rank within a layer: Opaque, then Additive, then Alpha.
inline int rank_of(Blend b) { return b == Blend::Opaque ? 0 : (b == Blend::Additive ? 1 : 2); }

struct Hash {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u32(std::uint32_t v) {
    for (int k = 0; k < 4; ++k) byte(static_cast<std::uint8_t>((v >> (8 * k)) & 0xffu));
  }
  void f32(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    u32(u);
  }
};

}  // namespace

const char* batch_status_text(BatchStatus s) {
  switch (s) {
    case BatchStatus::Ok: return "ok";
    case BatchStatus::TooManySprites: return "too many sprites";
    case BatchStatus::BadSprite: return "bad sprite";
    case BatchStatus::BadViewBounds: return "bad view bounds";
    case BatchStatus::BufferTooSmall: return "buffer too small";
  }
  return "unknown";
}

BatchResult build_batches(const Sprite* sprites, std::size_t count, const Rect* cull, Vertex* out_vertices,
                          std::size_t vertex_capacity, std::uint32_t* out_indices, std::size_t index_capacity,
                          DrawBatch* out_batches, std::size_t batch_capacity) {
  BatchResult res;
  if (count > kMaxSprites) {
    res.status = BatchStatus::TooManySprites;
    return res;
  }
  if (count > 0 && sprites == nullptr) {
    res.status = BatchStatus::BadSprite;
    return res;
  }
  for (std::size_t i = 0; i < count; ++i) {
    if (bad_sprite(sprites[i])) {
      res.status = BatchStatus::BadSprite;
      return res;
    }
  }
  if (cull != nullptr) {
    const Rect& c = *cull;
    if (!is_finite(c.min_x) || !is_finite(c.min_y) || !is_finite(c.max_x) || !is_finite(c.max_y) || c.min_x > c.max_x ||
        c.min_y > c.max_y) {
      res.status = BatchStatus::BadViewBounds;
      return res;
    }
  }

  // Cull. Closed rectangles: touching intersects.
  std::vector<std::uint32_t> order;
  order.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const Sprite& s = sprites[i];
    if (cull != nullptr) {
      const Rect b = xform::transform_rect(s.transform, Rect{0.0f, 0.0f, s.size.x, s.size.y});
      const bool hit = b.max_x >= cull->min_x && b.min_x <= cull->max_x && b.max_y >= cull->min_y && b.min_y <= cull->max_y;
      if (!hit) continue;
    }
    order.push_back(static_cast<std::uint32_t>(i));
  }

  // Stable order by (layer, group rank, texture for Opaque and Additive). Alpha sprites ignore the texture.
  std::stable_sort(order.begin(), order.end(), [sprites](std::uint32_t x, std::uint32_t y) {
    const Sprite& a = sprites[x];
    const Sprite& b = sprites[y];
    if (a.layer != b.layer) return a.layer < b.layer;
    const int ra = rank_of(a.blend), rb = rank_of(b.blend);
    if (ra != rb) return ra < rb;
    if (ra == 2) return false;
    return a.texture < b.texture;
  });

  const std::size_t drawn = order.size();
  std::size_t batches = 0;
  for (std::size_t k = 0; k < drawn; ++k) {
    const Sprite& s = sprites[order[k]];
    if (k == 0 || s.texture != sprites[order[k - 1]].texture || s.blend != sprites[order[k - 1]].blend) ++batches;
  }

  res.sprites_drawn = drawn;
  res.sprites_culled = count - drawn;
  res.vertex_count = 4 * drawn;
  res.index_count = 6 * drawn;
  res.batch_count = batches;
  if (vertex_capacity < res.vertex_count || index_capacity < res.index_count || batch_capacity < batches ||
      (res.vertex_count > 0 && out_vertices == nullptr) || (res.index_count > 0 && out_indices == nullptr) ||
      (batches > 0 && out_batches == nullptr)) {
    res.status = BatchStatus::BufferTooSmall;  // nothing is written; the counts above are the required sizes
    return res;
  }

  std::size_t bi = 0;
  for (std::size_t k = 0; k < drawn; ++k) {
    const Sprite& s = sprites[order[k]];
    const Vec2 corners[4] = {{0.0f, 0.0f}, {s.size.x, 0.0f}, {s.size.x, s.size.y}, {0.0f, s.size.y}};
    const float us[4] = {s.uv.min_x, s.uv.max_x, s.uv.max_x, s.uv.min_x};
    const float vs[4] = {s.uv.min_y, s.uv.min_y, s.uv.max_y, s.uv.max_y};
    const std::uint32_t base = static_cast<std::uint32_t>(4 * k);
    for (int j = 0; j < 4; ++j) {
      const Vec2 p = xform::apply(s.transform, corners[j]);
      Vertex& v = out_vertices[base + static_cast<std::uint32_t>(j)];
      v.x = p.x;
      v.y = p.y;
      v.u = us[j];
      v.v = vs[j];
      for (int c = 0; c < 4; ++c) v.rgba[c] = s.tint[c];
    }
    std::uint32_t* ix = out_indices + 6 * k;
    ix[0] = base + 0;
    ix[1] = base + 1;
    ix[2] = base + 2;
    ix[3] = base + 0;
    ix[4] = base + 2;
    ix[5] = base + 3;
    if (k == 0 || s.texture != out_batches[bi - 1].texture || s.blend != out_batches[bi - 1].blend) {
      out_batches[bi].texture = s.texture;
      out_batches[bi].blend = s.blend;
      out_batches[bi].first_index = static_cast<std::uint32_t>(6 * k);
      out_batches[bi].index_count = 0;
      ++bi;
    }
    out_batches[bi - 1].index_count += 6;
  }
  return res;
}

std::uint64_t digest_buffers(const Vertex* v, std::size_t vn, const std::uint32_t* idx, std::size_t in, const DrawBatch* b,
                             std::size_t bn) {
  Hash h;
  for (std::size_t i = 0; i < vn; ++i) {
    h.f32(v[i].x);
    h.f32(v[i].y);
    h.f32(v[i].u);
    h.f32(v[i].v);
    for (int c = 0; c < 4; ++c) h.byte(v[i].rgba[c]);
  }
  for (std::size_t i = 0; i < in; ++i) h.u32(idx[i]);
  for (std::size_t i = 0; i < bn; ++i) {
    h.u32(b[i].texture);
    h.u32(static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[i].blend)));
    h.u32(b[i].first_index);
    h.u32(b[i].index_count);
  }
  return h.h;
}

}  // namespace lumora::batch
