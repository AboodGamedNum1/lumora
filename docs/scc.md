# scc: strongly connected components of an edge list

Code: `core/scc/scc.cpp` (contract in `core/scc/scc.h`). Tests: `tests/scc/scc_tests.cpp`. Reference: `tools/scc_ref.py`.

## Rule

- Edge i goes from `eu[i]` to `ev[i]`. Parallel edges, antiparallel edges and self-loops are allowed; a self-loop changes no component.
- `comp[v]` is the component number of node v and `*out_count` the number of components. Two nodes share a number exactly when each is reachable from the other. An isolated node is its own component, so nodes above 0 with no edges give count = nodes and `comp[v] = v`.
- The numbering is canonical: components are numbered 0, 1, 2, ... in the order of their smallest member node. The numbers appear in order of first occurrence along `comp[0], comp[1], ...`.
- Limits: nodes at most `kMaxNodes` = 65536, edges at most `kMaxEdges` = 262144.
- `scc_scratch_words` writes 6 * nodes + edges + 1 for every in-limit size, nodes 0 included (edges + 1). Null `out_words` is NullArgument, then oversize is TooLarge with nothing written.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `*out_count` and `comp[0 .. nodes)` are written; `comp` at and beyond `nodes` keeps its values. Nodes 0 with no edges gives count 0 and writes nothing to `comp`. All validation finishes before the first write. On any failure `*out_count` and `comp` are untouched. Scratch contents after the call are unspecified and are not tested.

## Check order (`scc_run`)

1. NullArgument: `out_count` always; `eu` and `ev` only while edges is above 0; `scratch` and `comp` only while nodes is above 0. The conditions use the given sizes even above the limits.
2. TooLarge (nodes above `kMaxNodes` or edges above `kMaxEdges`).
3. ScratchTooSmall: only while nodes is above 0, `scratch_words` below 6 * nodes + edges + 1 (64-bit arithmetic). With nodes 0 `scratch_words` is ignored.
4. BadNode: one pass over every edge (loops included, the last edge included) for `eu` or `ev` not below nodes, before any write. Nodes 0 with any edge is BadNode.

## Method and scratch layout

Iterative Tarjan with explicit stacks, then a renumbering pass. The scratch holds start[N+1], adj[E], idx[N], low[N], stk[N], call[N], iter[N]: 5N + E + 1 words, inside the 6N + E + 1 the contract asks for. The edge list is turned into compressed adjacency (counting by `eu`, prefix sums, fill). The depth-first search keeps the path in `call` and the Tarjan stack in `stk`; `iter[u]` is the next arc of u, so every arc is looked at once. Components are first numbered in completion order (stored in `comp`), then renumbered by first appearance in node order, which is the smallest-member order; `idx` serves as the map. Steps: O(N + E). No recursion, so a 65536-node path or cycle needs no deep call stack.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan)

10 test cases, 1882906 assertions, no warnings. Expected values come from `tools/scc_ref.py`, which uses neither Tarjan nor the renumbering: transitive closure with bit masks for small graphs, Kosaraju (iterative) for max-size graphs, and both on the 100 mid graphs, compared in the reference. Success paths use exact-size scratch (6N + E + 1, filled with garbage) and exact-size `comp`. No scratch contents, aliasing or timing are asserted.

Exhaustive:
- Flat table: every digraph with loops on 1, 2 and 3 nodes (530 graphs) with the full result.
- Digest groups: every digraph with loops on 4 nodes (65536); every loop-free digraph on 5 nodes (1048576), and the same graphs with all five loops added give the same digest; every ordered edge list on 3 nodes with 0 to 4 edges (7381; parallels, antiparallels, loops); every ordered edge list on 2 nodes with 0 to 6 edges (5461).

Sampled, not exhaustive: the looped 5-node space (2^25 edge sets) is not covered exhaustively; an evenly strided sample (every 331st edge set, 101373 graphs) is compared by digest. Also sampled: a 20000-graph random battery (nodes 1..14, edges 0..40), 100 mid graphs (nodes 1..40, edges 0..150), relabelled copies (shuffled node names, 3 per mid graph, 3000 battery graphs, 9 max-size graphs) that must keep the partition and the canonical numbering, and 14 max-size patterns at 65536 nodes: one cycle with every edge four times (262144 edges), a shuffled cycle with random chords (262144), a forward path and a backward path (65535 edges), a path closed by one edge, a 512-node dense core with loops (262144), 21845 chained triangles, random forward edges, a layered DAG, no edges, loops only, two shuffled cycles with a bridge, uniform random edges (262144), four interleaved dense cores. Closed-form results are asserted for most of them as well as the digests.

Hand-checked: numbering by smallest member, a cycle whose smallest member is not the smallest label seen first, loops, parallels, antiparallels, one-way and two-way joins, a descending chain, nodes 0, one node, argument rules and check order (each pointer alone, smallest sizes, null before TooLarge, TooLarge before ScratchTooSmall before BadNode, every BadNode position and field, untouched outputs on every failure, tail of `comp` untouched on success, 64-bit size check at the limits).

## Mutation testing

105 source mutants (script outside the repo), 30 s timeout, built with ASan + UBSan, one full run on the final tests.

- 76 killed by a failed assertion.
- 24 crash-only (no failed assertion, killed by a sanitizer or abort): n out_count, n edges eu, n edges ev, n edges none, n edges >1, n scratch, n comp, n nodes>1, n none, b eu, b ev, b >, b > v, b and, b last, b first, z none, w null, c start init, l a idx, l a low, l a stk, l a call, l a iter.
- 1 timeout: t iter (the arc pointer is never advanced). Run alone with a loop counter (local, -O1, no sanitizer, driver outside the repo, 2 nodes and 1 edge): 50000000 passes of the main loop at depth 1 with the same state, at both a 3 s and a 30 s limit; the unmutated code returns at once. Non-termination shown.
- 4 not killed:
  - s 32bit: 32-bit instead of 64-bit size arithmetic in the scratch check. After TooLarge, 6N + E + 1 is at most 655361, far below 2^32, so both give the same result. Equivalent.
  - t low idx: the low-link of the node is lowered with `low[w]` instead of `idx[w]` for a node w still on the stack. This is the known variant of Tarjan's method; it gives the same components and the same number of steps. Equivalent in results.
  - t root test3: an extra condition `depth == 1` in the root test. The DFS root always has low equal to idx (nothing older is on the stack), so the condition never changes the test. Equivalent.
  - t parent cond: the low-link of the DFS root is not updated from its children. The root's low always equals its idx and a child's low cannot be lower than the root's idx, so nothing changes. Equivalent.
  No time-only survivor remains, so no timing comparison was needed.

## Not run

Exhaustive max-size cases, the looped 5-node space (2^25), timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
