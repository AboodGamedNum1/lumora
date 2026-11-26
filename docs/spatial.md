# Spatial index module

The spatial module (`core/spatial/spatial.h`, `core/spatial/spatial.cpp`) is the CPU reference for hit testing, culling and
nearest queries over many 2D boxes (sprites, particles, UI). It uses integer arithmetic only (64-bit where needed), no floating
point, no libm, no heap allocation, no recursion, no static mutable state, no pointer value in any decision and no library sort.
Results are identical on Linux, Windows and Android. Evidence label: software (CPU) only.

## Limits and memory

- Up to `kMaxItems` (2^20) items, leaf size 1..`kMaxLeaf` (16). Item boxes are closed, `x0 <= x1`, `y0 <= y1`, every coordinate in
  `[-kMaxCoord, kMaxCoord]` with `kMaxCoord = 2^28`.
- The caller owns all memory: the items, the node array (`node_bound(n, leaf)` nodes of 28 bytes), the order array (`n` `uint32`)
  and the query output. The module keeps only fixed local arrays.
- Stack use: `build` keeps at most 32 pending ranges of 16 bytes (512 bytes); a range is pushed only for the right child while the
  left child is processed, and the depth is at most 20 splits for 2^20 items, so at most 22 frames are live. Queries keep at most
  64 pending nodes (256 bytes for boxes and points, 1 KB for `nearest`, which also stores a 64-bit distance per entry). A valid
  tree needs at most 22 entries; the larger bound is a guard (see the BadIndex section).

## Build rule

`build` follows the header exactly:

- `order[0..n)` starts as `0..n-1`. A range of at most `leaf_size` items becomes a leaf with `first` set to the range start in
  `order`, `count` set to the range size (1..`leaf_size`) and `right` 0.
- Otherwise the tight bounds of the range are computed, the axis with the larger extent is chosen (x when equal), the range is
  sorted by the key (`x0 + x1` for x, `y0 + y1` for y, as `int64`) and then by item index, and split at `count / 2`: the left gets
  `count / 2` items, the right gets the rest. An inner node has `count` 0, `first` = left child index and `right` = right child
  index. Because nodes are numbered depth first with the parent before its children, the left child index is always `parent + 1`.
- The sort is an in-place heapsort on the `order` sub-range. The (key, index) order is total because item indices are distinct, so
  the sorted result is the same for any correct sorting algorithm.
- The explicit stack holds ranges. Popping a range assigns the next node number, links it into its parent (`first` for a left child,
  `right` for a right child), computes its bounds, and either finishes a leaf or sorts and pushes the right range and then the left
  range, so the left subtree is numbered completely before the right subtree.

Check order: `NullBuffer` (items, nodes or order null while `item_count > 0`), `BadCount`, `BadBox` (first bad item in index order),
`BufferTooSmall` (`node_capacity < node_bound`). Nothing is written on error. With `item_count == 0` the result is `Ok`, `node_count`
is 0 (it may be null) and null pointers are fine.

Bounds. `x0 + x1` is within `[-2^29, 2^29]`, exact in `int64` (and even in `int32`). The extent `x1 - x0` is at most `2^29`, computed
in `int64`. Nothing else is computed during a build.

Cost. Each level scans every range once and heapsorts it, so a build is O(n log^2 n) comparisons in the worst case. The golden test
below builds 2^20 boxes; on one Linux machine at `-O2`, without sanitizers, that took about 3.4 seconds (local, not CI).

## Exact node count

`node_bound(n, leaf)` is the exact number of nodes written by `build`, as `nodes(n) = 1` for `n <= leaf` and
`nodes(n) = 1 + nodes(n / 2) + nodes(n - n / 2)` otherwise. It returns 0 for `n == 0`, `n > kMaxItems` or an invalid leaf size.

It is computed level by level without recursion or heap. At every depth the range sizes take at most two values, `m` and `m + 1`:
the children of `2k` are `k, k`, of `2k + 1` are `k, k + 1`, of `2k + 2` are `k + 1, k + 1`, so two adjacent sizes always produce
sizes inside `{k, k + 1}`. The loop therefore keeps two (size, multiplicity) pairs per level (the code has four slots, merged by size,
and returns 0 if a fifth distinct size ever appeared, which the argument rules out). Every range at a level is one node, a range
with size at most `leaf` stops, a larger range adds its two children to the next level. A range of size `s > leaf >= 1` has
`s >= 2`, so both children have at least one item. The loop ends after at most 21 levels. Multiplicities are summed in `uint64`
(at most `2^21`), then narrowed. The tests compare the function with the recursive definition for every `n` up to 3000 for nine leaf
sizes and for several large `n` including `kMaxItems`, and with the real node count of `build` for every `n` up to 2500 for three leaf sizes.

## Queries

- `query_box` returns the items whose box intersects the closed query box (touching counts), `query_point` the items that contain
  the point. The indices are written in ascending order. `*found` is the total number of matches, also when `out` is too small; then
  the status is `BufferTooSmall` and `out` holds the first `out_capacity` matches in ascending order, that is the smallest indices
  among all matches.
- Ascending output without sorting every match and without heap: `out` is used as a bounded max-heap of the `out_capacity` smallest
  indices seen so far. While it has room, a match is pushed. Once it is full, a match replaces the root only when it is smaller
  (then sifts down). Every match is counted. After the traversal a heapsort in place turns the heap into ascending order. Cost: a
  traversal of the visited nodes plus `O(M log C)` for `M` matches and capacity `C`. With `C = 0` nothing is written.
- Only reversed bounds make a query box invalid (`BadBox`); the magnitude of query coordinates is not checked. `query_point` and
  `nearest` take every `int32` coordinate. Queries only compare values, so nothing can overflow.
- Check order for `query_box`: `NullBuffer`, `BadBox`, `BadIndex`, `BufferTooSmall`. `query_point`: `NullBuffer`, `BadIndex`,
  `BufferTooSmall`. `found` and `out` are required pointers (`out` may be null when `out_capacity` is 0). `found` is set to 0 first
  whenever it is not null, so a failed call reports no matches.
- `nearest` returns the exact smallest squared distance from the point to a box (0 inside or touching) and the lowest index among
  equal distances. It visits the nearer child first and prunes a node only when its box distance is greater than the best so far, so
  a node at equal distance is still searched and the lowest index wins whatever the tree shape. With no items it returns `Ok` with
  `*found` false and `*index`, `*dist2` set to 0. Checks: `NullBuffer` (the three result pointers are always required), `BadIndex`.
  Nothing is written on error.

Distance bound. For a point `p` (any `int32`) and an item box (coordinates within `2^28`) the gap per axis is at most
`2^31 + 2^28 = 2,415,919,104`, its square is below `2^63`, and the sum of the two squares is below `2^64` (the largest case,
`INT32_MIN` against the corner `(2^28, 2^28)`, is `11,673,330,234,144,325,632`, a test constant). The gaps are computed in `int64`
and the squares in `uint64`. Node boxes are tight bounds of item boxes, so the same bound holds for them.

## BadIndex: what is checked, and the limits

Queries do not receive the leaf size, so they cannot recompute the exact node count. The checks are cheap and local:

- Up front, O(1): with `item_count == 0` the `node_count` must be 0 (items, nodes and order may be null). Otherwise `items`,
  `nodes` and `order` must not be null (`NullBuffer`), `1 <= node_count <= 2n - 1`, and the root must be plausible: a leaf root needs
  `node_count == 1`, `first + count <= n` and `right == 0`; an inner root needs `first == 1` and `1 < right < node_count`.
- While walking: a child index must be larger than its parent and below `node_count`; a leaf must satisfy `first + count <= n`;
  every `order` entry read must be below `n`; the pending stack must stay within 64 entries. The walk also has a budget: it may
  visit at most `node_count` nodes and `n` leaf positions in total, which a valid tree never exceeds because each node and each
  position is reached once. So a damaged index cannot loop (child indexes only grow) and cannot fan out through shared subtrees.
  Any violation returns `BadIndex`.
- On `BadIndex` found during a walk, `*found` is 0 and `out` is unspecified (it may have been partly written). `nearest` writes
  nothing.
- Limitation: an index whose structure is internally consistent, but which was built from other items (or from items changed
  after the build), passes every check and returns plausible but wrong results. Detecting that would need an O(n) scan or a stored
  checksum, which the header does not provide. The caller must keep `items` unchanged and alive, as the header states.

## Tests and goldens

`tests/spatial/spatial_tests.cpp` checks, with independent references written in the test file or in Python:

- every status and the order of checks, no writes on error, exact capacity and no writes past it, empty and single sets, items at
  exactly `+-2^28`, and byte-identical repeated builds even with dirty output buffers;
- `build` against a recursive reference with a library sort for seven dataset kinds (random, identical, nested, touching grid,
  points, a line of flat touching boxes, extreme coordinates), leaf sizes 1, 2, 5 and 16 and sizes up to 3000, plus structural
  rules for every tree: depth first numbering, tight bounds that contain every item below, leaf counts 1..leaf, left child equals
  parent + 1, the order array is a permutation, split at `count / 2`;
- `node_bound` against the recursive definition and against real builds;
- box and point queries against brute force with ordered outputs, truncation totals and prefixes, touching and degenerate boxes;
- `nearest` against brute force, ties, containment, the largest distance, and the empty set;
- every BadIndex guard and the budgets, and that a plausible index from other items is accepted without a crash;
- constant golden digests for 18 dataset configurations plus one at 2^20 items (see below).

Golden digests are FNV-1a 64 over little-endian 32-bit words (`digest_indices`), computed by an independent Python reference: the
`order` array, the node array (box x0, y0, x1, y1 as two's complement words, then `first`, `count`, `right`), the concatenated
query results (for each query the count followed by the indices) and the `nearest` results (found flag, index, low and high 32 bits
of the distance). Datasets and queries come from a 64-bit LCG with multiplier `6364136223846793005` and increment
`1442695040888963407`, taking the high 32 bits. Examples (kind, items, leaf, seed: order digest):

| Dataset | Nodes | Order digest |
| --- | --- | --- |
| random, 1000 items, leaf 1, seed 5 | 1999 | `0x9c261768ff31c385` |
| grid of touching cells, 512 items, leaf 16, seed 11 | 63 | `0xb225f94dd46e1125` |
| extreme coordinates, 400 items, leaf 16, seed 16 | 63 | `0xc80c63ae09173571` |
| random, 65536 items, leaf 16, seed 18 | 8191 | `0x01585832053091dd` |
| random, 1048576 items, leaf 16, seed 19 | 131071 | `0xa1baf36686f39e3d` |

Performance. The 2^16-item test (build, 20,000 box queries and 20,000 nearest queries, brute-force cross-checks) runs under
sanitizers. The 2^20-item test (build, structure check, golden digests, 5,000 queries) is compiled out when AddressSanitizer is
active (`__SANITIZE_ADDRESS__`), because instrumenting a million-item tree makes it very slow; it runs in ordinary builds.
