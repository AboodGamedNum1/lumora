// Lumora glyph cache index (scope AC). Integer only, no heap, no recursion, no mutable statics. See docs/glyph_cache.md.
#include "glyph_cache.h"

namespace lumora::glyphcache {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kMaxTick = ~static_cast<u64>(0);

bool key_valid(const GlyphKey& k) { return k.subpixel <= 3 && k.size_px >= 1; }

bool key_equal(const GlyphKey& a, const GlyphKey& b) {
  return a.font_id == b.font_id && a.glyph == b.glyph && a.size_px == b.size_px && a.subpixel == b.subpixel;
}

// Index of the used entry with this key, or kCapacity when there is none.
u32 find_index(const GlyphCache& c, const GlyphKey& key) {
  for (u32 i = 0; i < kCapacity; ++i)
    if (c.entries[i].used && key_equal(c.entries[i].key, key)) return i;
  return kCapacity;
}

void bump(GlyphCache& c) {
  if (c.tick < kMaxTick) ++c.tick;
}

void bump_counter(u64& v) {
  if (v < kMaxTick) ++v;
}

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
  void word64(u64 v) {
    word(static_cast<u32>(v & 0xFFFFFFFFu));
    word(static_cast<u32>(v >> 32));
  }
};

}  // namespace

std::uint8_t subpixel_bin(std::int32_t pen_x_24_8) {
  return static_cast<std::uint8_t>((static_cast<u32>(pen_x_24_8) & 255u) >> 6);
}

bool cache_clear(GlyphCache* cache) {
  if (cache == nullptr) return false;
  for (u32 i = 0; i < kCapacity; ++i) cache->entries[i] = CacheEntry{};
  cache->tick = 0;
  cache->count = 0;
  cache->hits = 0;
  cache->misses = 0;
  cache->evictions = 0;
  return true;
}

bool cache_find(GlyphCache* cache, const GlyphKey& key, std::uint32_t* value) {
  if (cache == nullptr || value == nullptr || !key_valid(key)) return false;
  const u32 i = find_index(*cache, key);
  if (i == kCapacity) {
    bump_counter(cache->misses);
    return false;
  }
  *value = cache->entries[i].value;
  bump(*cache);
  cache->entries[i].last_used = cache->tick;
  bump_counter(cache->hits);
  return true;
}

InsertResult cache_insert(GlyphCache* cache, const GlyphKey& key, std::uint32_t value) {
  if (cache == nullptr) return InsertResult::NullCache;
  if (!key_valid(key)) return InsertResult::BadKey;
  GlyphCache& c = *cache;
  const u32 present = find_index(c, key);
  if (present != kCapacity) {
    c.entries[present].value = value;
    bump(c);
    c.entries[present].last_used = c.tick;
    return InsertResult::Updated;
  }
  u32 slot = kCapacity;  // the slot that receives the entry
  InsertResult result = InsertResult::Evicted;
  if (c.count < kCapacity) {
    for (u32 i = 0; i < kCapacity; ++i)
      if (!c.entries[i].used) {
        slot = i;
        break;
      }
    if (slot != kCapacity) result = InsertResult::Inserted;
  }
  if (result == InsertResult::Evicted) {
    slot = 0;  // no used entry at all: slot 0
    bool found = false;
    for (u32 i = 0; i < kCapacity; ++i) {
      if (!c.entries[i].used) continue;
      if (!found || c.entries[i].last_used < c.entries[slot].last_used) {
        slot = i;
        found = true;
      }
    }
    bump_counter(c.evictions);
  } else if (c.count < kCapacity) {
    ++c.count;
  }
  bump(c);
  CacheEntry e;
  e.key = key;
  e.value = value;
  e.last_used = c.tick;
  e.used = true;
  c.entries[slot] = e;
  return result;
}

std::uint32_t cache_size(const GlyphCache* cache) { return cache == nullptr ? 0 : cache->count; }

std::uint64_t cache_digest(const GlyphCache* cache) {
  Fnv f;
  if (cache == nullptr) return f.h;
  for (u32 i = 0; i < kCapacity; ++i) {
    const CacheEntry& e = cache->entries[i];
    if (!e.used) continue;
    f.word(e.key.font_id);
    f.word(static_cast<u32>(e.key.glyph) | (static_cast<u32>(e.key.size_px) << 16));
    f.word(e.key.subpixel);
    f.word(e.value);
    f.word64(e.last_used);
  }
  f.word64(cache->hits);
  f.word64(cache->misses);
  f.word64(cache->evictions);
  return f.h;
}

}  // namespace lumora::glyphcache
