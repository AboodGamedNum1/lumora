// Placeholder for the particle module. The contributor replaces this file; the interface is core/particles/particles.h.
#include "particles.h"

namespace lumora::particles {

ConfigStatus validate_config(const EmitterConfig&) { return ConfigStatus::BadRate; }
const char* config_status_text(ConfigStatus) { return "not implemented"; }
bool emitter_init(const EmitterConfig&, Emitter&) { return false; }
bool emitter_step(Emitter&, float) { return false; }
std::uint64_t emitter_digest(const Emitter&) { return 0; }

}  // namespace lumora::particles
