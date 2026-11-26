// Stub: implementation lands by pull request (contributor scope). See rect.h for the contract.
#include "rect.h"

namespace lumora::rect {

bool is_valid(const Rect&) { return false; }
bool is_empty(const Rect&) { return true; }
Rect intersect(const Rect&, const Rect&) { return Rect{}; }
bool overlaps(const Rect&, const Rect&) { return false; }
bool bounds_union(const Rect&, const Rect&, Rect*) { return false; }
bool contains_point(const Rect&, std::int32_t, std::int32_t) { return false; }
std::uint64_t area(const Rect&) { return 0; }

}  // namespace lumora::rect
