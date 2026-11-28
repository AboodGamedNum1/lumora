# arithfun: arithmetic functions of one u32

Code: `core/arithfun/arithfun.cpp` (contract in `core/arithfun/arithfun.h`). Tests: `tests/arithfun/arithfun_tests.cpp`. Reference: `tools/arithfun_ref.py`. Evidence label: unit tests only.

## Rule

For one u32 `n`: `euler_phi` gives phi(n), `divisor_count` gives d(n), `divisor_sigma` gives sigma(n) (as u64), `radical` gives rad(n), and `prime_factor_counts` gives omega(n) (distinct primes) and Omega(n) (primes with multiplicity).

- Check order for every function: a null output pointer is NullArgument (for `prime_factor_counts`, either pointer null); then n = 0 is BadArgument. Nothing is written on a failure. n = 1 is valid: phi = d = sigma = rad = 1 and omega = Omega = 0.
- Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array, no sieve or table. O(sqrt(n)) steps, O(1) memory.

## Method and proof

One internal pass `factorise(n)` computes all six values; each public function calls it and returns its field. Let m be what is left of n (m = n at the start) and p run over 2, 3, 5, 7, 9, 11, ... (2, then every odd number) while p * p <= m. When p divides m, divide it out as often as possible (exponent e), then update with p and e. After the loop, if m > 1 it is one prime (exponent 1) and gets the same update.

**Why the divisors found are exactly the primes of n.** Invariant: at the start of each step m has no prime factor below p. A composite odd p has all its prime factors below p, so it cannot divide m; the even numbers above 2 have the factor 2, already removed. So p divides m only when p is prime, and the exponent loop removes all of p. When the loop ends, p * p > m; if m were composite it would have a prime factor q with q * q <= m and q >= p (no smaller factor is left), so p * p <= q * q <= m, a contradiction. So m is 1 or a prime.

**Updates for a prime p with exponent e.**
- phi: phi -= phi / p. Before the update phi = (n / (product of the earlier primes)) * (product of their (q - 1)); since p divides n and not the earlier primes, p divides phi, so phi / p is exact and phi - phi / p = (phi / p) * (p - 1) is phi times (1 - 1/p) in integers.
- d: d *= e + 1. sigma: sigma *= 1 + p + ... + p^e (built as t by t += pp, pp *= p). rad: rad *= p. omega += 1. Omega += e.
These are the multiplicative formulas over the prime powers p^e that divide n exactly.

**Cost.** The loop runs while p * p <= m <= n < 2^32, so p <= 65535 and at most 32768 times (p = 2 and the 32767 odd p from 3 to 65535); the worst cases are a prime near 2^32 and a product of two primes near 2^16. Measured trips (a Python replica of the loop, counting the loop tests that pass): 4294967291 (prime) 32768, 65521 * 65537 (4294049777) 32761, 4294967295 129, 4227022800 9, 2^31 1.

## Bounds in u32 and u64

- p is at most 65537 in the loop (the last step before the test fails), so `static_cast<u64>(p) * p` is below 2^33 and exact; with a u32 product the test would wrap at p = 65537 (a mutant below does this).
- phi <= n, rad <= n, and every power pp = p^e divides n, so pp <= n < 2^32 (u64 holds it); d <= 1920 and e + 1 <= 32 (Omega <= 31).
- sigma. Write sigma(n) / n = the product over p^e exactly dividing n of (1 + 1/p + ... + 1/p^e) < the product over the distinct primes p of n of p / (p - 1). The product of 10 distinct primes is at least 2 * 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23 * 29 = 6469693230 > 2^32 - 1, so n has at most 9 distinct primes; p / (p - 1) falls as p rises, so the product over the distinct primes of n is at most the product over the first nine primes, which is exactly 676039 / 110592 = 6.11291... (computed in exact fractions: 223092870 / 36495360). Hence sigma(n) < 6.1130 * n <= 6.1130 * (2^32 - 1) < 2.626e10 < 2^35 = 34359738368, which is the header's bound. Every factor t = sigma(p^e) divides sigma(n) and every running product of coprime factors is at most sigma(n), so no intermediate leaves u64 (and none leaves 2^35).
- The test checks `sigma * 110592 < n * 676039 + 1` and `sigma < 2^35` on every large value it touches.

## Maxima over every u32

Measured by a full-range check run locally outside the repository (not part of the tests): a segmented factorisation sieve (primes up to 65535 plus one leftover prime per n) over every n = 1..2^32 - 1, in four chunks of 2^30:
- the largest sigma is 22318571520 at n = 4227022800 (ratio sigma / n = 5.2678);
- the largest d is 1920, first at n = 3491888400;
- per chunk [1, 2^30), [2^30, 2^31), [2^31, 3 * 2^30), [3 * 2^30, 2^32): largest sigma 5399654400, 10973491200, 16378951680, 22318571520; largest d 1344, 1600, 1792, 1920;
- the same run counted the n with 9 distinct prime factors: 919 below 2^31 and 3302 above, 4221 in all, with the sum 12443106907533; the structured set of that name in the tests has exactly 4221 members and the same sum (checked in Python).
The same two maxima are found by the reference's search over every number built from the first 16 primes (2 to 53) with any exponents (`python3 tools/arithfun_ref.py maxima`). That search is a search, not a proof of completeness for sigma: for d the argument is valid and for sigma it is not.
- d: d(n) depends only on the multiset of exponents. Replacing the distinct primes of n by the smallest primes (the largest exponent on 2) keeps d and makes n smaller or equal, so the largest d, and the smallest n reaching it, lie among numbers made from the first primes (at most 9 of them, so primes up to 23; the search used primes up to 53).
- sigma: the same swap raises sigma(n) / n but also lowers n, and sigma(n') = (sigma(n') / n') * n' can still be smaller than sigma(n), so the swap does NOT show that the largest sigma lies among numbers of the smallest primes. The value above is therefore stated as measured over the full range, and the safety of u64 rests on the ratio bound alone.
- omega <= 9 (the largest is 9, first at 3234846615 = 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23 * 29) and Omega <= 31 (n >= 2^Omega, equality only at 2^31).

## Tests

Expected values come from the header's examples, from an in-test sieve to 2^22 (phi by `phi[m] -= phi[m] / p` over the primes found as the untouched entries, d and sigma by marking the multiples of every divisor, rad and omega by the prime sieve, Omega by marking the multiples of every prime power: no factorisation anywhere), from the definitions (phi as the count of k <= n with gcd(k, n) = 1 by a plain Euclid loop; d and sigma by listing the divisors), from closed forms for prime powers, squares and products of two primes, from relations that hold for every n (rad divides n; omega <= 9 and omega <= Omega <= 31; 2^omega <= d; phi <= n <= sigma; phi * sigma <= n^2; the sigma ratio bound), and from FNV-1a digests made by `tools/arithfun_ref.py`. Every call passes its result cells between guard words.

The reference is Python standard library only. For the exhaustive range it uses a smallest-prime-factor sieve to 2^22 and factorises by repeated lookup. For the large values it uses Miller-Rabin (bases 2, 7, 61, deterministic below 4759123141) and Pollard rho with Brent's cycle: no trial division and no sieve. For the definitions it counts gcds and lists divisors. Commands (all run; the timings are for one core): `examples` (the header, 0.01 s), `maxima` (the search, about 2 s), `gcd K` for K = 0..7 (the definitions for n in 8192 K + 1 .. 8192 (K + 1) against the factorisation route; 8 runs, about 3 min in all), `chunk K` and `sample K` (digests, 3 to 4 s each), `sets` (1 s), `emit` (all digests as C++, 46 s). The one command `check` runs the examples, all gcd shards, the maxima search and a stride comparison of the two factorisers; it takes about 4 minutes, and the pieces above were run separately. Digest: FNV-1a 64-bit over u64 words (n, phi, d, sigma, rad, omega, Omega for each n in increasing order).

Extents and local runtimes (gcc 11.4.0, `-O1`, ASan + UBSan, one core, base 375d0f4; the whole binary has 41 cases and runs about 3.3 minutes; the longest case is 12 s; none asserts time):
- `header examples`, `failure cases: null first, then n = 0, nothing written`, `n = 1 and the smallest values`, `maxima over the u32 range: the values at the maximisers` (each under 0.01 s). The failure case calls every function with a null output (and `prime_factor_counts` with either or both null) at n in {0, 1, 2, UINT32_MAX}, then n = 0 with valid cells, and checks that every cell and guard word is untouched.
- `chunk 0` to `chunk 7` vs sieve and digest (exhaustive: every n = 1..2^22 for all five functions, 2^19 per chunk, the last includes 2^22; 1.4 to 2.6 s each; sum 15.9 s).
- `definitions 0` to `7` (exhaustive: every n = 1..2^16 for phi by the gcd count and for d and sigma by the divisor list; the 8 cases split the n at 23170, 32768, 40132, 46341, 51810, 56755, 61305, 65536 so each has about equal work; 10.0 to 11.4 s each).
- `set: prime powers p^k below 2^32 for every prime p below 2^16` (13489 values, all p^k, so every p^2 included; closed forms and digest; 4.1 s); `set: the 4096 largest u32 values (includes the 100 largest primes)` (digest, relations, and every prime in the window, at least 100, against the prime formulas; 0.5 s); `set: semiprimes and squares near 2^16` (p * q and p^2 for the 16 largest primes below 2^16 and the 16 smallest above, 340 products below 2^32, closed forms and digest; 0.5 s); `set: multiples j * P_k (j <= 64) of the primorials P_1 to P_9` (472 values; under 0.01 s); `set: every n below 2^32 with 9 distinct prime factors` (4221 values, digest and omega = 9 for each; 0.01 s); `set: edge values` (234 values: 2^k, 2^k - 1, 2^k + 1, 10^k, 2^31 - 64 .. 2^31 + 64, and 3491888400, 4227022800, 4294967295, 4294967294, 4294967291, 3234846615, 4190266080; 0.01 s).
- `sample 0` to `sample 7` (sampled: 1000000 values in (2^22, 2^32) from the splitmix64 stream with state 0x9E3779B97F4A7C15, each step adding that constant and mixing; n = 2^22 + 1 + z mod (2^32 - 2^22 - 1); 125000 per shard, each shard against its digest and the relations; 8.4 to 8.6 s each).

The measured times vary by about a factor of two between runs on this machine (the sample shards took 4 to 9 s in other runs); the figures above are from one declaration-order run of the final file.

## Mutation testing

144 source mutants of `core/arithfun/arithfun.cpp` (script outside the repository), built with ASan + UBSan against the final tests (md5 89d1f07b3b512fb32a9dec27fb9aac96, not edited after the run), abort after the first failure, declaration order (`--order decl`). The whole suite was run as three selections of the same unedited file by tag: everything except `[def]` and `[sample]` (all mutants), then `[def]` and `[sample]` for every survivor of the first. Mutated: the take step (each of the six updates: operator, operand, the p, e and t arguments), the types in the struct, the initial values, the start, test, step and the exponent loop of the pass, the leftover step, the null and zero checks and their order and return codes in each public function, the written field, and the `prime_factor_counts` pointer checks and outputs.

- 134 killed by a failed assertion.
- 6 survivors on the whole suite, each equivalent:
  - `tk_phi_mul` (phi = phi / p * (p - 1) instead of phi -= phi / p): p divides phi at that point, so the two are equal.
  - `st_phi_u64` (phi stored as u64): the values never exceed n, and the output narrows to the same u32.
  - `st_d_u16` (d stored as u16): d <= 1920 over every u32.
  - `fa_w_e31` (extra test e < 31 in the exponent loop): after 31 divisions of a u32 by p >= 2, m is 1 and `m % p == 0` is already false.
  - `fa_pp_u32` (pp as u32): pp = p^e divides n, so pp < 2^32 and no wrap.
  - `fa_left_t32` (`m + 1u` in u32 for the leftover prime): a leftover is a prime at most 4294967291, so m + 1 does not wrap.
- 4 cost mutants (the values are right, only the time grows; kept apart from kills and equivalent survivors; no test can detect them because the repository forbids timing assertions). Observed at `-O2`, one call of `euler_phi` plus `divisor_sigma`: `fa_le_n` (the test p * p <= n instead of p * p <= m): about 0.0001 s for 4294967295, 2^31 and 4227022800 against under 0.000001 s for the source (the loop no longer stops once m is small); `fa_step1` (p += 1): 0.00023 s at 4294967291 against 0.00012 s (twice the trips); `fa_p_u32` (`p * p` in u32: it wraps at p = 65537 and the loop goes on, finding the prime itself at p = n): 7.44 s at 4294967291 against 0.00012 s; `fa_p_le_m` (the test p <= m): 7.50 s at 4294967291. The last two, and `fa_le_n` on the `[sample]` selection, also ran past the 40 s limit of the mutation run; they are not counted as kills.

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, the functions at every u32 (above 2^22 only the structured sets and the 1000000 sampled values are compared), the whole Python `check` as one command, and a mutant run on shuffled case order. The base for local evidence is 375d0f4; the files were rebased onto the current main for the pull request without a rerun. Nothing here is a CI result.
