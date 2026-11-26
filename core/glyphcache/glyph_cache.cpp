// Stub: implementation lands by pull request (contributor scope). See glyph_cache.h for the contract.
#include "glyph_cache.h"

namespace lumora::glyphcache {

std::uint8_t subpixel_bin(std::int32_t) { return 0; }
bool cache_clear(GlyphCache*) { return false; }
bool cache_find(GlyphCache*, const GlyphKey&, std::uint32_t*) { return false; }
InsertResult cache_insert(GlyphCache*, const GlyphKey&, std::uint32_t) { return InsertResult::NullCache; }
std::uint32_t cache_size(const GlyphCache*) { return 0; }
std::uint64_t cache_digest(const GlyphCache*) { return 0; }

}  // namespace lumora::glyphcache
