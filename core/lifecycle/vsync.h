// Vsync period estimator (docs/android-lifecycle.md section 10.1: pacing is derived from the measured Choreographer
// vsync period). Feeds frame timestamps in nanoseconds and returns the median of the most recent frame deltas, which
// is the vsync_period_ns input of pacing.h. Integer only: no clock reads, no floating point, no libm, no 128-bit
// types (MSVC), no allocation. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::lifecycle {

constexpr std::uint32_t kVsyncWindow = 16;                    // deltas kept
constexpr std::uint32_t kVsyncMinSamples = 4;                 // deltas needed before an estimate exists
constexpr std::uint64_t kVsyncGapNs = 1000000000;             // a delta above this (1 s) is a pause, not a period

struct VsyncEstimator {
  std::uint64_t last_ns = 0;               // previous timestamp; meaningful only when has_last
  bool has_last = false;
  std::uint64_t deltas[kVsyncWindow] = {}; // ring buffer of deltas in nanoseconds
  std::uint32_t count = 0;                 // valid deltas, 0 to kVsyncWindow
  std::uint32_t next = 0;                  // slot the next delta is written to, 0 to kVsyncWindow - 1
};

// Repair and gap clears touch only the fields they name: invalid-state repair sets has_last false, count 0 and next 0
// (last_ns and the delta slots are left as they are); a gap clear sets count 0, next 0 and last_ns (the delta slots are
// left stale and are never read, because only the count most recent slots are the window). Valid state: count <= kVsyncWindow and next < kVsyncWindow. An invalid object is treated as freshly reset first
// (has_last = false, count = 0, next = 0) by on_vsync; estimate_vsync_ns returns false for it.

// Clears the estimator to its default state: last_ns 0, has_last false, every delta slot 0, count 0, next 0. Returns false when e is null.
bool reset(VsyncEstimator* e);

// Feeds one vsync timestamp. Returns false and changes nothing when e is null. Rules, in order:
//  1. An invalid object is reset first (see above).
//  2. With has_last false, the timestamp is stored in last_ns and has_last becomes true; count, next and the stored
//     deltas are not changed, even in a hand-built valid object with a positive count. No delta is recorded. Returns
//     true. A first timestamp of 0 is accepted; a later timestamp of 0 is rejected by rule 3.
//  3. A timestamp not greater than last_ns (equal or going backwards) is rejected: returns false, nothing changes
//     (last_ns stays, so the next good timestamp measures against it).
//  4. delta = timestamp - last_ns. If delta > kVsyncGapNs (strictly), the window is cleared (count = 0, next = 0),
//     last_ns = timestamp, returns true. A delta of exactly kVsyncGapNs is a normal delta.
//  5. Otherwise the delta is written at next, next advances with wrap-around, count rises up to kVsyncWindow (the
//     oldest delta is overwritten when full), last_ns = timestamp, returns true.
bool on_vsync(VsyncEstimator* e, std::uint64_t timestamp_ns);

// The estimate is the lower median of the stored deltas: sorted ascending, the element at index (count - 1) / 2.
// The window is the count most recent slots ending at next - 1 with wrap-around (order does not affect a median,
// but a hand-built object with count below kVsyncWindow reads exactly those slots). Has_last is not checked, so a hand-built
// object with has_last false can still estimate. Returns false and leaves *out
// untouched when e or out is null, the state is invalid, or count < kVsyncMinSamples. Never returns an estimate of 0:
// deltas are always at least 1 when recorded through on_vsync, but a hand-built zero delta is read as it is, and an
// estimate of 0 is then reported as false (the pacing input must be at least 1).
bool estimate_vsync_ns(const VsyncEstimator* e, std::uint64_t* out);

}  // namespace lumora::lifecycle
