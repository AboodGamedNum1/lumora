# bridges: bridges and articulation points of an edge list

Code: `core/bridges/bridges.cpp` (contract in `core/bridges/bridges.h`). Tests: `tests/bridges/bridges_tests.cpp`. Reference: `tools/bridges_ref.py`.

## Rule

- Edge i joins `eu[i]` and `ev[i]`; the order of the two ends does not matter. Parallel edges and self-loops are allowed.
- An edge is a bridge when removing that one edge (and only it) increases the number of connected components. A self-loop is never a bridge. Two parallel edges are never bridges; a single edge with no parallel twin can be.
- A node is an articulation point (cut) when removing it and every edge at it, loops included, increases the number of components among the remaining nodes. An isolated node and a node with only loops are never cuts.
- A lone edge is a bridge and neither end is a cut when its component is exactly that edge. An end with other edges can be a cut: a star centre with two or more leaves is a cut, every inner node of a path component is a cut, a node whose neighbours stay connected by other paths is not, and a node joined to one neighbour by parallel edges is not a cut because of them.
- `is_bridge[i]` and `is_cut[v]` are 0 or 1; `*out_bridges` and `*out_cuts` are the counts.
- Limits: nodes at most `kMaxNodes` = 65536, edges at most `kMaxEdges` = 262144.
- `bridges_scratch_words` writes 8 * nodes + 2 * edges + 1 for every in-limit size. Null `out_words` is NullArgument, then oversize is TooLarge with nothing written.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `is_bridge[0 .. edges)`, `is_cut[0 .. nodes)`, `*out_bridges` and `*out_cuts` are written; both flag arrays keep their values at and beyond those lengths. With edges 0 nothing is written to `is_bridge`; with nodes 0 nothing is written to `is_cut`. All validation finishes before the first write. On any failure all outputs are untouched. Scratch contents after the call are unspecified and are not tested.

## Check order (`bridges_run`)

1. NullArgument: `out_bridges` and `out_cuts` always; `eu`, `ev` and `is_bridge` only while edges is above 0; `scratch` and `is_cut` only while nodes is above 0. The conditions use the given sizes even above the limits.
2. TooLarge (nodes above `kMaxNodes` or edges above `kMaxEdges`).
3. ScratchTooSmall: only while nodes is above 0, `scratch_words` below 8 * nodes + 2 * edges + 1 (64-bit arithmetic). With nodes 0 `scratch_words` is ignored.
4. BadNode: one pass over every edge (loops and the last edge included) for `eu` or `ev` not below nodes, before any write. Nodes 0 with any edge is BadNode.
5. Nodes 0 with no edges: both counts 0, Ok.

## Method and scratch layout

Iterative depth-first search with low-points and explicit stacks. The edge list becomes compressed adjacency of arc ids: arc 2i is `eu[i]` to `ev[i]`, arc 2i + 1 is `ev[i]` to `eu[i]`, loops get no arcs. The search skips the parent edge id, not the parent node, so a parallel twin of the tree edge counts as a back edge. A tree edge to child c of p is a bridge when low[c] > idx[p]. A non-root p is a cut when some child has low[c] >= idx[p]; the root is a cut when it has two or more tree children.

Layout of the scratch, in words: start[N+1], adj[2E], idx[N], low[N], iter[N], call[N], par[N], kids[N]. That is 7N + 2E + 1, inside the 8N + 2E + 1 the contract asks for (N words unused). Steps: O(N + E). No recursion, so a 65536-node path needs no deep call stack.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

15 test cases, 3997116 assertions, no warnings. Local run line, from the repository root with the Catch2 objects of the build directory: compile `core/bridges/bridges.cpp` and `tests/bridges/bridges_tests.cpp` with the flags above and link with Catch2Main. Expected values come from `tools/bridges_ref.py`. Small graphs use brute force: remove each edge and each node and count components with bit masks. Large graphs use two independent methods, a BFS-tree path cover for bridges and chain decomposition for cuts; `python3 tools/bridges_ref.py --selfcheck` compares them with brute force on 30000 random graphs (0 mismatches), and the 100 mid graphs are compared inside the generator. Success paths use exact-size scratch (8N + 2E + 1, filled with garbage), an exact-size `is_bridge` (E bytes) and an exact-size `is_cut` (N bytes). Tiny or null buffers appear only for failures and empty graphs. No scratch contents, aliasing or timing are asserted.

Exhaustive (each by full result or by a digest of every result in a fixed order, with the golden in the test):
- Flat table: every simple graph with loops on 1, 2 and 3 nodes (2 + 8 + 64 = 74 graphs) with the full flags.
- Every simple graph with loops on 4 nodes (2^10) and on 5 nodes (2^15), digest.
- Every simple graph with loops on 6 nodes (2^21 = 2097152), digest group.
- Every loop-free simple graph on 7 nodes (2^21 = 2097152), digest group.
- Every ordered edge list on 3 nodes with 0 to 5 edges (66430 lists) and on 2 nodes with 0 to 6 edges (5461 lists), with parallels, loops and both orientations.
- Every multigraph on 4 nodes with 0, 1 or 2 parallel edges per node pair and any subset of the four loops (3^6 * 2^4 = 11664).

Sampled, not exhaustive: a 20000-graph random battery (nodes 1..14, edges 0..40, seed 5151), 100 mid graphs (nodes 1..40, edges 0..150) each checked against a C++ removal oracle on every edge and node, relabelled copies that shuffle node names, shuffle the edge order and flip the ends (flags must follow their edge or node: 3 per mid graph, 3000 battery graphs, 9 max-size graphs), and 14 max-size patterns by digest and counts (seeds 5000 + pattern): path of 65535 edges, cycle of 65536, star, 8192 cycles of 8 nodes joined by bridges, doubled path, path with every edge four times, random tree, no edges, one node with 262144 loops, one loop per node, uniform random with 262144 edges, 1024 blocks of 64 nodes sharing one node (64513 nodes), the cycle chain shuffled and flipped, and a 256 x 256 grid. Exact bound cases are among them: 65536 nodes with 262144 edges, 65536 nodes with no edges, 1 node with 262144 loops, the 65535-edge path (every inner node a cut), the 65536 cycle, and bridge-heavy trees. Closed-form flags are asserted for all of them except the two uniform and shuffled ones (those use digests and counts).

Removal oracle on the max-size graphs: for each pattern, 2000 sampled edges and 2000 sampled nodes are checked against a breadth-first removal oracle in the test (separate from the library and the reference): 1000 flagged and 1000 unflagged draws each, with replacement, all 2000 from one class when the other is empty. Pattern 7 has no edges, so it has no edge draws; pattern 8 has one node, drawn 2000 times. The cost is about 4000 early-exit searches per graph; counted node visits per pattern 0 to 13 are 98909497, 262138000, 131071000, 64844600, 32040866, 33553977, 196553292, 0, 0, 0, 120701653, 32026036, 65350602 and 53602 (patterns 7, 8 and 9 have no arcs to search). The oracle test takes about 52 s under ASan at -O1 on the machine used.

Hand-checked: every case of the rule above (lone edge both ways, parallel edges, star centres, triangle, ring, loops, path components with isolated nodes, bow tie, two triangles joined by a bridge, pendant edge, a cut found from a deeper child, root with one child), nodes 1, nodes 0, and argument rules and check order (each pointer alone, smallest sizes with 1 edge or 1 node, null before TooLarge, TooLarge before ScratchTooSmall before BadNode, exact scratch and one word less, every BadNode position and field, untouched outputs on every failure, guard regions of 16 bytes after both flag arrays checked on success and on failure, 64-bit size check at the limits).

## Mutation testing

126 source mutants (script outside the repo), built with ASan + UBSan and run against the final tests (a quick pass without the three longest cases, then the full suite on every survivor of the quick pass; the tests did not change between runs).

- 103 killed by a failed assertion.
- 16 crash-only (no failed assertion, killed by a sanitizer or abort): c start init, n edges >1, n edges none, n eu, n ev, n is_bridge, n is_cut, n nodes none, n nodes>1, n out none, n out_bridges, n out_cuts, n scratch, w null, z none, br flag u.
- 0 timeouts.
- 7 not killed by the full suite, all equivalent:
  - d counter2: preorder numbers start at 2, not 1. Only the order of the numbers is used and 0 still means unvisited, so nothing changes.
  - s 32bit: 32-bit size arithmetic in the scratch check. After TooLarge, 8N + 2E + 1 is at most 1048577, far below 2^32.
  - d root loop: the last node is never started as a root. If it is still unvisited then no lower node reached it, so it has no arcs; it is isolated or has only loops, and nothing is flagged for it.
  - d root low: the root's low-point is set to 0. The root's low-point is never read: children compare with idx[root], and the root's own low is not used.
  - d skip no kNone: the root's parent test is removed. The root's parent is 0xFFFFFFFF, and its edge id (0x7FFFFFFF) never equals an arc's edge id.
  - d back <: `idx[w] <= low[u]` assigns the same value when equal.
  - d low prop <=: the root's low-point is not updated from children; it is never read.
  No time-only survivor remains, so no step argument or doubling timing was needed.

## Not run

Exhaustive max-size cases, loop-free simple graphs on more than 7 nodes, simple graphs with loops on more than 6 nodes, timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
