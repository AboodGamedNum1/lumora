// Shelf allocator for glyph bitmaps in a fixed-size atlas: rectangles are placed left to right on horizontal shelves, shelves stack
// downward. It places rectangles only (no pixels, no free of single rectangles, only a full reset). Integer only, no allocation, no
// 128-bit types (MSVC). Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::shelfpack {

constexpr std::uint32_t kMaxAtlas = 4096;   // largest width or height
constexpr std::uint32_t kMaxShelves = 64;
constexpr std::uint32_t kPad = 1;           // empty pixels kept right of and below every rectangle

struct Shelf {
  std::uint32_t y = 0;       // top of the shelf
  std::uint32_t height = 0;  // cell height of the shelf (rectangle height + kPad of its first rectangle)
  std::uint32_t used_x = 0;  // next free x on the shelf
};

struct ShelfPacker {
  std::uint32_t width = 0, height = 0;  // 0 until packer_init succeeds
  std::uint32_t shelf_count = 0;        // 0 to kMaxShelves
  std::uint32_t next_y = 0;             // top of the next new shelf
  std::uint64_t used_area = 0;          // sum of w * h of the placed rectangles (without padding)
  Shelf shelves[kMaxShelves] = {};
};

enum class AllocResult : std::uint8_t { Ok = 0, NullArgument = 1, NotInitialized = 2, BadSize = 3, NoSpace = 4 };

// Sets the atlas size and empties the packer. Returns false and changes nothing when packer is null or width or height is 0 or above
// kMaxAtlas. May be called again to re-size.
bool packer_init(ShelfPacker* packer, std::uint32_t width, std::uint32_t height);

// Empties the packer and keeps width and height: shelf_count, next_y, used_area 0 and every shelf default. Returns false when packer is
// null, or when it is not initialized (width or height 0); nothing changes then.
bool packer_reset(ShelfPacker* packer);

// Places a w x h rectangle and writes its top left corner to *x and *y. The cell is (w + kPad) x (h + kPad). Check order, each failure
// writes nothing and changes nothing: any null pointer gives NullArgument; width or height 0 (uninitialized) gives NotInitialized; w or h
// 0, or a cell wider than the atlas width or taller than the atlas height, gives BadSize. Then: among the existing shelves whose
// height >= the cell height and whose used_x + cell width <= atlas width, choose the one with the smallest height (lowest index on a
// tie) and place there at (used_x, shelf.y); used_x += cell width. If none fits, open a new shelf at y = next_y with height = cell height
// when shelf_count < kMaxShelves and next_y + cell height <= atlas height (then place at (0, y), used_x = cell width, next_y += cell
// height, shelf_count += 1); otherwise NoSpace. On Ok, used_area += w * h. Decisions for extreme and hand-built values: all sums and comparisons in
// packer_alloc (w + kPad, h + kPad, used_x + cell width, next_y + cell height) are computed in uint64, so a huge input such as UINT32_MAX
// gives BadSize and an overflowing fit is a rejected fit, never a wrap. Hand-built fields are used as stored, without validation: shelf
// y and next_y are not checked, dimensions above kMaxAtlas are not rejected by alloc (only a zero width or height means NotInitialized),
// and at most the first min(shelf_count, kMaxShelves) shelves are inspected; a new shelf is opened only when the stored shelf_count is
// below kMaxShelves. No fit gives NoSpace with every field unchanged. used_area saturates at UINT64_MAX (no wrap). packer_init on
// success resets every field and every shelf; on failure the whole prior state is preserved.
AllocResult packer_alloc(ShelfPacker* packer, std::uint32_t w, std::uint32_t h, std::uint32_t* x, std::uint32_t* y);

// FNV-1a 64 byte-wise over 32-bit little-endian words (offset basis start; a null packer gives the basis): width, height, shelf_count,
// next_y, used_area low then high 32 bits, then for each shelf below shelf_count (clamped to kMaxShelves) in order: y, height, used_x.
std::uint64_t packer_digest(const ShelfPacker* packer);

}  // namespace lumora::shelfpack
