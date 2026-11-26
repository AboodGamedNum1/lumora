# 2D sprite batcher

`core/batch/batch.h` and `core/batch/batch.cpp` turn a list of sprites into vertex, index and draw batch buffers for a
renderer. Pure CPU, no I/O, no threads. It uses `xform.h` (`apply` and `transform_rect`) and nothing else. The interface
is in `batch.h`; this page records every rule.

## Determinism

Results are bit-identical on Linux, Windows and Android. Only `+ - * /` and comparisons are used; no libm function is
called. Positions come from `xform::apply` (a double evaluation of `a*x + c*y + tx`, rounded once to float; a negative
zero becomes `+0`). The order comes from a stable sort on integer keys. The source asks compilers not to fuse
multiply-add, and the project must not enable fast-math. The golden digests below also did not change with `-O0`,
`-O3 -march=native -ffp-contract=fast` and `-Os`. The buffers are heap allocated; no stack buffer scales with the sprite count.

## Steps and status order

`build_batches` checks in this order and returns the first failing status; on any error nothing is written to the output
buffers.

1. `TooManySprites`: `count > kMaxSprites` (16384). Only the count is looked at. `count == kMaxSprites` is valid.
2. `BadSprite`: the first sprite that has a non-finite transform entry, a size that is not finite or not greater than 0,
   a non-finite uv value, or a blend value other than 0, 1 and 2. A null sprite pointer with `count > 0` is also
   `BadSprite`; `count == 0` accepts null pointers everywhere.
3. `BadViewBounds`: whenever a cull rectangle is supplied (even if no sprite would be culled), a non-finite value or
   `min > max` on either axis. `min == max` is valid.
4. `BufferTooSmall`: see Capacity.

For all errors except `BufferTooSmall` every count in `BatchResult` is 0.

## Culling

If a cull rectangle is given, the bounds of each sprite are `xform::transform_rect(transform, (0, 0, size.x, size.y))`
(exact, rounded outward to float, axis aligned). A sprite is kept when the bounds intersect the closed cull rectangle:
`max_x >= cull.min_x`, `min_x <= cull.max_x`, `max_y >= cull.min_y`, `min_y <= cull.max_y`. Touching at an edge or a
corner counts as intersecting. Rotated sprites are tested by their axis-aligned bounds, so a rotated sprite whose bounds
touch the rectangle but whose corners do not is kept. `sprites_culled = count - sprites_drawn`. Culling never changes the
relative order of the remaining sprites.

## Order

The remaining sprites are stably sorted by this key, so ties keep input order:

1. `layer`, ascending, as a signed 32-bit integer.
2. Group rank within a layer: Opaque first, then Additive, then Alpha. This is not the numeric order of the enum values
   (Alpha 0, Additive 1, Opaque 2).
3. For Opaque and Additive: `texture`, ascending. Alpha sprites ignore the texture and keep strict input order.

## Vertices and indices

Per drawn sprite, in output order, the four vertices are the local corners `(0,0)`, `(size.x,0)`, `(size.x,size.y)`,
`(0,size.y)` transformed by the sprite matrix, with uv `(min_x,min_y)`, `(max_x,min_y)`, `(max_x,max_y)`,
`(min_x,max_y)` copied unchanged (any values, `min > max` is allowed) and the tint bytes copied to `rgba`. The indices
for sprite `k` with `base = 4*k` are `base+0, base+1, base+2, base+0, base+2, base+3`.

## Batches

Consecutive drawn sprites with equal `(texture, blend)` form one `DrawBatch`. Only the output order matters: a layer
change does not break a batch when texture and blend are equal. `first_index` is the offset of the batch in the index
buffer and `index_count` is a multiple of 6.

## Capacity

Capacities are element counts. Required: `4 * drawn` vertices, `6 * drawn` indices and as many batches as the merged
batch count (not the drawn count), so an exact fit works. Culled sprites need no room. A buffer pointer may be null only
if its required count is 0. When any capacity is too small or a needed pointer is null, the status is `BufferTooSmall`,
nothing is written, and `sprites_drawn`, `sprites_culled`, `vertex_count`, `index_count` and `batch_count` hold the
required sizes so the caller can allocate and call again.

## Digest

`digest_buffers` is FNV-1a 64 (offset `0xcbf29ce484222325`, prime `0x100000001b3`) over, in this order: for each vertex the
little-endian bytes of `x`, `y`, `u`, `v` and then the 4 `rgba` bytes (the same bytes as the 20-byte layout on
little-endian hosts); each index as 4 little-endian bytes; and for each batch `texture`, `blend` (0, 1 or 2), `first_index`
and `index_count`, each as 4 little-endian bytes.

## Golden digests

The scenes are generated in `tests/batch/batch_tests.cpp` with a fixed linear congruential generator; that file checks
the same values.

| Scene | Contents | Digest |
| --- | --- | --- |
| Mixed | 300 sprites, 5 textures, layers -1 to 2, all blends, translation and scaling matrices, no cull (116 batches) | `0x7bf77c4738ae3857` |
| Rotated and culled | 200 sprites with rotation from `compose_trs`, cull rectangle `(-150,-150,150,150)` (57 drawn, 27 batches) | `0xd5ccb04c626f574b` |
| UI | 40 sprites with exact 90 degree matrices, layers -3 to 3, all blends (40 batches) | `0x6840ba61d77a156c` |

## Evidence limits

The tests are CPU only. Numbers from a local sanitized Linux build with GCC are local results. Only the CI jobs of the
pull request count as CI evidence; no GPU, device or other-compiler claim is made here.
