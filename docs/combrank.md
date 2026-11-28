# combrank: ranking k-subsets of n elements as u64 masks

Code: `core/combrank/combrank.cpp` (contract in `core/combrank/combrank.h`). Tests: `tests/combrank/combrank_tests.cpp`. Reference: `tools/combrank_ref.py`. Evidence label: unit tests only.

## Rule

A k-subset c_1 < c_2 < ... < c_k of {0, ..., n - 1} is the mask with those bits set (n at most 64). Its rank is C(c_1, 1) + C(c_2, 2) + ... + C(c_k, k). The k-subsets of n elements get the ranks 0 to C(n, k) - 1, and the rank order is the numeric order of the masks.

- `combo_count(n, k)` gives C(n, k). `combo_rank(mask, n, k)` gives the rank, `combo_unrank(n, k, rank)` the mask, `combo_next(mask, n, k)` the mask with the next rank.
- Check order, every function: a null `out` is NullArgument; then n above 64 or k above n is BadArgument; then (rank, next) a mask with a bit at position n or above, or with a popcount other than k, is BadMask; then (unrank) a rank of C(n, k) or more is RankTooLarge; then (next) the last mask, and every call with k = 0, is NoNext. Nothing is written to `*out` on a failure. n = 0 and k = 0 are valid (the only 0-subset is the mask 0).
- Pure integer. No tables, arrays, allocation, recursion, mutable statics, 128-bit types, builtins or intrinsics in the implementation.

## Method

Binomials are never stored. Each one is built or updated from its neighbour by an exact division followed by a multiplication, after cancelling a gcd, so no product is larger than the binomial it produces.

- `binom(n, k)`: with k replaced by min(k, n - k), c_0 = 1 and c_i = C(n - k + i, i) = c_{i-1} * (n - k + i) / i. With g = gcd(c_{i-1}, i), c_i = (c_{i-1} / g) * ((n - k + i) / (i / g)).
- `down(B, c, j)` takes B = C(c, j) to C(c - 1, j) = (B / g) * ((c - j) / (c / g)), g = gcd(B, c) (c at least 1).
- `diag(B, c, j)` takes B = C(c, j) to C(c - 1, j - 1) = (B / g) * (j / (c / g)), the same g (c at least 1, j at least 1).
- `combo_rank` walks the positions c = n - 1 down to 0 with B = C(c, j) for the number j of set bits still to place, starting at j = k and B = C(n - 1, k): a set bit adds B to the sum and moves to C(c - 1, j - 1) with `diag` (the walk ends at the bit with j = 1); an unset bit moves to C(c - 1, j) with `down`.
- `combo_unrank` is the same walk: for each digit j from k down to 1 it steps c down with `down` while B is above the rest, sets bit c, subtracts B from the rest and moves to C(c - 1, j - 1) with `diag`.
- `combo_next`: with k at least 1, the last mask is the top k bits of n bits, `(all ones >> (64 - k)) << (n - k)`; it is NoNext, decided before any addition. Otherwise Gosper: low = mask & -mask, r = mask + low, next = r | (((mask ^ r) >> 2) / low).
- Cost (header): a call does O(n + k) binomial updates plus one binomial built from scratch in O(k) updates, each update one gcd (Euclid, at most about 90 trips) and one exact division and multiplication. Nothing loops over all masks or all ranks.

## Proofs

**The rank is a bijection onto 0 .. C(n, k) - 1.** By induction on k. For k = 0 the only subset is the empty one, rank 0 = C(n, 0) - 1. For k at least 1, take the subsets whose largest element is c (c from k - 1 to n - 1). The rest is a (k - 1)-subset of {0, ..., c - 1}, whose rank is by induction in 0 .. C(c, k - 1) - 1, so the rank of the whole subset is C(c, k) plus that, which lies in the interval [C(c, k), C(c + 1, k)) (Pascal: C(c, k) + C(c, k - 1) = C(c + 1, k)) and covers it exactly once. For c = k - 1 the interval starts at C(k - 1, k) = 0. The intervals for c = k - 1, ..., n - 1 follow each other without a gap from 0 to C(n, k). So the ranks are exactly 0 .. C(n, k) - 1, each once.

**The order is the numeric order of the masks.** A subset with a larger largest element has a larger mask and, by the intervals above, a larger rank. With the same largest element the masks compare as the rest does, and so do the ranks, by induction. So rank order and numeric order agree (this is colex order), and the next rank is the next larger mask with the same popcount inside n bits.

**Unrank.** For the rank r the largest element is the c with C(c, k) <= r < C(c + 1, k), that is the largest c with C(c, k) <= r (by the intervals); then r - C(c, k) is the rank of the rest with k - 1, and so on. The walk finds each c by stepping down from the previous c - 1 (the next digit is below the last and C(c', j) only grows with c'), starting at c = n - 1 with C(n - 1, k) (and C(n - 1, k) = 0 when k = n, which is at most r, so the first digit is n - 1 as it must be).

**The updates are exact and never exceed the binomial they produce.** Since C(c - 1, j) * c = C(c, j) * (c - j), with g = gcd(B, c) the integer B / g and c / g are coprime and (c / g) divides (B / g) * (c - j), so it divides c - j; the quotient is exact. The same holds for C(c - 1, j - 1) * c = C(c, j) * j. In `binom`, c_{i-1} * (n - k + i) = i * c_i, so with g = gcd(c_{i-1}, i), (i / g) divides (n - k + i). In each case the product (B / g) * quotient is the new binomial itself, and no other product is formed.

**The largest binomial.** C(n, k) <= C(n + 1, k) and, for fixed n, C(n, k) is largest at k = n / 2 (it rises while k < n / 2 and falls after), so the maximum over n <= 64 is C(64, 32) = 1832624140942590534 < 2^61 (checked by Python bigint and by the tests for every (n, k)). Every value that `binom`, `down` and `diag` produce is some C(c, j) with c <= 64, and in `binom` with k replaced by min(k, n - k) every c_i = C(n - k + i, i) <= C(n, k) (it is a binomial of a smaller or equal row with the same or a smaller index, bounded by C(n, k)). So every product is at most C(64, 32). The sum in `combo_rank` is a rank, below C(n, k). The measured maximum is in the Tests section.

**`combo_next` does not wrap.** `mask + low` wraps only if the lowest run of ones in mask extends to bit 63: then the sum carries out of bit 63. With popcount k such a mask is the single block of k ones ending at bit 63, which for n = 64 is the last mask (top k bits). The last mask is detected first and returns NoNext, so the addition is only done for masks that are not last. For n below 64 the mask lies in n bits and the sum is at most 2^n. The ones to put back are (mask ^ r) >> 2 divided by low: mask ^ r has the run length plus one ones starting at low, so the quotient has run length minus one ones at the bottom, below r's cleared run, so the `or` is also a sum or an exclusive or (the three are equal). The shift `<< (n - k)` is by at most 63 (k at least 1) and `>> (64 - k)` by at most 63.

## Tests

Expected values come from the header's examples, from a test-only Pascal triangle built by addition (a different method from the library's gcd updates: the oracle ranks by summing triangle entries and unranks by scanning it), from the definition (the class of a popcount in numeric order is ranked 0, 1, 2, ...), and from FNV-1a 64-bit digests made by `tools/combrank_ref.py` (Python standard library only; bigint `math.comb`; unrank by digit search; next is unrank of rank + 1). Every call writes its result between guard words; a failure must leave the cell as it was.

Extents and local runtimes (gcc 11.4.0, `-O1`, ASan + UBSan, one core, base fcf15d6; the whole binary has 21 cases and 131 assertions and runs about 3.4 s; the longest case is 0.97 s; none asserts time; declaration order):
- `header examples` (every example of the header), 0.000 s.
- `count: every (n, k) pair and larger n`: all 4225 pairs n, k = 0..64 (k above n is BadArgument) and eight pairs with n of 65 up to 2^32 - 1, against the Pascal triangle and the digest, 0.000 s. `count: Pascal identity, symmetry and the bound`: C(n, k) = C(n - 1, k - 1) + C(n - 1, k), C(n, k) = C(n, n - k), C(n, 0) = C(n, n) = 1 for every n up to 64, maximum C(64, 32) < 2^61, 0.001 s.
- `every mask of n = 0..17` (0.20 s), `n = 18` (0.22 s), `n = 19` (0.47 s), `n = 20` (0.97 s): every mask of n bits in numeric order, 2^21 - 1 masks in all. For each: the popcount k, the index i of the mask in its class so far, `combo_rank` equals i, `combo_unrank(n, k, i)` equals the mask, `combo_next(previous mask of the class)` equals the mask (the order property and the successor), digest of (n, mask, rank). At the end of each n: the class sizes equal C(n, k), `combo_next` of the last mask of each class is NoNext, and ranks C(n, k) and 2^64 - 1 are RankTooLarge.
- `bad popcount`: for every n up to 16, every k and every mask of n bits whose popcount is not k: BadMask from rank and next. 0.15 s.
- `bad masks`: at every (n, k) with n up to 64: the mask 0, all ones, a high bit at n, a bit moved, shifted and removed masks, against the oracle verdicts; for n below 64 a mask with the right popcount but one bit moved to exactly position n. 0.004 s.
- `check order matrix`: n in {0, 1, 10, 64, 65, 255, 2^32 - 1} times k in {0, 1, 4, 10, 11, 64, 65, 2^32 - 1} times nine masks times six ranks, with a null `out` for every function (always NullArgument) and without, against the oracle codes and values, and the order spelled out (n bad before mask, mask before rank, NoNext last, k = 0). 0.000 s.
- `edges at every (n, k)`: n = 0..64, k = 0..n (2145 pairs; 17031 rank checks): the first and the last mask (rank and next), the ranks 0, 1, 2, C - 2, C - 1, C, C + 1 and 2^64 - 1 (unrank, then rank back), and next chains of up to 130 steps from the first mask and from the mask of rank C - 65 until NoNext; digest. 0.28 s.
- `sampled triples 0` to `3`: 4 x 50000 = 200000 triples (n, k, rank) from the splitmix64 stream (position 2^40 + 100000 s + 2i; the rank is a raw word one time in four, which is mostly RankTooLarge, else reduced modulo C); unrank against the oracle and the digest, rank back, next against the oracle. 0.10 s each.
- `sampled masks 0` to `3`: 4 x 50000 = 200000 random valid masks (n from 1 to 64; the word is cut to n bits and its lowest set bits cleared or lowest clear bits set until the popcount is k); rank and next against the oracle, unrank back, digest. 0.12 s each.
- `n = 64: every subset for k = 1, 2, 3, 61, 62, 63, 64`: 87489 subsets in order (generated by the oracle's unrank; the Python reference sorts all subsets): rank, unrank, next against the index, strictly increasing masks, NoNext at the end, digest. This includes every k = 2 and k = 62 subset and the whole space in which the lowest set bit can reach bit 63. 0.19 s.
- `the update recipe`: a test-only copy of `binom`, `down` and `diag` with checked arithmetic, run for every (n, k) with n up to 64 (the binomial itself and the rank walk along the masks of rank 0, C - 1, C / 2 and C / 3): every division is exact, no product overflows, and the largest value is exactly C(64, 32). 0.014 s. This is a copy of the recipe, not the library.

The library itself was also measured (a local build outside the repository: the one multiplication `mul` of `combrank.cpp` replaced by a version that counts calls, checks for overflow and records the largest product, the whole test binary run on it): 155774160 products, no overflow, largest product 1832624140942590534 = C(64, 32). This was a measurement, not part of the committed tests.

The Python commands, all run: `check` (identities, the header examples, an enumeration of every subset for n up to 14 compared with the rank, unrank and next; 0.5 s), `count`, `exh LO HI`, `edge`, `samp K`, `masks K`, `wrap`, and `emit` (all digests and counts as C++, about 55 s).

## Mutation testing

229 source mutants of `core/combrank/combrank.cpp` (script outside the repository), each built with ASan + UBSan against the final tests (md5 f4e5667a4281591af44ec7c81307ef4b, not edited after the run), abort after the first failure, declaration order (`--order decl`), the whole suite for every mutant. Mutated: the multiplication, every line of the gcd, `binom` (the range test, the min(k, n - k), the start, the loop bounds, the factor, the gcd and the quotient forms, the integer widths), `down` and `diag` (the gcd, the factors), the popcount and `mask_ok` (every test and the high-bit test), and in each of the four functions the null check (inverted, removed, return code, order), the n and k checks, the mask check, the rank check, the start values, the loop tests, the digit steps, the bit written, the rank update, the stop test, the output, the return codes and the written-on-failure rule; in `combo_next` the k = 0 case, the last mask (the shifts and the comparison), the lowest bit, the sum, the or, the shifts and the division. Results:

- 160 killed by a failed assertion and 38 killed by an abnormal end (a sanitizer abort or a crash: a shift of 64 or more, a division by zero, an overread).
- 5 step-limited, not counted as kills, non-termination not proven by the probe (`gcd_div`, `pc_kern`, `rk_noc`, `un_kge0`, `un_down_nop`). Probe: each was rebuilt with a counter on every loop that aborts at 200000 trips; all five aborted at the limit (the loops of the gcd, the popcount, the rank walk, and twice the inner descent of unrank). The probe shows that the limit was hit, it does not prove a hang, and it looks for no repeated state. By hand, each has a reason it cannot stop (arguments, not measurements): `pc_kern` (`m &= m`) leaves m unchanged and nonzero; `gcd_div` (`a / b` for the remainder): from the first call gcd(1, 1) the pair (a, b) = (1, 1) maps to itself; `rk_noc` (no `--c` in the rank walk): with the top bit unset the same position is tested forever, because c and j do not change; `un_down_nop` (`b = b`): the loop test `b > rank` never changes, since nothing in the loop changes b or rank; `un_kge0` (`k >= 0u` as the guard): for k = 0 and rank 0 the walk starts at j = 0 with B = C(c, 0) = 1, and `down` keeps B at 1, so `b > rank` stays true.
- 16 survivors, each equivalent:
  - `dn_swap`, `dg_swap`, `bn_swap` (the two factors of the product swapped) and `bn_g_swap`, `dn_gcd_swap` (the gcd arguments swapped): multiplication and gcd are symmetric.
  - `bn_min_ge` (`k >= n - k` instead of `k > n - k`): at k = n - k the assignment does not change k.
  - `bn_m32` (the factor `n - k + i` held as u32): it is at most 64 + 32.
  - `pc_u8` (the popcount held as u8) and `rk_c32` (the position c held as u8): at most 64 and 63.
  - `rk_jle` and `un_jle` (`j <= 1u` for `j == 1u`): j is at least 1 where it is tested.
  - `un_bitx` (`mask ^= bit` for `|=`): each position is set at most once, c strictly decreases.
  - `un_c` (the unrank walk starts at c = n instead of n - 1): B = C(n, k) is above the rank (the rank is below C(n, k)), so the loop steps down once with `down` to C(n - 1, k), which is the real start, and bit n is never set.
  - `nx_ge` (`mask >= last`): no mask of popcount k inside n bits is above the top k bits.
  - `nx_or_xor` and `nx_or_plus` (`r ^ ones` and `r + ones` for `r | ones`): the ones sit in the bottom run L - 1 bits, where r has only zeros (the lowest L + p bits of r are zero below its new top bit), so the three operations agree.
- 10 cost mutants (the values are right, only the time grows; kept apart from the kills and the equivalent survivors; no test can detect them because the repository forbids timing assertions). Observed, whole suite, one core (the source runs in 3.4 s):
  - `cm_bn_nomin` (no min(k, n - k): `if (k > n)` instead) 3.8 s, `bn_nomin` (the minimum removed) 3.8 s, `bn_min_lt` (the maximum of k and n - k instead of the minimum) 4.1 s;
  - `cm_count_twice` (`combo_count` builds the binomial 200 times) 3.6 s;
  - `cm_rank_rebuild` (the walk rebuilds C(c - 1, j) from scratch after each `down`) 4.5 s, `cm_rank_diag_rebuild` (the same after each `diag`) 4.5 s, `cm_un_rebuild` (the same in unrank) 4.5 s;
  - `cm_gcd_rep` (the gcd run 20 more times) 16.0 s, `cm_pc_rep` (the popcount run 100 more times in the mask check) 7.4 s, `cm_nx_last` (the binomial built 100 more times in `combo_next`) 34.3 s.
  A first try of the last three with 300 and 20000 repeats ran past a 12 to 40 s limit and was reduced to the repeat counts above so that the suite finishes and is measured; the first results are not counted as kills.

The first mutant pass ran eight builds at a time on one core with a 12 s limit, which made slow-but-terminating mutants time out; every one of those was rerun three at a time with a 40 s limit (the five step-limited mutants above did not finish and were probed).

## Not run

Other builds than the local gcc 11.4 ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, and a mutant run on shuffled case order. For n above 20 only the edge masks and ranks, the 200000 sampled triples, the 200000 sampled masks and the 87489 subsets of n = 64 for seven k are compared (not every mask of every n up to 64, which is not possible). The base for local evidence is fcf15d6. Nothing here is a CI result.
