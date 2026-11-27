# stirling: Stirling numbers and Bell numbers modulo 998244353

Code: `core/stirling/stirling.cpp` (contract in `core/stirling/stirling.h`). Tests: `tests/stirling/stirling_tests.cpp`. Reference: `tools/stirling_ref.py`. Evidence label: unit tests only.

## Rule

- `stirling2_row(n, out)`: `out[k] = S(n, k)` for `k = 0 .. n`, the Stirling numbers of the second kind. `stirling1_row(n, out)`: `out[k] = c(n, k)`, the unsigned Stirling numbers of the first kind (the row sums to `n!`). `bell_table(n, scratch, scratch_words, out)`: `out[v] = B(v)` for `v = 0 .. n`. All values are exact numbers reduced modulo `kMod` = 998244353, in `0 .. 998244352`. `S(0, 0) = c(0, 0) = B(0) = 1`; for `n` above 0, `S(n, 0) = c(n, 0) = 0` and `S(n, n) = c(n, n) = 1`. `kMaxN` = 4096 is the largest `n`.
- Check order (all three functions): a null `out` (for `bell_table` also a null `scratch`, always, also for `n = 0`) is NullArgument; then `n > kMaxN` is TooLarge; then, for `bell_table` only, `scratch_words < n + 1` is ScratchTooSmall (also for `n = 0`, which needs one word). Nothing is written on any failure, not to `out` and not to the scratch. Each success writes exactly `out[0 .. n]`.

Integer only: no floating point, no 128-bit types, no compiler builtins or intrinsics, no allocation, no recursion, no mutable statics, no arrays beyond the caller's tables.

## Method

- `stirling2_row`: `S(m, k) = k S(m-1, k) + S(m-1, k-1)`. `out` holds row `m - 1` and is turned into row `m` in place by sweeping `k` from `m` down to 1 (so `out[k-1]` is still the old value when `out[k]` is updated), then `out[0] = 0`. Start: `out = 1, 0, 0, ...` (row 0); repeat for `m = 1 .. n`.
- `stirling1_row`: `c(m, k) = (m-1) c(m-1, k) + c(m-1, k-1)`, the same sweep with multiplier `m - 1`; `out[0]` becomes `(m-1) out[0]`, which is 0 from `m = 1` on.
- `bell_table`: the Aitken (Bell) triangle. Row 0 is `[1]`. Row `i` starts with the last entry of row `i - 1`, and each further entry is the previous entry of row `i` plus the entry above it; `B(i)` is the first entry of row `i`. Row `i` has `i + 1` entries. The triangle is kept as ONE row in the scratch: to turn row `i - 1` (in `scratch[0 .. i-1]`) into row `i` (in `scratch[0 .. i]`), set `carry = scratch[i-1]`; for `j = 0 .. i-1`: read `old = scratch[j]`, write `scratch[j] = carry`, then `carry = (carry + old) mod p`; finally `scratch[i] = carry`. Then `out[i] = scratch[0]`.

How `scratch` and `out` are used (as the contract asks): only one triangle row lives in the scratch. `out` is write-only: `out[i]` is written once, with `B(i)`, at the end of row `i`, and never read or rewritten. So `out` ends holding exactly `B(0) .. B(n)`, each entry written once. Scratch indices: row `i` reads and writes `scratch[0 .. i]`, and `i <= n`, so the highest index touched is `n` and the number of words used is exactly `n + 1` (`n + 1` words always suffice and the check `scratch_words >= n + 1` is exactly that; words beyond `n` are never touched, which the tests check with guard words after the scratch). Memory used: scratch `n + 1` words plus `out` `n + 1` words, nothing else.

Why the carry rewrite is correct: the new entry `j` (for `j >= 1`) is `new[j-1] + old[j-1]`, and `new[0] = old[last]`. The loop keeps `carry = new[j]` before step `j` (`carry = new[0]` at the start, and after step `j` it is `new[j] + old[j] = new[j+1]`), writes `new[j]` into slot `j` after reading `old[j]`, and the last carry is `new[i]`. The saved `scratch[i-1]` is read before any slot is overwritten.

Step counts (structural, not measured): each row update of the two Stirling functions is at most `n` steps, `n` rows, so at most `n (n + 1) / 2` updates (about 8.4 million at `n = 4096`); `bell_table` does `n (n + 1) / 2 + n` additions (8394752 triangle entries at `n = 4096`). O(n^2) each, as the contract states.

Ranges: Stirling updates form `m * out[k] + out[k-1]` in `u64` with `m < 4096` and `out[k] < 2^30`, below `2^42`, reduced with one `%` per entry. The Bell additions are two residues, below `2^31`, followed by one conditional subtraction.

## Tests

Expected values come from `tools/stirling_ref.py` (Python standard library only; no NumPy; Python 3.8 or newer; `python3 tools/stirling_ref.py check` took about 15 s on the machine used, local, and no test asserts time). Its methods are different in shape from the in-place sweeps of the implementation:
- exact big-integer triangles for `n <= 300` (no reduction), and the exact Bell numbers from `B(m+1) = sum C(m, k) B(k)` with exact binomials;
- the same rows for any `n` by inclusion-exclusion `S(n, k) = sum_j [(-1)^(k-j) / (k-j)!] [j^n / j!]` (a convolution of two sequences), the rising factorial `x (x+1) ... (x+n-1)` multiplied out by divide and conquer with schoolbook polynomial products for the first kind, and `B(m+1) = sum_k C(m, k) B(k)` modulo `p` with factorial tables for the Bell numbers. They are compared with the exact triangles on every row `n = 0 .. 80` and `n` in {150, 255, 256, 257, 300};
- checks that abort on disagreement: the first-kind row sums equal `n!` and the second-kind row sums equal the Bell numbers at `n <= 300` exactly and at the sampled rows modulo `p`; the near-diagonal closed forms; the exact orthogonality of the signed first-kind and second-kind matrices for every `n, m <= 300`; known textbook values (`B(0 .. 15)`, `S(10, 5) = 42525`, `c(10, 5) = 269325`, `S(7, 3) = 301`, `c(6, 3) = 225`) before the tests use them; and a count over the Aitken triangle modulo `p` (rows 1 to 4096, 8394752 entries) of entries congruent to 0 or to `p - 1` (both counts are 0, used in the survivor arguments below).
The goldens are FNV-1a digests over 32-bit words (one table entry per word, rows folded in the stated order) and explicit values. `python3 tools/stirling_ref.py emit` prints the golden constants used in the test file.

The tests also compare with in-test full two-dimensional triangles (built part by part from the defining recurrences, not in place) and with in-test identities. Every success run uses `out` with three guard words on each side and a sentinel fill, `bell_table` scratch of exactly `n + 1` words (at an odd u32 offset) with three guard words on each side and a fill of zeros, all ones or a junk pattern (guards checked after the call), every entry in `0 .. kMod - 1`. No test asserts scratch contents after a call, and none measures time.

Extents, exactly as named in the tests:
- `stirling: exhaustive rows n = 0..300 of both kinds, every entry` (exhaustive): every `n` from 0 to 300, full rows of both kinds, against the in-test triangles and the digest of all rows concatenated (one for each kind), row sums against `n!` and against the in-test Bell sum, range and guard checks.
- `stirling: exhaustive near-diagonal identities n = 4..300` (exhaustive): for each `n`: `S(n, 0) = c(n, 0) = 0`, `S(n, n) = c(n, n) = 1`, `S(n, 1) = 1`, `S(n, 2) = 2^(n-1) - 1`, `c(n, 1) = (n-1)!`, `S(n, n-1) = c(n, n-1) = C(n, 2)`, `S(n, n-2) = C(n, 3) + 3 C(n, 4)`, `c(n, n-2) = (3n - 1) C(n, 3) / 4`, `c(n, n-3) = C(n, 2) C(n, 4)`.
- `stirling: exhaustive Bell tables n = 0..300, every entry` (exhaustive): every `n` from 0 to 300, three scratch fills each, the table against two in-test sources (the row sums of the second-kind triangle, and the binomial recurrence), the digest at `n = 300`, and the same table from a scratch with 5000 words.
- `stirling: exhaustive orthogonality of signed first-kind and second-kind matrices, n, m = 0..300` (exhaustive): for every `n` and `m` from 0 to 300, both `sum_k (-1)^(n-k) c(n, k) S(k, m)` and `sum_k S(n, k) (-1)^(k-m) c(k, m)` equal `[n = m]`, using the rows the implementation produced.
- `stirling: sampled rows 512..4096 against reference digests, both kinds` (sampled): rows `n` in {512, 1000, 1024, 2047, 2048, 2049, 3000, 4095, 4096} of both kinds, reference digests.
- `stirling: sampled Bell tables n = 1024, 2048, 4095, 4096 against reference digests, prefix property` (sampled): the tables `n` in {1024, 2048, 4095, 4096} (and 300 in the exhaustive test) against reference digests, each equal to the prefix of the `n = 4096` table, and `B(n)` equal to the second-kind row sum at the sampled rows.
- `stirling: sampled row sums, near-diagonal, falling and rising factorial identities up to n = 4096` (sampled): 16 values of `n` from 301 to 4096; the first-kind row sum against `n!`, the near-diagonal identities above, and for six values of `x` (including `p - 1`) the identities `sum_k S(n, k) x (x-1) ... (x-k+1) = x^n` and `sum_k c(n, k) x^k = x (x+1) ... (x+n-1)`, which test a full row against a single polynomial identity.
- `stirling: sampled orthogonality for rows 1024, 2048, 4095, 4096 against m = 0..520` (sampled): for each such `n`, `sum_k (-1)^(n-k) c(n, k) S(k, m)` equals `[n = m]` for every `m` from 0 to 520, with the first-kind row from the implementation and the second-kind column `S(k, m)` generated in the test by the plain recurrence. The columns `m` above 520 are not checked this way (a limit of the extent, stated here).
- `stirling: failure cases, check order, nothing written`: null `out` or `scratch`, with `n = 0`, before TooLarge; `n` in {4097, 4294967295}; scratch short by one word, zero words, and `n = 0` with zero words; every pairwise order of the three checks; and a test that no failure writes to `out` or the scratch.
- `stirling: small known values`: hand checked rows and Bell numbers, `n = 0, 1, 2`.

Products of 32-bit values in the tests are widened to `u64` before the multiplication.

## Mutation testing

90 source mutants of `core/stirling/stirling.cpp` (script outside the repo; three further candidates were dropped before the run: two textual no-ops and one that did not compile), built with ASan + UBSan and run against the final tests (not edited after the run; the md5 of the test file is the same before and after). Each mutant ran the full test file once under a 20 s limit. A mutant is called killed by the first failed assertion; one that ended in a sanitizer abort or a crash without a failed assertion is listed separately as crash-only. A timeout would be listed separately and would not count as a kill (none occurred).

- 77 killed by a failed assertion.
- 9 crash-only: a_null, a_big_p1, a_big_rm, c_null, c_big_p1, c_big_rm, b_null1, b_null2, b_null3 (a missing null check, or a missing or loosened size check, which dereferences null or overruns the small buffer of the failure test; the sanitizer, not an assertion, stops them).
- 0 timeouts.
- 4 not killed (survivors), all four equivalent in output:
  - c_initk_s (`out[1]` is not zeroed at the start of `stirling1_row`): the first update (`m = 1`) multiplies the old `out[1]` by `m - 1 = 0`, so the content cannot matter. The same mutant on `stirling2_row` (multiplier `k = 1`) is killed.
  - c_out0a (`out[0] = 0` instead of `(m-1) out[0]`): `out[0]` is 1 only at `m = 0`; at `m = 1` the product is `0 * 1 = 0` and stays 0. Same value at every step.
  - b_r1 (`s > kMod` for `s >= kMod`) and b_r4 (`s >= kMod - 1` for `s >= kMod`): they differ from the original only when `s` is exactly `kMod` (b_r1) or exactly `kMod - 1` (b_r4), i.e. when a triangle entry is congruent to 0 or to `p - 1`. `tools/stirling_ref.py check` counts these over every entry of the Aitken triangle for rows 1 to 4096 (every entry `bell_table` can ever form, since `n <= 4096`): there are none. So over the whole domain of the function the branch is never taken at the boundary, and the outputs are identical. This is an exhaustive finite argument for `kMaxN = 4096`, not a coincidence test; a larger `kMaxN` would need a new count.

## Not run

Rows and tables for `n` in 301 .. 4095 other than the sampled ones, orthogonality columns above `m = 520` for the large rows, timing or step-count assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
