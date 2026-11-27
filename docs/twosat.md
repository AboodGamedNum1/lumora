# twosat: 2-satisfiability with a certified assignment or the smallest conflict variable

Code: `core/twosat/twosat.cpp` (contract in `core/twosat/twosat.h`). Tests: `tests/twosat/twosat_tests.cpp`. Reference: `tools/twosat_ref.py`.

## Rule

- Clause i is `la[i] OR lb[i]`. Literal 2v is variable v, literal 2v+1 is its negation. Tautologies, unit clauses (a literal twice), repeated clauses and both orders of a pair are allowed.
- Satisfiable: `out_sat` is 1 and `value[0 .. n)` holds a satisfying assignment (any one; the tests check only that it satisfies every clause). `out_conflict` is untouched.
- Unsatisfiable: `out_sat` is 0 and `out_conflict` is the smallest v such that v and its negation lie in the same strongly connected component of the implication graph (each is reachable from the other). `value` is untouched.
- n = 0 with m = 0 is Ok with `out_sat` 1. n = 0 with m > 0 is BadLiteral.
- Limits: `kMaxVars` = 65536, `kMaxClauses` = 262144.
- `twosat_scratch_words` writes 16 * n + 2 * m + 2 for every in-limit size. Null `out_words` is NullArgument, then oversize is TooLarge with nothing written.

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types, no compiler-specific constructs. No aliasing claim is made.

## Output rule

On success only `out_sat` and, by the verdict, `value[0 .. n)` or `out_conflict` are written. All validation finishes before the first write. On any failure all outputs are untouched. Scratch contents after the call are unspecified and are not tested.

## Check order (`twosat_solve`)

1. NullArgument: `out_sat` and `out_conflict` always; `la` and `lb` only while m is above 0; `value` and `scratch` only while n is above 0. The conditions use the given sizes even above the limits.
2. TooLarge (n above `kMaxVars` or m above `kMaxClauses`).
3. ScratchTooSmall: only while n is above 0, `scratch_words` below 16 * n + 2 * m + 2 (64-bit arithmetic). With n = 0 `scratch_words` is ignored.
4. BadLiteral: any literal not below 2n, over every clause in one pass before any write.

## Method and scratch layout

The implication graph on N = 2n literals has the edges (not a) to b and (not b) to a for each clause. It is stored as compressed adjacency in scratch. Strongly connected components come from an iterative Tarjan search with explicit stacks, O(n + m) steps. Tarjan numbers components in reverse topological order, so a literal is true when its component number is the smaller one. The conflict is the smallest v whose two literals have the same component number.

Layout of the scratch, in words: start[N+1], adj[2m], idx[N], low[N], comp[N], stk[N], call[N], cur[N]. That is 12n + 2m + 1, inside the 16n + 2m + 2 the contract asks for.

## Tests (local, g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

17 test cases, 7032811 assertions, no warnings. Local run line, from the repository root with the Catch2 objects of the build directory: compile `core/twosat/twosat.cpp` and `tests/twosat/twosat_tests.cpp` with the flags above and link with Catch2Main. The full suite takes about 15 to 20 s on the machine used. Buffers are exact size under ASan; tiny or null buffers are used only on failure paths and empty inputs.

Expected values come from `tools/twosat_ref.py`, which does not use strongly connected components for small instances: it tries every assignment (big-integer bitsets) for the verdict, and takes the conflict from a Floyd-Warshall style reachability closure (v and its negation reach each other). Large instances use an iterative Kosaraju search, cross-checked against the small method (`--selfcheck`, 0 mismatches on 30000 instances). Goldens are digests of every result in a fixed order (sat byte, then the conflict as 4 bytes, 0xFFFFFFFF when satisfiable).

A certificate checker in the test (separate from the library and the reference, breadth-first search only) validates a result: a satisfiable result has an assignment meeting every clause; an unsatisfiable result has v and its negation reaching each other, and every smaller variable does not.

Exhaustive (by digest, golden in the test):
- `every multiset of 0 to 6 clauses on 1 variable`: 3 clause kinds a <= b, 84 instances.
- `every multiset of 0 to 6 clauses on 2 variables`: 16 ordered literal pairs, 74613 instances.
- `every multiset of 0 to 4 clauses on 3 variables`: 36 ordered literal pairs, 91390 instances.
- `every multiset of 0 to 3 clauses on 4 variables`: 36 pairs a <= b, 9139 instances.
- `every ordered clause list of 0 to 4 clauses on 2 variables`: 16 ordered pairs, 69905 lists.
- `every ordered clause list of 0 to 3 clauses on 3 variables`: 36 ordered pairs, 47989 lists.
- `every set of clauses on 3 variables`: all 2^21 = 2097152 subsets of the 21 pairs a <= b.
These are exhaustive for the stated clause counts and variable counts only; larger sizes are sampled below.

Sampled, not exhaustive: a 20000-instance random battery (seed 6161, 1 to 9 variables, 0 to 24 clauses) by digest; 200 mid-size instances (seed 6262, up to 16 variables, up to 2n + 5 clauses, 149 satisfiable), digest and certificate; 15 max-size patterns (no clauses, unit-clause chains, shuffled chains, planted assignments with a conflict gadget on the last variable or on variables 40000 to 40010, uniform random with 262144 and with 50000 clauses, tautologies, repeated clause, equivalence rings, odd-twist ring, dense planted n = 512, planted n = 65536), by verdict, conflict and certificate, plus closed forms; relabelled copies (renamed variables, flipped polarities, shuffled clauses) that keep the verdict and give a certified result.

Hand-checked: smallest sizes (n = 1), argument rules and check order (each pointer alone, pointers not needed while m or n is 0, every pair of failures in order, nothing written on any failure), and untouched outputs for each verdict.

## Mutation testing

112 source mutants (script outside the repo), built with ASan + UBSan and run against the final tests. Staged run, disclosed: each mutant first ran every test case except the 2^21 subsets case; a mutant that passed that stage then ran the 2^21 subsets case alone. A mutant is called killed by the first failure. The tests did not change after the mutant runs.

- 89 killed by a failed assertion.
- 9 crash-only (no failed assertion, killed by a sanitizer or abort): wnull, n sat, n la, n lb, n m1, n value, n scr, n n1, a val loop3.
- 4 timeouts at 30 s, each run alone. Instrumented on a 3-variable, 3-clause instance, with the whole state (scratch, stack heights, counters) recorded at every step of the search loop: t rcur (root cursor reset to 0), t wcur (child cursor reset to 0) and t wedge (edge cursor not advanced) each repeat an identical state after 8, 3 and 4 steps, so they never end (a correct run is bounded by 2N + 2m steps). t pop (top of stack read without popping): the inner pop loop sees an identical state with the top not equal to the root, so it never exits.
- 10 not killed by the whole suite (survivors), argued:
  - s u32: 32-bit size arithmetic in the scratch check. After TooLarge, 16n + 2m + 2 is at most 1572866, below 2^32. Equivalent.
  - c psum: the prefix sum skips v = 0, but `start[0]` is 0, so the first step adds 0. Equivalent.
  - t visited: the root test uses `comp[r]` unset instead of `idx[r]` zero. When a search from one root ends, the stack is empty, so every visited node has a component. Equivalent.
  - t lowcmp: `<=` for `<` when lowering `low[u]` to `idx[w]`; at equality it stores the same value. Equivalent.
  - t popcmp: an extra `sp == 0` exit in the pop loop. The root u is on the stack, so the loop reaches it before the stack is empty. Equivalent.
  - t comp assign and t ncomp: component numbers shifted by one, or counted by 2. Only the order and equality of the numbers are used, and the unset marker (0xFFFFFFFF) is never reached. Equivalent.
  - t cp: the parent update is skipped when the finished node's parent is the root of the search. A root has no smaller number reachable (every node with a smaller index is already in a finished component), so `low[root]` stays equal to `idx[root]`. Equivalent.
  - t plowdone: the parent update is skipped when the finished node just formed a component. Then `low[u]` = `idx[u]` > `idx[p]` >= `low[p]`, so the update never fires. Equivalent.
  - a dir2: `<=` for `<` in the assignment. Equal numbers already returned unsatisfiable. Equivalent.

## Not run

Exhaustive max-size cases, exhaustive instances with more than 4 variables or clause counts above those stated, timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
