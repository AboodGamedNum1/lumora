# 2D spatial grid

`core/grid/grid.h` and `core/grid/grid.cpp` implement `SpatialGrid`, a deterministic broadphase for axis-aligned boxes:
insert, remove, update, box queries and all-pairs. Pure CPU, no I/O, no threads. The interface is in `grid.h`; this
page records every rule.

## Determinism

Output, including the order of every list, is identical on Linux, Windows and Android. The code uses integers, double
arithmetic with `+ - * /` and comparisons, and ordered containers (`std::map`, sorted vectors). It uses no hashing,
no pointer values, no unordered containers and no libm. The source asks compilers not to fuse multiply-add. The grid
state lives behind a heap allocated `Impl`; the class cannot be copied.

## What decides the output

The cells are only a candidate filter. Every item that is returned by `query` or `pairs` passed the exact closed
comparison on the float boxes:

`a.min_x <= b.max_x && b.min_x <= a.max_x && a.min_y <= b.max_y && b.min_y <= a.max_y`

so touching boxes intersect and a zero-width box is a valid box. Cell assignment can only decide which items are looked at,
never which are returned, provided no true intersection is dropped. That holds for these reasons:

- The cell of a coordinate is a monotone function (floor, then clamp), so a point shared by two boxes lies in a
  cell that belongs to the range of both boxes.
- The double computation `(p - origin) / cell_size` has a relative error of at most about 2.3e-16 and the clamped
  values are below 4097, so the absolute error is below 1e-12 cell units. The cell range is widened by 1e-9 cell units on
  each side (the lower edge uses `floor(t - 1e-9)` and the upper edge `floor(t + 1e-9)`), which absorbs that
  error. Widening only adds candidates; the exact test removes the extra ones, so it cannot change the output.
- A box edge exactly on a cell border occupies both neighbouring cells, as the header asks, and is also covered by
  the same widening.
- The tests compare every query and every pair list with an O(n^2) brute force reference, including boxes whose
  edges sit exactly on cell borders, one rounding beside them, in the border cells, and far outside the grid.

## Cells and clamping

The cell of a point is `floor((p - origin) / cell_size)` clamped to `0..cells-1` per axis. The value is clamped in
double **before** it is converted to an integer, so huge, tiny, infinite-looking or far values (up to the largest
float) never cause an overflowing or undefined conversion. Boxes outside the grid land in the border cells, and the
grid never rejects a box by position.

Implementation notes (they do not change any output): cells are kept in an ordered sparse map from cell index to item ids;
an item that covers more than 64 cells goes to a "large" list that is checked directly; a query that covers more than 256
cells scans all live items; `pairs` walks the live items in ascending id, collects the sorted unique candidate partners with a
larger id, and tests each exactly, so no pair is reported twice even if the boxes share many cells, and memory stays small
when many items overlap.

## Statuses and checks

- `init` checks, in this order: `BadCellSize` (not finite or `<= 0`), `BadOrigin` (not finite or `|coordinate| > 1e9`),
  `BadCells` (`cells_x` or `cells_y` is 0 or above `kMaxCellsPerAxis`). A failed `init` leaves the previous state. A successful
  `init` clears all items and restarts the ids at 0.
- Before the first successful `init` the grid has no cells: `insert` returns `BadCells`, `remove` and `update` return
  `UnknownItem`, `live_count` is 0, `query` and `pairs` return 0.
- `insert` and `update` check the box first (`BadBox`: a non-finite coordinate or `min > max`; a point box is valid),
  then `TooManyItems` (more than `kMaxItems` live items, or the 32-bit id space is exhausted) for `insert` and
  `UnknownItem` for `update`. `remove` returns `UnknownItem` for an id that is not live. A failed call changes nothing.
- `UnknownItem` applies to `remove` and `update` only; the header comment that also names `query_item` refers to a function
  that does not exist.

## Ids

Ids are handed out in increasing order from 0 and are never reused, also after `remove`. `kMaxItems` limits the live
items, so an id can be larger than `kMaxItems` after removals.

## Queries and pairs

- `query` returns the ids of all live items whose box intersects the query box (closed), ascending, no duplicates, writes at most
  `capacity` ids and returns the total. An invalid query box (non-finite or `min > max`) returns 0 and writes nothing.
- `pairs` returns every unordered pair `(a, b)` with `a < b` of live items with intersecting boxes (closed), once each, sorted
  by `(a, b)`, writes at most `capacity` pairs and returns the total.
- A null output pointer is never written; the total is still returned (so a null pointer with capacity 0, or any capacity,
  is a count-only call). For `pairs`, both pointers must be non-null for anything to be written.
- A box is stored exactly as given (a `-0` stays `-0` in the digest).

## Digests

All digests are FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over 32-bit values, each as 4
little-endian bytes. `digest()` hashes the live item count, then for each live item in ascending id the id and the bit
patterns of `min_x`, `min_y`, `max_x`, `max_y`. `digest_u32` hashes the given values in order.

## Golden digests

The scenes are generated in `tests/grid/grid_tests.cpp` with a fixed linear congruential generator; that file checks the
same values. Each scene value is `digest_u32` over: the low and high words of `digest()`, of `digest_u32` of all pairs
(a, b interleaved) and of `digest_u32` of 100 query results (each as count followed by the ids), then the pair count and the
live count.

| Scene | Contents | Digest |
| --- | --- | --- |
| Scatter | 600 small boxes, 32 x 32 cells of size 8 | `0x32a1586075426a3a` |
| Churn | 400 boxes and one large box, 150 random removes and updates, 50 x 50 cells with a negative origin | `0xa877de3e9622cfb0` |
| Stress | 150 boxes in an 8 x 8 grid plus boxes far outside, huge, point and cell-corner boxes | `0x90343d8556c3cfde` |

## Evidence limits

The tests are CPU only. Numbers from a local sanitized Linux build with GCC are local results. Only the CI jobs of the
pull request count as CI evidence; no GPU, device or other-compiler claim is made here.
