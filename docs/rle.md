# Run-length coding

`core/rle` encodes and decodes byte buffers as run-length pairs. The rules are in the comments of `core/rle/rle.h`; this page adds the
decisions, the bounds, the tests and the mutation results.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no floating point and no 128-bit
type. Evidence label: local unit tests only.

## The rule in short

The format is a sequence of pairs (b0, b1) meaning b1 repeated b0 + 1 times, so a run is 1 to 256 bytes. The empty buffer encodes to zero
bytes. The encoder takes greedy maximal runs cut at 256, and its output is the only canonical form: two neighbouring pairs hold the same
value only when the first of them is a full run (b0 == 255). The decoder accepts exactly that form.

## Decisions (confirmed by the maintainer)

1. `decode` checks, per pair and in order, that the pair is canonical and then adds its length to the running total. A pair that is both
   non-canonical and over the limit gives NonCanonical; an earlier overflow still wins over a later non-canonical pair.
2. The previous pair is the pair right before it in the input. The first pair has none. The same value after a full run (b0 == 255) is
   canonical, any number of times in a row.
3. Check order of `decode`: NullArgument (src null with n above 0, dst null with dst_cap above 0, out_len null), then odd n is Malformed,
   then n above 2^25 is TooLarge, then the scan, then BufferTooSmall. Odd n above 2^25 is Malformed. A decode of n == 0 may have null `src`
   or `dst` and gives Ok with length 0.
4. A raw total of exactly 2^24 is accepted and one more is TooLarge, even when the destination is smaller. A destination of exactly the
   raw size is Ok. A null `dst` needs `dst_cap` 0.
5. `encode` checks NullArgument (the same three checks), then n above 2^24 is TooLarge, then BufferTooSmall from an exact size computed in
   a pass that writes nothing. n == 0 is Ok with length 0 even with a null destination.
6. Only the result bytes are written, and a failure leaves `*out_len` and the destination untouched. The header makes no promise about
   `src` and `dst` overlapping, so nothing here relies on it or tests it.

## Bounds

`encode` makes two linear passes (size, then write), reading each source byte once per pass, and writes at most 2 x n bytes, at most
2^25. `decode` makes a scan over the pairs and then a write pass, at most 2^24 raw bytes. No storage beyond the arguments. Sums are kept in
64 bits.

## Tests

`tools/rle_ref.py` follows the rule text literally with Python integers and its own FNV. Its encoder cuts runs with `itertools.groupby`
and splits them into chunks of 256, a different method from the greedy scan in the C++ code. Its decoder follows the check order of the
header, and for every stream it accepts it asserts that re-encoding the output gives the input back. It prints the constants pasted into
`tests/rle/rle_tests.cpp`: 19 seeded encode cases (the input generator is mirrored in the test; run patterns that sit around 256, 512 and
the cut) and 30 decode streams embedded as byte arrays (valid, odd, non-canonical, full-run repeats, random pairs). Each case is checked by
a digest of the status, the length and the bytes for several destination capacities, including capacity one below and equal to the size.

Other tests: hand-computed encodes at 255, 256, 257, 511, 512 and 513 bytes; every check and its order for both functions, with a
sentinel proving that failures write nothing and leave `*out_len` alone; the input limit (2^24 bytes encode to exactly 2^25); the raw
limit of 2^24 and the order inside the scan; and a structural oracle on 300 random buffers (round trip, canonical form, size). All
destinations are exact-size heap vectors under ASan.

## Mutation testing

Evidence label: local only. 77 real source mutants of `core/rle/rle.cpp` were built and run against the test file, each one a single
edit. They cover: the run length scan (cut at 255, 256 and 257, bounds, start); each null check of both functions, including the cases
with n == 0 or a zero capacity; the size limits and their order against the other checks; the encoded size (step, comparison, missing
check); the written bytes and the loop steps; the odd check and its position; the canonical rule (value, previous b0, first pair, position
relative to the total, result code); the running total (step, limit, comparison, result code); the capacity check; the scan and write
loops; and every output length.

71 of the 77 were killed. Six survive, and each is equivalent:

- The scan comparing `src[i + len]` with `src[i + len - 1]` instead of `src[i]`. Every byte before it in the run already equals `src[i]`,
  so the two comparisons agree.
- The run length starting at 0 instead of 1. The first test compares `src[i]` with itself, which is true, and moves to 1 with the same
  checks, so the loop continues exactly as before.
- The encoded size kept in 32 bits. It is at most 2 x n and n is at most 2^24 after the TooLarge check, so it is at most 2^25.
- The first-pair guard written as `i > 1` instead of `i > 0`. The index `i` is always even, so the two tests agree.
- The raw total kept in 32 bits in `decode`. The total is checked against 2^24 after each pair, so it never exceeds 2^24 + 256 before the
  function returns.
- `*out_len` set from the raw total instead of the write count. On success both are the sum of the pair lengths.
