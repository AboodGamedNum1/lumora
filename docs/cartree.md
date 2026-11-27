# cartree: min-heap Cartesian tree of a u32 array

Code: `core/cartree/cartree.cpp` (contract in `core/cartree/cartree.h`). Tests: `tests/cartree/cartree_tests.cpp`. Reference: `tools/cartree_ref.py`. Evidence label: unit tests only.

## Rule

- `cartesian_tree(a, n, parent, left, right, root)` writes the tree of `a[0 .. n-1]` in which every node is an index, the root is the index of the minimum, and each subtree is built the same way on the part of the array to the left and to the right of its root. Ties: among equal minima the leftmost index is the root, so an equal element to the right is a descendant of the equal element to its left. `parent[i]`, `left[i]`, `right[i]` are `kNone` (-1) when absent, `parent[root] = kNone`, and `*root` is the root index, or `kNone` when `n = 0`. The in-order walk visits `0, 1, ..., n-1`. `kMaxN` = 2^20.
- Check order: a null `root` is NullArgument; then `n > kMaxN` is TooLarge (also with null arrays); then, only for `n > 0`, a null `a`, `parent`, `left` or `right` is NullArgument. For `n = 0` the arrays are not touched, `*root = kNone` and the result is Ok. On any failure nothing is written, including `*root`. `a` is never modified. The outputs must not overlap `a` or each other.

Integer only: no floating point, no 128-bit types, no compiler builtins or intrinsics, no allocation, no recursion, no mutable statics, no scratch beyond the three output arrays.

## Method

One left-to-right pass. After step `i - 1` the output arrays hold the tree of `a[0 .. i-1]`, and its right spine (the path from the root along right children) ends at node `i - 1`. The spine is reached from `i - 1` through the parent pointers, so no stack is needed. For `i = 1 .. n - 1`: set `parent[i] = left[i] = right[i] = kNone`; walk `cur` from `i - 1` up through `parent` while `a[cur] > a[i]` (strictly greater), remembering `passed`, the last node passed over; the node `passed` (if any) becomes the left child of `i` and gets `parent = i`; the node `cur` where the walk stopped (`a[cur] <= a[i]`) gets `i` as its new right child and becomes the parent of `i`; if the walk ran out of nodes, `i` is the new root. Node 0 starts with all three links `kNone`; `*root` is written last.

Ties: the walk stops on `a[cur] == a[i]`, so an equal element to the right becomes a descendant (right) of the equal element to its left, and the leftmost minimum stays the root.

Why the walk is correct: the nodes on the right spine have non-decreasing values from the root down, and `a[i]` belongs after the last spine node `cur` with `a[cur] <= a[i]`; everything passed over has a larger value, lies to the left of `i`, and is a right-spine suffix, so it forms the left subtree of `i`, whose root is the highest passed node (`passed`). The node `cur` keeps its left subtree, and its right subtree is replaced by `i` (the former right subtree is now inside the left subtree of `i`).

Step bound (structural, not measured): a node is passed over by the walk at most once, because after it is passed it is no longer on the right spine (its ancestors are never walked through again from a later node: the next walk starts at `i`, whose parent chain skips every passed node except the single `passed`, which has `i` as parent). So the total number of loop iterations is below `n` and the whole function takes at most about `3 n` simple steps: O(n).

Memory: reads `a`, writes the three output arrays and `*root`. Indices fit in `i32` (`n <= 2^20`); values of `a` are only compared.

## Tests

Expected trees come from `tools/cartree_ref.py` (Python standard library only; no NumPy; Python 3.8 or newer; `python3 tools/cartree_ref.py check` took about 52 s on the machine used, local, no test asserts time) and from two in-test constructions:
- the reference's oracle, `definition`: the root of a range is its leftmost minimum found by a linear scan, the two parts of the range form the subtrees, kept on an explicit list of ranges (O(n^2)); and `nearest_smaller`: for every `i` the nearest `j < i` with `a[j] <= a[i]` and the nearest `j > i` with `a[j] < a[i]` by monotonic stacks, the parent being the one with the larger value (the right one when the values are equal). The two agree on every small extent, on every generator at `n` up to 2000 (step 37), and the validators pass on all of them;
- the same two constructions written in the test (the range scan on an explicit stack of ranges, allocation free for the exhaustive loops; the nearest-smaller shape for the large cases), neither of which is the loop of the implementation. The in-test brute force is cross-checked against the Python digests on the extents where both exist (the three digest extents below); it is not an independent oracle beyond them.
Validators (reference and test): one root without a parent; `parent`, `left` and `right` agree; a left child has a smaller index and a strictly greater value, a right child a larger index and a greater or equal value; the in-order walk visits `0 .. n-1` once each.
The goldens are FNV-1a digests over 32-bit words (`kNone` as 0xFFFFFFFF; per tree `parent[0..n-1]`, `left[0..n-1]`, `right[0..n-1]`, root; trees folded in the enumeration order). Large inputs come from xorshift32 (13, 17, 5) with seed `0x9E3779B9 + n`, the same generators in the reference and the test: `equal` (all 7), `asc`, `desc`, `saw` (`i mod 1000`), `two` (random 0 or 1), `extremes` (random 0 or 0xFFFFFFFF), `perm` (Fisher-Yates random permutation), `organ` (`min(i, n-1-i)`), `full` (random u32).

Extents, exactly as named in the tests (runtimes are local, measured with the sanitizer build, gcc -O1, ASan + UBSan, one core; the whole test binary took 28 s there; a plain unoptimised build is much slower, a Release build is what CI uses):
- `exhaustive every array of length 0 to 8 with values below its length against the brute force (17650829 arrays)` (exhaustive; slow: about 13 s). Any array is order isomorphic to one with values below its length, so every tie pattern up to length 8 occurs.
- `exhaustive every array over {0,1,2} of length 0 to 13 and over {0,1} of length 0 to 20 against the brute force` (exhaustive; slow: about 7 s; 2391484 and 2097151 arrays).
- `exhaustive every permutation of length 0 to 9 against the brute force (409114 permutations)` (exhaustive; about 1 s).
- `exhaustive reference digests: arrays of length 0 to 6 below their length, {0,1,2} length 0 to 10, permutations 0 to 8` (exhaustive: 50070, 88573 and 46234 arrays; each as one digest from the reference definition, which the reference also checks against the nearest-smaller shape and the validators).
- `sampled large n 1 to 2^20, 9 generators, validator, nearest-smaller tree and reference digest` (sampled; about 7 s): the 9 generators times `n` in {1, 2, 3, 1000, 65535, 65536, 524289, 1048575, 1048576} (2^20 exactly, 2^20 - 1 and 2^19 + 1 included); each run is validated, compared with the in-test nearest-smaller tree and with the reference digest (computed from the reference `nearest_smaller`, itself checked against the definition for small `n` and the generators above). At large `n` the reference digests do not come from the O(n^2) definition.
- `n = 2^20 exact, ties and extremes, 2^19 + 1`: all equal at `n = 2^20` is the right chain `0 -> 1 -> ... -> n-1` (explicit check), all equal except one smaller element at the first, middle and last position, and an alternating 0 / 0xFFFFFFFF array at `n = 2^19 + 1`.
- `failure cases, check order, nothing written (every output pre-filled with a sentinel)`: every one of the 32 combinations of null `a`, `parent`, `left`, `right`, `root` for `n` in {0, 1, 2, 4, 2^20, 2^20 + 1, 2^32 - 1}, the expected result computed from the check order, and the sentinels compared after each failure (nothing written, including `*root`); `n = 0` writes only `*root`; TooLarge for `n` in {2^20 + 1, 2^20 + 2, 2^31, 2^32 - 1} with null and non-null arrays.
- `hand-checked trees and ties`: `[2,1,1,3]` (root 1, children 0 and 2, 3 under 2), all equal (right chain), all equal except one at the first, middle and last place, ascending, descending, `n = 1`, `n = 2`, a 0xFFFFFFFF / 0 / 0xFFFFFFFF array, each with four garbage fills.
- `garbage fills, input unchanged, guard words (random arrays n 1 to 40)`: 3000 random arrays, five fills each, three guard words before and after each array, the input compared with a copy after each call.

## Mutation testing

56 source mutants of `core/cartree/cartree.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (not edited after the run; md5 of the test file a9b7fc49c3e8b21c09dbc87e7d43fbeb before and after). Each mutant ran the test file with abort-after-first-failure under a 60 s limit. A mutant is called killed when a failed assertion is printed; one that ends in a sanitizer abort or a crash without a printed failed assertion is listed as crash-only. A timeout would be listed separately and would not count as a kill (none occurred).

- 48 killed by a failed assertion.
- 7 crash-only: root_null (the null root check removed), size_p1 and size_rm (the size limit loosened or removed, which overruns a small buffer), na_p, na_l, na_r (one array null check dropped), i_end (loop to `i <= n`).
- 0 timeouts.
- 1 not killed (survivor), equivalent in output: na_before_size, which swaps the order of the `n == 0` early return and the `n > kMaxN` check. The two conditions exclude each other (`n = 0` is never above `kMaxN`), so the order cannot change any result.

## Not run

Arrays of length 9 or more other than the sampled large cases and the stated permutation, ternary and binary extents, the large cases against the O(n^2) definition, timing or step-count assertions, memory sanitizer, MSVC, Clang, Windows, Android, a plain unoptimised build of the whole test binary (too slow to be useful; CI builds Release). Nothing here is a CI result.
