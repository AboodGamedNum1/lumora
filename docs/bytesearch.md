# bytesearch: byte string search

Code: `core/bytesearch/bytesearch.cpp` (contract in `core/bytesearch/bytesearch.h`). Tests: `tests/bytesearch/bytesearch_tests.cpp`. Reference: `tools/bytesearch_ref.py`.

## Rule

- A string is `s[0 .. n)`. Bytes compare as unsigned values. An occurrence of the needle at position p means p + nn <= nh and hay[p + i] == needle[i] for every i below nn.
- `bytes_find`: the smallest position of an occurrence. `bytes_find_last`: the largest position, even when occurrences overlap.
- `bytes_count`: occurrences counted from left to right without overlap; after a match at p the next search starts at p + nn.
- `bytes_equal_ascii_nocase`: true when na == nb and the strings are equal after folding only A to Z to a to z. Bytes 128 and above compare as they are. Two empty strings are equal.
- Empty needle: find gives found at position 0 (even in an empty haystack), find_last gives found at position nh, count gives EmptyNeedle.
- A needle longer than the haystack is Ok with found false (count 0).
- Not found writes `*out_found = false` and leaves `*out_pos` as it was. Found writes both.

Integer only: no allocation, no recursion, no global state, no libc string functions. The scan is a plain loop.

## Check order

1. NullArgument: any out pointer is null (always), or a string pointer is null while its length is above 0. A null pointer with length 0 is fine. This holds even when the other string is fine or the lengths differ.
2. TooLarge: either length above `kMaxLen` (2^28), also when the other string is fine.
3. EmptyNeedle, for `bytes_count` only.

On any failure nothing is written.

## Decisions

Eight readings (check order, empty needle, not-found out values, find_last on overlaps, greedy count, nocase length rule and ASCII-only folding, Not run scope, naive scan with no aliasing claim) were confirmed by the maintainer through the coordinating agent, with no corrections. A ninth reading, that nothing is read when no occurrence is possible, was rejected as a test assumption. The header guarantees only that a string of length 0 is never read and that a null or TooLarge failure comes before any read. This file makes no other claim about reads, and every success test uses buffers of exactly the declared length. No aliasing claim.

## Method

`bytes_find` and `bytes_count` scan forward from position 0 up to nh - nn. `bytes_find_last` scans backward from nh - nn to 0 and stops after position 0, so the unsigned counter does not wrap. The nocase function folds each byte of both strings through one small function.

## Tests

Expected values come from `tools/bytesearch_ref.py`, which builds every expected result from explicit slice comparisons (a list of every match position, then first, last and the greedy count), cross-checked against the Python bytes methods. It emits:
- 3000 find, find_last and count rows over the bytes {a, b, A, B, 0, 255, 128, '@', '['}, haystack length 0 to 40, needle length 0 to 6;
- 2000 nocase rows over the same bytes;
- an FNV-1a 64 digest over a 100000-byte haystack and 200 needles (an LCG with seed 271828), folding in found, position, last found, last position and count per needle.

8 test cases, 662959 assertions:
- the 3000 rows, each with exact-size haystack and needle vectors; the position starts as a sentinel and the found flag starts as the opposite of the expected value, so a missing write is seen
- the 2000 nocase rows
- the golden digest
- exhaustive: every haystack of length 0 to 10 and every needle of length 0 to 4 over {a, b}, against a position-list oracle written in the test file
- overlap and boundary cases by hand ("aaa"/"aa": first 0, last 1, count 1; "aaaa"/"aa": count 2; needle equal to haystack, longer than haystack, at the end; empty needle; bytes 0x7F, 0x80, 0xFF)
- found and not found out values
- nocase: all 65536 byte pairs against an oracle, the bytes next to the letter ranges, lengths, empty strings with null and non-null pointers
- argument rules: each out pointer null, each string pointer null with length above 0 and with length 0, TooLarge on either length (kMaxLen + 1 and 0xFFFFFFFF), EmptyNeedle only for count, null before TooLarge before EmptyNeedle, nothing written on failure. The buffers in the null and TooLarge cases are one byte, because those failures come before any read.

Local run (g++ 17, `-O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI. The test file has no compiler-specific constructs.

## Not run

- Any success that reads a string near `kMaxLen` (a search over 256 MiB, or nocase with na = nb = kMaxLen). Success paths use exact-size buffers only, so lengths of exactly kMaxLen on the success side are not exercised.
- MSVC, Clang, Windows and Android: no result seen.

## Mutation

146 source mutants of `core/bytesearch/bytesearch.cpp`, each built with ASan+UBSan and `-fno-sanitize-recover=all`, run against the full test file. 134 killed, 12 survived. Local result.

Of the 134 kills, 25 come from a crash, not a failed assertion, and are listed individually. A dropped null check dereferences a null pointer: `find no hay null`, `find no needle null`, `find null and`, `last no hay null`, `last no needle null`, `last null and`, `count no hay null`, `count no needle null`, `count null and`, `find no out null`, `find no found null`, `last no out null`, `last no found null`, `find out and`, `last out and`, `count no out null`, `nocase no out null`, `nocase no a null`, `nocase no b null`, `nocase null and`. Dropped size checks read past the one-byte buffers (the check cannot be seen otherwise): `count tl no nh`, `count tl and`, `count tl none`. Two scan mutants read out of bounds: `find no nn guard` (the unsigned bound wraps when the needle is longer) and `last starts nh` (the first window starts past the end). The other 109 are killed by a failed assertion.

Survivors that are NOT equivalent (8). They change the result only for a length of exactly `kMaxLen` on a success path, which needs a real 256 MiB buffer and is in Not run by decision:
- `find tl nh >=`, `last tl nh >=`, `count tl nh >=`: the haystack limit written as `>=` instead of `>`.
- `find tl nn >=`, `last tl nn >=`, `count tl nn >=`: the same for the needle.
- `nocase tl na >=`, `nocase tl nb >=`: the same for the two nocase lengths.
Condition: a call with a length of exactly 2^28 on that argument and a buffer of that size. With a length of 2^28 + 1 they all return TooLarge as required, which the tests check.

Survivors that are equivalent (4):
- `find no empty branch` and `last no empty branch`: without the early return for nn == 0, the scan starts at p = 0 (find) or p = nh (find_last), the empty comparison succeeds at once, and the same position and found value are written. The branch is for clarity.
- `fold upper instead`: folding a to z up to A to Z instead of A to Z down gives the same equivalence classes, so the equality result is the same for every pair of bytes (the 65536-pair test confirms the result either way).
- `nocase no break`: after the first difference `equal` is already false and nothing sets it back to true, so the remaining iterations change nothing.
