// Power policy (docs/android-lifecycle.md section 10.1 items 2 and 3): turns the battery-saver flag and the render-scale notch
// of pacing.h into a quality preset, a render-scale percentage and scaled pixel sizes. Pure integer functions. No clock reads,
// no floating point, no libm, no 128-bit types (MSVC), no allocation. The frame cap side of battery saver stays in pacing.h
// (frame_interval_ns); this module never touches frame intervals. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::lifecycle {

enum class Quality : std::uint8_t { High, Medium, Low };

// Checks run in this order: the output pointer, then max_notch (or percent for scaled_size); a failure writes nothing.
// Valid max_notch: 1 .. 15 (the range of pacing.h ScaleConfig). Every function below returns false and writes nothing when an
// output pointer is null or max_notch is outside 1 .. 15. A notch above max_notch is clamped to max_notch first (no failure), even UINT32_MAX.
// render_scale_percent has no battery-saver input.

// Render scale in percent: 100 - (50 * notch) / max_notch with integer division, after clamping. So notch 0 is 100 and max_notch
// is 50; for max_notch 3: 100, 84, 67, 50 (floor of 83.33 is not used: 50 * 1 / 3 = 16, 100 - 16 = 84).
bool render_scale_percent(std::uint32_t notch, std::uint32_t max_notch, std::uint32_t* percent_out);

// Quality preset. effective = max_notch when battery_saver is true (the supplied notch is ignored, even an oversized one), else the clamped notch. Low when effective * 2 >= max_notch;
// High when effective == 0; else Medium. Check order: High (effective == 0), then Low, then Medium; with max_notch 1 that gives
// notch 0 High and notch 1 Low. For max_notch 3: effective 0 High, 1 Medium, 2 and 3 Low. For max_notch 15: 0 High, 1 to 7 Medium, 8 to 15 Low. For
// max_notch 1 and 2 every positive effective value is Low.
bool quality_preset(bool battery_saver, std::uint32_t notch, std::uint32_t max_notch, Quality* out);

// Scaled pixel size: max(1, floor(size * percent / 100)) computed in 64-bit, for percent in 1 .. 100 and size any uint32 value.
// Returns false and writes nothing when out is null or percent is 0 or above 100. size 0 gives 1 (never an empty target); UINT32_MAX at 100 percent stays UINT32_MAX.
bool scaled_size(std::uint32_t size, std::uint32_t percent, std::uint32_t* out);

const char* to_string(Quality q);  // "High", "Medium", "Low", "unknown" for an invalid value (static strings, never null)

}  // namespace lumora::lifecycle
