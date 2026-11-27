# euler: Eulerian path or circuit of a directed multigraph

Code: `core/euler/euler.cpp` (contract in `core/euler/euler.h`). Tests: `tests/euler/euler_tests.cpp`. Reference: `tools/euler_ref.py`.

## Rule

- Edge i goes from `eu[i]` to `ev[i]`. Parallel edges and self-loops are allowed. Nodes without edges are ignored.
- Circuit: every node has equal in-degree and out-degree and all edges lie in one weakly connected component (so a graph of self-loops on one node is a Circuit). `out_edges[0 .. m)` is a closed trail and `*out_len` = m. With m = 0 the kind is Circuit, `*out_len` = 0 and `out_edges` is not written.
- Path: all edges in one weakly connected component, exactly one node with out-degree minus in-degree 1, exactly one with in-degree minus out-degree 1, all other nodes equal. `out_edges[0 .. m)` is an open trail that starts at the node with out-degree above in-degree; `*out_len` = m. Balanced graphs are never reported as Path.
- None: no trail exists (any other degree pattern, or edges in more than one component). `*out_kind` = None and `*out_len` = 0 are written; `out_edges` is untouched. None is an Ok result.
- Which trail is returned is not specified; the tests check only that it is a valid trail.
- Limits: `kMaxNodes` = 65536, `kMaxEdges` = 262144.
- `euler_scratch_words` writes 6 * n + 3 * m + 8 for every in-limit size. Null `out_words` is NullArgument, then oversize is TooLarge with nothing written.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On Ok, `*out_kind` and `*out_len` are written, and `out_edges[0 .. m)` only for a Circuit or Path; the array keeps its values at and beyond m. All validation finishes before the first write. On any failure (NullArgument, TooLarge, ScratchTooSmall, BadNode) nothing is written. Scratch contents after the call are unspecified and are not tested.

## Check order (`euler_trail`)

1. NullArgument: `out_kind` and `out_len` always; `eu`, `ev` and `out_edges` only while m is above 0; `scratch` only while n is above 0. The conditions use the given sizes even above the limits.
2. TooLarge (n above `kMaxNodes` or m above `kMaxEdges`).
3. ScratchTooSmall: only while n is above 0, `scratch_words` below 6 * n + 3 * m + 8 (64-bit arithmetic). With n = 0 `scratch_words` is ignored.
4. BadNode: any endpoint at or above n, in one pass over every edge before any write. With n = 0 any m above 0 is BadNode. BadNode wins over a graph with no trail, whatever the edge order.

n = 0 with m = 0 is Ok, Circuit, length 0.

## Method and scratch layout

Degree counts decide the kind: a Circuit needs all degrees equal, a Path needs one +1 node and one -1 node, anything else is None. Then Hierholzer's algorithm with an explicit edge stack (no recursion), started at the +1 node for a Path and at the tail of edge 0 for a Circuit. Edges are popped into the trail from the back. If fewer than m edges were used, the edges were not in one component and the answer is None. The trail is built in scratch and copied to `out_edges` only when all m edges were used, so None leaves `out_edges` untouched. O(n + m) steps: each edge is pushed and popped once, and each node's cursor only moves forward.

Layout of the scratch, in words: start[n+1], adj[m], cur[n], ind[n], stk[m], res[m]. That is 3n + 3m + 1, inside the 6n + 3m + 8 the contract asks for.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

17 test cases, 116346 assertions, no warnings. Local run line, from the repository root with the Catch2 objects of the build directory: compile `core/euler/euler.cpp` and `tests/euler/euler_tests.cpp` with the flags above and link with Catch2Main. The full suite takes about 5 s on the machine used. Buffers are exact size under ASan; tiny or null buffers are used only on failure paths and empty graphs.

Expected kinds come from `tools/euler_ref.py`. For every small graph it is a brute force over edge orders: a depth-first search over edge numbers (parallel edges are different edges), once per first edge, that stops at the first complete trail. A graph is a Circuit when some trail ends where it starts, a Path when some trail ends elsewhere (the set of first tails must be one node, which is the required start), None otherwise. No degree rule and no Hierholzer step is used there. The max-size patterns are known by construction and are confirmed by degree counts and union-find in the reference (`--selfcheck` compares that rule reference with the brute force on 30000 random graphs of up to 7 edges, 0 mismatches). Goldens are digests of every result in a fixed order (kind byte, then the required start as 4 bytes, 0xFFFFFFFF unless Path).

A certificate checker in the test (separate from the library and the reference) validates a result: a trail uses every edge exactly once, the head of each edge is the tail of the next, a Circuit ends where it starts, a Path ends elsewhere and starts at the required node; for None the length is 0 and `out_edges` is untouched. An in-test degree and union-find classifier supplies the expected kind where no digest exists (relabelled copies) and as a second check inside the exhaustive groups.

Exhaustive (by digest, golden in the test, brute-force reference):
- `every multiset of 0 to 6 self-loops on 1 node`: 7 graphs.
- `every multiset of 0 to 6 edges on 2 nodes`: 4 edge kinds (u, v), 210 graphs.
- `every multiset of 0 to 5 edges on 3 nodes`: 9 edge kinds, 2002 graphs.
- `every multiset of 0 to 4 edges on 4 nodes`: 16 edge kinds, 4845 graphs.
- `every ordered edge list of 0 to 6 edges on 2 nodes`: 5461 lists.
- `every ordered edge list of 0 to 4 edges on 3 nodes`: 7381 lists.
- `every ordered edge list of 0 to 3 edges on 4 nodes`: 4369 lists.
- `every set of the 12 non-loop edges on 4 nodes`: all 2^12 = 4096 subsets, edges in (u, v) order.
These are exhaustive for the stated edge counts and node counts only; larger sizes are sampled below.

Sampled, not exhaustive: a 20000-graph random battery (seed 7171, 1 to 6 nodes, 0 to 7 edges; uniform random edges, shuffled open walks, shuffled closed walks, closed or open walks with one edge head changed) by digest, with the kind counts (2602 None, 5981 Path, 11417 Circuit); 200 mid graphs (seed 7272, 2 to 9 nodes, 0 to 12 edges, also two disjoint circuits), digest and certificate, brute-force reference; 15 max-size patterns by kind and required start (no edges, a 65535-edge chain, a 65536-ring with 196608 self-loops, a random closed walk and a random open walk of 262144 edges, 262144 self-loops on one node, two nodes with 131073 and 131071 edges (None), two nodes with equal counts, two nodes as a Path with one self-loop, two disjoint rings, a chain with a separate ring, the complete digraph with loops on 512 nodes, a star of 131070 edges, a ring with one extra edge, 262144 uniform random edges); relabelled copies (3000 small graphs with renamed nodes and shuffled edges, and 7 reversed max-size graphs).

Hand-checked: 23 small graphs (self-loops, no edges with isolated nodes, single edge, two-cycle, parallel edges, chains listed forwards and backwards, triangle, extra isolated node, two circuits, two paths, a path plus a separate self-loop, two sinks, two sources, a path through a self-loop, paths that start in the middle or at the higher node number), argument rules and check order (each pointer alone, also for m = 1 and n = 1, pointers not needed while m or n is 0, null before TooLarge before ScratchTooSmall before BadNode, ScratchTooSmall by one word, endpoint equal to n in either array, BadNode over a no-trail graph in both edge orders, nothing written on any failure), None writes kind and length only, a trail writes exactly m words, and the smallest sizes (one node).

## Mutation testing

106 source mutants (script outside the repo), built with ASan + UBSan and run against the final tests. The whole final test file ran on every mutant (no staging; the tests changed once after a first mutant run, which found three gaps: a null check at m = 1, a null scratch at n = 1, and an endpoint equal to n in `ev`; tests were added and the whole mutant set was run again).

- 88 killed by a failed assertion.
- 12 crash-only (no failed assertion, killed by a sanitizer or abort): wnull, n kind, n len, n eu, n ev, n oe, n m1, n scr1, z m0 cond, h cmp, h push, o copy3.
- No timeouts.
- 6 not killed by the whole file (survivors), argued:
  - s u32: 32-bit size arithmetic in the scratch check. After TooLarge, 6n + 3m + 8 is at most 1179656, below 2^32. Equivalent.
  - c psum: the prefix sum skips v = 0, but `start[0]` is 0, so the first step adds 0. Equivalent.
  - d src2: a node with out-degree at least in-degree + 1 counts as the source. Degrees sum to zero, so a node with a surplus of 2 or more needs either a node with a deficit of 2 or more (which sets the failure flag) or two or more deficit-1 nodes (then the sink count differs from the source count, giving None). The accepted graphs are the same. Equivalent.
  - d snk: the same argument for the sink test (a deficit of 2 or more needs a surplus of 2 or more, which sets the failure flag, or two surplus-1 nodes). Equivalent.
  - d chk eq: the source count and the sink count are not compared. With every difference in -1, 0, 1 the differences sum to 0, so the counts are equal. Equivalent.
  - d s4: a Circuit starts at the head of edge 0 instead of its tail. In a balanced graph the head of an edge has an out-edge, and Hierholzer's circuit from any node with edges uses every edge of its component, so the result is the same valid circuit set. Equivalent.

## Not run

Exhaustive max-size cases, exhaustive graphs with more than 4 nodes or edge counts above those stated, a brute-force kind for any graph above 12 edges (the max-size kinds rest on construction plus the rule reference), timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
