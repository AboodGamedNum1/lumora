# necklace: necklace, bracelet and Lyndon word counts modulo 998244353

Code: `core/necklace/necklace.cpp` (contract in `core/necklace/necklace.h`). Tests: `tests/necklace/necklace_tests.cpp`. Reference: `tools/necklace_ref.py`. Evidence label: unit tests only.

## Rule

- `necklace_count(n, k, out)`: the number of necklaces of length `n` over `k` letters (rotations identified). `bracelet_count`: bracelets (rotations and reflections identified). `lyndon_count`: Lyndon words (aperiodic necklaces). Each writes the exact count reduced modulo `kMod` = 998244353, in `0 .. 998244352`, to `*out`.
- `kMaxN` = 2^20 = 1048576 is the largest `n`. `k` is any `u32` and is used as `k mod kMod`: `k` and `k + kMod` give the same answer, `k = kMod` and `k = 2 kMod` behave as 0, `k = kMod + 1` as 1.
- `n = 0` gives 1, 1, 0 (necklaces, bracelets, Lyndon words) for every `k`, without computing any power or inverse. For `n > 0`: `k = 0` gives 0 for all three; `k = 1` gives necklaces 1, bracelets 1, Lyndon words 1 at `n = 1` and 0 above. Bracelets at `n = 1` are `k` and at `n = 2` are `k (k + 1) / 2`.
- Check order: a null `out` is NullArgument; then `n > kMaxN` is TooLarge; `k` is never checked. Nothing is written on any failure.

Integer only: no floating point, no 128-bit types, no compiler builtins or intrinsics, no allocation, no recursion, no mutable statics, no arrays and no tables.

## Method

For `n >= 1` (`kr = k mod kMod`):
- Divisors of `n` by trial division, `i = 1 .. floor(sqrt(n))` (at most 1024 values of `i`); each `i` that divides `n` gives the two divisors `i` and `n / i`, the square root counted once.
- Necklaces: `N = (1 / n) sum over d | n of phi(d) kr^(n / d)`. Lyndon words: `L = (1 / n) sum over d | n of mu(d) kr^(n / d)`. `phi(d)` and `mu(d)` are computed for each divisor by trial division of `d` in the call (no table); `kr^e` by square and multiply in `u64`. `1 / n` is `n^(kMod - 2) mod kMod` (`n < kMod`, so `n` is invertible).
- Bracelets by Burnside over the dihedral group of order `2n`: `(1 / (2n)) (n N + reflections)`, that is `N / 2 + R` with `R = kr^((n+1)/2) / 2` for odd `n` (the `n` reflections each fix `k^((n+1)/2)` strings) and `R = (kr^(n/2+1) + kr^(n/2)) / 4` for even `n` (`n/2` reflections fix `k^(n/2+1)` strings and `n/2` fix `k^(n/2)`). The constants are `1/2 = 499122177` (`2 * 499122177 = kMod + 1`) and `1/4 = 748683265` (`4 * 748683265 = 3 kMod + 1`); the tests and the reference check them through the exact values.

Step count (structural, not measured): at most 1024 trial divisions for the divisors, then for each divisor `d` (at most 240 for `n <= 2^20`) a totient or Moebius computation of at most `sqrt(d)` steps and one fast power of at most 20 squarings. For `n = 720720` that is below 250 thousand steps; the largest case `n = 2^20` has 21 divisors. This is within the header's bound.

Ranges: every product is of two residues below `2^30` (below `2^60`), or `phi(d) < 2^20` times a residue (below `2^50`); one `%` per product. `(acc + kMod - t)` stays below `2^31`.

## Tests

Expected values come from `tools/necklace_ref.py` (Python standard library only; no NumPy; Python 3.8 or newer for `pow(x, -1, p)`; `python3 tools/necklace_ref.py check` took about 35 s on the machine used, local, and no test asserts time). Three sources of different shape, compared wherever they overlap:
- brute force: enumerate every string over `k` letters of length `n`; necklace = equals the smallest of its rotations, bracelet = equals the smallest over rotations and rotations of the reversal, Lyndon word = strictly smaller than every other rotation; `n = 0` has one empty string (a necklace and a bracelet, not a Lyndon word), `k = 0` with `n > 0` has no string;
- exact big-integer Burnside with every division asserted exact;
- a modular source for large `n` and large `k`: necklaces as `sum over j < n of k^gcd(j, n)`, divided by `n`; Lyndon words from the necklace counts of all divisors by `sum over d | n of L(d) = N(n)`; bracelets by the dihedral average with an explicit `1 / (2n)`.
Known values (`N(6, 2) = 14`, `B(6, 2) = 13`, `L(6, 2) = 9`, `(4, 3) -> 24, 21, 18`, `(5, 2) -> 8, 8, 6`) are asserted by the reference. The goldens are FNV-1a digests over 32-bit words (triples necklace, bracelet, Lyndon in the stated loop order) and explicit values. Large `n` with `k` above 16 is checked by the modular source only (exact integers of 32 million bits are too slow); for `k <= 16` the reference asserts that the exact and the modular source agree at those same `n`, and for every `n <= 2000` and every `k` in the list it asserts that they agree on a large subset (all `n <= 120`, all large `k`, and every seventh `n`).

Extents, exactly as named in the tests:
- `necklace: enumeration extent every (n, k) with n 0 to 24, k 0 to 1100 and k^n up to 30000, 2484 pairs` (exhaustive over that set, against brute-force counts as a digest), plus seven named heavier pairs against explicit brute-force counts: (10, 3), (12, 3), (16, 2), (18, 2), (8, 4), (7, 5), (6, 8).
- `necklace: exact extent every n 0 to 2000 times 50 values of k against exact big-integer digest` (exhaustive): `k` in 0 .. 40, 255, 256, 998244351, 998244352, 998244353, 998244354, 1996488706, 2147483648, 4294967295; all three functions.
- `necklace: gcd form and Gauss identities for every n 1 to 2000 times 50 values of k` (exhaustive): the necklace count equals the in-test gcd form; `sum over d | n of L(d) = N(n)` and `sum over d | n of d L(d) = k^n`, using the implementation's own Lyndon values; and the sum identity again on the list {0, 1, 2, 998244352, 998244353, 998244354, 1996488706, 4294967295} for every `n` 1 to 200.
- `necklace: bracelets equal the dihedral average from explicit permutations for every n 1 to 400 times 50 values of k` (exhaustive): the group is built in the test: rotations fix `k^gcd(j, n)`, and each of the `n` maps `i -> (s - i) mod n` is walked as a permutation and its cycles counted; the average over the `2n` elements is compared with `bracelet_count`.
- `necklace: sampled large n 1024 to 2^20 (primes, highly composite) times 20 values of k against reference digests` (sampled): `n` in {1024, 4096, 65536, 131072, 262144, 524288, 524289, 786432, 1000000, 720720, 997920, 65537, 7919, 1048573, 1048575, 1048576} (powers of two, the largest prime below 2^20 = 1048573, 65537 and 7919, the highly composite 720720 and 997920 with 240 divisors each, and odd neighbours of 2^20), `k` in {0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 16, 255, 256, 998244352, 998244353, 998244354, 1996488706, 2147483648, 4294967295}; one digest per `n` and an overall digest.
- `necklace: sampled gcd form and Gauss identities at the large n` (sampled): the gcd form and both Gauss identities at those `n`, with `k^n` for `n` up to 2^20.
- `necklace: failure cases, check order, nothing written`: null `out` with `n` in {0, 5, 2^20, 2^20 + 1, 2^32 - 1}; TooLarge for `n` in {2^20 + 1, 2^20 + 2, 2^31, 2^32 - 1} with every `k` of the 50 values and no write; success writes one word with guard words beside it.
- `necklace: edge cases n = 0, k = 0, k = 1, k as a residue, bracelet n = 1 and n = 2`: `n = 0` for every `k`; `k = 0`, `k = 1`, `k = kMod, 2 kMod, kMod + 1` against `k = 0, 0, 1` for a list of `n`; bracelets at `n = 1` (`k`) and `n = 2` (`k (k + 1) / 2`), necklaces at `n = 2`, Lyndon words at `n = 2` (`k (k - 1) / 2`), and seven textbook values.

Products of 32-bit values in the tests are widened to `u64` before the multiplication.

## Mutation testing

92 source mutants of `core/necklace/necklace.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (not edited after the run; md5 of the test file 5cb375ec4901b8a2b992b7da47dd2570 before and after). Each mutant ran the full test file once under a 20 s limit. A mutant is called killed when a failed assertion is printed; one that ends in a sanitizer abort or crash without a printed failed assertion is listed as crash-only. Timeouts would be listed separately and would not count as kills (none occurred).

- 84 killed by a failed assertion.
- 4 crash-only: n_null, n_nullinv, b_null, l_null (the null check removed or inverted, which stores through a null pointer).
- 0 timeouts.
- 4 not killed (survivors), all equivalent in output:
  - b_h2 (`h = (n + 1) / 2` for `n / 2` in the even branch): the branch is taken only for even `n`, where `(n + 1) / 2` and `n / 2` are the same integer.
  - r_k (`k` not reduced in `reduce_k`) and p_b (`pow_mod` does not reduce its base): either one alone is covered by the other, because `pow_mod` reduces its base and the callers only pass `kr` (below `2^32`) or `n` (below `kMod`); they are two lines of the same defence, and removing both is not tested.
  - s_p1 (`i * i <= n + 1` in the divisor loop): the extra values of `i` have `i * i = n + 1` and `i` divides `n` only if `i` divides 1, i.e. `n = 0`, which never enters the loop. Equivalent.

## Not run

Rows of `n` beyond the extents (every `n` in 2001 .. 2^20 other than the sampled values), `k` above 16 at the large `n` against exact big integers (the modular source only), timing or step-count assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
