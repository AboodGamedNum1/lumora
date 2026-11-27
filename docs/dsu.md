# dsu: disjoint-set union with the smallest element as root

Code: `core/dsu/dsu.cpp` (contract in `core/dsu/dsu.h`). Tests: `tests/dsu/dsu_tests.cpp`. Reference: `tools/dsu_ref.py`.

## Rule

Elements are 0 .. n-1, stored in one caller array of 32-bit parents. `dsu_init` makes n singleton sets. `dsu_find` returns the root of the set of x, `dsu_unite` joins two sets, `dsu_count_sets` counts the roots. The root of a set is its smallest element, and an element is a root exactly when `parent[i] == i`. Integer only: no allocation, no recursion, no global state.

## Method

`find` walks from x to the root in a loop, then walks the path again and points each node at the root (compression). Only non-root entries are written. `unite` finds both roots and sets the parent of the larger root to the smaller root, so the canonical root stays the smallest element. There is no union by rank, because the header fixes the choice of root.

## Check order

Every function: a null pointer (the parent array and every out pointer) gives NullArgument, even when n is 0. Then n above kMaxElements (2^26) gives TooLarge. Then an element argument at or above n gives OutOfRange (a or b in `unite`, x in `find`). With valid pointers and n = 0, `find` and `unite` give OutOfRange, `init` and `count_sets` give Ok. All validation happens before any read or write of the array, so on failure the array and the out values are untouched and `find` does not compress.

## Decisions

Ten readings were confirmed by the maintainer through the coordinating agent, with no corrections. Compression is allowed but not required. The tests never depend on whether it happened, or on any non-root value; they check roots (`parent[i] == i`), `find` results, counts, merged flags and result codes. The one exception is a failed call, where the header says nothing is written and the tests compare the whole array. No aliasing claim.

## Tests

Expected values come from `tools/dsu_ref.py`, an independent quick-find model: one label per element, merging relabels a set, the root is the minimum of the members. It has no parent array and no compression. It emits 49 scenarios: 7 sizes (2, 3, 5, 16, 64, 257, 1000) times 7 operation modes (random, chain up, chain down, star, star from the top element, same element, repeated pair). After every operation the test checks the merged flag, the set count and a `find`; at the end it checks every root and that `parent[i] == i` exactly for the roots. Buffers have the exact size, so ASan sees any overrun.

10 test cases, 1328768 assertions:
- the 49 scenarios against the reference
- exhaustive: n = 5, every sequence of 3 unite calls over the 25 ordered pairs (15625 sequences), with a `find` of every element after each step, against a label-array oracle
- random sequences: the root is the smallest element, and the new root is the smaller old root
- `find` never changes the set of roots
- a long chain of 200000 elements, built by `unite(i, i-1)` from the top down, resolved by one `find` (a recursive find would overflow the stack; the test checks results only, not the shape of the array)
- init, count and sizes 0, 1, 2, 7, 256, 1001
- unite on equal and already joined elements
- argument rules and check order, including the boundaries x = n and x = n - 1, oversized n with a 4-element array (the array is never read), and nothing written on failure
- a failing `find` does not compress (full array compared)
- `find` and `unite` with n = kMaxElements on a one-element array holding 0, element 0: both must give Ok, root 0, merged false (only element 0 is touched, so no large allocation)

Local run (g++, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI.

## Not run

- A successful `init` or `count_sets` at n = kMaxElements (2^26 elements, 256 MiB): they touch the whole array. No large allocations in tests. `find` and `unite` at n = kMaxElements are run on a one-element array. The TooLarge side is run with n = kMaxElements + 1 and a 4-element array.
- Arrays not made by `dsu_init` or changed by other means: unspecified by the header, not tested.
- MSVC, Clang, Windows and Android: no result seen. The test file has no compiler-specific constructs.

## Mutation

61 source mutants of `core/dsu/dsu.cpp`, each built with ASan+UBSan and `-fno-sanitize-recover=all`, run against the full test file. 55 killed, 6 survived. Local result.

Survivors, equivalent (4). Each argument holds for arrays made by `dsu_init` and changed only by these functions, the only arrays the header specifies:
- find loop condition `parent[root] < root` instead of `!=`: every non-root entry points to a smaller element (unite points the larger root at the smaller one, compression points at the root, which is smaller), so the two conditions agree.
- find loop stops also when `root == 0`: element 0 is the smallest element, so it is always a root.
- compression removed: compression is optional, and the results (roots, finds, counts) do not depend on it.
- compression step sets `parent[cur] = parent[parent[cur]]` (path halving) instead of the root: the loop still ends at the root, roots are untouched, and every result is the same.

Survivors, NOT equivalent (2): `n >= kMaxElements` instead of `n > kMaxElements`, in `init` and `count_sets`. They differ only at n = kMaxElements, where the real code accepts and the mutant returns TooLarge. That case needs the whole 256 MiB array and is not run (see above). Not killed. The same mutants in `find` and `unite` are killed by the one-element-array test at n = kMaxElements.
