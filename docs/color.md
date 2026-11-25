# Color and gradients

`core/color/color.h` and `core/color/color.cpp` implement sRGB transfer functions, 8-bit helpers, OKLab conversion,
alpha operations and gradients. Pure CPU code with no I/O. The interface is in `color.h`; this page records the rules
the implementation follows.

## Determinism

Results must be bit-identical on Linux, Windows and Android. The code calls no libm function (`pow`, `exp`, `log`,
`cbrt` and the rest). Own `log`, `exp`, `pow` and `cbrt` use only `+ - * /` and exact bit operations, and are computed
in `double`. Source pragmas ask compilers not to fuse multiply-add, and the project must not enable fast-math. The
golden digests below were also produced with `-O0`, `-O3 -march=native -ffp-contract=fast` and `-Os` and did not
change.

## Custom functions and maximum error

Errors are relative, measured over a dense sweep against a `long double` reference in a scratch program on a local
Linux build (local results, not CI). Results are rounded to `float` at the interface.

| Function | Method | Maximum relative error |
| --- | --- | --- |
| `log(x)` | exponent split, `2 atanh((m-1)/(m+1))`, series to `s^33` | 4.1e-16 |
| `exp(x)` | `x = k ln2 + r`, Taylor series to `r^22`, scale by `2^k` | 1.8e-16 |
| `cbrt(x)` | `exp(log(x)/3)` plus one Newton step | 1.6e-16 |
| `srgb_to_linear` (power `2.4`) | `exp(2.4 log(x))` | 2.1e-15 |
| `linear_to_srgb` (power `1/2.4`) | `exp(log(x) / 2.4)` | 1.3e-15 |

The tests compare against hard-coded values from a 60 digit decimal implementation of the same formulas: sRGB
functions to 2e-7 relative (float rounding) and OKLab to 2e-6 absolute.

## Transfer functions and 8-bit helpers

- `srgb_to_linear`: `v / 12.92` for `v <= 0.04045`, else `((v + 0.055) / 1.055)^2.4`.
- `linear_to_srgb`: `12.92 v` for `v <= 0.0031308`, else `1.055 v^(1/2.4) - 0.055`.
- Inputs are clamped to 0..1 and NaN maps to 0, so infinities clamp.
- `to_u8`: clamp (NaN to 0), `floor(v * 255 + 0.5)` in double, which is nearest with ties away from zero. For example
  `0.5` gives 128. `from_u8` is `v / 255` in float. Every 8-bit sRGB value survives
  8-bit to linear to 8-bit exactly (tested for all 256).

## OKLab

Matrices are Ottosson's published OKLab matrices, applied in `double` to linear sRGB. `srgb_to_oklab` clamps inputs
like `srgb_to_linear`. `oklab_to_srgb` cubes the LMS values, converts back and clamps the result to 0..1 (out of gamut
values clamp per channel, NaN gives 0). A negative LMS value uses a signed cube root.

## Alpha

- `premultiply`: rgb times alpha.
- `unpremultiply`: rgb divided by alpha. Alpha 0 gives all four channels 0.
- `over(src, dst)`: straight colors, result straight. `ao = a_s + a_d (1 - a_s)` and
  `rgb = (c_s a_s + c_d a_d (1 - a_s)) / ao`. When `ao` is 0 all four channels are 0. No conversion happens inside.

## Gradients

Validation order: `TooFewStops` (count under 2), `TooManyStops` (over 16), then `BadPosition` for all stops (NaN,
outside 0..1, or decreasing; equal is allowed), then `BadColor` for all stops (NaN or outside 0..1 in any channel),
then `BadSpace`. The first failing group in this order is returned.

Sampling (`sample_gradient`):

- `t` is clamped to 0..1 and NaN gives 0. An invalid gradient returns `{0,0,0,0}`.
- Before the first stop and after the last stop the end colors hold.
- The active span starts at the last stop whose position is `<= t`, so at equal positions the later stop wins for
  `t >= position` (hard step).
- A `t` exactly on a stop returns that stop's color unchanged, with no color-space round trip.
- Otherwise `u = (t - p0) / (p1 - p0)`; the three color channels are mixed linearly in the gradient space (sRGB,
  linear light or OKLab) in `double`, converted back to sRGB and clamped; alpha is mixed linearly.

`bake_gradient_u8` samples `t = i / (n - 1)` in float (`n == 1` gives `t = 0`) and packs RGBA8 with `to_u8`. It returns
false and leaves the output untouched for an invalid gradient, `n == 0` or `n > 65536`.

## Digest

`digest_u8` is standard 64-bit FNV-1a over `n` bytes in input order: offset `0xcbf29ce484222325`, prime
`0x100000001b3`, per byte `h ^= byte; h *= prime`. It depends on byte order.

## Golden digests

The golden gradient has five stops: red at 0 (alpha 1), yellow at 0.3 (alpha 0.5), a blue at 0.6 (alpha 1), a green at
0.6 (alpha 0, a hard step) and white at 1. It is baked to 256 entries and hashed as 1024 bytes. The stops are in
`golden()` in `tests/color/color_tests.cpp`, which checks the same values.

| Space | Digest |
| --- | --- |
| sRGB | `0xcc797225a83824b7` |
| Linear | `0x12d82711acf8f9e7` |
| OKLab | `0x5a312dc44ca9558a` |

## Evidence limits

The tests are CPU only. Numbers measured on a local sanitized Linux build are local results. Only the CI jobs of the
pull request count as CI evidence, and no GPU, device or other-compiler claim is made here.
