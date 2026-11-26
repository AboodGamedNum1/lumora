// Glyph cache index: maps a glyph raster key to a caller-owned 32-bit value (for example an atlas slot) with least-recently-used
// eviction. Fixed capacity, linear search, no allocation, no hashing, no clock (recency is an internal counter), integer only, no
// 128-bit types (MSVC). It stores no pixels. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::glyphcache {

constexpr std::uint32_t kCapacity = 128;

struct GlyphKey {
  std::uint32_t font_id = 0;    // caller defined
  std::uint16_t glyph = 0;      // glyph id as produced by core/textrun
  std::uint16_t size_px = 0;    // raster size in whole pixels
  std::uint8_t subpixel = 0;    // 0..3, see subpixel_bin
};

struct CacheEntry {
  GlyphKey key;
  std::uint32_t value = 0;
  std::uint64_t last_used = 0;  // value of the cache tick at the last find hit or insert of this entry
  bool used = false;
};

struct GlyphCache {
  CacheEntry entries[kCapacity] = {};
  std::uint64_t tick = 0;       // recency counter, saturates at UINT64_MAX
  std::uint32_t count = 0;      // used entries, 0 to kCapacity
  std::uint64_t hits = 0, misses = 0, evictions = 0;  // saturate at UINT64_MAX
};

enum class InsertResult : std::uint8_t { Inserted = 0, Updated = 1, Evicted = 2, NullCache = 3, BadKey = 4 };

// Quarter-pixel bin of a pen position in 24.8 fixed point (the unit core/textrun produces): (pen_x & 255) >> 6 on the two's complement
// value, so 0..3 and negative positions floor toward minus infinity (pen -1 is bin 3).
std::uint8_t subpixel_bin(std::int32_t pen_x_24_8);

// Keys are equal when all four fields are equal. A key is valid when subpixel <= 3 and size_px >= 1.

// Empties the cache: all entries default, tick, count and the three counters 0. Returns false when cache is null.
bool cache_clear(GlyphCache* cache);

// Looks key up. Returns false when cache or value is null, or the key is invalid (nothing changes then, counters included). On a hit:
// *value is set, tick += 1 (saturating), the entry's last_used = the new tick, hits += 1, returns true. On a miss: misses += 1, *value
// is untouched, returns false.
bool cache_find(GlyphCache* cache, const GlyphKey& key, std::uint32_t* value);

// Inserts or replaces. NullCache when cache is null, BadKey for an invalid key; both change nothing. If the key is present its value is
// replaced, tick += 1, last_used = tick, result Updated. Else if count < kCapacity the entry goes to the lowest unused slot, count += 1,
// result Inserted. Else the entry with the smallest last_used (lowest index on a tie) is replaced, evictions += 1, result Evicted.
// For Inserted and Evicted too: tick += 1 (saturating) and the new entry gets last_used = tick.
InsertResult cache_insert(GlyphCache* cache, const GlyphKey& key, std::uint32_t value);

// Number of used entries; 0 for a null cache.
std::uint32_t cache_size(const GlyphCache* cache);

// FNV-1a 64 byte-wise over 32-bit little-endian words (offset basis start; a null cache gives the basis), for each used entry in slot
// order: font_id, glyph then size_px packed as (glyph | size_px << 16), subpixel zero extended, value, last_used low 32 bits, last_used
// high 32 bits; then hits, misses, evictions each as low 32 bits then high 32 bits. Padding is never hashed.
std::uint64_t cache_digest(const GlyphCache* cache);

}  // namespace lumora::glyphcache
