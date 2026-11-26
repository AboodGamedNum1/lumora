# Tessellation module

The tessellation module (`core/tess/tess.h`, `core/tess/tess.cpp`) is the CPU reference for turning simple polygons into
triangles for the 2D renderer. It uses integer arithmetic only (64-bit where needed, a two-word accumulator for the shoelace sum),
no floating point, no libm, no heap allocation, no recursion, no static mutable state, no negative signed shifts and no pointer
value in any decision except the overlap check. Results are identical on Linux, Windows and Android. Evidence label: software
(CPU) only.

## Limits and arithmetic bounds

- 3..`kMaxVertices` (4096) vertices, every vertex coordinate in `[-kMaxCoord, kMaxCoord]` with `kMaxCoord = 2^28`. Query points of
  `locate` may be any `int32`.
- `orient(a, b, c) = (b - a) x (c - a)` is computed in `int64`. For vertices, each coordinate difference is at most `2^29`, each
  product at most `2^58` and the difference of two products at most `2^59`, so it is exact. The same bound holds for the dot
  product used by the fold test.
- Shoelace terms `x_i * y_j - x_j * y_i` are at most `2^57` (each product at most `2^56`). A simple polygon has `|area2| <= 2^59`
  (twice the area of a square of side `2^29`). A self-intersecting ring can wind around many times: 4096 terms reach `2^69`, which
  does not fit `int64`. The sum is therefore accumulated exactly in two 64-bit words (a 128-bit two's complement number with a
  carry), which never wraps because `2^69 < 2^127`. `area2` writes the exact value when it fits `int64` and otherwise saturates to
  `INT64_MAX` or `INT64_MIN`, with status `Ok`. The sign and zero test used by `is_simple` and `triangulate` use the exact wide sum.
  A simple polygon never saturates.
- `locate` with any `int32` query point: the vector from a vertex to the point has components of at most `2^31 + 2^28`, the edge
  has components of at most `2^29`, so each product is below `2^61`, the cross value below `2^62`, exact in `int64`.
- Everything else compares coordinates.

## area2, is_simple, locate

`area2` returns twice the signed area, counter-clockwise positive. `is_simple` returns false when any of these holds: two vertices
coincide, the area is exactly zero, adjacent edges fold back (cross product zero and positive dot product at the shared vertex),
or two non-adjacent edges share any point (the closed segment test: orientation signs with the on-segment case for zero, after an
exact bounding box reject). Collinear consecutive vertices going on in the same direction are allowed. The tests compare it with
a reference formulated differently (the parametric intersection), with the independent Python reference, and on thousands of
random small grid polygons where touching, overlapping and crossing cases are frequent.

`locate` checks every edge for the point lying on it (cross product zero and inside the edge box; a vertex is on its edges), which
gives `Boundary`, and otherwise counts crossings of a ray to +x with the half-open rule on y: an edge counts when exactly one end is
above the point; an upward edge counts when the point is on its left (cross product positive), a downward edge when the cross
product is negative. The result is `Inside` for an odd count. The polygon is not revalidated; a non-simple polygon gives an
unspecified answer and cannot crash (the loop only reads the vertices).

All three write their result only on `Ok`.

## Triangulation rule as implemented

`scratch_size(n)` is `3 * n` (0 for an invalid count). The scratch holds `prev[n]`, `next[n]` and `status[n]`. A removed vertex has
`next = 0xFFFFFFFF`. The ring is the vertex order, so the ring order from the lowest remaining index is simply ascending index
order, and a pass never needs to wrap.

- The sign `s` of the exact area gives the winding (+1 counter-clockwise, -1 clockwise). A corner (a, v, b) is strictly convex when
  its turn has sign `s`, collinear when the turn is exactly zero, reflex otherwise.
- A pass starts at the lowest remaining index and visits each vertex of the ring as it was when the pass started, once, in ring
  order. At each visited vertex the collinear test comes first. A collinear vertex is dropped (no triangle) and the pass continues at
  its successor. Otherwise, a strictly convex vertex with no other remaining vertex in the closed triangle (a, v, b) is an ear: the
  triangle is emitted, v is removed, and a new pass starts at the lowest remaining index. A pass that ends with drops and no ear
  starts a new pass at the lowest remaining index.
- A counter-clockwise ring emits (previous, current, next) and a clockwise ring emits (next, current, previous), so every emitted
  triangle is counter-clockwise.
- Only after a pass without a drop and without an ear, the first strictly convex vertex whose triangle has no remaining vertex
  strictly inside is clipped (boundary allowed). This branch is implemented as the header requires and is proven unreachable below.
- The loop ends when two vertices remain. `tri_count` is `n - 2` minus the number of dropped collinear vertices.

Cost control. Re-testing every vertex in every pass would cost `O(n)` per vertex and `O(n^3)` overall. Two facts allow a cache that
returns exactly the same decisions: the vertex set only shrinks, and the triangle of a vertex only changes when one of its two ring
neighbours changes. So a vertex that is reflex or an ear stays so until a neighbour changes, and a vertex blocked by a remaining
vertex w stays blocked while w is still there and the neighbours are unchanged. The scratch status stores "unknown", "ear" or the
blocking vertex; the two neighbours of a removed vertex are reset to "unknown". A removed blocker is detected by its removed marker
and the vertex is then re-tested with one `O(n)` scan of the ring. The pointer to the lowest remaining index only moves up.
A pass costs `O(n)` cheap visits plus the re-tests, so a polygon with `n` clips costs about `O(n^2)` plus the re-tests. The honest worst case
is above `O(n^2)`: an adversarial polygon can make many vertices lose their blocker between two passes, and each re-test is
`O(n)`. No such polygon is known to the tests, and the measured shapes below show no growth beyond quadratic. The cache never
changes the output: the tests compare it with a naive uncached implementation of the rule.

## Termination, exact tiling, and why the fallback is never taken

Invariant. After the validation, the ring is a simple polygon (no touching), its doubled area is `|area2|` minus the doubled areas
of the triangles emitted so far, and each emitted triangle has positive doubled area.

- Dropping a collinear vertex v with neighbours a and b removes a zero-area corner. In a simple ring the corner is not a fold (a
  fold has the neighbours on the same side, which would make adjacent edges overlap), so v lies strictly between a and b and
  the new edge ab is the union of av and vb: the ring stays simple and the area is unchanged.
- Clipping an ear (a, v, b): the corner is strictly convex, so the triangle lies on the interior side near v. No other remaining
  vertex is in the closed triangle, so no boundary edge has an end in it. An edge cannot cross the open triangle without an end
  inside it or crossing av or vb (impossible in a simple ring), so the open triangle contains no boundary and lies inside the
  polygon. The new edge ab touches no vertex and crosses nothing, so the ring stays simple. The triangle area is exactly what is
  removed from the ring area. The triangle has positive area because the corner turn is not zero.
- Hence, by induction, the emitted triangles tile the polygon exactly, and the sum of their doubled areas equals `|area2|` once
  the ring has been reduced to nothing. The tests assert that equality for every triangulation.

Existence of a closed ear (the two ears theorem with the exact rule). A simple ring with at least four vertices and no collinear
vertex has two non-adjacent vertices that are ears in the sense of the rule (strictly convex, closed triangle free of other
vertices). Proof by induction on the number of vertices. Take the lowest-leftmost vertex v: it is strictly convex (a vertex with
the smallest x and then y has a turn of at least zero, and zero is excluded). Let T = (a, v, b). If no other vertex is in the
closed T, v is an ear. Otherwise let u be the vertex in the closed T (other than a, v, b) that is farthest from the line ab. The
segment vu contains no vertex (a vertex between would be farther from ab) and lies inside the polygon. It splits the ring into two
simple rings with fewer vertices, each having v and u as adjacent vertices. A ring with three vertices has three ears. A ring with
at least four has two non-adjacent ears by the hypothesis, and they cannot both be among the adjacent pair v, u, so at least one ear
x is a vertex other than v and u. The triangle of x lies inside its sub-ring, and no vertex of the other sub-ring can be inside it
(those lie outside the sub-ring except on the segment vu, which holds no vertex), so x is an ear of the whole ring. Doing this
in both sub-rings gives two ears of the whole ring. Two ears, one from each side, are not adjacent. (The ear x must also be
strictly convex in the whole ring: its corner is the same corner as in the sub-ring.)

Termination and unreachability of the fallback. Every outer iteration of the loop removes at least one vertex: a pass either
drops a vertex, clips an ear, or finds neither; in the last case no vertex is collinear (a collinear vertex would have been dropped),
the ring is simple by the invariant, and the theorem above gives an ear, so the pass would have clipped it. Hence a pass with
neither a drop nor an ear cannot happen for a simple polygon and the fallback branch is never taken. The ring has at least four vertices when the
theorem is used; with three vertices left the corner turns all have sign `s` (the remaining area is positive), no other vertex
exists, and the first visited vertex is an ear, so the loop finishes. The number of outer iterations is at most `n`, each costing at most a
pass.
The two defensive `BadPolygon` returns (a collinear vertex found with only three vertices left, and no ear and no fallback candidate)
are therefore unreachable after `is_simple` has passed. The tests show they are not taken: every simple polygon in the golden
shapes, the two random grid corpora and the 300 grown polygons returns `Ok`, and the naive reference, which implements the fallback,
reports zero fallbacks for every one of them (the Python reference reports zero as well). A random search of tens of thousands of
grown polygons with many collinear points found no fallback case either. Because these branches cannot be reached through the public
functions, the tests do not execute the fallback code itself.

If a defensive return were ever taken, output and scratch would already hold partial data. Everything else is validated before
the first write, so "nothing is written on error" holds for every status except that unreachable case.

## Check order and Overlap

Checks in order: `NullBuffer` (pts, out, scratch, tri_count), `BadCount`, `BadPoint`, `BufferTooSmall` (`out_capacity < n - 2` or
`scratch_count < 3 * n`), `Overlap`, `BadPolygon` (`is_simple`, which includes zero area). Nothing is written on error.

Overlap compares every pair of four touched byte ranges: the points (`8 * n` bytes), the scratch (`3 * n` words), the output extent
(`3 * min(out_capacity, n - 2)` words, which is `3 * (n - 2)` after the capacity check), and the 4 bytes of `*tri_count`. The
address rule is the one of `core/composite`, `core/resample` and `core/sdf`: clear the top byte of both addresses, take the unsigned
difference modulo `2^64`, answer "no overlap" when its absolute value is `2^47` or more before any other arithmetic, and
otherwise compare the exact ranges (they intersect when the start of A relative to B is below B's length and above minus A's
length). The `2^47` cutoff cannot be reached by real buffers in a test, so it is argued here: two real allocations in one process
that overlap are within `2^47` bytes of each other. Adjacent ranges that touch end to start do not overlap. The tests place the four
buffers inside one arena at chosen word offsets and check each boundary, including a large `out_capacity` that does not reach a
range beyond `3 * (n - 2)` words.

## Measured cost (local, Linux g++, not CI)

Each line is the whole `triangulate` call for 4096 vertices, including its `is_simple` validation (about 0.02 s of the total
unsanitized, about 0.09 to 0.12 s under sanitizers):

- Unsanitized `-O2`: comb 0.03 to 0.04 s, square spiral 0.05 to 0.07 s, collinear rectangle 0.04 s, convex parabola 0.04 s,
  including rotated and reversed variants.
- AddressSanitizer plus UBSan at `-O1`: comb 0.15 to 0.21 s, spiral 0.22 to 0.35 s, collinear rectangle 0.20 to 0.24 s,
  convex 0.22 s.

The performance test runs the comb, the spiral and the collinear rectangle with 4096 vertices under sanitizers.

## Tests and goldens

`tests/tess/tess_tests.cpp` checks statuses and their order, no writes on error, exact capacities, area and saturation cases,
every rejection rule of `is_simple`, `locate` against the edge list and the tessellation on grids and with extreme `int32`
queries, and `triangulate` against a naive uncached reference of the exact rule. Shapes: triangle, convex, star, comb, spiral,
collinear rectangles, +/-2^28 extremes, in four variants (as is, reversed, rotated by a third, reversed and rotated by a half).
Every triangulation is checked for valid indices, positive counter-clockwise doubled areas, `tri_count = n - 2 - drops` and the sum of
areas equal to `|area2|`.

Golden digests are FNV-1a 64 over little-endian 32-bit words (`digest_indices`) of the triangle count followed by the index triples,
computed by an independent Python reference (big integers, brute-force tests, a naive uncached loop). Examples:

| Shape and variant | Vertices | Triangles | Dropped | Digest |
| --- | --- | --- | --- | --- |
| triangle, counter-clockwise | 3 | 1 | 0 | `0xc3a3bd3a59bc7a17` |
| triangle, clockwise | 3 | 1 | 0 | `0x6e5ae5a25523fdf7` |
| square at the extremes | 4 | 2 | 0 | `0x74abd744285dde85` |
| star, reversed | 64 | 50 | 12 | `0x09caf32726a86549` |
| comb, rotated by a third | 384 | 256 | 126 | `0xe6bc2cd097307652` |
| spiral | 322 | 320 | 0 | `0x3b626a4a775293ee` |

Corpora (a 64-bit LCG with multiplier `6364136223846793005`, increment `1442695040888963407`, high 32 bits): two sets of random small
grid polygons and 300 grown simple polygons (random vertex insertion kept simple by `is_simple`, random winding and start index)
have their `is_simple` flags, doubled areas and triangulations hashed together with the number of simple polygons, triangles and
dropped vertices, as constants in the tests.
