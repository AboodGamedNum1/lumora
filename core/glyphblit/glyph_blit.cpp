// Stub: implementation lands by pull request (contributor scope). See glyph_blit.h for the contract.
#include "glyph_blit.h"

namespace lumora::glyphblit {

BlitResult blit_coverage(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint8_t*, std::uint32_t, std::uint32_t,
                         std::uint32_t, std::uint32_t, std::uint32_t) {
  return BlitResult::NullArgument;
}
std::uint64_t image_digest(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t) { return 0; }

}  // namespace lumora::glyphblit
