// Render gate (docs/android-lifecycle.md section 10.1 item 3: never render while known invisible, that is screen off,
// surface destroyed or visibility reported false). A pure state machine over the three known facts. It decides when
// the frame loop must start and stop; it does not read any platform state. Integer and bool only: no clock reads,
// no floating point, no libm, no allocation. Evidence label: unit tests only.
// Visibility is best-effort on Android (the plan says so): the gate only reflects what was reported to it.
#pragma once
#include <cstdint>

namespace lumora::lifecycle {

enum class GateEvent : std::uint8_t { SurfaceCreated, SurfaceDestroyed, VisibleTrue, VisibleFalse, ScreenOn, ScreenOff };
enum class GateAction : std::uint8_t { None, Start, Stop };

struct RenderGate {
  bool surface = false;     // a surface exists
  bool visible = false;     // the last reported visibility
  bool screen_on = true;    // the last reported screen state; the screen is assumed on until told otherwise
  bool running = false;     // the loop is running (the result of the last apply_event, never set by hand elsewhere)
};

// True when the loop should run: surface && visible && screen_on.
bool should_render(const RenderGate& g);

// Applies one event. Checks run in this order: g null, then the event range; there is no other precondition. A failed
// call returns false and writes nothing, including *action. A null action with a valid gate and event still applies
// the event and returns true. Fails when g is null or the event value is outside the enum.
// Otherwise: the matching fact is set (SurfaceCreated: surface = true; SurfaceDestroyed: surface = false; VisibleTrue:
// visible = true; VisibleFalse: visible = false; ScreenOn: screen_on = true; ScreenOff: screen_on = false), repeats of
// the same value are allowed and change nothing, then running (every valid event recomputes it, even a repeat, which
// heals a hand-built inconsistent running flag; should_render reads only the three facts and ignores running) is recomputed as should_render(*g). The action is the
// edge of running: Start when running went false to true, Stop when it went true to false, None otherwise. *action
// (when not null) is written on every success. The previous running field is trusted as given: a hand-built object
// whose running disagrees with its facts yields the edge from that value (for example running true with all facts
// true and a repeated VisibleTrue gives None; running false with all facts true and a repeated VisibleTrue gives Start).
bool apply_event(RenderGate* g, GateEvent event, GateAction* action);

// The to_string functions return static strings, lowercase "unknown" for an invalid value, never null.
const char* to_string(GateEvent e);   // "SurfaceCreated", "SurfaceDestroyed", "VisibleTrue", "VisibleFalse", "ScreenOn", "ScreenOff", "unknown"
const char* to_string(GateAction a);  // "None", "Start", "Stop", "unknown"

}  // namespace lumora::lifecycle
