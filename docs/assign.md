# assign: minimum-cost assignment for a square u32 cost matrix

Code: `core/assign/assign.cpp` (contract in `core/assign/assign.h`). Tests: `tests/assign/assign_tests.cpp`. Reference: `tools/assign_ref.py`.

## Rule

- `cost[i * n + j]` is the cost of giving row i column j, any u32 (0 to 4294967295). An assignment is a permutation `col`; its cost is the sum of `cost[i * n + col[i]]`.
- On Ok, `*out_total` is the least cost over all n! permutations (exact, as u64, at most 512 * 4294967295) and `out_col[0 .. n)` is one assignment with exactly that cost. Which optimal assignment is returned is not specified; ties are free. n = 0 gives total 0 and writes nothing to `out_col`.
- Limit: `kMaxN` = 512. `assign_scratch_words` writes 16 * n + 16 for every in-limit n (16 for n = 0). Null `out_words` is NullArgument, then n above `kMaxN` is TooLarge with nothing written.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. The scratch pointer needs only u32 alignment: a 64-bit value is stored as two u32 words (low word first) and read back with shifts; no pointer is cast to a wider type. `cost` is never modified. No aliasing claim is made.

## Output rule

On Ok only `*out_total` and `out_col[0 .. n)` are written. All validation finishes before the first write. On any failure nothing is written, including `*out_total`. Scratch contents after the call are unspecified and are not tested.

## Check order (`assign_run`)

1. NullArgument: `out_total` always; `cost`, `scratch` and `out_col` only while n is above 0. The condition uses the given n even above the limit.
2. TooLarge (n above `kMaxN`).
3. ScratchTooSmall: only while n is above 0, `scratch_words` below 16 * n + 16 (64-bit arithmetic). With n = 0 `scratch_words` is ignored.

## Method and scratch layout

The Hungarian method with potentials. Rows 1 .. n are added one at a time; each phase finds a shortest augmenting path from the new row to a free column with a Dijkstra-like scan and flips it. Columns are numbered 1 .. n, column 0 is the usual sentinel that holds the new row. Every number is an unsigned 64-bit value: `u[r]` is the row potential and `w[j]` is the negated column potential of the usual formulation, so the "negative" potentials never appear and no signed arithmetic is used. The reduced cost of the edge (r, j) is `cost + w[j] - u[r]`. `minv[j]` is the least reduced cost from a tree row to the unused column j.

Layout of the scratch, in words, with M = n + 1: u[M], w[M], minv[M] (two words per entry), p[M], way[M], used[M]. That is 9 * (n + 1) words, inside 16n + 16. `p[j]` is the row matched to column j (0 when free), `way[j]` the previous column on the path, `used[j]` marks the columns of the tree of the current phase.

### Step bound

In phase i the rows 1 .. i - 1 are matched and row i is new. Each pass of the inner loop adds one new column to the tree and the loop goes on only while the column just added is matched. Only i - 1 columns are matched, so a phase has at most i passes. A pass scans at most n columns and then adjusts n + 1 entries, at most 2n + 1 elementary steps. The phase also resets n + 1 entries and flips a path of at most i columns. In total, at most the sum over i = 1 .. n of (i * (2n + 1) + (n + 1) + i) = n (n + 1) (n + 2) elementary steps, plus O(n) for the final read-out. For n = 512 that is 135005184, so the method is O(n^3) and does not depend on the values.

### Range and sign of every value

Write C = 4294967295 and T for the sum of all the deltas of the whole run.

1. Signs. `u` and `w` start at 0 and only ever receive `+ delta` with `delta >= 0`, so they stay non-negative; they are stored unsigned.
2. Feasibility. At every moment `u[r] <= cost[r][j] + w[j]` for every row r and column j (every reduced cost is non-negative), and matched edges have reduced cost 0. At the start all potentials are 0, so it holds. In an iteration the tree rows and tree columns both gain `delta` (an edge between two tree vertices keeps its reduced cost, and the matched row of a tree column is a tree row, so matched edges stay tight), an edge from a tree row to an unused column loses `delta`, which is at most its reduced cost because `delta` is the minimum of those, and an edge from an unused row to a tree column only gains. So it holds after every iteration.
3. Every subtraction is safe. In `cur = (cost + w[j]) - u[i0]` the bracket is at least `u[i0]` by item 2, so the result is a reduced cost, at least 0, with no wrap. In `minv[j] - delta`, `delta` is the minimum of `minv` over the unused columns, so `minv[j] >= delta`. In the final read-out only additions are made.
4. Size of delta. In phase i the deltas add up to the reduced length of the augmenting path found. The new row has `u = 0` (it was never in a tree) and the free column reached has `w = 0` (a column enters a tree only through a phase that ends with it matched). The potentials telescope along the path, using that matched edges are tight, so the reduced length is (sum of the costs of the forward edges) - (sum of the costs of the matched edges on the path). That is at most `k * C` with k forward edges, and k <= i. So the deltas of phase i add up to at most `i * C`.
5. Hence T <= C * n (n + 1) / 2 <= 4294967295 * 131328 = 564049464917760 (< 2^50) for n = 512. Every `u[r]` and `w[j]` (column 0 included) gains each delta at most once, so they are at most T. Every finite `minv` is a reduced cost, at most `cost + w[j] <= C + T < 2^51`. The sum `cost + w[j]` is below 2^51, so nothing overflows u64.
6. The sentinel `kInf` = 2^64 - 1 is larger than every finite value, so the first scan of a phase replaces it everywhere (an unused column always exists: a phase has at most i - 1 matched tree columns plus column 0, and i <= n). `delta` is therefore finite in every pass, and `kInf` is never subtracted.
7. The total is formed at the end from the chosen entries: at most 512 terms of at most C, below 2^41, exact in u64.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

17 test cases, 622130 assertions, no warnings. Local run line, from the repository root with the Catch2 objects of the build directory: compile `core/assign/assign.cpp` and `tests/assign/assign_tests.cpp` with the flags above and link with Catch2Main. The full suite takes about 10 s on the machine used, 7 s of it in the max-size case. Buffers are exact size under ASan; tiny or null buffers are used only on failure paths and the empty matrix.

Expected totals come from `tools/assign_ref.py`. For every matrix of side up to 6 it is a brute force over all permutations of the columns (itertools.permutations), with no Hungarian logic. For the mid matrices (side 7 to 12) it is an exact dynamic programme over subsets of used columns, also a different method. For the max-size patterns the optimum is known by construction: with cost = r[i] + c[j] + d[i][j] and d >= 0, every assignment costs at least the sum of r and c, and a permutation along which d is 0 reaches it (constant matrices, a zero diagonal and a zero lower triangle are the same argument). Goldens are digests (FNV-1a) of every total in a fixed order, 8 bytes each.

A certificate checker in the test (separate from the library and the reference) validates every result: `out_col` is a permutation, its cost equals the total, the total equals the expected minimum, the matrix is unchanged. The exhaustive groups also compare with an in-test brute force (`std::next_permutation`).

Exhaustive (by digest, golden in the test):
- `every 1 x 1 matrix with an entry from 0, 1, 2, 4294967294, 4294967295`: 5 matrices.
- `every 2 x 2 matrix with entries from 0 to 5`: 1296 matrices.
- `every 3 x 3 matrix with entries from 0 to 3`: 262144 matrices.
- `every 4 x 4 matrix with entries from 0 and 1`: 65536 matrices.
- `every 2 x 2 matrix with entries from 0, 1, 4294967294, 4294967295`: 256 matrices.
- `every 3 x 3 matrix with entries from 0, 1, 4294967294, 4294967295`: 262144 matrices.
These are exhaustive for the stated sides and entry sets only; other values and larger sides are sampled below.

Sampled, not exhaustive: an 8000-matrix random battery (seed 8181, side 1 to 6; entries below 4, below 100, full 32-bit, or within 7 of the maximum), by digest and against the in-test brute force; 300 mid matrices (seed 8282, side 7 to 12, same entry modes), digest and certificate; 12 max-size patterns (side 512 unless noted: all zero, all 4294967295, constant 1234567, zero diagonal with maximum elsewhere, a shuffled zero permutation with random 32-bit entries, a planted matrix with large random parts, a planted matrix with entries within 4000 of 2^32, a planted matrix with many ties, a constant per row, a constant per column, a zero lower triangle with maximum above it, a planted matrix of side 300), by the optimum known by construction; random scratch (3000 matrices of side 1 to 8 with scratch words full random, or per 64-bit pair small, random or near 2^64, so that no two potentials start equal), compared with the brute force; scratch starting at an odd u32 boundary of a larger buffer (200 matrices, side 1 to 9); relabelled matrices (2000 matrices of side 1 to 8 with permuted rows and columns, the transpose, and row and column constants added, whose minimum changes by exactly their sum; and one max-size pattern with reversed rows).

Hand-checked: 1 x 1 matrices including the maximum, 2 x 2 ties, anti-diagonals and all-maximum, classic 3 x 3 and 4 x 4 matrices, a cyclic permutation of zeros, a single cheap cell, a Latin square, argument rules and check order (each pointer alone, also for n = 1, n = 0 needing only `out_total`, null before TooLarge before ScratchTooSmall, ScratchTooSmall by one word, larger scratch, nothing written on any failure), a successful run writes only the total and `out_col[0 .. n)` and leaves the matrix alone.

## Mutation testing

82 source mutants (script outside the repo), built with ASan + UBSan and run against the final tests. Staged run, disclosed: each mutant first ran every test case except the max-size case; a mutant that passed that stage then ran the max-size case alone. A mutant is called killed by the first failure. During the work the first run left two survivors (`i init u`, `i init w`: potentials not initialised) because the scratch fill was one repeated word; the random-scratch test was added and the whole mutant set was run again on the final tests.

- 55 killed by a failed assertion.
- 10 crash-only (no failed assertion, killed by a sanitizer or abort): wnull, n total, n cost, n scr, n col, n n1, l row, l row2, l cur idx, o tot.
- 7 timeouts at the limit, each run alone with a counter:
  - l used (the column is not marked used), l skip (used columns are not skipped in the scan) and l j1 (the chosen column is never set): the whole state of the pass (scratch, phase, column) repeated after 5, 6 and 3 passes of the inner loop on small matrices, so it never ends.
  - l cmp2 (the strict comparison reversed, so `way` is never set): the augmenting loop follows `way` pointers that this loop never changes; the column index repeats (4 was revisited) before reaching 0, so it never ends.
  - p used (the marks are not cleared between phases), l scan (the last column is never scanned) and l mj2 (the minimum search reversed): the loop does not follow the intended path of tree columns and its state kept changing. One phase went over 200000 passes of the inner loop (a correct phase has at most n, at most 12 on these matrices) without a repeat of the state. Not proved non-terminating; the step bound is broken by a factor of more than 16000.
- 10 not killed by the whole suite (survivors), argued:
  - s u32: 32-bit size arithmetic in the scratch check. After TooLarge, 16n + 16 is at most 8208, below 2^32. Equivalent.
  - i init p0: `p[0]` is not cleared at the start; every phase sets `p[0]` before reading it. Equivalent.
  - p used2: `used[0]` is not cleared between phases; the first pass of every phase sets `used[0]` to 1 before it is read, and nothing reads it before. Equivalent.
  - l delta: `delta` starts at 0xFFFFFFFFFFFFFFFE instead of 0xFFFFFFFFFFFFFFFF; every real `minv` is below 2^51 (item 5), so the first comparison replaces it either way. Equivalent.
  - l scan0: the scan starts at column 0, which is always marked used and skipped. Equivalent.
  - l cmp: `<=` for `<` when a reduced cost ties with `minv`: the value is stored again unchanged and only `way[j]` points to the later of two tight predecessors, which is another shortest path of the same length. Another optimal assignment may come out, which the contract leaves open; the total is the same. Equivalent for the contract. The step count is unchanged (the same loops).
  - l mj: `<=` for `<` when choosing the column of the least `minv`: among equal minima the later column is taken, a different but valid tie. Equivalent for the contract, same step count.
  - l delta2: `delta = mj + 0`. Equivalent.
  - w0: `w[0]` is not updated. Column 0 is never scanned and `w[0]` is never read. Equivalent.
  - j0: the loop also stops when the next column is 0. The next column comes from a scan that always finds an unused column (item 6), so it is at least 1. Equivalent.

## Not run

Exhaustive max-size cases, exhaustive matrices of side 4 with more than two entry values or side 5 and above, a brute force beyond side 6 (the mid matrices use the subset programme), timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
