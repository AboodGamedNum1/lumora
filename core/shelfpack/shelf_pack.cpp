// Lumora shelf allocator (scope AD). Integer only, no heap, no recursion, no mutable statics. See docs/shelf_pack.md.
#include "shelf_pack.h"

namespace lumora::shelfpack {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

void clear_state(ShelfPacker& p) {
  p.shelf_count = 0;
  p.next_y = 0;
  p.used_area = 0;
  for (u32 i = 0; i < kMaxShelves; ++i) p.shelves[i] = Shelf{};
}

}  // namespace

bool packer_init(ShelfPacker* packer, std::uint32_t width, std::uint32_t height) {
  if (packer == nullptr || width == 0 || height == 0 || width > kMaxAtlas || height > kMaxAtlas) return false;
  packer->width = width;
  packer->height = height;
  clear_state(*packer);
  return true;
}

bool packer_reset(ShelfPacker* packer) {
  if (packer == nullptr || packer->width == 0 || packer->height == 0) return false;
  clear_state(*packer);
  return true;
}

AllocResult packer_alloc(ShelfPacker* packer, std::uint32_t w, std::uint32_t h, std::uint32_t* x, std::uint32_t* y) {
  if (packer == nullptr || x == nullptr || y == nullptr) return AllocResult::NullArgument;
  ShelfPacker& p = *packer;
  if (p.width == 0 || p.height == 0) return AllocResult::NotInitialized;
  // The cell is computed in 64 bits so that w or h near 2^32 cannot wrap.
  const u64 cell_w = static_cast<u64>(w) + kPad;
  const u64 cell_h = static_cast<u64>(h) + kPad;
  if (w == 0 || h == 0 || cell_w > p.width || cell_h > p.height) return AllocResult::BadSize;
  const u32 scan = p.shelf_count < kMaxShelves ? p.shelf_count : kMaxShelves;
  u32 best = kMaxShelves;  // index of the chosen shelf
  for (u32 i = 0; i < scan; ++i) {
    const Shelf& s = p.shelves[i];
    if (s.height < cell_h) continue;
    if (static_cast<u64>(s.used_x) + cell_w > p.width) continue;
    if (best == kMaxShelves || s.height < p.shelves[best].height) best = i;
  }
  u32 px = 0, py = 0;
  if (best != kMaxShelves) {
    Shelf& s = p.shelves[best];
    px = s.used_x;
    py = s.y;
    s.used_x = static_cast<u32>(static_cast<u64>(s.used_x) + cell_w);
  } else {
    if (p.shelf_count >= kMaxShelves || static_cast<u64>(p.next_y) + cell_h > p.height) return AllocResult::NoSpace;
    Shelf& s = p.shelves[p.shelf_count];
    s.y = p.next_y;
    s.height = static_cast<u32>(cell_h);
    s.used_x = static_cast<u32>(cell_w);
    px = 0;
    py = p.next_y;
    p.next_y = static_cast<u32>(static_cast<u64>(p.next_y) + cell_h);
    p.shelf_count += 1;
  }
  {
    const u64 add = static_cast<u64>(w) * h;
    p.used_area = (add > ~static_cast<u64>(0) - p.used_area) ? ~static_cast<u64>(0) : p.used_area + add;
  }
  *x = px;
  *y = py;
  return AllocResult::Ok;
}

std::uint64_t packer_digest(const ShelfPacker* packer) {
  Fnv f;
  if (packer == nullptr) return f.h;
  f.word(packer->width);
  f.word(packer->height);
  f.word(packer->shelf_count);
  f.word(packer->next_y);
  f.word(static_cast<u32>(packer->used_area & 0xFFFFFFFFu));
  f.word(static_cast<u32>(packer->used_area >> 32));
  const u32 n = packer->shelf_count < kMaxShelves ? packer->shelf_count : kMaxShelves;
  for (u32 i = 0; i < n; ++i) {
    f.word(packer->shelves[i].y);
    f.word(packer->shelves[i].height);
    f.word(packer->shelves[i].used_x);
  }
  return f.h;
}

}  // namespace lumora::shelfpack
