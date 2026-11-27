# maxflow: maximum flow and minimal source side of a minimum cut

Code: `core/maxflow/maxflow.cpp` (contract in `core/maxflow/maxflow.h`). Tests: `tests/maxflow/maxflow_tests.cpp`. Reference: `tools/maxflow_ref.py`.

## Rule

- Directed edge i goes from `eu[i]` to `ev[i]` with capacity `ecap[i]` (u32, may be 0). Parallel edges, antiparallel pairs and self-loops are allowed. A self-loop carries nothing.
- `*out_flow` is the maximum flow value from `src` to `sink` as u64. `out_side[v]` is 1 for nodes reachable from `src` in the residual graph of a maximum flow, else 0. That set is the minimal source side: the intersection of all minimum source cuts. It is the same for every maximum flow.
- Limits: nodes at most `kMaxNodes` = 4096, edges at most `kMaxEdges` = 65536. The largest possible flow is 65536 * 4294967295 = 281474976645120 (2^48 - 65536). No flow above 2^48 can occur, and nothing here claims one.
- `maxflow_scratch_words` gives 8 * edges + 6 * nodes u32 words.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `*out_flow` and `out_side[0 .. nodes)` are written. All validation finishes before the first write. On any failure nothing is written. Scratch contents after the call are unspecified and are not tested.

## Check order (`maxflow_run`)

1. NullArgument: `out_flow` always; `eu`, `ev`, `ecap` only while edges is above 0; `scratch` only while nodes + edges (64 bits) is above 0; `out_side` only while nodes is above 0.
2. TooLarge (nodes above `kMaxNodes` or edges above `kMaxEdges`).
3. ScratchTooSmall (`scratch_words` below 8 * edges + 6 * nodes, 64 bits).
4. BadNode: `src` or `sink` at or above nodes, then every edge endpoint, loops included.
5. SameTerminal (`src` equal to `sink`).

## Method and scratch layout

Iterative Dinic. Edge i gives arcs 2i (forward) and 2i + 1 (reverse, capacity 0); self-loops and zero-capacity edges get no arcs. Layout in the scratch: to[2E], cap[2E], adj[2E], start[N+1], level[N], iter[N], queue[N], path[N]. That is 6E + 5N + 1 words, inside the 8E + 6N the contract asks for, so the layout has slack. Each phase builds BFS levels, then a depth-first search over an explicit arc stack (`path`) with current-arc pointers; a 4096-node path needs no deep call stack. After an augment the search retreats to the tail of the first saturated arc. A node with no way forward gets level none for the rest of the phase. The step count does not depend on capacity values. A last residual BFS from `src` fills `out_side`, and `*out_flow` is written last.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan)

10 test cases, 581683 assertions, no warnings. Expected values come from `tools/maxflow_ref.py`, which does not share the method: brute force over node subsets for the flat table, Edmonds-Karp for digest groups, the battery and mid graphs, FIFO push-relabel with global relabeling plus a certificate check for the max-size graphs. Edmonds-Karp was cross-checked against brute force on 20000 random small graphs. Success paths use exact-size buffers. No scratch contents are asserted.

Exhaustive:
- Flat table: nodes 2 with up to 3 edges and nodes 3 with up to 2 edges, capacities {0,1,2}, all ordered terminal pairs (8312 cases).
- Four digest groups, all ordered pairs: nodes 3, up to 3 edges, caps {0,1,2} (122640 cases); nodes 4, up to 2 edges, caps {0,1,2^32-1} (28236); nodes 3, up to 3 edges, caps {0,2^32-1} (37050); nodes 4, up to 3 edges, caps {1} (52428).

Sampled: the random battery (20000 graphs, nodes 2..12, edges 0..40, four capacity modes, shared LCG seed 777, digest of flow and side, plus a cut-capacity check per case); 80 mid graphs (nodes 2..24, edges 0..120); 10 max-size patterns (layered, parallel bundles, scrambled path, bipartite, 90% zero capacity, loops only, unreachable sink, dense core with huge capacities, grid, 65536 parallel edges at 4294967295 giving exactly 281474976645120). Hand-checked: serial equal bottlenecks, diamonds, antiparallel pairs, a flow cycle that does not count, a unit case with several maximum flows, totals around 2^32, 2^40, 2^47 and the exact maximum, a 4096-node path with and without a middle bottleneck, argument rules and check order.

## Mutation testing

127 source mutants (`/tmp` script, not in the repo), 10 s timeout, built with ASan + UBSan, one full run on the final tests.

- 85 killed by a failed assertion.
- 12 crash-only (no failed assertion, killed by a sanitizer or abort): w null flow, w null edges>0, w null edges none, w null eu only, w null ev only, w null cap only, w null side none, w sizes null, d bfs visited, d adv level, d adv step, f visited.
- 16 timeouts. 13 did not finish within 5 s on a 20000-graph small-graph driver that the unmutated code finishes well under 1 s (non-termination shown by that hang test): b cap live, b count none v, d bfs init, d bfs src level, d bfs cap, d bfs level, d bfs end, d iter reset, d full none, d adv cap, d adv level ge, d adv skip, d sink test. 3 are unclassified (they finished that driver with equal results; the slowness shows only in the test run): w sink >=, w src only, d iter reset part. They are not called hangs or quadratic.
- 14 not killed:
  - b live loop: self-loops also get arcs. A loop arc never satisfies the level rule, so results are the same; it fits the 2E arcs. Equivalent.
  - d dead src: `depth == 0` instead of `u == src`. The two are the same at that point because levels rise strictly along the path. Equivalent.
  - d dead iter: the current-arc pointer is not advanced on retreat. The dead node has level none, so that arc fails the next check and is skipped. Same results.
  - f side vs dist: also excludes `sink` from the side. After a maximum flow `sink` is never residual-reachable. Equivalent.
  - b arcs, b layout adj, b layout start, b layout iter, b layout queue, b layout path: shift an array by 1 to 4 words. The layout uses 6E + 5N + 1 of 8E + 6N words, so the shifted arrays still fit and stay disjoint; no result changes. Killing them needs asserting scratch contents or a tighter bound, which is not tested.
  - d full cond, d retreat depth, d adv level eq, d dead mark: results are the same on every test but each costs extra steps (a zero-capacity arc stays on the path, restart from the source after each augment, arcs that skip a level, dead nodes re-entered). Not killed, not proved equivalent in time. No timing is asserted.

## Not run

Exhaustive max-size cases, timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
