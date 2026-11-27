# sssp: single-source shortest paths with counts and canonical predecessors

Code: `core/sssp/sssp.cpp` (contract in `core/sssp/sssp.h`). Tests: `tests/sssp/sssp_tests.cpp`. Reference: `tools/sssp_ref.py`.

## Rule

- Edge i goes from `eu[i]` to `ev[i]` with weight `w[i]`, at least 1 (a weight 0 is BadWeight). Parallel edges and self-loops are allowed. A self-loop never lies on a shortest path.
- `dist[v]` is the length of a shortest path from `source`: exact u64 sums, below 2^48 for in-limit inputs (at most 65535 edges of weight 2^32 - 1), 0 for the source, `kInf` when v is not reachable.
- `count[v]` is the number of shortest paths from the source to v, taken modulo 2^64. A path is a sequence of edges, so parallel edges count separately; `count[source]` = 1. `count[v]` is 0 for an unreachable node, and a REACHABLE node has count 0 when its true number of shortest paths is a multiple of 2^64 (header amendment, maintainer ruling). Only `dist` tells reachability.
- `parent[v]` for a reachable v other than the source is the smallest node index u with a finite `dist[u]` and an edge u to v of weight x with `dist[u] + x == dist[v]`; `kNoNode` for the source and for unreachable nodes. Sums are formed only from a finite `dist[u]`, because `kInf` plus a weight wraps in u64.
- Limits: `kMaxNodes` = 65536, `kMaxEdges` = 262144.
- `sssp_scratch_words` writes 8 * n + 3 * m + 2 for every in-limit size and does not look at endpoints. Null `out_words` is NullArgument, then oversize is TooLarge with nothing written.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `dist[0 .. n)`, `count[0 .. n)` and `parent[0 .. n)` are written; the arrays keep their values at and beyond those lengths. All validation finishes before the first write. On any failure all outputs are untouched. Scratch contents after the call are unspecified and are not tested.

## Check order (`sssp_run`)

1. NullArgument: `eu`, `ev` and `w` only while m is above 0; `scratch`, `dist`, `count` and `parent` only while n is above 0. The conditions use the given sizes even above the limits.
2. TooLarge (n above `kMaxNodes` or m above `kMaxEdges`).
3. ScratchTooSmall: only while n is above 0, `scratch_words` below 8 * n + 3 * m + 2 (64-bit arithmetic). With n = 0 `scratch_words` is ignored.
4. BadNode: `source` not below n (with n = 0 any source is BadNode), or any `eu` or `ev` not below n, over every edge before any write. With n = 0 any edge is BadNode.
5. BadWeight: any weight 0, checked in a second pass over every edge only after step 4 passed, so BadNode wins over BadWeight whatever the edge order.

## Method and scratch layout

Dijkstra with an indexed binary min-heap in scratch, iterative, O((n + m) log n): each node enters the heap once (a decrease re-sifts it up) and is popped once; each edge is relaxed once. The edge list becomes compressed adjacency by source node (the heap array doubles as the fill cursors while it is built). Weights are at least 1, so every shortest-path predecessor of a node has a strictly smaller distance and is popped before it: `count` and `parent` are final when a node is popped and are accumulated at relaxation. A strictly shorter path resets `count` to the predecessor's count and `parent` to that predecessor; an equal one adds the count (u64 wraps) and keeps the smaller index.

Layout of the scratch, in words: start[n+1], adjv[m], adjw[m], heap[n], pos[n]. That is 3n + 2m + 1, inside the 8n + 3m + 2 the contract asks for. A node popped is never improved again (all weights are at least 1), so no "done" flag is needed. No quadratic step: nothing scans all nodes per pop.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

16 test cases, 3798925 assertions, no warnings. Local run line, from the repository root with the Catch2 objects of the build directory: compile `core/sssp/sssp.cpp` and `tests/sssp/sssp_tests.cpp` with the flags above and link with Catch2Main. The full suite takes about 8 s on the machine used. Expected values come from `tools/sssp_ref.py`. Small graphs use brute force: every simple path from the source is listed (with positive weights a shortest walk is a simple path), and the shortest length, the number of shortest edge sequences and the smallest predecessor are read from that list, with no shortest-path algorithm. Large graphs use a second method, heapq Dijkstra plus a pass over the tight edges in distance order; `python3 tools/sssp_ref.py --selfcheck` compares the two and a Bellman-Ford relaxation on 30000 random graphs (0 mismatches), and the 100 mid graphs are compared inside the generator. Success paths use exact-size scratch (8n + 3m + 2 words, filled with garbage) and exact-size `dist`, `count` and `parent` arrays (n entries, sentinel-filled). Tiny or null buffers appear only for failures and empty graphs. No scratch contents, aliasing or timing are asserted. Test names carry the exact extents.

A certificate checker in the test (separate from the library and the reference, using `std::sort`) validates a full result: `dist[source]` is 0, every edge from a finite node is feasible (the successor is finite and `dist[v] <= dist[u] + w`, with the sum formed only from a finite `dist[u]`), every reachable node other than the source has a tight edge, and `count` and `parent` are recomputed over the tight edges in distance order and must match. It is applied to every mid graph, every max-size pattern, the flat table, and the relabelled copies.

Exhaustive (each by full result or by a digest of every result in a fixed order, with the golden in the test; a graph is run once per source):
- Flat table: every multiset of at most 3 edges on 1 node and on 2 nodes, weights 1 to 3 (475 graphs, 930 runs), full results.
- Every multiset of 0 to 6 edges on 1 node (3 loop kinds, 84 graphs), digest.
- Every multiset of 0 to 8 edges on 2 nodes (12 kinds of (u, v, w), 125970 graphs), digest.
- Every multiset of 0 to 5 edges on 3 nodes (27 kinds of (u, v, w), 201376 graphs), digest.
- Every ordered edge list of 0 to 5 edges on 2 nodes (271453 lists), digest.
- Every ordered edge list of 0 to 3 edges on 3 nodes (20440 lists), digest.
These are exhaustive for the stated edge counts only (weights 1 to 3, no more than 3 nodes); larger edge counts and node counts are sampled below.

Sampled, not exhaustive: a 20000-graph random battery (seed 8181, 1 to 10 nodes, 0 to 40 edges, weights up to 6, random source) by digest; 100 mid graphs (seed 8282, up to 9 nodes, up to 20 edges, weights up to 9), digest and certificate; relabelled copies (3000 battery-style graphs and 9 max-size graphs) that rename nodes and shuffle the edge order (distances and counts must follow the node; the result is certified); and 15 max-size patterns by digest, reachable count and certificate (seed 9000 + pattern): 0 path of 65536 nodes with weights 2^32 - 1 (distance up to 281470681677825, below 2^48), 1 the same path reversed with the source at the far end, 2 a 256 x 256 grid with edges right and down (weight 1, counts are binomials modulo 2^64), 3 a 256 x 256 grid with both directions and random weights 1 to 3, 4 an out-star with edges back to the centre, 5 an in-star with source 5, 6 a layered DAG of width 2 (node 0 is the only reachable node of the first layer; layer k holds 2^(k-1) paths, so layer 65 wraps to exactly 2^64, count 0 with a finite dist), 7 a layered DAG of width 3, 8 1024 complete 16-node blocks (only the block of the source is reachable), 9 a path of 65536 nodes with every edge four times (weights 1, 1, 2, 1, count 3^k modulo 2^64), 10 65536 nodes with no edges, 11 uniform random with 262144 edges and weights up to 100, 12 uniform random with weights up to 2^32 - 1, 13 random with 100000 edges and weights 1 to 2 (many ties), 14 a complete graph with loops on 512 nodes (262144 edges, weights 1 to 2). Closed forms are asserted for patterns 0, 1, 2, 4, 5, 6, 9 and 10; the others use digests, reachable counts and the certificate.

Modulo wrap: a layered graph with exactly 2^64 shortest paths to one node (64 stages of two parallel edges) has count 0 and a finite dist there; a node reached both through it and by a direct edge of the same length has count 1 (2^64 + 1 paths); nodes behind it keep count 0 although reachable, and a node with 2^63 + 2^64 paths has count 2^63.

Hand-checked: single node, self-loops and edges back into the source, parallel edges, ties with the smaller index chosen, a shorter longer-path winning, parent changes on a strict improvement, a different source, weights of 2^32 - 1, no edges, different scratch garbage, a larger scratch, and argument rules and check order (each pointer alone, edge pointers not needed while m is 0, node pointers not needed while n is 0, null before TooLarge, TooLarge before ScratchTooSmall before BadNode before BadWeight, BadNode beating BadWeight in either edge order, exact scratch and one word less, every BadNode position and field, untouched outputs on every failure, smallest sizes, 64-bit size check at the limits).

## Mutation testing

139 source mutants (script outside the repo), built with ASan + UBSan and run against the final tests. The whole final test file ran on every mutant (no staging needed; the tests did not change after the mutant runs). One batch of tests was added during the work (smallest sizes), after which the whole mutant set was run again.

- 110 killed by a failed assertion.
- 15 crash-only (no failed assertion, killed by a sanitizer or abort): n eu, n ev, n w, n edges >1, n scratch, n dist, n count, n parent, n nodes>1, b src>, b src none, b ev>, c start init, l insert size, w null.
- 4 timeouts at 30 s, each classified alone with a counter on the pop loop (printed after 400000000 pops, or the number of pops at four sizes for the slow ones):
  - l set dist (the new distance is not stored): the counter fired at 400000000 pops at every size from n = 1000 to 16000 (a correct run pops at most n nodes). Non-terminating in practice, not proved.
  - l greater (relax on a longer path): on the flat-table graph with one node and a self-loop the counter fired at 400000000 pops, with n = 1 (a correct run pops once). Non-terminating in practice, not proved. (On a random 1000-node graph it ends at once with a wrong result.)
  - l pop sift (no sift-down after a pop) and h down cmp2 (sift-down stops at the wrong comparison): these end, but a node is popped many times. Pops for random graphs with 3n edges at n = 1000, 2000, 4000, 8000, 16000: 47877, 185432, 739443, 3059597 and 12781237 for the first, 47877, 185298, 739419, 3059590 and 12781236 for the second (the correct code pops 953, 1873, 3768, 7509 and 15064). About four times more for each doubling: quadratic, so these are slow, not non-terminating.
- 10 not killed by the whole file (survivors), argued:
  - s 32bit: 32-bit size arithmetic in the scratch check. After TooLarge, 8n + 3m + 2 is at most 1310722, below 2^32. Equivalent.
  - c prefix: the prefix sum skips v = 0, but `start[0]` is 0, so the first step adds 0. Equivalent.
  - i pos src: `pos[source]` is not set to 0. The source is the only node in the heap at the first pop, and that pop sets `pos[source]` to the empty marker; nothing reads it before. Equivalent.
  - l pop pos: `pos[u]` is not reset on a pop. A popped node has the final distance and weights are at least 1, so a later relaxation never gives a strictly shorter distance for it, and `pos` is read only after a strict improvement. Equivalent.
  - l du: `dist[u] + 0`. Equivalent.
  - l eq parent <=: on equal indices the same value is stored. Equivalent.
  - h down bound and h down pick bound: the sift-down reads `heap[size]`, which still holds the node being sifted (it was moved to the root from that slot), so the comparison with itself breaks out or picks the same slot. Equivalent.
  - h up cmp and h down cmp (`<` for `<=` in the heap sifts): only the order among nodes with equal distance changes. Equal-distance nodes cannot change each other's result (a weight of at least 1 never gives a tie), so the results are equal. The time is not proved equal; worded "not killed, not proved equivalent in time". Step argument: a sift moves at most the height of the heap, floor(log2 n) steps, whatever it does on ties, so each push and pop stays O(log n) and the total stays O((n + m) log n). Local doubling timing, 4n random edges with weights 1 to 2 (many ties), -O2, one run each, n = 16384, 32768, 65536: 2.1, 4.4 and 10.8 ms unmutated, 2.2, 4.6 and 11.3 ms for h up cmp, 2.0, 4.5 and 10.9 ms for h down cmp. Timings are local and are not asserted.

## Not run

Exhaustive max-size cases, exhaustive graphs with more than 3 nodes or edge counts above those stated, weights above 3 in the exhaustive groups, timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
