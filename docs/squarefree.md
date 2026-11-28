# squarefree: the Moebius function and the count of squarefree numbers

Code: `core/squarefree/squarefree.cpp` (contract in `core/squarefree/squarefree.h`). Tests: `tests/squarefree/squarefree_tests.cpp`. Reference: `tools/squarefree_ref.py`. Evidence label: unit tests only.

## Rule

`mobius(n, out)` sets `*out` to mu(n) for every u32 n from 1 to 2^32 - 1: 1 for n = 1, 0 when a square above 1 divides n, otherwise (-1)^k for k distinct primes. `squarefree_count(n, out)` sets `*out` to Q(n), the number of squarefree k with 1 <= k <= n, for every u32 n; Q(0) = 0.

- Check order for both functions: a null `out` is NullArgument (also for `mobius(0)`); then, for `mobius` only, n = 0 is BadArgument. Nothing is written to `*out` on a failure. Each function writes only its own output.
- Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array.

## Method and proof

**Claim.** Q(n) = the sum over d = 1..floor(sqrt(n)) of mu(d) * floor(n / d^2).

**Proof.** Every k >= 1 is uniquely k = m^2 s with s squarefree (take m^2 as the largest square dividing k). The integers d with d^2 | k are exactly the divisors of m. The sum of mu(d) over d | m is 1 for m = 1 and 0 for m > 1 (for m = p1^e1 ... pr^er with r >= 1 the divisors with mu(d) != 0 are the 2^r products of distinct primes, and the signs cancel). So [k is squarefree] = the sum of mu(d) over the d with d^2 | k. Summing over k <= n and swapping the sums gives the sum over d of mu(d) * #{k <= n : d^2 | k} = the sum of mu(d) floor(n / d^2). Terms with d^2 > n are zero, so the sum stops at floor(sqrt(n)). The two facts used (unique k = m^2 s, and the divisor sum of mu) are proved here, not cited.

**mu by trial division.** For m >= 1: strip one 2 (a second 2 gives 0, otherwise one sign flip), then for the odd q while q * q <= the remainder: when q divides the remainder, divide once; a second division by q gives 0, otherwise one sign flip. What is left above 1 has no factor up to its square root, so it is a prime: one more flip. `mobius(n)` is this with m = n; the worst case is a prime near 2^32, 32767 odd divisions plus the division by 2 (measured: mu(4294967291) takes 32767 loop tests). The count calls it for d <= 65535: at most 127 loop tests for one d (measured over all d <= 65535), and 1,570,846 loop tests in total for n = 2^32 - 1, the whole count (measured by replaying the loop in Python). That is inside the header's sqrt(n) * n^(1/4) = 65536 * 256 = 16.8 million.

**Bounds.** The loop condition is d * d <= n with d and d * d as u64 (d reaches 65536 when n >= 65535^2, where a u32 d * d would wrap to 0). Every term is at most n. The running sum is a signed 64-bit integer. Its partial sums are bounded by n times the sum over d >= 1 of 1/d^2, which is below 1.645 n < 2^33 for every u32 n; the largest partial sum seen over the test lists is exactly n (the first term). The final value is Q(n) <= n, converted to u64 after the sum is known to be non-negative. Memory is O(1).

## Tests

Expected values come from the header's table, in-test definitional scans (a flag table of squarefree k made by marking the multiples of d^2 for every d >= 2, with prefix counts, to 2^24; a linear sieve for mu to 2^22; mu of special families straight from the definition), an in-test copy of the identity route below, and digests and tables from `tools/squarefree_ref.py`.

The independent large reference uses no mu at all. Every k is uniquely m^2 s with s squarefree, so the sum over m >= 1 of Q(floor(n / m^2)) = n, hence Q(n) = n - the sum over m = 2..floor(sqrt(n)) of Q(floor(n / m^2)), solved with memoisation over a table of Q by the definition up to 2^22 (Python) or 2^24 (the test). A second, different check on the large values: the Python reference also computes the mu-sum (the same formula as the source, other code, mu from its own factorisation) and the two must agree on every entry (`python3 tools/squarefree_ref.py large`). The known values Q(10^6) = 607926, Q(10^9) = 607927124, Q(2^31) = 1305513583 and Q(2^32 - 1) = 2611027094 were first reported by the maintainer; they are used in the tests because the reference proves them by the recurrence (the Python run asserts them and the test checks them against the in-test recurrence).

`python3 tools/squarefree_ref.py brute` compares mu from its own factorisation with the definition (every n <= 2^16) and with the divisor sum (every n <= 4000), and the recurrence (table cut at 64) and the mu-sum with the definitional count for n < 5000 and at 2^j and 10^6; `emit` prints the C++ golden values; `check` runs everything (about 20 s on CPython 3.10 here). Digest: FNV-1a 64-bit over u64 words (n, mu(n) + 1 in increasing order).

Extents and local runtimes (gcc 11.4, `-O1`, ASan + UBSan, one core; the whole binary, 31 cases and 88 assertions, takes about 8 s; no case near 30 s; none asserts time). Each call passes its result cell between guard words.
- `header table` (0.00 s): the header's mu and Q values, mu(2^32 - 1) = -1 (3 * 5 * 17 * 257 * 65537), mu(2^32 - 2) = 1 (2 * 2147483647).
- `failure cases: null first, BadArgument for mobius(0), nothing written` (0.00 s): a null `out` with n in {0, 1, 2, 3, 2^31, UINT32_MAX} for both functions (null wins, also for `mobius(0)`); `mobius(0)` with a valid `out` gives BadArgument with the sentinel and guards intact; `squarefree_count(0)` is Ok with 0; each function leaves the other's cell alone.
- `mobius vs sieve chunk 0` to `7` (exhaustive over every n = 1..2^22, 2^19 values each; 0.1 to 0.4 s).
- `mobius digest chunk 0` to `7` (exhaustive over every n = 1..2^20, 2^17 values each, the last includes 2^20; 0.01 to 0.04 s).
- `count vs the definitional scan for every n = 0..65535` (0.1 s) and `... = 65536..2^17` (0.2 s): exhaustive over n = 0..2^17.
- `count at d^2 - 1, d^2, d^2 + 1 for every d = 1..4096` (exhaustive over those 12288 values; 0.6 s).
- `count vs the definitional scan, 4000 sampled n from 2^17 to 2^24` (sampled; 0.3 s).
- `Q(n) - Q(n - 1) = mu(n)^2` for every n = 1..2^16 (0.1 s); `sum over d of Q(floor(n / d^2)) = n` for every n = 0..2^15 (0.1 s); `sum of mu(d) over d | n is [n = 1]` for every n = 1..2^16 (0.05 s); `mu(ab) = mu(a) mu(b)` on 20000 sampled coprime pairs (sampled; 0.04 s).
- `every prime up to 65521: mu(p), mu(p^2), mu(p q)` (all 6542 primes, mu(p) = -1, mu(p^2) = 0, mu(p q) = 1 for neighbouring primes; 0.6 s) and `grid 2^a 3^b 5^c 7^d below 2^32 from the definition` (every product of the grid, mu from the exponents).
- `large mu values against the table` (0.05 s): 1414 values (sampled): 2^j - 1, 2^j, 2^j + 1; UINT32_MAX and its neighbour, 2^31; the 30 largest u32 primes and twice the 10 largest; primorials to 223092870 and their neighbours; products of two primes near 65536; p^2 and p^2 - 1 for the primes above 65000, and p^2, 3 p^2 for the 60 smallest; 60 values with exactly one prime above 65535; 40 values p^2 q; 600 xorshift64 random n.
- `large count values against the table` (1.2 s) and `large count values against the identity route` (2.4 s): 487 values up to UINT32_MAX (sampled): 10^k for k = 0..9; 2^j - 1, 2^j, 2^j + 1; UINT32_MAX, UINT32_MAX - 1, 2^31; d^2 - 1, d^2, d^2 + 1 for d in {65535, 65521, 65534, 46341, 32768, 30030, 4096, 2310, 1000}; the squares of the 40 largest primes up to 65521 with neighbours, and the squares of the 30 smallest; squares of primorials with neighbours; 150 random n and 60 random n up to 2^24.

## Mutation testing

82 source mutants of `core/squarefree/squarefree.cpp` (script outside the repo), built with ASan + UBSan against the final tests (md5 0edd9d6482d44a1e18cae677eccb0393, not edited after the run), abort after the first failure, 25 s limit, every mutant run on the whole suite (no reduced pass, except `c_mu_n` below). Mutated: the null check and its order, the n = 0 path, the strip of 2, the loop start, bound and step, the skip test, the second-division zero, the sign flips, the leftover prime, the return value, the count's loop start, bound and step, the division by d * d, the sign and sum, the integer widths and the result.

- 69 killed by a failed assertion.
- 1 crash-only (null dereference): `c_null_rm`.
- 4 survivors on the whole suite, each equivalent in value:
  - `m_rem_gt2` (`rem > 2` for the leftover prime): `rem` is odd after the strip of 2, so it is never 2.
  - `c_sum_u64` (an unsigned sum): two's complement addition gives the same final 64-bit value; the final value is below 2^32.
  - `c_d0` (the loop starts at d = 0): mu(0) is 0 in `mu_of` (the strip of 2 returns 0), so the term is skipped.
  - `c_sign_ge` (`mu >= 0` for the sign): a zero mu has already been skipped.
- 8 timeouts, not counted as kills and not equivalent survivors:
  - 6 are complexity mutants, same values, only slower, and no test detects them because the repo forbids timing assertions. Observed with `-O2` at n = 4294967291 (the original takes about 0.01 s for both functions): `m_bound_q` (`q <= rem`) returns the right mu and Q in 11.8 s; `m_bound_h` (`q * 2u <= rem`) in 3.4 s; `m_bound_m` (`q * q <= m`, no early exit on `rem`) and `m_step1` (`q += 1`) return the right values in 0.03 s and 0.01 s at that n, but their timeouts under the sweeps come from the many composite n, where they do not stop early (a larger constant, the same worst case); `c_bound_d` (`d <= n`) and `c_bound_h` (`d * 2 <= n`) return the right Q at n = 1000000 (0.23 s and 0.09 s, against 0.00 s) and did not finish in 15 s at n = 4294967291 (terms with d^2 > n are zero, so the value is the same by construction).
  - 1 unclassified: `c_d32` (u32 `d`): `d * d` wraps at d = 65536, so the loop does not stop at the true bound for n >= 65535^2 and runs on for more than 15 s at n = 4294967291 (it returns the right Q(10^6) in 0.00 s). I did not show a repeating state for it (the sum keeps changing), so it is not classified as a hang.
  - 1 slow wrong-value mutant, killed only in isolation: `c_mu_n` (`mu_of(n)` in the count) is too slow under the full sweeps to reach a failing assertion; run alone on the case `header table` of the same unedited file it is killed (Q(1000) = 0). That is a reduced run of the same unedited file selected by case name; it is not counted in the 69 kills of the whole-suite run.

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, and Q beyond the 487 large values (no exhaustive sweep above 2^17). Nothing here is a CI result.
