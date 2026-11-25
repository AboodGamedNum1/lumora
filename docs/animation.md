# Animation clips and sampler

`core/anim/animation.h` and `core/anim/animation.cpp` hold keyframe tracks (1 to 4 floats, or a quaternion rotation),
a deterministic sampler and a draft clip JSON parser. Pure CPU code: no GPU, no file system, no dependencies. The
interface is in `animation.h`; this page records the exact rules the implementation follows.

## Statuses

`validate_track` checks in this order and returns the first problem:

| Status | When |
| --- | --- |
| `NoKeys` | `times` is empty. |
| `TooManyKeys` | more than `kMaxKeysPerTrack` (1048576) keys. Checked before any value count is compared. |
| `BadComponents` | `components` is not 1 to 4, or `is_rotation` with `components != 4`. |
| `BadValueCount` | `values.size()` is not `keys * components` (Step, Linear) or `keys * components * 3` (CubicSpline). Counts are 64-bit and compared before anything is indexed. |
| `TimesNotIncreasing` | a time is negative, NaN or infinite, or a time is not greater than the one before. Negative zero counts as zero. |
| `NonFinite` | any value, including a tangent, is NaN or infinite. |
| `ZeroQuaternion` | a rotation key has length zero. For CubicSpline only the value element is checked, tangents may be zero. |
| `BadTarget` | the target is empty, longer than 128 bytes, not valid UTF-8 (no overlong forms, surrogates or code points above U+10FFFF) or contains a control character (below 0x20 or 0x7F). |

`track_status_text` returns a fixed short text for every status and never input bytes. `kMaxTracksPerClip` (4096) is
enforced by `parse_clip_json`; a `Clip` built in code is not limited by it.

`track_duration` is the last key time of a valid track and 0 for an invalid one. `clip_duration` is the largest
`track_duration`, so invalid tracks count as 0.

## Sampling

`sample_track(track, time, out)` writes `components` floats and returns true, or returns false and leaves `out`
untouched when the track is not valid or `out` is null.

- **Cost.** It runs the full `validate_track` on every call, so each call costs O(number of keys) before the
  O(log n) interval search. Validate once and keep the track if you sample often; a loop over many samples of a very
  long track pays for the validation each time.
- **Outside the key range** the first or last key value is held. With a single key the value is held everywhere. At a
  key time the key value is returned exactly (rotations are returned normalized).
- **Non-finite times.** NaN gives the first key, +infinity the last key, -infinity the first key.
- **Interval.** The interval is found by binary search on `times`: the first key with a time greater than the sample
  time, and the key before it. With key times `t0 < t1` and sample time `t`, let `dt = t1 - t0` and `u = (t - t0) / dt`.
- **Step** returns the value of the left key (for a time before the next key).
- **Linear** returns `v0 + (v1 - v0) * u` per component.
- **Rotation, Linear** uses normalized shortest-path slerp. Both keys are normalized first. Let `d` be their dot
  product. If `d < 0`, the second key is negated and `d` becomes `-d` (q and -q are the same rotation). If `d > 0.9995`
  the result is the normalized `a * (1 - u) + b * u` (normalized lerp). Otherwise, with `theta = acos(d)`, the result is
  `(sin((1 - u) * theta) * a + sin(u * theta) * b) / sin(theta)`, normalized again to remove rounding drift. Keys that
  are exactly opposite (`q` and `-q`) become parallel after the flip and give that rotation.
- **CubicSpline** follows glTF 2.0. Each key stores in-tangent, value and out-tangent, in that order, `components`
  floats each. With `v0` and `m0` (out-tangent) from key k and `v1` and `m1` (in-tangent) from key k+1:
  `p(u) = (2u^3 - 3u^2 + 1) * v0 + (u^3 - 2u^2 + u) * dt * m0 + (-2u^3 + 3u^2) * v1 + (u^3 - u^2) * dt * m1`.
  For a rotation the four components are interpolated this way and the result is normalized. If that vector has
  length zero, the normalized left key is returned instead.
- **Finite output.** All math is done in double and rounded to float once. A result that would overflow float is
  clamped to the largest finite float. No output is NaN or infinite, and no vector is read outside its size.
- **Step and CubicSpline rotations at keys** return the key value normalized.

## Loop mapping

`apply_loop(time, duration, mode)` returns a value in `[0, duration]`.

- A NaN or infinite `time` or `duration`, a `time` that is negative or zero, and a `duration` that is zero or negative
  all give 0. This holds for every mode, including Repeat and PingPong.
- **Clamp:** `min(time, duration)`. A time of exactly the duration gives the duration.
- **Repeat:** `fmod(time, duration)`, so the result is in `[0, duration)`. An exact multiple of the duration,
  including `time == duration`, gives 0.
- **PingPong:** `m = fmod(time, 2 * duration)`, then `m` if `m <= duration`, else `2 * duration - m`. A time of exactly the
  duration gives the duration, and exactly twice the duration gives 0.
- The arithmetic is done in double (so `2 * duration` cannot overflow) and the result is cast back to float.
  `fmod` is exact, so times such as 1e30 give the exact remainder.

Sampling time infinity or NaN is handled by `sample_track` as above; `apply_loop` is a separate step the caller
applies to the absolute time first.

## Tolerance and libm

Everything uses IEEE operations whose results are exactly specified (add, subtract, multiply, divide, `sqrt`, `fmod`,
comparisons) except `acos` and `sin`, which are used only by rotation slerp (and only when the keys are not nearly
parallel). Different C libraries can differ in the last bit of those calls, so Linux and Windows rotation results are
documented to agree within **1e-6 relative (and 1e-6 absolute for components near zero)**. The tests use that
tolerance for every slerp value. Every other function is expected to agree bit for bit when the compiler does not
fuse multiply-add or use fast-math. The source asks MSVC and Clang not to contract multiply-add; the project must not
enable fast-math.

Results do not depend on pointers, uninitialized memory or the order of calls: a copy of a track at another address
gives identical samples (tested).

## Clip JSON (draft proposal)

**This schema is a draft proposal.** It is not part of the frozen package format until the owner accepts it.

```json
{
  "name": "intro",
  "tracks": [
    {
      "target": "layer/hills/position",
      "components": 2,
      "rotation": false,
      "interpolation": "linear",
      "times": [0, 1, 2],
      "values": [0, 0, 10, 5, 20, 0]
    }
  ]
}
```

- Top level: an object. `name` is optional (a string of at most 128 bytes, valid UTF-8, no control characters).
  `tracks` is required, an array of at most 4096 track objects.
- Track: `target` (string, required, the same rules as `BadTarget`), `components` (whole number 1 to 4, required),
  `rotation` (true or false, optional, default false), `interpolation` (`"step"`, `"linear"` or `"cubicspline"`,
  optional, default `"linear"`), `times` and `values` (arrays of numbers, required). Values for `cubicspline` use the
  in, value, out layout above.
- Unknown fields are ignored. Duplicate keys anywhere are an error (the existing parser rejects them).
- Limits: text at most 4 MiB, nesting depth at most 8, at most 1048576 keys per track, and at most 1048576 numbers
  (times plus values) in the whole clip. A number outside the float range is an error. After reading, every track is
  checked with `validate_track`.
- Errors: `ParseClipResult::message` is a fixed text that starts with a JSON path, such as
  `tracks[2].components: must be a whole number from 1 to 4` or `tracks[0]: times must be finite, not negative and
  strictly increasing`. Only indices (counts) and fixed words appear; input text never does. On any failure `ok` is
  false and `clip` is empty (no name, no tracks). On success `message` is `ok`.

## Usage

```cpp
#include "animation.h"
using namespace lumora::anim;

ParseClipResult r = parse_clip_json(text);
if (!r.ok) { /* report r.message */ }
float out[4];
for (const Track& t : r.clip.tracks) {
  float local = apply_loop(now, track_duration(t), Loop::Repeat);
  if (sample_track(t, local, out)) { /* use t.components floats */ }
}
```

## Tests and evidence limits

Tests are in `tests/anim/anim_tests.cpp` (Catch2, target `lumora_anim_tests`). They cover every status, hand-computed
Step, Linear and CubicSpline values at key, quarter and mid times, slerp unit length, shortest path, endpoints,
180 degrees and parallel keys, the loop tables, 240 fixed-seed random tracks (finite output, key values, Linear
continuity, unit rotations), the JSON errors and hostile inputs, and a 100000-key track with a time cap.

- Evidence comes from CPU unit tests run in CI and locally. Nothing here measures GPU, rendering or real content.
- The performance test measures one validation of a 100000-key track and then samples it 1000 times. Because every
  `sample_track` call validates the whole track, it checks the documented O(n) cost, not a fast path.
- The cross-host tolerance is a documented design limit checked by the CI platforms, not a proof for every C library.
- The clip JSON schema is a draft and may change when the owner decides.
