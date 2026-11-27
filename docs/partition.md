# partition: partition counts modulo 998244353

Code: `core/partition/partition.cpp` (contract in `core/partition/partition.h`). Tests: `tests/partition/partition_tests.cpp`. Reference: `tools/partition_ref.py`. Evidence label: unit tests only.

## Rule

- `partition_all`: `out[v]` is the number of partitions of `v`, `v = 0 .. n`. `partition_distinct`: the number of partitions of `v` into pairwise different parts. `partition_at_most_k`: the number of partitions of `v` into at most `k` parts (equivalently, with every part at most `k`). All values are exact counts reduced modulo `kMod` = 998244353, in `0 .. 998244352`; `out[0]` is 1.
- `k = 0` gives `out[0] = 1` and 0 elsewhere; `k` above `n` behaves as `k = n`. `kMaxN` = 65536 is the largest `n` and the largest `k`.
- Check order: a null `out` (and for `partition_at_most_k` a null `scratch`, always, also for `n = 0`) is NullArgument; then `n > kMaxN` is TooLarge; for `partition_at_most_k` then `k > kMaxN` is TooLarge (also for `n = 0`); then, only for `partition_at_most_k`, `scratch_words < n + 1` is ScratchTooSmall (also for `n = 0`, which needs one word). Nothing is written on any failure, not to `out` and not to the scratch. Every function writes exactly `out[0 .. n]`.

Integer only: no floating point, no 128-bit types, no compiler builtins or intrinsics, no allocation, no recursion, no mutable statics, no arrays beyond the caller's tables.

## Method

Write `P(x)` for the generating function of `p`, and `E(x) = prod over i >= 1 of (1 - x^i) = sum over j in Z of (-1)^j x^(j(3j-1)/2)` (Euler's pentagonal number theorem). `g1(j) = j(3j-1)/2` and `g2(j) = j(3j+1)/2 = g1(j) + j` are the generalized pentagonal numbers.

- `partition_all`: `P E = 1` gives `p(v) = sum over j >= 1 of (-1)^(j+1) (p(v - g1(j)) + p(v - g2(j)))`, computed in place for `v = 1 .. n` with the two partial sums (odd and even `j`) kept in `u64` and reduced once per entry.
- `partition_distinct`: `Q(x) = prod (1 + x^i) = E(x^2) / E(x)`, so `Q E = E(x^2)` and `q(v) = e(v) + sum over j >= 1 of (-1)^(j+1) (q(v - g1(j)) + q(v - g2(j)))`, where `e(v)` is the coefficient of `x^v` in `E(x^2)`: 1 for `v = 0`, `(-1)^j` for `v = 2 g1(j)` or `v = 2 g2(j)`, else 0. Computed in place on `out`; no second table.
- `partition_at_most_k` (with `k` clamped to `n`):
  - `k * k <= n`: the part-by-part table in `out` (`out` set to `1, 0, 0, ...`, then for `i = 1 .. k` and `v = i .. n`: `out[v] += out[v - i]`, with a conditional subtraction).
  - `k * k > n`: `p_k(x) = prod over i <= k of 1/(1 - x^i) = P(x) prod over i > k of (1 - x^i)`. A product over `i > k` of `(1 - x^i)` is `sum over j >= 0 of (-1)^j` times the generating function of sets of `j` distinct integers above `k`. Such a set `a_1 < ... < a_j` is `a_i = k + i + c_i` with `0 <= c_1 <= ... <= c_j`, whose generating function is `x^(s_j) / ((1 - x)(1 - x^2) ... (1 - x^j))` with `s_j = j k + j(j+1)/2`. So `p_k(x) = sum over j >= 0 of (-1)^j x^(s_j) T_j(x)`, `T_j = P / ((1 - x) ... (1 - x^j))`, `T_0 = P`, `T_j = T_{j-1} / (1 - x^j)`. The scratch holds `T_j`: first `P` (as for `partition_all`), then for `j = 1, 2, ...` while `s_j <= n`, one in place prefix pass `T_j[v] = T_{j-1}[v] + T_j[v - j]` for `v = j .. n - s_j` (only that part is read later), then `out[v] +/-= T_j[v - s_j]` for `v = s_j .. n` (minus for odd `j`). `out` starts as a copy of `P`.

Scratch use: exactly `n + 1` words, `scratch[0 .. n]`. Every index is in `0 .. n`: `all_table` writes `t[0 .. n]` and reads `t[v - g]` with `g <= v`; the prefix pass reads and writes `scratch[v]` and `scratch[v - j]` with `j <= v <= n - s_j <= n`; the correction pass reads `scratch[v - s_j]` with `s_j <= v <= n` and, since `v - s_j <= n - s_j`, only entries the prefix pass produced. The check `scratch_words >= n + 1` therefore always suffices, and the direct branch does not touch the scratch. No other memory is used.

Step bounds (structural, not measured). In the pentagonal loops the `j` loop for entry `v` runs while `j(3j-1)/2 <= v`, so `j <= sqrt(2v/3) + 1` and the whole table takes at most about `2 * sqrt(2/3) * (2/3) * n^(3/2)`, roughly 18 million term additions at `n = 65536`: O(n sqrt n). `partition_all` and `partition_distinct` have this bound.
- Branch `k * k <= n`: `sum over i = 1 .. k of (n - i + 1) <= n k <= n sqrt n` (at most 16.8 million at `n = 65536`, `k = 256`).
- Branch `k * k > n`: the pentagonal table is about 18 million steps (O(n sqrt n)). Each pass `j` has `s_j >= j k`, so `j <= n / k < sqrt n` (since `k > sqrt n`), the number of passes is below `sqrt n`, and each pass is at most `2n` steps, so the passes total below `2 n sqrt n`. Together O(n sqrt n) for this branch as well.
Both branches compute the exact table for every `k`; the threshold only chooses the cheaper one.

Ranges: all additions are modular (a sum of two residues is below `2 kMod < 2^32` before the conditional subtraction); the `u64` partial sums hold at most 420 residues each, below `2^40`.

## Tests

Expected tables come from `tools/partition_ref.py` (Python standard library only; no NumPy). It uses: exact part-by-part tables in Python integers (no reduction) for the small extents; the same table modulo `p` by strided prefix sums (`itertools.accumulate` on each residue class), with a snapshot after every part size; Newton inversion of the Euler series with Kronecker packed big-integer products for the unrestricted table at `n = 65536`, then `Q = P E(x^2)`, each verified at full size by the product identities `P E = 1` and `Q E = E(x^2)`; and, where `n <= 2k + 2`, `p_k(v) = p(v) - sum over m = k+1 .. v of p(v - m)`, which counts the partitions with a part above `k` directly. These are not the recurrences of the implementation. `E` is checked against the defining product in exact integers up to degree 400, and the known coefficients (`p(0..10)`, `q(0..10)`, `p(100) = 190569292`, `q(100) = 444793`, `p(1000)`) are verified by the reference before the tests use them. A disagreement aborts. The goldens are FNV-1a digests over 64-bit words (one table entry per word, tables folded in the stated order) and explicit values. `python3 tools/partition_ref.py check` uses the standard library only and took about 9 s on the machine used (local; no test asserts time).

The small extents are also compared in the test with an in-test plain coin change table (a different shape from the pentagonal and alternating sum methods).

Every success run uses `out` with four guard words on each side and a sentinel fill, scratch of exactly `n + 1` words at an odd u32 offset with guard words and a garbage fill (guards checked after the call), every entry in `0 .. kMod - 1`. No test asserts scratch contents after a call.

Extents, exactly as named in the tests:
- `partition exhaustive extent every n 0 to 300 all and distinct full tables`: every `n` from 0 to 300 (301 calls each), full tables, against the in-test table and the digest of all tables.
- `partition exhaustive extent at most k every n 0 to 200 and every k 0 to 210`: all 42411 pairs (201 values of `n` times 211 values of `k`), full tables, against the in-test table and a digest. This includes every `n` through 96 with every `k` through 110, and `k` below, equal to and above `n`.
- `partition at most k around the branch threshold every n 1 to 200 k from isqrt minus 2 to isqrt plus 3`: for every `n` from 1 to 200, six values of `k` around `r = floor(sqrt n)` (`k * k` below, equal to for the 14 perfect squares `n` and above `n`), full tables against the in-test table.
- `partition sampled grid at most k 208 pairs n up to 10000 and k up to 65536` (sampled, not exhaustive): `n` in {127, 128, 255, 256, 257, 1000, 4095, 4096, 4097, 10000} and `k` in {0, 1, 2, 3, 15, 16, 17, 63, 64, 65, 100, 255, 256, 257, 300, 1000, 4096, 65535, 65536, `n - 1`, `n`, `n + 1`}, restricted to the pairs the reference can compute independently (`k <= 1000`, or `n <= 2k + 2`), 208 pairs, full tables, digest from the reference; closed forms for `k = 1` (all ones), `k = 2` (`floor(v/2) + 1`) and `k = 3` (`round((v+3)^2 / 12)`), `k = 0`, and `k >= n` equal to `partition_all`.
- `partition n 65536 all and distinct full tables digest explicit values and prefixes` (test tag `[partition-large]`): the two tables at `n = 65536`, digest and explicit values at 11 indices from the reference, and 16 sampled `n` whose tables must equal the prefix of the `n = 65536` table (the exhaustive version of this is the first test above, every `n` through 300).
- `partition n 65536 at most k for k 0 1 2 255 256 257 65535 65536 and 65537 fails`: the tables at `n = 65536` for `k` in {0, 1, 2, 255, 256, 257, 65535, 65536}, digests from the reference, closed forms for `k` 0, 1, 2, `k = 65536` equal to `partition_all`, `k = 65535` equal to `partition_all` except at `v = 65536` where one partition (all ones, 65536 parts) is excluded, and `k = 65537` is TooLarge with nothing written. `k = 256` is the branch edge (`k * k = n`).
- `partition collision case n 7667 k 2589`: a case found by search where `p(7667)` is congruent to `p(0) + ... + p(5077)`, so the first correction term meets `out[v]` with equality; the table is compared with an in-test part-by-part table and a reference digest. It exercises that equality in the correction pass, but it does not by itself kill a mutant (see below).
- Hand checked values and `n = 0` for all three functions, failures (null pointers alone, with `n = 0`, before TooLarge; `n` in {65537, 100000, 4294967295}; `k` in {65537, 4294967295} also with `n = 0`; scratch short by one word, zero words, `n = 0` with zero words and `n = 65536` with 65536 words; check order between them) and a test that no failure writes to `out` or the scratch.

Products of 32-bit values in the tests are widened to `u64` before the multiplication.

## Mutation testing

93 source mutants of `core/partition/partition.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (not edited after the run; the whole set was run again after the last edit, a comment line and the collision case). Each mutant ran the full test file once under a 20 s limit. A mutant is called killed by the first failed assertion; one that ended in a sanitizer abort or crash with no failed assertion is crash-only.

- 71 killed by a failed assertion.
- 9 crash-only: a_g1idx, p_anull, p_an2, p_dnull, p_dn2, k_null2, k_sb2, k_top2, k_p3.
- 0 timeouts.
- 13 not killed (survivors):
  - Branch choice: k_br1 (`k * k < n`), k_br2 (`k * k <= n + 1`), k_br3 (the product in 32 bits; for `k = 65536` it wraps to 0 and takes the direct branch for `n = k = 65536`, about 2.1 billion steps, which still finished inside the limit), k_br4 (`k <= n`, always the direct branch). Both branches compute the exact table for every `k`, so the output is identical; only the step count differs, and no test asserts time or step counts. Equivalent in output, not in the step bound.
  - Clamp: k_cl1 (`k >= n`, the same assignment when equal), k_cl2 (no clamp) and k_cl3 (`k = n + 1`). For `k > n` the first shift `k + 1` exceeds `n`, so the second branch returns `P`, which is the table for `k = n`; for the direct branch the output is exact for any `k`. Equivalent in output.
  - k_sc3: `scratch_words < n + 1u` in 32 bits; `n <= 65536` at that point, so it cannot wrap. Equivalent.
  - k_top1: the prefix pass computes one extra entry `T_j[top + 1]`; the later passes read at most `T_{j}[n - s_j]` and `T_{j+1}` needs fewer entries, so it is never read. Equivalent.
  - k_x1, k_p5, k_q8 (`>` for `>=` at the conditional subtraction): the two differ only when the sum is exactly `kMod` (a table entry `T_j[v]` congruent to 0, or `out[v] + tv = kMod`). No test hits such a value, and I could not construct one (it needs a coincidence of probability about `1/kMod` per entry). This is a real gap in the tests, not an equivalence.
  - k_q6 (`out[v] > tv` for `>=`): differs only when `out[v] == tv`, which gives the invalid value `kMod` instead of 0. The search found one such coincidence (n = 7667, k = 2589, the collision test), but there a later pass (`j = 2`) reduces the value again, so the final table is unchanged. A case where it is the last term was not found. A real gap, not an equivalence.

## Not run

Exhaustive tests beyond the stated extents, `n` in 301 .. 65535 for all and distinct beyond the sampled prefixes, timing or step-count assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
