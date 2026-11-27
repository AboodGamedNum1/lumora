# perm: next arrangement, rank and unrank

Code: `core/perm/perm.cpp` (contract in `core/perm/perm.h`). Tests: `tests/perm/perm_tests.cpp`. Reference: `tools/perm_ref.py`.

## Rule

- `next_permutation_u32`: rearranges `a[0 .. n)` into the next arrangement in lexicographic order of the unsigned values. Duplicates are allowed and equal values never swap with each other, so a multiset is visited without repeats. When a next arrangement exists, `*out_has_next = true`. When `a` is already the last one (non-increasing), it becomes the first (ascending) and `*out_has_next = false`. n 0 and n 1: no change, false. The result is Ok in every one of these cases.
- `perm_rank`: the position of a permutation of 0 .. n-1 among the n! permutations in lexicographic order, from 0 (identity 0, reverse n! - 1). n 0 gives 0. Values that are not each of 0 .. n-1 exactly once give NotPermutation.
- `perm_unrank`: the inverse. OutOfRange when rank is not below n!. n 0 has exactly one permutation, rank 0.
- Limits: n up to 2^20 for next, n up to 20 for rank and unrank (20! = 2432902008176640000 fits in 64 bits, so no overflow anywhere).

Integer only: no allocation, no recursion, no global state, no 128-bit types.

## Method

- Next: find the longest non-increasing tail, swap the element before it with the rightmost tail element that is strictly larger, then reverse the tail. With no such element the whole array is the tail, and reversing it gives the first arrangement.
- Rank: for each position, the number of later smaller values times the factorial of the remaining length, summed. Validation uses a 32-bit seen-mask (n is at most 20), before anything is computed.
- Unrank: repeated division by the factorials, picking the idx-th unused value each time.
- Factorials come from a loop, not a table.

## Check order

NullArgument first: any out pointer is null (`out_has_next`, `out_rank`: always), or an array pointer is null while n is above 0 (`a`, `perm`, `out_perm`). A null array with n 0 is fine. Then TooLarge: n above 2^20 for next, above 20 for rank and unrank, also when the array is a tiny buffer. A null array with n above the limit gives NullArgument, not TooLarge. Then NotPermutation (rank) or OutOfRange (unrank). On any failure nothing is written.

## Decisions

Eight readings were confirmed by the maintainer through the coordinating agent, with no corrections: the check order above, the next rules for the last arrangement and n 0 and 1, NotPermutation for any value at or above n or any duplicate, no rank overflow at n = 20, the OutOfRange and n 0 rules for unrank, real buffers on every success path, a real 4 MiB test at n = 2^20, and no aliasing claim. The seen-mask is this implementation's choice and not part of the contract.

## Tests

Expected values come from `tools/perm_ref.py`, which does not use the C++ algorithm:
- Next arrangements come from the sorted list of distinct arrangements built with `itertools` (the entry after the input, or the first entry when the input is last).
- Ranks come from a counting argument with exact Python integers, cross-checked in Python by the inverse (division) and, for n up to 8, by the index in the enumeration.
- Digests (FNV-1a 64) of the full lexicographic walk for n 1 to 8, and of the full walk of three multisets (60, 630 and 30 distinct arrangements).

10 test cases, 256434 assertions:
- 5761 next rows (every array of length 0 to 6 over {0, 2, 0x80000000, 0xFFFFFFFF}, and 300 random arrays of length 7 over four values), with the found flag starting as the opposite of the expected value
- 939 rank and unrank rows for n 0 to 20 (identity, reverse, 40 random, and ranks 1, n! - 2, n!/2)
- the full walk for n 1 to 8 by repeated next, with digest, rank equal to the step, unrank equal to the array, count equal to n!, and the wrap to ascending
- the three multiset walks (count and digest)
- hand-checked next cases (n 0, n 1, all equal, pivot with duplicates, unsigned comparison of 0x80000000 and 0xFFFFFFFF)
- next at n = 2^20 with a real 4 MiB array: strictly descending becomes ascending with false, then the next call swaps the last two and gives true
- rank and unrank at n = 20 (identity, reverse = 20! - 1 = 2432902008176639999, the factorial table)
- rank rejects every array of length 0 to 4 over values 0 to 5 that is not a permutation, plus large values, a duplicate of the neighbour and a duplicate at the far ends at n = 20
- unrank range rules for every n up to 20 (n! is OutOfRange, n! - 1 is the reverse, 2^64 - 1 is OutOfRange, nothing written)
- argument rules and check order, with one-element or null buffers only for null and TooLarge failures (n = 2^20 + 1 and n = 21 included, and TooLarge before NotPermutation and before OutOfRange)

Local run (g++ 17, `-O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI. The test file has no compiler-specific constructs.

## Not run

- Every permutation for n above 8, and every rank at n = 20 (20! is about 2.4e18). Covered: n 1 to 8 completely, 40 random permutations and boundary ranks for each n up to 20, the reverse and identity at n = 20.
- Exhaustive next over arrays longer than 7 entries or with larger alphabets. Covered: all arrays up to length 6 over four values, 300 of length 7, three full multiset walks, the 2^20 case.
- MSVC, Clang, Windows and Android: no result seen.

## Mutation

100 source mutants of `core/perm/perm.cpp`, each built with ASan+UBSan and `-fno-sanitize-recover=all`, run against the full test file. 92 killed, 8 survived. Local result.

Of the 92 kills, 12 come from a crash, not a failed assertion, and are listed individually. Dropped null checks dereference a null pointer: `next no out null`, `rank no out null`, `rank no perm null`, `rank perm null n>1`, `unrank no null`, `unrank null n>1`. Dropped or loosened size checks run past the declared arrays or shift by 32 or more: `rank tl +1`, `rank tl none`, `unrank tl none`. Broken unrank selection loops run past the used mask: `unrank idx`, `unrank idx==0 cmp`, `unrank no dec`. The other 80 are killed by a failed assertion, including every exact-limit mutant (`>=` instead of `>` on the limits 2^20 and 20, which the n = 2^20 and n = 20 success tests and the n = 2^20 + 1 and n = 21 failure tests separate).

No survivor is non-equivalent. The 8 survivors are all equivalent:
- `fact i from 1 same`: the factorial loop starting at 1 instead of 2 multiplies by 1 once.
- `next small n<1`: the early return for n below 2 becomes n below 1; n = 1 then takes the general path, finds no pivot (tail 0), reverses nothing and writes false, the same result.
- `next j leftmost`: searching for the successor by walking right from the tail start while the value stays above the pivot finds the same element as walking left from the end, because the tail is non-increasing and its elements above the pivot form a prefix.
- `next rev <=`: the reverse loop also runs when lo equals hi, which swaps an element with itself.
- `rank seen xor`: `seen` toggles instead of sets, but a duplicate is rejected before the second visit, so every bit is toggled at most once and is equal to a set.
- `rank smaller <=`: the validated array has distinct values, so a later value is never equal to the current one.
- `rank k start`: starting the inner loop at i compares the value with itself, which is never smaller.
- `rank loop i<n-1`: the last position has no later values, so its term is 0 times 0! = 0.
