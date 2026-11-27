# mst: minimum spanning forest of an edge list

Code: `core/mst/mst.cpp` (contract in `core/mst/mst.h`). Tests: `tests/mst/mst_tests.cpp`. Reference: `tools/mst_ref.py`.

## Rule

- Edges are processed in ascending (weight, index) order. An edge is taken if and only if its endpoints are in different components of the edges taken so far. A self-loop is never taken. The result is unique under this rule.
- `out_idx[0 .. *out_count)` holds the taken edge indices in that processing order, so weights never decrease along the output. `*out_weight` is their sum as u64.
- `*out_count` is nodes minus the number of components (0 for nodes 0 or 1, and when only self-loops exist).
- Limits: nodes at most `kMaxNodes` = 65536, edges at most `kMaxEdges` = 262144.
- `mst_sizes` gives scratch = nodes + edges words and forest words = nodes - 1 (0 for nodes 0 and 1). The forest words are the output capacity every call must offer; it does not depend on the graph.

Integer only: no allocation, no recursion, no mutable statics, no thread_local, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `out_idx[0 .. *out_count)`, `*out_count` and `*out_weight` are written; `out_idx` at and beyond the count keeps its old values. For an empty or loop-only graph `*out_count = 0` and `*out_weight = 0` are written and `out_idx` is untouched. On any failure nothing is written. All validation, including the scan of every edge, runs before the first output write.

## Check order (`mst_forest`)

1. NullArgument: `out_count` and `out_weight` always; `eu`, `ev`, `ew` only while edges is above 0; `scratch` only while nodes + edges (computed in 64 bits) is above 0; `out_idx` only while nodes is above 1. These conditions use the sizes as given, even above the limits.
2. TooLarge (nodes above `kMaxNodes` or edges above `kMaxEdges`).
3. ScratchTooSmall (`scratch_words` below nodes + edges).
4. OutputTooSmall (`out_capacity` below nodes - 1, or below 0 for nodes 0). The capacity is never compared with the number of edges taken.
5. BadEdge: a separate pass over all edges, both endpoints, including edges that Kruskal would never reach after it stops early. nodes 0 with edges above 0 is BadEdge.

`mst_sizes`: NullArgument for either null output pointer, then TooLarge; nothing is written on failure.

## Method

- Scratch `[0 .. edges)` holds edge indices, sorted by an iterative heapsort with key (weight, index): worst case O(m log m).
- Scratch `[edges .. edges + nodes)` is the union-find. A root slot has the top bit set and the low bits hold the component size; any other slot holds the parent. No second array is needed. Finds use path halving, unions attach the smaller component under the larger. Cost is near-linear in n + m after the sort.
- The loop stops as soon as nodes - 1 edges are taken.
- Stack use is a few dozen words.

## Decisions

Confirmed by the maintainer through the coordinating agent: the size and null rules above (including 64-bit nodes + edges, Null before TooLarge), the separate validation pass for every edge before any write, count and weight written as 0 for empty or loop-only graphs, Prim as the reference method with the same (weight, index) key, the size stored in the root entry of the union-find, and the test extents below.

## Tests

Expected values come from `tools/mst_ref.py`, which shares nothing with the C++ method. Small graphs use a brute force over edge subsets (the acyclic subset of maximum size that minimises the sum of 2^rank, where rank is the position in the (weight, index) order). Digest groups, mid and max-size graphs use a heap-based Prim search keyed by (weight, index). Prim is cross-checked against the brute force on 20000 random small graphs. A separate cut-property check (a tree edge is the smallest-key edge crossing the cut its removal makes) runs on every mid graph and 300 further random small graphs; the max-size graphs use a cycle-property check on a sample of untaken edges (binary lifting), a component-count check and a total-weight check. Success tests use buffers of exactly the declared size under ASan and a sentinel-filled output; the words beyond the count must still hold the sentinel.

Exhaustive extents (every graph, with the weights and edge counts named):
- flat table, full result per case: nodes 1 with up to 3 edges, nodes 2 with up to 3 edges, nodes 3 with up to 2 edges, weights 0, 1, 2 (2682 cases);
- digest groups, every graph: nodes 3 with up to 4 edges and weights 0, 1 (111151 cases); nodes 4 with up to 3 edges and weights 0, 1, 2 (112945 cases); nodes 3 with up to 3 edges and weights 0, 4294967294, 4294967295 (20440 cases).

Sampled, not exhaustive: 80 mid graphs (random, up to 40 nodes and 120 edges, with loops, parallel edges, ties and extreme weights; also a structural check with plain label propagation and a check that the scratch fill does not matter), and nine max-size patterns compared by count, total, digest, first and last index: random connected graph at 65536 nodes and 262144 edges, all weights equal (ties by index), all self-loops, a path with descending weights plus parallel heavier copies, disconnected pairs with weights near 2^32 (total above 2^32), a star of 65535 edges of weight 4294967295 plus parallel copies (total 65535 * 4294967295), 100 edges on 65536 nodes, 262144 edges on 1000 nodes, and a random tree with all weights 0.

Also: a golden digest, hand-checked cases (ties by index, parallel edges, loops, forests, descending weights, a total above 2^32, empty graphs), the argument rules and check order (including sizes of UINT32_MAX and a bad edge after the forest is complete), and `mst_sizes`.

## Mutation

One run of all 128 source mutants against the final test file (8 test cases, 491449 assertions), local, g++ -std=c++17 -O1, ASan+UBSan with `-fno-sanitize-recover=all`, 10 s harness timeout. Every count below is from that run.

- 82 killed by assertions (a Catch2 FAILED line).
- 21 crash-only (a sanitizer abort or fault, no failed assertion), listed individually: sizes null scratch; sizes null forest; sizes null and; c null count; c null weight; c null and; c null edges>0; c null edges none; c null eu; c null eu only; c null ev only; c null scratch; c null out none; c bad eu only; c bad ev only; c bad >; c bad > v; c bad and; c bad last; c bad first; c bad none.
- 1 timeout, listed apart: `find step` (path halving followed by an extra store `p[x] = px`). It is a non-termination, shown by a hang test: a 9-node, 9-edge graph that builds a find chain of depth 3 does not finish in 3 s with the mutant (non-sanitized build) and finishes at once with the original. The extra store makes a two-element parent cycle.
- 24 survivors, none non-equivalent in result:
  - Time-only (15; see the next section). The forest is identical in every one; only the shape of the union-find trees, and so the running time, can differ. These are not killed and are not proved equivalent in time: the header forbids quadratic behavior, and no test here asserts time. Each line names the mechanism the variant breaks and the one that stays (the two mechanisms are path halving and union by size):
    - `find no halving` (the path is not shortened): breaks path halving; union by size stays.
    - `find halving only` (`x = px` after the store, which is path splitting): replaces halving by path splitting; both mechanisms stay in kind.
    - `c union size` (the larger component goes under the smaller): breaks union by size, in the opposite direction; path halving stays.
    - `c union size always` (always swap) and `c union never` (never swap): break union by size; path halving stays.
    - `c union <=` (swap on a tie): union by size stays, only the tie rule changes; path halving stays.
    - `c union size sum`, `c union size sum b`, `c union size none` (the stored size is not the component size): break union by size; path halving stays.
    - `c sizes mask` (the first size keeps the root bit, so it never compares smaller: never swap) and `c sizes mask b` (the second keeps the root bit: always swap): break union by size; path halving stays.
    - `c comp init size0` (all sizes stay 0, so every comparison ties): breaks union by size; path halving stays.
    - `c comp init` (every node starts at size 2, so sizes are doubled and keep their order): union by size stays; path halving stays.
    - `c root bit` and `c root bit low` (a different root marker, 0x10000 and 0x40000000; node indices and sizes stay below it): both mechanisms stay.
    - None of the 15 breaks both mechanisms.
  - `before idx <=` (`a <= b` for equal weights): the heap never compares an index with itself, so `<` and `<=` agree.
  - `before w <=`: the branch is entered only for unequal weights, where `<=` and `<` agree.
  - `heap end >0`: the extra pass swaps element 0 with itself and sifts a heap of size 0, a no-op.
  - `sizes forest >1`: nodes - 1 is 0 for nodes 1, so the result is the same.
  - `c target n1`: the target is 0 for nodes 0 and nodes 1 either way.
  - `c target edges guard` and `c target guard`: the guards only skip work that cannot take an edge (no edges, or a target of 0).
  - `c order init`: the sort key is a total order, so the start order of the indices does not change the sorted result.
  - `c loop no early`: once nodes - 1 edges are taken every later edge joins two nodes in the same component, so none is taken.

During development three further null-pointer variants (`c null edges>0`, `c null scratch >1`, `c null scratch nodes`) survived until tests with a single edge and with nodes + edges equal to 1 were added; in this run they are killed.

## Doubled-size timing of the weakest variants

Local, labelled as such: g++ -std=c++17 -O2, no sanitizer, one run, best of 5 calls, whole `mst_forest` call (the sort included), nodes 16384, 32768, 65536 with edges = 4 * nodes (262144 at the largest), against the original. Two inputs, each with one isolated last node so the early stop never fires. Input 0 is a chain (each new node becomes the first endpoint over the previous component, edges in ascending weight) followed by random edges among the first nodes/8. Input 1 merges pairs, then pairs of pairs and so on (a binomial shape), followed by random edges among the last nodes/16 nodes. The variants are the source mutants `find no halving`, `c union never`, `c union size always` and `c union size`. Find-loop iterations were counted with a copy of each source that increments a counter; times and counts come from separate builds.

| variant | input | 16384: ms, steps | 32768: ms, steps | 65536: ms, steps |
| --- | --- | --- | --- | --- |
| original | 0 | 5.57, 131072 | 12.02, 262144 | 26.74, 524288 |
| find no halving | 0 | 5.63, 131072 | 12.19, 262144 | 27.12, 524288 |
| union never | 0 | 5.68, 150447 | 13.25, 300798 | 28.48, 601671 |
| union size always | 0 | 5.85, 131072 | 12.07, 262144 | 27.21, 524288 |
| union size (inverted) | 0 | 5.66, 150447 | 12.23, 300798 | 27.62, 601671 |
| original | 1 | 5.58, 132595 | 12.02, 265164 | 26.76, 530333 |
| find no halving | 1 | 7.45, 820073 | 16.13, 1736953 | 35.20, 3669894 |
| union never | 1 | 5.61, 132595 | 12.04, 265164 | 27.36, 530333 |
| union size always | 1 | 5.65, 140578 | 12.10, 281264 | 26.87, 562520 |
| union size (inverted) | 1 | 5.59, 132552 | 12.07, 265117 | 27.33, 530268 |

Reading: doubling the input roughly doubles the time and the steps for the original and for every variant on both inputs. The one visible cost is `find no halving` on input 1 (about 6 times the original's find steps at 65536, growing by a factor of about 2.1 per doubling, which fits a logarithmic depth). No variant shows a quadratic trend on these inputs. These inputs are not proven worst cases, so this is not a proof that the variants are time-equivalent, and the survivors stay classified as not killed.

## Not run

Any exhaustive case at max size, timing assertions (including the time-only survivors above), memory sanitizer, MSVC, Clang, Windows, Android. CI results are reported separately from this local evidence.
