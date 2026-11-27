# Histogram

`core/histogram` builds a 256-bin histogram of an 8-bit image and answers percentile, mean and contrast-stretch questions from the bins.
The rules are in the comments of `core/histogram/histogram.h`; this page adds the decisions, the bounds, the tests and the mutation
results.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no floating point and no 128-bit
type. Evidence label: local unit tests only.

## The rule in short

- `build` counts a w x h image (row r starts at `src + r * src_stride`, only the first w bytes of each row are read) into the bins. Check
  order: null `src` or `out`, then w or h of 0 or above 4096 (BadSize), then a stride below w or above 65536 (BadStride). A failure leaves
  `*out` as it was. On Ok every bin is overwritten, not added to.
- `total` is always the sum of the bins in 64 bits. There is no stored total.
- `percentile(per_mille)`: target = ceil(total x per_mille / 1000). Per-mille 0 gives the smallest value whose bin is not 0, otherwise
  the smallest v with bins[0..v] summing to at least the target. 1000 gives the largest value whose bin is not 0.
- `mean_q8` is (sum of v x bins[v] x 256 + total / 2) / total, rounded half up.
- `stretch_lut(lo_pm, hi_pm)`: lo and hi are the two percentiles. If lo equals hi the table is the identity for every v. Otherwise 0 up to
  and including lo, 255 from hi, and a rounded linear ramp between.

## Decisions (confirmed by the maintainer)

1. `build` counts into a local array and copies it to `*out` only at the end, so every failure leaves `*out` untouched. The header makes
   no promise about `out` and `src` overlapping, so nothing here relies on it or tests it.
2. The check order is null, then size, then stride; a bad size with a bad stride gives BadSize.
3. Percentile, mean and stretch need a total above 0 for every per-mille including 0, and return false with the output untouched when it
   is 0. A per-mille above 1000 is false even for an empty histogram.
4. When lo equals hi the identity table wins over the 0 and 255 rule for every v. `lo_pm >= hi_pm`, `hi_pm > 1000` and a total of 0
   give false and leave the table unchanged.
5. All sums are plain 64-bit and never clamped, including hand-built bins of 4294967295. The largest weighted sum is below 2^57 and the
   Q8 mean is at most 65280.
6. Results depend only on the bins. A histogram that did not come from `build` goes through the same formulas.

## Bounds

`build` reads at most w x h bytes in h passes of w bytes, at most 2^24 pixels, and nothing between or after the rows. The other functions
make one or two passes over 256 bins. Storage is one local array of 256 counters. `total x per_mille` is at most 256 x (2^32 - 1) x 1000,
about 1.1 x 10^12, so it fits in 64 bits.

## Tests

`tools/histogram_ref.py` follows the rule text literally with Python integers and its own FNV. For images it counts with plain Python
and finds each percentile a second way, by sorting the pixels and reading the value at rank target - 1, and it asserts that this agrees
with the cumulative scan used for hand-built histograms. It prints the constants pasted into `tests/histogram/histogram_tests.cpp`: 18
image cases (strides above the width with random bytes between the rows, several pixel patterns, sizes up to 4096 wide and a stride of
65536) and 12 seeded hand-built histograms (bins up to 4294967295, many empty bins). Each case is checked by a digest of the bins, the
total, the mean, 13 percentiles and 10 stretch tables, including the failing ones.

Other tests: a hand-counted small image with padding; the check order and every failure leaving `*out` unchanged; the accepted limits
(4096 wide, 4096 tall, stride 65536); the largest image (2^24 pixels); `total` above 2^32; percentile, mean and stretch by hand including
ceil, ties and rounding; the failure rules with sentinels; and a structural oracle on 200 random images (bins equal a manual count, the
percentile is a used value and never decreases with per-mille, the table is monotonic and 0 at lo and 255 at hi). Image buffers are
exact-size heap vectors under ASan.

## Mutation testing

Evidence label: local only. 97 real source mutants of `core/histogram/histogram.cpp` were built and run against the test file, each one a
single edit. They cover: the bin sum; the per-mille 0 path; the target rounding and its 64-bit arithmetic; the cumulative scan and its
end; every null, size and stride check, their limits and their order; the row and column loops and the row offset; overwrite versus add
and the copy loop; the mean sum, rounding, scale and 32-bit versions; the stretch checks, the percentile arguments, the identity rule
and its position, the 0 and 255 rules, the rounding and the table loop; and every return value and output write.

90 of the 97 were killed. Seven survive, and each is equivalent:

- `percentile_of` for per-mille 0 without the `v + 1 < kBins` bound, or with the bound written as `v < 255`. The function is only called
  with a total above 0, so some bin is not 0 and the scan stops there, before the bound could matter.
- The final scan loop running to 256 bins instead of 255. The running sum reaches the total, which is at least the target, at the last
  bin at the latest, so the loop returns 255 there, which is the value the fallback returns anyway.
- The row offset computed as `static_cast<u32>(y * src_stride)`. After the checks y is below 4096 and the stride is at most 65536, so the
  product is below 2^28 and has the same value in 32 and in 64 bits.
- The weighted sum starting at value 1 instead of 0. Value 0 adds 0 x bins[0].
- The stretch ramp using `v < lo` instead of `v <= lo` in the 0 rule. For v equal to lo (with lo below hi) the ramp gives
  (0 x 255 + d / 2) / d, and d / 2 is below d, so it is 0 as well.
- The 255 rule using `v > hi` instead of `v >= hi`. For v equal to hi the ramp gives (d x 255 + d / 2) / d, which is 255 because d / 2
  is below d.
