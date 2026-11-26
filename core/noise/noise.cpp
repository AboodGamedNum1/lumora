// Stub. The scope Q contributor replaces this file with the real implementation.
#include "noise.h"

namespace lumora::noise {
const char* status_text(Status) { return "stub"; }
std::uint32_t hash2(std::uint32_t, std::int32_t, std::int32_t) { return 0; }
std::uint32_t hash3(std::uint32_t, std::int32_t, std::int32_t, std::int32_t) { return 0; }
std::uint32_t fade(std::uint32_t) { return 0; }
Q16 value2(std::uint32_t, Pos, Pos) { return 0; }
Q16 value3(std::uint32_t, Pos, Pos, Pos) { return 0; }
Q16 gradient2(std::uint32_t, Pos, Pos) { return 0; }
Q16 gradient3(std::uint32_t, Pos, Pos, Pos) { return 0; }
Q16 cellular2(std::uint32_t, Pos, Pos) { return 0; }
Q16 fbm2(Kind, std::uint32_t, Pos, Pos, std::uint32_t) { return 0; }
Status fill_image(std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t, Kind, std::uint32_t, Pos, Pos, Pos, std::uint32_t) { return Status::NullBuffer; }
std::uint64_t digest_image(const std::uint8_t*, std::uint32_t, std::uint32_t, std::uint32_t) { return 0; }
}  // namespace lumora::noise
