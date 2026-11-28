# sumsq: sums of two and four squares

Code: `core/sumsq/sumsq.cpp` (contract in `core/sumsq/sumsq.h`). Tests: `tests/sumsq/sumsq_tests.cpp`. Reference: `tools/sumsq_ref.py`. Evidence label: unit tests only.

## Rule

`count_two_squares(n, out)` sets `*out` to r2(n), the number of integer pairs (x, y) with x^2 + y^2 = n; `count_four_squares(n, out)` sets `*out` to r4(n), the number of integer quadruples (x, y, z, w) with x^2 + y^2 + z^2 + w^2 = n. Signs and order count. Every u32 n from 0 to 2^32 - 1 is valid; r2(0) = r4(0) = 1.

- Check order: a null `out` is NullArgument; nothing is written to `*out` on a failure; each function writes only its own output.
- Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array. O(sqrt(n)) steps, O(1) memory.

## Method

Two classical theorems are used. They are cited by name, the formulas are stated exactly as used, and the tests check them (exhaustively where stated below); the docs do not prove them.

- Two-square theorem (r2): for n >= 1, r2(n) = 4 (d1(n) - d3(n)), where d1 and d3 count the divisors of n that are 1 and 3 modulo 4. Equivalently, with n = 2^a * prod p^e * prod q^f (p = 1 mod 4, q = 3 mod 4): r2(n) = 0 if some f is odd, otherwise r2(n) = 4 * prod (e + 1).
- Jacobi's four-square theorem (r4): for n >= 1, r4(n) = 8 * the sum of the divisors of n that are not divisible by 4. That is 8 sigma(n) for odd n, and 24 sigma(m) for n = 2^a m with a >= 1 and m odd (the divisors not divisible by 4 are the divisors of m and twice those).

Source: trial division by 2 (the exponent of 2 does not matter for r2 and only the parity of n matters for r4), then by the odd q while q * q <= the remainder, each prime stripped completely as found. What is left above 1 has no factor up to its square root, so it is a prime. r2 multiplies (e + 1) for p = 1 mod 4, returns 0 for an odd exponent of q = 3 mod 4, and for the leftover prime multiplies 2 (p = 1 mod 4) or returns 0 (p = 3 mod 4). r4 multiplies sigma(q^e) = 1 + q + ... + q^e per odd prime power and (rem + 1) for the leftover prime, then by 8 (n odd) or 24 (n even).

Bounds in u64. The real maxima were measured by a reference search over all n below 2^32 (not a loose bound): the divisor count is at most 1920 (at 3491888400); sigma(n) is at most 22318571520 (at 4227022800); r2 is at most 576 (at 4132042525); r4 is at most 140391014400 (at 4219365150, below 2^37.1). The worst trial-division count for a u32 is 32767 odd divisions plus the divisions by 2. The partial products of sigma never exceed sigma(n), so u64 holds everywhere; q * q is below 2^34. The tests check the two maxima r2 = 576 and r4 = 140391014400 over the large list (which holds those n); they do not rerun the search over all n.

## Tests

Expected values come from the header's table, in-test brute force (a histogram of x^2 + y^2 over all integer x, y; the direct 4-variable loop for n <= 60; the convolution r4(n) = sum over k of r2(k) r2(n - k) of the histogram), in-test sieves that do not factor (the d1 - d3 sieve for r2, the sigma sieve for r4 = 8 (sigma(n) - 4 sigma(n / 4))), in-test divisor-pair loops over d <= sqrt(n) and a two-pointer x-loop for r2 at the large values, and digests and tables from `tools/sumsq_ref.py`.

The reference is Python standard library only. It factors n by its own trial division over a prime list, enumerates ALL divisors and takes r2 = 4 (d1 - d3) and r4 = 8 * the sum of the divisors not divisible by 4; the large list also passes the x-loop (isqrt per x) for r2. `python3 tools/sumsq_ref.py brute` compares it with the histogram (every n <= 2^16), the 4-variable loop (every n <= 60) and the convolution (every n <= 3000); `check` runs everything and prints the digests; `emit` prints the C++ golden values (about 10 s in total with gcc/CPython 3.10 here). Digest: FNV-1a 64-bit over u64 words (n, r2, r4 in increasing order).

Extents and local runtimes (gcc 11.4, `-O1`, ASan + UBSan, one core; the whole binary, 33 cases and 256 assertions, takes about 5 s; no case near 30 s; none asserts time). Each call passes its result cell between guard words.
- `header table` (0.00 s): the header's table, r2(0) = r4(0) = 1, r2/r4 of 2^31 and 2^32 - 1 (58436826624 = 8 * 4 * 6 * 18 * 258 * 65538), r2(2^32 - 2) = 0.
- `failure cases: null first, nothing written, each writes only its own cell` (0.00 s): a null `out` with n in {0, 1, 2, 3, 2^31, UINT32_MAX} for both functions; a non-null `out` writes only its own cell (the other keeps its sentinel) and the guards survive.
- `direct 4-variable loop for every n = 0..60` (exhaustive; 0.00 s).
- `r4 against the convolution of the histogram for every n = 0..2^15` (exhaustive; 1.9 s) and `..., 200 sampled n up to 2^21` (sampled; 0.7 s; includes 2^21).
- `sieve chunk 0` to `sieve chunk 15` (exhaustive over every n = 0..2^21, 2^17 values per chunk, the last also 2^21; 0.03 to 0.3 s each): r2 against the histogram and the d1 - d3 sieve, r4 against the sigma sieve.
- `digest chunk 0` to `digest chunk 7` (exhaustive over every n = 0..2^20, 2^17 values per chunk; chunk 0 holds n = 0 and the last holds 2^20; 0.03 to 0.08 s each): (n, r2, r4) against the reference digest.
- `identities for every n = 1..2^19` (exhaustive; 0.4 s): r2(2n) = r2(n); r4(2n) = 3 r4(n) for odd n and r4(n) for even n; r2 divisible by 4, r4 divisible by 8.
- `multiplicativity on 40000 sampled coprime pairs` (sampled; 0.1 s): 4 r2(ab) = r2(a) r2(b); 8 r4(ab) = r4(a) r4(b) for odd a, b.
- `large values against the table` (0.03 s) and `large values: x-loop and divisor-pair routes` (0.3 s): 1616 values up to UINT32_MAX (sampled): 2^j - 1, 2^j, 2^j + 1; the n at the maxima above; 2^32 - 1 and 2^32 - 2; 2^31 and 2^30 with neighbours; the 30 largest u32 primes and the 30 largest of each class mod 4; 2 * each of the 30 largest primes below 2^31; squares of the 30 largest primes up to 65521 and of the 50 smallest; cubes and fifth powers; powers of 2, 3, 5, 7, 11, 13, 17, 29; primorials; products of the primes 1 mod 4 and of the primes 3 mod 4 (and their squares, and 4 times those); 40 values a^2 + b^2; 40 odd values m times 1, 2, 4, 8, 16 (a = 0..4); 80 values 3 and 7 mod 8; 60 values with exactly one prime above 65535 times a small factor; 600 xorshift64 random n.

## Mutation testing

128 source mutants of `core/sumsq/sumsq.cpp` (script outside the repo), built with ASan + UBSan against the final tests (md5 bc2ba574eedf140e9a0d80b43199bf6e, not edited after the run), abort after the first failure, 25 s limit, every mutant run on the whole suite (no reduced pass). Mutated: the null check and its order, the n = 0 path, the factor-2 strip, the loop start, bound and step, the skip test, the exponent count, the mod-4 tests, the odd-exponent zero, the (e + 1) term, the leftover-prime branch and constants, the 8 / 24 constants and the odd test, the sigma accumulation, and integer widths.

- 96 killed by a failed assertion. One of them, `a_s2_div4` (r2 strip `rem /= 4u`, which loops forever at n = 2), is killed in this build because gcc assumes a loop without side effects terminates; with a probe inside the loop it hangs like `b_s2_div4`.
- 4 crash-only (null dereference): `a_null`, `b_null`, `a_swap_null`, `b_swap_null`.
- 18 timeouts, not counted as kills.
  - 12 with a repeating state (probe added to the loop, shown at the first n that hangs): `a_zero_rm`, `b_zero_rm`, `a_zero_noret`, `b_zero_noret`, `a_zero_ne` (n = 0: `rem` is stuck at 0 and `rem % 2 == 0` stays true); `b_rem_n` (n = 1: `rem` = 0 stays 0); `b_s2_div4` (n = 2: `rem` = 0); `a_q1`, `b_q1` (n = 1: `q` = 1 and `rem` = 1 repeat, the loop test depends only on those); `a_div_rm`, `b_div_rm` (n = 9: `rem` stays 9 at q = 3); `b_div_q2` (n = 27: `rem` = 0 at q = 3).
  - 6 are complexity mutants, not hangs: `a_bound_q`, `b_bound_q` (`q <= rem`), `a_bound_h`, `b_bound_h` (`q * 2 <= rem`), `a_q32`, `b_q32` (u32 `q`, so `q * q` wraps and the loop does not stop at 65536). Compiled at `-O2` they return the right values for n = 1000, 65537 and 4294967291 (a whole run of both functions on the three values takes 5.6 s for the `q <= rem` and the `q32` mutants and 2.8 s for `q * 2 <= rem`) but need O(n) instead of O(sqrt(n)) steps; under ASan they pass the 25 s limit. No test asserts time (a rule of this repo), so these are not detected: they break the cost contract, not a value.
- 10 survivors on the whole suite, each equivalent in value:
  - `a_step1`, `b_step1` (`q += 1`): after the strip of 2 and of each smaller prime, `rem` has no even or composite divisor below q, so q never divides `rem` unless it is a prime; the same results, about twice the steps.
  - `a_bound_n`, `b_bound_n` (`q * q <= n`): the remainder is at most n; extra iterations find no divisor (a leftover prime P that is at most sqrt(n) is then reached as q = P and handled like any other prime of the loop), so the same value.
  - `a_skip_rm`, `b_skip_rm` (no `continue`): for q not dividing `rem` the exponent is 0, which multiplies by (0 + 1) in r2, adds nothing to the parity test and multiplies sigma by 1.
  - `a_rem_gt`, `b_rem_gt` (`rem > 2`): `rem` is odd after the strip of 2, so it is never 2.
  - `b_pw32` (u32 `pw`): `pw` is a prime power dividing n < 2^32, so it fits.
  - `a_prod32` (u32 `prod`): prod is at most 144 (r2 <= 576).

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, and a rerun of the search over every n < 2^32 for the maxima (they were measured by the maintainer's reference search and agree with the large-list entries). Nothing here is a CI result.
