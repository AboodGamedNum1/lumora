// Stub: implementation lands by pull request (contributor scope). See mic_state.h for the contract.
#include "mic_state.h"

namespace lumora::lifecycle {

MicState next_state(MicState state, MicEvent) { return state; }
bool service_running(MicState) { return false; }
bool capturing(MicState) { return false; }
const char* to_string(MicState) { return ""; }
const char* to_string(MicEvent) { return ""; }

}  // namespace lumora::lifecycle
