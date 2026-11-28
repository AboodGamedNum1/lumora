# divsum: the divisor summatory functions

Code: `core/divsum/divsum.cpp` (contract in `core/divsum/divsum.h`). Tests: `tests/divsum/divsum_tests.cpp`. Reference: `tools/divsum_ref.py`. Evidence label: unit tests only.

## Rule

`divisor_count_sum(n, out)` sets `*out` to D(n) = the sum of d(k) for k = 1..n (d = the number of divisors), for every n up to and including 2^46; `divisor_sum_sum(n, out)` sets `*out` to S(n) = the sum of sigma(k) for k = 1..n (sigma = the sum of the divisors), for every u32 n. Both are 0 at n = 0. Both are u64.

- Check order for both: a null `out` is NullArgument; then, for `divisor_count_sum` only, n > 2^46 is TooLarge (2^46 itself is valid; 2^46 + 1 up to UINT64_MAX are TooLarge). `divisor_sum_sum` takes a u32, so it has no TooLarge path. Nothing is written to `*out` on a failure; each function writes only its own output.
- Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array. O(sqrt(n)) steps, O(1) memory.

## Method and proof

Let r = floor(sqrt(n)), so r^2 <= n < (r + 1)^2.

**D.** D(n) is the number of pairs (a, b) of positive integers with ab <= n (each k <= n has d(k) pairs with ab = k). Pairs with a <= r number the sum over a = 1..r of floor(n / a); pairs with b <= r number the same. Pairs with both a, b <= r number r^2 (all of them have ab <= r^2 <= n). No pair has both a > r and b > r (then ab >= (r + 1)^2 > n). Inclusion and exclusion: D(n) = 2 * (the sum over k = 1..r of floor(n / k)) - r^2.

**S.** S(n) is the sum over pairs (a, b) with ab <= n of a (a is a divisor of k = ab, and every divisor of k occurs once). Pairs with a <= r contribute the sum over a = 1..r of a * floor(n / a). For pairs with a > r: b <= n / a < n / r... exactly, a >= r + 1 gives b <= n / (r + 1) < r + 1, so b <= r. For each b <= r the allowed a run over r + 1 .. floor(n / b), which is non-empty because floor(n / b) >= floor(n / r) >= r (r^2 <= n); their sum is T(floor(n / b)) - T(r) with T(m) = m (m + 1) / 2. Hence S(n) = (the sum over a = 1..r of a * floor(n / a)) + (the sum over b = 1..r of (T(floor(n / b)) - T(r))).

**Source.** One loop `k * k <= n` finds r (the last k) and sums floor(n / k) (for D) or k * floor(n / k) (for S); S has a second loop over b = 1..r. T(m) is computed as (m / 2) * (m + 1) for even m and m * ((m + 1) / 2) for odd m, so m * (m + 1) is never formed.

**Cost.** D at n = 2^46: r = 2^23 = 8388608 iterations of one 64-bit division. S at n = 2^32 - 1: r = 65535, so 65535 iterations of the first loop and 65535 of the second (131070 in all).

## Bounds in u64

D. D(n) = the sum over k <= n of floor(n / k) <= n (1 + ln n); for n = 2^46 that is below 2^46 * 32.9 = 2.3e15, under 2^52 = 4.5e15 (the header's bound). The source forms 2 * sum (sum over k <= r of floor(n / k) <= n (1 + ln r) < 1.2e15, so doubled < 2.4e15 < 2^52) and then subtracts r^2 <= n; D(n) >= r^2 holds because the doubled sum is at least 2 r^2 (each of the r terms is at least r). k * k <= 2^46 + 2^24 in u64. Measured at n = 2^46: D = 2254558383554734, and 2 * sum = D + r^2 = 2.325e15 (below 2^52).

S. Group the pairs by b instead: S(n) = the sum over b = 1..n of T(floor(n / b)). With T(m) <= m (m + 1) / 2 and m = floor(n / b) <= n / b: T <= n^2 / (2 b^2) + n / (2 b). Summing over b, S(n) <= (n^2 / 2) * (the sum of 1 / b^2, which is below pi^2 / 6) + (n / 2) * H(n), with H(n) <= 1 + ln n, so S(n) < (pi^2 / 12) n^2 + n (1 + ln n) / 2. At n = 2^32 - 1: (pi^2 / 12) n^2 = 0.822467 * 1.8446744e19 = 1.51717e19, plus about 5e10, under 2^64 = 1.8446744e19 (17.8 percent room). The measured value is S(2^32 - 1) = 15171838869443902239. Intermediates of the source: the first sum is at most n * r < 2^48 (each term k * floor(n / k) <= n); the second sum has terms T(floor(n / b)) - T(r) that are non-negative for b <= r (floor(n / b) >= r), so the subtraction cannot wrap, each partial sum is at most the final second sum, and first + second = S(n) < 2^64. T(m) for a u32 m is at most T(2^32 - 1) = 9223372034707292160 < 2^63 (so every call of T is exact in u64); T(2^32) = 2^63 + 2^31 is the first T past 2^63. S itself passes 2^63 after n = 3348772261: S(3348772261) = 9223372031893649489 <= 2^63 - 1 and S(3348772262) = 9223372037611712081 >= 2^63 (found by bisection with the block route; the test asserts both values from the implementation).

## Tests

Expected values come from the header's table, an in-test divisor sieve (d(k) and sigma(k) by marking the multiples of every d, with prefix sums, to 2^20), the in-test quotient-block route, direct O(n) sums, the difference certificate (D(n) - D(n - 1) = d(n) and S(n) - S(n - 1) = sigma(n), with d and sigma by in-test trial division), and digests and tables from `tools/divsum_ref.py`.

The reference is Python standard library only. It gets D and S by the definition (a divisor sieve, running sums) and, for large n, by the quotient-block route: for k = 1.. take q = n // k and hi = n // q (every k' in [k, hi] has floor(n / k') = q), add q * (hi - k + 1) to D and q * (T(hi) - T(k - 1)) to S, set k = hi + 1; about 2 sqrt(n) blocks. That is a different method from the source's split at sqrt(n). The certificate uses d and sigma from the factorisation by trial division over a prime list (every third entry of each large list in Python). `python3 tools/divsum_ref.py brute` compares the block route with the definition for every n <= 2^16 and with the direct sums for every n <= 2^13 and 40 larger n; `large` runs the lists and the 2^63 threshold; `emit` prints the C++ golden values; `check` runs everything (about 40 s warm; `DIVSUM_CACHE=file` caches the block route, whose cold run takes about 4 minutes, mostly the entries near 2^46). Digest: FNV-1a 64-bit over u64 words (n, D(n), S(n) in increasing order).

The exact large values asserted by the tests, D(2^46) = 2254558383554734, D(2^46 - 1) = 2254558383554687, D(10^13) = 300880375389757 and S(2^32 - 1) = 15171838869443902239, were first reported by the maintainer (S(2^32 - 1) is also in the header); the reference's block route reproduces them (`large` asserts them), an ad hoc Python run of the source's split (not kept in the repo) gave the same D(2^46), D(10^13) is asserted the same way, and the test checks them against the block route in C++.

Extents and local runtimes (gcc 11.4, `-O1`, ASan + UBSan, one core; the whole binary, 31 cases and 97 assertions, takes about 30 s; the longest case is 2.6 s; none asserts time). Each call passes its result cell between guard words.
- `header table` (0.00 s): the header's D and S examples at 0, 1, 2, 3, 4, 5, 10, 100, 1000, 10^6, the two constants, S(2^32 - 1).
- `exact large values: D(2^46), D(2^46 - 1), D(10^13)` (0.09 s).
- `failure cases: null first, TooLarge above 2^46, nothing written` (0.03 s): a null `out` with D at n in {0, 1, 2^32 - 1, 2^46, 2^46 + 1, UINT64_MAX} and S at {0, 1, 2^31, UINT32_MAX}; D at 2^46 + 1, 2^47, 2^63, UINT64_MAX with a valid `out` gives TooLarge, sentinel and guard words intact (the TooLarge path does no loop); n = 0 gives Ok with 0; each function leaves the other's cell alone.
- `sieve chunk 0` to `7` (exhaustive over every n = 0..2^20 for both functions, 2^17 values per chunk, the last includes 2^20; 0.3 to 1.2 s each) and `digest chunk 0` to `7` (the same n against the Python digests; 0.3 to 1.2 s each).
- `windows around r^2 and r^2 + r for every r = 1..4096` (exhaustive over the 24576 values n = r^2 - 1, r^2, r^2 + 1, r^2 + r - 1, r^2 + r, r^2 + r + 1, both functions against the block route; 2.3 s) and `windows at large r (sampled)` (D at r in {65535, 65536, 46341, 2^20, 2^22 - 1, 2^22, 2^23 - 1, 2^23} wherever n <= 2^46, S at r in {65535, 46341, 32768}; 2.6 s).
- `difference certificate for every n = 1..2^16` (exhaustive; 0.14 s), `direct O(n) sums for every n = 0..2^13` (exhaustive; 0.07 s), `direct O(n) sums, 150 sampled n up to 2^22 (part 1)` and `(part 2)` (300 sampled in all; 0.8 s and 0.7 s).
- `large D values part 0` to `3` (445 values of D, sampled, up to 2^46; 2.2 to 2.4 s each): 10^k for k = 0..13; 2^j - 1, 2^j, 2^j + 1 for j <= 46; 2^46 - 1, 2^46; UINT32_MAX and its neighbours, 2^31 and 2^31 - 1; the r^2 and r^2 + r windows for r in {2^23, 2^23 - 1, 2^22, 2^22 - 1, 3000000, 2^20, 65536, 65535, 46341, 100000}; the 6 largest primes below 2^46 and below 2^32; 3491888400, 4227022800, 4219365150, 4132042525; primorials; 200 xorshift64 values with log-uniform size from 2^20 to 2^46; 30 random u32 values. Each against the table and the block route, and every 8th also the certificate (by trial division to 2^23).
- `large S values against the table, the block route and the certificate` (360 values of S up to UINT32_MAX, sampled; 0.3 s; the certificate on every entry): 10^k for k <= 9; 2^j - 1, 2^j, 2^j + 1; UINT32_MAX and its neighbour, 2^31 and 2^31 - 1; the r^2 and r^2 + r windows for r in {65535, 65534, 46341, 46340, 32768, 30000, 4096, 1000}; the 8 largest u32 primes; three highly composite numbers; primorials; kS63Last - 1, kS63Last, kS63Last + 1; 150 random u32 values and 50 random values below 2^24.
- `S crosses 2^63 after kS63Last; T(u32max) < 2^63 <= T(2^32)` (0.00 s).

## Mutation testing

94 source mutants of `core/divsum/divsum.cpp` (script outside the repo), built with ASan + UBSan against the final tests (md5 57df75d348a89331e51663d3a56671a0, not edited after the run), abort after the first failure, every mutant run on the whole suite. The test binary shuffles the case order (the Catch2 main in this repository), so the order differs between runs; the run used a 50 to 75 s limit because a survivor needs about 30 s. Mutated: the T helper (parity test, halving, the +1 and widths), the null check and its order, the TooLarge test and its constant and result code, the loop start, bound, step and width, the term, the sign and the sum, the value of r, the output formula and the return codes.

- 81 killed by a failed assertion in the shuffled-order run.
- 7 crash-only: `d_null_rm`, `s_null_rm` (null dereference); `d_k0`, `s_k0`, `s_b0` (the loop starts at 0: division by zero, reported by UBSan); `d_k32`, `s_k32` (u32 `k`: `k * k` wraps, the loop test stays true until `k` wraps to 0 and the division by zero is reported; at n = 2^46 for D and at u32 n near 2^32 for S).
- 4 timeouts in the shuffled-order run, not counted as kills there: `d_kn`, `d_khalf`, `s_kn`, `s_khalf` (loop bounds `k <= n` and `k * 2 <= n`). They do NOT keep the value: r becomes n (or n / 2), so the result is wrong from n = 2 on, but the shuffled order ran a huge call (up to 2^46 or 2^32 - 1, which these loops cannot finish) before any assertion. Rerun on the whole suite of the same unedited file in declaration order (`--order decl`, otherwise identical), all four are killed by a failed assertion. They are counted as killed, with this disclosure (81 killed by assertion in the shuffled run, plus these 4 in the rerun: 85).
- 2 survivors on the whole suite, each equivalent:
  - `t_p2_o` (`(m + 2) / 2` instead of `(m + 1) / 2` for odd m): m + 1 is even, so (m + 2) / 2 and (m + 1) / 2 are equal for odd m.
  - `s_b32` (u32 `b` in the second loop): b <= r <= 65535, so a u32 holds it and `n / b` is the same.
- No pure cost mutant (same values, only slower) was among the 94: every loop-bound change that makes the loop longer also changes r and so changes the value, which is why those are killed. The observed runtimes at -O2 of the four loop-bound mutants, for the record: `d_kn` at n = 10^9 takes 2.9 s (29 s at 10^10) and returns a wrong value; `d_khalf` at 10^9 takes 1.4 s; `s_khalf` at UINT32_MAX takes 12.5 s; `s_kn` takes 25 s. The original takes under 0.01 s at those n. The repository forbids timing assertions, so no test would detect a slowdown that kept the value; none was found here.

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, D or S beyond the sampled large values (no exhaustive sweep above 2^20), and a mutant run with a fixed test order for the whole set (only the four above). Nothing here is a CI result.
