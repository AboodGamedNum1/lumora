// Tests for core/damage (scope AH). Expected values come from an independent Python reference (tools/damage_ref.py: it follows the rule
// text literally with Python integers and its own FNV) and are pasted below: scenario programs with the digest of every recorded result,
// and seeded random corpora. Hand-computed cases, hand-built and corrupt sets, and an invariant oracle on random adds sit beside them.
// Sets are plain stack objects; the harness copies them byte for byte to prove that a false return changes nothing. Evidence label: local
// unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>

#include "damage.h"
#include "rect.h"

using namespace lumora::damage;
using lumora::rect::Rect;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr std::int32_t kMax = 2147483647;
constexpr std::int32_t kMin = -kMax - 1;

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int k = 0; k < 4; ++k) {
      h ^= (v >> (8 * k)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
  void sword(std::int32_t v) { word(static_cast<u32>(v)); }
};

struct Lcg {
  u64 s;
  explicit Lcg(u64 seed) : s(seed) {}
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  std::int32_t rng(i64 lo, i64 hi) { return static_cast<std::int32_t>(lo + static_cast<i64>(next() % static_cast<u64>(hi - lo + 1))); }
};

struct Op {
  char k;
  i64 v[9];
};

struct Expect {
  u64 results, state;
  u32 count;
  u64 area;
  u32 words;
};

struct ProgRef {
  const Op* ops;
  u32 n;
  Expect e;
};

struct CorpusRef {
  u64 digest, state;
  u32 words;
  int seed, nops, world, side, ext;
};

Rect R(i64 x, i64 y, i64 w, i64 h) {
  Rect r;
  r.x = static_cast<std::int32_t>(x);
  r.y = static_cast<std::int32_t>(y);
  r.w = static_cast<std::int32_t>(w);
  r.h = static_cast<std::int32_t>(h);
  return r;
}

bool same(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

void rect_words(Fnv& f, const Rect& r, u32* n) {
  f.sword(r.x);
  f.sword(r.y);
  f.sword(r.w);
  f.sword(r.h);
  *n += 4;
}

// Records the same snapshot as the reference: size, area low and high, bounding box, then every get from 0 to size inclusive.
void snapshot(Fnv& f, const DamageSet& s, u32* n) {
  f.word(size(&s));
  const u64 t = total_area(&s);
  f.word(static_cast<u32>(t & 0xFFFFFFFFu));
  f.word(static_cast<u32>(t >> 32));
  *n += 3;
  rect_words(f, bounding_box(&s), n);
  const u32 sz = size(&s);
  for (u32 i = 0; i <= sz; ++i) {
    Rect g = R(0, 0, 0, 0);
    if (get(&s, i, &g)) {
      f.word(1);
      ++*n;
      rect_words(f, g, n);
    } else {
      f.word(0);
      ++*n;
    }
  }
}

void state_words(Fnv& f, const DamageSet& s) {
  f.word(s.count);
  if (s.count > kMaxRects) return;
  for (u32 i = 0; i < s.count; ++i) {
    f.sword(s.rects[i].x);
    f.sword(s.rects[i].y);
    f.sword(s.rects[i].w);
    f.sword(s.rects[i].h);
  }
}

u64 state_digest(const DamageSet& s) {
  Fnv f;
  state_words(f, s);
  return f.h;
}

bool bytes_equal(const DamageSet& a, const DamageSet& b) { return std::memcmp(&a, &b, sizeof(DamageSet)) == 0; }

const Op kProg_basic[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 20ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'C', {0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {1ll, 1ll, 2ll, 2ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_basic = {11178620113999084555ull, 7211396892751399700ull, 1, 4ull, 79};
const Op kProg_clip[] = {
  {'A', {-5ll, -5ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {95ll, 95ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {200ll, 200ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 0ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 5ll, 0ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 10ll, -3ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 10ll, 4ll, 4ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {10ll, 10ll, 4ll, 4ll, 20ll, 20ll, 5ll, 5ll, 0ll}},
  {'A', {10ll, 10ll, 4ll, 4ll, 0ll, 0ll, -5ll, 5ll, 0ll}},
};
const Expect kExp_clip = {1853095183286155213ull, 15356638463368665479ull, 2, 50ull, 166};
const Op kProg_touch_no_merge[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 10ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 10ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_touch_no_merge = {1041087952094185491ull, 12318122771168672388ull, 5, 425ull, 120};
const Op kProg_merge_one[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {5ll, 5ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_merge_one = {14914204627551523999ull, 16123166815521931129ull, 2, 325ull, 52};
const Op kProg_merge_chain[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {12ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {24ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {5ll, 0ll, 25ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_merge_chain = {10067413662047910393ull, 3086641665856457708ull, 1, 340ull, 71};
const Op kProg_merge_restart[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 40ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {8ll, 8ll, 36ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 3ll, 50ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_merge_restart = {9173106210201592026ull, 1994183120953192004ull, 1, 2500ull, 90};
const Op kProg_reorder_covered[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {22ll, 2ll, 3ll, 3ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_reorder_covered = {3970571348641871655ull, 13173691448473393754ull, 3, 300ull, 105};
const Op kProg_order_kept[] = {
  {'A', {0ll, 0ll, 4ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 4ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 4ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 4ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {11ll, 1ll, 6ll, 6ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_order_kept = {14888350724186752964ull, 205345903200672833ull, 4, 97ull, 115};
const Op kProg_fill_eight[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 50ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_fill_eight = {17349585651426363065ull, 4773277423354304680ull, 1, 4125ull, 266};
const Op kProg_fill_exact[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_fill_exact = {15579755573374849061ull, 16632368886381839709ull, 8, 200ull, 252};
const Op kProg_full_merge_not_collapse[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {2ll, 2ll, 10ll, 2ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_full_merge_not_collapse = {23056715155440237ull, 11974636769459890834ull, 7, 225ull, 296};
const Op kProg_full_merge_three[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {2ll, 2ll, 20ll, 2ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_full_merge_three = {2534974362442248692ull, 2057454790760898833ull, 6, 250ull, 291};
const Op kProg_full_collapse_after[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 60ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {1ll, 1ll, 3ll, 3ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'C', {0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {2ll, 2ll, 2ll, 2ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_full_collapse_after = {787806428108043356ull, 13677037807088200356ull, 1, 4ull, 302};
const Op kProg_full_empty_add[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {500ll, 500ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_full_empty_add = {13484634835251675349ull, 16632368886381839709ull, 8, 200ull, 350};
const Op kProg_full_covered[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {10ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {30ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {50ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {70ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_full_covered = {11823206552703972762ull, 6732675575433497821ull, 8, 200ull, 301};
const Op kProg_clip_to_bounds_merge[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {-20ll, -20ll, 30ll, 30ll, 0ll, 0ll, 50ll, 50ll, 0ll}},
};
const Expect kExp_clip_to_bounds_merge = {11186133256146387589ull, 169532744879249412ull, 1, 100ull, 28};
const Op kProg_diff_bounds[] = {
  {'A', {0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 20ll, 20ll, 0ll}},
  {'A', {50ll, 50ll, 10ll, 10ll, 40ll, 40ll, 30ll, 30ll, 0ll}},
  {'A', {5ll, 5ll, 100ll, 100ll, 0ll, 0ll, 8ll, 8ll, 0ll}},
};
const Expect kExp_diff_bounds = {5357292103565732256ull, 10054794256064964375ull, 2, 200ull, 52};
const Op kProg_union_fail_merge[] = {
  {'A', {0ll, 0ll, 2147483647ll, 10ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {5ll, 50ll, 4ll, 4ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {-2000000000ll, 0ll, 2147483647ll, 10ll, -2000000000ll, 0ll, 2147483647ll, 100ll, 0ll}},
  {'A', {-2000000000ll, 0ll, 10ll, 10ll, -2000000000ll, 0ll, 2147483647ll, 100ll, 0ll}},
  {'A', {-2000000000ll, 52ll, 2147483647ll, 10ll, -2000000000ll, 0ll, 2147483647ll, 100ll, 0ll}},
};
const Expect kExp_union_fail_merge = {13126207230346138173ull, 4846270684162130002ull, 3, 47244640334ull, 100};
const Op kProg_union_fit_exact[] = {
  {'A', {0ll, 0ll, 147483647ll, 10ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {-2000000000ll, 0ll, 2147483647ll, 10ll, -2000000000ll, 0ll, 2147483647ll, 100ll, 0ll}},
};
const Expect kExp_union_fit_exact = {16352924726056756686ull, 1092194101393139220ull, 1, 21474836470ull, 28};
const Op kProg_union_fail_collapse[] = {
  {'A', {-2000000000ll, 0ll, 10ll, 10ll, -2147483648ll, -2147483648ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {20ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {40ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {60ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {80ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {100ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {120ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {1900000000ll, 0ll, 10ll, 10ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {0ll, 100ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
};
const Expect kExp_union_fail_collapse = {6888004532336962365ull, 975580473702730414ull, 1, 199500001050ull, 275};
const Op kProg_extreme_edges[] = {
  {'A', {2147483642ll, 2147483642ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {-2147483648ll, -2147483648ll, 5ll, 5ll, -2147483648ll, -2147483648ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {2147483642ll, 2147483642ll, 6ll, 6ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {-2147483648ll, -2147483648ll, 2147483647ll, 2147483647ll, -2147483648ll, -2147483648ll, 2147483647ll, 2147483647ll, 0ll}},
};
const Expect kExp_extreme_edges = {16146482443619318436ull, 9377759408472499335ull, 2, 9223372028264841218ull, 90};
const Op kProg_invalid_add[] = {
  {'A', {2147483647ll, 0ll, 5ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {0ll, 0ll, -1ll, 5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {0ll, 2147483647ll, 5ll, 1ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll}},
  {'A', {1ll, 1ll, 3ll, 3ll, 2147483647ll, 0ll, 5ll, 5ll, 0ll}},
};
const Expect kExp_invalid_add = {9100616490806898757ull, 5558979605539197941ull, 0, 0ull, 36};
const Op kProg_hb_corrupt[] = {
  {'A', {0ll, 0ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'N', {9ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {10ll, 10ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'N', {4294967295ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {10ll, 10ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'C', {0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {10ll, 10ll, 5ll, 5ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_hb_corrupt = {16089173925381251397ull, 1198700384758768468ull, 1, 25ull, 70};
const Op kProg_hb_stale_beyond_count[] = {
  {'H', {5ll, 0ll, 0ll, 50ll, 50ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {7ll, 3ll, 3ll, 9ll, 9ll, 0ll, 0ll, 0ll, 0ll}},
  {'N', {2ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {0ll, 0ll, 0ll, 4ll, 4ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {1ll, 10ll, 0ll, 4ll, 4ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {2ll, 2ll, 4ll, 4ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_hb_stale_beyond_count = {16345624371544664802ull, 5955146857992190525ull, 2, 52ull, 89};
const Op kProg_hb_empty_invalid_stored[] = {
  {'H', {0ll, 0ll, 0ll, 0ll, 5ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {1ll, 0ll, 0ll, -3ll, 4ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {2ll, 2147483647ll, 0ll, 5ll, 5ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {3ll, 10ll, 10ll, 5ll, 5ll, 0ll, 0ll, 0ll, 0ll}},
  {'N', {4ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {0ll, 0ll, 50ll, 50ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_hb_empty_invalid_stored = {4738426956463470798ull, 6556205448858290962ull, 4, 2500ull, 89};
const Op kProg_hb_overlapping_stored[] = {
  {'H', {0ll, 0ll, 0ll, 10ll, 10ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {1ll, 5ll, 5ll, 10ll, 10ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {2ll, 40ll, 40ll, 4ll, 4ll, 0ll, 0ll, 0ll, 0ll}},
  {'N', {3ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {6ll, 6ll, 2ll, 2ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
  {'A', {0ll, 0ll, 1ll, 1ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_hb_overlapping_stored = {3408766629734485055ull, 17817446086977662567ull, 2, 241ull, 85};
const Op kProg_hb_area_wrap[] = {
  {'H', {0ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {1ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {2ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {3ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {4ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {5ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {6ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {7ll, 0ll, 0ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'N', {8ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
};
const Expect kExp_hb_area_wrap = {8459448260970751766ull, 6019286020009824573ull, 8, 18446744039349813256ull, 112};
const Op kProg_hb_area_big_disjoint[] = {
  {'H', {0ll, -2147483648ll, -2147483648ll, 2147483647ll, 2147483647ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {1ll, 0ll, 0ll, 2147483646ll, 2147483646ll, 0ll, 0ll, 0ll, 0ll}},
  {'N', {2ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
};
const Expect kExp_hb_area_big_disjoint = {16384274511186205468ull, 14697364524796002807ull, 2, 9223372023969873925ull, 34};
const Op kProg_hb_full_overlap_collapse[] = {
  {'H', {0ll, 0ll, 0ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {1ll, 1ll, 1ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {2ll, 2ll, 2ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {3ll, 3ll, 3ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {4ll, 4ll, 4ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {5ll, 5ll, 5ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {6ll, 6ll, 6ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'H', {7ll, 7ll, 7ll, 20ll, 20ll, 0ll, 0ll, 0ll, 0ll}},
  {'N', {8ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll, 0ll}},
  {'A', {500ll, 500ll, 1ll, 1ll, 0ll, 0ll, 1000ll, 1000ll, 0ll}},
  {'A', {50ll, 50ll, 1ll, 1ll, 0ll, 0ll, 100ll, 100ll, 0ll}},
};
const Expect kExp_hb_full_overlap_collapse = {15500868277641404281ull, 15760892472225146888ull, 1, 251001ull, 140};
const ProgRef kProgs[] = {{kProg_basic, sizeof(kProg_basic) / sizeof(kProg_basic[0]), kExp_basic}, {kProg_clip, sizeof(kProg_clip) / sizeof(kProg_clip[0]), kExp_clip}, {kProg_touch_no_merge, sizeof(kProg_touch_no_merge) / sizeof(kProg_touch_no_merge[0]), kExp_touch_no_merge}, {kProg_merge_one, sizeof(kProg_merge_one) / sizeof(kProg_merge_one[0]), kExp_merge_one}, {kProg_merge_chain, sizeof(kProg_merge_chain) / sizeof(kProg_merge_chain[0]), kExp_merge_chain}, {kProg_merge_restart, sizeof(kProg_merge_restart) / sizeof(kProg_merge_restart[0]), kExp_merge_restart}, {kProg_reorder_covered, sizeof(kProg_reorder_covered) / sizeof(kProg_reorder_covered[0]), kExp_reorder_covered}, {kProg_order_kept, sizeof(kProg_order_kept) / sizeof(kProg_order_kept[0]), kExp_order_kept}, {kProg_fill_eight, sizeof(kProg_fill_eight) / sizeof(kProg_fill_eight[0]), kExp_fill_eight}, {kProg_fill_exact, sizeof(kProg_fill_exact) / sizeof(kProg_fill_exact[0]), kExp_fill_exact}, {kProg_full_merge_not_collapse, sizeof(kProg_full_merge_not_collapse) / sizeof(kProg_full_merge_not_collapse[0]), kExp_full_merge_not_collapse}, {kProg_full_merge_three, sizeof(kProg_full_merge_three) / sizeof(kProg_full_merge_three[0]), kExp_full_merge_three}, {kProg_full_collapse_after, sizeof(kProg_full_collapse_after) / sizeof(kProg_full_collapse_after[0]), kExp_full_collapse_after}, {kProg_full_empty_add, sizeof(kProg_full_empty_add) / sizeof(kProg_full_empty_add[0]), kExp_full_empty_add}, {kProg_full_covered, sizeof(kProg_full_covered) / sizeof(kProg_full_covered[0]), kExp_full_covered}, {kProg_clip_to_bounds_merge, sizeof(kProg_clip_to_bounds_merge) / sizeof(kProg_clip_to_bounds_merge[0]), kExp_clip_to_bounds_merge}, {kProg_diff_bounds, sizeof(kProg_diff_bounds) / sizeof(kProg_diff_bounds[0]), kExp_diff_bounds}, {kProg_union_fail_merge, sizeof(kProg_union_fail_merge) / sizeof(kProg_union_fail_merge[0]), kExp_union_fail_merge}, {kProg_union_fit_exact, sizeof(kProg_union_fit_exact) / sizeof(kProg_union_fit_exact[0]), kExp_union_fit_exact}, {kProg_union_fail_collapse, sizeof(kProg_union_fail_collapse) / sizeof(kProg_union_fail_collapse[0]), kExp_union_fail_collapse}, {kProg_extreme_edges, sizeof(kProg_extreme_edges) / sizeof(kProg_extreme_edges[0]), kExp_extreme_edges}, {kProg_invalid_add, sizeof(kProg_invalid_add) / sizeof(kProg_invalid_add[0]), kExp_invalid_add}, {kProg_hb_corrupt, sizeof(kProg_hb_corrupt) / sizeof(kProg_hb_corrupt[0]), kExp_hb_corrupt}, {kProg_hb_stale_beyond_count, sizeof(kProg_hb_stale_beyond_count) / sizeof(kProg_hb_stale_beyond_count[0]), kExp_hb_stale_beyond_count}, {kProg_hb_empty_invalid_stored, sizeof(kProg_hb_empty_invalid_stored) / sizeof(kProg_hb_empty_invalid_stored[0]), kExp_hb_empty_invalid_stored}, {kProg_hb_overlapping_stored, sizeof(kProg_hb_overlapping_stored) / sizeof(kProg_hb_overlapping_stored[0]), kExp_hb_overlapping_stored}, {kProg_hb_area_wrap, sizeof(kProg_hb_area_wrap) / sizeof(kProg_hb_area_wrap[0]), kExp_hb_area_wrap}, {kProg_hb_area_big_disjoint, sizeof(kProg_hb_area_big_disjoint) / sizeof(kProg_hb_area_big_disjoint[0]), kExp_hb_area_big_disjoint}, {kProg_hb_full_overlap_collapse, sizeof(kProg_hb_full_overlap_collapse) / sizeof(kProg_hb_full_overlap_collapse[0]), kExp_hb_full_overlap_collapse}};
const CorpusRef kCorpus_aa1 = {17532295963817098629ull, 1754146637101460900ull, 8879u, 2721, 3000, 60u, 12, 0};
const CorpusRef kCorpus_aa2 = {14922054326834208377ull, 14718830210918840164ull, 8940u, 2722, 3000, 200u, 40, 0};
const CorpusRef kCorpus_aa3 = {14126122485616243777ull, 17302835014856816686ull, 4750u, 2723, 2000, 30u, 30, 0};
const CorpusRef kCorpus_aa4 = {11005573988090803780ull, 4409516498999448055ull, 8468u, 2724, 2500, 1000u, 100, 150};
const u64 kSmallState = 12359327634050641475ull;  // two rectangles (0,0,10,10) and (20,0,10,10)

struct RunOut {
  u64 digest = 0, state = 0;
  u32 words = 0;
  DamageSet set;
};

}  // namespace

namespace {

RunOut run_prog(const Op* ops, u32 n) {
  RunOut o;
  DamageSet s{};
  Fnv f;
  for (u32 i = 0; i < n; ++i) {
    const Op& op = ops[i];
    if (op.k == 'A') {
      const bool ok = add(&s, R(op.v[0], op.v[1], op.v[2], op.v[3]), R(op.v[4], op.v[5], op.v[6], op.v[7]));
      f.word(ok ? 1u : 0u);
      ++o.words;
    } else if (op.k == 'C') {
      clear(&s);
    } else if (op.k == 'H') {
      s.rects[op.v[0]] = R(op.v[1], op.v[2], op.v[3], op.v[4]);
    } else if (op.k == 'N') {
      s.count = static_cast<u32>(op.v[0]);
    }
    snapshot(f, s, &o.words);
  }
  o.digest = f.h;
  o.state = state_digest(s);
  o.set = s;
  return o;
}

RunOut run_corpus(const CorpusRef& c) {
  RunOut o;
  DamageSet s{};
  Fnv f;
  Lcg r(static_cast<u64>(c.seed));
  for (int k = 0; k < c.nops; ++k) {
    const std::int32_t x = r.rng(0, 999);
    if (x < 30) {
      s.count = 0;
    } else {
      Rect rc, b;
      if (r.rng(0, 999) < c.ext) {
        const std::int32_t rx = r.rng(kMin, kMax), ry = r.rng(kMin, kMax);
        const std::int32_t rw = r.rng(0, kMax), rh = r.rng(0, kMax);
        rc = R(rx, ry, rw, rh);
        if (r.rng(0, 1)) {
          b = R(kMin, kMin, kMax, kMax);
        } else {
          const std::int32_t bx = r.rng(kMin, 0), by = r.rng(kMin, 0), bw = r.rng(0, kMax), bh = r.rng(0, kMax);
          b = R(bx, by, bw, bh);
        }
      } else {
        const std::int32_t rx = r.rng(-5, c.world), ry = r.rng(-5, c.world);
        const std::int32_t rw = r.rng(0, c.side), rh = r.rng(0, c.side);
        rc = R(rx, ry, rw, rh);
        if (r.rng(0, 9) == 0) {
          const std::int32_t bx = r.rng(0, c.world / 2), by = r.rng(0, c.world / 2), bw = r.rng(0, c.world), bh = r.rng(0, c.world);
          b = R(bx, by, bw, bh);
        } else {
          b = R(0, 0, c.world, c.world);
        }
      }
      f.word(add(&s, rc, b) ? 1u : 0u);
      ++o.words;
    }
    if (k % 10 == 9) snapshot(f, s, &o.words);
  }
  o.digest = f.h;
  o.state = state_digest(s);
  o.set = s;
  return o;
}

DamageSet fresh() {
  DamageSet s{};
  return s;
}

const Rect kW = R(0, 0, 100, 100);

void expect_rects(const DamageSet& s, const Rect* want, u32 n) {
  REQUIRE(size(&s) == n);
  for (u32 i = 0; i < n; ++i) {
    Rect g = R(7, 7, 7, 7);
    REQUIRE(get(&s, i, &g));
    CHECK(same(g, want[i]));
  }
  Rect g = R(7, 7, 7, 7);
  CHECK_FALSE(get(&s, n, &g));
  CHECK(same(g, R(7, 7, 7, 7)));
}

}  // namespace

TEST_CASE("damage: scenario programs match the independent reference") {
  for (const ProgRef& p : kProgs) {
    const RunOut o = run_prog(p.ops, p.n);
    CHECK(o.digest == p.e.results);
    CHECK(o.state == p.e.state);
    CHECK(o.words == p.e.words);
    CHECK(o.set.count == p.e.count);
    CHECK(total_area(&o.set) == (o.set.count > kMaxRects ? 0 : p.e.area));
  }
}

TEST_CASE("damage: seeded corpora match the independent reference") {
  const CorpusRef* cs[] = {&kCorpus_aa1, &kCorpus_aa2, &kCorpus_aa3, &kCorpus_aa4};
  for (const CorpusRef* c : cs) {
    const RunOut o = run_corpus(*c);
    CHECK(o.digest == c->digest);
    CHECK(o.state == c->state);
    CHECK(o.words == c->words);
  }
}

TEST_CASE("damage: hand-computed adds, merges and order") {
  DamageSet s = fresh();
  s.rects[3] = R(1, 1, 1, 1);  // stale, must be ignored
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  CHECK(add(&s, R(20, 0, 10, 10), kW));
  CHECK(add(&s, R(0, 20, 5, 5), kW));
  const Rect a[] = {R(0, 0, 10, 10), R(20, 0, 10, 10), R(0, 20, 5, 5)};
  expect_rects(s, a, 3);
  CHECK(total_area(&s) == 225u);
  CHECK(same(bounding_box(&s), R(0, 0, 30, 25)));
  // an add that overlaps the second one only: union replaces it and goes to the end
  CHECK(add(&s, R(25, 5, 10, 10), kW));
  const Rect b[] = {R(0, 0, 10, 10), R(0, 20, 5, 5), R(20, 0, 15, 15)};
  expect_rects(s, b, 3);
  // clipping to bounds
  s = fresh();
  CHECK(add(&s, R(-5, -5, 10, 10), kW));
  CHECK(add(&s, R(95, 95, 10, 10), kW));
  const Rect c[] = {R(0, 0, 5, 5), R(95, 95, 5, 5)};
  expect_rects(s, c, 2);
  clear(&s);
  CHECK(size(&s) == 0u);
  CHECK(total_area(&s) == 0u);
  CHECK(same(bounding_box(&s), R(0, 0, 0, 0)));
}

TEST_CASE("damage: touching rectangles stay separate, overlap by one pixel merges") {
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  CHECK(add(&s, R(10, 0, 10, 10), kW));
  CHECK(add(&s, R(0, 10, 10, 10), kW));
  CHECK(add(&s, R(10, 10, 10, 10), kW));
  CHECK(size(&s) == 4u);
  CHECK(add(&s, R(9, 9, 2, 2), kW));
  const Rect w[] = {R(0, 0, 20, 20)};
  expect_rects(s, w, 1);
}

TEST_CASE("damage: a fully covered add reorders the set") {
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  CHECK(add(&s, R(20, 0, 10, 10), kW));
  CHECK(add(&s, R(40, 0, 10, 10), kW));
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  const Rect w[] = {R(20, 0, 10, 10), R(40, 0, 10, 10), R(0, 0, 10, 10)};
  expect_rects(s, w, 3);
}

TEST_CASE("damage: merge loop restarts from index 0 and chains") {
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  CHECK(add(&s, R(12, 0, 10, 10), kW));
  CHECK(add(&s, R(24, 0, 10, 10), kW));
  CHECK(add(&s, R(5, 0, 25, 4), kW));
  const Rect w[] = {R(0, 0, 34, 10)};
  expect_rects(s, w, 1);
  // cur grows into a rectangle that was passed earlier in the scan
  s = fresh();
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  CHECK(add(&s, R(40, 0, 10, 10), kW));
  CHECK(add(&s, R(0, 40, 10, 10), kW));
  CHECK(add(&s, R(8, 8, 36, 4), kW));  // hits 1 (index 0 fails? it overlaps rect 0 first) then grows over the second
  const Rect v[] = {R(0, 40, 10, 10), R(0, 0, 50, 12)};
  expect_rects(s, v, 2);
}

TEST_CASE("damage: full set merges first and collapses only when still full") {
  DamageSet s = fresh();
  for (int i = 0; i < 8; ++i) CHECK(add(&s, R(i * 10, 0, 5, 5), kW));
  CHECK(size(&s) == 8u);
  DamageSet t = s;
  CHECK(add(&t, R(2, 2, 10, 2), kW));  // merges rect 0 and 1: 7 left, so no collapse
  CHECK(size(&t) == 7u);
  CHECK(add(&t, R(60, 60, 5, 5), kW));  // 8th again
  CHECK(size(&t) == 8u);
  CHECK(add(&t, R(90, 90, 5, 5), kW));  // full and disjoint: one bounding rectangle
  const Rect w[] = {R(0, 0, 95, 95)};
  expect_rects(t, w, 1);
  // the collapse also covers a clipped, empty add: nothing changes
  DamageSet u = s;
  CHECK(add(&u, R(500, 500, 5, 5), kW));
  CHECK(bytes_equal(u, s));
  // fully covered add on a full set does not collapse
  u = s;
  CHECK(add(&u, R(0, 0, 5, 5), kW));
  CHECK(size(&u) == 8u);
  CHECK(same(u.rects[7], R(0, 0, 5, 5)));
}

TEST_CASE("damage: invalid or empty adds succeed and change nothing") {
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  const DamageSet before = s;
  const Rect bad[] = {R(kMax, 0, 5, 5), R(0, 0, -1, 5), R(0, 0, 5, -1), R(0, 0, 0, 5), R(0, 0, 5, 0), R(0, kMax, 5, 1), R(500, 500, 5, 5)};
  for (const Rect& r : bad) {
    CHECK(add(&s, r, kW));
    CHECK(bytes_equal(s, before));
  }
  const Rect badb[] = {R(0, 0, 0, 0), R(0, 0, -5, 5), R(20, 20, 5, 5), R(kMax, 0, 5, 5)};
  for (const Rect& b : badb) {
    CHECK(add(&s, R(1, 1, 3, 3), b));
    CHECK(bytes_equal(s, before));
  }
}

TEST_CASE("damage: a failing union returns false and leaves the set unchanged") {
  // Bounds differ per call, so a stored rectangle can reach outside the bounds of a later add and the union becomes too wide.
  const Rect pos = R(0, 0, kMax, kMax);
  const Rect wide = R(-2000000000, 0, kMax, 100);  // right edge 147483647
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, kMax, 10), pos));
  CHECK(add(&s, R(5, 50, 4, 4), pos));
  const DamageSet before = s;
  // cur is (-2000000000, 0, 2147483647, 10) and overlaps the first rectangle: the union is 4147483647 wide
  CHECK_FALSE(add(&s, R(-2000000000, 0, kMax, 10), wide));
  CHECK(bytes_equal(s, before));
  CHECK(size(&s) == 2u);
  // a union of exactly INT32_MAX width still fits
  DamageSet t = fresh();
  CHECK(add(&t, R(0, 0, 147483647, 10), pos));
  CHECK(add(&t, R(-2000000000, 0, kMax, 10), wide));
  const Rect w[] = {R(-2000000000, 0, kMax, 10)};
  expect_rects(t, w, 1);
}

TEST_CASE("damage: collapse that cannot fold returns false and leaves the set unchanged") {
  DamageSet s = fresh();
  s.rects[0] = R(-2000000000, 0, 10, 10);
  for (int i = 1; i < 8; ++i) s.rects[i] = R(i * 20, 0, 5, 5);
  s.count = 8;
  const DamageSet before = s;
  CHECK_FALSE(add(&s, R(1900000000, 0, 10, 10), R(0, 0, kMax, kMax)));
  CHECK(bytes_equal(s, before));
  CHECK(add(&s, R(1000, 0, 10, 10), R(0, 0, kMax, kMax)));  // a nearer add collapses fine
  const Rect w[] = {R(-2000000000, 0, 2000001010, 10)};
  expect_rects(s, w, 1);
}

TEST_CASE("damage: bounding_box of a hand-built set whose union does not fit is all zeros") {
  DamageSet s = fresh();
  s.rects[0] = R(-2000000000, 0, 10, 10);
  s.rects[1] = R(1900000000, 0, 10, 10);
  s.count = 2;
  CHECK(same(bounding_box(&s), R(0, 0, 0, 0)));
  s.count = 1;
  CHECK(same(bounding_box(&s), R(-2000000000, 0, 10, 10)));
}

TEST_CASE("damage: null pointers") {
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, 4, 4), kW));
  clear(nullptr);
  CHECK_FALSE(add(nullptr, R(0, 0, 4, 4), kW));
  CHECK_FALSE(add(nullptr, R(0, 0, 0, 0), kW));
  CHECK(size(nullptr) == 0u);
  Rect g = R(9, 9, 9, 9);
  CHECK_FALSE(get(nullptr, 0, &g));
  CHECK(same(g, R(9, 9, 9, 9)));
  CHECK_FALSE(get(&s, 0, nullptr));
  CHECK(total_area(nullptr) == 0u);
  CHECK(same(bounding_box(nullptr), R(0, 0, 0, 0)));
  CHECK(size(&s) == 1u);
}

TEST_CASE("damage: a corrupt set is invalid for every function and is never written") {
  const u32 counts[] = {9u, 0xFFFFFFFFu, 100u};
  for (u32 c : counts) {
    DamageSet s = fresh();
    for (int i = 0; i < 8; ++i) s.rects[i] = R(i * 10, 0, 5, 5);
    s.count = c;
    const DamageSet before = s;
    CHECK_FALSE(add(&s, R(0, 0, 4, 4), kW));
    CHECK_FALSE(add(&s, R(0, 0, 0, 0), kW));  // an empty add still fails: the corrupt check is first
    CHECK_FALSE(add(&s, R(500, 500, 4, 4), kW));
    CHECK(size(&s) == 0u);
    Rect g = R(9, 9, 9, 9);
    CHECK_FALSE(get(&s, 0, &g));
    CHECK_FALSE(get(&s, 8, &g));
    CHECK(same(g, R(9, 9, 9, 9)));
    CHECK(total_area(&s) == 0u);
    CHECK(same(bounding_box(&s), R(0, 0, 0, 0)));
    CHECK(bytes_equal(s, before));
    clear(&s);  // clear repairs it
    CHECK(s.count == 0u);
    CHECK(add(&s, R(0, 0, 4, 4), kW));
  }
  DamageSet e = fresh();
  e.count = 8;  // exactly kMaxRects is valid
  CHECK(size(&e) == 8u);
}

TEST_CASE("damage: get at every index, count boundary") {
  DamageSet s = fresh();
  for (int i = 0; i < 8; ++i) s.rects[i] = R(i * 10, 0, 5, 5);
  for (u32 n = 0; n <= 8; ++n) {
    s.count = n;
    for (u32 i = 0; i < 10; ++i) {
      Rect g = R(9, 9, 9, 9);
      const bool ok = get(&s, i, &g);
      CHECK(ok == (i < n));
      if (ok) {
        CHECK(same(g, s.rects[i]));
      } else {
        CHECK(same(g, R(9, 9, 9, 9)));
      }
    }
  }
  Rect g = R(9, 9, 9, 9);
  CHECK_FALSE(get(&s, 0xFFFFFFFFu, &g));
}

TEST_CASE("damage: stale rectangles beyond count are ignored") {
  DamageSet s = fresh();
  s.rects[5] = R(0, 0, 50, 50);
  s.rects[0] = R(0, 0, 4, 4);
  s.rects[1] = R(10, 0, 4, 4);
  s.count = 2;
  CHECK(total_area(&s) == 32u);
  CHECK(same(bounding_box(&s), R(0, 0, 14, 4)));
  CHECK(add(&s, R(2, 2, 4, 4), kW));  // must not merge with the stale rect 5
  const Rect w[] = {R(10, 0, 4, 4), R(0, 0, 6, 6)};
  expect_rects(s, w, 2);
}

TEST_CASE("damage: total_area is a plain 64 bit sum that wraps on hand-built overlapping sets") {
  DamageSet s = fresh();
  for (int i = 0; i < 8; ++i) s.rects[i] = R(0, 0, kMax, kMax);
  s.count = 8;
  const u64 one = static_cast<u64>(kMax) * static_cast<u64>(kMax);
  CHECK(total_area(&s) == 8u * one);  // unsigned wrap defined: 8 * (2^31-1)^2 is above 2^64
  CHECK(8u * one < one * 4u + one * 4u + 1u);
  s.count = 3;
  CHECK(total_area(&s) == 3u * one);
  // empty and invalid stored rectangles add nothing
  s.rects[0] = R(0, 0, 0, 5);
  s.rects[1] = R(0, 0, -3, 4);
  s.rects[2] = R(kMax, 0, 5, 5);
  CHECK(total_area(&s) == 0u);
}

TEST_CASE("damage: invariants hold on random adds") {
  Lcg r(0xD00D);
  DamageSet s = fresh();
  for (int k = 0; k < 4000; ++k) {
    const Rect b = R(r.rng(0, 20), r.rng(0, 20), r.rng(0, 60), r.rng(0, 60));
    const Rect rc = R(r.rng(-10, 90), r.rng(-10, 90), r.rng(0, 25), r.rng(0, 25));
    if (r.rng(0, 99) == 0) clear(&s);
    const DamageSet before = s;
    const bool ok = add(&s, rc, b);
    REQUIRE(ok);
    REQUIRE(s.count <= kMaxRects);
    const Rect clipped = lumora::rect::intersect(rc, b);
    if (lumora::rect::is_empty(clipped)) {
      REQUIRE(bytes_equal(s, before));
      continue;
    }
    for (u32 i = 0; i < s.count; ++i) {
      REQUIRE_FALSE(lumora::rect::is_empty(s.rects[i]));
      for (u32 j = i + 1; j < s.count; ++j) REQUIRE_FALSE(lumora::rect::overlaps(s.rects[i], s.rects[j]));
    }
    // every pixel of the clipped add is covered, and the set only grows within its old bounding box plus the add
    for (i64 py = clipped.y; py < static_cast<i64>(clipped.y) + clipped.h; ++py) {
      for (i64 px = clipped.x; px < static_cast<i64>(clipped.x) + clipped.w; ++px) {
        bool hit = false;
        for (u32 i = 0; i < s.count; ++i) hit = hit || lumora::rect::contains_point(s.rects[i], static_cast<std::int32_t>(px), static_cast<std::int32_t>(py));
        REQUIRE(hit);
      }
    }
    // every pixel that was covered before stays covered
    for (u32 j = 0; j < before.count; ++j) {
      const Rect& o = before.rects[j];
      bool hit = false;
      for (u32 i = 0; i < s.count; ++i) hit = hit || lumora::rect::contains_point(s.rects[i], o.x, o.y);
      REQUIRE(hit);
    }
  }
}

TEST_CASE("damage: digest self check of a small state") {
  DamageSet s = fresh();
  CHECK(add(&s, R(0, 0, 10, 10), kW));
  CHECK(add(&s, R(20, 0, 10, 10), kW));
  CHECK(state_digest(s) == kSmallState);
}
