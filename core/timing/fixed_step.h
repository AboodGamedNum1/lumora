// Fixed-timestep accumulator (docs/android-lifecycle.md, "Timing model"): physics and scene simulation run in
// fixed steps, the frame delta is clamped after a pause, the number of catch-up steps per frame is capped (no
// spiral of death), and rendering gets an interpolation factor between the last two simulation states. Pure
// integer arithmetic on nanoseconds: no clock reads, no floating point, no libm, no 128-bit types (MSVC). The
// caller reads the clock and passes deltas. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::timing {

struct StepConfig {
  std::uint64_t step_ns = 16666667;          // one simulation step; must be at least 1
  std::uint32_t max_steps = 5;               // most steps returned by one advance(); must be at least 1
  std::uint64_t max_delta_ns = 250000000;    // frame deltas above this are clamped (pause, debugger, resume)
};

struct StepState {
  std::uint64_t accumulator_ns = 0;  // time not yet simulated; below step_ns after every advance()
  std::uint64_t dropped_ns = 0;      // total time discarded by delta clamping and by the catch-up cap (saturating)
};

struct StepResult {
  std::uint32_t steps = 0;       // simulation steps to run now, 0 to config.max_steps
  std::uint32_t alpha_q16 = 0;   // interpolation factor, floor(accumulator_ns * 65536 / step_ns), 0 to 65535
};

// Valid config: step_ns >= 1 and max_steps >= 1. max_delta_ns may be 0 (every delta is then clamped to 0).
bool valid(const StepConfig& c);

// Advances the accumulator. Returns false and changes nothing when state or out is null or the config is not
// valid. Exact rules, in order:
//  1. clamped = min(delta_ns, max_delta_ns); the excess delta_ns - clamped is added to dropped_ns (saturating).
//  2. The accumulator is added to clamped with saturation at UINT64_MAX (a forged huge state cannot wrap).
//  3. wanted = accumulator / step_ns (as a 64-bit quotient); steps = min(wanted, max_steps).
//  4. accumulator -= steps * step_ns (no overflow: steps * step_ns <= accumulator).
//  5. If wanted > max_steps, the catch-up cap was hit: the whole steps still left in the accumulator are discarded,
//     that is accumulator is replaced by accumulator % step_ns and the removed amount is added to dropped_ns
//     (saturating). The remainder below one step is always kept.
//  6. alpha_q16 = floor(accumulator * 65536 / step_ns), exact for every step_ns up to UINT64_MAX; compute it
//     without 128-bit types (for example 16 shift-and-subtract rounds on the remainder). Always below 65536.
// Notes: time lost to the saturation in rule 2 is not counted in dropped_ns (only the clamp excess of rule 1 and the
// catch-up discard of rule 5 are). Rule 5 compares wanted and max_steps as 64-bit values; equality drops nothing.
// When both caps trigger, the rule 1 excess is added to dropped_ns first and the rule 5 discard second, each
// saturating. A null state, null out or invalid config returns false and leaves state and out untouched.
// The alpha division compares the remainder against step_ns minus the remainder to avoid overflow.
// state->accumulator_ns is below step_ns afterwards, even if the caller passed a larger value in.
bool advance(StepState* state, std::uint64_t delta_ns, const StepConfig& config, StepResult* out);

// Resets the accumulator to 0 (scene start, wallpaper resumed after a long pause); dropped_ns is kept. A null
// state is ignored.
void reset_accumulator(StepState* state);

}  // namespace lumora::timing
