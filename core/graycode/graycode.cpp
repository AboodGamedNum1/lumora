// Gray code stub (contract in graycode.h). Owned by the graycode contributor; the implementation is not written yet.
#include "graycode.h"
namespace lumora::graycode {
std::uint64_t gray_encode(std::uint64_t) { return 0u; }
std::uint64_t gray_decode(std::uint64_t) { return 0u; }
GrResult kgray_count(std::uint32_t, std::uint32_t, std::uint64_t*) { return GrResult::BadArgument; }
GrResult kgray_unrank(std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t*) { return GrResult::BadArgument; }
GrResult kgray_rank(std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t*) { return GrResult::BadArgument; }
GrResult kgray_next(std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t*) { return GrResult::BadArgument; }
}  // namespace lumora::graycode
