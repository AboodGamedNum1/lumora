# Easing curves and tween helper

`core/anim/easing.h` and `core/anim/easing.cpp` hold 22 easing curves, a CSS-style cubic Bezier timing function and a
tween helper. Pure CPU code with no dependencies. The interface is in `easing.h`; this page records the exact rules.

## Easing kinds

`ease(kind, t)` maps progress `t` to eased progress. The formulas are the standard Penner curves as published on
easings.net. All math is done in double and rounded to float once.

- `t` is clamped to [0, 1]. NaN and -infinity give 0, +infinity gives 1.
- `ease(kind, 0)` is exactly 0 and `ease(kind, 1)` is exactly 1 for every kind. These two cases are handled before any
  formula runs (Expo and Elastic formulas do not give exactly 0 at `t = 0`).
- The output is always finite. Back and Elastic overshoot outside [0, 1]; Bounce stays inside [0, 1].
- A value outside the enum behaves like Linear.

For `t` strictly between 0 and 1 (in-out kinds split at 0.5, `t < 0.5` is the first half):

| Name | Formula |
| --- | --- |
| `linear` | `t` |
| `quad-in` | `t^2` |
| `quad-out` | `1 - (1 - t)^2` |
| `quad-in-out` | `2 t^2`, else `1 - (-2 t + 2)^2 / 2` |
| `cubic-in` | `t^3` |
| `cubic-out` | `1 - (1 - t)^3` |
| `cubic-in-out` | `4 t^3`, else `1 - (-2 t + 2)^3 / 2` |
| `sine-in` | `1 - cos(t pi / 2)` |
| `sine-out` | `sin(t pi / 2)` |
| `sine-in-out` | `-(cos(pi t) - 1) / 2` |
| `expo-in` | `2^(10 t - 10)` |
| `expo-out` | `1 - 2^(-10 t)` |
| `expo-in-out` | `2^(20 t - 10) / 2`, else `(2 - 2^(-20 t + 10)) / 2` |
| `back-in` | `c3 t^3 - c1 t^2` |
| `back-out` | `1 + c3 (t - 1)^3 + c1 (t - 1)^2` |
| `back-in-out` | with `a = 2 t`: `a^2 ((c2 + 1) a - c2) / 2`; else with `a = 2 t - 2`: `(a^2 ((c2 + 1) a + c2) + 2) / 2` |
| `elastic-in` | `-2^(10 t - 10) sin((10 t - 10.75) c4)` |
| `elastic-out` | `2^(-10 t) sin((10 t - 0.75) c4) + 1` |
| `elastic-in-out` | `-(2^(20 t - 10) sin((20 t - 11.125) c5)) / 2`, else `(2^(-20 t + 10) sin((20 t - 11.125) c5)) / 2 + 1` |
| `bounce-in` | `1 - bounce-out(1 - t)` |
| `bounce-out` | see below |
| `bounce-in-out` | `(1 - bounce-out(1 - 2 t)) / 2`, else `(1 + bounce-out(2 t - 1)) / 2` |

Constants:

- Back: `c1 = 1.70158`, `c2 = c1 * 1.525 = 2.5949095`, `c3 = c1 + 1 = 2.70158`.
- Elastic: `c4 = 2 pi / 3` (in, out), `c5 = 2 pi / 4.5` (in-out).
- Bounce out, with `n1 = 7.5625` and `d1 = 2.75`:
  - `t < 1 / d1`: `n1 t^2`
  - `t < 2 / d1`: `n1 (t - 1.5 / d1)^2 + 0.75`
  - `t < 2.5 / d1`: `n1 (t - 2.25 / d1)^2 + 0.9375`
  - otherwise: `n1 (t - 2.625 / d1)^2 + 0.984375`

All in-out kinds satisfy `f(t) + f(1 - t) = 1`, and every family satisfies `out(t) = 1 - in(1 - t)`. The tests check both
within the tolerance below.

## Names

`ease_name` returns a fixed lower-case name for every kind and `unknown` for a value outside the enum.
`ease_from_name` accepts exactly these names and leaves `out` untouched for anything else (any other case, spaces,
underscores, an empty string, extra characters, embedded NUL bytes, or very long text):

`linear`, `quad-in`, `quad-out`, `quad-in-out`, `cubic-in`, `cubic-out`, `cubic-in-out`, `sine-in`, `sine-out`,
`sine-in-out`, `expo-in`, `expo-out`, `expo-in-out`, `back-in`, `back-out`, `back-in-out`, `elastic-in`, `elastic-out`,
`elastic-in-out`, `bounce-in`, `bounce-out`, `bounce-in-out`.

## Cubic Bezier

`cubic_bezier(x1, y1, x2, y2, t, out)` is the CSS timing function through (0, 0), (x1, y1), (x2, y2) and (1, 1).

- It returns false and leaves `out` untouched unless all four numbers are finite and `x1` and `x2` are in [0, 1]. `y1`
  and `y2` may be any finite numbers.
- `t` is clamped like `ease`: NaN and -infinity give 0, +infinity gives 1. `t <= 0` gives exactly 0 and `t >= 1`
  gives exactly 1.
- With `x(s) = 3 (1 - s)^2 s x1 + 3 (1 - s) s^2 x2 + s^3`, the solver finds `s` with `x(s) = t`, then returns `y(s)`.
  Because `x1` and `x2` are in [0, 1], `x(s)` never decreases on [0, 1], so the curve is monotonic in x.
- Solver bound: at most 8 Newton iterations starting from `s = t`, accepted when `|x(s) - t| < 1e-9`. Newton stops early
  and the solver falls back to bisection when the derivative is not above 1e-6 or a step leaves (0, 1). Bisection runs
  at most 60 iterations on [0, 1] and stops when `|x(s) - t| < 1e-9`. The error in x is far below the documented 1e-6.
  The work per call is bounded (68 evaluations of the polynomial at most).
- The result is rounded to float and clamped to the largest finite float.

## Tween

`tween_value(tween, time, out)` evaluates a tween at an absolute time.

- Invalid tweens return false and leave `out` untouched: a non-finite `from`, `to`, `duration` or `delay`, a duration
  that is not above 0 (including -0), or a negative delay. A delay of -0 is valid and the same as 0.
- If `time` is before the delay, or NaN, the value is `from`.
- Otherwise the elapsed time `time - delay` (computed in double) goes through `apply_loop(elapsed, duration, loop)`.
  `apply_loop` maps a NaN or infinite time to 0, so +infinity gives `from` in every loop mode, including Clamp
  (a non-finite clock counts as not started). The same is true for -infinity, which is before the delay.
- The progress is `looped / duration`, passed to `ease`. Eased progress 0 gives exactly `from` and 1 gives exactly `to`.
  Otherwise the value is `from + (to - from) * eased` in double, clamped to the finite float range. So a tween from
  -3e38 to 3e38 stays finite, and an overshooting kind (Back, Elastic) is clamped to the largest finite float instead
  of overflowing.
- Loop modes follow `docs/animation.md`: Clamp holds the end, Repeat wraps (an exact multiple of the duration gives the
  start value), PingPong mirrors (exactly the duration gives the end value).

## Tolerance and libm

`sin`, `cos` and `pow` come from the C library and can differ in the last bits between libraries. Everything else is
exact IEEE arithmetic. The documented tolerance between hosts and against the reference values is **1e-6 relative (plus
1e-6 absolute near zero)** for `ease`, `cubic_bezier` and `tween_value`. Results do not depend on the rounding mode set
by the caller (no code reads or sets it) and the source asks MSVC and Clang not to fuse multiply-add. The project must
not use fast-math.

## Usage

```cpp
#include "easing.h"
using namespace lumora::anim;

float p = ease(Ease::CubicInOut, 0.25f);

Ease kind;
if (ease_from_name("back-out", kind)) { /* use kind */ }

float y;
if (cubic_bezier(0.25f, 0.1f, 0.25f, 1.0f, 0.5f, y)) { /* CSS "ease" */ }

Tween t;
t.from = 0; t.to = 100; t.duration = 2; t.delay = 0.5f; t.easing = Ease::QuadOut; t.loop = Loop::PingPong;
float v;
if (tween_value(t, seconds, v)) { /* use v */ }
```

## Tests and evidence limits

Tests are in `tests/anim/easing_tests.cpp` (Catch2, target `lumora_anim_tests`). They cover exact endpoints and
non-finite inputs for every kind, reference values at five times per kind (from a separate Python implementation of the
formulas above), the symmetry rules, a fixed-seed sweep of 100000 values per kind (finite for all, within [0, 1] and
monotonic for every kind except Back, Elastic and Bounce), the name table and near misses, the CSS keyword curves,
400 fixed-seed Bezier control sets, the tween rules and edge cases, and ten million `ease` calls under a 30 second cap.

- Evidence comes from CPU unit tests run in CI and locally. Nothing measures rendering, a GPU or real animation content.
- The 30 second cap is a generous limit, not a benchmark.
- The cross-host tolerance is a documented design limit that the CI platforms exercise. It is not a proof for every C
  library.
- The CSS keyword reference values come from a bisection solver, not from a browser.
