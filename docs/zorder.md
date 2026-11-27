# zorder: Morton codes and the Hilbert curve

Code: `core/zorder/zorder.cpp` (contract in `core/zorder/zorder.h`). Tests: `tests/zorder/zorder_tests.cpp`. Reference: `tools/zorder_ref.py`.

## Rule

- Morton 2D: bit i of x goes to bit 2i of the code, bit i of y to bit 2i + 1. Every 16-bit x, y and every 32-bit code is valid.
- Morton 3D: bit i of x goes to bit 3i, of y to bit 3i + 1, of z to bit 3i + 2. Coordinates are at most 1023 and the code uses the low 30 bits.
- Hilbert 2D, order 1 to 16, on the grid 0 .. 2^order - 1: the standard curve. Order 1 visits (0,0), (0,1), (1,1), (1,0) in this order (x first). The curve of order k has four quadrants in that same order. Quadrant 0 holds the order k-1 curve with x and y swapped, quadrants 1 and 2 hold it unchanged, quadrant 3 holds it mirrored along the anti-diagonal (x -> 2s-1-y, y -> s-1-x for a half side s). So the orientation alternates with the order, as in the usual rotate-and-flip routines. Consecutive distances are always adjacent cells.

Integer only: no allocation, no recursion, no global state.

## Method

Morton: the usual mask-and-shift bit spreading (16 bits to every second bit, 10 bits to every third bit) and its inverse. Hilbert: one loop over the bit levels with the rotate-and-flip step, from the top level down for `xy2d` and from the bottom level up for `d2xy`. At order 16 the side is 65536 and distances go up to 2^32 - 1, all in 32-bit arithmetic.

## Check order

Null out pointers first (`morton2_decode`: x or y; `morton3_encode`: out; `morton3_decode`: x, y or z; `hilbert_xy2d`: out_d; `hilbert_d2xy`: out_x or out_y). For Morton 3D then OutOfRange: any coordinate above 1023 (encode), or a code with bit 30 or 31 set (decode). For Hilbert then BadOrder (order 0 or above 16), then OutOfRange: a coordinate at or above 2^order, or a distance at or above 4^order. At order 16 every 32-bit distance is valid, so `d2xy` never gives OutOfRange there. On any failure the out pointers are left untouched.

## Decisions

Seven readings were confirmed by the maintainer through the coordinating agent, with no corrections. The maintainer checked the standard routine against the geometric recursion for orders 1 to 8, all distances, and accepted the geometric reference. No aliasing claim between the out pointers.

## Tests

Expected values come from `tools/zorder_ref.py`. Morton codes are built there with a plain bit-by-bit loop. The Hilbert curve is built from its geometric recursive definition, with no bit routine. The reference emits sample tables and digests (FNV-1a, 64 bit):
- Morton 2D: 1542 samples (corners, single bits, random pairs); a digest over the 256 x 256 square.
- Morton 3D: 1537 samples (corners, single bits, random triples); a digest over the 16 x 16 x 16 cube.
- Hilbert: every cell of orders 1 to 4, and 132 boundary and random cells for each of orders 5 to 16 (first and last cells, quadrant boundaries, random distances); a digest of the whole curve for each of orders 1 to 8.

11 test cases, 5234427 assertions:
- Morton 2D samples, both directions
- Morton 2D digest; exhaustive over x, y in 0 .. 1023 (1,048,576 pairs) against a plain bit-loop oracle written in the test file, with the decode round trip; 20000 random 32-bit codes decoded and re-encoded; edge codes
- Morton 2D null arguments
- Morton 3D samples, both directions
- Morton 3D digest; exhaustive over x, y, z in 0 .. 31 against an oracle with the decode round trip; 20000 random codes below 2^30 round trip
- Morton 3D range and null rules (1023 valid, 1024 not, on each axis; bits 30 and 31; null before range; outputs untouched)
- Hilbert samples, both directions
- Hilbert order 1 anchored to the header
- Hilbert orders 1 to 8, all distances: digest, every cell visited once, consecutive cells adjacent, `xy2d(d2xy(d)) == d`, start (0,0) and end (side-1, 0)
- Hilbert orders 9 to 16: 400 random distances per order with the round trip and adjacency to the next distance, the last cell of each grid, the largest distance at order 16
- Hilbert argument rules and check order for every order 1 to 16 (coordinate at 2^order, distance at 4^order, bad orders, null before bad order before range, nothing written on failure)

Local run (g++, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`, ASan+UBSan): all passed, no warnings, no runtime errors. This is a local result, not CI.

## Not run

- All 2^32 Morton 2D codes and all 2^32 coordinate pairs. Covered: the 1024 x 1024 square exhaustively, 20000 random codes, samples with every single bit.
- All 2^30 Morton 3D codes and all 1024^3 triples. Covered: the 32^3 cube exhaustively, 20000 random codes, samples with every single bit.
- Hilbert orders 9 to 16 are not exhaustive (order 16 has 2^32 cells). Covered: random distances with round trip and adjacency, boundary cells, reference samples. Orders 1 to 8 are exhaustive.
- MSVC, Clang, Windows and Android: no result seen. The test file has no compiler-specific constructs.

## Mutation

110 source mutants of `core/zorder/zorder.cpp`, each built with ASan+UBSan and `-fno-sanitize-recover=all`, run against the full test file. 103 killed, 7 survived. Local result.

Of the 103 kills, 11 come from a crash, not from a failed assertion, and are listed individually: `m2d null x`, `m2d null y`, `m2d null and`, `m3e null`, `m3d null x`, `m3d null y`, `m3d null z`, `h2d null`, `d2x null x`, `d2x null y` (a null pointer is dereferenced instead of returning NullArgument), and `d2x range <=` (the check also runs at order 16, where the shift by 32 is undefined behaviour caught by UBSan). The other 92 are killed by a failed assertion.

Survivors, all equivalent (7):
- `spread2` without its first `v &= 0xFFFF`: the only callers pass a 16-bit value (`morton2_encode` takes u16), so the mask removes nothing.
- `spread3` without its first `v &= 0x3FF`: the only caller is `morton3_encode`, which has already rejected every value above 1023.
- `gather2` without its last `& 0x0000FFFF`: the only callers are in `morton2_decode`, which casts the result to 16 bits, so the extra high bits are dropped.
- `morton2_encode` with `^` instead of `|`: the two spread values occupy disjoint bits (even and odd positions), so OR and XOR agree.
- `hilbert_xy2d` calling `rotate(s, ...)` instead of `rotate(n, ...)`: the flip `s-1-x` and `n-1-x` differ only in bits at and above s (unsigned wrap-around), and the later levels read only bits below s.
- `hilbert_xy2d` calling `rotate(n, y, x, ...)` with the two arguments exchanged: the step flips both values or neither, then swaps them, which is symmetric in its two arguments, so the final pair is the same.

No survivor is non-equivalent.
