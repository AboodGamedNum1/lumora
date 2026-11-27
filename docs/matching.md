# matching: maximum matching in a bipartite graph

Code: `core/matching/matching.cpp` (contract in `core/matching/matching.h`). Tests: `tests/matching/matching_tests.cpp`. Reference: `tools/matching_ref.py`.

## Rule

- Edge i joins left vertex `eu[i]` to right vertex `ev[i]`. Parallel edges are allowed and count once for matching purposes.
- A matching is a set of edges with no vertex used twice. `*out_size` is the size of a maximum matching.
- Per-vertex status, `cl[v]` for left vertices and `cr[w]` for right vertices:
  - 2: the vertex is matched in every maximum matching.
  - 1: the vertex is matched in some maximum matching and unmatched in another.
  - 0: the vertex is matched in no maximum matching. This happens only for a vertex with no edge.
- Limits: `kMaxSide` = 65536 vertices per side, `kMaxEdges` = 262144 edges.
- `matching_scratch_words` writes 8 * left + 8 * right + 2 * edges + 2 for every in-limit size. A null `out_words` is NullArgument, then oversize is TooLarge with nothing written; endpoints are not checked.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `cl[0 .. left)`, `cr[0 .. right)` and `*out_size` are written; both arrays keep their values at and beyond those lengths. All validation finishes before the first write. On any failure all outputs are untouched. Scratch contents after the call are unspecified and are not tested.

## Check order (`matching_run`)

1. NullArgument: `out_size` always; `eu` and `ev` only while edges is above 0; `scratch` only while left + right is above 0; `cl` only while left is above 0; `cr` only while right is above 0. The conditions use the given sizes even above the limits.
2. TooLarge (left or right above `kMaxSide`, or edges above `kMaxEdges`).
3. ScratchTooSmall: only while left + right is above 0, `scratch_words` below 8 * left + 8 * right + 2 * edges + 2 (64-bit arithmetic). With both sides 0 `scratch_words` is ignored.
4. BadNode: one pass over every edge for `eu` not below left or `ev` not below right, before any write. With left or right equal to 0, any edge is BadNode.
5. Left and right both 0: size 0, Ok.

## Method and scratch layout

Iterative Hopcroft-Karp on a left-side compressed adjacency (a right-side copy is built for the second pass). Each phase builds breadth-first layers from the free left vertices, then runs an explicit-stack depth-first search with per-vertex edge cursors, so a phase costs O(L + E). Statuses come from alternating reachability once the matching is maximum: a left vertex is unmatched in some maximum matching exactly when it is free or reachable from a free left vertex by an alternating path of even length (non-matching edge to a right vertex, then its matching edge back); the right side is symmetric. A free vertex with no edge has status 0; a matched vertex that is not reached has status 2; the others have status 1.

Layout of the scratch, in words: lstart[L+1], ladj[E], rstart[R+1], radj[E], mL[L], mR[R], dist[L], it[L], stk[L], seenL[L], seenR[R], q[L+R]. That is 7L + 4R + 2E + 2, inside the 8L + 8R + 2E + 2 the contract asks for. No recursion, so a 65536-vertex augmenting path needs no deep call stack.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

14 test cases, 2333276 assertions, no warnings. Local run line, from the repository root with the Catch2 objects of the build directory: compile `core/matching/matching.cpp` and `tests/matching/matching_tests.cpp` with the flags above and link with Catch2Main. The full suite takes about 50 s on the machine used. Expected values come from `tools/matching_ref.py`. Small graphs use brute force over every matching (the set of vertex masks of all matchings). Large graphs use a second method, phase-wise Kuhn with visited stamps plus Dulmage-Mendelsohn reachability; `python3 tools/matching_ref.py --selfcheck` compares the two with brute force on 30000 random graphs (0 mismatches), and the 100 mid graphs are compared inside the generator. Success paths use exact-size scratch (filled with garbage) and exact-size `cl` and `cr`. Tiny or null buffers appear only for failures and empty inputs. No scratch contents, aliasing or timing are asserted. Test names carry the exact extents.

Exhaustive (each by full result or by a digest of every result in a fixed order, with the golden in the test):
- Flat table: every simple bipartite graph with 1 to 3 vertices per side (682 graphs), full results.
- Every simple bipartite graph with 0 to 4 vertices per side, all 25 side pairs, up to 2^16 each (74963 graphs), digest.
- Every simple bipartite graph with 5 + 4 and with 4 + 5 vertices (2^20 each, 2097152 graphs), digest.
- Every simple bipartite graph with 6 + 3 and with 3 + 6 vertices (2^18 each, 524288 graphs), digest.
- Every ordered edge list on 2 + 2 vertices with up to 6 edges, on 2 + 3 and 3 + 2 with up to 5 edges, and on 3 + 3 with up to 4 edges, with parallels (31504 lists), digest.

Sampled, not exhaustive: a 20000-graph random battery (seed 7171, 1 to 10 vertices per side, 0 to 40 edges) by digest; 100 mid graphs (up to 9 + 9 vertices, up to 30 edges) each checked against the reference and a C++ removal oracle on every vertex; relabelled copies that rename vertices, shuffle the edge order, and the mirror graph with the sides swapped (statuses must follow the vertex); and 15 max-size patterns by digest and size (seed 6000 + pattern): 0 identity perfect matching, 1 random perfect matching plus random edges up to 262144, 2 one left vertex joined to all 65536 right vertices, 3 all left vertices joined to right vertex 0, 4 1024 complete 16 + 16 blocks (16384 per side), 5 8192 complete 8 + 4 blocks (65536 + 32768), 6 and 7 two long path graphs (each has a unique perfect matching), 8 no edges at 65536 + 65536, 9 no left vertices, 10 all left vertices joined to one right vertex, 11 identity with every edge four times, 12 uniform random with 262144 edges, 13 random with 100000 edges on 65536 + 40000, 14 random with 262144 edges on 512 + 65536. Closed-form size and statuses are asserted for patterns 0, 4, 5, 6, 7, 11 and the star cases; the random ones use digests and sizes.

Removal oracle on the max-size graphs: for each pattern, 2000 sampled left and 2000 sampled right vertices are checked against an independent C++ matching in the test (removing the vertex and recomputing the size, separate from the library and the reference): 1000 status-2 and 1000 other draws per side, with replacement, all 2000 from one class when the other is empty. Pattern 8 has no edges, so it has no status-2 class; pattern 9 has no left vertices. Counted node visits per pattern 0 to 14 are 4000, 231303786, 1000, 1000, 64000, 9008, 129929486, 130078860, 0, 0, 2000, 4000, 61902536, 30931 and 994523. The oracle test takes about 18 s under ASan at -O1 on the machine used.

Hand-checked: every case of the rule above, empty sides, and argument rules and check order (each pointer alone, null before TooLarge, TooLarge before ScratchTooSmall before BadNode, exact scratch and one word less, every BadNode position and field, untouched outputs on every failure, 64-bit size check at the limits).

## Mutation testing

148 source mutants (script outside the repo), built with ASan + UBSan. Staged run: a quick pass ran every mutant against all test cases except the three longest (5 + 4 and 4 + 5, 6 + 3 and 3 + 6, and the max-size removal oracle); every mutant that was not killed there then ran the whole final test file, including those three. A kill by the subset counts as a kill. The tests did not change between runs; the only change after the runs was to comments in `matching.cpp`.

- 110 killed by a failed assertion.
- 20 crash-only (no failed assertion, killed by a sanitizer or abort): n out_size, n eu, n ev, n edges none, n edges >1, n scratch none, n scratch left only, n scratch right only, n scratch >1, n cl none, n cl >1, n cr none, n cr >1, z none, w null, h dfs layer, r left seen, r left mark, r right seen, r right mark2.
- 11 timeouts at 40 s in the quick pass: h found none, h found inv, h bfs dist, h it reset, h dfs free inv, h dfs layer0, h dfs layer2, h dfs push none, h aug partial, h it bound, h while. Each was run alone with a counter on the phase loop (`for (;;)` over phases) and, as a second counter, on the depth-first loop, on a 1000 + 1000 random graph with 3000 edges and, for `found inv`, on a no-edge graph (10 + 10). A correct run needs at most L + 1 phases (every phase adds at least one edge to the matching). In each case the unmutated code finished at once and the mutant passed 100000 phases of the outer loop with the counter firing within 3 s (at the 3 s and the 30 s limit alike): h found none (size stuck at 926), h found inv (no-edge graph only; on the random graph it terminates with a wrong size, 0), h bfs dist, h it reset, h dfs free inv, h dfs layer0, h dfs layer2, h dfs push none, h it bound, h while on the random graph, h aug partial (size garbage, far above L). The counter fired on the outer loop in every case; the inner loop counter never fired. This shows the phase loop does not leave after 100000 phases, far above the bound, so these mutants are non-terminating in the phase loop in practice; it is not a proof for every input.
- 7 not killed by the whole file (survivors), all argued below:
  - s 32bit: 32-bit size arithmetic in the scratch check. After TooLarge, 8L + 8R + 2E + 2 is at most 1572866, far below 2^32. Equivalent.
  - z and: the empty-graph exit also requires edges 0. With left and right both 0 any edge is BadNode in step 4, so edges is already 0 there. Equivalent.
  - h free dist: free left vertices start at layer 1, not 0. Only differences between layers are used. Equivalent.
  - r left y and r right x: the reachability pass drops the "not free" test on the matched partner. After the last phase no augmenting path exists, so a free right vertex is never reached from a free left vertex by an alternating path (and symmetrically); the partner is never the empty marker. Equivalent.
  - h dead end and h dead end keep: a vertex that failed in the depth-first search is not marked dead (or is marked layer 0). Its edge cursor is already used up, so if it is pushed again it is popped at once and the result is the same; layer 0 never equals a layer + 1, so it is not pushed at all. The results are equal, but the time is not proved equal: they are worded "not killed, not proved equivalent in time". Step argument: each edge cursor only moves forward, so a phase makes at most E cursor moves and at most E + L pushes, whichever way dead vertices are handled, so a phase stays O(L + E). Local doubling timing, 3 * L random edges, O2, one run each: L = 16384, 32768, 65536 take 8.0, 16.9 and 45.5 ms unmutated, 8.0, 17.1, 46.5 ms for h dead end, 7.6, 16.7 and 45.2 ms for h dead end keep. Timings are local and are not asserted.

## Not run

Exhaustive max-size cases, exhaustive simple graphs with more than 6 + 3 vertices (except the 5 + 4 and 4 + 5 group), ordered edge lists above the listed sizes, timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
