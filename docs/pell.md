# pell: the fundamental solution of x^2 - d y^2 = 1

Code: `core/pell/pell.cpp` (contract in `core/pell/pell.h`). Tests: `tests/pell/pell_tests.cpp`. Reference: `tools/pell_ref.py`. Evidence label: unit tests only.

## Rule

`pell_fundamental(d, x, y, period)` for a non-square `d` from 2 to `kMaxD` = 2^20 sets `*x`, `*y` to the smallest solution (x, y), y > 0, of x^2 - d y^2 = 1, each modulo 998244353, and `*period` to the length P of the repeating part of the continued fraction of sqrt(d).

- Check order: a null `x`, `y` or `period` (any of the three) is NullArgument; then `d` = 0, 1, above 2^20 or a perfect square is BadD (2^20 is a square, so the largest legal `d` is 2^20 - 1). Nothing is written to any output on a failure.
- The three outputs must be distinct objects (caller precondition, not tested). The outputs are written from locals at the end.
- Pure integer: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array.

## Method

With `a0 = floor(sqrt(d))` (a loop on u32), `m = 0`, `dd = 1`, `a = a0`, the expansion is `m' = a * dd - m`, `dd' = (d - m'^2) / dd`, `a' = (a0 + m') / dd'`. It repeats from `a1`; the first quotient equal to `2 * a0` is `a_P`, the end of the first period, so `P` is the number of quotients consumed when it first appears. The convergents `p_n = a_n p_{n-1} + p_{n-2}` (and `q_n`) are kept modulo p only, as two rolling pairs. The fundamental solution is the convergent of index `P - 1` for an even `P` and `2P - 1` for an odd `P`; for an odd `P` the loop runs through a second period and stops at the second `2 * a0`. At most `2P` steps, `P <= 2672` for every `d <= 2^20`.

Bounds (every one measured over all non-square `d` up to 2^20 by `tools/pell_ref.py`, and held by the tests): `m <= 1023`, `dd <= 2046`, `a <= 2046`, `P <= 2672` (at `d = 1018879`). So `a * dd <= 4186116 < 2^22`, `m * m < 2^20` and `d - m * m < 2^20 + 1` all fit u32. The convergent step is NOT small: a residue is below `2^30`, so `a * p < 2^41`, and `a * p + p'` is below `2^41 + 2^30`; it is formed in u64 and reduced by one `% kMod`. The same for `q`. Nothing overflows.

Working memory: scalars only (`a0`, `m`, `dd`, `a`, `n`, `per`, `pm1`, `pm2`, `qm1`, `qm2`, and the two reduced values of one step); no array anywhere.

## Tests

Expected values come from known solutions, from `tools/pell_ref.py` (Python standard library only), and from an in-test oracle that does not use the implementation's stopping rule: the period is the first return of the state `(m, dd)` to its value after the first step, the solution is the product of the 2 x 2 matrices `[[a, 1], [1, 0]]` modulo p (a vector of the quotients, the second period run explicitly for an odd period), and the norm `x^2 - d y^2 = 1 (mod p)` is checked on every result.

The reference has two oracles, labelled:
- EXACT (Python integers): the convergents are built exactly and the fundamental solution is the first convergent with `p^2 - d q^2 = 1`, a definition that does not use the period rule; the period is the first return of the state. The largest solution has 1368 digits (`d = 1018879`) and 1677 digits in the targeted list, so exact big integers over the whole domain turned out to be feasible. `python3 tools/pell_ref.py verify K S` compared the exact oracle with the modular one on every non-square `d` of the domain, in 32 slices (`K` = 0..7, `S` = 0..3): all 32 slices were run and all agreed (about 11 minutes locally in total). The exact oracle also ran on the targeted list (167 values of `d`: longest periods including 1018879, `n^2 - 2 .. n^2 + 2` and `n^2 +- 1` for sampled `n`, 100 random up to 2^20) and on `d <= 30000` with a digest.
- MODULAR (every non-square `d` up to 2^20, in 8 chunks of 2^17): period by the first return of the state, solution by the matrix product, the norm identity on every `d`. One chunk takes 5 to 17 s in Python. It gave the digests and the maxima above; no `d` has a zero residue.
Digest: FNV-1a 64-bit over u32 words (`d`, `x`, `y`, period for each non-square `d` of a chunk, in increasing order).

Extents and local runtimes (gcc 11.4, `-O1`, ASan + UBSan, one core; the whole binary takes about 15 s; no case near 30 s; none asserts time):
- `hand-checked values` (0.00 s): 24 exact solutions for `d` = 2 to 61 (for example 2: (3, 2), P = 1; 13: (649, 180), P = 5; 61: (1766319049 mod p = 768074696, 226153980), P = 11).
- `failure matrix: null first, then BadD, nothing written` (0.01 s): the 7 null masks of the three outputs with 16 fixed values of `d` (0, 1, 2, 3, 4, 5, 61, 2^20 - 1 to 2^20 + 2, 2 * 2^20, 0x7FFFFFFF, 0x80000000, UINT32_MAX - 1, UINT32_MAX) and every perfect square up to 2^20 (null wins, untouched outputs); then BadD for those `d` with non-null outputs and a sentinel in each output compared after each failure, guard words around each cell.
- `targeted d against exact big-integer residues` (0.00 s): 167 values of `d`, and the longest period (2672 at 1018879).
- `every non-square d to 30000 against the exact big-integer digest` (exhaustive; 0.02 s).
- `sweep chunk 0` to `sweep chunk 7` (exhaustive; 0.8, 1.3, 1.6, 1.8, 2.0, 2.1, 2.4, 2.4 s): every non-square `d` from 2 to 2^20 - 1, in chunks of 2^17: implementation against the in-test oracle (result, period), norm identity, bounds (`P <= 2672`, residues below p), count and digest against the reference.

Deviation, stated plainly: the in-test oracle and the reference digests rest on the state-return period and the quotient-matrix product; the exact big-integer oracle is checked against them by the reference on every `d` (reference runs, not in the test binary). The test binary itself holds the exact residues only through the digest of `d <= 30000` and the 167 targeted values.

## Mutation testing

79 source mutants of `core/pell/pell.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (not edited after the run; md5 ccb4780e61c58565c8b6cc2be1b81311), the whole suite for each mutant, abort after the first failure, 25 s limit.

- 51 killed by a failed assertion.
- 5 crash-only (sanitizer abort or a fault): `sq_rm` (the perfect-square test removed), `m_plus`, `m_rm`, `dd_mul`, `a_nom`.
- 14 timeouts, not counted as kills: `d_lo_rm`, `d_bad_rm`, `d_and`, `a0_le`, `a0_noplus`, `a0_init2`, `dd_nom`, `dd_plus`, `a_p1`, `a_ceil`, `term_a0`, `term_m1`, `term_p1`, `else_break_rm`. Each was classified by a probe added to the loop head: the state `(m, dd, a, per)` returned to its value 1 to 3 iterations after a snapshot (for example for `d = 0` in the first three, `m = 1`, `dd = 4294967295`, `a = 0`; for `term_a0` at `d = 2`, `m = 1`, `dd = 1`, `a = 2`), and the stop test depends only on `a` and `per`, which the cycle never changes, so the loop never ends. For `else_break_rm` the state repeats with `per = 1` and the stop is the removed `break`.
- 9 survivors, each equivalent:
  - `d_lo1` (`d < 1`): `d = 1` is a perfect square and gets BadD from the square test; `d = 0` still hits the guard `d < 1`.
  - `d_hi_ge` (`d >= kMaxD`): `kMaxD = 2^20` is a perfect square and gets BadD anyway.
  - `sq_ge` (`a0 * a0 >= d`): `a0 = floor(sqrt(d))`, so `a0 * a0 <= d` and `>=` is `==`.
  - `a0_init0` (the loop starts at 0): `d >= 2` here, so the first step `1 <= d` runs and `a0` ends equal.
  - `term_ge` (`a >= 2 * a0`): every quotient is at most `2 * a0` (the maximum over the domain is 2046, with `2 * a0` the largest allowed), so `>=` is `==`.
  - `else_n`, `else_n3` (`n >= 2 * per`, `n == 2 * per` in the second-period branch): that branch is reached at the second `2 * a0`, at `n = 2P` exactly.
  - `early_write`, `early_write_p` (`*x = 0` or `*period = 0` after the checks): both are overwritten on the only path that continues.

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, aliasing of the outputs (a caller precondition), and any timing or step-count assertion. Nothing here is a CI result.
