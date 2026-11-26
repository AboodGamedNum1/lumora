// Crash-loop breaker (docs/android-lifecycle.md section 10.7, item 2): a crash log is kept per scene and backend and
// persisted by the caller; after N crashes inside a time window the scene is disabled, and a scene whose Vulkan
// runs crashed repeatedly is moved to GLES before the scene itself is disabled. Pure functions on small values: no
// clock reads, no I/O, no floating point, no persistence. The caller passes the current time in milliseconds
// (any monotonic-enough wall clock). Evidence label: unit tests only. N and the window are pinned at M2; the
// defaults below are the plan's initial proposal.
#pragma once
#include <cstdint>

namespace lumora::lifecycle {

constexpr std::uint32_t kMaxCrashes = 16;  // capacity of one log

enum class Backend : std::uint8_t { Vulkan, Gles };

struct CrashLog {
  std::uint32_t count = 0;                      // valid entries in times_ms, 0 to kMaxCrashes
  std::uint64_t times_ms[kMaxCrashes] = {};     // crash times, oldest first
};

struct Policy {
  std::uint32_t max_crashes = 3;              // crashes in the window (both backends together) that disable the scene
  std::uint64_t window_ms = 86400000;         // 24 hours
  std::uint32_t backend_crashes = 2;          // crashes in the window on Vulkan that move the scene to GLES
};

// Valid: 1 <= max_crashes <= kMaxCrashes, 1 <= backend_crashes <= max_crashes, window_ms >= 1.
bool valid(const Policy& p);

// A crash at time t counts as inside the window at time now when age < window_ms, where age = now - t, and age is 0
// when t > now (a clock that moved backwards must not hide crashes). A log with count above kMaxCrashes is treated
// as having kMaxCrashes entries.
std::uint32_t recent_crashes(const CrashLog& log, std::uint64_t now_ms, std::uint64_t window_ms);

// Records a crash at now_ms. Returns false and changes nothing when log is null or the policy is invalid. Rules:
//  1. Drop entries that are outside the window (see above); the order of the rest is kept.
//  2. Append now_ms at the end. If the log is full, drop the oldest entry first.
//  3. A count above kMaxCrashes on input is first clamped to kMaxCrashes.
bool record_crash(CrashLog* log, std::uint64_t now_ms, const Policy& policy);

enum class Action : std::uint8_t {
  Run,        // start the scene on `backend`
  Disable     // the breaker is open: do not start the scene, fall back to the previous scene or the default wallpaper
};

struct Decision {
  Action action = Action::Run;
  Backend backend = Backend::Vulkan;  // meaningful for Run; Vulkan when Disable
};

// Decides how to start a scene. Returns false (and writes nothing) when out is null or the policy is invalid.
// Rules, in order:
//  1. total = recent(vulkan) + recent(gles) in the window. If total >= max_crashes: Disable.
//  2. Else if gles_available and recent(vulkan) >= backend_crashes: Run on Gles.
//  3. Else Run on Vulkan.
// A null log pointer counts as an empty log.
bool decide(const CrashLog* vulkan, const CrashLog* gles, std::uint64_t now_ms, const Policy& policy,
            bool gles_available, Decision* out);

const char* to_string(Action a);
const char* to_string(Backend b);

}  // namespace lumora::lifecycle
