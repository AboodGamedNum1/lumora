# divtrans: divisor zeta, divisor Moebius and Dirichlet convolution modulo 998244353

Code: `core/divtrans/divtrans.cpp` (contract in `core/divtrans/divtrans.h`). Tests: `tests/divtrans/divtrans_tests.cpp`. Reference: `tools/divtrans_ref.py`. Evidence label: unit tests only.

## Rule

An array of `n` entries is indexed by the integers 1..n through position `m - 1`. `kMod` = 998244353, `kMaxN` = 2^20.

- `divisor_zeta(a, n)`: `a(m)` becomes the sum of `a(d)` over every divisor `d` of `m`.
- `divisor_mobius(a, n)`: the exact inverse (`a(m) = sum over d | m of mu(m / d) * A(d)`).
- `divisor_convolve(a, b, n, out)`: `out(m) = sum over d | m of a(d) * b(m / d)` modulo `kMod`. `a` and `b` are not modified and may be the same array; `out` must not overlap them (outside the contract, not tested).
- Check order, transforms: `n > kMaxN` is TooLarge; for `n > 0` a null `a` is NullArgument; then any entry at or above `kMod` is ValueOutOfRange. Convolution: TooLarge; for `n > 0` a null among `a`, `b`, `out` is NullArgument; then a bad entry of `a`, then of `b`, is ValueOutOfRange. `n = 0` is Ok and nothing is read or written, whatever the pointers. On any failure nothing is written (`a`, or `out`, keeps its contents); the check of the entries comes before the first write.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no mutable static, no memory beyond the caller's arrays.

## Method

Every pass runs over the harmonic pairs `(d, m)` with `m = k * d`, `k >= 2`, `m <= n`: `n / d - 1` pairs for each `d`, about `n ln n` steps in all, in place.

- Zeta: `d` from `n` down to 1; for each proper multiple `m` of `d`, `a[m] += a[d]`. When the turn of `d` comes, `a[d]` is still its original entry, because only the turns of smaller divisors of `d` change it and they come later. So every original `a(d)` reaches each proper multiple of `d` exactly once, and every entry ends as the sum over all its divisors. (Going upward would add a partial sum and count chains such as 1, 2, 4 twice; the tests include the unit at 1 for that reason.)
- Mobius: the exact reverse, `d` from 1 up to `n`, `a[m] -= a[d]` for each proper multiple. When the turn of `d` comes, the turns of all smaller divisors have been undone, so `a[d]` is its original entry again.
- Convolution: the output is zeroed after all checks, then for every pair `d * e <= n`, `out[d * e] += a[d] * b[e]`. Each product is below 2^60 and is reduced at once with `% kMod`, and the running sum is kept reduced by a conditional subtract, so a 64-bit value never holds more than one residue plus one product.
- Residue sums use a conditional subtract (both operands are residues, so the sum is below 2 * kMod and fits 32 bits) and differences a conditional add.

The transforms are O(n log n), which the header allows. The convolution is O(n log n) (the harmonic sum `sum of n / d`).

## Tests

Expected results come from an in-test scan brute force (the definition: for every `m`, every `d` in 1..m with `m % d == 0`; `mu` by trial division), from in-test second constructions of a different shape (a per-prime pass for the transforms; the divisors of each `m` from its smallest-prime-factor factorisation for the convolution), from closed forms on the full array, and from digests made by `tools/divtrans_ref.py`.

The reference (Python standard library only; `python3 tools/divtrans_ref.py check` takes about 20 s locally) has three oracles. The scan oracle is THE oracle: it is used on every small extent and for every `n` up to 300 of the random arrays. It is infeasible at `n = 2^20` (about 5 * 10^11 steps), so the large digests do NOT come from the scan: they come from the pair oracle (the same definition with the divisors enumerated as the pairs `d * e = m`, O(n log n)), which `check` compares with the scan on all small extents and on random arrays for `n = 1..300`, and with the per-prime pass (transforms) and the factorisation method (convolution) on every large input it digests (the factorisation method on the random-by-random convolution only). The digests are FNV-1a over 32-bit words, results folded in the stated order (last position fastest). Inputs: xorshift32 (13, 17, 5) with seed `0x9E3779B9 + n` (plus 77 for the second operand), kinds `rand` (value mod p), `sparse` (one entry in eight non-zero), `ones`, `max` (all `p - 1`).

Extents, named as in the tests. Runtimes are local, measured with a gcc 11.4 `-O1` build with ASan and UBSan on one core (the whole binary took about 16 s; no case is near 30 s; none asserts time):
- `exhaustive transforms over {0,1,p-1} length 0 to 12 and {0,1,2,p-1} length 0 to 8 against the scan, both round trips` (exhaustive: 797161 and 87381 arrays; about 1 s). Each array: zeta and mobius against the scan, and zeta(mobius(a)) = a and mobius(zeta(a)) = a through the implementation.
- `exhaustive convolution pairs over {0,1,p-1} length 0 to 6 and {0,1,2} length 0 to 7 against the scan, also a = b` (exhaustive: 597871 and 5380840 pairs, each array also against itself as the same pointer; about 2.5 s).
- `exhaustive reference digests` (exhaustive: transforms over {0,1,p-1} length 0..8, {0,1,2,p-1} length 0..6, pairs over {0,1,p-1} length 0..4 and {0,1,2} length 0..5, each as one reference digest; under 0.1 s).
- `random arrays for every n 1 to 2000` (sampled: all n against the second constructions and a digest, n up to 300 also against the scan, and the convolution with a = b; about 0.7 s).
- `large n transforms`, `large n convolution` (sampled: n = 65536, 65537, 524289, 720720, 1048573 (prime), 1048575, 1048576, 4 kinds each, against reference digests; per-prime second construction and both round trips on random and sparse arrays; the factorisation second construction on random-by-random; about 3.5 s and 2.7 s).
- `closed forms on the full array` (sampled n as above; about 3.1 s): zeta(1) = divisor count, zeta(phi) = identity, mobius(1) = e_1, mobius(e_1) = mu, zeta(mu) = e_1, mobius(d) = 1, mobius(id) = phi, 1 * 1 = d, phi * 1 = id, mu * 1 = e_1, from independent sieves.
- `failure cases`: every null combination of `a`, `b`, `out` (and of `a` for the transforms) with `n` in {0, 1, 2, 2^20, 2^20 + 1, UINT32_MAX} against the stated check order; invalid residues (`kMod`, `kMod + 1`, UINT32_MAX) at the first, middle and last position of `a`, of `b` and of both, in the convolution and the transforms, and at the last position for `n = 5000`; sentinel-filled outputs and unchanged inputs after every failure.
- `hand-checked values and wrong-order patterns`: the unit at 1 for `n = 8` (a wrong order counts 4 and 8 more than once), small `n = 4` patterns, divisor counts, mu, phi, the identity of the convolution, commutativity, `n = 0`, `n = 1` and (p-1)^2 = 1.

Every run of the implementation uses guard words (3 before and after each array), a garbage fill of the output that varies between calls, and a comparison of the inputs afterwards.

Deviation, stated plainly: the exhaustive extents against the in-test scan are larger than the exhaustive extents with a Python digest (the digests cover transforms over {0,1,p-1} up to length 8, {0,1,2,p-1} up to 6, pairs up to length 4 and 5). The larger ones rely on the in-test scan, which is the definition itself; the Python scan oracle agrees with the in-test scan on the extents where both exist only through the common digests.

## Mutation testing

83 source mutants of `core/divtrans/divtrans.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests (the test file was not edited after the run; md5 7485baa3ddb2345c351e0e57c147a509). Each ran with abort-after-first-failure under a 90 s limit. A killed mutant ends with a printed failed assertion; a mutant that ends in a sanitizer abort or crash without one is listed as crash-only. A timeout would be listed apart and would not count as a kill (none occurred). One pattern was corrected after a first run (a mutant that did not compile, and one whose pattern did not match); those two were rerun on the same test file.

- 60 killed by a failed assertion.
- 16 crash-only: z_size_p1, z_size_rm, m_size_p1, m_size_rm, c_size_p1, c_size_rm (the size limit loosened or removed, so the call reads past a small buffer), z_null_rm, m_null_rm, c_null_rm, c_na, c_nb, c_nout (a null check dropped), z_val_before_null, m_val_before_null, c_val_before_null (entries read before the null check), c_last (a harmonic bound one too large).
- 0 timeouts.
- 7 survivors, each equivalent:
  - z_size_after_n0, m_size_after_n0, c_size_after_n0: the size check moved after the `n == 0` return. `n == 0` and `n > kMaxN` exclude each other, so the order cannot change a result.
  - z_dstart: the zeta loop starts at `n - 1` instead of `n`. The turn of `d = n` has no proper multiple `m <= n`, so it changes nothing.
  - m_dend: the mobius loop stops at `n - 1`. The same turn, `d = n`, has no proper multiple.
  - g_res_early: the residue scan returns at the first bad entry instead of finishing. The scan has no side effect and only its yes or no is used, so the result is the same.
  - c_ab_swap: the entries of `b` checked before those of `a`. Both failures give ValueOutOfRange and nothing has been written before either check, so the result is the same.

## Not run

Convolution against the in-test scan for `n` above 7 beyond the random arrays up to 300, the O(n^2) scan at large `n`, timing or step-count assertions, MSVC, Clang, Windows, Android, and any build other than the local gcc ASan + UBSan. Nothing here is a CI result.
