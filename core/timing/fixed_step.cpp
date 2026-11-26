// Stub: implementation lands by pull request (contributor scope). See fixed_step.h for the contract.
#include "fixed_step.h"

namespace lumora::timing {

bool valid(const StepConfig&) { return false; }
bool advance(StepState*, std::uint64_t, const StepConfig&, StepResult*) { return false; }
void reset_accumulator(StepState*) {}

}  // namespace lumora::timing
