// Stub: implementation lands by pull request (contributor scope). See damage.h for the contract.
#include "damage.h"

namespace lumora::damage {

void clear(DamageSet*) {}
bool add(DamageSet*, const lumora::rect::Rect&, const lumora::rect::Rect&) { return false; }
std::uint32_t size(const DamageSet*) { return 0; }
bool get(const DamageSet*, std::uint32_t, lumora::rect::Rect*) { return false; }
std::uint64_t total_area(const DamageSet*) { return 0; }
lumora::rect::Rect bounding_box(const DamageSet*) { return lumora::rect::Rect{}; }

}  // namespace lumora::damage
