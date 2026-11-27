# ntt: convolution modulo 998244353

Code: `core/ntt/ntt.cpp` (contract in `core/ntt/ntt.h`). Tests: `tests/ntt/ntt_tests.cpp`. Reference: `tools/ntt_ref.py`. Evidence label: unit tests only.

## Rule

- Inputs `a[0 .. na)` and `b[0 .. nb)` are residues below `kMod` = 998244353. The result has `r = na + nb - 1` entries (0 when `na` or `nb` is 0): `out[k]` is the sum of `a[i] * b[k - i]` over every valid `i`, modulo `kMod`.
- `r` is computed in 64 bits, so even `na = nb = 4294967295` cannot wrap. The padded size `m` is the smallest power of two at least `r` (`m = 1` for `r = 1`). The largest input length is `kMaxLen` = 2^18, so `r` is at most 524287 and `m` at most 2^19, inside the 2^23 limit of the prime.
- `ntt_scratch_words` writes `2 * m` words (0 when `na` or `nb` is 0). Null `out_words` is NullArgument, then a length above `kMaxLen` is TooLarge with nothing written.
- `ntt_convolve` on Ok writes `out[0 .. r)` and `*out_len = r` (also for `r = 0`, where nothing is written to `out`).
- Check order: NullArgument (`out_len`; `a` while `na > 0`; `b` while `nb > 0`; `scratch` and `out` while `r > 0`), TooLarge (given lengths, even when `r = 0`), ScratchTooSmall (`r > 0`, `scratch_words < 2m`), OutTooSmall (`r > 0`, `out_cap < r`), ValueOutOfRange. The residue scan looks at every entry of both arrays and does not depend on `r`: with `na = 0` an entry of `b` that is not below `kMod` still gives ValueOutOfRange.
- Nothing is written on any failure, not even `*out_len`; the inputs are never modified.
- Aliasing: `a`, `b` and `out` may be the same array in any combination (equal start pointers). The implementation validates first, copies both inputs into scratch, and writes `out` only at the end, so every input entry is read before it can be overwritten. Other overlaps, and any overlap with the scratch, are outside the contract and are not tested. The scratch pointer needs only u32 alignment.

Integer only: no floating point, no allocation, no recursion, no mutable statics, no 128-bit types, no compiler builtins or intrinsics. Products use `u64` and one `%`.

## Method

Iterative radix-2 transform with the primitive root 3, in the caller scratch: `fa = scratch[0 .. m)` and `fb = scratch[m .. 2m)`. Both inputs are copied in and zero padded, each is transformed (bit reversal permutation, then `log2 m` stages of butterflies with the stage root `3^((kMod - 1) / len)` computed by `pow_mod`), multiplied pointwise, and transformed again. The inverse transform is the same forward transform: the result is read back in reverse order (`out[k]` from index `(m - k)` modulo `m`) and scaled by `m^(-1)` (computed as `m^(kMod - 2)`). No table is needed.

Step bound: three transforms, each `(m / 2) * log2 m` butterflies with two modular multiplications each, plus `log2 m` calls of `pow_mod` of at most 30 squarings, plus linear copy, product and read-back loops: O(m log m). For the largest input (`m = 2^19`) that is about 15 million butterflies. The bit reversal loop takes amortised constant steps per index. Nothing is quadratic. This is a structural argument; time is not measured.

Ranges: every stored residue is below `kMod` after each butterfly (the sum is reduced by one conditional subtraction, the difference adds `kMod` when negative), so `x + y` is below `2 * kMod < 2^32`. A product is below `(kMod - 1)^2 < 2^60`.

## Tests

Expected results come from `tools/ntt_ref.py`, which uses no transform: the defining schoolbook sum (Python integers), Kronecker substitution (both inputs packed into one Python big integer with 80 bits per coefficient, the largest true coefficient is below 2^78, multiplied by Python and unpacked), evaluation identities at five fixed points for the large cases, and a closed form for the all `kMod - 1` inputs. The three methods are cross-checked while the data is generated. The golden is an FNV-1a digest of the length and every output entry, so every table case is compared coefficient by coefficient, including the full maximum-size products (`2^18 x 2^18`, `r = 524287`) by Kronecker big-integer packing. Generating the data (all cross-checks included) takes about 1.5 minutes. The same generator (a 64-bit LCG, modes: uniform, within 3 of `kMod - 1`, below 3, sparse, all `kMod - 1`, all zero) is in the test file. Cases with at most 4096 result entries are also compared with an in-test schoolbook.

Every success run uses scratch of exactly `2m` words at an odd u32 offset (some runs aligned), guard words on both sides, a varied non-uniform garbage fill, and an `out` array with guard words after `out_cap` (some runs with extra capacity that must stay untouched). No test asserts scratch contents after a call.

Extents, exactly as named in the tests:
- `ntt exhaustive extent na nb 0 to 40 random residues`: every `(na, nb)` with `0 <= na, nb <= 40` (1681 pairs), seeded random residues, in-test schoolbook plus a golden digest of all outputs. Exhaustive for these lengths only, with these residues.
- `ntt scratch words exhaustive extent na nb 0 to 40`: the same pairs for `ntt_scratch_words`, value `2m` or 0.
- Table cases (258, sampled, not exhaustive): every padded-size boundary `r = 2^k - 1, 2^k, 2^k + 1` for `k` from 1 to 19 (where `r` fits) with up to five splits of `r` (`nb` = 1, 2, about half, about half plus one, `na = 1`), several odd shapes, all `kMod - 1` (`n` = 1, 2, 3, 17, 1000, 2^18), an all-zero case, `2^18 x 2^18` in four residue modes and two `a == b` cases, `2^18 x 1`, `1 x 2^18`, `2^18 x 2`, `2 x 2^18`, `2^18 x (2^17 + 3)`, `(2^18 - 1) x 2^18`, `2^18 x 40`, `131073 x 131073`. 162 of them have at most 4096 result entries (test `ntt table cases up to 4096 result entries`), 96 more (test `ntt large table cases above 4096 result entries digest of the full output`).
- Aliasing, equal pointers only: `a == out`, `b == out` for the small table cases flagged for it, `a == b` and `a == b == out` for the `a == b` cases (including the full `2^18 x 2^18` square), `ntt alias a == b with different lengths extent na nb 1 to 24` (all four kinds for every pair, `b` a prefix of `a` or the reverse, exact `out_cap = r`), a large `a == out` and `b == out` at 100000 by 80000, and an out-of-range value in an aliased array changes nothing.
- Failures: every null pointer alone (also with a single entry), null checks before TooLarge, TooLarge with lengths up to 4294967295 (also `r = 0` and the case where `na + nb` wraps in 32 bits), ScratchTooSmall by one word, OutTooSmall by one entry, the order between them, ValueOutOfRange at the first, middle and last entry of `a`, `b` or both, with four bad values (also for an empty result, and the largest valid residue is accepted), the same failures at the largest extent, empty results with null pointers, nothing written (guards, sentinel `*out_len`) on every failure.

## Mutation testing

87 source mutants of `core/ntt/ntt.cpp` (script outside the repo), built with ASan + UBSan and run against the final tests. Staged run, disclosed: each mutant first ran every test case except the large ones; a mutant that passed that stage then ran the large cases alone. A mutant is called killed by the first failure. During the work an earlier run left four survivors (a missing `out_len` null check, `na > 1` and `nb > 1` in place of `> 0` for the null checks of `a` and `b`, `r > 1` in place of `r > 0` for the scratch and out null check): the tests had no case with everything else valid or with a single entry. Tests were added and the whole mutant set was run again on the final tests; two more mutants had been rejected by the script (a pattern that did not match, and one that did not compile) and were rewritten.

- 69 killed by a failed assertion.
- 11 crash-only (no failed assertion, killed by a sanitizer or abort): start_step, sw_null, c_outlen, c_a, c_b, c_sc_and, c_sc_only, c_out_only, c_null_r, c_copy_a, c_copy_b_alias.
- 1 timeout, run alone with a counter: br_while (the bit reversal `while` condition inverted). The `bit` value is halved at each pass and reaches 0 after at most 19 passes; with 0 the condition `(j & 0) == 0` stays true and `bit >>= 1` stays 0, so the loop never ends. Shown with a counter: the loop passed 64 times in one step, which a correct run never does (at most 19).
- 6 not killed by the whole suite (survivors), argued:
  - br_swap_le: `i <= j` for `i < j`; with `i == j` the swap exchanges an entry with itself. Equivalent.
  - br_swap_gt: `i > j` for `i < j`; each pair of reversed indices is visited twice, once at the smaller index and once at the larger, and is swapped at exactly one of the two visits either way. Equivalent.
  - br_xor: `j |= bit` for `j ^= bit` at the end of the reversal step; the loop has just cleared that bit, so `|` and `^` give the same value. Equivalent.
  - s_gt: `s > kMod` for `s >= kMod` in the butterfly sum: a sum equal to `kMod` stays as `kMod`, the same residue as 0. Every later use is a product (`kMod` times anything is 0 modulo `kMod`), a sum or difference with the value at most `kMod` (still below 2^32), and the final read-back goes through `mul_mod`, so no stored value above `kMod` appears and the result is unchanged. Equivalent for the output.
  - d_gt: `x > y` for `x >= y` in the butterfly difference: for `x == y` the result is `kMod` instead of 0, the same residue, same argument as s_gt. Equivalent for the output.
  - c_scr_u32: the scratch size check in 32-bit arithmetic. After TooLarge, `m` is at most 2^19 and `2m` at most 2^20, far below 2^32. Equivalent.

## Not run

Exhaustive tests beyond the stated extents (lengths above 40, other residues), the full coefficient digest for every possible length, partial overlap and overlap with the scratch (outside the contract), timing assertions, memory sanitizer, MSVC, Clang, Windows, Android. Nothing here is a CI result.
