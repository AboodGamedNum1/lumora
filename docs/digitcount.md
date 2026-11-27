# digitcount: digit sums in a positional base

Code: `core/digitcount/digitcount.cpp` (contract in `core/digitcount/digitcount.h`). Tests: `tests/digitcount/digitcount_tests.cpp`. Reference: `tools/digitcount_ref.py`. Evidence label: unit tests only.

## Rule

The digit sum of `x` in base `b` is the sum of its base-`b` digits (0 for `x = 0`). `kMaxN` = 2^40, bases 2 to 64.

- `count_digit_sum(n, base, s, out)`: `*out` = the number of integers `x` with `0 <= x <= n` whose digit sum in `base` is `s`. `s` is any u64; an unattainable `s`, also the maximum u64, gives 0. `n = 0` gives 1 for `s = 0` and 0 otherwise.
- `digit_sum_total(n, base, out)`: `*out` = the sum of the digit sums of every `x` in `0 .. n`. `n = 0` gives 0.
- Check order for both: a null `out` is NullArgument; then a base below 2 or above 64 is BadBase; then `n > kMaxN` is TooLarge. `n = 2^40` and `n = UINT32_MAX` are legal. Nothing is written to `*out` on a failure. `s` is never validated.

Integer only: no floating point, no 128-bit type, no builtin or intrinsic, no heap, no recursion, no static table, no variable-length array.

## Method

Notation: `b` the base, `k` the number of digits of `n` (0 for `n = 0`), `d[k-1] .. d[0]` its digits, `W(i, t)` the number of strings of `i` digits (each below `b`) with sum `t`, so `W(0, .) = [1]` and `W(i + 1, .)` is `W(i, .)` times `1 + x + ... + x^(b-1)`.

`count_digit_sum`: every `x` in `0 .. n` is either `n` itself, or agrees with `n` above some position `i`, has a digit `c < d[i]` at position `i`, and any `i` digits below (leading zeros included, so `x = 0` is one of these). These cases are disjoint and cover everything, so with `pre(i)` the sum of the digits of `n` above position `i`:

    count(n, s) = [digit sum of n == s] + sum over i of sum over c < d[i] of W(i, s - pre(i) - c).

The positions are visited from the bottom while `W(i, .)` is built in one array in place: form the prefix sums of `W(i, .)`, then `W(i + 1, t) = prefix(t) - prefix(t - b)` for `t` from the top down, which reads only entries not yet overwritten. The cost is O(k * (b + cap)) with `cap = (b - 1) * k`, within the header's O(digits * base * limit). A value of `s` above `cap` returns 0 at once; that test is a shortcut only (the loop would also give 0, because an index above the current maximum `i * (b - 1)` is never read).

`digit_sum_total`: for the position of weight `p = b^i` the `b * p` integers of one block hold every digit exactly `p` times. The `n + 1` integers `0 .. n` are `f = (n + 1) / (b * p)` full blocks plus `r = (n + 1) % (b * p)` more: `q = r / p` complete runs of `p` integers for the digits `0 .. q - 1` and `r % p` integers with the digit `q`. The position contributes `f * p * (b * (b - 1) / 2) + p * (q * (q - 1) / 2) + q * (r % p)`; positions run while `p <= n`.

Bound of every intermediate (all quantities are non-negative, and each sum below adds terms that are each at most the final value of the quantity):
- `n + 1 <= 2^40 + 1`; `b * p <= 2^46` for every visited position (`p <= n`).
- `W(i, t) <= b^i <= 2^40` for `i < k`, and the prefix sums of `W(i, .)` are at most `b^i`, so every array entry is at most 2^40.
- `count(n, s) <= n + 1`.
- A position contributes at most `(n + 1) * (b - 1)`, so the total is below `2^40 * 41 * 63` (more exactly at most 41 * 63 * (2^40 + 1) < 2^52). Nothing wraps.

Working memory: one array of 384 u64 words (the largest index used is `(k - 1) * (b - 1) <= 378`, reached for `b = 64`, `k = 7`, so 379 words are needed) and one array of 41 u32 digits (41 digits for base 2 at 2^40), at most 425 words counting each digit as a word, below the 512 words of the contract.

## Tests

Expected values come from an in-test scan (running counts of the digit sum of every `x` by repeated division), from digests made by `tools/digitcount_ref.py`, and from identities that do not use the method above. The reference (Python standard library only; `python3 tools/digitcount_ref.py check` takes about 7 s locally) has two oracles. The scan oracle (the digit sum of every `x` in `0..n` counted one by one) is THE oracle on the small extents. It is infeasible at `n = 2^40` (about 10^12 integers), so the digests of the large inputs do NOT come from the scan: they come from the second oracle, a big-integer generating function (the count polynomial of `n` packed into one Python integer, 64 bits per coefficient; `G = 1 + x + ... + x^(b-1)`; the sum over positions of `x^pre(i) * (1 + ... + x^(d_i - 1)) * G^i`, plus `x^(digit sum of n)`). `check` compares the two oracles for every base 2 to 64 at every `n` up to 1500, on a stride of 97 up to 2^16 and on the neighbours of every power of the base up to 2^16 (with the scan run once over all `x` up to 2^16 per base), and checks the identities below on every large input of the digests. The digests are FNV-1a over 64-bit words: for one `(n, b)` the counts for `s = 0 .. (b - 1) * k + 1`, then the total, folded in the stated order.

Extents, named as in the tests. Runtimes are local, from a gcc 11.4 `-O1` build with ASan and UBSan on one core; the whole binary took about 13 s; no case is near 30 s and none asserts time:
- `exhaustive count_digit_sum for bases 2 to 16 with n 0 to 2000 and bases 17 to 64 with n 0 to 300, every s, against the scan` (exhaustive; about 1.0 s): every `s` from 0 to `cap + 1`, plus the sum of the counts (`n + 1`), the sum of `s * count` (the scan total), and the largest `s` with a non-zero count (the scan's maximum digit sum), and the total against the scan.
- `exhaustive digit_sum_total for every base 2 to 64 and every n 0 to 2^18 against a running scan` (exhaustive; about 1.0 s).
- `reference digests of the scan for n 0 to 200 in every base` (exhaustive vs the scan oracle's digests; about 0.4 s).
- `boundaries b^k - 1, b^k, b^k + 1 up to 2^40 and 2^40 - 1, 2^40 in every base` (sampled; about 2.1 s): reference digests (generating function) and, at every one of those `n`, the identities: the counts add up to `n + 1`, the sum of `s * count` equals `digit_sum_total`, `count(n, s) - count(n - 1, s)` is 1 exactly at the digit sum of `n` and 0 elsewhere, `total(n) - total(n - 1)` is the digit sum of `n`, the counts are symmetric under `s -> k (b - 1) - s` at `n = b^k - 1`, and the count for `s = 0` is exactly 1 (also at `n = 0` and `n = b^k`).
- `forty random n up to 2^40 per base` (sampled; about 8.6 s, the slowest): xorshift64 seeds in the reference; the same digests and identities.
- `hand-checked values and closed forms` (base 10 and base 2 small tables, `n = 0` in every base, `n = 2^40` in base 2 (41 powers of two for `s = 1`, total `40 * 2^39 + 1`), and `total(b^k - 1) = k * b^(k-1) * b * (b - 1) / 2` for every base and power up to 2^40).
- `failure cases`: every null `out` (null first) with bad bases, valid bases, every `n` and every `s`; bad bases 0, 1, 65, 66, 128, UINT32_MAX with valid and oversize `n` (base before size); `2^40 + 1`, `2^40 + 2`, UINT64_MAX with every valid base; `2^40` and UINT32_MAX legal; unattainable and huge `s` (up to UINT64_MAX) give 0; a sentinel in `*out` compared after every failure.

Every call passes a cell between guard words, pre-filled with a sentinel.

Deviation, stated plainly: the digests for the boundary and random `n` rest on the generating function, not on the scan, because the scan cannot reach `2^40`. The exhaustive extents against the scan (all `s` for bases 2 to 16 up to `n = 2000` and bases 17 to 64 up to `n = 300`; totals up to `2^18`) are the only places the definition itself is run against the implementation.

## Mutation testing

84 source mutants of `core/digitcount/digitcount.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (the test file was not edited after the run; md5 a063546990a2c430c29ed922b8c91c54). Each ran with abort-after-first-failure under a 90 s limit. A killed mutant ends with a printed failed assertion; one that ends in a sanitizer abort without one is listed as crash-only. A timeout is not counted as a kill.

- 68 killed by a failed assertion.
- 6 crash-only: `c_top_rm`, `c_top_def`, `c_top_def2` (the upper index limit of `W` dropped or enlarged, so an index beyond the array is read), `c_ext_cond` (the array extended once too often), `g_ways_378` (the array one word short), `g_digits_40` (the digit array one short for base 2).
- 2 timeouts, not counted as kills: `t_block` (`block = p * (b - 1)`) and `t_pnext` (`p = p * (b - 1)`). They were run alone, and classified only by a repeating state: with a probe added, `digit_sum_total(5, 2)` showed the loop variable `p` (the only variable of the loop) unchanged for four iterations in a row, since for base 2 `p * (b - 1) = p`, so the loop never ends.
- 8 survivors, each equivalent:
  - `c_cap_p1`, `c_cap_rm`, `c_cap_dig` (the early return `s > cap` loosened by one, removed, or `cap` computed with `k + 1`): the early return is a shortcut only. Without it the loop adds `W(i, s - pre - c)` only for `s - pre - c <= i * (b - 1)` and the test for the digit sum of `n` itself; for an `s` above the largest digit sum of any `x <= n` every term is 0, so the result is 0 either way. Larger `s` never index the array.
  - `c_ge_rm` (`s >= used` dropped): if `s < used` the unsigned difference `s - used` wraps to at least `2^64 - 441`, which is above `top`, so the next test rejects it anyway.
  - `c_nhi` (the extension runs one index higher): the extra entry is computed as `prefix(t) - prefix(t - b)` at an index above the true top, where both prefix sums equal the total, so it is 0, its correct value and its initial value; the index is at most 379, inside the array.
  - `g_ways_379` (the array has 379 words): the largest index used is 378.
  - `t_loop2` (the position loop runs while `p <= n + 1`): the one extra position has `p = n + 1`, so `f = 0`, `r = p`, `q = 1` and `r % p = 0`: its contribution is `0 + p * 0 + 1 * 0 = 0`.
  - `t_q0` (the guard `q == 0` dropped from `q * (q - 1) / 2`): for `q = 0` the product is `0 * (2^64 - 1) = 0`, the same value.

## Not run

Digests of the large inputs against the scan, other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, and any timing or step-count assertion. Nothing here is a CI result.
