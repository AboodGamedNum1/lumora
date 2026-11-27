# Bitset

`core/bitset/bitset.h` and `bitset.cpp` (`lumora::bitset`) are fixed-size bit sets over a caller array of 64-bit words: `words_for`, `set`, `test`, `fill`, `popcount` and `find_next`.

Evidence label: unit tests only (local runs; CI is reported separately in the pull request).

Integer only: no floating point, no libm, no compiler builtins or intrinsics (popcount and trailing zeros are plain shift and mask loops), no 128-bit types, no allocation, no recursion, no mutable statics. The source includes only the header, which includes `<cstdint>`.

## Rule

- Bit `i` (`i < nbits`) is bit `i % 64` of word `i / 64`, bit 0 least significant. The caller supplies `W = words_for(nbits)` words.
- The bits of the last word at or above `nbits` are padding. No function reads them into a result, and none changes them: `set` and `fill` keep the padding bits the word had.
- Arguments are valid when `nbits <= kMaxBits` and `words` is non-null whenever `nbits > 0`. `words` may be null when `nbits` is 0. An invalid argument, or any other listed failure, returns false and changes nothing, including `*out`.
- `words_for(nbits)` is `(nbits + 63) / 64`, and 0 above `kMaxBits`.
- `set` and `test` fail for `i >= nbits` (so for every `i` when `nbits` is 0). `test` also fails for a null `out`.
- `fill` sets every bit below `nbits`; `nbits` 0 succeeds.
- `popcount` counts the set bits below `nbits`; `nbits` 0 gives 0.
- `find_next(from, value)` gives the smallest `j` with `from <= j < nbits` whose bit equals `value`, or `nbits` when there is none (still true). `from == nbits` is valid and gives `nbits`, also with null `words` and `nbits` 0. `from > nbits` is invalid. A zero padding bit never counts as a hit for `value == false`.

## Decisions

Confirmed by the maintainer (relayed through the coordinating agent) as the numbered readings above: the validity rule, `words_for` values, `nbits` 0 with null `words`, the `find_next` edge cases and the padding rules. The order in which failures are detected is not contractual and is not tested. `words` and `out` must not overlap; this is not checked and not tested. Reads are not observable, so nothing here claims that words below `from / 64` or padding are not read: the tests check that their content does not change any result.

## Bounds

| Item | Bound |
| --- | --- |
| `nbits` | 0 to 1073741824 (`kMaxBits`), 16777216 words at most |
| aliasing of `words` and `out` | not supported, not tested |

## Tests

`tools/bitset_ref.py` is an independent reference in Python. Its model is a plain list of bits, one entry per bit, padding included; it never uses words or masks for the operations, and has its own FNV-1a 64. A scenario is `(nbits, seed, mode, nops)`: seeded initial words (random, zero, ones, sparse), then 300 scripted operations with valid and invalid indices, and a digest over every result and the final words, padding included. The script prints the table of 100 scenarios (25 sizes from 0 to 20000, around every word boundary, times 4 modes) pasted into the tests.

`tests/bitset/bitset_tests.cpp` has 8 test cases and 883846 assertions in the last local run: the 100 scenarios; `words_for` for every `n` below 1000 and at the limit; invalid arguments (limit exceeded, null `words`, null `out`, index at or above `nbits`) with a small dummy buffer that stays unchanged; `nbits` 0; padding for every `nbits` from 1 to 200 with padding all ones and all zeros (results ignore it, `set` and `fill` keep it bit for bit); `set` changing exactly one bit for sizes 1, 63, 64, 65, 130; `find_next` against a bit-by-bit oracle for every `nbits` to 200, every `from` and both values, with the words below `from / 64` inverted; and `fill` on every `nbits` to 200. Buffers are exact size, so ASan sees any access past the last word.

Local run: g++ with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined`, no warnings, all passed. No other compiler was run.

## Not run

By the maintainer's decision, no buffer of 128 MiB (2^24 words) is allocated. A successful call at `nbits = kMaxBits` is therefore not run. The limit is tested only where the function returns before reading: `nbits` of `kMaxBits + 1` and 4294967295 with a small dummy buffer, and `words_for` at `kMaxBits - 1`, `kMaxBits` and `kMaxBits + 1`.

## Mutation check

96 hand-written source mutants of `bitset.cpp` (the validity rule and each of its parts, the bit loops, `words_for`, every check and index test, the shifts and masks of `set` and `test`, the full-word loop and the tail mask of `fill` and `popcount`, and every step of `find_next`: the start word, the first-word mask, the word walk, the end word, the padding cap and the not-found result) were built against the unchanged tests under ASan and UBSan. 93 were killed and 3 survive.

Survivors:

- Validity with `nbits < kMaxBits` instead of `<=`: differs only at `nbits = kMaxBits`, where the original would use a 2^24-word array. Not equivalent. It is not killed because a call at that size is not run by the maintainer's decision.
- `find_next` returning `j <= nbits ? j : nbits` instead of `j < nbits`: when `j == nbits` both give `nbits`. Equivalent.
- `find_next` computing `j` in 32 bits: `w` is below 2^24 whenever `nbits <= kMaxBits`, so `(w << 6) + 63` is below 2^30 + 64 and fits in 32 bits. Equivalent under that bound.
