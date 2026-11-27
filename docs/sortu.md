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

9 test cases, 26539 assertions:
- the 91 scenarios against the reference
- stability with many equal keys against an insertion-sort oracle
- every byte position and extreme keys
- argument rules and error order
- `is_sorted_u32`
- random sizes against the oracle
- odd and even executed passes (byte masks that make 1, 2, 3 or 4 passes execute)
- count table cleared on every call (stack pre-dirtied before each sort)
- stale count entry 255: four keys, the first with low byte 0xFF and the others unsorted with other low bytes. A separate non-inlined function fills about 64 KiB of volatile stack with the value 3, then the sort runs (called before any REQUIRE, because the Catch2 assertion handler would reuse that stack). One section for keys, one for pairs.

Local run (g++, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI.

## Not run

- A successful sort at n = kMaxCount (2^26 keys, 256 MiB per buffer). No large allocations in tests.
- Reading an uninitialised stack as a failure mode. It is not reliably observable without a memory sanitizer.
- MSVC, Clang, Windows and Android: no result seen.

## Mutation

85 source mutants. Each is built with `-fno-sanitize-recover=all` in the sanitizer run. The suite is run in two builds, both local:

- Plain build: g++ `-O1`, sortu.cpp and the tests without instrumentation (the prebuilt Catch2 library is instrumented and linked with the sanitizer flags). 76 killed, 9 survived.
- Sanitizer build: g++ `-O1` with ASan+UBSan. 74 killed, 11 survived.

The two builds differ only on the two count-table mutants below. In the sanitizer build ASan changes the stack layout of the count table, so the stale value from the fill function does not land on entry 255 and the mutants survive. In the plain build the stale-entry test kills both.

Survivors in both builds, equivalent (6):
- pass over a uniform byte not skipped (keys, pairs): the pass is an identity permutation, and the copy-back handles the buffer swap.
- skip test reads `src[n-1]` instead of `src[0]` (keys): a count of n for one byte value means every key has that byte, so any index gives the same answer.
- sort early exit `n < 1` instead of `n < 2` (sorts, pairs): for n = 1 every pass is skipped because the count equals n, and nothing is copied.
- `is_sorted_u32` without `break`: the flag stays false and the loop only reads in-bounds keys, so the output is the same. Only the cost differs.

Survivors in both builds, NOT equivalent (3):
- `n >= kMaxCount` instead of `n > kMaxCount` (sorts, pairs, is_sorted). They differ only at n = kMaxCount, which is not run (see above). Not killed.

Survivors in the sanitizer build only, NOT equivalent (2):
- count table not fully initialised (keys: `count[255]`; pairs: same). With a stale 3 in entry 255, the first key's byte (0xFF, true count 1) reads 4 = n, so the pass is wrongly skipped. Killed in the plain build by the stale-entry test. Not killed under ASan+UBSan, where the stack layout differs. It is a layout-dependent check, not a guarantee, and does not replace a memory sanitizer.
