# Vector path drawing

`core/vecdraw` is the first end-to-end vector pipeline: a path of outline commands becomes an 8-bit anti-aliased coverage mask. It
only composes three merged modules and contains no flattening, stroking or rasterising logic of its own:

- `core/outline` (over `core/flatten`) turns the commands into flat contours with a closed flag.
- `core/stroke` turns one flat contour into convex counter-clockwise contours (stroke path only).
- `core/raster` fills contours into the mask.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state and no floating point. The
caller provides one work area. Evidence label: software (CPU) only.

## Units and limits

All coordinates are raster 24.8 fixed point, `|value| <= 2^24`. The tolerance (`1 .. 2^16`) and the half width (`1 .. 2^19`) use the same
units. A path has `1 .. 64` commands. The target is at most 8192 by 8192 with `stride >= width`.

## Fill

`fill_path` flattens the path with the outline builder and fills every contour with the given rule. Open contours are closed
implicitly by the rasteriser (the last point joins the first). Contours with fewer than 3 points are skipped. If no contour
qualifies the rasteriser is still called with zero contours and clears the mask. Contours are passed in outline order.

## Stroke

`stroke_path` strokes each outline contour with `core/stroke`. The `closed` field of the style is ignored: each contour uses its own
closed flag, also for a closed contour of two points. All resulting contours are filled together with the NonZero rule, in contour
order.

## Check order

Both functions return the first failure of this list and never write the target on any error.

1. `NullBuffer`: `commands`, `target.data` or `work` is null (all are always required).
2. `BadSize`: width or height is 0 or above 8192, or `stride < width`.
3. `BadCount`: `command_count` is 0 or above 64.
4. `BadTolerance`: outside `1 .. 2^16`.
5. `BadFill` (fill) or `BadStyle` (stroke): the rule is not a `Fill` value; or `half_width` outside `1 .. 2^19`, `miter_limit` outside
   `1 .. 64`, `join` or `cap` outside their enums. Stroke has no fill rule and fill has no style, so exactly one of them applies.
6. `BadCommand`, `BadPoint`, `BadPath`: the outline rules, in the outline order (all ops, then used points, then the sequence).
7. `WorkTooSmall`: `work_bytes < work_bytes_needed(command_count, stroke)`, or the work address is not a multiple of 8. This is the one
   place where a pointer value decides a status; the interface says so.
8. `Overlap`: any two of these three byte ranges intersect: the commands (`command_count * sizeof(Command)` bytes), the target rows
   (exactly `stride * (height - 1) + width` bytes) and the whole supplied work (`work_bytes` bytes, not only the needed part). Same modular
   address rule as the other modules: clear the top byte, take the unsigned difference modulo `2^64`, answer "no overlap" at
   `|delta| >= 2^47` before any other arithmetic, then compare the exact ranges. Real buffers cannot reach the `2^47` branch, so it is argued
   here and not tested.

Steps 1 to 5 look only at scalar arguments. Step 6 reuses the outline builder without touching any caller memory: it is called with dummy
local buffers of capacity 0. The outline builder reports BadCommand, BadPoint and BadPath before its capacity checks and writes nothing on
any error, so a `BufferTooSmall` answer means that all three checks passed. This is how the order of the interface (the path checks before
`WorkTooSmall` and `Overlap`) is kept without a second implementation of the path rules.

After the checks, two kinds of data dependent rejects exist for stroke only, both found before the rasteriser is called, so the target is
untouched:

- `TooManyPoints`: the work is first scanned: any flattened contour with more than 1024 points (`stroke::kMaxPoints`) gives this status
  before any stroking.
- Then the contours are stroked in order. For each contour the first violation wins: a generated vertex outside `+-2^24` gives `BadPoint`
  (the rasteriser would reject it, no clamping is done, because clamping would change the geometry); otherwise a running total of stroke
  contours above 4096 (`raster::kMaxContours`) gives `TooManyContours`. When one contour has both a vertex out of range and pushes the total
  over 4096, the vertex check comes first. This is a named test.

Valid inputs near the limit can generate stroke vertices beyond `2^24` (an offset of up to `2^19` per side, a miter tip up to `2^25`
away). That is why `BadPoint` is also a stroke result for a generated vertex. A test has a segment whose offset side lands exactly on
`+-2^24` (accepted) and the same one unit further (rejected).

## Work memory

`work_bytes_needed(n, stroke)` is `0` for an invalid `n`. Otherwise the work is carved by integer offsets into sections. Every section
size is rounded up to a multiple of 8 and the work address is a multiple of 8, so every section is 8-byte aligned. Point types
(`outline::Point`, `stroke::Point`, `raster::P`) are all two `int32` (8 bytes); the module copies point by point and never reinterprets
one type as another. A raster contour descriptor takes a fixed 16 bytes (`static_assert` that `sizeof(raster::Contour) <= 16`), so the
bound does not depend on the pointer size. Let `R(x)` be `x` rounded up to a multiple of 8, and `n` the command count.

Common to both paths, in this order:

| section | bytes | why |
| --- | --- | --- |
| outline points | `R(8 * 256 n) = 2048 n` | `outline::point_bound(n) = 256 n` points |
| outline contour ends | `R(4 n)` | `outline::contour_bound(n) = n` words |
| outline closed flags | `R(n)` | `n` bytes |

Fill adds:

| section | bytes | why |
| --- | --- | --- |
| raster point copies | `2048 n` | every outline point can be kept, `256 n` points of 8 bytes |
| raster descriptors | `16 n` | at most `n` contours (one per outline contour) |

So `work_bytes_needed(n, false) = 4096 n + 16 n + R(4 n) + R(n)`. For `n = 1` that is 4128 bytes and for `n = 64` it is 263488.

Stroke adds (all independent of `n`, because one polyline has at most `stroke::kMaxPoints = 1024` points, checked before stroking):

| section | bytes | why |
| --- | --- | --- |
| stroke input points | `8 * 1024 = 8192` | one contour, at most 1024 points |
| stroke output points | `8 * 8192 = 65536` | `stroke::point_bound(1024, closed) = 4 * 2048 = 8192` points |
| stroke output ends | `4 * 2048 = 8192` | `stroke::contour_bound(1024, closed) = 2048` words |
| raster point copies | `8 * 4 * 4096 = 131072` | at most 4096 kept contours, each a triangle or a quad (at most 4 points) |
| raster descriptors | `16 * 4096 = 65536` | at most 4096 kept contours |

So `work_bytes_needed(n, true) = 2048 n + R(4 n) + R(n) + 278528`. For `n = 1` that is 280592 bytes and for `n = 64` it is 409920.

The stroke buffers are sized for the largest polyline and reused for each contour, so the bound does not use the 4096 cap to shrink anything
except the kept raster copy. The kept copy is bounded by 4096 contours of at most 4 points because every stroke contour is a bevel
triangle (3 points) or a quad (4 points: segment, miter join, square cap, or the square of a collapsed point), and the running total never
exceeds 4096 (a violation returns before anything is kept). Nothing else is shrunk because of a runtime rejection. The sum of the sections
is also a multiple of 8, and the bound is exact for the sections above (the layout has no padding besides the rounding).

## Why the sub-module calls cannot fail after the checks

Each call below is followed by a branch that maps a non-Ok status to an error return. Those branches are unreachable.

`build_outline`: the pointers are non-null; the count is `1 .. 64` (at most 1024); the tolerance is in range; every op, used point and the
sequence passed the zero-capacity call, which runs the same code in the same order; the capacities passed are exactly `point_bound`,
`contour_bound` and `contour_bound`, so no `BufferTooSmall`; the six extents (commands, three work sections, the two local counters) are
pairwise disjoint: the sections are disjoint by construction of the offsets inside the supplied work, the commands are disjoint from the work
by the `Overlap` check, and the counters are locals of this function. The work is aligned to 8 and the offsets are multiples of 8, so the
`Point`, `uint32` and `uint8` arrays are aligned.

`stroke_polyline`: the pointers are non-null; the point count `m` is `2 .. 1024` (the outline emits contours of at least 2 points, and the
scan above rejects more than 1024); the style passed the style check (the contour only changes `closed`); every input coordinate is at most
`2^24` in magnitude (it is an outline point), below `stroke::kMaxCoord = 2^28`; the capacities are 8192 points and 2048 words, which are at
least `point_bound(m, closed)` and `contour_bound(m, closed)` for every valid `m`; the input, output points, output ends and the two local
counters are pairwise disjoint.

`fill_polygons`: `target.data` is non-null and the size, the rule and the descriptors are valid: the fill rule was checked (stroke uses
NonZero); the contour count is at most 64 for fill and at most 4096 for stroke; every contour has 3 or more and at most `2^20` points
(fill: contours of fewer than 3 points are skipped and an outline contour has at most `256 * 64 = 16384` points; stroke: 3 or 4 points);
every descriptor has a non-null point pointer (a section of the work); the contour array pointer is the non-null descriptor section also for
zero contours; every kept coordinate is within `2^24`: fill points are outline points (flatten keeps its points within the control
point hull), stroke points were checked one by one before being kept. The rasteriser finds all its errors before its first write to the
target (null, size, fill, contour and point checks, then the heap work), so even a rasteriser failure would leave the target untouched (read from `core/raster/raster.cpp` and stated in `docs/raster.md`). The rasteriser allocates its own band buffers on the heap, as documented in `docs/raster.md`; that is inside the raster
module and not a work area of this module.

The mask is written only by the last call, and only after every check and every reject.

## Tests

`tests/vecdraw/vecdraw_tests.cpp` holds the expected values from an independent Python reference: its own exact-integer flatten and subpath
state machine, its own stroker, and a pixel-piece rasteriser written from `docs/raster.md` (edge snapping, then per-pixel cover and
area, then the fold; it clips the same way the definition does, and was cross-checked against a padded unclipped canvas). The reference has
its own FNV and generators. The cases are:

- named paths in all modes with digest goldens: nesting, curves, open and closed paths, two-point closed contours, a lone move, collapsed
  points with square caps, clipping, and a path covering the whole target;
- extreme coordinates: the exact `+-2^24` vertex boundary for stroke (accepted, and one unit over rejected), a square cap, a miter tip,
  a contour of 1024 points (accepted) and 1025 points (stroke rejected, fill accepted), closed variants, TooManyPoints before BadPoint, and
  4095, 4096 and 4097 stroke contours with the fill of the 4097 case accepted, plus BadPoint beating the contour count in one contour;
- an exhaustive sweep of all command sequences of length 1 to 3 over the five ops with three point presets and four modes (14460 runs,
  2460 successes, the rest are path errors);
- three random corpora (seeds 0xA1, 0xB2, 0xC3) with status counts and digests;
- every combination of nine faults (and each of three null pointers) for fill and stroke, checking the check order and that nothing in the
  arena changed on a check error;
- work boundaries (`needed`, `needed - 1`, `-7`, `-8`, `0`, misaligned addresses, surplus bytes untouched, guard words intact), the exact
  formula for every `n`, and an exact-size work vector so the address sanitizer sees any overrun, also for the largest path (64 curve
  commands) and the 4096-contour stroke;
- overlap for all three pairs with exact adjacent and one-byte cases, including bytes inside the supplied work beyond the needed part and
  the `stride * (height - 1) + width` extent;
- style, tolerance, size and count limits at both sides; unused command fields and `style.closed` ignored; a path with no qualifying
  fill contour clears the mask;
- every successful mask is also compared with an oracle that calls the three public APIs in sequence on `std::vector` storage.

A mutation run over the module (55 mutants of checks, limits, layout sizes, copies and the check order) was caught except: the
overlap `2^47` cutoff (not reachable with real buffers, argued above); a stroke vertex check that looks at x only, which is equivalent
because the rasteriser reports the same `BadPoint` with the target untouched (the preflight matters for the order against
`TooManyContours`, which a named test covers); a comment-only edit; and the status mapping of an unreachable rasteriser `BadPoint`.
