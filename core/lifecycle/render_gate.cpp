// Stub: implementation lands by pull request (contributor scope). See render_gate.h for the contract.
#include "render_gate.h"

namespace lumora::lifecycle {

bool should_render(const RenderGate&) { return false; }
bool apply_event(RenderGate*, GateEvent, GateAction*) { return false; }
const char* to_string(GateEvent) { return ""; }
const char* to_string(GateAction) { return ""; }

}  // namespace lumora::lifecycle
