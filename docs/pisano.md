# pisano: the Pisano period

Code: `core/pisano/pisano.cpp` (contract in `core/pisano/pisano.h`). Tests: `tests/pisano/pisano_tests.cpp`. Reference: `tools/pisano_ref.py`. Evidence label: unit tests only.

## Rule

`pisano_period(m, out)` sets `*out` to the Pisano period pi(m): the smallest n > 0 with F(n) = 0 and F(n + 1) = 1 modulo m (F(0) = 0, F(1) = 1), for every u32 m from 1 to 2^32 - 1. pi(1) = 1. The result is a u64: pi(m) <= 6 m can exceed 2^32 (for m = 2 * 5^13 = 2441406250 it is 14648437500).

- Check order: a null `out` is NullArgument; then m = 0 is BadModulus. Nothing is written to `*out` on a failure.
- Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array.

## Method and proof

Let Q = [[1, 1], [1, 0]], so Q^n = [[F(n+1), F(n)], [F(n), F(n-1)]]. Q^n = I modulo m exactly when F(n) = 0 and F(n + 1) = 1 modulo m, so pi(m) is the order of Q in GL2(Z/m). That order is the LCM of the orders modulo the prime powers p^k exactly dividing m (Q^n = I modulo m exactly when it holds modulo every p^k). The answer is that LCM. For one prime power p^k:

1. A multiple of the order is known; it is a candidate, not the order.
   - p = 1 or 4 (mod 5): the characteristic polynomial x^2 - x - 1 has discriminant 5, so it has two distinct roots in F_p, Q is diagonalisable over F_p and Q^(p-1) = I modulo p. N0 = p - 1.
   - p = 2 or 3 (mod 5): the roots are conjugate in F_(p^2) and distinct; the product of the roots is -1, and a root r has norm r^(p+1) equal to the product of r and its conjugate, which is -1, so r^(2(p+1)) = 1 and Q^(2(p+1)) = I modulo p. N0 = 2(p + 1). For p = 2 this gives 6, a multiple of the true order 3.
   - p = 5 is degenerate: x^2 - x - 1 = (x - 3)^2 modulo 5, so Q = 3 I + N with N^2 = 0 and Q^20 = 3^20 I + 20 * 3^19 N = I modulo 5 (3^4 = 1 and 20 = 0 modulo 5). N0 = 20, shown directly.
   - Lift to p^k: if A is an integer matrix, (I + p^j A)^p = I modulo p^(j+1) for every j >= 1, also for p = 2 ((I + 2A)^2 = I + 4(A + A^2)). From Q^N0 = I modulo p, Q^(N0 p^(k-1)) = I modulo p^k. The candidate c = N0 * p^(k-1) is a multiple of the order of Q modulo p^k.
2. The exact order is found by stripping: for every prime q dividing c, while q divides the current n and Q^(n/q) = I modulo p^k, set n = n / q. The order divides n throughout, and at the end no prime can be removed, so n is the order. The source does NOT use pi(p^k) = pi(p) p^(k-1), and the docs do not claim it: the order is pi(p) p^j for some j <= k - 1 and stripping finds it. (p = 2 is stripped like any other prime, including the prime 2 itself, which is stripped last.)

Q^n modulo a modulus below 2^32 is the fast doubling of (F(n), F(n + 1)): F(2k) = F(k) (2 F(k+1) - F(k)), F(2k + 1) = F(k)^2 + F(k+1)^2. The identity target is F(n) = 0 and F(n + 1) = 1 % mod.

Factoring: m by trial division by 2 and the odd numbers up to 65535; what remains above 1 has no factor up to 65535 and is below 2^32, so it is a prime. N0 by trial division up to its square root, each prime stripped as soon as it is found, so no array is needed.

Bounds in u64: a residue is below the modulus (at most 2^32 - 1), so a product is below 2^64 and is reduced at once; t = (2 b + mod - a) mod m is formed first (the sum is below 3 * 2^32), so that both factors of t * a are below the modulus; the two squares for F(2k + 1) are reduced before they are added (a sum below 2^33). The candidate is at most 3 m (2(p + 1) / p <= 3, p = 2 gives 3 * 2^k), below 2^34. Every LCM prefix divides pi(m) <= 6 m < 2^35, and a / gcd(a, b) * b equals that LCM, so nothing overflows.

Cost: O(sqrt(m)) with a small constant. For a prime near 2^32: about 32000 odd trial divisions of m, about 46000 of N0 (N0 <= 2 * 2^32 + 2), then at most about 35 fast doublings of 64 steps (6 reductions each). For a prime square p^2 (p <= 65521) trial division of m stops at p and N0 is about p. The worst cases are a prime, or a product of two primes near 2^16, both about 10^5 trial divisions plus a few thousand reductions. Whole sweeps of 2^17 values of m take about 1 s in the tests below.

## Tests

Expected values come from the header's table, an in-test brute walk (F(0), F(1), ... until the pair (0, 1 % m) returns), an in-test CERTIFICATE that needs no Pisano theory (n is the order of Q modulo m exactly when Q^n = I and Q^(n/q) is not I for every prime q dividing n; 2 x 2 matrix powers by square and multiply, n factored by trial division), and digests and tables from `tools/pisano_ref.py`.

The reference is Python standard library only. It factors m by its own trial division and, for each prime power, ascends through ALL divisors of N0 * p^(k-1) (a multiple of the order, see above) with exact 2 x 2 integer matrix powers, taking the least divisor d with Q^d = I; pi(m) is the LCM. It uses neither stripping nor doubling. Its large entries also pass the certificate. `python3 tools/pisano_ref.py walk` compares it with the brute walk (every m <= 4000 and 14 special moduli); `emit` takes about 25 s warm and a few minutes cold. Digest: FNV-1a 64-bit over u64 words (m, pi(m) in increasing order).

Extents and local runtimes (gcc 11.4, `-O1`, ASan + UBSan, one core; the whole binary takes about 20 s; no case near 30 s; none asserts time):
- `header table` (0.00 s): the header's table plus pi(6) = 24, pi(8) = 12, pi(9) = 24, pi(12) = 24, pi(24) = 24, pi(120) = 120, pi(2441406250) = 14648437500, pi(2^32 - 1) = 56362680.
- `failure cases: null first, then BadModulus, nothing written` (0.00 s): a null `out` with m in {0, 1, 2, 10, 2^31, UINT32_MAX} (null wins, also for m = 0); m = 0 with a non-null `out` gives BadModulus with the sentinel unchanged and guard words intact; the other values are Ok.
- `every m = 1..4000 against the brute walk` (exhaustive; 0.05 s).
- `special moduli against the brute walk` (0.05 s): 2 * 5^k for k = 1..8 (period 6 m, up to 4687500 steps), 24 * 5^4 to 24 * 5^6, 999983, 10^6, 10^6 + 3.
- `pi(m) = m for m = 24 * 5^k and pi(m) = 6 m for m = 2 * 5^k up to 2^32` (0.00 s).
- `large moduli against the reference values` (0.05 s) and `large moduli pass the certificate` (0.06 s): 1134 values up to UINT32_MAX (sampled): 2^j - 1, 2^j, 2^j + 1; 2^32 - 2 and 2^31; the 30 largest u32 primes and the 30 largest of each class mod 5; squares of the 60 largest primes up to 65521 and of the primes below 200; cubes; every power of 2, 3, 5, 7, 11, 13 below 2^32 (including 5^13 and 3^20); 2 * 5^k; 24 * 5^k; Fibonacci and Lucas numbers; primorials; products of two primes near 2^16; 600 xorshift64 random m.
- `prime squares: pi(p^2) = p * pi(p) for every prime p up to 65521` (exhaustive over the 6542 primes; 0.5 s): no Wall-Sun-Sun prime below 65522, which is what makes the last strip of p a no-op for odd p (see the survivors).
- `certificate for every m = 1..2^18`, `2^18 + 1..2^19`, `2^19 + 1..3 * 2^18`, `3 * 2^18 + 1..2^20` (exhaustive; 2.6, 2.9, 3.0, 3.0 s).
- `sweep chunk 0` to `sweep chunk 7` (exhaustive over every m = 1..2^20, 2^17 values per chunk, the last also 2^20; 0.8 to 1.0 s each): bounds 1 <= pi(m) <= 6 m and the reference digest.

Deviation, stated plainly: for the large moduli there is no brute walk (the period is up to 1.5 * 10^10). The oracle there is the reference's divisor enumeration plus the certificate, which proves each value without the method's theory; the walk is run only up to the special moduli above.

## Mutation testing

91 source mutants of `core/pisano/pisano.cpp` (script outside the repo), built with ASan + UBSan against the final tests (md5 e95c27b070f5ad427c95a97b70a04a94, not edited after the run), abort after the first failure, 25 s limit. First pass: the cases `header table`, `failure cases`, `every m = 1..4000`, `special moduli`, `pi(m) = m`, `large moduli` (both) and `prime squares`, the same unedited file selected by case name. Every mutant that survived the first pass was rerun on the whole suite.

- 75 killed by a failed assertion in the first pass.
- 2 crash-only: `g_t`, `g_ret` (a broken gcd).
- 1 timeout, not counted as a kill: `st_div_rm` (the divisibility test dropped in the strip loop). With a probe added to the loop, m = 11 showed `n = 10` then `n = 0` at q = 11, and `n = 0` repeating at every later iteration (Q^0 = I, so the test never fails and n / q stays 0).
- 13 survivors on the whole suite, each equivalent:
  - `id_b_1`, `id_ret_b1` (the target 1 instead of 1 % mod): the modulus is at least 2 there (m = 1 has no prime power), so 1 % mod = 1.
  - `id_bit62`, `id_bit40` (the doubling starts at a lower top bit): the argument n is at most 2^34, so the higher bits are zero.
  - `id_bit31` (the doubling starts at bit 31): an argument of at least 2^32 occurs only for the candidate c = 8 * 3^19 = 9298091736 of 3^20, where c / 2 = 4649045868; it is not a multiple of the order 8 * 3^19, so the identity test is false either way (a prime p gives c / 2 <= p + 1 < 2^32, and every other prime power below 2^32 has c < 2^33; the reference scan over all p^k with k >= 2 found only 3^20).
  - `op_20_40` (the candidate 40 for p = 5) and `op_pm1_2` (2(p - 1) for p - 1): still a multiple of the order, and stripping removes the excess.
  - `op_fl_step1` (every candidate q, not only 2 and the odd numbers): an even q > 2 never divides what remains after 2 was removed.
  - `op_fl_noskip`, `op_rem_noeq` (strip the prime p early, in the loop or as the leftover): stripping is independent per prime, and the final strip of p is then a no-op.
  - `op_rem_gt2` (`rem > 2` for the leftover prime): the leftover 2 would need N0 = 2 or 3 times a power of 2 with no loop pass, which no p gives.
  - `op_final_q` (the last strip is of 2 instead of p): for odd p the strip of p can only matter when pi(p^k) lacks a factor p that the candidate has, so for k = 1 the candidate has no p (p does not divide p - 1 or 2(p + 1)); for k >= 2 it needs pi(p^2) = pi(p), a Wall-Sun-Sun prime, and the test `prime squares` shows pi(p^2) = p * pi(p) for every p <= 65521, which gives pi(p^k) = p^(k-1) pi(p) for all k. For p = 2 the mutant is the original, and `op_final_rm` kills it.
  - `pp_early` (`*out = 0` after the checks): overwritten at the end on the only path that continues.

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, the brute walk beyond the special moduli, and any timing assertion. Nothing here is a CI result.
