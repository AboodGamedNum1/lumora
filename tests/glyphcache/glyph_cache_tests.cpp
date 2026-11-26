// Tests for core/glyphcache (scope AC). Expected values come from an independent Python reference (tools/glyph_cache_ref.py: its
// own slot model of the cache, tick and counters, and FNV) and are pasted below as constants: scenario programs with their result
// digests, state digests and counters, plus seeded random corpora. Hand-computed LRU cases are derived from glyph_cache.h.
// Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>

#include "glyph_cache.h"

using namespace lumora::glyphcache;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kMaxU64 = ~static_cast<u64>(0);

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int k = 0; k < 4; ++k) {
      h ^= (v >> (8 * k)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

struct Lcg {
  u64 s;
  explicit Lcg(u64 seed) : s(seed) {}
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  u32 rng(u32 lo, u32 hi) { return lo + next() % (hi - lo + 1); }
};

GlyphKey key(u32 f, u32 g, u32 sz, u32 sb) {
  GlyphKey k;
  k.font_id = f;
  k.glyph = static_cast<std::uint16_t>(g);
  k.size_px = static_cast<std::uint16_t>(sz);
  k.subpixel = static_cast<std::uint8_t>(sb);
  return k;
}

struct Op {
  char kind;
  u64 a[8];
};
struct Expect {
  u64 results_digest, state_digest;
  u32 count;
  u64 tick, hits, misses, evictions;
  u32 result_words;
};
struct ProgRef {
  const Op* ops;
  std::size_t n;
  const Expect& exp;
};

struct Replay {
  GlyphCache c;
  Fnv results;
  u32 words = 0;
  void res(u32 v) {
    results.word(v);
    ++words;
  }
  void ins(const GlyphKey& k, u32 v) { res(static_cast<u32>(cache_insert(&c, k, v))); }
  void find(const GlyphKey& k) {
    u32 v = 0xA5A5A5A5u;
    const bool ok = cache_find(&c, k, &v);
    if (!ok) REQUIRE(v == 0xA5A5A5A5u);  // *value untouched on a miss or an error
    res(ok ? 1u : 0u);
    res(ok ? v : 0u);
  }
  void apply(const Op& o) {
    switch (o.kind) {
      case 'I': ins(key(static_cast<u32>(o.a[0]), static_cast<u32>(o.a[1]), static_cast<u32>(o.a[2]), static_cast<u32>(o.a[3])), static_cast<u32>(o.a[4])); break;
      case 'F': find(key(static_cast<u32>(o.a[0]), static_cast<u32>(o.a[1]), static_cast<u32>(o.a[2]), static_cast<u32>(o.a[3]))); break;
      case 'C': REQUIRE(cache_clear(&c)); break;
      case 'T': c.tick = o.a[0]; break;
      case 'N': c.count = static_cast<u32>(o.a[0]); break;
      case 'E': {
        CacheEntry& e = c.entries[o.a[0]];
        e.key = key(static_cast<u32>(o.a[1]), static_cast<u32>(o.a[2]), static_cast<u32>(o.a[3]), static_cast<u32>(o.a[4]));
        e.value = static_cast<u32>(o.a[5]);
        e.last_used = o.a[6];
        e.used = o.a[7] != 0;
        break;
      }
      case 'H':
        c.hits = o.a[0];
        c.misses = o.a[1];
        c.evictions = o.a[2];
        break;
      default: FAIL("bad op");
    }
  }
  void check(const Expect& e) const {
    INFO("count " << c.count << " tick " << c.tick << " hits " << c.hits << " misses " << c.misses << " ev " << c.evictions);
    REQUIRE(words == e.result_words);
    REQUIRE(results.h == e.results_digest);
    REQUIRE(cache_digest(&c) == e.state_digest);
    REQUIRE(c.count == e.count);
    REQUIRE(c.tick == e.tick);
    REQUIRE(c.hits == e.hits);
    REQUIRE(c.misses == e.misses);
    REQUIRE(c.evictions == e.evictions);
  }
};

const Op kProg_lru[] = {
  {'I', {1ull, 0ull, 10ull, 0ull, 1000ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 10ull, 1ull, 1001ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 10ull, 2ull, 1002ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 3ull, 10ull, 3ull, 1003ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 4ull, 10ull, 0ull, 1004ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 5ull, 10ull, 1ull, 1005ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 6ull, 10ull, 2ull, 1006ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 7ull, 10ull, 3ull, 1007ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 8ull, 10ull, 0ull, 1008ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 9ull, 10ull, 1ull, 1009ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 10ull, 10ull, 2ull, 1010ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 11ull, 10ull, 3ull, 1011ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 12ull, 10ull, 0ull, 1012ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 13ull, 10ull, 1ull, 1013ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 14ull, 10ull, 2ull, 1014ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 15ull, 10ull, 3ull, 1015ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 16ull, 10ull, 0ull, 1016ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 17ull, 10ull, 1ull, 1017ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 18ull, 10ull, 2ull, 1018ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 19ull, 10ull, 3ull, 1019ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 20ull, 10ull, 0ull, 1020ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 21ull, 10ull, 1ull, 1021ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 22ull, 10ull, 2ull, 1022ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 23ull, 10ull, 3ull, 1023ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 24ull, 10ull, 0ull, 1024ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 25ull, 10ull, 1ull, 1025ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 26ull, 10ull, 2ull, 1026ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 27ull, 10ull, 3ull, 1027ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 28ull, 10ull, 0ull, 1028ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 29ull, 10ull, 1ull, 1029ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 30ull, 10ull, 2ull, 1030ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 31ull, 10ull, 3ull, 1031ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 32ull, 10ull, 0ull, 1032ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 33ull, 10ull, 1ull, 1033ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 34ull, 10ull, 2ull, 1034ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 35ull, 10ull, 3ull, 1035ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 36ull, 10ull, 0ull, 1036ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 37ull, 10ull, 1ull, 1037ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 38ull, 10ull, 2ull, 1038ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 39ull, 10ull, 3ull, 1039ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 40ull, 10ull, 0ull, 1040ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 41ull, 10ull, 1ull, 1041ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 42ull, 10ull, 2ull, 1042ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 43ull, 10ull, 3ull, 1043ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 44ull, 10ull, 0ull, 1044ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 45ull, 10ull, 1ull, 1045ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 46ull, 10ull, 2ull, 1046ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 47ull, 10ull, 3ull, 1047ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 48ull, 10ull, 0ull, 1048ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 49ull, 10ull, 1ull, 1049ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 50ull, 10ull, 2ull, 1050ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 51ull, 10ull, 3ull, 1051ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 52ull, 10ull, 0ull, 1052ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 53ull, 10ull, 1ull, 1053ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 54ull, 10ull, 2ull, 1054ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 55ull, 10ull, 3ull, 1055ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 56ull, 10ull, 0ull, 1056ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 57ull, 10ull, 1ull, 1057ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 58ull, 10ull, 2ull, 1058ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 59ull, 10ull, 3ull, 1059ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 60ull, 10ull, 0ull, 1060ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 61ull, 10ull, 1ull, 1061ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 62ull, 10ull, 2ull, 1062ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 63ull, 10ull, 3ull, 1063ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 64ull, 10ull, 0ull, 1064ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 65ull, 10ull, 1ull, 1065ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 66ull, 10ull, 2ull, 1066ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 67ull, 10ull, 3ull, 1067ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 68ull, 10ull, 0ull, 1068ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 69ull, 10ull, 1ull, 1069ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 70ull, 10ull, 2ull, 1070ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 71ull, 10ull, 3ull, 1071ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 72ull, 10ull, 0ull, 1072ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 73ull, 10ull, 1ull, 1073ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 74ull, 10ull, 2ull, 1074ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 75ull, 10ull, 3ull, 1075ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 76ull, 10ull, 0ull, 1076ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 77ull, 10ull, 1ull, 1077ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 78ull, 10ull, 2ull, 1078ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 79ull, 10ull, 3ull, 1079ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 80ull, 10ull, 0ull, 1080ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 81ull, 10ull, 1ull, 1081ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 82ull, 10ull, 2ull, 1082ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 83ull, 10ull, 3ull, 1083ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 84ull, 10ull, 0ull, 1084ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 85ull, 10ull, 1ull, 1085ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 86ull, 10ull, 2ull, 1086ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 87ull, 10ull, 3ull, 1087ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 88ull, 10ull, 0ull, 1088ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 89ull, 10ull, 1ull, 1089ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 90ull, 10ull, 2ull, 1090ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 91ull, 10ull, 3ull, 1091ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 92ull, 10ull, 0ull, 1092ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 93ull, 10ull, 1ull, 1093ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 94ull, 10ull, 2ull, 1094ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 95ull, 10ull, 3ull, 1095ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 96ull, 10ull, 0ull, 1096ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 97ull, 10ull, 1ull, 1097ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 98ull, 10ull, 2ull, 1098ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 99ull, 10ull, 3ull, 1099ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 100ull, 10ull, 0ull, 1100ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 101ull, 10ull, 1ull, 1101ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 102ull, 10ull, 2ull, 1102ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 103ull, 10ull, 3ull, 1103ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 104ull, 10ull, 0ull, 1104ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 105ull, 10ull, 1ull, 1105ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 106ull, 10ull, 2ull, 1106ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 107ull, 10ull, 3ull, 1107ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 108ull, 10ull, 0ull, 1108ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 109ull, 10ull, 1ull, 1109ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 110ull, 10ull, 2ull, 1110ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 111ull, 10ull, 3ull, 1111ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 112ull, 10ull, 0ull, 1112ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 113ull, 10ull, 1ull, 1113ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 114ull, 10ull, 2ull, 1114ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 115ull, 10ull, 3ull, 1115ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 116ull, 10ull, 0ull, 1116ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 117ull, 10ull, 1ull, 1117ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 118ull, 10ull, 2ull, 1118ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 119ull, 10ull, 3ull, 1119ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 120ull, 10ull, 0ull, 1120ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 121ull, 10ull, 1ull, 1121ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 122ull, 10ull, 2ull, 1122ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 123ull, 10ull, 3ull, 1123ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 124ull, 10ull, 0ull, 1124ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 125ull, 10ull, 1ull, 1125ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 126ull, 10ull, 2ull, 1126ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 127ull, 10ull, 3ull, 1127ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 0ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 5ull, 10ull, 1ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {9ull, 9ull, 9ull, 1ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 7ull, 12ull, 3ull, 7ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 8ull, 12ull, 3ull, 8ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 9ull, 12ull, 3ull, 9ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 0ull, 10ull, 0ull, 5555ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 10ull, 1ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 0ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_lru = {16309652155613478290ull, 9699196860013711020ull, 128, 135ull, 3ull, 2ull, 3ull, 142};
const Op kProg_invalid[] = {
  {'I', {1ull, 1ull, 10ull, 0ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 0ull, 0ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 10ull, 4ull, 3ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 10ull, 4ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {5ull, 5ull, 5ull, 3ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {0ull, 0ull, 1ull, 3ull, 9ull, 0ull, 0ull, 0ull}},
  {'I', {4294967295ull, 65535ull, 65535ull, 3ull, 4294967295ull, 0ull, 0ull, 0ull}},
  {'F', {4294967295ull, 65535ull, 65535ull, 3ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_invalid = {563982216112474048ull, 13816095258109133509ull, 3, 5ull, 2ull, 1ull, 0ull, 15};
const Op kProg_fields[] = {
  {'I', {1ull, 2ull, 3ull, 0ull, 10ull, 0ull, 0ull, 0ull}},
  {'F', {2ull, 2ull, 3ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 3ull, 3ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 2ull, 4ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 2ull, 3ull, 1ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 2ull, 3ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_fields = {10098437865017967262ull, 126317271889022704ull, 1, 2ull, 1ull, 4ull, 0ull, 11};
const Op kProg_clear[] = {
  {'I', {1ull, 0ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 10ull, 0ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 10ull, 0ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 3ull, 10ull, 0ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 4ull, 10ull, 0ull, 4ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 5ull, 10ull, 0ull, 5ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 6ull, 10ull, 0ull, 6ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 7ull, 10ull, 0ull, 7ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 8ull, 10ull, 0ull, 8ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 9ull, 10ull, 0ull, 9ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 10ull, 10ull, 0ull, 10ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 11ull, 10ull, 0ull, 11ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 12ull, 10ull, 0ull, 12ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 13ull, 10ull, 0ull, 13ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 14ull, 10ull, 0ull, 14ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 15ull, 10ull, 0ull, 15ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 16ull, 10ull, 0ull, 16ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 17ull, 10ull, 0ull, 17ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 18ull, 10ull, 0ull, 18ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 19ull, 10ull, 0ull, 19ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 20ull, 10ull, 0ull, 20ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 21ull, 10ull, 0ull, 21ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 22ull, 10ull, 0ull, 22ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 23ull, 10ull, 0ull, 23ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 24ull, 10ull, 0ull, 24ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 25ull, 10ull, 0ull, 25ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 26ull, 10ull, 0ull, 26ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 27ull, 10ull, 0ull, 27ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 28ull, 10ull, 0ull, 28ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 29ull, 10ull, 0ull, 29ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 30ull, 10ull, 0ull, 30ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 31ull, 10ull, 0ull, 31ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 32ull, 10ull, 0ull, 32ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 33ull, 10ull, 0ull, 33ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 34ull, 10ull, 0ull, 34ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 35ull, 10ull, 0ull, 35ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 36ull, 10ull, 0ull, 36ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 37ull, 10ull, 0ull, 37ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 38ull, 10ull, 0ull, 38ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 39ull, 10ull, 0ull, 39ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 40ull, 10ull, 0ull, 40ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 41ull, 10ull, 0ull, 41ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 42ull, 10ull, 0ull, 42ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 43ull, 10ull, 0ull, 43ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 44ull, 10ull, 0ull, 44ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 45ull, 10ull, 0ull, 45ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 46ull, 10ull, 0ull, 46ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 47ull, 10ull, 0ull, 47ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 48ull, 10ull, 0ull, 48ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 49ull, 10ull, 0ull, 49ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 50ull, 10ull, 0ull, 50ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 51ull, 10ull, 0ull, 51ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 52ull, 10ull, 0ull, 52ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 53ull, 10ull, 0ull, 53ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 54ull, 10ull, 0ull, 54ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 55ull, 10ull, 0ull, 55ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 56ull, 10ull, 0ull, 56ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 57ull, 10ull, 0ull, 57ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 58ull, 10ull, 0ull, 58ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 59ull, 10ull, 0ull, 59ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 60ull, 10ull, 0ull, 60ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 61ull, 10ull, 0ull, 61ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 62ull, 10ull, 0ull, 62ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 63ull, 10ull, 0ull, 63ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 64ull, 10ull, 0ull, 64ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 65ull, 10ull, 0ull, 65ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 66ull, 10ull, 0ull, 66ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 67ull, 10ull, 0ull, 67ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 68ull, 10ull, 0ull, 68ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 69ull, 10ull, 0ull, 69ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 70ull, 10ull, 0ull, 70ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 71ull, 10ull, 0ull, 71ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 72ull, 10ull, 0ull, 72ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 73ull, 10ull, 0ull, 73ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 74ull, 10ull, 0ull, 74ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 75ull, 10ull, 0ull, 75ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 76ull, 10ull, 0ull, 76ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 77ull, 10ull, 0ull, 77ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 78ull, 10ull, 0ull, 78ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 79ull, 10ull, 0ull, 79ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 80ull, 10ull, 0ull, 80ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 81ull, 10ull, 0ull, 81ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 82ull, 10ull, 0ull, 82ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 83ull, 10ull, 0ull, 83ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 84ull, 10ull, 0ull, 84ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 85ull, 10ull, 0ull, 85ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 86ull, 10ull, 0ull, 86ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 87ull, 10ull, 0ull, 87ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 88ull, 10ull, 0ull, 88ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 89ull, 10ull, 0ull, 89ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 90ull, 10ull, 0ull, 90ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 91ull, 10ull, 0ull, 91ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 92ull, 10ull, 0ull, 92ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 93ull, 10ull, 0ull, 93ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 94ull, 10ull, 0ull, 94ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 95ull, 10ull, 0ull, 95ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 96ull, 10ull, 0ull, 96ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 97ull, 10ull, 0ull, 97ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 98ull, 10ull, 0ull, 98ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 99ull, 10ull, 0ull, 99ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 100ull, 10ull, 0ull, 100ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 101ull, 10ull, 0ull, 101ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 102ull, 10ull, 0ull, 102ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 103ull, 10ull, 0ull, 103ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 104ull, 10ull, 0ull, 104ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 105ull, 10ull, 0ull, 105ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 106ull, 10ull, 0ull, 106ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 107ull, 10ull, 0ull, 107ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 108ull, 10ull, 0ull, 108ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 109ull, 10ull, 0ull, 109ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 110ull, 10ull, 0ull, 110ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 111ull, 10ull, 0ull, 111ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 112ull, 10ull, 0ull, 112ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 113ull, 10ull, 0ull, 113ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 114ull, 10ull, 0ull, 114ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 115ull, 10ull, 0ull, 115ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 116ull, 10ull, 0ull, 116ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 117ull, 10ull, 0ull, 117ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 118ull, 10ull, 0ull, 118ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 119ull, 10ull, 0ull, 119ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 120ull, 10ull, 0ull, 120ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 121ull, 10ull, 0ull, 121ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 122ull, 10ull, 0ull, 122ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 123ull, 10ull, 0ull, 123ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 124ull, 10ull, 0ull, 124ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 125ull, 10ull, 0ull, 125ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 126ull, 10ull, 0ull, 126ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 127ull, 10ull, 0ull, 127ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 128ull, 10ull, 0ull, 128ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 129ull, 10ull, 0ull, 129ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 3ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'C', {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 129ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 1ull, 1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_clear = {8745572205521346279ull, 2279642031152834140ull, 1, 1ull, 0ull, 1ull, 0ull, 135};
const Op kProg_tick_sat[] = {
  {'T', {18446744073709551612ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 0ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 10ull, 0ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 10ull, 0ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 3ull, 10ull, 0ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 4ull, 10ull, 0ull, 4ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 5ull, 10ull, 0ull, 5ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 0ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 5ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 100ull, 10ull, 0ull, 100ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 101ull, 10ull, 0ull, 101ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 102ull, 10ull, 0ull, 102ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 103ull, 10ull, 0ull, 103ull, 0ull, 0ull, 0ull}},
  {'N', {128ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 0ull, 10ull, 0ull, 77ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 1ull, 10ull, 0ull, 77ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 2ull, 10ull, 0ull, 77ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_tick_sat = {13310226612192628882ull, 3412460416903316902ull, 128, 18446744073709551615ull, 2ull, 0ull, 3ull, 17};
const Op kProg_tick_full[] = {
  {'T', {18446744073709551614ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 0ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 10ull, 0ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 10ull, 0ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 3ull, 10ull, 0ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 4ull, 10ull, 0ull, 4ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 5ull, 10ull, 0ull, 5ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 6ull, 10ull, 0ull, 6ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 7ull, 10ull, 0ull, 7ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 8ull, 10ull, 0ull, 8ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 9ull, 10ull, 0ull, 9ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 10ull, 10ull, 0ull, 10ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 11ull, 10ull, 0ull, 11ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 12ull, 10ull, 0ull, 12ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 13ull, 10ull, 0ull, 13ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 14ull, 10ull, 0ull, 14ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 15ull, 10ull, 0ull, 15ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 16ull, 10ull, 0ull, 16ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 17ull, 10ull, 0ull, 17ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 18ull, 10ull, 0ull, 18ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 19ull, 10ull, 0ull, 19ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 20ull, 10ull, 0ull, 20ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 21ull, 10ull, 0ull, 21ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 22ull, 10ull, 0ull, 22ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 23ull, 10ull, 0ull, 23ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 24ull, 10ull, 0ull, 24ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 25ull, 10ull, 0ull, 25ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 26ull, 10ull, 0ull, 26ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 27ull, 10ull, 0ull, 27ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 28ull, 10ull, 0ull, 28ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 29ull, 10ull, 0ull, 29ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 30ull, 10ull, 0ull, 30ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 31ull, 10ull, 0ull, 31ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 32ull, 10ull, 0ull, 32ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 33ull, 10ull, 0ull, 33ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 34ull, 10ull, 0ull, 34ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 35ull, 10ull, 0ull, 35ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 36ull, 10ull, 0ull, 36ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 37ull, 10ull, 0ull, 37ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 38ull, 10ull, 0ull, 38ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 39ull, 10ull, 0ull, 39ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 40ull, 10ull, 0ull, 40ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 41ull, 10ull, 0ull, 41ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 42ull, 10ull, 0ull, 42ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 43ull, 10ull, 0ull, 43ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 44ull, 10ull, 0ull, 44ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 45ull, 10ull, 0ull, 45ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 46ull, 10ull, 0ull, 46ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 47ull, 10ull, 0ull, 47ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 48ull, 10ull, 0ull, 48ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 49ull, 10ull, 0ull, 49ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 50ull, 10ull, 0ull, 50ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 51ull, 10ull, 0ull, 51ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 52ull, 10ull, 0ull, 52ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 53ull, 10ull, 0ull, 53ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 54ull, 10ull, 0ull, 54ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 55ull, 10ull, 0ull, 55ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 56ull, 10ull, 0ull, 56ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 57ull, 10ull, 0ull, 57ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 58ull, 10ull, 0ull, 58ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 59ull, 10ull, 0ull, 59ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 60ull, 10ull, 0ull, 60ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 61ull, 10ull, 0ull, 61ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 62ull, 10ull, 0ull, 62ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 63ull, 10ull, 0ull, 63ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 64ull, 10ull, 0ull, 64ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 65ull, 10ull, 0ull, 65ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 66ull, 10ull, 0ull, 66ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 67ull, 10ull, 0ull, 67ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 68ull, 10ull, 0ull, 68ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 69ull, 10ull, 0ull, 69ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 70ull, 10ull, 0ull, 70ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 71ull, 10ull, 0ull, 71ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 72ull, 10ull, 0ull, 72ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 73ull, 10ull, 0ull, 73ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 74ull, 10ull, 0ull, 74ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 75ull, 10ull, 0ull, 75ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 76ull, 10ull, 0ull, 76ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 77ull, 10ull, 0ull, 77ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 78ull, 10ull, 0ull, 78ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 79ull, 10ull, 0ull, 79ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 80ull, 10ull, 0ull, 80ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 81ull, 10ull, 0ull, 81ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 82ull, 10ull, 0ull, 82ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 83ull, 10ull, 0ull, 83ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 84ull, 10ull, 0ull, 84ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 85ull, 10ull, 0ull, 85ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 86ull, 10ull, 0ull, 86ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 87ull, 10ull, 0ull, 87ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 88ull, 10ull, 0ull, 88ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 89ull, 10ull, 0ull, 89ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 90ull, 10ull, 0ull, 90ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 91ull, 10ull, 0ull, 91ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 92ull, 10ull, 0ull, 92ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 93ull, 10ull, 0ull, 93ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 94ull, 10ull, 0ull, 94ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 95ull, 10ull, 0ull, 95ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 96ull, 10ull, 0ull, 96ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 97ull, 10ull, 0ull, 97ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 98ull, 10ull, 0ull, 98ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 99ull, 10ull, 0ull, 99ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 100ull, 10ull, 0ull, 100ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 101ull, 10ull, 0ull, 101ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 102ull, 10ull, 0ull, 102ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 103ull, 10ull, 0ull, 103ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 104ull, 10ull, 0ull, 104ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 105ull, 10ull, 0ull, 105ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 106ull, 10ull, 0ull, 106ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 107ull, 10ull, 0ull, 107ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 108ull, 10ull, 0ull, 108ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 109ull, 10ull, 0ull, 109ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 110ull, 10ull, 0ull, 110ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 111ull, 10ull, 0ull, 111ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 112ull, 10ull, 0ull, 112ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 113ull, 10ull, 0ull, 113ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 114ull, 10ull, 0ull, 114ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 115ull, 10ull, 0ull, 115ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 116ull, 10ull, 0ull, 116ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 117ull, 10ull, 0ull, 117ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 118ull, 10ull, 0ull, 118ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 119ull, 10ull, 0ull, 119ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 120ull, 10ull, 0ull, 120ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 121ull, 10ull, 0ull, 121ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 122ull, 10ull, 0ull, 122ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 123ull, 10ull, 0ull, 123ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 124ull, 10ull, 0ull, 124ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 125ull, 10ull, 0ull, 125ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 126ull, 10ull, 0ull, 126ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 127ull, 10ull, 0ull, 127ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 128ull, 10ull, 0ull, 128ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 129ull, 10ull, 0ull, 129ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 129ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 3ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_tick_full = {9815272703367221927ull, 748469964934888981ull, 128, 18446744073709551615ull, 2ull, 0ull, 2ull, 134};
const Op kProg_counters[] = {
  {'H', {18446744073709551615ull, 18446744073709551614ull, 18446744073709551615ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 1ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 1ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 1ull, 0ull, 1ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 1ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 1ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'H', {5ull, 18446744073709551615ull, 18446744073709551614ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 1ull, 0ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 3ull, 1ull, 0ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 4ull, 1ull, 0ull, 4ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 5ull, 1ull, 0ull, 5ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 6ull, 1ull, 0ull, 6ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 7ull, 1ull, 0ull, 7ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 8ull, 1ull, 0ull, 8ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 9ull, 1ull, 0ull, 9ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 10ull, 1ull, 0ull, 10ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 11ull, 1ull, 0ull, 11ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 12ull, 1ull, 0ull, 12ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 13ull, 1ull, 0ull, 13ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 14ull, 1ull, 0ull, 14ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 15ull, 1ull, 0ull, 15ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 16ull, 1ull, 0ull, 16ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 17ull, 1ull, 0ull, 17ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 18ull, 1ull, 0ull, 18ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 19ull, 1ull, 0ull, 19ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 20ull, 1ull, 0ull, 20ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 21ull, 1ull, 0ull, 21ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 22ull, 1ull, 0ull, 22ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 23ull, 1ull, 0ull, 23ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 24ull, 1ull, 0ull, 24ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 25ull, 1ull, 0ull, 25ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 26ull, 1ull, 0ull, 26ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 27ull, 1ull, 0ull, 27ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 28ull, 1ull, 0ull, 28ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 29ull, 1ull, 0ull, 29ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 30ull, 1ull, 0ull, 30ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 31ull, 1ull, 0ull, 31ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 32ull, 1ull, 0ull, 32ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 33ull, 1ull, 0ull, 33ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 34ull, 1ull, 0ull, 34ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 35ull, 1ull, 0ull, 35ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 36ull, 1ull, 0ull, 36ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 37ull, 1ull, 0ull, 37ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 38ull, 1ull, 0ull, 38ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 39ull, 1ull, 0ull, 39ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 40ull, 1ull, 0ull, 40ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 41ull, 1ull, 0ull, 41ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 42ull, 1ull, 0ull, 42ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 43ull, 1ull, 0ull, 43ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 44ull, 1ull, 0ull, 44ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 45ull, 1ull, 0ull, 45ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 46ull, 1ull, 0ull, 46ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 47ull, 1ull, 0ull, 47ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 48ull, 1ull, 0ull, 48ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 49ull, 1ull, 0ull, 49ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 50ull, 1ull, 0ull, 50ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 51ull, 1ull, 0ull, 51ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 52ull, 1ull, 0ull, 52ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 53ull, 1ull, 0ull, 53ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 54ull, 1ull, 0ull, 54ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 55ull, 1ull, 0ull, 55ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 56ull, 1ull, 0ull, 56ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 57ull, 1ull, 0ull, 57ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 58ull, 1ull, 0ull, 58ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 59ull, 1ull, 0ull, 59ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 60ull, 1ull, 0ull, 60ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 61ull, 1ull, 0ull, 61ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 62ull, 1ull, 0ull, 62ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 63ull, 1ull, 0ull, 63ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 64ull, 1ull, 0ull, 64ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 65ull, 1ull, 0ull, 65ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 66ull, 1ull, 0ull, 66ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 67ull, 1ull, 0ull, 67ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 68ull, 1ull, 0ull, 68ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 69ull, 1ull, 0ull, 69ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 70ull, 1ull, 0ull, 70ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 71ull, 1ull, 0ull, 71ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 72ull, 1ull, 0ull, 72ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 73ull, 1ull, 0ull, 73ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 74ull, 1ull, 0ull, 74ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 75ull, 1ull, 0ull, 75ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 76ull, 1ull, 0ull, 76ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 77ull, 1ull, 0ull, 77ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 78ull, 1ull, 0ull, 78ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 79ull, 1ull, 0ull, 79ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 80ull, 1ull, 0ull, 80ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 81ull, 1ull, 0ull, 81ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 82ull, 1ull, 0ull, 82ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 83ull, 1ull, 0ull, 83ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 84ull, 1ull, 0ull, 84ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 85ull, 1ull, 0ull, 85ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 86ull, 1ull, 0ull, 86ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 87ull, 1ull, 0ull, 87ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 88ull, 1ull, 0ull, 88ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 89ull, 1ull, 0ull, 89ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 90ull, 1ull, 0ull, 90ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 91ull, 1ull, 0ull, 91ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 92ull, 1ull, 0ull, 92ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 93ull, 1ull, 0ull, 93ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 94ull, 1ull, 0ull, 94ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 95ull, 1ull, 0ull, 95ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 96ull, 1ull, 0ull, 96ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 97ull, 1ull, 0ull, 97ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 98ull, 1ull, 0ull, 98ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 99ull, 1ull, 0ull, 99ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 100ull, 1ull, 0ull, 100ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 101ull, 1ull, 0ull, 101ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 102ull, 1ull, 0ull, 102ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 103ull, 1ull, 0ull, 103ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 104ull, 1ull, 0ull, 104ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 105ull, 1ull, 0ull, 105ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 106ull, 1ull, 0ull, 106ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 107ull, 1ull, 0ull, 107ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 108ull, 1ull, 0ull, 108ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 109ull, 1ull, 0ull, 109ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 110ull, 1ull, 0ull, 110ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 111ull, 1ull, 0ull, 111ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 112ull, 1ull, 0ull, 112ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 113ull, 1ull, 0ull, 113ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 114ull, 1ull, 0ull, 114ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 115ull, 1ull, 0ull, 115ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 116ull, 1ull, 0ull, 116ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 117ull, 1ull, 0ull, 117ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 118ull, 1ull, 0ull, 118ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 119ull, 1ull, 0ull, 119ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 120ull, 1ull, 0ull, 120ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 121ull, 1ull, 0ull, 121ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 122ull, 1ull, 0ull, 122ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 123ull, 1ull, 0ull, 123ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 124ull, 1ull, 0ull, 124ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 125ull, 1ull, 0ull, 125ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 126ull, 1ull, 0ull, 126ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 127ull, 1ull, 0ull, 127ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 128ull, 1ull, 0ull, 128ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 129ull, 1ull, 0ull, 129ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 130ull, 1ull, 0ull, 130ull, 0ull, 0ull, 0ull}},
  {'F', {7ull, 7ull, 7ull, 1ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {7ull, 7ull, 7ull, 1ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_counters = {8971455098167155077ull, 13857440853791056016ull, 128, 132ull, 5ull, 18446744073709551615ull, 18446744073709551615ull, 142};
const Op kProg_inc_count_low[] = {
  {'E', {0ull, 1ull, 0ull, 10ull, 0ull, 0ull, 500ull, 1ull}},
  {'E', {1ull, 1ull, 1ull, 10ull, 0ull, 1ull, 999ull, 1ull}},
  {'E', {2ull, 1ull, 2ull, 10ull, 0ull, 2ull, 998ull, 1ull}},
  {'E', {3ull, 1ull, 3ull, 10ull, 0ull, 3ull, 500ull, 1ull}},
  {'E', {4ull, 1ull, 4ull, 10ull, 0ull, 4ull, 996ull, 1ull}},
  {'E', {5ull, 1ull, 5ull, 10ull, 0ull, 5ull, 995ull, 1ull}},
  {'E', {6ull, 1ull, 6ull, 10ull, 0ull, 6ull, 500ull, 1ull}},
  {'E', {7ull, 1ull, 7ull, 10ull, 0ull, 7ull, 993ull, 1ull}},
  {'E', {8ull, 1ull, 8ull, 10ull, 0ull, 8ull, 992ull, 1ull}},
  {'E', {9ull, 1ull, 9ull, 10ull, 0ull, 9ull, 500ull, 1ull}},
  {'E', {10ull, 1ull, 10ull, 10ull, 0ull, 10ull, 990ull, 1ull}},
  {'E', {11ull, 1ull, 11ull, 10ull, 0ull, 11ull, 989ull, 1ull}},
  {'E', {12ull, 1ull, 12ull, 10ull, 0ull, 12ull, 500ull, 1ull}},
  {'E', {13ull, 1ull, 13ull, 10ull, 0ull, 13ull, 987ull, 1ull}},
  {'E', {14ull, 1ull, 14ull, 10ull, 0ull, 14ull, 986ull, 1ull}},
  {'E', {15ull, 1ull, 15ull, 10ull, 0ull, 15ull, 500ull, 1ull}},
  {'E', {16ull, 1ull, 16ull, 10ull, 0ull, 16ull, 984ull, 1ull}},
  {'E', {17ull, 1ull, 17ull, 10ull, 0ull, 17ull, 983ull, 1ull}},
  {'E', {18ull, 1ull, 18ull, 10ull, 0ull, 18ull, 500ull, 1ull}},
  {'E', {19ull, 1ull, 19ull, 10ull, 0ull, 19ull, 981ull, 1ull}},
  {'E', {20ull, 1ull, 20ull, 10ull, 0ull, 20ull, 980ull, 1ull}},
  {'E', {21ull, 1ull, 21ull, 10ull, 0ull, 21ull, 500ull, 1ull}},
  {'E', {22ull, 1ull, 22ull, 10ull, 0ull, 22ull, 978ull, 1ull}},
  {'E', {23ull, 1ull, 23ull, 10ull, 0ull, 23ull, 977ull, 1ull}},
  {'E', {24ull, 1ull, 24ull, 10ull, 0ull, 24ull, 500ull, 1ull}},
  {'E', {25ull, 1ull, 25ull, 10ull, 0ull, 25ull, 975ull, 1ull}},
  {'E', {26ull, 1ull, 26ull, 10ull, 0ull, 26ull, 974ull, 1ull}},
  {'E', {27ull, 1ull, 27ull, 10ull, 0ull, 27ull, 500ull, 1ull}},
  {'E', {28ull, 1ull, 28ull, 10ull, 0ull, 28ull, 972ull, 1ull}},
  {'E', {29ull, 1ull, 29ull, 10ull, 0ull, 29ull, 971ull, 1ull}},
  {'E', {30ull, 1ull, 30ull, 10ull, 0ull, 30ull, 500ull, 1ull}},
  {'E', {31ull, 1ull, 31ull, 10ull, 0ull, 31ull, 969ull, 1ull}},
  {'E', {32ull, 1ull, 32ull, 10ull, 0ull, 32ull, 968ull, 1ull}},
  {'E', {33ull, 1ull, 33ull, 10ull, 0ull, 33ull, 500ull, 1ull}},
  {'E', {34ull, 1ull, 34ull, 10ull, 0ull, 34ull, 966ull, 1ull}},
  {'E', {35ull, 1ull, 35ull, 10ull, 0ull, 35ull, 965ull, 1ull}},
  {'E', {36ull, 1ull, 36ull, 10ull, 0ull, 36ull, 500ull, 1ull}},
  {'E', {37ull, 1ull, 37ull, 10ull, 0ull, 37ull, 963ull, 1ull}},
  {'E', {38ull, 1ull, 38ull, 10ull, 0ull, 38ull, 962ull, 1ull}},
  {'E', {39ull, 1ull, 39ull, 10ull, 0ull, 39ull, 500ull, 1ull}},
  {'E', {40ull, 1ull, 40ull, 10ull, 0ull, 40ull, 960ull, 1ull}},
  {'E', {41ull, 1ull, 41ull, 10ull, 0ull, 41ull, 959ull, 1ull}},
  {'E', {42ull, 1ull, 42ull, 10ull, 0ull, 42ull, 500ull, 1ull}},
  {'E', {43ull, 1ull, 43ull, 10ull, 0ull, 43ull, 957ull, 1ull}},
  {'E', {44ull, 1ull, 44ull, 10ull, 0ull, 44ull, 956ull, 1ull}},
  {'E', {45ull, 1ull, 45ull, 10ull, 0ull, 45ull, 500ull, 1ull}},
  {'E', {46ull, 1ull, 46ull, 10ull, 0ull, 46ull, 954ull, 1ull}},
  {'E', {47ull, 1ull, 47ull, 10ull, 0ull, 47ull, 953ull, 1ull}},
  {'E', {48ull, 1ull, 48ull, 10ull, 0ull, 48ull, 500ull, 1ull}},
  {'E', {49ull, 1ull, 49ull, 10ull, 0ull, 49ull, 951ull, 1ull}},
  {'E', {50ull, 1ull, 50ull, 10ull, 0ull, 50ull, 950ull, 1ull}},
  {'E', {51ull, 1ull, 51ull, 10ull, 0ull, 51ull, 500ull, 1ull}},
  {'E', {52ull, 1ull, 52ull, 10ull, 0ull, 52ull, 948ull, 1ull}},
  {'E', {53ull, 1ull, 53ull, 10ull, 0ull, 53ull, 947ull, 1ull}},
  {'E', {54ull, 1ull, 54ull, 10ull, 0ull, 54ull, 500ull, 1ull}},
  {'E', {55ull, 1ull, 55ull, 10ull, 0ull, 55ull, 945ull, 1ull}},
  {'E', {56ull, 1ull, 56ull, 10ull, 0ull, 56ull, 944ull, 1ull}},
  {'E', {57ull, 1ull, 57ull, 10ull, 0ull, 57ull, 500ull, 1ull}},
  {'E', {58ull, 1ull, 58ull, 10ull, 0ull, 58ull, 942ull, 1ull}},
  {'E', {59ull, 1ull, 59ull, 10ull, 0ull, 59ull, 941ull, 1ull}},
  {'E', {60ull, 1ull, 60ull, 10ull, 0ull, 60ull, 500ull, 1ull}},
  {'E', {61ull, 1ull, 61ull, 10ull, 0ull, 61ull, 939ull, 1ull}},
  {'E', {62ull, 1ull, 62ull, 10ull, 0ull, 62ull, 938ull, 1ull}},
  {'E', {63ull, 1ull, 63ull, 10ull, 0ull, 63ull, 500ull, 1ull}},
  {'E', {64ull, 1ull, 64ull, 10ull, 0ull, 64ull, 936ull, 1ull}},
  {'E', {65ull, 1ull, 65ull, 10ull, 0ull, 65ull, 935ull, 1ull}},
  {'E', {66ull, 1ull, 66ull, 10ull, 0ull, 66ull, 500ull, 1ull}},
  {'E', {67ull, 1ull, 67ull, 10ull, 0ull, 67ull, 933ull, 1ull}},
  {'E', {68ull, 1ull, 68ull, 10ull, 0ull, 68ull, 932ull, 1ull}},
  {'E', {69ull, 1ull, 69ull, 10ull, 0ull, 69ull, 500ull, 1ull}},
  {'E', {70ull, 1ull, 70ull, 10ull, 0ull, 70ull, 930ull, 1ull}},
  {'E', {71ull, 1ull, 71ull, 10ull, 0ull, 71ull, 929ull, 1ull}},
  {'E', {72ull, 1ull, 72ull, 10ull, 0ull, 72ull, 500ull, 1ull}},
  {'E', {73ull, 1ull, 73ull, 10ull, 0ull, 73ull, 927ull, 1ull}},
  {'E', {74ull, 1ull, 74ull, 10ull, 0ull, 74ull, 926ull, 1ull}},
  {'E', {75ull, 1ull, 75ull, 10ull, 0ull, 75ull, 500ull, 1ull}},
  {'E', {76ull, 1ull, 76ull, 10ull, 0ull, 76ull, 924ull, 1ull}},
  {'E', {77ull, 1ull, 77ull, 10ull, 0ull, 77ull, 923ull, 1ull}},
  {'E', {78ull, 1ull, 78ull, 10ull, 0ull, 78ull, 500ull, 1ull}},
  {'E', {79ull, 1ull, 79ull, 10ull, 0ull, 79ull, 921ull, 1ull}},
  {'E', {80ull, 1ull, 80ull, 10ull, 0ull, 80ull, 920ull, 1ull}},
  {'E', {81ull, 1ull, 81ull, 10ull, 0ull, 81ull, 500ull, 1ull}},
  {'E', {82ull, 1ull, 82ull, 10ull, 0ull, 82ull, 918ull, 1ull}},
  {'E', {83ull, 1ull, 83ull, 10ull, 0ull, 83ull, 917ull, 1ull}},
  {'E', {84ull, 1ull, 84ull, 10ull, 0ull, 84ull, 500ull, 1ull}},
  {'E', {85ull, 1ull, 85ull, 10ull, 0ull, 85ull, 915ull, 1ull}},
  {'E', {86ull, 1ull, 86ull, 10ull, 0ull, 86ull, 914ull, 1ull}},
  {'E', {87ull, 1ull, 87ull, 10ull, 0ull, 87ull, 500ull, 1ull}},
  {'E', {88ull, 1ull, 88ull, 10ull, 0ull, 88ull, 912ull, 1ull}},
  {'E', {89ull, 1ull, 89ull, 10ull, 0ull, 89ull, 911ull, 1ull}},
  {'E', {90ull, 1ull, 90ull, 10ull, 0ull, 90ull, 500ull, 1ull}},
  {'E', {91ull, 1ull, 91ull, 10ull, 0ull, 91ull, 909ull, 1ull}},
  {'E', {92ull, 1ull, 92ull, 10ull, 0ull, 92ull, 908ull, 1ull}},
  {'E', {93ull, 1ull, 93ull, 10ull, 0ull, 93ull, 500ull, 1ull}},
  {'E', {94ull, 1ull, 94ull, 10ull, 0ull, 94ull, 906ull, 1ull}},
  {'E', {95ull, 1ull, 95ull, 10ull, 0ull, 95ull, 905ull, 1ull}},
  {'E', {96ull, 1ull, 96ull, 10ull, 0ull, 96ull, 500ull, 1ull}},
  {'E', {97ull, 1ull, 97ull, 10ull, 0ull, 97ull, 903ull, 1ull}},
  {'E', {98ull, 1ull, 98ull, 10ull, 0ull, 98ull, 902ull, 1ull}},
  {'E', {99ull, 1ull, 99ull, 10ull, 0ull, 99ull, 500ull, 1ull}},
  {'E', {100ull, 1ull, 100ull, 10ull, 0ull, 100ull, 900ull, 1ull}},
  {'E', {101ull, 1ull, 101ull, 10ull, 0ull, 101ull, 899ull, 1ull}},
  {'E', {102ull, 1ull, 102ull, 10ull, 0ull, 102ull, 500ull, 1ull}},
  {'E', {103ull, 1ull, 103ull, 10ull, 0ull, 103ull, 897ull, 1ull}},
  {'E', {104ull, 1ull, 104ull, 10ull, 0ull, 104ull, 896ull, 1ull}},
  {'E', {105ull, 1ull, 105ull, 10ull, 0ull, 105ull, 500ull, 1ull}},
  {'E', {106ull, 1ull, 106ull, 10ull, 0ull, 106ull, 894ull, 1ull}},
  {'E', {107ull, 1ull, 107ull, 10ull, 0ull, 107ull, 893ull, 1ull}},
  {'E', {108ull, 1ull, 108ull, 10ull, 0ull, 108ull, 500ull, 1ull}},
  {'E', {109ull, 1ull, 109ull, 10ull, 0ull, 109ull, 891ull, 1ull}},
  {'E', {110ull, 1ull, 110ull, 10ull, 0ull, 110ull, 890ull, 1ull}},
  {'E', {111ull, 1ull, 111ull, 10ull, 0ull, 111ull, 500ull, 1ull}},
  {'E', {112ull, 1ull, 112ull, 10ull, 0ull, 112ull, 888ull, 1ull}},
  {'E', {113ull, 1ull, 113ull, 10ull, 0ull, 113ull, 887ull, 1ull}},
  {'E', {114ull, 1ull, 114ull, 10ull, 0ull, 114ull, 500ull, 1ull}},
  {'E', {115ull, 1ull, 115ull, 10ull, 0ull, 115ull, 885ull, 1ull}},
  {'E', {116ull, 1ull, 116ull, 10ull, 0ull, 116ull, 884ull, 1ull}},
  {'E', {117ull, 1ull, 117ull, 10ull, 0ull, 117ull, 500ull, 1ull}},
  {'E', {118ull, 1ull, 118ull, 10ull, 0ull, 118ull, 882ull, 1ull}},
  {'E', {119ull, 1ull, 119ull, 10ull, 0ull, 119ull, 881ull, 1ull}},
  {'E', {120ull, 1ull, 120ull, 10ull, 0ull, 120ull, 500ull, 1ull}},
  {'E', {121ull, 1ull, 121ull, 10ull, 0ull, 121ull, 879ull, 1ull}},
  {'E', {122ull, 1ull, 122ull, 10ull, 0ull, 122ull, 878ull, 1ull}},
  {'E', {123ull, 1ull, 123ull, 10ull, 0ull, 123ull, 500ull, 1ull}},
  {'E', {124ull, 1ull, 124ull, 10ull, 0ull, 124ull, 876ull, 1ull}},
  {'E', {125ull, 1ull, 125ull, 10ull, 0ull, 125ull, 875ull, 1ull}},
  {'E', {126ull, 1ull, 126ull, 10ull, 0ull, 126ull, 500ull, 1ull}},
  {'E', {127ull, 1ull, 127ull, 10ull, 0ull, 127ull, 873ull, 1ull}},
  {'N', {100ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 3ull, 3ull, 3ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 4ull, 3ull, 3ull, 4ull, 0ull, 0ull, 0ull}},
  {'N', {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 5ull, 3ull, 3ull, 5ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_count_low = {4678820108535708375ull, 15094498766084365379ull, 0, 3ull, 0ull, 0ull, 3ull, 3};
const Op kProg_inc_count_high[] = {
  {'E', {0ull, 1ull, 0ull, 10ull, 0ull, 0ull, 10ull, 1ull}},
  {'E', {2ull, 1ull, 2ull, 10ull, 0ull, 2ull, 12ull, 1ull}},
  {'E', {4ull, 1ull, 4ull, 10ull, 0ull, 4ull, 14ull, 1ull}},
  {'E', {6ull, 1ull, 6ull, 10ull, 0ull, 6ull, 16ull, 1ull}},
  {'E', {8ull, 1ull, 8ull, 10ull, 0ull, 8ull, 18ull, 1ull}},
  {'E', {10ull, 1ull, 10ull, 10ull, 0ull, 10ull, 20ull, 1ull}},
  {'E', {12ull, 1ull, 12ull, 10ull, 0ull, 12ull, 22ull, 1ull}},
  {'E', {14ull, 1ull, 14ull, 10ull, 0ull, 14ull, 24ull, 1ull}},
  {'E', {16ull, 1ull, 16ull, 10ull, 0ull, 16ull, 26ull, 1ull}},
  {'E', {18ull, 1ull, 18ull, 10ull, 0ull, 18ull, 28ull, 1ull}},
  {'E', {20ull, 1ull, 20ull, 10ull, 0ull, 20ull, 30ull, 1ull}},
  {'E', {22ull, 1ull, 22ull, 10ull, 0ull, 22ull, 32ull, 1ull}},
  {'E', {24ull, 1ull, 24ull, 10ull, 0ull, 24ull, 34ull, 1ull}},
  {'E', {26ull, 1ull, 26ull, 10ull, 0ull, 26ull, 36ull, 1ull}},
  {'E', {28ull, 1ull, 28ull, 10ull, 0ull, 28ull, 38ull, 1ull}},
  {'E', {30ull, 1ull, 30ull, 10ull, 0ull, 30ull, 40ull, 1ull}},
  {'E', {32ull, 1ull, 32ull, 10ull, 0ull, 32ull, 42ull, 1ull}},
  {'E', {34ull, 1ull, 34ull, 10ull, 0ull, 34ull, 44ull, 1ull}},
  {'E', {36ull, 1ull, 36ull, 10ull, 0ull, 36ull, 46ull, 1ull}},
  {'E', {38ull, 1ull, 38ull, 10ull, 0ull, 38ull, 48ull, 1ull}},
  {'E', {40ull, 1ull, 40ull, 10ull, 0ull, 40ull, 50ull, 1ull}},
  {'E', {42ull, 1ull, 42ull, 10ull, 0ull, 42ull, 52ull, 1ull}},
  {'E', {44ull, 1ull, 44ull, 10ull, 0ull, 44ull, 54ull, 1ull}},
  {'E', {46ull, 1ull, 46ull, 10ull, 0ull, 46ull, 56ull, 1ull}},
  {'E', {48ull, 1ull, 48ull, 10ull, 0ull, 48ull, 58ull, 1ull}},
  {'E', {50ull, 1ull, 50ull, 10ull, 0ull, 50ull, 60ull, 1ull}},
  {'E', {52ull, 1ull, 52ull, 10ull, 0ull, 52ull, 62ull, 1ull}},
  {'E', {54ull, 1ull, 54ull, 10ull, 0ull, 54ull, 64ull, 1ull}},
  {'E', {56ull, 1ull, 56ull, 10ull, 0ull, 56ull, 66ull, 1ull}},
  {'E', {58ull, 1ull, 58ull, 10ull, 0ull, 58ull, 68ull, 1ull}},
  {'E', {60ull, 1ull, 60ull, 10ull, 0ull, 60ull, 70ull, 1ull}},
  {'E', {62ull, 1ull, 62ull, 10ull, 0ull, 62ull, 72ull, 1ull}},
  {'E', {64ull, 1ull, 64ull, 10ull, 0ull, 64ull, 74ull, 1ull}},
  {'E', {66ull, 1ull, 66ull, 10ull, 0ull, 66ull, 76ull, 1ull}},
  {'E', {68ull, 1ull, 68ull, 10ull, 0ull, 68ull, 78ull, 1ull}},
  {'E', {70ull, 1ull, 70ull, 10ull, 0ull, 70ull, 80ull, 1ull}},
  {'E', {72ull, 1ull, 72ull, 10ull, 0ull, 72ull, 82ull, 1ull}},
  {'E', {74ull, 1ull, 74ull, 10ull, 0ull, 74ull, 84ull, 1ull}},
  {'E', {76ull, 1ull, 76ull, 10ull, 0ull, 76ull, 86ull, 1ull}},
  {'E', {78ull, 1ull, 78ull, 10ull, 0ull, 78ull, 88ull, 1ull}},
  {'E', {80ull, 1ull, 80ull, 10ull, 0ull, 80ull, 90ull, 1ull}},
  {'E', {82ull, 1ull, 82ull, 10ull, 0ull, 82ull, 92ull, 1ull}},
  {'E', {84ull, 1ull, 84ull, 10ull, 0ull, 84ull, 94ull, 1ull}},
  {'E', {86ull, 1ull, 86ull, 10ull, 0ull, 86ull, 96ull, 1ull}},
  {'E', {88ull, 1ull, 88ull, 10ull, 0ull, 88ull, 98ull, 1ull}},
  {'E', {90ull, 1ull, 90ull, 10ull, 0ull, 90ull, 100ull, 1ull}},
  {'E', {92ull, 1ull, 92ull, 10ull, 0ull, 92ull, 102ull, 1ull}},
  {'E', {94ull, 1ull, 94ull, 10ull, 0ull, 94ull, 104ull, 1ull}},
  {'E', {96ull, 1ull, 96ull, 10ull, 0ull, 96ull, 106ull, 1ull}},
  {'E', {98ull, 1ull, 98ull, 10ull, 0ull, 98ull, 108ull, 1ull}},
  {'E', {100ull, 1ull, 100ull, 10ull, 0ull, 100ull, 110ull, 1ull}},
  {'E', {102ull, 1ull, 102ull, 10ull, 0ull, 102ull, 112ull, 1ull}},
  {'E', {104ull, 1ull, 104ull, 10ull, 0ull, 104ull, 114ull, 1ull}},
  {'E', {106ull, 1ull, 106ull, 10ull, 0ull, 106ull, 116ull, 1ull}},
  {'E', {108ull, 1ull, 108ull, 10ull, 0ull, 108ull, 118ull, 1ull}},
  {'E', {110ull, 1ull, 110ull, 10ull, 0ull, 110ull, 120ull, 1ull}},
  {'E', {112ull, 1ull, 112ull, 10ull, 0ull, 112ull, 122ull, 1ull}},
  {'E', {114ull, 1ull, 114ull, 10ull, 0ull, 114ull, 124ull, 1ull}},
  {'E', {116ull, 1ull, 116ull, 10ull, 0ull, 116ull, 126ull, 1ull}},
  {'E', {118ull, 1ull, 118ull, 10ull, 0ull, 118ull, 128ull, 1ull}},
  {'E', {120ull, 1ull, 120ull, 10ull, 0ull, 120ull, 130ull, 1ull}},
  {'E', {122ull, 1ull, 122ull, 10ull, 0ull, 122ull, 132ull, 1ull}},
  {'E', {124ull, 1ull, 124ull, 10ull, 0ull, 124ull, 134ull, 1ull}},
  {'E', {126ull, 1ull, 126ull, 10ull, 0ull, 126ull, 136ull, 1ull}},
  {'N', {128ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 3ull, 3ull, 3ull, 3ull, 0ull, 0ull, 0ull}},
  {'N', {500ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 4ull, 3ull, 3ull, 4ull, 0ull, 0ull, 0ull}},
  {'F', {3ull, 3ull, 3ull, 3ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 2ull, 10ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_count_high = {3153271331795842342ull, 17024145806125928192ull, 500, 3ull, 1ull, 1ull, 2ull, 6};
const Op kProg_inc_none_used[] = {
  {'N', {128ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 3ull, 3ull, 3ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 4ull, 3ull, 3ull, 4ull, 0ull, 0ull, 0ull}},
  {'N', {5ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 5ull, 3ull, 3ull, 5ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 6ull, 3ull, 3ull, 6ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_none_used = {4293412624433750917ull, 17429816102331708249ull, 7, 4ull, 0ull, 0ull, 2ull, 4};
const Op kProg_inc_count_low_free[] = {
  {'E', {5ull, 1ull, 1ull, 1ull, 0ull, 1ull, 1ull, 1ull}},
  {'N', {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 1ull, 0ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 3ull, 1ull, 0ull, 3ull, 0ull, 0ull, 0ull}},
  {'N', {127ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 4ull, 1ull, 0ull, 4ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 5ull, 1ull, 0ull, 5ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_count_low_free = {14417561491788717447ull, 5309496928086916416ull, 128, 4ull, 0ull, 0ull, 1ull, 4};
const Op kProg_inc_unused_flag[] = {
  {'E', {0ull, 1ull, 1ull, 1ull, 0ull, 9ull, 3ull, 0ull}},
  {'E', {1ull, 1ull, 1ull, 1ull, 0ull, 8ull, 4ull, 1ull}},
  {'N', {1ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'F', {1ull, 1ull, 1ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 1ull, 1ull, 0ull, 7ull, 0ull, 0ull, 0ull}},
  {'I', {1ull, 2ull, 1ull, 0ull, 6ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_unused_flag = {160139458447629261ull, 11012748753080841207ull, 2, 3ull, 1ull, 0ull, 0ull, 4};
const Op kProg_inc_ties[] = {
  {'E', {0ull, 1ull, 1ull, 1ull, 0ull, 1ull, 7ull, 1ull}},
  {'E', {1ull, 1ull, 2ull, 1ull, 0ull, 1ull, 3ull, 1ull}},
  {'E', {2ull, 1ull, 3ull, 1ull, 0ull, 1ull, 3ull, 1ull}},
  {'E', {3ull, 1ull, 4ull, 1ull, 0ull, 1ull, 3ull, 1ull}},
  {'N', {4ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'N', {128ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 2ull, 2ull, 2ull, 2ull, 0ull, 0ull, 0ull}},
  {'I', {2ull, 3ull, 2ull, 2ull, 3ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_ties = {12017906404658336997ull, 18088049303777452079ull, 128, 2ull, 0ull, 0ull, 2ull, 2};
const Op kProg_inc_slot0_unused[] = {
  {'E', {1ull, 1ull, 1ull, 10ull, 0ull, 1ull, 27ull, 1ull}},
  {'E', {2ull, 1ull, 2ull, 10ull, 0ull, 2ull, 34ull, 1ull}},
  {'E', {3ull, 1ull, 3ull, 10ull, 0ull, 3ull, 41ull, 1ull}},
  {'E', {4ull, 1ull, 4ull, 10ull, 0ull, 4ull, 48ull, 1ull}},
  {'E', {5ull, 1ull, 5ull, 10ull, 0ull, 5ull, 55ull, 1ull}},
  {'E', {6ull, 1ull, 6ull, 10ull, 0ull, 6ull, 62ull, 1ull}},
  {'E', {7ull, 1ull, 7ull, 10ull, 0ull, 7ull, 69ull, 1ull}},
  {'E', {8ull, 1ull, 8ull, 10ull, 0ull, 8ull, 26ull, 1ull}},
  {'E', {9ull, 1ull, 9ull, 10ull, 0ull, 9ull, 33ull, 1ull}},
  {'E', {10ull, 1ull, 10ull, 10ull, 0ull, 10ull, 40ull, 1ull}},
  {'E', {11ull, 1ull, 11ull, 10ull, 0ull, 11ull, 47ull, 1ull}},
  {'E', {12ull, 1ull, 12ull, 10ull, 0ull, 12ull, 54ull, 1ull}},
  {'E', {13ull, 1ull, 13ull, 10ull, 0ull, 13ull, 61ull, 1ull}},
  {'E', {14ull, 1ull, 14ull, 10ull, 0ull, 14ull, 68ull, 1ull}},
  {'E', {15ull, 1ull, 15ull, 10ull, 0ull, 15ull, 25ull, 1ull}},
  {'E', {16ull, 1ull, 16ull, 10ull, 0ull, 16ull, 32ull, 1ull}},
  {'E', {17ull, 1ull, 17ull, 10ull, 0ull, 17ull, 39ull, 1ull}},
  {'E', {18ull, 1ull, 18ull, 10ull, 0ull, 18ull, 46ull, 1ull}},
  {'E', {19ull, 1ull, 19ull, 10ull, 0ull, 19ull, 53ull, 1ull}},
  {'E', {20ull, 1ull, 20ull, 10ull, 0ull, 20ull, 60ull, 1ull}},
  {'E', {21ull, 1ull, 21ull, 10ull, 0ull, 21ull, 67ull, 1ull}},
  {'E', {22ull, 1ull, 22ull, 10ull, 0ull, 22ull, 24ull, 1ull}},
  {'E', {23ull, 1ull, 23ull, 10ull, 0ull, 23ull, 31ull, 1ull}},
  {'E', {24ull, 1ull, 24ull, 10ull, 0ull, 24ull, 38ull, 1ull}},
  {'E', {25ull, 1ull, 25ull, 10ull, 0ull, 25ull, 45ull, 1ull}},
  {'E', {26ull, 1ull, 26ull, 10ull, 0ull, 26ull, 52ull, 1ull}},
  {'E', {27ull, 1ull, 27ull, 10ull, 0ull, 27ull, 59ull, 1ull}},
  {'E', {28ull, 1ull, 28ull, 10ull, 0ull, 28ull, 66ull, 1ull}},
  {'E', {29ull, 1ull, 29ull, 10ull, 0ull, 29ull, 23ull, 1ull}},
  {'E', {30ull, 1ull, 30ull, 10ull, 0ull, 30ull, 30ull, 1ull}},
  {'E', {31ull, 1ull, 31ull, 10ull, 0ull, 31ull, 37ull, 1ull}},
  {'E', {32ull, 1ull, 32ull, 10ull, 0ull, 32ull, 44ull, 1ull}},
  {'E', {33ull, 1ull, 33ull, 10ull, 0ull, 33ull, 51ull, 1ull}},
  {'E', {34ull, 1ull, 34ull, 10ull, 0ull, 34ull, 58ull, 1ull}},
  {'E', {35ull, 1ull, 35ull, 10ull, 0ull, 35ull, 65ull, 1ull}},
  {'E', {36ull, 1ull, 36ull, 10ull, 0ull, 36ull, 22ull, 1ull}},
  {'E', {37ull, 1ull, 37ull, 10ull, 0ull, 37ull, 29ull, 1ull}},
  {'E', {38ull, 1ull, 38ull, 10ull, 0ull, 38ull, 36ull, 1ull}},
  {'E', {39ull, 1ull, 39ull, 10ull, 0ull, 39ull, 43ull, 1ull}},
  {'E', {40ull, 1ull, 40ull, 10ull, 0ull, 40ull, 50ull, 1ull}},
  {'E', {41ull, 1ull, 41ull, 10ull, 0ull, 41ull, 57ull, 1ull}},
  {'E', {42ull, 1ull, 42ull, 10ull, 0ull, 42ull, 64ull, 1ull}},
  {'E', {43ull, 1ull, 43ull, 10ull, 0ull, 43ull, 21ull, 1ull}},
  {'E', {44ull, 1ull, 44ull, 10ull, 0ull, 44ull, 28ull, 1ull}},
  {'E', {45ull, 1ull, 45ull, 10ull, 0ull, 45ull, 35ull, 1ull}},
  {'E', {46ull, 1ull, 46ull, 10ull, 0ull, 46ull, 42ull, 1ull}},
  {'E', {47ull, 1ull, 47ull, 10ull, 0ull, 47ull, 49ull, 1ull}},
  {'E', {48ull, 1ull, 48ull, 10ull, 0ull, 48ull, 56ull, 1ull}},
  {'E', {49ull, 1ull, 49ull, 10ull, 0ull, 49ull, 63ull, 1ull}},
  {'E', {50ull, 1ull, 50ull, 10ull, 0ull, 50ull, 20ull, 1ull}},
  {'E', {51ull, 1ull, 51ull, 10ull, 0ull, 51ull, 27ull, 1ull}},
  {'E', {52ull, 1ull, 52ull, 10ull, 0ull, 52ull, 34ull, 1ull}},
  {'E', {53ull, 1ull, 53ull, 10ull, 0ull, 53ull, 41ull, 1ull}},
  {'E', {54ull, 1ull, 54ull, 10ull, 0ull, 54ull, 48ull, 1ull}},
  {'E', {55ull, 1ull, 55ull, 10ull, 0ull, 55ull, 55ull, 1ull}},
  {'E', {56ull, 1ull, 56ull, 10ull, 0ull, 56ull, 62ull, 1ull}},
  {'E', {57ull, 1ull, 57ull, 10ull, 0ull, 57ull, 69ull, 1ull}},
  {'E', {58ull, 1ull, 58ull, 10ull, 0ull, 58ull, 26ull, 1ull}},
  {'E', {59ull, 1ull, 59ull, 10ull, 0ull, 59ull, 33ull, 1ull}},
  {'E', {60ull, 1ull, 60ull, 10ull, 0ull, 60ull, 40ull, 1ull}},
  {'E', {61ull, 1ull, 61ull, 10ull, 0ull, 61ull, 47ull, 1ull}},
  {'E', {62ull, 1ull, 62ull, 10ull, 0ull, 62ull, 54ull, 1ull}},
  {'E', {63ull, 1ull, 63ull, 10ull, 0ull, 63ull, 61ull, 1ull}},
  {'E', {64ull, 1ull, 64ull, 10ull, 0ull, 64ull, 68ull, 1ull}},
  {'E', {65ull, 1ull, 65ull, 10ull, 0ull, 65ull, 25ull, 1ull}},
  {'E', {66ull, 1ull, 66ull, 10ull, 0ull, 66ull, 32ull, 1ull}},
  {'E', {67ull, 1ull, 67ull, 10ull, 0ull, 67ull, 39ull, 1ull}},
  {'E', {68ull, 1ull, 68ull, 10ull, 0ull, 68ull, 46ull, 1ull}},
  {'E', {69ull, 1ull, 69ull, 10ull, 0ull, 69ull, 53ull, 1ull}},
  {'E', {70ull, 1ull, 70ull, 10ull, 0ull, 70ull, 60ull, 1ull}},
  {'E', {71ull, 1ull, 71ull, 10ull, 0ull, 71ull, 67ull, 1ull}},
  {'E', {72ull, 1ull, 72ull, 10ull, 0ull, 72ull, 24ull, 1ull}},
  {'E', {73ull, 1ull, 73ull, 10ull, 0ull, 73ull, 31ull, 1ull}},
  {'E', {74ull, 1ull, 74ull, 10ull, 0ull, 74ull, 38ull, 1ull}},
  {'E', {75ull, 1ull, 75ull, 10ull, 0ull, 75ull, 45ull, 1ull}},
  {'E', {76ull, 1ull, 76ull, 10ull, 0ull, 76ull, 52ull, 1ull}},
  {'E', {77ull, 1ull, 77ull, 10ull, 0ull, 77ull, 59ull, 1ull}},
  {'E', {78ull, 1ull, 78ull, 10ull, 0ull, 78ull, 66ull, 1ull}},
  {'E', {79ull, 1ull, 79ull, 10ull, 0ull, 79ull, 23ull, 1ull}},
  {'E', {80ull, 1ull, 80ull, 10ull, 0ull, 80ull, 30ull, 1ull}},
  {'E', {81ull, 1ull, 81ull, 10ull, 0ull, 81ull, 37ull, 1ull}},
  {'E', {82ull, 1ull, 82ull, 10ull, 0ull, 82ull, 44ull, 1ull}},
  {'E', {83ull, 1ull, 83ull, 10ull, 0ull, 83ull, 51ull, 1ull}},
  {'E', {84ull, 1ull, 84ull, 10ull, 0ull, 84ull, 58ull, 1ull}},
  {'E', {85ull, 1ull, 85ull, 10ull, 0ull, 85ull, 65ull, 1ull}},
  {'E', {86ull, 1ull, 86ull, 10ull, 0ull, 86ull, 22ull, 1ull}},
  {'E', {87ull, 1ull, 87ull, 10ull, 0ull, 87ull, 29ull, 1ull}},
  {'E', {88ull, 1ull, 88ull, 10ull, 0ull, 88ull, 36ull, 1ull}},
  {'E', {89ull, 1ull, 89ull, 10ull, 0ull, 89ull, 43ull, 1ull}},
  {'E', {90ull, 1ull, 90ull, 10ull, 0ull, 90ull, 50ull, 1ull}},
  {'E', {91ull, 1ull, 91ull, 10ull, 0ull, 91ull, 57ull, 1ull}},
  {'E', {92ull, 1ull, 92ull, 10ull, 0ull, 92ull, 64ull, 1ull}},
  {'E', {93ull, 1ull, 93ull, 10ull, 0ull, 93ull, 21ull, 1ull}},
  {'E', {94ull, 1ull, 94ull, 10ull, 0ull, 94ull, 28ull, 1ull}},
  {'E', {95ull, 1ull, 95ull, 10ull, 0ull, 95ull, 35ull, 1ull}},
  {'E', {96ull, 1ull, 96ull, 10ull, 0ull, 96ull, 42ull, 1ull}},
  {'E', {97ull, 1ull, 97ull, 10ull, 0ull, 97ull, 49ull, 1ull}},
  {'E', {98ull, 1ull, 98ull, 10ull, 0ull, 98ull, 56ull, 1ull}},
  {'E', {99ull, 1ull, 99ull, 10ull, 0ull, 99ull, 63ull, 1ull}},
  {'E', {100ull, 1ull, 100ull, 10ull, 0ull, 100ull, 20ull, 1ull}},
  {'E', {101ull, 1ull, 101ull, 10ull, 0ull, 101ull, 27ull, 1ull}},
  {'E', {102ull, 1ull, 102ull, 10ull, 0ull, 102ull, 34ull, 1ull}},
  {'E', {103ull, 1ull, 103ull, 10ull, 0ull, 103ull, 41ull, 1ull}},
  {'E', {104ull, 1ull, 104ull, 10ull, 0ull, 104ull, 48ull, 1ull}},
  {'E', {105ull, 1ull, 105ull, 10ull, 0ull, 105ull, 55ull, 1ull}},
  {'E', {106ull, 1ull, 106ull, 10ull, 0ull, 106ull, 62ull, 1ull}},
  {'E', {107ull, 1ull, 107ull, 10ull, 0ull, 107ull, 69ull, 1ull}},
  {'E', {108ull, 1ull, 108ull, 10ull, 0ull, 108ull, 26ull, 1ull}},
  {'E', {109ull, 1ull, 109ull, 10ull, 0ull, 109ull, 33ull, 1ull}},
  {'E', {110ull, 1ull, 110ull, 10ull, 0ull, 110ull, 40ull, 1ull}},
  {'E', {111ull, 1ull, 111ull, 10ull, 0ull, 111ull, 47ull, 1ull}},
  {'E', {112ull, 1ull, 112ull, 10ull, 0ull, 112ull, 54ull, 1ull}},
  {'E', {113ull, 1ull, 113ull, 10ull, 0ull, 113ull, 61ull, 1ull}},
  {'E', {114ull, 1ull, 114ull, 10ull, 0ull, 114ull, 68ull, 1ull}},
  {'E', {115ull, 1ull, 115ull, 10ull, 0ull, 115ull, 25ull, 1ull}},
  {'E', {116ull, 1ull, 116ull, 10ull, 0ull, 116ull, 32ull, 1ull}},
  {'E', {117ull, 1ull, 117ull, 10ull, 0ull, 117ull, 39ull, 1ull}},
  {'E', {118ull, 1ull, 118ull, 10ull, 0ull, 118ull, 46ull, 1ull}},
  {'E', {119ull, 1ull, 119ull, 10ull, 0ull, 119ull, 53ull, 1ull}},
  {'E', {120ull, 1ull, 120ull, 10ull, 0ull, 120ull, 60ull, 1ull}},
  {'E', {121ull, 1ull, 121ull, 10ull, 0ull, 121ull, 67ull, 1ull}},
  {'E', {122ull, 1ull, 122ull, 10ull, 0ull, 122ull, 24ull, 1ull}},
  {'E', {123ull, 1ull, 123ull, 10ull, 0ull, 123ull, 31ull, 1ull}},
  {'E', {124ull, 1ull, 124ull, 10ull, 0ull, 124ull, 38ull, 1ull}},
  {'E', {125ull, 1ull, 125ull, 10ull, 0ull, 125ull, 45ull, 1ull}},
  {'E', {126ull, 1ull, 126ull, 10ull, 0ull, 126ull, 52ull, 1ull}},
  {'E', {127ull, 1ull, 127ull, 10ull, 0ull, 127ull, 59ull, 1ull}},
  {'N', {128ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 3ull, 3ull, 3ull, 3ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 4ull, 3ull, 3ull, 4ull, 0ull, 0ull, 0ull}},
  {'N', {127ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {3ull, 5ull, 3ull, 3ull, 5ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_inc_slot0_unused = {70133486216692149ull, 6209112994536975860ull, 128, 3ull, 0ull, 0ull, 2ull, 3};
const ProgRef kProgs[] = {{kProg_lru, sizeof(kProg_lru) / sizeof(kProg_lru[0]), kExp_lru}, {kProg_invalid, sizeof(kProg_invalid) / sizeof(kProg_invalid[0]), kExp_invalid}, {kProg_fields, sizeof(kProg_fields) / sizeof(kProg_fields[0]), kExp_fields}, {kProg_clear, sizeof(kProg_clear) / sizeof(kProg_clear[0]), kExp_clear}, {kProg_tick_sat, sizeof(kProg_tick_sat) / sizeof(kProg_tick_sat[0]), kExp_tick_sat}, {kProg_tick_full, sizeof(kProg_tick_full) / sizeof(kProg_tick_full[0]), kExp_tick_full}, {kProg_counters, sizeof(kProg_counters) / sizeof(kProg_counters[0]), kExp_counters}, {kProg_inc_count_low, sizeof(kProg_inc_count_low) / sizeof(kProg_inc_count_low[0]), kExp_inc_count_low}, {kProg_inc_count_high, sizeof(kProg_inc_count_high) / sizeof(kProg_inc_count_high[0]), kExp_inc_count_high}, {kProg_inc_none_used, sizeof(kProg_inc_none_used) / sizeof(kProg_inc_none_used[0]), kExp_inc_none_used}, {kProg_inc_count_low_free, sizeof(kProg_inc_count_low_free) / sizeof(kProg_inc_count_low_free[0]), kExp_inc_count_low_free}, {kProg_inc_unused_flag, sizeof(kProg_inc_unused_flag) / sizeof(kProg_inc_unused_flag[0]), kExp_inc_unused_flag}, {kProg_inc_ties, sizeof(kProg_inc_ties) / sizeof(kProg_inc_ties[0]), kExp_inc_ties}, {kProg_inc_slot0_unused, sizeof(kProg_inc_slot0_unused) / sizeof(kProg_inc_slot0_unused[0]), kExp_inc_slot0_unused}};
const Expect kCorpus_c1 = {17866905944349978769ull, 15517674089495522561ull, 42, 52ull, 2ull, 34ull, 0ull, 8785};
const Expect kCorpus_c2 = {6450652320051237325ull, 17697061432546082423ull, 128, 1579ull, 574ull, 113ull, 22ull, 8705};
const Expect kCorpus_c3 = {3120457538346808111ull, 13903639035647198843ull, 54, 1318ull, 498ull, 24ull, 0ull, 4335};
const u64 kSmallDigest = 11447887134245552807ull;
const u64 kBasisDigest = 14695981039346656037ull;  // a null cache hashes nothing
const u64 kEmptyDigest = 9354609568656401157ull;

}  // namespace

TEST_CASE("glyphcache: scenario programs against the reference", "[glyphcache]") {
  int pi = 0;
  for (const ProgRef& p : kProgs) {
    INFO("program " << pi++);
    Replay r;
    r.c = GlyphCache{};
    for (std::size_t i = 0; i < p.n; ++i) r.apply(p.ops[i]);
    r.check(p.exp);
  }
}

TEST_CASE("glyphcache: seeded random corpora", "[glyphcache]") {
  struct Cfg {
    u64 seed;
    u32 universe, nops;
    const Expect* exp;
  };
  const Cfg cfgs[3] = {{0xC1, 200, 6000, &kCorpus_c1}, {0xC2, 130, 6000, &kCorpus_c2}, {0xC3, 40, 3000, &kCorpus_c3}};
  for (const Cfg& cfg : cfgs) {
    Lcg rng(cfg.seed);
    Replay r;
    r.c = GlyphCache{};
    for (u32 k = 0; k < cfg.nops; ++k) {
      const u32 x = rng.rng(0, 999);
      const u32 u = rng.rng(0, cfg.universe - 1);
      const GlyphKey kk = key(u % 3, u / 3, 1 + (u % 5), u % 4);
      if (x < 550) {
        r.ins(kk, rng.rng(0, 1000000));
      } else if (x < 920) {
        r.find(kk);
      } else if (x < 960) {
        const u32 sz = rng.rng(0, 1);
        const u32 sb = rng.rng(3, 5);
        r.ins(key(kk.font_id, kk.glyph, sz, sb), 1);
      } else if (x < 999) {
        r.find(key(kk.font_id, kk.glyph, kk.size_px, 4));
      } else {
        REQUIRE(cache_clear(&r.c));
      }
      if (k % 64 == 63) {
        const u64 d = cache_digest(&r.c);
        r.res(static_cast<u32>(d & 0xFFFFFFFFu));
        r.res(static_cast<u32>(d >> 32));
        r.res(r.c.count);
      }
    }
    r.check(*cfg.exp);
  }
}

TEST_CASE("glyphcache: subpixel_bin", "[glyphcache]") {
  REQUIRE(subpixel_bin(0) == 0);
  REQUIRE(subpixel_bin(63) == 0);
  REQUIRE(subpixel_bin(64) == 1);
  REQUIRE(subpixel_bin(127) == 1);
  REQUIRE(subpixel_bin(128) == 2);
  REQUIRE(subpixel_bin(191) == 2);
  REQUIRE(subpixel_bin(192) == 3);
  REQUIRE(subpixel_bin(255) == 3);
  REQUIRE(subpixel_bin(256) == 0);
  REQUIRE(subpixel_bin(256 + 64) == 1);
  REQUIRE(subpixel_bin(-1) == 3);
  REQUIRE(subpixel_bin(-64) == 3);
  REQUIRE(subpixel_bin(-65) == 2);
  REQUIRE(subpixel_bin(-128) == 2);
  REQUIRE(subpixel_bin(-129) == 1);
  REQUIRE(subpixel_bin(-192) == 1);
  REQUIRE(subpixel_bin(-193) == 0);
  REQUIRE(subpixel_bin(-256) == 0);
  REQUIRE(subpixel_bin(INT32_MAX) == 3);
  REQUIRE(subpixel_bin(INT32_MIN) == 0);
  REQUIRE(subpixel_bin(INT32_MIN + 64) == 1);
  for (std::int32_t v = -1024; v <= 1024; ++v) {
    const std::int32_t m = ((v % 256) + 256) % 256;
    REQUIRE(subpixel_bin(v) == m / 64);
  }
}

TEST_CASE("glyphcache: null arguments and invalid keys change nothing", "[glyphcache]") {
  GlyphCache c;
  u32 v = 5;
  REQUIRE_FALSE(cache_clear(nullptr));
  REQUIRE_FALSE(cache_find(nullptr, key(1, 1, 1, 0), &v));
  REQUIRE(cache_insert(nullptr, key(1, 1, 1, 0), 1) == InsertResult::NullCache);
  REQUIRE(cache_insert(nullptr, key(1, 1, 0, 0), 1) == InsertResult::NullCache);  // the null cache is reported before the key
  REQUIRE(cache_size(nullptr) == 0);
  REQUIRE(cache_digest(nullptr) == kBasisDigest);
  REQUIRE(cache_insert(&c, key(1, 1, 1, 0), 1) == InsertResult::Inserted);
  const u64 before = cache_digest(&c);
  REQUIRE_FALSE(cache_find(&c, key(1, 1, 1, 0), nullptr));  // a null value pointer: no hit counted
  REQUIRE_FALSE(cache_find(&c, key(1, 1, 0, 0), &v));
  REQUIRE_FALSE(cache_find(&c, key(1, 1, 1, 4), &v));
  REQUIRE(cache_insert(&c, key(1, 1, 0, 0), 2) == InsertResult::BadKey);
  REQUIRE(cache_insert(&c, key(1, 1, 1, 4), 2) == InsertResult::BadKey);
  REQUIRE(cache_insert(&c, key(1, 1, 1, 255), 2) == InsertResult::BadKey);
  REQUIRE(v == 5);
  REQUIRE(cache_digest(&c) == before);
  REQUIRE(c.tick == 1);
  REQUIRE(c.misses == 0);
  REQUIRE(c.hits == 0);
  // the valid extremes of a key
  REQUIRE(cache_insert(&c, key(0xFFFFFFFFu, 0xFFFF, 0xFFFF, 3), 0xFFFFFFFFu) == InsertResult::Inserted);
  REQUIRE(cache_find(&c, key(0xFFFFFFFFu, 0xFFFF, 0xFFFF, 3), &v));
  REQUIRE(v == 0xFFFFFFFFu);
  REQUIRE(cache_insert(&c, key(0, 0, 1, 0), 0) == InsertResult::Inserted);
  REQUIRE(cache_find(&c, key(0, 0, 1, 0), &v));
  REQUIRE(v == 0);
}

TEST_CASE("glyphcache: hand computed bookkeeping", "[glyphcache]") {
  GlyphCache c;
  u32 v = 0;
  REQUIRE(cache_size(&c) == 0);
  REQUIRE(cache_insert(&c, key(1, 2, 3, 1), 10) == InsertResult::Inserted);
  REQUIRE(c.tick == 1);
  REQUIRE(c.count == 1);
  REQUIRE(c.entries[0].used);
  REQUIRE(c.entries[0].last_used == 1);
  REQUIRE(cache_insert(&c, key(4, 5, 6, 3), 20) == InsertResult::Inserted);
  REQUIRE(c.entries[1].last_used == 2);
  REQUIRE(cache_find(&c, key(1, 2, 3, 1), &v));
  REQUIRE(v == 10);
  REQUIRE(c.tick == 3);
  REQUIRE(c.entries[0].last_used == 3);
  REQUIRE(c.entries[1].last_used == 2);
  REQUIRE(c.hits == 1);
  v = 99;
  REQUIRE_FALSE(cache_find(&c, key(9, 9, 9, 0), &v));
  REQUIRE(v == 99);
  REQUIRE(c.misses == 1);
  REQUIRE(c.tick == 3);  // a miss leaves the tick
  // Updated: new value, tick and last_used only
  const u32 count_before = c.count;
  REQUIRE(cache_insert(&c, key(4, 5, 6, 3), 21) == InsertResult::Updated);
  REQUIRE(c.entries[1].value == 21);
  REQUIRE(c.entries[1].last_used == 4);
  REQUIRE(c.tick == 4);
  REQUIRE(c.count == count_before);
  REQUIRE(c.hits == 1);
  REQUIRE(c.misses == 1);
  REQUIRE(c.evictions == 0);
  REQUIRE(cache_digest(&c) != kSmallDigest);
  // the small digest of the reference: two inserts, one hit, one miss (before the update above)
  GlyphCache d;
  cache_insert(&d, key(1, 2, 3, 1), 10);
  cache_insert(&d, key(4, 5, 6, 3), 20);
  cache_find(&d, key(1, 2, 3, 1), &v);
  cache_find(&d, key(9, 9, 9, 0), &v);
  REQUIRE(cache_digest(&d) == kSmallDigest);
  const GlyphCache fresh;
  REQUIRE(cache_digest(&fresh) == kEmptyDigest);
  // clear
  REQUIRE(cache_clear(&c));
  REQUIRE(c.tick == 0);
  REQUIRE(c.count == 0);
  REQUIRE(c.hits == 0);
  REQUIRE(c.misses == 0);
  REQUIRE(c.evictions == 0);
  REQUIRE(cache_digest(&c) == kEmptyDigest);
  for (u32 i = 0; i < kCapacity; ++i) {
    REQUIRE_FALSE(c.entries[i].used);
    REQUIRE(c.entries[i].value == 0);
    REQUIRE(c.entries[i].last_used == 0);
    REQUIRE(c.entries[i].key.size_px == 0);
  }
}

TEST_CASE("glyphcache: least recently used eviction", "[glyphcache]") {
  GlyphCache c;
  u32 v = 0;
  for (u32 i = 0; i < kCapacity; ++i) REQUIRE(cache_insert(&c, key(1, i, 10, 0), i) == InsertResult::Inserted);
  REQUIRE(cache_size(&c) == kCapacity);
  REQUIRE(c.tick == kCapacity);
  REQUIRE(c.entries[127].last_used == 128);
  // refresh entry 0 and 1: the oldest is now entry 2
  REQUIRE(cache_find(&c, key(1, 0, 10, 0), &v));
  REQUIRE(cache_find(&c, key(1, 1, 10, 0), &v));
  REQUIRE(cache_insert(&c, key(2, 0, 10, 0), 500) == InsertResult::Evicted);
  REQUIRE(c.evictions == 1);
  REQUIRE(c.count == kCapacity);
  REQUIRE(c.entries[2].key.font_id == 2);
  REQUIRE(c.entries[2].value == 500);
  REQUIRE(c.entries[2].last_used == 131);
  REQUIRE_FALSE(cache_find(&c, key(1, 2, 10, 0), &v));
  REQUIRE(cache_find(&c, key(1, 0, 10, 0), &v));
  REQUIRE(cache_find(&c, key(1, 3, 10, 0), &v));
  // the next oldest is entry 4 (entry 3 was just refreshed)
  REQUIRE(cache_insert(&c, key(2, 1, 10, 0), 501) == InsertResult::Evicted);
  REQUIRE(c.entries[4].key.font_id == 2);
  REQUIRE(c.evictions == 2);
  // an update on a full cache does not evict
  REQUIRE(cache_insert(&c, key(2, 1, 10, 0), 502) == InsertResult::Updated);
  REQUIRE(c.evictions == 2);
  REQUIRE(c.entries[4].value == 502);
  // each of the four key fields distinguishes entries
  GlyphCache e;
  REQUIRE(cache_insert(&e, key(1, 1, 1, 0), 1) == InsertResult::Inserted);
  REQUIRE(cache_insert(&e, key(2, 1, 1, 0), 2) == InsertResult::Inserted);
  REQUIRE(cache_insert(&e, key(1, 2, 1, 0), 3) == InsertResult::Inserted);
  REQUIRE(cache_insert(&e, key(1, 1, 2, 0), 4) == InsertResult::Inserted);
  REQUIRE(cache_insert(&e, key(1, 1, 1, 1), 5) == InsertResult::Inserted);
  REQUIRE(e.count == 5);
  const u32 want[5] = {1, 2, 3, 4, 5};
  const GlyphKey ks[5] = {key(1, 1, 1, 0), key(2, 1, 1, 0), key(1, 2, 1, 0), key(1, 1, 2, 0), key(1, 1, 1, 1)};
  for (int i = 0; i < 5; ++i) {
    REQUIRE(cache_find(&e, ks[i], &v));
    REQUIRE(v == want[i]);
  }
}

TEST_CASE("glyphcache: saturation of tick and counters", "[glyphcache]") {
  GlyphCache c;
  u32 v = 0;
  c.tick = kMaxU64 - 1;
  REQUIRE(cache_insert(&c, key(1, 1, 1, 0), 1) == InsertResult::Inserted);
  REQUIRE(c.tick == kMaxU64);
  REQUIRE(c.entries[0].last_used == kMaxU64);
  REQUIRE(cache_insert(&c, key(1, 2, 1, 0), 2) == InsertResult::Inserted);
  REQUIRE(c.tick == kMaxU64);
  REQUIRE(c.entries[1].last_used == kMaxU64);
  REQUIRE(cache_find(&c, key(1, 1, 1, 0), &v));
  REQUIRE(c.tick == kMaxU64);
  REQUIRE(cache_insert(&c, key(1, 1, 1, 0), 9) == InsertResult::Updated);
  REQUIRE(c.tick == kMaxU64);
  c.hits = kMaxU64;
  c.misses = kMaxU64;
  c.evictions = kMaxU64;
  REQUIRE(cache_find(&c, key(1, 1, 1, 0), &v));
  REQUIRE_FALSE(cache_find(&c, key(7, 7, 7, 0), &v));
  REQUIRE(c.hits == kMaxU64);
  REQUIRE(c.misses == kMaxU64);
  for (u32 i = 0; i < kCapacity; ++i) cache_insert(&c, key(3, i, 1, 0), i);
  REQUIRE(c.evictions == kMaxU64);
  c.hits = kMaxU64 - 1;
  REQUIRE(cache_find(&c, key(3, 127, 1, 0), &v));
  REQUIRE(c.hits == kMaxU64);
}

TEST_CASE("glyphcache: cache_size returns the stored count", "[glyphcache]") {
  GlyphCache c;
  REQUIRE(cache_size(&c) == 0);
  c.count = 500;  // hand built, not clamped
  REQUIRE(cache_size(&c) == 500);
  c.count = 0;
  c.entries[3].used = true;
  c.entries[4].used = true;
  REQUIRE(cache_size(&c) == 0);  // not recounted
  c.count = 7;
  REQUIRE(cache_size(&c) == 7);
  c.count = kCapacity;
  REQUIRE(cache_size(&c) == kCapacity);
}
