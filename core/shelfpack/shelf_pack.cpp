// Stub: implementation lands by pull request (contributor scope). See shelf_pack.h for the contract.
#include "shelf_pack.h"

namespace lumora::shelfpack {

bool packer_init(ShelfPacker*, std::uint32_t, std::uint32_t) { return false; }
bool packer_reset(ShelfPacker*) { return false; }
AllocResult packer_alloc(ShelfPacker*, std::uint32_t, std::uint32_t, std::uint32_t*, std::uint32_t*) { return AllocResult::NullArgument; }
std::uint64_t packer_digest(const ShelfPacker*) { return 0; }

}  // namespace lumora::shelfpack
