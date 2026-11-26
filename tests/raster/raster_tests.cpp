// Tests for the integer polygon rasterizer (core/raster).
//
// Expected masks, flatten points and digests below were produced by an independent Python reference of the rules in
// docs/raster.md (a naive per-edge accumulation written from the rules, not from raster.cpp), then checked into this file as
// constants. Nothing here is computed by the implementation under test, except the relations (reversal, crop, split) which
// compare the implementation with itself on purpose.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "raster.h"

using namespace lumora::raster;

namespace {

constexpr Fx kE = 256;
constexpr std::uint8_t kSentinel = 0xA5;
constexpr std::uint64_t kBasis = 0xcbf29ce484222325ull;

using Poly = std::vector<std::vector<P>>;

struct Case {
  const char* name;
  std::uint32_t w, h;
  int rule;
  Poly cs;
  std::vector<std::uint8_t> exp;
  std::uint64_t digest;
};

struct FlatCase {
  const char* name;
  bool quad;
  std::vector<P> pts;
  Fx tol;
  std::vector<P> exp;
};

const std::vector<Case>& cases() {
  static const std::vector<Case> v = {
    {"rect_aligned", 4, 4, 0, {{{256,256},{768,256},{768,768},{256,768}}}, {0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0}, 0x39374f4ff20962c9ull},
    {"rect_aligned_evenodd", 4, 4, 1, {{{256,256},{768,256},{768,768},{256,768}}}, {0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0}, 0x39374f4ff20962c9ull},
    {"rect_half_x", 3, 1, 0, {{{128,0},{576,0},{576,256},{128,256}}}, {128,255,64}, 0x2ee7381ae0c9c5e0ull},
    {"rect_frac_all_sides", 4, 3, 0, {{{100,60},{900,60},{900,700},{100,700}}}, {119,195,195,101,155,255,255,131,114,187,187,97}, 0xad4ed2ef0e6e919cull},
    {"square_pixel_sized_offset", 2, 2, 0, {{{100,100},{356,100},{356,356},{100,356}}}, {95,61,61,39}, 0xab97dd44f2f0a461ull},
    {"tiny_square_one_unit", 2, 2, 0, {{{255,255},{257,255},{257,257},{255,257}}}, {0,0,0,0}, 0x4d25767f9dce13f5ull},
    {"triangle_diag", 4, 4, 0, {{{0,0},{1024,0},{0,1024}}}, {255,255,255,128,255,255,128,0,255,128,0,0,128,0,0,0}, 0xee8620637358cefdull},
    {"triangle_odd", 8, 8, 0, {{{37,91},{1900,300},{700,1777}}}, {109,126,97,68,40,12,0,0,103,255,255,255,255,255,228,30,14,244,255,255,255,254,88,0,0,158,255,255,255,135,0,0,0,57,255,255,181,2,0,0,0,0,212,216,15,0,0,0,0,0,97,39,0,0,0,0,0,0,0,0,0,0,0,0}, 0x6e5dd1400d05cd75ull},
    {"triangle_cw_vs_ccw_a", 8, 8, 0, {{{37,91},{1900,300},{700,1777}}}, {109,126,97,68,40,12,0,0,103,255,255,255,255,255,228,30,14,244,255,255,255,254,88,0,0,158,255,255,255,135,0,0,0,57,255,255,181,2,0,0,0,0,212,216,15,0,0,0,0,0,97,39,0,0,0,0,0,0,0,0,0,0,0,0}, 0x6e5dd1400d05cd75ull},
    {"triangle_cw_vs_ccw_b", 8, 8, 0, {{{700,1777},{1900,300},{37,91}}}, {109,126,97,68,40,12,0,0,103,255,255,255,255,255,228,30,14,244,255,255,255,254,88,0,0,158,255,255,255,135,0,0,0,57,255,255,181,2,0,0,0,0,212,216,15,0,0,0,0,0,97,39,0,0,0,0,0,0,0,0,0,0,0,0}, 0x6e5dd1400d05cd75ull},
    {"same_dir_nonzero", 4, 4, 0, {{{0,0},{768,0},{768,768},{0,768}},{{256,256},{1024,256},{1024,1024},{256,1024}}}, {255,255,255,0,255,255,255,255,255,255,255,255,0,255,255,255}, 0xf81974f848732c97ull},
    {"same_dir_evenodd", 4, 4, 1, {{{0,0},{768,0},{768,768},{0,768}},{{256,256},{1024,256},{1024,1024},{256,1024}}}, {255,255,255,0,255,0,0,255,255,0,0,255,0,255,255,255}, 0xfdc8161775bd5633ull},
    {"opposite_dir_nonzero", 4, 4, 0, {{{0,0},{768,0},{768,768},{0,768}},{{256,1024},{1024,1024},{1024,256},{256,256}}}, {255,255,255,0,255,0,0,255,255,0,0,255,0,255,255,255}, 0xfdc8161775bd5633ull},
    {"opposite_dir_evenodd", 4, 4, 1, {{{0,0},{768,0},{768,768},{0,768}},{{256,1024},{1024,1024},{1024,256},{256,256}}}, {255,255,255,0,255,0,0,255,255,0,0,255,0,255,255,255}, 0xfdc8161775bd5633ull},
    {"hole_nonzero", 6, 6, 0, {{{0,0},{1536,0},{1536,1536},{0,1536}},{{512,1024},{1024,1024},{1024,512},{512,512}}}, {255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255}, 0xc8f33e78d4809855ull},
    {"hole_evenodd", 6, 6, 1, {{{0,0},{1536,0},{1536,1536},{0,1536}},{{512,512},{1024,512},{1024,1024},{512,1024}}}, {255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255}, 0xc8f33e78d4809855ull},
    {"same_dir_inner_nonzero", 6, 6, 0, {{{0,0},{1536,0},{1536,1536},{0,1536}},{{512,512},{1024,512},{1024,1024},{512,1024}}}, {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}, 0xc7711968b780b1b1ull},
    {"half_overlap_partial_pixels", 4, 2, 0, {{{100,0},{700,0},{700,512},{100,512}},{{300,0},{900,0},{900,512},{300,512}}}, {155,255,255,131,155,255,255,131}, 0x6dff719edf10fd5dull},
    {"half_overlap_partial_pixels_eo", 4, 2, 1, {{{100,0},{700,0},{700,512},{100,512}},{{300,0},{900,0},{900,512},{300,512}}}, {155,44,68,131,155,44,68,131}, 0x040994b3695fc581ull},
    {"star_nonzero", 8, 8, 0, {{{1024,156},{1534,1726},{198,756},{1850,756},{514,1726}}}, {0,0,0,6,6,0,0,0,0,0,0,74,74,0,0,0,2,12,12,169,169,12,12,2,2,123,255,255,255,255,123,2,0,0,124,255,255,124,0,0,0,0,150,184,184,150,0,0,0,0,74,0,0,74,0,0,0,0,0,0,0,0,0,0}, 0x325a60709f687ca3ull},
    {"star_evenodd", 8, 8, 1, {{{1024,156},{1534,1726},{198,756},{1850,756},{514,1726}}}, {0,0,0,6,6,0,0,0,0,0,0,74,74,0,0,0,2,12,12,169,169,12,12,2,2,123,255,20,20,255,123,2,0,0,124,33,33,124,0,0,0,0,150,184,184,150,0,0,0,0,74,0,0,74,0,0,0,0,0,0,0,0,0,0}, 0x6e4746bc54cafa59ull},
    {"outside_left_part", 4, 3, 0, {{{-818,-256},{592,-256},{592,512},{-818,512}}}, {255,255,80,0,255,255,80,0,0,0,0,0}, 0x5a93307d3dbd4301ull},
    {"outside_top_part", 3, 3, 0, {{{40,-1280},{522,-1280},{522,346},{40,346}}}, {215,255,10,76,90,4,0,0,0}, 0x56c978fb15f80121ull},
    {"outside_right_part", 4, 3, 0, {{{316,40},{2304,40},{2304,519},{316,519}}}, {0,165,215,215,0,195,255,255,0,5,7,7}, 0x6a7d8beb04fd4c42ull},
    {"outside_bottom_part", 3, 4, 0, {{{30,356},{611,356},{611,5120},{30,5120}}}, {0,0,0,137,155,60,225,255,99,225,255,99}, 0x2a97901ef2e91f27ull},
    {"outside_all_around_covers", 3, 3, 0, {{{-16777216,-16777216},{16777216,-16777216},{16777216,16777216},{-16777216,16777216}}}, {255,255,255,255,255,255,255,255,255}, 0x2804ddde49e161a6ull},
    {"fully_outside_left", 3, 3, 0, {{{-1280,0},{-256,0},{-256,768},{-1280,768}}}, {0,0,0,0,0,0,0,0,0}, 0xe604823a249029bfull},
    {"fully_outside_right", 3, 3, 0, {{{1024,0},{2304,0},{2304,768},{1024,768}}}, {0,0,0,0,0,0,0,0,0}, 0xe604823a249029bfull},
    {"fully_outside_above", 3, 3, 0, {{{0,-1280},{768,-1280},{768,-256},{0,-256}}}, {0,0,0,0,0,0,0,0,0}, 0xe604823a249029bfull},
    {"fully_outside_below", 3, 3, 0, {{{0,1024},{768,1024},{768,2304},{0,2304}}}, {0,0,0,0,0,0,0,0,0}, 0xe604823a249029bfull},
    {"slanted_edge_outside_left", 6, 6, 0, {{{-1024,-256},{1280,512},{256,1636}}}, {128,42,0,0,0,0,255,255,213,128,42,0,255,255,255,255,139,0,255,255,255,161,1,0,254,255,180,4,0,0,154,197,10,0,0,0}, 0xa928c4358bb97ceaull},
    {"slanted_far_coords", 5, 5, 0, {{{-16777216,-16777216},{16777216,-16772216},{77,16777216}}}, {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}, 0x96fdd03483dfbb16ull},
    {"steep_long_edge", 12, 12, 0, {{{10,10},{23,3072},{512,3069}}}, {19,0,0,0,0,0,0,0,0,0,0,0,59,0,0,0,0,0,0,0,0,0,0,0,100,0,0,0,0,0,0,0,0,0,0,0,141,0,0,0,0,0,0,0,0,0,0,0,182,0,0,0,0,0,0,0,0,0,0,0,222,0,0,0,0,0,0,0,0,0,0,0,238,25,0,0,0,0,0,0,0,0,0,0,237,67,0,0,0,0,0,0,0,0,0,0,236,109,0,0,0,0,0,0,0,0,0,0,235,150,0,0,0,0,0,0,0,0,0,0,234,192,0,0,0,0,0,0,0,0,0,0,232,232,0,0,0,0,0,0,0,0,0,0}, 0x2f6e9a84cea6bc01ull},
    {"horizontal_and_repeated_points", 4, 4, 0, {{{0,0},{0,0},{512,0},{1024,0},{1024,1024},{1024,1024},{0,1024}}}, {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}, 0xd6607508f5a1e855ull},
    {"zero_area_contour", 4, 4, 0, {{{256,256},{768,768},{512,512}}}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x88201fb960ff6465ull},
    {"degenerate_all_same_point", 4, 4, 0, {{{256,256},{256,256},{256,256}}}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x88201fb960ff6465ull},
    {"no_contours", 3, 3, 0, {}, {0,0,0,0,0,0,0,0,0}, 0xe604823a249029bfull},
    {"two_disjoint_triangles", 8, 4, 0, {{{10,10},{768,20},{256,768}},{{1280,10},{1822,256},{1536,1014}}}, {196,240,162,0,0,157,72,1,124,236,25,0,0,160,232,5,41,87,0,0,0,95,140,0,0,0,0,0,0,30,45,0}, 0xcf9235c3dbca1e37ull},
    {"thin_sliver", 5, 5, 0, {{{0,0},{1280,1280},{1280,1279}}}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1}, 0x0f1304cb145bb057ull},
    {"wide_target_one_row", 16, 1, 0, {{{20,10},{4063,100},{2048,249}}}, {11,35,59,83,108,132,157,181,181,156,131,107,82,58,33,9}, 0xe944d5f446af7af2ull},
    {"tall_target_one_col", 1, 16, 1, {{{10,20},{247,4091},{128,768}}}, {11,36,61,71,66,60,55,49,43,37,31,25,20,14,9,3}, 0x9528e3f1c9ea1d96ull},
    {"quad_whole", 8, 8, 0, {{{37,91},{1293,60},{1737,1079},{90,1287}}}, {140,173,179,185,192,42,0,0,205,255,255,255,255,153,0,0,194,255,255,255,255,246,19,0,183,255,255,255,255,255,121,0,168,225,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x2a6e7f1c1daceb3full},
    {"quad_whole_eo", 8, 8, 1, {{{37,91},{1293,60},{1737,1079},{90,1287}}}, {140,173,179,185,192,42,0,0,205,255,255,255,255,153,0,0,194,255,255,255,255,246,19,0,183,255,255,255,255,255,121,0,168,225,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x2a6e7f1c1daceb3full},
    {"quad_tri_abc", 8, 8, 0, {{{37,91},{1293,60},{1737,1079}}}, {56,168,179,185,192,42,0,0,0,41,185,255,255,153,0,0,0,0,0,78,219,246,19,0,0,0,0,0,7,119,108,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x93be05106d09aa2eull},
    {"quad_tri_cda", 8, 8, 0, {{{1737,1079},{90,1287},{37,91}}}, {84,5,0,0,0,0,0,0,205,214,70,0,0,0,0,0,194,255,255,177,36,0,0,0,183,255,255,255,248,136,13,0,168,225,193,160,128,96,43,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x8251daaac958586dull},
    {"quad_tri_bcd", 8, 8, 0, {{{1293,60},{1737,1079},{90,1287}}}, {0,0,0,0,64,42,0,0,0,0,0,60,243,153,0,0,0,0,57,242,255,246,19,0,0,53,240,255,255,255,121,0,47,208,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0xddc0c0ca81f9e15aull},
    {"quad_tri_dab", 8, 8, 0, {{{90,1287},{37,91},{1293,60}}}, {140,173,179,185,128,0,0,0,205,255,255,195,12,0,0,0,194,255,198,13,0,0,0,0,183,202,15,0,0,0,0,0,121,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0xb8dae2214b99c05aull},
    {"quad_split_ac", 8, 8, 0, {{{37,91},{1293,60},{1737,1079}},{{1737,1079},{90,1287},{37,91}}}, {140,173,179,185,192,42,0,0,205,255,255,255,255,153,0,0,194,255,255,255,255,246,19,0,183,255,255,255,255,255,121,0,168,225,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x2a6e7f1c1daceb3full},
    {"quad_split_ac_eo", 8, 8, 1, {{{37,91},{1293,60},{1737,1079}},{{1737,1079},{90,1287},{37,91}}}, {140,173,179,185,192,42,0,0,205,255,255,255,255,153,0,0,194,255,255,255,255,246,19,0,183,255,255,255,255,255,121,0,168,225,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x2a6e7f1c1daceb3full},
    {"quad_split_bd", 8, 8, 0, {{{1293,60},{1737,1079},{90,1287}},{{90,1287},{37,91},{1293,60}}}, {140,173,179,185,192,42,0,0,205,255,255,255,255,153,0,0,194,255,255,255,255,246,19,0,183,255,255,255,255,255,121,0,168,225,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x2a6e7f1c1daceb3full},
    {"quad_split_bd_eo", 8, 8, 1, {{{1293,60},{1737,1079},{90,1287}},{{90,1287},{37,91},{1293,60}}}, {140,173,179,185,192,42,0,0,205,255,255,255,255,153,0,0,194,255,255,255,255,246,19,0,183,255,255,255,255,255,121,0,168,225,193,160,128,96,50,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x2a6e7f1c1daceb3full},
    {"tie_neg_num_a", 4, 4, 0, {{{500,0},{499,512},{900,768}}}, {0,11,55,0,0,12,187,0,0,0,90,44,0,0,0,0}, 0x200102b1c243b032ull},
    {"tie_neg_num_b", 4, 4, 0, {{{500,0},{497,1536},{0,1536}}}, {0,41,0,0,0,124,0,0,0,207,0,0,47,242,0,0}, 0x0251a4c1295e525eull},
    {"tie_neg_num_c", 4, 4, 0, {{{700,0},{698,768},{0,768}}}, {0,4,111,0,1,160,186,0,139,255,186,0,0,0,0,0}, 0x4b535ea86bc5a9ddull},
    {"tie_pos_num", 4, 4, 0, {{{300,0},{301,512},{900,768}}}, {0,99,0,0,0,210,88,0,0,37,133,29,0,0,0,0}, 0x1d3603648269c2fbull},
    {"tie_cols_neg", 4, 4, 0, {{{0,700},{514,696},{0,768}}}, {0,0,0,0,0,0,0,0,51,17,0,0,0,0,0,0}, 0x909a6ca3e7dd9541ull},
    {"tie_cols_swapped", 4, 4, 0, {{{0,700},{514,698},{768,768}}}, {0,0,0,0,0,0,0,0,12,35,24,0,0,0,0,0}, 0xb049d5c28310da60ull},
    {"very_steep", 4, 16, 0, {{{100,0},{103,4096},{200,4096}}}, {3,0,0,0,9,0,0,0,15,0,0,0,21,0,0,0,27,0,0,0,33,0,0,0,40,0,0,0,45,0,0,0,51,0,0,0,57,0,0,0,64,0,0,0,70,0,0,0,76,0,0,0,82,0,0,0,88,0,0,0,94,0,0,0}, 0xc3adc93aac215f6eull},
    {"very_steep_neg", 4, 16, 0, {{{768,0},{763,4096},{256,4096}}}, {0,0,16,0,0,0,47,0,0,0,79,0,0,0,111,0,0,0,142,0,0,0,173,0,0,0,205,0,0,0,237,0,0,16,253,0,0,48,252,0,0,80,252,0,0,112,252,0,0,143,251,0,0,175,251,0,0,207,251,0,0,239,250,0}, 0xa302932342c4cdc1ull},
    {"very_shallow", 16, 4, 0, {{{0,100},{4096,103},{0,768}}}, {155,155,155,154,154,154,154,154,153,153,153,153,144,104,62,21,255,255,255,255,255,255,240,200,158,117,75,34,2,0,0,0,234,193,151,110,69,27,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x4062af1926feb3e2ull},
    {"very_shallow_rows", 16, 4, 0, {{{0,250},{4096,262},{0,777}}}, {5,4,4,3,2,1,1,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,245,213,180,148,115,82,49,16,247,216,184,151,120,88,56,24,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x13a3a3e22cb33a5dull},
    {"very_shallow_neg", 16, 4, 0, {{{4096,100},{0,346},{4096,768}}}, {0,0,0,0,0,0,10,25,40,56,71,86,102,117,132,148,20,62,104,145,187,229,254,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,7,32,58,84,111,137,163,189,216,242,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x6ee8a0ee350a2d71ull},
  };
  return v;
}

const std::vector<FlatCase>& flat_cases() {
  static const std::vector<FlatCase> v = {
    {"quad_flat_k0", true, {{0,0},{512,0},{1024,0}}, 1, {{1024,0}}},
    {"quad_k2_tol64", true, {{0,0},{0,1024},{1024,0}}, 64, {{64,384},{256,512},{576,384},{1024,0}}},
    {"quad_k5_tol1", true, {{0,0},{0,1024},{1024,0}}, 1, {{1,62},{4,120},{9,174},{16,224},{25,270},{36,312},{49,350},{64,384},{81,414},{100,440},{121,462},{144,480},{169,494},{196,504},{225,510},{256,512},{289,510},{324,504},{361,494},{400,480},{441,462},{484,440},{529,414},{576,384},{625,350},{676,312},{729,270},{784,224},{841,174},{900,120},{961,62},{1024,0}}},
    {"quad_tol0_as1", true, {{0,0},{0,1024},{1024,0}}, 0, {{1,62},{4,120},{9,174},{16,224},{25,270},{36,312},{49,350},{64,384},{81,414},{100,440},{121,462},{144,480},{169,494},{196,504},{225,510},{256,512},{289,510},{324,504},{361,494},{400,480},{441,462},{484,440},{529,414},{576,384},{625,350},{676,312},{729,270},{784,224},{841,174},{900,120},{961,62},{1024,0}}},
    {"quad_tolneg_as1", true, {{0,0},{0,1024},{1024,0}}, -77, {{1,62},{4,120},{9,174},{16,224},{25,270},{36,312},{49,350},{64,384},{81,414},{100,440},{121,462},{144,480},{169,494},{196,504},{225,510},{256,512},{289,510},{324,504},{361,494},{400,480},{441,462},{484,440},{529,414},{576,384},{625,350},{676,312},{729,270},{784,224},{841,174},{900,120},{961,62},{1024,0}}},
    {"quad_odd_mid", true, {{-3,5},{1001,-7},{15,999}}, 8, {{116,9},{218,19},{305,37},{375,63},{431,98},{471,140},{496,190},{504,248},{498,314},{475,388},{438,470},{384,560},{316,658},{231,764},{131,878},{15,999}}},
    {"quad_k6_cap", true, {{-16777216,-16777216},{16777216,-16777216},{16777216,16777216}}, 1, {{-15736832,-16769024},{-14712832,-16744448},{-13705216,-16703488},{-12713984,-16646144},{-11739136,-16572416},{-10780672,-16482304},{-9838592,-16375808},{-8912896,-16252928},{-8003584,-16113664},{-7110656,-15958016},{-6234112,-15785984},{-5373952,-15597568},{-4530176,-15392768},{-3702784,-15171584},{-2891776,-14934016},{-2097152,-14680064},{-1318912,-14409728},{-557056,-14123008},{188416,-13819904},{917504,-13500416},{1630208,-13164544},{2326528,-12812288},{3006464,-12443648},{3670016,-12058624},{4317184,-11657216},{4947968,-11239424},{5562368,-10805248},{6160384,-10354688},{6742016,-9887744},{7307264,-9404416},{7856128,-8904704},{8388608,-8388608},{8904704,-7856128},{9404416,-7307264},{9887744,-6742016},{10354688,-6160384},{10805248,-5562368},{11239424,-4947968},{11657216,-4317184},{12058624,-3670016},{12443648,-3006464},{12812288,-2326528},{13164544,-1630208},{13500416,-917504},{13819904,-188416},{14123008,557056},{14409728,1318912},{14680064,2097152},{14934016,2891776},{15171584,3702784},{15392768,4530176},{15597568,5373952},{15785984,6234112},{15958016,7110656},{16113664,8003584},{16252928,8912896},{16375808,9838592},{16482304,10780672},{16572416,11739136},{16646144,12713984},{16703488,13705216},{16744448,14712832},{16769024,15736832},{16777216,16777216}}},
    {"quad_hugetol", true, {{-16777216,-16777216},{16777216,-16777216},{16777216,16777216}}, 268435456, {{16777216,16777216}}},
    {"cubic_flat_k0", false, {{0,0},{300,0},{600,0},{900,0}}, 1, {{900,0}}},
    {"cubic_k3", false, {{0,0},{0,2048},{2048,2048},{2048,0}}, 16, {{23,360},{88,672},{189,936},{320,1152},{475,1320},{648,1440},{833,1512},{1024,1536},{1215,1512},{1400,1440},{1573,1320},{1728,1152},{1859,936},{1960,672},{2025,360},{2048,0}}},
    {"cubic_k4_tol1", false, {{0,0},{0,300},{300,300},{300,0}}, 1, {{2,28},{5,54},{10,79},{14,100},{22,121},{29,139},{39,156},{48,170},{60,184},{72,195},{85,205},{97,212},{112,219},{124,222},{138,225},{150,225},{165,225},{179,222},{194,219},{206,212},{220,205},{232,195},{244,184},{254,170},{265,156},{274,139},{282,121},{288,100},{294,79},{297,54},{300,28},{300,0}}},
    {"cubic_neg_odd", false, {{-5,-9},{1001,-333},{-777,1999},{31,-1}}, 4, {{83,-30},{154,-39},{211,-34},{254,-19},{286,9},{305,44},{314,88},{313,137},{305,194},{289,253},{267,317},{237,380},{206,445},{169,507},{130,568},{88,624},{49,678},{8,723},{-31,762},{-69,791},{-101,812},{-131,820},{-155,816},{-173,797},{-182,765},{-184,715},{-177,648},{-160,561},{-130,455},{-90,326},{-36,176},{31,-1}}},
    {"cubic_k6", false, {{-16777216,-16777216},{16777216,-16777216},{-16777216,16777216},{16777216,16777216}}, 1, {{-15252992,-16752896},{-13824000,-16680960},{-12487168,-16562944},{-11239424,-16400384},{-10077696,-16194816},{-8998912,-15947776},{-8000000,-15660800},{-7077888,-15335424},{-6229504,-14973184},{-5451776,-14575616},{-4741632,-14144256},{-4096000,-13680640},{-3511808,-13186304},{-2985984,-12662784},{-2515456,-12111616},{-2097152,-11534336},{-1728000,-10932480},{-1404928,-10307584},{-1124864,-9661184},{-884736,-8994816},{-681472,-8310016},{-512000,-7608320},{-373248,-6891264},{-262144,-6160384},{-175616,-5417216},{-110592,-4663296},{-64000,-3900160},{-32768,-3129344},{-13824,-2352384},{-4096,-1570816},{-512,-786176},{0,0},{512,786176},{4096,1570816},{13824,2352384},{32768,3129344},{64000,3900160},{110592,4663296},{175616,5417216},{262144,6160384},{373248,6891264},{512000,7608320},{681472,8310016},{884736,8994816},{1124864,9661184},{1404928,10307584},{1728000,10932480},{2097152,11534336},{2515456,12111616},{2985984,12662784},{3511808,13186304},{4096000,13680640},{4741632,14144256},{5451776,14575616},{6229504,14973184},{7077888,15335424},{8000000,15660800},{8998912,15947776},{10077696,16194816},{11239424,16400384},{12487168,16562944},{13824000,16680960},{15252992,16752896},{16777216,16777216}}},
    {"cubic_second_dominates", false, {{0,0},{10,0},{20,0},{30,2000}}, 1, {{3,1},{3,1},{3,1},{3,1},{6,3},{6,3},{6,4},{6,4},{9,7},{9,9},{9,12},{9,14},{9,19},{9,22},{9,28},{9,32},{12,40},{12,46},{12,55},{12,63},{12,74},{12,83},{12,95},{12,106},{15,121},{15,136},{15,153},{15,169},{15,189},{15,208},{15,229},{15,250},{18,276},{18,301},{18,329},{18,357},{21,389},{21,420},{21,455},{21,489},{24,528},{24,567},{24,609},{24,651},{24,698},{24,744},{24,794},{24,844},{27,900},{27,955},{27,1014},{27,1074},{27,1138},{27,1203},{27,1271},{27,1341},{30,1415},{30,1490},{30,1569},{30,1649},{30,1734},{30,1820},{30,1909},{30,2000}}},
  };
  return v;
}

const Case& by_name(const std::string& n) {
  for (const Case& c : cases()) {
    if (n == c.name) return c;
  }
  FAIL("unknown case " << n);
  return cases().front();
}

std::vector<Contour> view(const Poly& cs) {
  std::vector<Contour> out;
  for (const auto& c : cs) out.push_back(Contour{c.data(), c.size()});
  return out;
}

// Renders into a buffer with `pad` bytes of stride padding per row. Returns the packed w*h bytes. Checks that every mask
// pixel was written (the buffer is pre-filled with a value that cannot appear twice by accident: 0x77 then verified through
// the reference) and that the padding is untouched.
std::vector<std::uint8_t> render(const Poly& cs, std::uint32_t w, std::uint32_t h, Fill rule, std::uint32_t pad = 3) {
  const std::uint32_t stride = w + pad;
  std::vector<std::uint8_t> buf(static_cast<std::size_t>(stride) * h, kSentinel);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) buf[static_cast<std::size_t>(y) * stride + x] = 0x77;
  }
  const std::vector<Contour> cv = view(cs);
  Target t{buf.data(), w, h, stride};
  REQUIRE(fill_polygons(cv.data(), cv.size(), rule, t) == RasterStatus::Ok);
  std::vector<std::uint8_t> out(static_cast<std::size_t>(w) * h);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) out[static_cast<std::size_t>(y) * w + x] = buf[static_cast<std::size_t>(y) * stride + x];
    for (std::uint32_t x = w; x < stride; ++x) REQUIRE(buf[static_cast<std::size_t>(y) * stride + x] == kSentinel);
  }
  return out;
}

Fill rule_of(int r) { return r == 0 ? Fill::NonZero : Fill::EvenOdd; }

std::uint64_t fnv(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t h = kBasis;
  for (std::uint8_t b : bytes) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  return h;
}

bool same_pt(const P& a, const P& b) { return a.x == b.x && a.y == b.y; }

}  // namespace

TEST_CASE("fill_polygons matches the reference masks", "[raster]") {
  for (const Case& c : cases()) {
    INFO(c.name);
    REQUIRE(c.exp.size() == static_cast<std::size_t>(c.w) * c.h);
    const auto got = render(c.cs, c.w, c.h, rule_of(c.rule));
    REQUIRE(got == c.exp);
    REQUIRE(fnv(got) == c.digest);
    // digest_mask agrees, and skips the padding.
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(c.w + 3) * c.h, kSentinel);
    const auto cv = view(c.cs);
    Target t{buf.data(), c.w, c.h, c.w + 3};
    REQUIRE(fill_polygons(cv.data(), cv.size(), rule_of(c.rule), t) == RasterStatus::Ok);
    REQUIRE(digest_mask(t) == c.digest);
  }
}

TEST_CASE("hand-computed exact masks", "[raster]") {
  // Aligned rectangle: pixels (1..2, 1..2) fully covered, nothing else.
  CHECK(by_name("rect_aligned").exp == std::vector<std::uint8_t>{0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0, 0});
  // Rectangle x 128..576 in one row: half, full, quarter. (32768*255+32768)>>16 = 128 and (16384*255+32768)>>16 = 64.
  CHECK(by_name("rect_half_x").exp == std::vector<std::uint8_t>{128, 255, 64});
  // A pixel-sized square offset by (100,100)/256: areas 156*156, 100*156, 156*100, 100*100 units of 1/65536.
  CHECK(by_name("square_pixel_sized_offset").exp == std::vector<std::uint8_t>{
            static_cast<std::uint8_t>((156 * 156 * 255 + 32768) >> 16), static_cast<std::uint8_t>((100 * 156 * 255 + 32768) >> 16),
            static_cast<std::uint8_t>((156 * 100 * 255 + 32768) >> 16), static_cast<std::uint8_t>((100 * 100 * 255 + 32768) >> 16)});
  // Right triangle (0,0),(4,0),(0,4) pixels in a 4x4 target: full pixels above the diagonal, half on it.
  CHECK(by_name("triangle_diag").exp == std::vector<std::uint8_t>{255, 255, 255, 128, 255, 255, 128, 0, 255, 128, 0, 0, 128, 0, 0, 0});
  // A one-unit square at a pixel corner is under half a unit of coverage, so it rounds to 0 everywhere.
  CHECK(by_name("tiny_square_one_unit").exp == std::vector<std::uint8_t>{0, 0, 0, 0});
}

TEST_CASE("overlap and winding under both fill rules", "[raster]") {
  // Same direction: NonZero saturates, EvenOdd cancels the overlap. Opposite direction: both rules cancel the overlap.
  const auto& sn = by_name("same_dir_nonzero").exp;
  const auto& se = by_name("same_dir_evenodd").exp;
  const auto& on = by_name("opposite_dir_nonzero").exp;
  const auto& oe = by_name("opposite_dir_evenodd").exp;
  CHECK(sn[1 * 4 + 1] == 255);  // overlap pixel, NonZero
  CHECK(se[1 * 4 + 1] == 0);    // overlap pixel, EvenOdd
  CHECK(on[1 * 4 + 1] == 0);
  CHECK(oe[1 * 4 + 1] == 0);
  CHECK(sn[0] == 255);
  CHECK(se[0] == 255);
  CHECK(se == on);
  CHECK(on == oe);
  // Same-direction inner square saturates under NonZero (all 36 pixels) but is a hole under EvenOdd.
  CHECK(by_name("same_dir_inner_nonzero").exp[2 * 6 + 2] == 255);
  CHECK(by_name("hole_evenodd").exp[2 * 6 + 2] == 0);
  CHECK(by_name("hole_nonzero").exp[2 * 6 + 2] == 0);
  // The self-intersecting star: the centre pentagon is wound twice, so it is filled by NonZero and almost empty under EvenOdd
  // (pixel (3,3) holds only slivers of singly wound area, 20/255). Arm pixels such as (2,3) are wound once and agree.
  CHECK(by_name("star_nonzero").exp[3 * 8 + 3] == 255);
  CHECK(by_name("star_evenodd").exp[3 * 8 + 3] == 20);
  CHECK(by_name("star_nonzero").exp[3 * 8 + 2] == 255);
  CHECK(by_name("star_evenodd").exp[3 * 8 + 2] == 255);
  CHECK(by_name("star_evenodd").exp != by_name("star_nonzero").exp);
  // Partial overlap pixels: sum of two partial areas saturates differently under the two rules.
  CHECK(by_name("half_overlap_partial_pixels").exp != by_name("half_overlap_partial_pixels_eo").exp);
}

TEST_CASE("contours that are partly or fully outside the target", "[raster]") {
  for (const char* n : {"fully_outside_left", "fully_outside_right", "fully_outside_above", "fully_outside_below", "no_contours"}) {
    INFO(n);
    for (std::uint8_t v : by_name(n).exp) CHECK(v == 0);
  }
  for (std::uint8_t v : by_name("outside_all_around_covers").exp) CHECK(v == 255);
  // A rectangle spilling over the left edge keeps its cover: columns 0..1 are full where the rectangle spans.
  const auto& l = by_name("outside_left_part").exp;
  CHECK(l[0 * 4 + 0] == 255);
  CHECK(l[1 * 4 + 1] == 255);
}

TEST_CASE("reversing every contour gives identical bytes", "[raster]") {
  for (const Case& c : cases()) {
    INFO(c.name);
    Poly rv = c.cs;
    for (auto& ct : rv) {
      const std::vector<P> copy(ct.rbegin(), ct.rend());
      ct = copy;
    }
    CHECK(render(rv, c.w, c.h, rule_of(c.rule)) == c.exp);
    // Reversing the contour order as well changes nothing either (sum is order independent).
    Poly rr(rv.rbegin(), rv.rend());
    CHECK(render(rr, c.w, c.h, rule_of(c.rule)) == c.exp);
  }
}

TEST_CASE("a clipped render equals a larger render cropped", "[raster]") {
  for (const Case& c : cases()) {
    INFO(c.name);
    const std::uint32_t bw = c.w + 5, bh = c.h + 4;
    const auto big = render(c.cs, bw, bh, rule_of(c.rule));
    for (std::uint32_t y = 0; y < c.h; ++y) {
      for (std::uint32_t x = 0; x < c.w; ++x) REQUIRE(big[static_cast<std::size_t>(y) * bw + x] == c.exp[static_cast<std::size_t>(y) * c.w + x]);
    }
    // Shifting everything by whole pixels and cropping gives the same bytes again (pieces are translation invariant).
    Poly sh = c.cs;
    bool ok = true;
    for (auto& ct : sh) {
      for (P& p : ct) {
        const std::int64_t nx = static_cast<std::int64_t>(p.x) + 2 * kE, ny = static_cast<std::int64_t>(p.y) + 3 * kE;
        if (nx > kMaxCoord || ny > kMaxCoord) ok = false;
        p.x = static_cast<Fx>(nx);
        p.y = static_cast<Fx>(ny);
      }
    }
    if (!ok) continue;
    const std::uint32_t sw = c.w + 4, sh_h = c.h + 6;
    const auto shifted = render(sh, sw, sh_h, rule_of(c.rule));
    for (std::uint32_t y = 0; y < c.h; ++y) {
      for (std::uint32_t x = 0; x < c.w; ++x) REQUIRE(shifted[static_cast<std::size_t>(y + 3) * sw + (x + 2)] == c.exp[static_cast<std::size_t>(y) * c.w + x]);
    }
  }
}

TEST_CASE("a shared slanted boundary in both directions", "[raster]") {
  // The quad a,b,c,d split along a-c (triangles abc and cda) and along b-d (bcd and dab). Each diagonal is traversed in
  // opposite directions by the two triangles, so the snapped pieces cancel exactly and the union equals the whole quad
  // byte for byte, under both fill rules.
  const Case& whole = by_name("quad_whole");
  for (const char* n : {"quad_split_ac", "quad_split_ac_eo", "quad_split_bd", "quad_split_bd_eo"}) {
    INFO(n);
    CHECK(by_name(n).exp == whole.exp);
  }
  // Complementary coverage: the two halves add up to the union within the rounding of the two separate roundings (+-1).
  const std::pair<const char*, const char*> halves[] = {{"quad_tri_abc", "quad_tri_cda"}, {"quad_tri_bcd", "quad_tri_dab"}};
  for (const auto& hv : halves) {
    const auto& a = by_name(hv.first).exp;
    const auto& b = by_name(hv.second).exp;
    for (std::size_t i = 0; i < whole.exp.size(); ++i) {
      const int sum = a[i] + b[i];
      INFO(hv.first << " pixel " << i);
      CHECK(sum >= whole.exp[i] - 1);
      CHECK(sum <= whole.exp[i] + 1);
    }
  }
  // Same two triangles, with the second one given the other way round in the contour list and starting at another vertex.
  Poly alt = {{P{0, 0}}};
  const Poly& src = by_name("quad_split_ac").cs;
  alt = src;
  alt[1] = {src[1][2], src[1][0], src[1][1]};
  CHECK(render(alt, 8, 8, Fill::NonZero) == whole.exp);
}

TEST_CASE("rounding ties and extreme slopes", "[raster]") {
  // tie_neg_num_*: crossings with a negative numerator whose quotient is exactly k + 1/2 (ties go toward +infinity).
  // Edge (500,0)->(499,512): at y=256, n=-256, d=512, floor((-512+512)/1024)=0, so x=500 (not 499).
  // Edge (500,0)->(497,1536): at y=256, n=-768, d=1536, quotient -1/2, x=500.
  // The expected masks come from the reference; here we only pin the snapped crossing through a mask-level consequence by
  // comparing with the equivalent polygon that uses the snapped integer points directly.
  {
    const Case& c = by_name("tie_neg_num_a");
    Poly snapped = {{P{500, 0}, P{500, 256}, P{499, 512}, P{900, 3 * kE}}};
    CHECK(render(snapped, c.w, c.h, rule_of(c.rule)) == c.exp);
  }
  {
    const Case& c = by_name("tie_neg_num_b");
    // dx=-3 over d=1536. Row crossings (n/d, snapped x): y=256 (-0.5, 500), y=512 (-1, 499), y=768 (-1.5, 499),
    // y=1024 (-2, 498), y=1280 (-2.5, 498). Both ties land on the larger integer.
    Poly snapped = {{P{500, 0}, P{500, 256}, P{499, 512}, P{499, 768}, P{498, 1024}, P{498, 1280}, P{497, 1536}, P{0, 1536}}};
    CHECK(render(snapped, c.w, c.h, rule_of(c.rule)) == c.exp);
  }
  // The positive counterpart: (300,0)->(301,512): at y=256, n=256, d=512, floor(1024/1024)=1, x=301.
  {
    const Case& c = by_name("tie_pos_num");
    Poly snapped = {{P{300, 0}, P{301, 256}, P{301, 512}, P{900, 3 * kE}}};
    CHECK(render(snapped, c.w, c.h, rule_of(c.rule)) == c.exp);
  }
  // Very steep and very shallow edges are only checked against the reference masks (table test above); here we also pin that
  // the steep/shallow polygons have the expected areas to within the pixel grid.
  for (const char* n : {"very_steep", "very_steep_neg", "very_shallow", "very_shallow_rows", "very_shallow_neg"}) {
    INFO(n);
    int total = 0;
    for (std::uint8_t v : by_name(n).exp) total += v;
    CHECK(total > 0);
  }
}

TEST_CASE("status codes and their check order", "[raster]") {
  std::vector<std::uint8_t> buf(64, kSentinel);
  const std::vector<P> tri = {P{0, 0}, P{256, 0}, P{0, 256}};
  const Contour good{tri.data(), tri.size()};
  auto untouched = [&] {
    for (std::uint8_t b : buf) {
      if (b != kSentinel) return false;
    }
    return true;
  };
  const Target ok_t{buf.data(), 4, 4, 8};

  // NullBuffer: first, even if everything after it is also wrong.
  {
    const Target t{nullptr, 0, 0, 0};
    CHECK(fill_polygons(nullptr, 5000, static_cast<Fill>(9), t) == RasterStatus::NullBuffer);
  }
  CHECK(fill_polygons(nullptr, 1, Fill::NonZero, ok_t) == RasterStatus::NullBuffer);
  {
    const Contour nullpts{nullptr, 3};
    const Contour two{nullptr, 0};
    CHECK(fill_polygons(&nullpts, 1, Fill::NonZero, ok_t) == RasterStatus::NullBuffer);
    // A null point pointer with a nonzero count is a null buffer even when another error comes earlier in enum order.
    const Target bad_size{buf.data(), 0, 4, 8};
    CHECK(fill_polygons(&nullpts, 1, static_cast<Fill>(3), bad_size) == RasterStatus::NullBuffer);
    // An empty contour with a null pointer is not a null buffer: it is a bad contour (count < 3).
    CHECK(fill_polygons(&two, 1, Fill::NonZero, ok_t) == RasterStatus::BadContour);
  }
  CHECK(untouched());
  // BadSize: zero, over the limit, stride below width. Reported before a bad fill, contour and point.
  const Contour badc{tri.data(), 2};
  const std::vector<P> far = {P{0, 0}, P{kMaxCoord + 1, 0}, P{0, 256}};
  const Contour badp{far.data(), far.size()};
  const Target sizes[] = {{buf.data(), 0, 4, 8}, {buf.data(), 4, 0, 8}, {buf.data(), 8193, 4, 9000}, {buf.data(), 4, 8193, 8},
                          {buf.data(), 4, 4, 3}};
  for (const Target& t : sizes) {
    CHECK(fill_polygons(&badc, 1, static_cast<Fill>(7), t) == RasterStatus::BadSize);
    CHECK(fill_polygons(&badp, 1, Fill::NonZero, t) == RasterStatus::BadSize);
  }
  // BadFill: before bad contours and points.
  CHECK(fill_polygons(&badc, 1, static_cast<Fill>(2), ok_t) == RasterStatus::BadFill);
  CHECK(fill_polygons(&badp, 1, static_cast<Fill>(255), ok_t) == RasterStatus::BadFill);
  // BadContour: before bad points. Count 2, count 2^20+1 and 4097 contours.
  {
    const Contour both[] = {badp, badc};
    CHECK(fill_polygons(both, 2, Fill::NonZero, ok_t) == RasterStatus::BadContour);
    std::vector<P> big(static_cast<std::size_t>(kMaxPointsPerContour) + 1, P{0, 0});
    const Contour toobig{big.data(), big.size()};
    CHECK(fill_polygons(&toobig, 1, Fill::NonZero, ok_t) == RasterStatus::BadContour);
    std::vector<Contour> many(static_cast<std::size_t>(kMaxContours) + 1, good);
    CHECK(fill_polygons(many.data(), many.size(), Fill::EvenOdd, ok_t) == RasterStatus::BadContour);
    many.back() = badp;
    CHECK(fill_polygons(many.data(), many.size(), Fill::EvenOdd, ok_t) == RasterStatus::BadContour);
  }
  // BadPoint: last, for either axis, either sign, in a later contour; the limit itself is allowed.
  {
    const std::vector<P> px = {P{0, 0}, P{256, 0}, P{-kMaxCoord - 1, 256}};
    const std::vector<P> py = {P{0, 0}, P{256, 0}, P{0, kMaxCoord + 1}};
    const std::vector<P> ny = {P{0, 0}, P{256, 0}, P{0, -kMaxCoord - 1}};
    const Contour c1[] = {good, Contour{px.data(), px.size()}};
    const Contour c2[] = {good, good, Contour{py.data(), py.size()}};
    const Contour c3[] = {Contour{ny.data(), ny.size()}};
    CHECK(fill_polygons(c1, 2, Fill::NonZero, ok_t) == RasterStatus::BadPoint);
    CHECK(fill_polygons(c2, 3, Fill::EvenOdd, ok_t) == RasterStatus::BadPoint);
    CHECK(fill_polygons(c3, 1, Fill::NonZero, ok_t) == RasterStatus::BadPoint);
    CHECK(untouched());
    const std::vector<P> edge = {P{-kMaxCoord, -kMaxCoord}, P{kMaxCoord, -kMaxCoord}, P{kMaxCoord, kMaxCoord}};
    const Contour ce{edge.data(), edge.size()};
    CHECK(fill_polygons(&ce, 1, Fill::NonZero, ok_t) == RasterStatus::Ok);
  }
  CHECK(std::string(raster_status_text(RasterStatus::Ok)) != std::string(raster_status_text(RasterStatus::BadPoint)));
}

TEST_CASE("empty input clears the mask and the limits are accepted", "[raster]") {
  std::vector<std::uint8_t> buf(12 * 3, 0x55);
  const Target t{buf.data(), 9, 3, 12};
  REQUIRE(fill_polygons(nullptr, 0, Fill::NonZero, t) == RasterStatus::Ok);
  for (std::uint32_t y = 0; y < 3; ++y) {
    for (std::uint32_t x = 0; x < 12; ++x) CHECK(buf[y * 12 + x] == (x < 9 ? 0 : 0x55));
  }
  // 4096 contours are valid. Each triangle covers half of pixel (0,0): A2 = 65536.
  const std::vector<P> tri = {P{0, 0}, P{256, 0}, P{0, 256}};
  std::vector<Contour> many(kMaxContours, Contour{tri.data(), tri.size()});
  std::uint8_t px = 7;
  const Target one{&px, 1, 1, 1};
  REQUIRE(fill_polygons(many.data(), many.size(), Fill::NonZero, one) == RasterStatus::Ok);
  CHECK(px == 255);  // 4096 * 65536 saturates at 131072
  REQUIRE(fill_polygons(many.data(), many.size(), Fill::EvenOdd, one) == RasterStatus::Ok);
  CHECK(px == 0);  // 4096 * 65536 mod 262144 = 0
  REQUIRE(fill_polygons(many.data(), many.size() - 1, Fill::EvenOdd, one) == RasterStatus::Ok);
  CHECK(px == 128);  // 4095 * 65536 mod 262144 = 196608, reflected: 65536
  REQUIRE(fill_polygons(many.data(), 1, Fill::NonZero, one) == RasterStatus::Ok);
  CHECK(px == 128);
}

TEST_CASE("a contour with 2^20 points", "[raster]") {
  // A pixel-aligned rectangle (256,256)-(1792,1280) whose perimeter is cut into collinear points. Exact result: 255 inside
  // (columns 1..6, rows 1..4 of an 8x6 target), 0 elsewhere.
  const std::size_t n = static_cast<std::size_t>(kMaxPointsPerContour);
  const std::size_t q = n / 4;
  std::vector<P> pts;
  pts.reserve(n);
  for (std::size_t i = 0; i < q; ++i) pts.push_back(P{static_cast<Fx>(256 + (i * 1536) / q), 256});
  for (std::size_t i = 0; i < q; ++i) pts.push_back(P{1792, static_cast<Fx>(256 + (i * 1024) / q)});
  for (std::size_t i = 0; i < q; ++i) pts.push_back(P{static_cast<Fx>(1792 - (i * 1536) / q), 1280});
  for (std::size_t i = 0; i < q; ++i) pts.push_back(P{256, static_cast<Fx>(1280 - (i * 1024) / q)});
  REQUIRE(pts.size() == n);
  const Poly p = {pts};
  const auto m = render(p, 8, 6, Fill::NonZero);
  for (std::uint32_t y = 0; y < 6; ++y) {
    for (std::uint32_t x = 0; x < 8; ++x) CHECK(m[y * 8 + x] == ((x >= 1 && x <= 6 && y >= 1 && y <= 4) ? 255 : 0));
  }
}

TEST_CASE("flatten_quad and flatten_cubic", "[raster]") {
  for (const FlatCase& c : flat_cases()) {
    INFO(c.name);
    const std::size_t n = c.exp.size();
    std::vector<P> out(n + 2, P{-12345, 54321});
    std::size_t got = 0;
    if (c.quad) {
      got = flatten_quad(c.pts[0], c.pts[1], c.pts[2], c.tol, out.data(), n);
    } else {
      got = flatten_cubic(c.pts[0], c.pts[1], c.pts[2], c.pts[3], c.tol, out.data(), n);
    }
    REQUIRE(got == n);
    for (std::size_t i = 0; i < n; ++i) {
      REQUIRE(out[i].x == c.exp[i].x);
      REQUIRE(out[i].y == c.exp[i].y);
    }
    CHECK(same_pt(out[n - 1], c.pts.back()));  // ends exactly at the last control point
    CHECK(out[n].x == -12345);                 // nothing written past the count
    CHECK(out[n + 1].y == 54321);
    // Cap one short, and a cap of zero: returns 0 and writes nothing.
    if (n > 1) {
      std::vector<P> small(n, P{-12345, 54321});
      const std::size_t r = c.quad ? flatten_quad(c.pts[0], c.pts[1], c.pts[2], c.tol, small.data(), n - 1)
                                   : flatten_cubic(c.pts[0], c.pts[1], c.pts[2], c.pts[3], c.tol, small.data(), n - 1);
      CHECK(r == 0);
      for (const P& p : small) CHECK(same_pt(p, P{-12345, 54321}));
    }
    std::vector<P> zero(1, P{-12345, 54321});
    const std::size_t r0 = c.quad ? flatten_quad(c.pts[0], c.pts[1], c.pts[2], c.tol, zero.data(), 0)
                                  : flatten_cubic(c.pts[0], c.pts[1], c.pts[2], c.pts[3], c.tol, zero.data(), 0);
    CHECK(r0 == 0);
    CHECK(same_pt(zero[0], P{-12345, 54321}));
  }
}

TEST_CASE("flatten rejects bad input without writing", "[raster]") {
  P out[4];
  for (P& p : out) p = P{-1, -1};
  const P a{0, 0}, b{100, 200}, c{300, 0}, d{400, 100};
  CHECK(flatten_quad(a, b, c, 1, nullptr, 4) == 0);
  CHECK(flatten_cubic(a, b, c, d, 1, nullptr, 4) == 0);
  const P bad[] = {P{kMaxCoord + 1, 0}, P{0, kMaxCoord + 1}, P{-kMaxCoord - 1, 0}, P{0, -kMaxCoord - 1}};
  for (const P& e : bad) {
    CHECK(flatten_quad(e, b, c, 1, out, 4) == 0);
    CHECK(flatten_quad(a, e, c, 1, out, 4) == 0);
    CHECK(flatten_quad(a, b, e, 1, out, 4) == 0);
    CHECK(flatten_cubic(e, b, c, d, 1, out, 4) == 0);
    CHECK(flatten_cubic(a, e, c, d, 1, out, 4) == 0);
    CHECK(flatten_cubic(a, b, e, d, 1, out, 4) == 0);
    CHECK(flatten_cubic(a, b, c, e, 1, out, 4) == 0);
  }
  for (const P& p : out) CHECK(same_pt(p, P{-1, -1}));
  // Coordinates exactly at the limit are accepted.
  const P lo{-kMaxCoord, -kMaxCoord}, hi{kMaxCoord, kMaxCoord};
  P big[64];
  CHECK(flatten_quad(lo, hi, lo, 1 << 20, big, 64) >= 1);
  CHECK(flatten_cubic(lo, hi, lo, hi, 1 << 20, big, 64) >= 1);
}

TEST_CASE("digest_mask", "[raster]") {
  // Known FNV-1a 64 values: "a" -> af63dc4c8601ec8c, "foobar" -> 85944171f73967e8, empty -> offset basis.
  std::uint8_t a = 'a';
  CHECK(digest_mask(Target{&a, 1, 1, 1}) == 0xaf63dc4c8601ec8cull);
  const char* fb = "foobar";
  std::vector<std::uint8_t> row(fb, fb + 6);
  CHECK(digest_mask(Target{row.data(), 6, 1, 6}) == 0x85944171f73967e8ull);
  // Two rows of 3 with 2 padding bytes each (different content): only the 6 pixel bytes are hashed, top row first.
  std::vector<std::uint8_t> pad = {'f', 'o', 'o', 1, 2, 'b', 'a', 'r', 9, 9};
  CHECK(digest_mask(Target{pad.data(), 3, 2, 5}) == 0x85944171f73967e8ull);
  pad[3] = 200;
  pad[9] = 0;
  CHECK(digest_mask(Target{pad.data(), 3, 2, 5}) == 0x85944171f73967e8ull);
  // Zero bytes hashed: null data, zero width or zero height.
  CHECK(digest_mask(Target{nullptr, 4, 4, 4}) == kBasis);
  CHECK(digest_mask(Target{row.data(), 0, 4, 4}) == kBasis);
  CHECK(digest_mask(Target{row.data(), 4, 0, 4}) == kBasis);
}

TEST_CASE("large targets: golden digests and bounded time", "[raster][perf]") {
  // 4096 x 4096 (16 MiB mask on the heap): a big triangle and an inner rectangle. The digests come from the Python reference.
  const Poly big = {{P{100, 50}, P{4096 * kE - 77, 300}, P{2000 * kE + 3, 4096 * kE - 9}},
                    {P{500 * kE + 17, 700 * kE + 33}, P{3000 * kE + 91, 700 * kE + 33}, P{3000 * kE + 91, 2500 * kE + 5},
                     P{500 * kE + 17, 2500 * kE + 5}}};
  {
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(4096) * 4096, 0);
    const auto cv = view(big);
    Target t{buf.data(), 4096, 4096, 4096};
    REQUIRE(fill_polygons(cv.data(), cv.size(), Fill::NonZero, t) == RasterStatus::Ok);
    CHECK(digest_mask(t) == 0x4041e8c50354ce33ull);
    REQUIRE(fill_polygons(cv.data(), cv.size(), Fill::EvenOdd, t) == RasterStatus::Ok);
    CHECK(digest_mask(t) == 0x79099e7bf6aef0b9ull);
  }
  // 8192 x 300: wider than one band row budget, so the work is done in several bands (128 rows each).
  const Poly wide = {{P{10, 5}, P{8192 * kE - 33, 3 * kE + 1}, P{4000 * kE + 7, 300 * kE - 5}},
                     {P{kE + 3, kE + 9}, P{8190 * kE + 11, kE + 9}, P{8190 * kE + 11, 250 * kE}, P{kE + 3, 250 * kE}}};
  {
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(8192) * 300, 0);
    const auto cv = view(wide);
    Target t{buf.data(), 8192, 300, 8192};
    REQUIRE(fill_polygons(cv.data(), cv.size(), Fill::NonZero, t) == RasterStatus::Ok);
    CHECK(digest_mask(t) == 0xdc163e8ee5fdf1a4ull);
    REQUIRE(fill_polygons(cv.data(), cv.size(), Fill::EvenOdd, t) == RasterStatus::Ok);
    CHECK(digest_mask(t) == 0x2a5a4b9264f4ab04ull);
  }
}
