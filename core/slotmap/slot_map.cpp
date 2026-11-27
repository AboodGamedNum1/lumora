// Stub: implementation lands by pull request (contributor scope). See slot_map.h for the contract.
#include "slot_map.h"

namespace lumora::slotmap {

void init(SlotMap*) {}
bool allocate(SlotMap*, std::uint32_t*) { return false; }
bool release(SlotMap*, std::uint32_t) { return false; }
bool is_live(const SlotMap*, std::uint32_t) { return false; }
std::uint32_t size(const SlotMap*) { return 0; }

}  // namespace lumora::slotmap
