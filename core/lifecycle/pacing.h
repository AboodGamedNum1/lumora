// Frame pacing and render-scale control (docs/android-lifecycle.md sections 10.1, 10.4 and the thermal guard of
// 10.1 item 3). Two pure parts: (1) the frame interval the loop paces to, from the user's frame cap, the measured
// vsync period and battery saver; (2) a render-scale notch controller that steps down after a sustained frame-time
// overrun (the fallback for devices that report no thermal status) or when the platform reports throttling.
// Integer only: no clock reads, no floating point, no libm, no 128-bit types (MSVC). The recovery rule (stepping back
// up) is provisional: the plan only specifies stepping down. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::lifecycle {

// ---- Frame interval ----------------------------------------------------------------------------------------
enum class FrameCap : std::uint8_t { Auto, Fps30, Fps60, Fps90, Fps120 };

struct PacingInput {
  FrameCap cap = FrameCap::Auto;
  std::uint64_t vsync_period_ns = 0;  // measured display period; must be at least 1
  bool battery_saver = false;
};

// Period of one capped rate, rounded to the nearest nanosecond (half up): Fps30 33333333, Fps60 16666667,
// Fps90 11111111, Fps120 8333333. Auto and invalid values return 0.
std::uint64_t cap_period_ns(FrameCap cap);

// Computes the interval the loop paces to. Returns false and leaves *out untouched when out is null, the cap value
// is outside the enum, or vsync_period_ns is 0. Rules, in order:
//  1. desired = vsync_period_ns for Auto, else cap_period_ns(cap).
//  2. With battery_saver, desired = max(desired, 33333333) (30 fps).
//  3. desired = max(desired, vsync_period_ns): never faster than the display.
//  4. The result is a whole number of vsync periods: q = desired / vsync, r = desired % vsync, tolerance =
//     vsync / 20 (integer division), k = q + (r > tolerance ? 1 : 0), result = k * vsync. Since desired >= vsync, q >= 1
//     and k >= 1. No overflow: when q >= 2 the desired value is a constant below 2^26, so k * vsync stays small.
// Example: vsync 11111111 (90 Hz panel) with cap Fps60 gives 22222222 (45 fps), because 60 fps does not divide 90 Hz.
bool frame_interval_ns(const PacingInput& in, std::uint64_t* out);

// ---- Render scale notches ----------------------------------------------------------------------------------
struct ScaleConfig {
  std::uint32_t max_notch = 3;          // notch 0 is full scale; max_notch is the lowest scale
  std::uint32_t overrun_frames = 30;    // consecutive overrun frames that step down one notch
  std::uint32_t recover_frames = 300;   // consecutive fast frames that step back up one notch (provisional)
  std::uint32_t overrun_percent = 110;  // a frame is an overrun when frame_ns > budget_ns * overrun_percent / 100
  std::uint32_t fast_percent = 90;      // a frame is fast when frame_ns <= budget_ns * fast_percent / 100
};

struct ScaleState {
  std::uint32_t notch = 0;
  std::uint32_t overrun_run = 0;  // consecutive overrun frames so far
  std::uint32_t fast_run = 0;     // consecutive fast frames so far
};

// Valid: 1 <= max_notch <= 15, overrun_frames >= 1, recover_frames >= 1, 100 <= overrun_percent <= 1000,
// 10 <= fast_percent <= 100.
bool valid(const ScaleConfig& c);

// Feeds one frame time. Returns false and changes nothing when state is null, the config is invalid, or budget_ns is
// 0. Otherwise returns true, with *changed (if not null) telling whether the notch moved. Rules, in order. The two
// thresholds are floor(budget_ns * percent / 100), computed without overflow (saturating at UINT64_MAX):
//  1. frame_ns > overrun threshold: overrun_run += 1 (saturating), fast_run = 0. If overrun_run >= overrun_frames and
//     notch < max_notch: notch += 1, overrun_run = 0, changed. At max_notch the run just keeps counting.
//  2. Else frame_ns <= fast threshold: fast_run += 1 (saturating), overrun_run = 0. If fast_run >= recover_frames and
//     notch > 0: notch -= 1, fast_run = 0, changed.
//  3. Else both runs reset to 0.
// A state whose notch is above max_notch is first clamped down to max_notch, without resetting the runs. *changed is true
// when the final notch differs from the caller's original notch, so a clamp alone (for example 9 with max_notch 3)
// reports true. Failure leaves state and *changed untouched; success always writes *changed when it is not null.
// Thresholds are exact (saturated only if the exact value exceeds UINT64_MAX; a saturated overrun threshold can never
// be exceeded); the overrun and fast classes cannot coincide. Runs saturate at UINT32_MAX and a frame-count limit of
// UINT32_MAX fires on reaching it. At max_notch the overrun run keeps counting; at notch 0 the fast run keeps counting.
bool on_frame(ScaleState* state, std::uint64_t frame_ns, std::uint64_t budget_ns, const ScaleConfig& config,
              bool* changed);

// The platform reported throttling: step down one notch now (capped at max_notch) and reset both runs. Returns false
// and changes nothing when state is null or the config is invalid; *changed (if not null) tells whether the notch
// moved. The step is notch + 1 capped at max_notch after the clamp above; both runs reset on every call, even at max_notch;
// *changed compares the final notch with the original one, so an oversized notch clamped to max_notch reports true.
bool on_thermal_throttle(ScaleState* state, const ScaleConfig& config, bool* changed);

const char* to_string(FrameCap cap);  // "Auto", "Fps30", "Fps60", "Fps90", "Fps120", "unknown" for invalid values

}  // namespace lumora::lifecycle
