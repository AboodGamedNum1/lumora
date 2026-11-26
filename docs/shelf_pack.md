# Shelf packer

`core/shelfpack` places glyph rectangles in a fixed atlas of at most 4096 x 4096 pixels. It only decides positions: there are no pixels,
no copying and no removal of a single rectangle. The rules are in the comments of `core/shelfpack/shelf_pack.h`; this page adds the
bounds, the decisions and the tests.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state and no 128-bit type. Evidence
label: local unit tests only.

## How a rectangle is placed

- The cell of a w x h rectangle is (w + 1) x (h + 1): one empty pixel is kept right of and below every rectangle.
- A shelf is a horizontal band. It has a start row `y`, a `height` (the cell height of its first rectangle) and `used_x`, the next free
  column. Rectangles on a shelf are placed left to right.
- A shelf fits a cell when its height is at least the cell height and `used_x + cell width` is at most the atlas width. Among the
  fitting shelves the one with the smallest height wins, and the lowest index wins a tie.
- When no shelf fits, a new shelf starts at `next_y` if fewer than 64 shelves exist and `next_y + cell height` is at most the atlas
  height. Its height is the cell height. Otherwise the result is NoSpace and nothing changes.
- `used_area` adds w * h without padding. It saturates at `UINT64_MAX` and never wraps.

## Bounds and decisions

- A call scans at most 64 shelves once. A hand-built `shelf_count` above 64 scans only the first 64 and cannot start a new shelf.
- All sums and comparisons in `packer_alloc` use 64-bit unsigned values. A width or height of `UINT32_MAX` is BadSize, and a sum
  that would overflow 32 bits is a rejected fit, never a wrap.
- Hand-built shelf `y` and `next_y` are used as stored. `packer_alloc` does not reject an atlas size above 4096; only a zero width or
  height is NotInitialized.
- Check order in `packer_alloc`: null argument, not initialized, bad size (zero width or height, or a cell larger than the atlas),
  then the shelf choice. `x` and `y` are written only on Ok.
- A successful `packer_init` resets every field and every shelf. A failed one keeps the whole earlier state. `packer_reset` with a
  zero dimension returns false and changes nothing.
- `packer_digest` is FNV-1a 64 over 32-bit little-endian words: width, height, shelf count, `next_y`, `used_area` (low word, then high
  word), then y, height and `used_x` of each of the first 64 shelves below the shelf count. A null packer gives the offset basis.

## Tests

`tools/shelf_pack_ref.py` is an independent model with its own shelf choice and its own FNV. It prints the constants pasted into
`tests/shelfpack/shelf_pack_tests.cpp`: scenario programs (placement, shelf choice, row fill, shelf limit, init and reset errors,
hand-built packers) and four seeded random corpora, each with result digests and final state.

Other tests: an oracle that checks placed cells lie inside the atlas and never overlap, hand-computed placements, the check order,
the 64-shelf limit, saturation of `used_area`, and clearing of the last shelf slot.
