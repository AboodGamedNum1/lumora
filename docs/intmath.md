# Integer math

`core/intmath/intmath.h` and `intmath.cpp` (`lumora::intmath`) are exact integer helpers: `isqrt_u64`, `gcd_u64`, `lcm_u64`, checked signed `add_i64`, `sub_i64`, `mul_i64`, and `mul_div_u64` (a * b / d with an exact 128-bit product).

Evidence label: unit tests only (local runs; CI is reported separately in the pull request).

Integer only: no floating point, no libm, no `__int128`, no compiler builtins or intrinsics (MSVC must compile it), no allocation, no recursion, no mutable statics. The source includes only the header, which includes `<cstdint>`.

## Rule

- `isqrt_u64(x)` is the largest `r` with `r * r <= x`, exact for every 64-bit `x`. It uses the digit-by-digit method (one result bit per step, shifts and subtractions only); `isqrt_u64(UINT64_MAX)` is 4294967295.
- `gcd_u64`: Euclid with `%`, no recursion. `gcd(0, 0)` is 0, `gcd(a, 0)` is `a`.
- `lcm_u64(a, b, out)`: false when `out` is null (also when an operand is 0). `lcm(0, x)`, `lcm(x, 0)` and `lcm(0, 0)` are 0. Otherwise `a / gcd * b`, false only when that exceeds `UINT64_MAX` (a result of exactly `UINT64_MAX` is valid). `*out` is untouched on failure.
- `add_i64`, `sub_i64`, `mul_i64`: false when `out` is null or the exact result is outside int64. `INT64_MIN` is a valid operand and result. The overflow tests are done before the operation (no signed overflow ever happens). `mul_i64` works on magnitudes: the product of two magnitudes must not exceed `2^63` when the signs differ and `2^63 - 1` when they agree.
- `mul_div_u64(a, b, d, mode, out)`: the 128-bit product is built from 32-bit halves, then divided by `d` with a shift-subtract long division (128 steps, remainder kept in one 64-bit word with an explicit carry). `Down` is the floor, `Up` the ceiling (an exact quotient is not bumped), `NearestUp` adds 1 when the remainder is at least `d - remainder` (a remainder of exactly half rounds up; written this way to avoid overflow). A quotient above 64 bits, or a bump that carries `UINT64_MAX` to `2^64`, is false. False also for a null `out`, `d == 0` and a mode outside the three, including when the product is 0. A zero product with a valid `d` and mode gives 0. `*out` is untouched on failure.
- The order in which `mul_div_u64` detects failures is not part of the contract and is not tested.

## Decisions

Confirmed by the maintainer (relayed through the coordinating agent) as the numbered readings above, including: the mode check applies even for a zero product; a bump that overflows is false; `lcm` with a null `out` is false even for zero operands; the 128-bit intermediate may be built by any exact method; `isqrt` is tested around roots (the root itself, `root^2 - 1`, `root^2`, `root^2 + 1`) and is not exhaustive over 2^64. No aliasing between `out` and the operands is claimed or tested (operands are passed by value).

## Bounds

| Item | Bound |
| --- | --- |
| unsigned operands | any `uint64_t` |
| signed operands | any `int64_t`, including `INT64_MIN` |
| `mul_div_u64` result | at most `UINT64_MAX` after rounding |

## Tests

`tools/intmath_ref.py` is an independent reference in Python on exact integers, with no fixed-width arithmetic. The square root is found by bisection (not `math.isqrt`), `lcm` is `a * b // gcd` checked against 2^64, signed results are checked against the int64 range, and `mul_div` is `divmod` of the exact product with the three rounding rules. The standard `math` module is only a second check inside the script (assertions). Operands come from a seeded generator mirrored in the C++ test (special values, and random values of every bit length from 0 to 64, with common-factor pairs for gcd and lcm, equal or opposite pairs for the signed functions, and exact-tie and carry-heavy cases for `mul_div`). It prints 7 digests, each over 3000 results, pasted into the tests.

`tests/intmath/intmath_tests.cpp` has 7 test cases and 2387166 assertions in the last local run: the 7 seeded digests; `isqrt` for every `x` below 2^20, around every `k^2` (`k` itself, `k^2 - 1`, `k^2`, `k^2 + 1`, `k^2 + 2k`, `k^2 + 2k + 1`) for 20000 roots up to 4294967295, and 20000 random values by the defining inequality; gcd and lcm edges, properties (symmetry, divisibility, `gcd(a/g, b/g) == 1`, `lcm * gcd == a * b`) and the overflow border at `UINT64_MAX`; every add, sub and mul edge around `INT64_MIN` and `INT64_MAX` with `out` untouched on failure; a `mul_div_u64` rounding table of 30 rows checked in all three modes (ties, exact, zero product, `UINT64_MAX` products, 2^64 quotients); three witnesses where the floor is exactly `UINT64_MAX` with a remainder (Down fits, Up never fits, NearestUp fits only below half); all failure rules in every combination; and 20000 random cases against plain 64-bit arithmetic when the product fits.

Local run: g++ with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined`, no warnings, no runtime errors, all passed. No other compiler was run.

## Not run

- `isqrt_u64` is not checked for every 64-bit input: 2^64 values are out of reach. It is checked exhaustively below 2^20, around 20000 perfect squares, and on 20000 random values.
- `mul_div_u64`, `mul_i64`, `lcm_u64` and the other functions are not checked exhaustively over their 128-bit or 2^128 input spaces; they are checked on the edge tables and the seeded streams above.

## Mutation check

119 hand-written source mutants of `intmath.cpp` (every half, partial product and shift of the wide multiply; every step of the square root; Euclid's loop; the lcm zero cases, quotient and overflow border; both overflow conditions of add and sub; the zero cases, signs, magnitudes and limits of `mul_i64`; every check of `mul_div_u64`, the long division start, end, word choice, carry and subtraction, the quotient check, the three rounding rules, the carry guard and the result) were built against the unchanged tests under ASan and UBSan, with `-fno-sanitize-recover=all` on the mutated file so that undefined behavior counts as a kill. 112 were killed and 7 survive.

Survivors, all equivalent under the conditions stated:

- `isqrt` without the initial `while (bit > x) bit >>= 2`: while `bit > x`, the loop body takes the else branch with `res == 0`, so `res >>= 1` leaves 0 and only `bit` shrinks; the result is the same.
- `lcm_u64` handling only `b == 0` as the zero case: for `a == 0` and `b != 0`, `gcd` is `b`, so `q = 0 / b = 0` and the result is `0 * b = 0`, with no division by zero because `b != 0`.
- `add_i64` with `b >= 0` instead of `b > 0` in the condition choice: for `b == 0` the positive test is `a > INT64_MAX`, which is never true, so it matches the negative test result (also never true).
- `sub_i64` with `b >= 0` instead of `b > 0`: for `b == 0` the test is `a < INT64_MIN`, never true; same result.
- `mul_i64` handling only `b == 0` as the zero case: for `a == 0` and `b != 0`, `ua == 0`, so `0 > limit / ub` is false and `m == 0`; `*out` is 0 for either sign. (The opposite mutant, handling only `a == 0`, divides by zero for `b == 0` and is killed.)
- `mul_div_u64` without the `d == 0` check (the null check kept): with `d == 0` every long division step has `rem >= d`, so all 128 quotient bits are set, `qhi != 0` and the function returns false before writing `*out`. No division is executed. The observable result is the same.
- `mul_div_u64` with `mode != Round::Down` instead of `mode == Round::NearestUp` in the last branch: that branch is reached only when `mode != Round::Up`, and any mode outside the three is rejected earlier, so `mode != Round::Down` there means `NearestUp`.
