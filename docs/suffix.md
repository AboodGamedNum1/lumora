# suffix: suffix array and LCP array of a byte string

Code: `core/suffix/suffix.cpp` (contract in `core/suffix/suffix.h`). Tests: `tests/suffix/suffix_tests.cpp`. Reference: `tools/suffix_ref.py`.

## Rule

- `suffix_array_u8(text, n, sa_out, scratch, scratch_words)`: `sa_out[0 .. n)` holds the start positions of all suffixes of text[0 .. n) in ascending order. Bytes compare as unsigned values (a NUL byte is an ordinary byte) and a proper prefix sorts before the longer string. All suffixes differ, so the order is unique.
- `lcp_array_u8(text, n, sa, lcp_out, scratch, scratch_words)`: `lcp_out[0] = 0` and, for i from 1 to n - 1, `lcp_out[i]` is the length of the longest common prefix of the suffixes at `sa[i-1]` and `sa[i]`. `sa` must hold each of 0 .. n - 1 exactly once, else BadSuffixArray. It is not checked that `sa` is the sorted suffix array of the text; for another permutation the values written are unspecified and are not asserted.
- Limit: n is at most `kMaxLen` = 65536.
- Scratch need, in u32 words: for the suffix array 3 * n when n is above 1, else 0; for the LCP array n when n is above 0, else 0. Both are computed in 64 bits; after the TooLarge check n is at most 65536, so a 32-bit computation would agree on every reachable input. Scratch contents are unspecified afterwards and this file does not assert any scratch value.
- n = 0 writes nothing and needs no non-null pointer. n = 1 gives `sa_out[0] = 0` (no scratch needed), and an LCP of `{0}` (one scratch word needed).

Integer only: no allocation, no recursion, no mutable statics, no 128-bit types. No aliasing claim is made. Besides the caller scratch, `suffix_array_u8` uses one automatic array of 256 u32 counters (1 KiB of stack) for the first byte sort.

## Method

- Suffix array: prefix doubling with counting sorts, O(n log n) in time and iterative. The first pass is a stable counting sort on the byte value. Then, for step k = 1, 2, 4, ..., positions are ordered by the rank of position + k (suffixes without one first), stably counting-sorted by their own rank, and given new dense ranks, until all n ranks differ. The scratch holds the rank array, a temporary array and the counters (n words each); `rk` and `tmp` swap roles each round. Time does not depend on the text: there is no character-by-character comparison of suffixes, so equal bytes and periodic text are not special cases.
- LCP array: Kasai's method. The scratch holds the rank of each position; the same pass checks that `sa` is a permutation, before anything is written to `lcp_out`.

## Check order

- `suffix_array_u8`: NullArgument first (`text` and `sa_out` only while n is above 0, `scratch` only while the need is above 0), then TooLarge, then ScratchTooSmall. On any failure `sa_out` is left untouched.
- `lcp_array_u8`: NullArgument (`text`, `sa` and `lcp_out` only while n is above 0, `scratch` only while the need is above 0), TooLarge, ScratchTooSmall, then BadSuffixArray for a non-permutation `sa`. On any failure `lcp_out` is left untouched.

## Decisions

These readings were confirmed by the maintainer through the coordinating agent: the scratch needs and their 64-bit computation, the null rules at n = 0 and n = 1, BadSuffixArray as "not a permutation" checked after ScratchTooSmall with `lcp_out` untouched, a valid but unsorted permutation returning Ok with no assertion on the values (only the result code and memory safety), the use of one fixed 256-counter local array (1 KiB stack, no static, no thread_local, no heap, no recursion), and the way the no-quadratic requirement is shown (below).

## Tests

Expected values come from `tools/suffix_ref.py`. For small and mid texts it uses a naive sort of the suffixes (Python bytes comparison) and a direct byte-by-byte LCP, neither of which shares code or method with the C++. For the 65536-byte texts a naive sort is too heavy, so it uses tuple-key doubling with Python's sort and a separately written Kasai, cross-checked by a permutation check, 2500 adjacent suffix pairs compared by slices per text, and 2500 LCP values found by binary search on slice equality. The tests keep digests for those. Success tests use buffers of exactly the declared length under ASan; tiny or null buffers appear only for NullArgument, TooLarge, ScratchTooSmall and BadSuffixArray failures and for length-0 inputs.

- Exhaustive: every string over three byte values (0x00, 0x80, 0xFF) of length 1 to 7, and over two (0x00, 0xFF) of length 1 to 10, full suffix and LCP arrays.
- 60 mid-size texts (length 8 to 300): random binary, ternary and full-byte texts, periodic texts with and without one changed byte, equal bytes, and equal bytes with a different last byte.
- 11 texts of 65536 bytes: all 0x00, all 0xFF, period 2, 3 and 7, a Fibonacci word, Thue-Morse, equal bytes with a different last byte, pseudo-random binary, pseudo-random bytes and a repeated pseudo-random block. The suffix array and LCP array are compared by 64-bit digests, the LCP sum, and the first and last entries.
- A golden digest of every output, with scratch pre-filled with two different patterns (the result must not depend on it), and with the stack dirtied before each suffix array call.
- Hand values (banana, NUL bytes, unsigned order, n = 1), argument rules and check order for both functions, scratch size boundaries for n = 1, 2, 3, 5, 100, BadSuffixArray cases, and the LCP call on valid but unsorted permutations (result code only).

No-quadratic requirement: the tests do not measure time. The 65536-byte equal and periodic texts finishing is the evidence; a quadratic method would need on the order of 10^9 byte comparisons on them, and the harness timeout was 10 s per mutant. There is no wall-clock or step-count assertion.

## Local run

LOCAL only, not CI: g++ -std=c++17 -O1, ASan + UBSan, -Wall -Wextra -Wpedantic -Wconversion -Wshadow, no warnings and no runtime errors: 9 test cases, 21775 assertions, all passed. No MSVC, Clang, Windows or Android result was seen; Windows is covered by CI only.

## Not run

- Exhaustive strings beyond the lengths above, and any exhaustive 65536-byte case.
- Any timing or step-count assertion. Doubled-size timing was run once by hand (below) and is local evidence only.
- Memory sanitizer (the uninitialised-counter survivors below are argued, not detected).
- MSVC, Clang, Windows, Android.

## Mutation

104 source mutants (generated by a script that is not committed), each built with `-fno-sanitize-recover=all`, harness timeout 10 s. 58 were killed by an assertion, 31 by a crash only, 6 stopped by timeout, 9 were not killed. A few outcomes depend on stack contents (for example one mutant was a crash-only kill in one run and a timeout in another, before the stack was dirtied in the tests); the figures here are from the last full run.

### Crash-only kills (31)

suffix array: no null check on `text`; the null check with `&&` instead of `||`; no early return for n = 0; no n = 1 branch; first byte count masked to 7 bits; first byte count with a signed byte index; inclusive prefix in the first byte sort; first byte prefix loop one short; step multiplied by 3; second-key list built without subtracting k; inclusive prefix in the main counting sort; placement through the key of `tmp[0]`; new rank starting at 1; second key bound `<=`; new ranks from the first key only; new ranks from the second key only; no swap of `rk` and `tmp`.

lcp: need only for n above 1; need 0; no null check on `text`; no null check on `sa`; no null check on `lcp_out`; null check with `&&`; no null check on scratch; no early return for n = 0; no range check on entries of `sa`; range check `>`; loop bound on `i + h` as `<=`; no bound on `i + h`; no bound on `j + h`; bound on `j + h` as `<=`.

### Timeouts (6), listed apart from assertion kills

A timeout is not counted as an assertion kill. The class says what was shown.

- Non-termination, shown by a separate build without sanitizers on small inputs that did not finish in 3 s: `sa loop k start` (20 equal bytes and 20 varied bytes; step starts at 2), `sa loop <=` (the loop never ends once all ranks differ), `sa newrank and` (20 equal bytes and 20 varied bytes), `sa cnt count` (20 varied bytes; it finished on 20 equal bytes).
- Quadratic time, shown by doubled-size timing on equal bytes, local, -O1, no sanitizers: `sa loop k same` (step never grows): 14.0, 57.5, 225.1 and 909.3 ms at n = 2048, 4096, 8192 and 16384, about 4x per doubling; the real code takes 0.1, 0.2, 0.5 and 1.0 ms. `lcp dec reset` (h reset to 0 each position): 14.7, 59.0, 232.7 and 934.7 ms at n = 8192, 16384, 32768 and 65536, about 4x per doubling; the real code takes at most 0.1 ms.

### Not killed (9), none non-equivalent in result

- `sa need 32-bit`, `sa scratch 32-bit`, `lcp scratch 32-bit` (32-bit arithmetic or comparison): reached only after the TooLarge check, so n is at most 65536 and 3 * n fits. Equivalent on every reachable input.
- `sa first clear 255` (counter 255 not cleared) and `sa cnt clear classes-1` (the last counter not cleared): the starting value of the last counter is only used to compute a sum that is then dropped, so the result does not depend on it. A reading of an indeterminate value is formally undefined, which is why a memory sanitizer would flag it; none was run. The tests dirty the stack before each call and still see no difference in the result.
- `sa first place reverse` (first byte pass places equal bytes in reverse order): the doubling rounds order every tie by the second key, and equal ranks give equal results; the final array is unique. Equivalent in result.
- `sa sa2 reverse` (positions without a second key listed in reverse): those positions have different first ranks (each suffix shorter than k is its own class), so their order in the list does not decide anything. Equivalent in result.
- `lcp r==0 no reset` (h not reset at the smallest suffix): when the suffix at i is the smallest, the previous position had an LCP of at most 1, so h is already 0 after the decrement. Equivalent for a sorted suffix array. For another permutation the values are unspecified and not asserted.
- `sa loop k+1` (step grows by 1 instead of doubling): the result is the same, since each round still compares overlapping windows that cover the prefix. It is slower, not quadratic: on equal bytes at n = 16384, 32768 and 65536 it took 10.4, 29.9 and 85.8 ms (about 2.8x per doubling, local, -O1), against 1.0 ms for the real code at n = 16384. The tests do not measure time, so it survives. It is a time-only survivor, equivalent in result.

None of the 9 is non-equivalent in result.
