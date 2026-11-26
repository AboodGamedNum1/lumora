// Stub: implementation lands by pull request (contributor scope AB). See textrun.h for the contract.
#include "textrun.h"

namespace lumora::textrun {

ttf::Status shape_utf8(const ttf::Font&, const char*, std::size_t, std::int32_t, Placed*, std::uint32_t, std::uint32_t*, std::int32_t*) {
  return ttf::Status::NullBuffer;
}
bool count_code_points(const char*, std::size_t, std::uint32_t*) { return false; }
std::uint64_t digest_placed(const Placed*, std::size_t) { return 0; }

}  // namespace lumora::textrun
