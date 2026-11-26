# Outline builder

The outline builder (`core/outline/outline.h`, `core/outline/outline.cpp`) turns a vector path of Move, Line, Quad, Cubic and Close
commands into flat contours (polylines with a closed flag) for core/stroke, core/tess and core/raster. Curves are flattened with
`lumora::flatten::flatten_quad` and `flatten_cubic`; the logic is not copied. It uses integer arithmetic only, no floating point,
no libm, no heap allocation, no recursion and no static mutable state. Results are identical on every platform. Evidence label:
software (CPU) only.

## Path rules

- The first command is a Move. After a Close only a Move or the end may follow. Line, Quad and Cubic need a current point, so they
  follow a Move, Line, Quad or Cubic. A Close needs a current point as well, so Close first, Close after Close and anything but a
  Move after a Close are BadPath. A Move after a Move is allowed, and so is a Close right after a Move. BadPath depends on the ops
  only, and the first violation in command order decides.
- A Move ends the subpath in progress as open (no implicit closing) and starts a new one at its point. Close ends it as closed.

## Building a contour

- A Move starts a contour with its point. A Line adds its end point. A Quad or Cubic is flattened from the current point (the last
  point of the contour) with the given tolerance and adds all flattened points after the first.
- A point equal to the last point of the contour is not added. This covers lines, flattened points and the point where a curve
  starts, so consecutive duplicates collapse across commands.
- A closed contour whose last point equals its first drops that last point. One drop is enough, because the last point differs from
  its predecessor by the rule above. Open contours are never closed or trimmed at the end.
- A contour is emitted only with at least two points after this. A lone Move, Move then Close and all-equal points are dropped without
  error. A closed contour with exactly two distinct points is kept (closed, two points). Contours come out in subpath order.

## Inputs and checks

Order: NullBuffer (all six pointers), BadCount (0 or above 1024), BadTolerance (outside `1..2^16`, checked even without curves),
BadCommand (every op in command order), BadPoint (used fields only, in command order), BadPath, BufferTooSmall (`pts_capacity <
point_bound`, `ends_capacity < contour_bound`, `closed_capacity < contour_bound`), Overlap. Nothing is written on an error: every
check happens before the first output write.

Used fields: Move `a`; Line `a`; Quad `a` and `b`; Cubic `a`, `b` and `c`; Close none. Unused fields are never read for a decision
(the tests fill them with out-of-range values). Overlap uses the shared rule: clear the top byte, take the unsigned difference
modulo `2^64`, answer no overlap at `|delta| >= 2^47` before other arithmetic, then compare exact byte ranges. All 15 pairs of six
extents are checked: commands (`command_count * sizeof(Command)` bytes), pts (`point_bound` points), contour_ends (`contour_bound`
words), closed (`contour_bound` bytes) and the two four-byte counters. The `2^47` branch cannot be reached with real buffers in one
process, so it is argued here and not tested; the mutation check that lowers it survives for that reason.

## Bounds

- `contour_bound = command_count`: every contour starts with a command (a Move), so there are at most as many contours as commands.
- `point_bound = 256 * command_count`: a Move or Line adds at most one point and a curve adds at most `n <= 256` points after its
  first (flatten gives `n + 1` points, the first is the current point). Every command therefore adds at most 256 points, and the total
  is at most `1024 * 256 = 262144 = 2^18`. Points of a dropped contour are removed again, so the bound also holds for the scratch use.
- Both are 0 for an invalid count.
- Output coordinates are input coordinates or flatten outputs. Flatten outputs lie in the convex hull of the control points, so every
  coordinate has magnitude at most `2^24` and fits `int32`. Counts are at most `2^18`, so every index fits `uint32`.

## Why flatten cannot fail after the checks

`flatten_quad` and `flatten_cubic` return NullBuffer for a null pointer, BadTolerance for a tolerance outside `1..2^16`, BadPoint for a
control point beyond `2^24`, BufferTooSmall for a capacity below 257 and Overlap for a counter inside the output extent. After the checks:
the tolerance is valid; the control and end points of the command are used fields, so they are range checked; the current point is the
last point of the contour, which is a previously checked used point or a flatten output within range; the output is a local array of
257 points with capacity 257; the counter is a separate local variable. So none of the five can occur. The code still maps a non-Ok
flatten status to an error return, which is unreachable, and the tests show that a bad used point is found before any output write.
The scratch array is 2056 bytes on the stack (no heap, no static).

## Digest

`digest_outline` is FNV-1a 64 over 32-bit little-endian words: always the contour count, so an empty outline hashes one zero word.
It stops there if the count is 0 or `contour_ends` or `closed` is null. Otherwise it hashes per contour the end index and the closed flag
(any nonzero byte is 1), and then, only with a non-null `pts`, the points `0 .. contour_ends[count - 1] - 1`, x then y.

## Tests

`tests/outline/outline_tests.cpp` holds explicit expected outlines for named paths, an exhaustive sweep of all command sequences up to
length four over the five ops (four point presets per command, two tolerances, 336840 runs), three random corpora (seeds 21, 22, 23; ranges
20, 2000, `2^24`), digest cases and null-pointer rules, unused field garbage, the state machine, every status and check order, no write on
error, overlap for all 15 pairs, a check that curve points come from core/flatten, and a performance case of 1024 extreme commands. The
expected values come from an independent Python reference (a subpath state machine over explicit lists, exact-integer Bernstein
flattening with its own segment count, own FNV and generator). Every output is also checked for contour size, consecutive duplicates, the
closing point, ranges and the flags.
