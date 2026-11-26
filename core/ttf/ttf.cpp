// Stub. The scope AA contributor replaces this file with the real implementation.
#include "ttf.h"

namespace lumora::ttf {
const char* status_text(Status) { return "stub"; }
Status parse_font(const std::uint8_t*, std::size_t, Font*) { return Status::NullBuffer; }
Status glyph_index(const Font&, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
Status glyph_advance(const Font&, std::uint32_t, std::uint32_t*) { return Status::NullBuffer; }
Status glyph_outline(const Font&, std::uint32_t, std::int32_t, std::int32_t, std::int32_t, outline::Command*, std::uint32_t,
                     std::uint32_t*) {
  return Status::NullBuffer;
}
std::uint64_t digest_commands(const outline::Command*, std::size_t) { return 0; }
}  // namespace lumora::ttf
