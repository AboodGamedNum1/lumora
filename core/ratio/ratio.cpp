// Stub: implementation lands by pull request (contributor scope). See ratio.h for the contract.
#include "ratio.h"

namespace lumora::ratio {

RatioResult ratio_make(std::int64_t, std::int64_t, Ratio*) { return RatioResult::NullArgument; }
RatioResult ratio_add(const Ratio*, const Ratio*, Ratio*) { return RatioResult::NullArgument; }
RatioResult ratio_sub(const Ratio*, const Ratio*, Ratio*) { return RatioResult::NullArgument; }
RatioResult ratio_mul(const Ratio*, const Ratio*, Ratio*) { return RatioResult::NullArgument; }
RatioResult ratio_div(const Ratio*, const Ratio*, Ratio*) { return RatioResult::NullArgument; }
RatioResult ratio_compare(const Ratio*, const Ratio*, std::int32_t*) { return RatioResult::NullArgument; }
RatioResult ratio_floor(const Ratio*, std::int64_t*) { return RatioResult::NullArgument; }
RatioResult ratio_ceil(const Ratio*, std::int64_t*) { return RatioResult::NullArgument; }

}  // namespace lumora::ratio
