# floorsum: floor sums modulo 2^64 and modulo 998244353

Code: `core/floorsum/floorsum.cpp` (contract in `core/floorsum/floorsum.h`). Tests: `tests/floorsum/floorsum_tests.cpp`. Reference: `tools/floorsum_ref.py`. Evidence label: unit tests only.

## Rule

- `S` is the exact mathematical sum over `i = 0 .. n-1` of `floor((a * i + b) / m)`, with floor toward minus infinity. `n`, `a`, `b` are any `u32` (`a`, `b` any `i32` for `floorsum_signed`), `m` is a `u32` of at least 1. `n = 0` gives `S = 0`.
- `floorsum_wrap` writes `S mod 2^64`. `floorsum_signed` writes the two's complement bit pattern of `S mod 2^64` (so a negative `S` is the pattern of its low 64 bits). `floorsum_mod` writes `S mod 998244353`, in `0 .. 998244352`.
- Check order for all three: a null `out` is NullArgument, then `m == 0` is ZeroModulus (so a null `out` with `m == 0` is NullArgument, and `n` plays no part). Nothing is written on a failure. Ok writes `out` also for `n = 0`.

Integer only: no floating point, no 128-bit types, no compiler builtins or intrinsics, no allocation, no recursion, no mutable statics, no arrays.

## Method

`tri(n) = n (n - 1) / 2` is computed in `u64` from the even factor of `n (n - 1)` halved first, so it is exact (below `2^63`).

The core loop works on `n, m, a, b` below `2^32` (`n` up to `2^32 - 1`):
1. If `a >= m`, add `tri(n) * (a / m)` and set `a = a % m`. If `b >= m`, add `n * (b / m)` and set `b = b % m`.
2. `y = a * n + b`. If `y < m`, stop. Otherwise set `n = y / m`, `b = y % m`, and swap `m` and `a`.

After step 1, `a < m <= 2^32 - 1`, so `a <= 2^32 - 2` and `n <= 2^32 - 1`: `y <= (2^32 - 2)(2^32 - 1) + 2^32 - 2 < 2^64`, exact in `u64`. The new `n = floor(y / m)` is at most `n`, and the new `m` (the old `a`) and new `a` (the old `m`) are below `2^32`, so the invariant holds on every pass. Each pass maps `(m, a)` to `(a, m mod a)` after the reduction, so the number of passes is that of the Euclidean algorithm, at most about 46 for 32-bit values (the reduction at the top of a pass is the modulo step). In the tests the largest pass count seen was 21 (a counter in an instrumented copy, not in the repo). Step bound: O(log m) passes, each a constant number of integer operations. This is a structural argument; time is not measured.

`floorsum_wrap`: the sums are added in `u64`, which wraps. `tri(n) * (a / m)` is the product of two exact non-negative integers, reduced modulo `2^64` by the hardware, and the same holds for `n * (b / m)`; addition modulo `2^64` of residues is the residue of the sum. So the result is `S mod 2^64`.

`floorsum_mod`: the same loop with every term reduced modulo `p = 998244353`: `(tri(n) mod p) * ((a / m) mod p)` and `(n mod p) * ((b / m) mod p)` are below `p^2 < 2^60`, and the running sum is kept below `p`.

`floorsum_signed`, why the wrapped product of a possibly negative quotient with `n (n - 1) / 2` is the exact value modulo `2^64`:
- Let `qa = floor(a / m)` and `ra = a - qa * m` in `0 .. m - 1`, and the same for `b` (`qb`, `rb`). Then `a i + b = m (qa i + qb) + (ra i + rb)`, so `floor((a i + b) / m) = qa i + qb + floor((ra i + rb) / m)` and `S = qa * tri(n) + qb * n + S(n, m, ra, rb)` exactly, with an integer `qa`, `qb` that may be negative.
- The map from the integers to `u64` by two's complement (`x -> x mod 2^64`) is a ring homomorphism: the residue of a product (or sum) is the product (or sum) of the residues modulo `2^64`. So `u64(qa) * tri(n)` (with `tri(n)` exact, below `2^63`) equals `qa * tri(n) mod 2^64`, and `u64(qb) * u64(n)` equals `qb * n mod 2^64`, whatever the signs. The third term is the unsigned core on non-negative residues.
- `qa`, `qb` and `ra`, `rb` are computed in `i64`: `|a|, |b| <= 2^31` and `m < 2^32`, so `|qa| * m <= 2^31 + m` and nothing overflows. The floor division of a negative `v` is `-((-v + m - 1) / m)`, with `-v + m - 1 < 2^33`.
- `a = INT32_MIN = -2^31`, `m = 1`: `qa = -2^31`, `ra = 0`; the `a` term is `u64(-2^31) * tri(n)`, which is `(2^64 - 2^31) * tri(n) mod 2^64`, the exact `-2^31 * tri(n)` modulo `2^64`. Likewise for `b = INT32_MIN`, `qb = -2^31`, term `u64(-2^31) * n`. The test `floorsum hand checked values` evaluates this for `n = 2^32 - 1` by the same expression in `u64` arithmetic.
- `a = INT32_MIN`, `m = 2^32 - 1`: `-2^31 = -1 * m + (2^32 - 1 - 2^31)`, so `qa = -1` and `ra = 2^31 - 1`; the same for `b`. The terms are `u64(-1) * tri(n) = -tri(n) mod 2^64` and `-n`. For `n = 2`: `i = 0` gives `floor(-2^31 / m) = -1` and `i = 1` gives `floor(-2^32 / m) = -2`, so `S = -3`, which the test checks as `0xFFFFFFFFFFFFFFFD`.

## Tests

Expected values come from `tools/floorsum_ref.py` (exact Python integers, no overflow), which uses independent methods: the defining loop; the classic recurrence for the sum over `i = 0 .. n` of `floor((a i + b) / c)` (reduce modulo `c`, then swap through `floor((a n + b) / c)`); an iterative variant in big integers; and lattice counting (`S = sum over k of #{i : a i + b >= k m}`) when the number of levels is small. For signed inputs the recurrence is applied after a reflection `i -> n - 1 - i` and a shift of `b` by a multiple of `m`, which is a different reduction from the one in the implementation. A disagreement aborts the generation. The goldens are FNV-1a digests over 64-bit words (`h = (h ^ word) * 0x100000001B3`), and a table of explicit values. `python3 tools/floorsum_ref.py check` runs all the cross-checks without printing and took about 4 s on the machine used (local; no test asserts time).

The small extents are also checked in the test against an exact brute force loop (64-bit integers, floor toward minus infinity), so every case there has two independent expected values.

Extents, exactly as named in the tests:
- `floorsum exhaustive extent unsigned n 0 to 24 m 1 to 24 a 0 to 30 b 0 to 30`: 576600 cases (25 * 24 * 31 * 31). For each, wrap, mod and signed are checked against the loop (the exact sum is below `2^64`, so mod equals the wrapped value reduced, and signed equals wrap), and the digest of wrap and mod is compared.
- `floorsum exhaustive extent signed n 0 to 16 m 1 to 16 a and b each minus 64 to 64`: 4526352 cases (17 * 16 * 129 * 129), against the loop and the digest.
- `floorsum exhaustive extent boundary grid unsigned 16 n 15 m 16 a 16 b`: 61440 cases, every combination of `n` and `a`, `b` in {0, 1, 2, 3, 4, 2^15 - 1, 2^15, 2^15 + 1, 2^16 - 1, 2^16, 2^16 + 1, 2^31 - 1, 2^31, 2^31 + 1, 2^32 - 2, 2^32 - 1} and `m` in the same list without 0 (so the moduli include 1, 2, 2^31 and 2^32 - 1). The digest of wrap and mod comes from the exact value. In-test properties: signed equals wrap whenever `a` and `b` are at most `2^31 - 1`, and mod equals wrap reduced whenever `n <= 2^15` (the exact sum is then below `2^62`).
- `floorsum exhaustive extent boundary grid signed 16 n 15 m 23 a 23 b`: 126960 cases, `n` and `m` as above and `a`, `b` in {0, 1, -1, 2, -2, 3, -3, 2^15 - 1, 2^15, 2^15 + 1, their negatives, 2^16 - 1, 2^16, 2^16 + 1, their negatives, 2^31 - 1, 2^31 - 2, INT32_MIN, -(2^31 - 1)}. Digest from the exact value. Signed equals wrap whenever `a` and `b` are both non-negative.
- `floorsum sampled table 975 rows random large closed form and above 2^64` (sampled, not exhaustive): 300 unsigned and 300 signed random cases (random bit lengths), families with `n = 2^32 - 1` and large slopes, closed forms (`a = 0`, `m = 1`, `a = m`), and negative inputs with `INT32_MIN` for `m` in {1, 2, 2^31, 2^31 + 1, 2^32 - 1}. 94 of the 441 unsigned rows have an exact sum of at least `2^64`, and wrap and mod are checked against the exact Python value there (347 unsigned rows are below `2^64` and also check that mod equals wrap reduced). 74 of the 534 signed rows have an exact absolute value of at least `2^64` (460 below). The counts are asserted.
- `floorsum hand checked values`, and `floorsum n zero writes zero and failures write nothing`: null `out` for each function (with a valid `m` and with `m = 0`, for `n` in {0, 1, 2^32 - 1}), `m = 0` with sentinels left untouched, `n = 0` writes 0.

Products of 32-bit values in the tests are widened to `u64` or `i64` before the multiplication.

## Mutation testing

90 source mutants of `core/floorsum/floorsum.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (which were not edited after the run). A mutant is called killed by the first failed assertion; a mutant that ended in a sanitizer abort or crash with no failed assertion is crash-only.

- 70 killed by a failed assertion.
- 7 crash-only: w_ge_b, w_bmod, w_ymax_sub, d_ge_b, d_bmod, d_ymax_sub, e_szero.
- 5 timeouts, each run alone in an instrumented copy with a pass counter in the loop: w_ge_a, w_amod, w_swap_b, d_ge_a, d_amod. The counter passed 100 in one call in all five; the unmutated code never exceeds 21 passes in any test, so these are non-terminating loops (for example `a > m` instead of `a >= m` leaves `a == m` unreduced, so the swap returns the same pair forever).
- 8 not killed (survivors), argued:
  - tri_odd: `n * (n / 2)` for `n * ((n - 1) / 2)`; this is the odd branch, where `n` is odd, so `n / 2 == (n - 1) / 2`. Equivalent.
  - w_ylt, d_ylt: `y_max <= m` for `y_max < m`. With `y == m` the next pass would have `n = 1`, `b = 0`; one term `floor(0 / m')` is 0 and the term `tri(1) * q` is 0, so the pass adds nothing. Equivalent.
  - d_atri: `(tri(n) % p) * (a / m)` without the second reduction: `tri(n) % p < 2^30` and `a / m < 2^32`, so the product is below `2^62`, no overflow, and the value is the same modulo `p`. Equivalent.
  - d_bnp: `n * ((b / m) % p)` without reducing `n`: `n < 2^32` and the other factor is below `2^30`, so the product is below `2^62`. Equivalent.
  - f_ge, f_le: `v > 0` or `v >= 1` for `v >= 0` in the floor division: for `v = 0` the other branch gives `-((m - 1) / m) = 0` too. Equivalent.
  - f_neg: `-((-v + m) / m)` for `-((-v + m - 1) / m)`: for a negative multiple of `m` the quotient is one too small and the remainder comes out as `m` instead of 0. The unsigned core reduces `a` and `b` by `m` first (adding back the quotient 1), so the total is unchanged. Equivalent for the output of all three functions (the intermediate remainder `m` is at most `2^32 - 1`, within the core's input range).

## Not run

Exhaustive tests beyond the stated extents, timing or step-count assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
