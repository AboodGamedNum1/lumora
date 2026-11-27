# seqdist: LCS, edit distance and longest increasing subsequence

Code: `core/seqdist/seqdist.cpp` (contract in `core/seqdist/seqdist.h`). Tests: `tests/seqdist/seqdist_tests.cpp`. Reference: `tools/seqdist_ref.py`.

## Rule

- `lcs_length_u32`: length of the longest common subsequence of a[0 .. na) and b[0 .. nb), by value equality. An empty side gives 0.
- `edit_distance_u32`: the Levenshtein distance with unit cost for insertion, deletion and substitution. An empty side gives the length of the other side.
- `lis_length_u32`: the longest strictly increasing subsequence when `strict` is true, the longest non-decreasing one when it is false, by unsigned comparison. n 0 gives 0.
- Limit: every length is at most `kMaxLen` = 65536.
- Scratch need, in u32 words: for the first two, 2 * (min(na, nb) + 1) when both lengths are above 0, else 0. For lis, n + 1 when n is above 0, else 0. Scratch contents are unspecified afterwards, and this file does not assert any scratch value after a call.

Integer only: no allocation, no recursion, no global state, no 128-bit types.

## Method

- LCS and edit distance: two rolling rows over the shorter sequence, the longer one walked once, O(na * nb) time.
- LIS: patience tails kept in the scratch, with a loop-based binary search (lower bound for strict, upper bound for non-decreasing), O(n log n) time.
- The scratch is written before it is read, so the result does not depend on its initial contents. Only the first `need` words are used.

## Check order

NullArgument first: `out` is always required; `a` only while na is above 0 and `b` only while nb is above 0; the scratch only while the need is above 0. The need is computed in 64 bits, so lengths near 2^32 do not wrap, and a null scratch with such lengths is NullArgument. Then TooLarge: any length above 65536, also when the other side is fine. Then ScratchTooSmall: `scratch_words` below the need. On any failure `*out` is left untouched.

## Decisions

Six readings were confirmed by the maintainer through the coordinating agent, with corrections to the others:
- Check order, scratch need formulas, the 64-bit need and null precedence, strict versus non-decreasing semantics, no aliasing claim: confirmed.
- Scratch contents: tests never assert a scratch value after a call (success or failure) and never assert that words after the need are untouched. Overruns are caught by scratch buffers of exactly the need, under ASan. A garbage or zero or all-ones initial fill must not change the result.
- Output: only the final value on success and no change on failure are tested, not how often it is written.
- Reads: the header says nothing about reading the other sequence when one side is empty, so no test asserts either behaviour. A non-empty side must still be non-null.
- The success with both lengths at 65536 is not run (see Not run).

## Tests

Expected values come from `tools/seqdist_ref.py`, which does not use the C++ method: full two-dimensional tables (suffix form) for LCS and edit distance, a quadratic programme over end positions for the increasing subsequences, and brute force over all subsequences for short inputs. The reference asserts, while generating, that the edit distance lies between |na - nb| and na + nb - 2 * lcs. It emits 3162 pair rows, 2133 LIS rows and a digest (FNV-1a 64) of five larger pairs from an LCG stream (seed 314159).

7 test cases, 44547 assertions:
- pair rows: all pairs of sequences of length 0 to 3 over {0, 1, 2} (1600), 1500 random pairs of length 0 to 30 over six values including 0x80000000 and 0xFFFFFFFF, 60 pairs of length 100 to 250, and two mid-size pairs (2000 x 2000 and 2000 x 1500). Each pair runs in both orientations, with the scratch starting as garbage, 0 and all ones
- LIS rows: all sequences of length 0 to 6 over {0, 1, 2}, 1000 random ones up to length 60 (small values and values around 2^31), 40 of length 300 to 900; strict and non-decreasing
- the golden digest
- hand-checked cases (kitten and sitting, empty sides, equal sequences, unsigned order, all equal values, repeated values)
- scratch boundaries: exactly the need is Ok, one less is ScratchTooSmall, a larger scratch is Ok, empty sides need no scratch (null allowed), a one-element side needs 4 words
- length limit on real buffers: 65536 against 1 and 1 against 65536 for LCS and edit; LIS at n = 65536 (strictly increasing, and repeated values, both modes) with 65537 scratch words; 65537 on either side as TooLarge on tiny storage, also with lengths of 2^32 - 1
- argument rules and check order: each of out, a, b and scratch null, null with length 0, null before TooLarge before ScratchTooSmall, the 64-bit need (a null scratch with lengths of 2^32 - 1 is NullArgument), `*out` untouched on every failure

Local run (g++ 17, `-O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI. The test file has no compiler-specific constructs.

## Not run

- LCS or edit distance with both lengths at 65536: about 4.3e9 steps. Covered: 65536 x 1 and 1 x 65536, 2000 x 2000 in both orientations, 65537 as TooLarge. No mutant survived because of this gap.
- Exhaustive sequences beyond length 3 (pairs) and 6 (LIS).
- MSVC, Clang, Windows and Android: no result seen.

## Mutation

132 source mutants of `core/seqdist/seqdist.cpp`, each built with ASan+UBSan and `-fno-sanitize-recover=all`, run against the full test file. 128 killed, 4 survived. Local result. The first run left two more survivors (the need computed in 32 bits for the null check in the pair functions and in lis), and tests with a null scratch and lengths of 2^32 - 1 now kill them.

Of the 128 kills, 17 come from a crash, not a failed assertion, and are listed individually. A dropped null check dereferences null: `pair no out null`, `pair no a null`, `pair no b null`, `pair scratch null none`, `lis no out null`, `lis no a null`, `lis scratch null none`, `lis need n>1` (a null scratch is used at n = 1). Broken empty-side handling uses the missing scratch: `lcs empty none`, `lcs empty and`, `edit empty none`, `edit empty and`. Wrong row or sequence choice reads out of bounds: `lcs m choice`, `lcs l choice`, `lcs cmp idx`, `edit m choice`, `edit l choice`. The other 111 are killed by a failed assertion.

No survivor is non-equivalent in its result. The 4 survivors:
- `pair scratch null >1`: the need is 0 or at least 4, so `need > 2` and `need > 0` agree.
- `lis len cond` (`lo >= len` instead of `lo == len`): the search result `lo` never exceeds `len`.
- `lis mid` and `lis mid hi`: probing at `lo` or at `hi - 1` instead of the middle turns the binary search into a linear scan from the left or the right. The predicate is monotone over the tails, so the found position is the same and the output is identical. Only the running time differs (quadratic instead of n log n), which the tests do not measure.
