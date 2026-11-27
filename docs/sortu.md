# sortu: stable radix sort of u32 keys

Code: `core/sortu/sortu.cpp` (contract in `core/sortu/sortu.h`). Tests: `tests/sortu/sortu_tests.cpp`. Reference: `tools/sortu_ref.py`.

## Rule

`radix_sort_u32` and `radix_sort_pairs_u32` sort ascending by key. The sort is stable: equal keys keep their input order, and each value follows its key. `is_sorted_u32` reports whether keys are non-decreasing. Integer only: no heap, no recursion, no mutable statics, no 128-bit types, builtins or intrinsics. The only working memory is a 256-entry count table on the stack and the caller's scratch buffers.

## Method

Four LSD passes of 8 bits, low byte first. A pass is skipped when every key has the same byte at that position, because the stable result would not change. When an odd number of passes ran, the result sits in scratch, so it is copied back into the caller's keys (and values). The caller always gets the result in its own buffers.

## Check order

Sorts: NullArgument if n > 0 and any pointer is null, then TooLarge if n > kMaxCount (2^26), then n < 2 returns Ok. For n = 0 every pointer may be null.

`is_sorted_u32`: NullArgument if `out` is null (even when n is 0) or if n > 0 and keys is null, then TooLarge. On any error `*out` is untouched. Null keys is accepted when n is 0.

## Decisions

Nine readings were confirmed by the maintainer through the coordinating agent. Reading 1 was decided as the wording above: `is_sorted_u32` needs a non-null `out` even for n = 0. Reading 6 added a requirement: with skipped passes, an odd number of executed passes must still copy back. The tests cover exactly one and exactly three executed passes, and the even cases.

## Tests

Expected values come from `tools/sortu_ref.py`, an independent LSD sort over bucket lists (Python `sorted()` is only an assertion there). It emits 91 scenarios: 13 sizes (0, 1, 2, 3, 4, 5, 16, 255, 256, 257, 1000, 4096, 5000) times 7 key modes. Values are `index * 2654435761 mod 2^32`. Results are checked by golden digest and by exact-size buffers under ASan.

8 test cases, 26525 assertions:
- the 91 scenarios against the reference
- stability with many equal keys against an insertion-sort oracle
- every byte position and extreme keys
- argument rules and error order
- `is_sorted_u32`
- random sizes against the oracle
- odd and even executed passes (byte masks that make 1, 2, 3 or 4 passes execute)
- count table cleared on every call (stack pre-dirtied before each sort)

Local run (g++, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI.

## Not run

- A successful sort at n = kMaxCount (2^26 keys, 256 MiB per buffer). No large allocations in tests.
- Reading an uninitialised stack as a failure mode. It is not reliably observable without a memory sanitizer.
- MSVC, Clang, Windows and Android: no result seen.

## Mutation

85 source mutants, each built with `-fno-sanitize-recover=all`. 74 killed, 11 survived.

Survivors that are equivalent:
- pass over a uniform byte not skipped (keys and pairs): the pass is an identity permutation, and the copy-back handles the buffer swap.
- skip test reads `src[n-1]` instead of `src[0]` (keys): a count of n for one byte value means every key has that byte, so any index gives the same answer.
- sort early exit `n < 1` instead of `n < 2` (sorts and pairs): for n = 1 every pass is skipped because the count equals n, and nothing is copied.
- `is_sorted_u32` without `break`: the flag stays false and the loop only reads in-bounds keys, so the output is the same. Only the cost differs.

Survivors that are NOT equivalent:
- `n >= kMaxCount` instead of `n > kMaxCount` (sorts, pairs, is_sorted). They differ only at n = kMaxCount, which is not run (see above). Not killed.
- count table not fully initialised (keys: `count[255]`; pairs: same). A real defect, but a stale stack value is not observable without MSan. The pre-dirtied stack test did not kill it. Not killed.
