// Microphone capture state machine (docs/android-lifecycle.md section 10.7, "Microphone capture states"; supports
// section 9.3 item 5). Pure transition table: no audio calls, no service calls, no clock, no persistence. The
// platform layer feeds events and starts or stops the foreground service and the AudioRecord from the state
// flags. Capability gating (declared, granted) lives in core/package/capabilities.h. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::lifecycle {

enum class MicState : std::uint8_t { Inactive, Active, Paused, Stopped };

enum class MicEvent : std::uint8_t {
  UserActivate,       // the user turns capture on in the visible Lumora app
  Hidden,             // wallpaper hidden, screen off, or the lock screen covers it
  Visible,            // wallpaper visible again
  ServiceKilled,      // the foreground service or the process died
  CaptureRefused,     // the platform refused to start or resume capture
  PermissionRevoked,  // the user revoked the microphone permission
  UserStop            // the user stopped it (for example in the Task Manager)
};

// Full transition table. Any pair not listed leaves the state unchanged.
//   Inactive: UserActivate -> Active; PermissionRevoked -> Stopped.
//   Active:   Hidden -> Paused; ServiceKilled, CaptureRefused, PermissionRevoked, UserStop -> Stopped.
//   Paused:   Visible -> Active; ServiceKilled, CaptureRefused, PermissionRevoked, UserStop -> Stopped.
//   Stopped:  UserActivate -> Active. Nothing else ever leaves Stopped: there is no automatic restart, so Visible
//             alone never restarts capture.
// An event value outside the enum leaves the state unchanged. A state value outside the enum is treated as
// Stopped (fail safe: not capturing) and then the event is applied to Stopped.
MicState next_state(MicState state, MicEvent event);

// State flags, from the plan's table: foreground service running is true for Active and Paused only; capturing
// (AudioRecord recording) is true for Active only. A value outside the enum gives false for both.
bool service_running(MicState state);
bool capturing(MicState state);

// Names "Inactive", "Active", "Paused", "Stopped" and "UserActivate", "Hidden", "Visible", "ServiceKilled",
// "CaptureRefused", "PermissionRevoked", "UserStop"; lowercase "unknown" (never null) for invalid values.
const char* to_string(MicState state);
const char* to_string(MicEvent event);

}  // namespace lumora::lifecycle
