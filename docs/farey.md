# farey: neighbours in the Farey sequence F_n

Code: `core/farey/farey.cpp` (contract in `core/farey/farey.h`). Tests: `tests/farey/farey_tests.cpp`. Reference: `tools/farey_ref.py`. Evidence label: unit tests only.

## Rule

- `farey_next(n, a, b, c, d)` and `farey_prev(n, a, b, c, d)` give the term after and the term before a / b in F_n, the reduced fractions in [0, 1] with denominator at most n, in increasing order. n is any u32 above 0 (up to 2^32 - 1); the cost is O(log n) and nothing is walked or searched.
- Check order, both functions: a null `c` or `d` is NullArgument; then n = 0 is BadOrder; then a / b must be a term of F_n (b = 0, b > n, a > b or gcd(a, b) > 1 is BadFraction); then NoNeighbour (1 / 1 has no next, 0 / 1 has no previous). Nothing is written to `*c` or `*d` on a failure. `c` and `d` must be different objects (aliasing is not tested).
- Pure integer. No heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics in the implementation.

## Method

- One extended Euclid on (b, a) (iterative, i64 coefficients): it gives g = gcd(a, b) and the inverse t of a modulo b. g above 1 is BadFraction. Then the end cases (a = b for next, a = 0 for prev) are NoNeighbour.
- next: d0 = (-t) mod b; prev: d0 = t mod b. Then d = d0 + ((n - d0) / b) * b, the largest d <= n with d = d0 modulo b (d0 < b <= n). The neighbour numerator is c = (1 + a d) / b for next and c = (a d - 1) / b for prev.
- Every product is a * d with a <= b <= n and d <= n, both below 2^32, so a d + 1 is below 2^64 (all in u64). 0 / 1 and 1 / 1 need no special case: for 0 / 1 (t = 0, b = 1) next gives d = n, c = 1; for 1 / 1 prev gives d = n, c = n - 1.

## Proofs

**Neighbours.** Let a / b be a term of F_n. Suppose c / d has b c - a d = 1 and n - b < d <= n (it exists by the next proof). It is a term of F_n: gcd(c, d) divides b c - a d = 1, and d <= n. No term x / y of F_n lies strictly between a / b and c / d: otherwise k = b x - a y >= 1 and m = c y - d x >= 1, and the identity y = k d + m b (expand the right side and use b c - a d = 1) gives y >= d + b > n, a contradiction. So c / d is the next term. For prev the same argument holds with the determinant a d - b c = 1.

**The denominator.** b c - a d = 1 gives a d = -1 modulo b, so d = -t modulo b (t the inverse of a), which fixes d modulo b. The window n - b < d <= n holds exactly one such d, the largest one at most n. c follows from the determinant, and 0 <= c <= d because a / b < c / d <= 1 (next) or 0 <= c / d < a / b (prev). Uniqueness: the three conditions determinant 1, 1 <= d <= n and d + b > n pick one fraction, so the library result is the neighbour (this is the property the tests check).

**Extended Euclid.** With r0 = b, r1 = a, t0 = 0, t1 = 1 the loop keeps r_j = t_j a modulo b. At the end r0 = gcd and t0 a = gcd modulo b. The coefficients alternate in sign and |t_j| <= b / r_(j-1) (standard), so every coefficient except the last computed one is at most b / 2 in magnitude, and the last computed one (never used) is at most b. The loop runs at most about 46 rounds for b below 2^32 (Fibonacci bound; measured below).

**Reduction of t.** t0 may be negative. The code reduces it into [0, b) before use. The arithmetic after it is done modulo 2^64 in u64, so the result does not depend on the reduction (see the equivalent mutants below).

## Tests

Expected values come from the header's examples, from test-only oracles of a different method than the library (the neighbour as the exact minimum or maximum over every denominator d in 1..n, for n up to 120; the Haros recurrence, which uses no gcd, for the whole sequences; the complete property test for the sampled and edge cases with a binary gcd) and from FNV-1a 64-bit digests made by `tools/farey_ref.py` (Python standard library only: the brute-force sorted Fraction sequence, the scan over every denominator, the window property with `pow(a, -1, b)`, and the Haros recurrence). Every case checks both functions, the code, guard words around both cells and no write on failure.

Extents and local runtimes (gcc 11.4.0, `-O1`, ASan + UBSan, one core; built on base 545bbc3; whole binary 12 test cases, 39 assertions, 1.56 s; each case is far under the 30 s limit). Cases are declared cheap first:

- `farey: header examples`, 0.000 s: the seven header examples for both functions plus n = 1.
- `farey: check order and no write on failure`, 0.000 s: six orders (0, 1, 2, 5, 100, 2^32 - 1) with ten fractions (including 0 / 0, 3 / 2, 2 / 4, 4 / 6, 2^32 - 1 over 2^32 - 1) with all four null patterns for both functions.
- `farey: F3 edge orders and fractions`, 0.001 s: 26 orders (1 to 8, 10, 100, 255 to 257, 1000, 65535 to 65537, 2^20, 2^24 - 1, 2^31 - 1 to 2^31 + 1, 2^32 - 5, 2^32 - 3, 2^32 - 2, 2^32 - 1), each with 0 / 1, 1 / 1, 1 / 2, 1 / 3, 2 / 3, 1 / n, 1 / (n - 1), (n - 1) / n, the Fibonacci convergents and their complements, and an invalid list (b = n + 1, a = b + 1, a = b = 2^32 - 1, 0 / b above 1, 2a / 2b, b = n, and others). 2047 cases, digest.
- `farey: F4b sampled invalid fractions`, 0.003 s: 20000 sampled invalid or boundary fractions (b = 0, b = n + 1 to n + 5, a = b + 1 to b + 5, a = b = 2^32 - 1, not reduced, 0 / b, b = n) with orders of 1 to 32 bits, digest.
- `farey: F1 every pair against the scan`, 0.225 s: every n in 1..120 with every (a, b) in [0, n + 3]^2 (643220 cases) for both functions, against the scan oracle and the digest; n = 0 with 25 pairs gives BadOrder.
- `farey: F4 sampled terms 0` to `3`, 0.047 s each: 4 x 50000 valid terms (n of 1 to 32 bits with a bias to 29 to 32 bits; b = n, near n, small, or random; a made coprime), checked by the property test and the digest.
- `farey: F2 walks n = 1..400`, 0.584 s: every n from 1 to 400, 6533695 terms; `n = 500, 777, 1000, 1500`, 0.123 s, 1248246 terms; `n = 2000, 3000`, 0.414 s, 3952778 terms. At each step the library next of the predecessor and prev of the term must equal the Haros recurrence term; the term count must equal 1 + the sum of phi (by a sieve); the digest of the sequence; 1 / 1 has no next and 0 / 1 has no previous.

Measurement of the library (a local build outside the repository: counters added to the Euclid loop and to the product, whole test binary, 24850886 calls): at most 45 rounds; largest quotient 4294967295; largest coefficient |t| at the loop head 2136161669 (below 2^31); largest |q t1| 4294967295 and largest |t2| 4294967295 (the last computed coefficient, never used; both below 2^63); largest a d + 1 = 18446744056529682437 (below 2^64); no overflow seen. These are measurements over the tests, the bounds above are the arguments.

The Python commands, all run: `check` (brute sequences against the scan and the window formula for n up to 40, 400 sampled cases up to n = 3000, the header examples and term counts; 2 s), `f1`, `f2 A|B|C`, `f3`, `f4 K`, `f4b`, and `emit_part N` (all digests and counts; 6 to 12 s each, three at a time).

## Mutation testing

116 source mutants of `core/farey/farey.cpp` (script outside the repository), each built with ASan + UBSan against the final tests (md5 `86c7ba0e98ae469f39da3fc4e5b5be7c`, not edited after the run), abort after the first failure, declaration order, 30 s limit. The first pass ran three builds at a time on one core with a 30 s limit. The two step-limited mutants and the 11 survivors were rerun alone (15 s limit) with the same result; the loop probe ran alone; the cost mutants were timed alone.

- 98 killed: 86 by a failed assertion and 12 by an abnormal end (`e_loop_r0`, `e_q_mul`, `e_q_inv`, `e_q_p1`, `e_r2_add`, `n_nullc`, `n_nullc2`, `n_nulld2`, `n_nonull`, `n_b0`, `n_and`, `n_d0_b1`: a sanitizer abort or a crash).
- 2 step-limited, not counted as kills: `e_adv_r` and `e_adv_r2` (`r1` is not advanced) did not finish in 30 s (three at a time) or in 15 s (alone). A probe build with a counter on every loop passed 200000 trips in one entry in both. Argument (not a measurement): `r1` is unchanged in the loop and the loop is entered only with `r1` not 0, so the loop does not end.
- 11 survivors, each equivalent:
  - `e_t2_u32` (`q` cast to u32): q is at most b < 2^32. `e_t2_i32` (the new coefficient cast to i32): every coefficient except the last computed one is at most b / 2 < 2^31, and the last one is never read.
  - `e_m_nofix`, `e_m_le0`, `e_m_lt1`, `e_m_sub`, `e_m_modb1`, `e_m_nomod` (the reduction of t0 changed or removed): d0 is used only modulo b and by u64 arithmetic modulo 2^64. A negative t0 converts to 2^64 + t0. For next, b - d0 = b + |t0| exactly, then modulo b gives |t0| mod b as required. For prev, d = d0 + ((n - d0) / b) b is computed modulo 2^64 and equals floor((n + |t0|) / b) b - |t0|, which is the largest d <= n with d = t0 modulo b. `m < 1` and `m <= 0` add b when m = 0, which changes d0 by b and not d (n >= b); `m -= b` and the modulus b + 1 give a value equal to t0 modulo b by the same argument; `nomod` differs only for |t0| = b, which happens only for b = 1, where d = n either way.
  - `e_ret_t` (returns r0 + 1 when r0 is not 1): the result is only compared with 1. `n_gcd_gt` (`> 1` for `!= 1`): the gcd of b >= 1 is at least 1. `n_d0_nomod` (no `% b` in d0 for next): d0 = b instead of 0 gives the same d because n >= b.
- 5 cost mutants (the values are right, only the time grows; kept apart from the kills and the survivors; no test can detect them because the repository forbids timing assertions). Observed, whole suite alone, one core: `cm_e_rep` (40 extra divisions per round) passes in 3.1 s; `cm_e_twice` (40 extra Euclid runs per call) passes in 21.2 s; `cm_n_rep` (40 extra Euclid runs in the neighbour) passes in 21.0 s; `cm_e_sub` (subtractive Euclid) and `cm_dd_walk` (d found by adding b until n) did not finish in 105 s and were stopped. The unmutated binary takes 1.56 s.

## Not run

Other builds than the local gcc 11.4 ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, and a mutant run on shuffled case order. For n above 3000 only the edge list and the samples are checked (no whole sequence); aliasing of `c` and `d` is not tested.
