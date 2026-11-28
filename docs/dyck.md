# dyck: ranking Dyck words of semilength up to 32 as u64 masks

Code: `core/dyck/dyck.cpp` (contract in `core/dyck/dyck.h`). Tests: `tests/dyck/dyck_tests.cpp`. Reference: `tools/dyck_ref.py`. Evidence label: unit tests only.

## Rule

- A word of semilength n (0 to 32) has 2n characters and is a mask: bit 2n - 1 is the first character, bit 0 the last, '(' is 0 and ')' is 1. The rank is the position in lexicographic order with '(' before ')', which is the numeric order of the masks. C_n words, ranks 0 to C_n - 1, C_32 = 55534064877048198.
- `dyck_count`, `dyck_rank`, `dyck_unrank`, `dyck_next`. Check order: a null `out` is NullArgument; n above 32 is BadArgument; then (rank and next) an invalid mask is BadMask, which comes before NoNext; then (unrank) rank >= C_n is RankTooLarge; then (next) the last word is NoNext. Nothing is written to `*out` on a failure. A mask is valid when it has no bit at 2n or above and is a balanced word.
- Pure integer. No heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics in the implementation. The cost per call is O(n) loop steps, each with one gcd of two small numbers.

## Method

- B(r, k) is the number of ways to finish r remaining characters from k open parentheses (walks of r steps from height k to 0 that never go below 0). The code carries `cur` = B(pos, depth), where pos is the number of characters left, and starts with cur = C_n.
- `up_count(pos, depth, cur)` is B(pos - 1, depth + 1), the number of completions that put '(' next. With m = (pos - depth) / 2: up = cur * (depth + 2) * m / (pos * (depth + 1)), from B(r, k) = (k + 1) / (r + 1) * C(r + 1, (r - k) / 2). It is computed as (cur / g) * (num / (den / g)) with num = (depth + 2) m, den = pos (depth + 1), g = gcd(cur, den).
- Rank: at each character, if it is '(' then cur = up; if it is ')' then rank += up, cur -= up. Unrank is the inverse: ')' is chosen when rank >= up (then rank -= up, cur -= up), otherwise '(' and cur = up. Bit pos - 1 is set for ')'.
- Valid: the shift by 2n is taken only for n below 32, and the scan runs over positions 2n down to 1 with a depth counter. The shift is never by 64.
- Next: the rightmost '(' that has an open parenthesis before it (depth at least 1 counted from the right of the scan) becomes ')'; the rest is the least completion: opens = (pos - depth) / 2 '(' and then closes = depth - 1 + opens ')'. If there is no such '(' the word is the last (NoNext).
- Count: C_(i+1) = C_i * 2 (2i + 1) / (i + 2).

## Proofs

**The recurrence and the formula.** B(r, k) = B(r - 1, k + 1) + B(r - 1, k - 1) (the first character is '(' or ')'; the second term is 0 at k = 0), B(0, 0) = 1. Rank is the number of words that have the same prefix and a '(' where the word has ')'; that count is B(pos - 1, depth + 1) at each ')' of the word, so rank = the sum of up over the ')' positions. After '(' the completions that remain are exactly the up ones, so cur = up; after ')' they are cur - up. At the end cur = B(0, 0) = 1. At depth 0 the formula gives up = cur (num = 2 pos / 2 = pos, den = pos). It gives 0 when m = 0.

**Exactness.** B(pos - 1, depth + 1) is an integer, so up = cur * num / den is an integer. With g = gcd(cur, den), gcd(cur / g, den / g) = 1 and (den / g) divides (cur / g) * num, hence divides num. So num / (den / g) is exact, and cur / g * (num / (den / g)) = up with no rounding.

**No intermediate reaches 2^64, for every n up to 32.**
- cur is B(pos, depth) for a prefix of some word of semilength n, which is at most the number of words C_n at the start and is non-increasing (up <= cur, since up counts a part of the completions); so cur <= C_32 = 55534064877048198 < 2^56.
- Every value computed from cur in `up_count` is at most cur: cur / g <= cur, up <= cur, and the product (cur / g) * (num / (den / g)) equals up. The factors never exceed cur / g <= cur and num / (den / g) <= num.
- pos <= 64 and depth <= 32 (a valid word of semilength 32 never has more than 32 open parentheses), m <= 32, so num = (depth + 2) m <= 34 * 32 = 1088 and den = pos (depth + 1) <= 64 * 33 = 2112. These are small.
- rank (in `dyck_rank`) is a sum of up values over the ')' positions, and by the first proof it counts words below the given word, so it is below C_n <= 2^56 at all times. In `dyck_unrank` rank is only decreased (rank -= up when rank >= up) and the cur is decreased, so both stay at most their starting values. The largest state of both functions is therefore cur = C_32 at the first step (rank: up = B(63, 1) <= C_32; unrank: the same).
- Catalan: C_(i+1) = C_i * 2 (2i + 1) / (i + 2). The product is at most C_31 * 126 = 14544636039226909 * 126 = 1832624140942590534 < 2^61 = 2305843009213693952; for smaller i the product is smaller, since C_i * 2(2i + 1) grows with i. The division by (i + 2) is exact (it is the Catalan recurrence).
- Shifts: `valid` shifts by 2n only when n < 32 (at most 62), by pos - 1 <= 63 elsewhere; `dyck_next` shifts by best_pos - 1 <= 63 and by closes. closes counts ')' that are not the flipped one in a word that has exactly n of them, so closes <= n - 1 <= 31, and the shift by closes is below 32 (a 32-bit shift would give the same result).
- gcd: `x % y` on u64; the first step reduces cur to below den <= 2112, so the loop has at most about 15 rounds (measured below).

**Next.** Let the word w have ')' at positions that make it not the last word. The next word in lexicographic order keeps the longest possible prefix of w, so it changes the rightmost '(' that can become ')' (it needs depth >= 1 so that the prefix stays valid, i.e. there must be an open parenthesis before it), and the suffix is the least valid completion: '(' as long as they fit, then ')'. With best_depth the depth when scanning from the right and best_pos the position, the suffix has opens = (best_pos - best_depth) / 2 '(' and closes = best_depth - 1 + opens ')' (the flipped one accounts for the - 1). The check against the test oracle (unrank of rank + 1) covers every word for n up to 15.

## Tests

Expected values come from the header examples, from `tools/dyck_ref.py` (Python standard library only, no code shared with the library) as FNV-1a 64-bit digests, and from in-test oracles of a different method than the library: a Pascal-table reflection ballot count and Catalan, a counting validity check, and the successor as unrank(rank + 1). The reference computes words and ranks by brute force (every word as a string, sorted with '(' < ')') for n up to 10, by a memoised dynamic program f(remaining, depth) with Python integers for every n up to 32, Catalan numbers by `math.comb`, and per-position bit counts of the whole set by prefix times suffix. Every case checks guard words around the output cell and that nothing is written on a failure.

Local runtimes (gcc 11.4.0, `-O1`, ASan + UBSan, one core; base c730c34; the whole binary has 14 test cases and 69 assertions and runs in about 24 s; every case is under 30 s, the largest is 15.62 s). Cases are declared cheap first:

- `dyck: header examples`, 0.000 s: every example in `dyck.h` for the four functions.
- `dyck: check order and no write on failure`, 0.000 s: every combination of null output, n above 32 and invalid masks, and the order of the codes.
- `dyck: D3 counts n = 0..32`, 0.000 s (33 values, digest; C_(n+1) = sum C_i C_(n-i) for n up to 29; n = 33 is BadArgument).
- `dyck: D6 edge ranks for n = 0..32`, 0.008 s (4587 cases, digest): ranks 0, 1, 2, C_n - 2, C_n - 1, C_n, C_n + 1, C_n + 2, C_n / 2 - 1, C_n / 2, C_n / 2 + 1, 2^64 - 1, 2^64 - 2, and 2^k - 1 and 2^k for k = 1 to 63.
- `dyck: D8 chains at both ends of every n`, 0.004 s (1220 cases, digest): the first and last 21 ranks of every n from 1 to 32 (unrank, rank, next), and the chain by next from the 21st word before the end to the last word.
- `dyck: boundary bits at 2n, 63 and every single flip, n = 0..32`, 0.001 s: for ranks 0, C_n - 1 and C_n / 2 of every n, every bit from 2n to 63 set on a valid word is BadMask (this includes n = 32 with bit 63, where the shift by 2n is never taken), every single flip of bits 0 to 63, the first character closed, and the masks 2^64 - 1, bit 63, the full 2n-bit mask and that mask shifted right by one.
- `dyck: D7a sampled ranks, n = 17..32`, 1.17 s: for each n from 17 to 32, 12500 ranks drawn by a seeded SplitMix64 (200000 cases, digest).
- `dyck: D7b near-valid masks, n = 17..32`, 0.029 s: for each n, 1250 masks made from a valid word by edit kind i mod 10 (cycled by case index, so 125 per kind per n and 2000 per kind in all): 0 one bit flipped; 1 two bits exchanged; 2 a bit set at 2n or above (for n = 32, bit 63 flipped); 3 the all-ones 2n-bit mask; 4 the mask 0; 5 `0x5555...` cut to 2n bits; 6 `0xAAAA...` cut to 2n bits; 7 the word complemented in its 2n bits; 8 two flips; 9 the word shifted right by one. 20000 cases, digest; the valid and invalid counts by kind are below.
- `dyck: D7c random masks, n = 17..32`, 0.007 s: for each n, 3125 masks of SplitMix64 draws, every fifth a full 64-bit draw (625 per n) and the others cut to 2n bits (2500 per n); 50000 cases, digest.
- `dyck: D4 every mask below 2^(2n), n = 0..9`, 0.036 s: every mask below 2^(2n) for n = 0 to 9, plus for each valid word three masks with high bits set (bit 2n, bit 63, bit 2n + 3 or 63); 370279 cases, digest; the ranks of the valid words in numeric order are 0, 1, 2 and so on, and the count of valid masks is C_n.
- `dyck: D5 walks n = 0..12`, 0.369 s; `D5 walk n = 13`, 1.03 s; `D5 walk n = 14`, 4.03 s; `D5 walk n = 15`, 15.62 s: every word of those n in order (290512, 742900, 2674440 and 9694845 words visited by the four walks in the same order as the cases). Per word: valid by the oracle, strict increase of the mask, rank = index, unrank(index) = the word, next = the following word, NoNext at the end, and a word count of C_n. Digest per n: the n, C_n, the per-position bit counts of the whole set and the sum of the masks.

D7 exact counts. D7a: 16 values of n (17 to 32) x 12500 = 200000 valid words. D7b: 16 x 1250 = 20000 masks, 125 per kind per n (2000 per kind in all). The real run result by kind over the 20000: kind 0, 2, 3, 4, 6, 7 and 9 give 2000 invalid masks each; kind 1 gives 1718 valid and 282 invalid; kind 5 gives 2000 valid; kind 8 gives 722 valid and 1278 invalid. D7c: 16 x 3125 = 50000 masks, of which 10000 are full 64-bit draws (4 valid, 9996 invalid) and 40000 are cut to 2n bits (198 valid, 39802 invalid).

The reference commands, all run: `check` and `emit_part 0` to `2` (4 s, 11 s and 2 s). `check` verifies the dynamic program against the brute-force sorted list (n up to 10), the Catalan numbers against `math.comb`, the header examples, and the bit counts against the walk.

Measurement of the library (a local build outside the repository: counters added to the gcd loop, to `up_count` and to the Catalan product; the whole test binary under ASan + UBSan; 814173880 `up_count` calls): at most 7 gcd rounds; largest cur 55534064877048198 (= C_32); largest den 1056; largest num 289; largest up 55534064877048198; largest cur / g 27767032438524099; largest Catalan product 1832624140942590534 (below 2^61); the division was exact every time and up never exceeded cur (0 exceptions). These are measurements over the tests; the bounds above are the arguments.

## Mutation testing

252 source mutants of `core/dyck/dyck.cpp` (script outside the repository, grouped by function: gcd, up_count, valid, catalan, rank, unrank, next and the check order, shift width, ballot index and the output), each built with ASan + UBSan against the final tests (md5 `b373db0801b293b933b4f3c00f9be835`; the library md5 is `a3103c3ffe01c11b282bba4569b6fe9a`; neither was edited after the run; the reference md5 is `6df630d053cff73261131b45f96ccffa`), abort after the first failure, declaration order. The first pass ran three builds at a time on one core with a 45 s limit. Every timeout and every survivor was rerun alone (40 to 45 s limit, one at a time); the loop probe ran alone; the cost mutants were timed alone.

First pass: 198 killed by a failed assertion, 31 by an abnormal end (a sanitizer abort or a crash) = 229 killed; 2 survived; 21 timed out (6 cost mutants and 15 others). The 15 others, rerun alone: `k_n_none` was killed; `g_sub` and `n_order` still timed out; the other 13 survived. So the 252 mutants are: 230 killed (the 229 of the first pass and `k_n_none`; this includes the cost-type mutant `cm_rank_twice`), 2 step-limited, 14 survivors (the 2 of the first pass and 12 of the 15 non-cost timeouts), and 6 cost mutants that are not killed.

- 2 step-limited, not counted as kills: `g_sub` (gcd by subtraction, `x - y`) and `n_order` (the check n above 32 moved after the call to `catalan(n)`, which then loops n times for a large n). A probe build with a counter on every loop passed 200000 trips in one entry in both ("step-limited, non-termination not proven"). These are step-limit hits, not proofs. For `n_order` the loop in `catalan` runs n times, up to 2^32 - 1, so the work is large but finite for any one call. For `g_sub` no termination argument is given.
- 14 survivors, each equivalent (the result of every function is the same for every input):
  - `g_xy` (an early return of 1 when the remainder is 1): gcd(y, 1) = 1, so the result is the same.
  - `u_num_m1`, `x_opens_p`, `x_best_d1`: pos - depth is even at every use (the number of characters left and the depth have the same parity, since a valid word has 2n characters), so (pos - depth + 1) / 2 = (pos - depth) / 2 in integer division. For `x_best_d1` (best_depth + 1): opens becomes (best_pos - best_depth - 1) / 2 = opens - 1 (the difference is even, so it rounds down by one) and closes = best_depth + 1 - 1 + opens - 1 = best_depth - 1 + opens, the same as before.
  - `v_end_le` (depth <= 1 for depth == 0 at the end): the final depth has the parity of 2n, so it is even and never 1.
  - `r_loop_gt1` (rank loop stops before position 1): the last character of a valid word is ')' and at pos = 1 the depth is 1, so m = 0 and up = 0 (rank += 0, cur -= 0); the loop step changes nothing.
  - `r_out_cur` (rank + cur - 1): at the end cur = B(0, 0) = 1.
  - `n_mask_and` (xor for or in unrank): the bit is not yet set when it is written.
  - `x_loop_gt1` (the next scan stops before position 1) and `x_nonext_lt` (best_pos <= 1 for == 0): position 1 holds ')' in every valid word, so it is never a candidate and best_pos is never 1.
  - `x_high_pos` (`(mask >> best_pos) << best_pos`): the bit best_pos - 1 is '(' (0), so clearing it changes nothing.
  - `x_out_xor`, `x_out_plus` (xor or plus for or in the result): the three parts are high bits, the flipped bit (0 in `high`) and the low bits below it; their bit sets are disjoint, so or, xor and plus agree.
  - `x_low_u32` (the low mask built with a 32-bit shift): closes <= n - 1 <= 31 as shown above, so the 32-bit shift gives the same value.
- 6 cost mutants not killed (the values are right, only the time grows; kept apart from the kills and the survivors; no test can detect them because the repository forbids timing assertions). Observed, whole suite alone, one core, gcc ASan + UBSan: `cm_gcd_rep` (40 extra divisions per round) passes in about 68 s (the elapsed time of the call); `cm_up_rep` (40 extra gcd calls per step) 24 s; `cm_valid_rep` (40 extra scans in `valid`) 33 s; `cm_cat_rep` (400 extra multiplications in `catalan`) 25 s; `cm_next_scan` (40 extra `valid` calls in next) 35 s; `cm_gcd_sub` (subtractive remainder) did not finish in 55 s and was stopped (Catch2 prints the SIGTERM as a failure; that is the timeout, not a kill). `cm_rank_twice` (40 calls of `dyck_rank` with n = 0 added) was killed and is counted among the kills. The unmutated binary takes about 24 s.

## Not run

Other builds than the local gcc 11.4 ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, and a mutant run on shuffled case order. The walks cover every word for n up to 15 only; for n from 17 to 32 the checks are the edge ranks, the chains at both ends, the boundary bits and the seeded samples above (no whole walk). Aliasing is not applicable (one output).
