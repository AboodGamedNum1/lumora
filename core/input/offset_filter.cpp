// Stub: implementation lands by pull request (contributor scope). See offset_filter.h for the contract.
#include "offset_filter.h"

namespace lumora::input {

bool valid(const OffsetFilterConfig&) { return false; }
bool reset(OffsetFilter*) { return false; }
bool step(OffsetFilter*, std::int32_t, std::int32_t, const OffsetFilterConfig&, std::int32_t*, std::int32_t*) { return false; }

}  // namespace lumora::input
