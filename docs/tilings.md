# tilings: domino tilings of an h by w rectangle

Code: `core/tilings/tilings.cpp` (contract in `core/tilings/tilings.h`). Tests: `tests/tilings/tilings_tests.cpp`. Reference: `tools/tilings_ref.py`. Evidence label: unit tests only.

## Rule

`domino_tilings(h, w, out)` sets `*out` to the number of tilings of an `h` by `w` rectangle by 1 x 2 dominoes, modulo 998244353. `h` is 1 to 6 (`kMaxH`); `w` is any u32, `w = 0` gives 1 (the empty tiling).

- Check order: a null `out` is NullArgument (for every `h`); then `h` outside 1..6 is BadHeight. Nothing is written to `*out` on a failure. Every u32 `w` is legal. There is no parity shortcut in the source (an odd area gives 0 by the method).

Integer only: no floating point, no 128-bit type, no builtin or intrinsic, no heap, no recursion, no mutable static.

## Method

Profile transfer matrix. Go through the columns one at a time. Let `s` be the set of rows of the current column already covered by horizontal dominoes that started in the previous column, and `t` the set of rows where a horizontal domino starts in this column. A column can be completed from `s` to `t` exactly when `s` and `t` are disjoint and the remaining free rows can be paired by vertical dominoes, i.e. every maximal run of consecutive free rows has even length. `T[s][t]` is 1 in that case, else 0. The answer is `(T^w)[0][0]`.

`T^w` is applied to a row vector: for each bit of `w` from the lowest, the vector is multiplied by the current power when the bit is set, then the power is squared (not after the last bit). That is at most 31 squarings and 32 vector products, about `32 * 64^3` multiply-adds for `h = 6` whatever `w` is.

Overflow bound: every entry is below `kMod < 2^30`, so a product is below `2^60` and is reduced at once; a partial sum plus one product is below `2^61`. A u64 never overflows.

Working memory (contract cap: 16384 u32 words across all local arrays). The only local arrays are in `domino_tilings`: `m[4096]` (the current power, stored 64 x 64), `sq[4096]` (the product being formed), `v[64]`, `nv[64]`: 8320 words in total. `build_profile`, `square` and `apply` have no arrays of their own (they write into the caller's). No temporaries beyond scalars.

## Tests

Expected values come from an in-test definitional backtracking count (cover the first free cell by a horizontal or vertical domino, exact integers), an in-test column-by-column DP whose transitions are generated row by row (a different construction from the run-parity rule and from the fast power), closed forms, known values, digests from `tools/tilings_ref.py`, and the recurrence coefficients embedded in the test.

The reference is Python standard library only (`python3 tools/tilings_ref.py check` prints `OK`, about 22 s locally). It holds a brute force, an exact column DP, an independent Python matrix power, and the recurrence route: Berlekamp-Massey on the exact column-DP sequence gives a linear recurrence mod p (orders 2, 2, 4, 4, 8, 8 for h = 1..6), evaluated at any `w` by Kitamasa. Digests are FNV-1a 64-bit over u32 words.

Deviation, stated plainly: at huge widths there is no brute force. The oracle there is the Berlekamp-Massey recurrence (checked against the exact column DP for every w up to 3000 and against the independent matrix power at all the huge widths in the digests), not the definition. The definition itself is run against the implementation only for w <= 8 (exhaustive, all h) and, through the column DP, for w <= 4096.

Extents and local runtimes (gcc 11.4, `-O1`, ASan + UBSan, one core; whole binary about 17 s; no case near 30 s; none asserts time):
- `hand-checked values, symmetry and known tables` (about 0.01 s): 2x2 = 2, 4x4 = 36, 6x6 = 6728, 4x5 = 95, 3x2k = 3, 11, 41, 153, h = 2 Fibonacci for w = 1..8, w = 0 gives 1 for every h, h = 1 parity for w <= 12, symmetry h,w in 1..6 (exhaustive), odd area gives 0 for every h and w = 1..40.
- `exhaustive h = 1..6 and w = 0..8 against the backtracking brute force` (exhaustive; 0.03 s), including 6x8 = 167089.
- `exhaustive every h = 1..6 with every w = 0..4096 against the column DP` (exhaustive; about 10.0 s, the slowest).
- `reference digests of the exact column DP, w = 0..300 for every h` (exhaustive; 0.4 s).
- `huge widths for h = 1..4` (0.1 s), `h = 5` (0.8 s) and `h = 6` (5.1 s) (sampled: 135 widths per h: 2^j - 1, 2^j, 2^j + 1 for j = 0..31, UINT32_MAX - 1, UINT32_MAX and 40 random widths; xorshift64 in the reference): the reference digest; at every width the recurrence identity `sum c_i * a(w - i) = a(w)` through the implementation at the neighbouring widths; h = 2 against fast-doubling Fibonacci; h = 1 parity; odd h with odd w gives 0 (h = 1, 3, 5). For odd h with even huge w only the oracle equality is asserted, never nonzero by itself.
- `failure cases, check order, nothing written`: null `out` with h in {0..8, UINT32_MAX} and w in {0, 1, 2, 3, 10, UINT32_MAX} (null wins); h = 0, 7, 8, 100, UINT32_MAX with a non-null `out` give BadHeight and the sentinel is unchanged; every u32 width is accepted for h = 1..6.

Every call passes the result cell between guard words.

## Mutation testing

83 source mutants of `core/tilings/tilings.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (not edited after the run; md5 d3d8573a0337cc0f02b096c724001c7b), abort after the first failure, 60 s limit.

- 69 killed by a failed assertion.
- 3 crash-only (sanitizer abort): `d_h_hi2` (height 7 accepted), `d_size_h2` (`size = 1 << (h + 1)`), `g_mod_swap` (`kDim = 63`).
- 2 timeouts, not counted as kills. `d_shift0` (`e -= 1` instead of `e >>= 1`): not a repeating state; `e` strictly decreases, so it is finite but about `w` iterations at huge widths. Run alone, the hand-checked case fails (31 failed assertions), so the behaviour differs, but it is not counted. `s_skip_a` (the `a == 0` skip removed): the whole suite passes in about 68 s, so it is equivalent and only slower (adding 0 changes nothing).
- 9 survivors, each equivalent:
  - `d_sq_cond` (square also after the last bit): the last square is never read.
  - `d_out_first` (`*out = 0` after the checks): overwritten by `*out = v[0]` on the only path that continues.
  - `b_used2` (`s ^ t` for `s | t`): `s` and `t` are disjoint there, so equal.
  - `b_run_noreset` (no reset of `run` after an odd run): `ok` is already false, and it never becomes true again.
  - `b_set_idx`, `s_copy_t`, `s_idx`, `s_idx2`, `a_idx` (transposed index or copy): `T` is symmetric (`s & t = 0` and the run test depend on `s | t` only), so every power is symmetric and `m * m^T`, `m^T * m` and `v * m^T` equal `m * m` and `v * m`.

## Not run

Other builds than the local gcc ASan + UBSan, MSVC, Clang, Windows, Android, the brute force beyond w = 8, and any timing assertion. Nothing here is a CI result.
