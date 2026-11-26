// Tests for core/shelfpack (scope AD). Expected values come from an independent Python reference (tools/shelf_pack_ref.py: its own
// packer model with Python integers and its own FNV) and are pasted below as constants: scenario programs with their result digests,
// state digests and fields, and seeded random corpora. A separate oracle checks that placed rectangles stay inside the atlas and
// never overlap. Hand-computed placements are derived from shelf_pack.h. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "shelf_pack.h"

using namespace lumora::shelfpack;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kMaxU64 = ~static_cast<u64>(0);
constexpr u32 kSentinel = 0xA5A5A5A5u;

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

struct Op {
  char kind;
  u64 a[5];
};
struct Expect {
  u64 results_digest, state_digest;
  u32 shelf_count, next_y, width;
  u64 used_area;
  u32 result_words;
};
struct ProgRef {
  const Op* ops;
  std::size_t n;
  const Expect& exp;
};

struct Replay {
  ShelfPacker p;
  Fnv results;
  u32 words = 0;
  void res(u32 v) {
    results.word(v);
    ++words;
  }
  AllocResult alloc(u32 w, u32 h) {
    u32 x = kSentinel, y = kSentinel;
    const AllocResult r = packer_alloc(&p, w, h, &x, &y);
    if (r != AllocResult::Ok) {
      REQUIRE(x == kSentinel);  // nothing written on a failure
      REQUIRE(y == kSentinel);
    }
    res(static_cast<u32>(r));
    res(r == AllocResult::Ok ? x : 0u);
    res(r == AllocResult::Ok ? y : 0u);
    return r;
  }
  void apply(const Op& o) {
    switch (o.kind) {
      case 'I': res(packer_init(&p, static_cast<u32>(o.a[0]), static_cast<u32>(o.a[1])) ? 1u : 0u); break;
      case 'R': res(packer_reset(&p) ? 1u : 0u); break;
      case 'A': alloc(static_cast<u32>(o.a[0]), static_cast<u32>(o.a[1])); break;
      case 'P':
        p.width = static_cast<u32>(o.a[0]);
        p.height = static_cast<u32>(o.a[1]);
        p.shelf_count = static_cast<u32>(o.a[2]);
        p.next_y = static_cast<u32>(o.a[3]);
        p.used_area = o.a[4];
        break;
      case 'H':
        p.shelves[o.a[0]].y = static_cast<u32>(o.a[1]);
        p.shelves[o.a[0]].height = static_cast<u32>(o.a[2]);
        p.shelves[o.a[0]].used_x = static_cast<u32>(o.a[3]);
        break;
      default: FAIL("bad op");
    }
  }
  void check(const Expect& e) const {
    INFO("shelves " << p.shelf_count << " next_y " << p.next_y << " width " << p.width << " area " << p.used_area);
    REQUIRE(words == e.result_words);
    REQUIRE(results.h == e.results_digest);
    REQUIRE(packer_digest(&p) == e.state_digest);
    REQUIRE(p.shelf_count == e.shelf_count);
    REQUIRE(p.next_y == e.next_y);
    REQUIRE(p.width == e.width);
    REQUIRE(p.used_area == e.used_area);
  }
};

const Op kProg_basic[] = {
  {'A', {4ull, 4ull, 0ull, 0ull, 0ull}},
  {'I', {64ull, 64ull, 0ull, 0ull, 0ull}},
  {'A', {4ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {4ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {10ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {4ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {0ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {4ull, 0ull, 0ull, 0ull, 0ull}},
  {'A', {64ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {63ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {4ull, 64ull, 0ull, 0ull, 0ull}},
  {'A', {4ull, 63ull, 0ull, 0ull, 0ull}},
  {'A', {4294967295ull, 1ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 4294967295ull, 0ull, 0ull, 0ull}},
  {'A', {4294967294ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_basic = {14751352101460985406ull, 13691822513008793779ull, 2, 10, 64, 330ull, 43};
const Op kProg_shelf_choice[] = {
  {'I', {100ull, 100ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 9ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 19ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 9ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 14ull, 0ull, 0ull, 0ull}},
  {'A', {9ull, 19ull, 0ull, 0ull, 0ull}},
  {'A', {70ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {20ull, 8ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_shelf_choice = {10589950395201429958ull, 11089356453227588692ull, 3, 35, 100, 1142ull, 28};
const Op kProg_row_fill[] = {
  {'I', {33ull, 40ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {31ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {32ull, 5ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_row_fill = {6055795475534771010ull, 15993888096351252179ull, 4, 24, 33, 524ull, 34};
const Op kProg_vertical[] = {
  {'I', {20ull, 31ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'A', {19ull, 4ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_vertical = {2386027169651794648ull, 16838927620625495607ull, 1, 5, 20, 76ull, 29};
const Op kProg_shelf_limit[] = {
  {'I', {4096ull, 4096ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 2ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 4ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 6ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 7ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 8ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 9ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 10ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 11ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 12ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 13ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 14ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 15ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 16ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 17ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 18ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 19ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 20ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 21ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 22ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 23ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 24ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 25ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 26ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 27ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 28ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 29ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 30ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 31ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 32ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 33ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 34ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 35ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 36ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 37ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 38ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 39ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 40ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 41ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 42ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 43ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 44ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 45ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 46ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 47ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 48ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 49ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 50ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 51ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 52ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 53ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 54ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 55ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 56ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 57ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 58ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 59ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 60ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 61ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 62ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 63ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 64ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 65ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 66ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 67ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 68ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 69ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 70ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'A', {200ull, 4000ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_shelf_limit = {15698424425382864757ull, 9754244544948142429ull, 64, 2144, 4096, 2081ull, 217};
const Op kProg_no_space[] = {
  {'I', {16ull, 16ull, 0ull, 0ull, 0ull}},
  {'A', {15ull, 15ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'A', {15ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_no_space = {15900520149983492836ull, 9699285915568918213ull, 1, 16, 16, 225ull, 10};
const Op kProg_init_errors[] = {
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {0ull, 5ull, 0ull, 0ull, 0ull}},
  {'I', {5ull, 0ull, 0ull, 0ull, 0ull}},
  {'I', {4097ull, 5ull, 0ull, 0ull, 0ull}},
  {'I', {5ull, 4097ull, 0ull, 0ull, 0ull}},
  {'I', {4096ull, 4096ull, 0ull, 0ull, 0ull}},
  {'A', {4095ull, 4095ull, 0ull, 0ull, 0ull}},
  {'A', {4096ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {8ull, 8ull, 0ull, 0ull, 0ull}},
  {'A', {7ull, 7ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {4097ull, 8ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'I', {6ull, 6ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_init_errors = {3353061914729774485ull, 14657376272950852311ull, 1, 2, 6, 1ull, 30};
const Op kProg_reset[] = {
  {'I', {50ull, 50ull, 0ull, 0ull, 0ull}},
  {'A', {10ull, 10ull, 0ull, 0ull, 0ull}},
  {'A', {20ull, 20ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_reset = {5620888878207615982ull, 626212785313351493ull, 0, 0, 50, 0ull, 13};
const Op kProg_area[] = {
  {'I', {4096ull, 4096ull, 0ull, 0ull, 0ull}},
  {'A', {4094ull, 4094ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'A', {2ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_area = {15900520149983492836ull, 9588968212428268901ull, 1, 4095, 4096, 16760836ull, 10};
const Op kProg_hb_count_high[] = {
  {'I', {40ull, 40ull, 0ull, 0ull, 0ull}},
  {'P', {40ull, 40ull, 100ull, 0ull, 0ull}},
  {'H', {0ull, 0ull, 10ull, 0ull, 0ull}},
  {'H', {63ull, 5ull, 6ull, 0ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {30ull, 30ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_count_high = {12190269857536155365ull, 2306586641487358357ull, 100, 0, 40, 9ull, 7};
const Op kProg_hb_shelf_count_64[] = {
  {'I', {40ull, 40ull, 0ull, 0ull, 0ull}},
  {'P', {40ull, 40ull, 64ull, 2ull, 0ull}},
  {'H', {0ull, 0ull, 3ull, 38ull, 0ull}},
  {'H', {1ull, 3ull, 9ull, 0ull, 0ull}},
  {'A', {2ull, 2ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {5ull, 20ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_shelf_count_64 = {1721397987249861635ull, 3891554885955595308ull, 64, 2, 40, 29ull, 10};
const Op kProg_hb_tie_and_height[] = {
  {'I', {40ull, 40ull, 0ull, 0ull, 0ull}},
  {'P', {40ull, 40ull, 3ull, 30ull, 7ull}},
  {'H', {0ull, 0ull, 12ull, 0ull, 0ull}},
  {'H', {1ull, 12ull, 12ull, 0ull, 0ull}},
  {'H', {2ull, 24ull, 6ull, 35ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {3ull, 9ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_tie_and_height = {15364892061919699707ull, 11227578809144915425ull, 3, 30, 40, 52ull, 10};
const Op kProg_hb_big_fields[] = {
  {'I', {40ull, 40ull, 0ull, 0ull, 0ull}},
  {'P', {40ull, 40ull, 1ull, 4294967295ull, 18446744073709551610ull}},
  {'H', {0ull, 7ull, 10ull, 4294967295ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_big_fields = {18124778767446425124ull, 5592161995582354732ull, 1, 4294967295, 40, 18446744073709551610ull, 7};
const Op kProg_hb_big_next_y[] = {
  {'I', {40ull, 40ull, 0ull, 0ull, 0ull}},
  {'P', {40ull, 40ull, 0ull, 4294967294ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_big_next_y = {18124778767446425124ull, 11670420404649670064ull, 0, 4294967294, 40, 0ull, 7};
const Op kProg_hb_uninit[] = {
  {'P', {0ull, 5ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'P', {5ull, 0ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'R', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'P', {5000ull, 5000ull, 0ull, 0ull, 0ull}},
  {'A', {100ull, 100ull, 0ull, 0ull, 0ull}},
  {'A', {4999ull, 4999ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_uninit = {12505246191066907457ull, 2026419317230704156ull, 1, 101, 5000, 10000ull, 14};
const Op kProg_hb_area_sat[] = {
  {'I', {4096ull, 4096ull, 0ull, 0ull, 0ull}},
  {'P', {4096ull, 4096ull, 0ull, 0ull, 18446744073709551615ull}},
  {'A', {2ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
  {'P', {4096ull, 4096ull, 0ull, 0ull, 18446744073709551609ull}},
  {'A', {2ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_area_sat = {17183351901226378628ull, 9432642615761904489ull, 1, 4, 4096, 18446744073709551615ull, 13};
const Op kProg_hb_area_edge[] = {
  {'I', {4096ull, 4096ull, 0ull, 0ull, 0ull}},
  {'P', {4096ull, 4096ull, 0ull, 0ull, 18446744073709551608ull}},
  {'A', {2ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_area_edge = {2164280660266345287ull, 9432642615761904489ull, 1, 4, 4096, 18446744073709551615ull, 7};
const Op kProg_hb_zero_shelf[] = {
  {'I', {30ull, 30ull, 0ull, 0ull, 0ull}},
  {'P', {30ull, 30ull, 2ull, 5ull, 0ull}},
  {'H', {0ull, 0ull, 0ull, 0ull, 0ull}},
  {'H', {1ull, 0ull, 6ull, 29ull, 0ull}},
  {'A', {3ull, 3ull, 0ull, 0ull, 0ull}},
  {'A', {1ull, 1ull, 0ull, 0ull, 0ull}},
};
const Expect kExp_hb_zero_shelf = {3468962837698688464ull, 3520737025838921033ull, 3, 9, 30, 10ull, 7};
const ProgRef kProgs[] = {{kProg_basic, sizeof(kProg_basic) / sizeof(kProg_basic[0]), kExp_basic}, {kProg_shelf_choice, sizeof(kProg_shelf_choice) / sizeof(kProg_shelf_choice[0]), kExp_shelf_choice}, {kProg_row_fill, sizeof(kProg_row_fill) / sizeof(kProg_row_fill[0]), kExp_row_fill}, {kProg_vertical, sizeof(kProg_vertical) / sizeof(kProg_vertical[0]), kExp_vertical}, {kProg_shelf_limit, sizeof(kProg_shelf_limit) / sizeof(kProg_shelf_limit[0]), kExp_shelf_limit}, {kProg_no_space, sizeof(kProg_no_space) / sizeof(kProg_no_space[0]), kExp_no_space}, {kProg_init_errors, sizeof(kProg_init_errors) / sizeof(kProg_init_errors[0]), kExp_init_errors}, {kProg_reset, sizeof(kProg_reset) / sizeof(kProg_reset[0]), kExp_reset}, {kProg_area, sizeof(kProg_area) / sizeof(kProg_area[0]), kExp_area}, {kProg_hb_count_high, sizeof(kProg_hb_count_high) / sizeof(kProg_hb_count_high[0]), kExp_hb_count_high}, {kProg_hb_shelf_count_64, sizeof(kProg_hb_shelf_count_64) / sizeof(kProg_hb_shelf_count_64[0]), kExp_hb_shelf_count_64}, {kProg_hb_tie_and_height, sizeof(kProg_hb_tie_and_height) / sizeof(kProg_hb_tie_and_height[0]), kExp_hb_tie_and_height}, {kProg_hb_big_fields, sizeof(kProg_hb_big_fields) / sizeof(kProg_hb_big_fields[0]), kExp_hb_big_fields}, {kProg_hb_big_next_y, sizeof(kProg_hb_big_next_y) / sizeof(kProg_hb_big_next_y[0]), kExp_hb_big_next_y}, {kProg_hb_uninit, sizeof(kProg_hb_uninit) / sizeof(kProg_hb_uninit[0]), kExp_hb_uninit}, {kProg_hb_area_sat, sizeof(kProg_hb_area_sat) / sizeof(kProg_hb_area_sat[0]), kExp_hb_area_sat}, {kProg_hb_area_edge, sizeof(kProg_hb_area_edge) / sizeof(kProg_hb_area_edge[0]), kExp_hb_area_edge}, {kProg_hb_zero_shelf, sizeof(kProg_hb_zero_shelf) / sizeof(kProg_hb_zero_shelf[0]), kExp_hb_zero_shelf}};
const Expect kCorpus_d1 = {15924773773426709795ull, 5222971535709770383ull, 8, 119, 836, 25272ull, 8946};
const Expect kCorpus_d2 = {8459311922580332343ull, 1979000325236786707ull, 2, 67, 936, 3431ull, 8960};
const Expect kCorpus_d3 = {17542696879783478822ull, 3153938701371611765ull, 2, 21, 1631, 1438ull, 8918};
const Expect kCorpus_d4 = {5005662686529099999ull, 3580188037537618785ull, 3, 684, 3781, 505316ull, 8904};
const u64 kBasisDigest = 14695981039346656037ull;
const u64 kSmallDigest = 4894965084912024042ull;  // 100x50: allocs 9x9, 9x4, 20x3

}  // namespace

TEST_CASE("shelfpack: scenario programs against the reference", "[shelfpack]") {
  int pi = 0;
  for (const ProgRef& p : kProgs) {
    INFO("program " << pi++);
    Replay r;
    r.p = ShelfPacker{};
    for (std::size_t i = 0; i < p.n; ++i) r.apply(p.ops[i]);
    r.check(p.exp);
  }
}

TEST_CASE("shelfpack: seeded random corpora", "[shelfpack]") {
  struct Cfg {
    u64 seed;
    u32 aw, ah, mx, nops;
    const Expect* exp;
  };
  const Cfg cfgs[4] = {{0xD1, 256, 256, 24, 3000, &kCorpus_d1},
                       {0xD2, 1024, 512, 40, 3000, &kCorpus_d2},
                       {0xD3, 64, 4096, 12, 3000, &kCorpus_d3},
                       {0xD4, 4096, 4096, 300, 3000, &kCorpus_d4}};
  for (const Cfg& cfg : cfgs) {
    Lcg rng(cfg.seed);
    Replay r;
    r.p = ShelfPacker{};
    REQUIRE(packer_init(&r.p, cfg.aw, cfg.ah));
    for (u32 k = 0; k < cfg.nops; ++k) {
      const u32 x = rng.rng(0, 999);
      if (x < 940) {
        const u32 w = rng.rng(1, cfg.mx);
        const u32 h = rng.rng(1, cfg.mx);
        r.alloc(w, h);
      } else if (x < 970) {
        const u32 a = rng.rng(0, 1);
        const u32 w = a * rng.rng(1, cfg.mx);
        const u32 h = rng.rng(0, cfg.mx);
        r.alloc(w, h);
      } else if (x < 995) {
        r.res(packer_reset(&r.p) ? 1u : 0u);
      } else {
        const u32 w = rng.rng(1, 5000);
        const u32 h = rng.rng(1, 5000);
        r.res(packer_init(&r.p, w, h) ? 1u : 0u);
      }
      if (k % 50 == 49) {
        const u64 d = packer_digest(&r.p);
        r.res(static_cast<u32>(d & 0xFFFFFFFFu));
        r.res(static_cast<u32>(d >> 32));
      }
    }
    r.check(*cfg.exp);
  }
}

TEST_CASE("shelfpack: placed rectangles stay inside the atlas and never overlap", "[shelfpack]") {
  struct Rect {
    u32 x, y, w, h;  // the cell: rectangle plus padding
  };
  const u32 sizes[4][3] = {{200, 150, 30}, {512, 64, 20}, {97, 300, 9}, {64, 64, 64}};
  for (const auto& cfg : sizes) {
    Lcg rng(cfg[0] * 31u + cfg[1]);
    ShelfPacker p;
    REQUIRE(packer_init(&p, cfg[0], cfg[1]));
    std::vector<Rect> placed;
    u64 area = 0;
    for (int k = 0; k < 600; ++k) {
      const u32 w = rng.rng(1, cfg[2]);
      const u32 h = rng.rng(1, cfg[2]);
      u32 x = 0, y = 0;
      if (packer_alloc(&p, w, h, &x, &y) != AllocResult::Ok) continue;
      area += static_cast<u64>(w) * h;
      REQUIRE(x + w + kPad <= cfg[0]);
      REQUIRE(y + h + kPad <= cfg[1]);
      for (const Rect& r : placed) {
        const bool apart = x + w + kPad <= r.x || r.x + r.w <= x || y + h + kPad <= r.y || r.y + r.h <= y;
        REQUIRE(apart);
      }
      placed.push_back(Rect{x, y, w + kPad, h + kPad});
    }
    REQUIRE(p.used_area == area);
    REQUIRE(placed.size() > 10);
    // a shelf never holds a cell taller than itself and next_y is the sum of the shelf heights
    u32 sum = 0;
    for (u32 i = 0; i < p.shelf_count; ++i) {
      REQUIRE(p.shelves[i].y == sum);
      sum += p.shelves[i].height;
    }
    REQUIRE(sum == p.next_y);
  }
}

TEST_CASE("shelfpack: init and reset", "[shelfpack]") {
  ShelfPacker p;
  REQUIRE_FALSE(packer_init(nullptr, 8, 8));
  REQUIRE_FALSE(packer_reset(nullptr));
  REQUIRE_FALSE(packer_reset(&p));  // not initialized
  REQUIRE_FALSE(packer_init(&p, 0, 8));
  REQUIRE_FALSE(packer_init(&p, 8, 0));
  REQUIRE_FALSE(packer_init(&p, kMaxAtlas + 1, 8));
  REQUIRE_FALSE(packer_init(&p, 8, kMaxAtlas + 1));
  REQUIRE(p.width == 0);
  REQUIRE(p.height == 0);
  REQUIRE(packer_init(&p, kMaxAtlas, kMaxAtlas));
  REQUIRE(p.width == kMaxAtlas);
  REQUIRE(packer_init(&p, 1, 1));
  u32 x = 0, y = 0;
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::BadSize);  // the cell is 2 x 2
  REQUIRE(packer_init(&p, 10, 10));
  REQUIRE(packer_alloc(&p, 3, 3, &x, &y) == AllocResult::Ok);
  const u64 before = packer_digest(&p);
  REQUIRE_FALSE(packer_init(&p, 0, 5));  // a failed init changes nothing
  REQUIRE(packer_digest(&p) == before);
  REQUIRE_FALSE(packer_init(&p, 5, 4097));
  REQUIRE(packer_digest(&p) == before);
  REQUIRE(packer_reset(&p));
  REQUIRE(p.width == 10);
  REQUIRE(p.height == 10);
  REQUIRE(p.shelf_count == 0);
  REQUIRE(p.next_y == 0);
  REQUIRE(p.used_area == 0);
  for (u32 i = 0; i < kMaxShelves; ++i) {
    REQUIRE(p.shelves[i].y == 0);
    REQUIRE(p.shelves[i].height == 0);
    REQUIRE(p.shelves[i].used_x == 0);
  }
  REQUIRE(packer_alloc(&p, 3, 3, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 0);
  REQUIRE(y == 0);
  // init re-sizes and clears the shelves
  REQUIRE(packer_init(&p, 20, 30));
  REQUIRE(p.width == 20);
  REQUIRE(p.height == 30);
  REQUIRE(p.shelf_count == 0);
  REQUIRE(p.shelves[0].height == 0);
  REQUIRE(p.used_area == 0);
}

TEST_CASE("shelfpack: hand computed placements", "[shelfpack]") {
  ShelfPacker p;
  u32 x = 99, y = 99;
  REQUIRE(packer_init(&p, 100, 50));
  // 9 x 9: cell 10 x 10, new shelf 0 at y 0
  REQUIRE(packer_alloc(&p, 9, 9, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 0);
  REQUIRE(y == 0);
  REQUIRE(p.shelf_count == 1);
  REQUIRE(p.shelves[0].height == 10);
  REQUIRE(p.shelves[0].used_x == 10);
  REQUIRE(p.next_y == 10);
  // 9 x 4: cell 10 x 5 fits the shelf of height 10 (taller shelf accepted), placed at (10, 0)
  REQUIRE(packer_alloc(&p, 9, 4, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 10);
  REQUIRE(y == 0);
  REQUIRE(p.shelf_count == 1);
  // 20 x 3: cell 21 x 4 at (20, 0)
  REQUIRE(packer_alloc(&p, 20, 3, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 20);
  REQUIRE(y == 0);
  REQUIRE(p.shelves[0].used_x == 41);
  REQUIRE(p.used_area == 81 + 36 + 60);
  REQUIRE(packer_digest(&p) == kSmallDigest);
  // 9 x 19: cell 10 x 20 does not fit shelf 0 (height 10): new shelf at y 10
  REQUIRE(packer_alloc(&p, 9, 19, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 0);
  REQUIRE(y == 10);
  REQUIRE(p.shelves[1].height == 20);
  REQUIRE(p.next_y == 30);
  // a cell of height 6 fits both shelves; the smaller shelf (height 10) wins even though shelf 1 has more room
  REQUIRE(packer_alloc(&p, 5, 5, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 41);
  REQUIRE(y == 0);
  // a row that is full: 60 wide cell does not fit in shelf 0 (used_x 47) but fits shelf 1 (used_x 10)
  REQUIRE(packer_alloc(&p, 59, 5, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 10);
  REQUIRE(y == 10);
  // exact fit to the right edge: shelf 0 has used_x 47, a cell of width 53 ends at 100
  REQUIRE(packer_alloc(&p, 52, 2, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 47);
  REQUIRE(y == 0);
  REQUIRE(p.shelves[0].used_x == 100);
  // one more column does not fit any more
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::Ok);  // goes to shelf 1 (used_x 70)
  REQUIRE(y == 10);
  // the atlas edges: a cell as large as the atlas is allowed, one more is BadSize
  REQUIRE(packer_init(&p, 10, 10));
  REQUIRE(packer_alloc(&p, 9, 9, &x, &y) == AllocResult::Ok);
  REQUIRE(packer_reset(&p));
  REQUIRE(packer_alloc(&p, 10, 9, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 9, 10, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 0, 9, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 9, 0, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 0xFFFFFFFFu, 1, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 1, 0xFFFFFFFFu, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 0xFFFFFFFEu, 1, &x, &y) == AllocResult::BadSize);
  REQUIRE(p.shelf_count == 0);
  REQUIRE(packer_alloc(&p, 9, 9, &x, &y) == AllocResult::Ok);
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::NoSpace);
  REQUIRE(p.shelf_count == 1);
  REQUIRE(p.used_area == 81);
}

TEST_CASE("shelfpack: argument checks and their order", "[shelfpack]") {
  ShelfPacker p;
  u32 x = kSentinel, y = kSentinel;
  REQUIRE(packer_alloc(nullptr, 1, 1, &x, &y) == AllocResult::NullArgument);
  REQUIRE(packer_alloc(&p, 1, 1, nullptr, &y) == AllocResult::NullArgument);
  REQUIRE(packer_alloc(&p, 1, 1, &x, nullptr) == AllocResult::NullArgument);
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::NotInitialized);
  REQUIRE(packer_alloc(&p, 0, 0, &x, &y) == AllocResult::NotInitialized);  // before BadSize
  REQUIRE(packer_alloc(nullptr, 0, 0, nullptr, nullptr) == AllocResult::NullArgument);
  p.width = 5;  // height still 0
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::NotInitialized);
  p.width = 0;
  p.height = 5;
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::NotInitialized);
  REQUIRE_FALSE(packer_reset(&p));
  REQUIRE(x == kSentinel);
  REQUIRE(y == kSentinel);
  REQUIRE(packer_digest(nullptr) == kBasisDigest);
  REQUIRE(packer_init(&p, 8, 8));
  REQUIRE(packer_alloc(&p, 0, 0, &x, &y) == AllocResult::BadSize);
  REQUIRE(packer_alloc(&p, 20, 0, &x, &y) == AllocResult::BadSize);
  REQUIRE(x == kSentinel);
  REQUIRE(y == kSentinel);
  REQUIRE(p.shelf_count == 0);
  REQUIRE(p.used_area == 0);
}

TEST_CASE("shelfpack: at most 64 shelves", "[shelfpack]") {
  ShelfPacker p;
  REQUIRE(packer_init(&p, 4096, 4096));
  u32 x = 0, y = 0;
  for (u32 i = 0; i < kMaxShelves; ++i) {
    REQUIRE(packer_alloc(&p, 1, 1 + i, &x, &y) == AllocResult::Ok);
  }
  REQUIRE(p.shelf_count == kMaxShelves);
  // the 65th shelf is refused although the atlas has room; a rectangle that fits an existing shelf is still placed
  REQUIRE(packer_alloc(&p, 1, 80, &x, &y) == AllocResult::NoSpace);
  REQUIRE(p.shelf_count == kMaxShelves);
  REQUIRE(packer_alloc(&p, 1, 3, &x, &y) == AllocResult::Ok);
  REQUIRE(p.shelf_count == kMaxShelves);
}

TEST_CASE("shelfpack: used_area saturates at the maximum", "[shelfpack]") {
  ShelfPacker p;
  u32 x = 0, y = 0;
  REQUIRE(packer_init(&p, 100, 100));
  p.used_area = kMaxU64 - 6;
  REQUIRE(packer_alloc(&p, 2, 3, &x, &y) == AllocResult::Ok);  // exactly reaches the maximum
  REQUIRE(p.used_area == kMaxU64);
  REQUIRE(packer_alloc(&p, 1, 1, &x, &y) == AllocResult::Ok);
  REQUIRE(p.used_area == kMaxU64);  // stays, no wrap
  REQUIRE(packer_init(&p, 100, 100));
  p.used_area = kMaxU64 - 5;
  REQUIRE(packer_alloc(&p, 2, 3, &x, &y) == AllocResult::Ok);  // one past the maximum
  REQUIRE(p.used_area == kMaxU64);
  REQUIRE(packer_init(&p, 100, 100));
  p.used_area = kMaxU64;
  REQUIRE(packer_alloc(&p, 9, 9, &x, &y) == AllocResult::Ok);
  REQUIRE(p.used_area == kMaxU64);
  // a NoSpace result leaves used_area alone
  REQUIRE(packer_init(&p, 10, 10));
  p.used_area = 77;
  REQUIRE(packer_alloc(&p, 9, 9, &x, &y) == AllocResult::Ok);
  REQUIRE(p.used_area == 158);
  REQUIRE(packer_alloc(&p, 9, 9, &x, &y) == AllocResult::NoSpace);
  REQUIRE(p.used_area == 158);
}

TEST_CASE("shelfpack: clear reaches the last shelf slot and large hand-built atlases", "[shelfpack]") {
  ShelfPacker p;
  u32 x = 0, y = 0;
  REQUIRE(packer_init(&p, 10, 10));
  p.shelves[kMaxShelves - 1] = Shelf{3, 4, 5};
  REQUIRE(packer_reset(&p));
  REQUIRE(p.shelves[kMaxShelves - 1].y == 0);
  REQUIRE(p.shelves[kMaxShelves - 1].height == 0);
  REQUIRE(p.shelves[kMaxShelves - 1].used_x == 0);
  p.shelves[kMaxShelves - 1] = Shelf{3, 4, 5};
  REQUIRE(packer_init(&p, 10, 10));
  REQUIRE(p.shelves[kMaxShelves - 1].used_x == 0);
  REQUIRE(p.shelves[kMaxShelves - 1].height == 0);
  // alloc does not reject dimensions above kMaxAtlas: w * h is taken in 64 bits (70000 * 70000 > 2^32)
  p.width = 70002;
  p.height = 70002;
  REQUIRE(packer_alloc(&p, 70000, 70000, &x, &y) == AllocResult::Ok);
  REQUIRE(x == 0);
  REQUIRE(y == 0);
  REQUIRE(p.used_area == 4900000000ull);
  REQUIRE(p.shelves[0].height == 70001);
  REQUIRE(p.next_y == 70001);
}
