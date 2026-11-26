# Grid pathfinding (A*)

`core/pathfind/pathfind.h` and `core/pathfind/pathfind.cpp` implement `find_path`, a deterministic A* search on a 2D cost
grid, plus `validate_map`, `step_cost`, the status texts and `digest_path`. Pure CPU, no I/O, no threads. The interface is
in `pathfind.h`; this page records every rule.

## Determinism

The search uses integers only: no floats, no libm, no hashing, no pointer values, no unordered containers. Costs are
64-bit inside the search. The open set is a binary heap built with `std::push_heap` and `std::pop_heap` over entries whose
order is a strict total order (below), so the result does not depend on the standard library. Output is identical on
Linux, Windows and Android.

## Map and step cost

- `validate_map` checks in this order: `BadSize` (width or height is 0 or above `kMaxDim` = 4096), then `NullData`. `BadCost`
  exists in the enum and is never produced, because every byte is a valid cost.
- A cell byte of 0 is blocked. A byte `c` from 1 to 255 is the cost to enter that cell.
- `step_cost(c, diagonal)` is `c * 10` for an orthogonal step and `c * 14` for a diagonal step. `step_cost(0, ...)` is 0.
  The largest step is 255 * 14 = 3570.
- The start cell is never charged, whatever its byte. The goal cell is charged like any entered cell.

## Moves

- `Moves::Four` uses N (0,-1), E (1,0), S (0,1), W (-1,0). `Moves::Eight` uses those four first, then NE (1,-1), SE (1,1),
  SW (-1,1), NW (-1,-1). Neighbours are examined in exactly this order.
- No corner cutting: a diagonal step from (x,y) to (x+dx,y+dy) needs both (x+dx,y) and (x,y+dy) to be inside the map and not
  blocked, as well as the target cell.
- Any other `Moves` value gives `BadMoves`.

## Order of checks

`find_path` checks in this order and stops at the first failure: `BadMap` (`validate_map` is not `Ok`), `OutOfBounds`
(start or goal outside the map, including negative values), `Blocked` (start or goal cell is 0), `BadMoves`. Then it
searches. Every non-`Found` result except `BufferTooSmall` has `total_cost`, `path_length` and `expanded` equal to 0, except
`NoPath`, which reports in `expanded` how many cells were expanded before the open set ran out. Nothing is written to
`out_path` on an error status.

## Heuristic and tie-breaking

The heuristic is `10 * max(dx,dy) + 4 * min(dx,dy)` for `Eight` and `10 * (dx + dy)` for `Four`, with dx and dy the distances
to the goal. It is admissible and consistent for all costs from 1 to 255, so the first time the goal is popped its cost is
optimal.

The open set is ordered by `f = g + h`, then lower `h`, then lower `y`, then lower `x` (the heap entry stores the row-major
index `y * width + x`, so the last key is row-major). Entries are never decreased in place: a better route pushes a new entry
and the old one is skipped when it is popped (the cell is already closed).

A cell's parent changes only when a new route is strictly cheaper (`g` strictly lower). The first parent found at the
lowest cost wins, which together with the neighbour order fixes the path among equal cost routes.

`expanded` counts every cell that is popped and expanded once, including the goal cell, and no stale entry.

## Result

- `Found` with a one cell path and `total_cost` 0 when start equals goal.
- `path_length` is the number of cells including start and goal. The path is written start to goal.
- `BufferTooSmall` is returned only when `out_path` is not null and the path is longer than `capacity`. The first
  `capacity` cells are written, `path_length` is the true length and `total_cost` is the full cost. A path of one cell does
  not fit in capacity 0.
- A null `out_path` means length only: the status is `Found`, nothing is written, whatever `capacity` is.

## Total cost overflow

`total_cost` is 32 bits but the search is 64 bits. The largest possible true total is about 6e10 (16,777,215 steps of
3570), above 2^32 - 1, and a long forced corridor of high cost cells can reach it. The rule: if the true optimal cost is
greater than 4,294,967,294, the result keeps status `Found` and the exact optimal path, and `total_cost` saturates to
`0xFFFFFFFF`. No status is added. The value is a safe sentinel: every real total is a sum of multiples of 10 and 14, so it is
always even, and `0xFFFFFFFF` is odd. Read it as "at least 4,294,967,295". The optimal choice is made on the 64-bit costs, so
saturation never changes the path.

## Memory and time

Memory is linear in the map size: per cell a 64-bit cost, a direction byte and a closed byte (10 bytes), plus the open set,
which holds at most one entry per push (16 bytes each; a cell can be pushed several times). All large buffers are on the
heap. At 4096 x 4096 the per-cell arrays are about 160 MiB.

Measured locally on Linux with `-DLUMORA_SANITIZE=ON` (address and undefined sanitizers, GCC): the 4096 x 4096 performance
test case, which holds the 16 MiB map, a 2,000,000 cell path buffer and both searches, peaks at about 430 MiB resident
including sanitizer overhead. The open map search takes about 1.8 s and the 1,048,831 cell serpentine path about 0.3 s
sanitized; the tests allow 10 s.

## Tests

`tests/pathfind/pathfind_tests.cpp` covers every status and the check order, `validate_map`, all 256 values of `step_cost`,
4 and 8 moves, corner cutting, uniform and varying costs, exact tie-determined paths, start equal goal, unreachable goals,
narrow gaps, `BufferTooSmall` and length-only calls, the saturation rule, and optimality against a brute force Dijkstra on
600 seeded random maps (own LCG, both move sets) with every returned path checked for valid, adjacent, not blocked cells and
a cost that adds up. The expected tie paths and digests were computed by an independent implementation of the rules above,
not by the code under test. The 4096 x 4096 tests find a path on an open map with 3 percent random obstacles and on a
serpentine corridor; no 4096 x 4096 map in the tests is unreachable.

## Golden path digests

`digest_path` is FNV-1a 64 over x then y of each cell, each as a little-endian 4 byte two's complement value. The empty
path digests to `0xcbf29ce484222325`. These digests are in the tests and must match on every platform:

| Map (see the tests) | Moves | Cost | Cells | Digest |
| --- | --- | --- | --- | --- |
| M1, 16 x 7 maze | Eight | 250 | 26 | `0x3eb88fde32a26c10` |
| M2, 12 x 6 mixed costs | Four | 300 | 23 | `0xf6ce76ca4ef438be` |
| M2, 12 x 6 mixed costs | Eight | 228 | 15 | `0x7d3fd793f881efe0` |
| M3, 8 x 5 rooms | Eight | 150 | 16 | `0x873458ab65f4d281` |

## Evidence limits

Results above are from one Linux machine. CI results for Windows and other compilers are reported on the pull request, not
here.
