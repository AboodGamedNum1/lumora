// Wallpaper offset filter (docs/android-lifecycle.md section 10.1: launcher offset and tilt feed the parallax input). A pure
// integer smoothing step with a dead zone for one two-axis value in Q16 (65536 is 1.0). No clock reads, no floating point, no libm,
// no 128-bit types (MSVC), no allocation. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::input {

constexpr std::int32_t kOne = 65536;           // 1.0 in Q16
constexpr std::int32_t kMaxDeadzone = 32768;   // 0.5

struct OffsetFilterConfig {
  std::uint32_t smooth_percent = 80;  // 0 .. 99: how much of the old state is kept per step (0 = no smoothing)
  std::int32_t deadzone = 1966;       // 0 .. kMaxDeadzone: input magnitudes up to this give 0 (about 0.03)
};

struct OffsetFilter {
  std::int32_t x = 0, y = 0;  // the smoothed value, always within +/- kOne once written by step
  bool primed = false;        // false until the first successful step
};

// Valid config: smooth_percent <= 99 and 0 <= deadzone <= kMaxDeadzone.
bool valid(const OffsetFilterConfig& c);

// Clears the filter (x = 0, y = 0, primed = false). Returns false when f is null.
bool reset(OffsetFilter* f);

// One step with the raw input (in_x, in_y), any int32 values. Returns false and writes nothing (not the filter, not the outputs)
// when f is null, out_x or out_y is null, or the config is invalid; checks in that order. Otherwise, for each axis independently:
//  1. clamp the input to [-kOne, kOne].
//  2. dead zone: m = |v|; if m <= deadzone the value is 0; else the value is sign(v) * ((m - deadzone) * kOne / (kOne - deadzone)),
//     integer division in 64-bit (floor of the non-negative magnitude). So the full-scale input still gives full scale.
//  3. smoothing: if the filter is not primed, the state is the value from step 2 for both axes (the first sample passes through
//     unsmoothed). Else state = state + trunc((value - state) * (100 - smooth_percent) / 100), where trunc is C++ integer division
//     (rounds toward zero), computed in 64-bit. The state therefore stalls within 99 units of the target at smooth_percent 99
//     (accepted: below 0.2 percent of full scale); at smooth_percent 0 it equals the value.
// Then primed = true and *out_x, *out_y receive the new state. A filter whose x or y is outside +/- kOne (hand-built) is repaired
// before step 3 by clamping its state to [-kOne, kOne] (primed is kept as it is).
bool step(OffsetFilter* f, std::int32_t in_x, std::int32_t in_y, const OffsetFilterConfig& config, std::int32_t* out_x,
          std::int32_t* out_y);

}  // namespace lumora::input
