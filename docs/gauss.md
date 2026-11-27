# gauss: Gaussian elimination modulo 998244353

Code: `core/gauss/gauss.cpp` (contract in `core/gauss/gauss.h`). Tests: `tests/gauss/gauss_tests.cpp`. Reference: `tools/gauss_ref.py`. Evidence label: unit tests only.

## Rule

- Every entry of `a` and `b` is a residue below `kMod` = 998244353. Dimensions are at most `kMaxDim` = 256.
- `gauss_scratch_words` writes `rows * cols` (`with_rhs` false) or `rows * (cols + 1)` (true). Null `out_words` is NullArgument, then a dimension above `kMaxDim` is TooLarge with nothing written.
- `gauss_det` gives the determinant of the `n x n` matrix (1 for `n = 0`, 0 when singular). `gauss_rank` gives the rank of `a` (0 when `rows` or `cols` is 0).
- `gauss_solve` writes `*out_rank` and `*out_kind` on every Ok result, including Inconsistent. `*out_rank` is the rank of `a`, not of the augmented matrix. `x` is written only for Unique and Many. The solution written is the canonical one: the unique reduced row echelon form, the leftmost independent columns as pivots, every free variable 0. Row operation order cannot change it, and the tests check this exactly.
- Check order as in the header: NullArgument, TooLarge, ScratchTooSmall (only while the matrix has entries), ValueOutOfRange. The residue scan covers every entry of `a` and `b` after the scratch check, also for singular or inconsistent systems. Nothing is written on any failure.
- `cols = 0` with a nonzero `b` is Inconsistent; `rows = 0` with `cols > 0` is Many with `x` all 0.

Integer only: no floating point, no allocation, no recursion, no mutable statics, no 128-bit types, no compiler builtins or intrinsics. Products use `u64` and one `%`.

## Method

Gauss-Jordan elimination in the caller scratch (a copy of `a`, with `b` as an extra column for solve). For each column the first row at or after the current rank with a nonzero entry is the pivot row; it is swapped up, scaled by the inverse of the pivot (one modular inverse per pivot, by exponentiation with at most 30 squarings), and subtracted from every other row. At the end the pivot rows are the reduced row echelon form in rows `0 .. rank - 1`. Solve reads each pivot variable from the leading entry of its pivot row. The determinant is the product of the pivots with a sign flip for each swap, 0 when the rank is below `n`.

Step bound: O(rows * cols * min(rows, cols)) field operations, plus one inverse per pivot. For 256 x 256 that is about 17 million multiplications. Not measured.

Ranges: stored values stay below `kMod`; a product is below `(kMod - 1)^2 < 2^60`.

## Tests

Expected results come from `tools/gauss_ref.py`, which uses four independent methods: Leibniz expansion and minors (small sizes), Bareiss fraction free elimination in Python integers, a separate Python Gauss-Jordan, and a construction `A = P L E U` with known rank, determinant and solutions. They are cross-checked while the data is generated and a disagreement aborts. The goldens are FNV-1a digests over the complete outputs. `python3 tools/gauss_ref.py check` runs the cross-checks without printing and took about 22 s on the machine used (local, one core, not a timing assertion; no test asserts time). Generating the data file takes about the same.

Every success run uses scratch of exactly the required size at an odd u32 offset with guard words, a garbage fill, and sentinel fills on outputs. No test asserts scratch contents after a call.

Extents, exactly as named in the tests:
- `gauss exhaustive extent binary square n 1 to 4 det and rank`: every 0/1 matrix of size 1 to 4 (exhaustive for these matrices only).
- `gauss exhaustive extent binary rank rows cols 1 to 4`: every 0/1 matrix with `rows, cols` from 1 to 4 (rank).
- `gauss exhaustive extent binary systems rows cols 1 to 3`: every 0/1 system `a`, `b` with `rows, cols` from 1 to 3.
- `gauss exhaustive extent four residues 2x2 det and rank`: all 4^4 = 256 matrices over `{0, 1, 2, 998244352}`.
- `gauss exhaustive extent four residues 2x3 systems`: all 4^8 = 65536 systems (`a` 2x3 plus `b`) over the same four residues.
- `gauss exhaustive extent residues 0 1 and p minus 1 3x3 det and rank`: all 3^9 = 19683 matrices of size 3x3 over `{0, 1, 998244352}` (`p` = `kMod`; this one is not the set `{0, 1, 2}`).
- `gauss exhaustive extent ternary 0 1 2 every shape rows cols 1 to 3 rank and square det`: every matrix over `{0, 1, 2}` for every shape `rows, cols` from 1 to 3, 21297 matrices in total (3, 9, 27, 9, 81, 729, 27, 729, 19683 for the nine shapes). Rank for all of them, determinant for the 19767 square ones (3 + 81 + 19683).
- `gauss exhaustive extent ternary 0 1 2 systems rows 1 to 2 cols 1 to 3`: every system with `a` over `{0, 1, 2}` and `b` over `{0, 1, 2}`, `rows` 1 to 2 and `cols` 1 to 3, 7488 systems in total (9, 27, 81, 81, 729, 6561 for the six shapes). The test asserts both counts.
- `gauss scratch words exhaustive extent rows cols 0 to 6 both flags`.
- Table cases (757, sampled, not exhaustive): 585 random and 172 constructed with known answers; 736 with sizes up to 100 (test `gauss table cases random and constructed up to 100 x 100`) and 21 with `rows` or `cols` from 128 to 256 (test `gauss large table cases 128 to 256 rows and columns`).
- `gauss row order does not change the answer extent all row permutations of 5 systems`: every row permutation of five systems gives the same canonical answer.
- Hand checked systems, zero dimensions, failures (null pointers alone, TooLarge, ScratchTooSmall by one word, check order, nothing written), out of range values at the first, middle and last entry of `a` and `b` also for singular and inconsistent systems, failures at the largest extent, and closed form patterns at 256 x 256.

## Mutation testing

132 source mutants of `core/gauss/gauss.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (this file's test list, 21 test cases). Staged run, disclosed: each mutant first ran every test case except the large ones; a mutant that passed that stage then ran the large cases alone. A mutant is called killed by the first failure. Early in the work three mutants had an ambiguous pattern (sw_and, sw_ger, sw_gec); they were rewritten and are killed. The test file was edited after earlier complete runs (comment edit, then the two ternary extents added at review); the whole set was run again from scratch on the final file, and the numbers here are from that last run only.

- 97 killed by a failed assertion.
- 23 crash-only (no failed assertion, killed by a sanitizer or abort): red_skip, red_rank2, sw_null, det_null, det_and, det_a, det_s, det_n1, rk_null, rk_nulls, rk_nulla, rk_nulls2, sv_null1, sv_null2, sv_null3, sv_a3, sv_bs, sv_b, sv_s, sv_bs0, sv_x, sv_copy_a, sv_xc.
- 0 timeouts.
- 12 not killed (survivors), argued:
  - red_and, red_le: `rank < rows` removed or loosened. At `rank = rows` the pivot search starts at `p = rows` and takes the `continue` path. Equivalent.
  - red_pe: `p >= rows` for `p == rows`. Equivalent.
  - red_swj2: the swap starts at column `c` instead of 0; entries before `c` in rows at or above the rank are zero. Equivalent.
  - red_scale0: the scaling starts at column 0 instead of `c`; it scales zeros. Equivalent.
  - red_fnone: no skip for factor 0; subtracting 0 times the pivot row changes nothing. Equivalent.
  - red_el0: elimination starts at column 0 instead of `c`; the pivot row is zero before `c`. Equivalent.
  - det_n0s: the scratch check without `n > 0`; for `n = 0` the product is 0 and `scratch_words < 0` is never true. Equivalent.
  - det_noempty: no early return for `n = 0`; the cell count is 0, nothing reduces, rank 0 equals `n`, the determinant is 1, null scratch is never touched. Equivalent.
  - sv_sc3: the scratch check without `rows > 0`; with `rows = 0` the product is 0 and the check never fires. Equivalent.
  - sv_rows0: the `rows = 0` branch disabled; the generic path gives the same `x = 0`, rank 0 and Unique if `cols = 0`, else Many. Equivalent.
  - sv_kind2: `rank >= cols` for `rank == cols`; the rank never exceeds `cols`. Equivalent.

## Not run

Exhaustive tests beyond the stated extents, timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
