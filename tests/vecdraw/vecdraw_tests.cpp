// Tests for the vector path drawing (scope Z). Expected values come from an independent Python reference (exact integers: its own
// flatten, subpath state machine, stroker and a pixel-piece rasteriser written from docs/raster.md, plus its own FNV and
// generators) and are pasted here as constants. Every successful mask is also compared with the public sub-module APIs called in
// sequence by an oracle in this file. See docs/vecdraw.md.
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "vecdraw.h"

using namespace lumora;
using namespace lumora::vecdraw;
using lumora::outline::Command;
using lumora::outline::Op;
using lumora::outline::Point;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;
using Cmds = std::vector<Command>;

constexpr std::int32_t K = 1 << 24;

struct Lcg {
  u64 s;
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  std::int32_t rng(std::int32_t lo, std::int32_t hi) {  // inclusive
    return static_cast<std::int32_t>(lo + static_cast<i64>(next() % static_cast<u32>(static_cast<i64>(hi) - lo + 1)));
  }
};

struct FnvW {  // 32-bit little-endian words
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

u64 digest_mask(const std::uint8_t* data, u32 w, u32 h, u32 stride) {
  u64 d = 0xcbf29ce484222325ull;
  for (u32 y = 0; y < h; ++y)
    for (u32 x = 0; x < w; ++x) {
      d ^= data[static_cast<std::size_t>(y) * stride + x];
      d *= 0x100000001b3ull;
    }
  return d;
}

struct StyleArgs {
  std::int32_t hw, ml;
  int join, cap;
};

struct NamedCase {
  const char* name;
  Cmds cmds;
  std::int32_t tol;
  int mode;  // 0 fill NonZero, 1 fill EvenOdd, 2 stroke
  StyleArgs st;
  u32 w, h;
  int status;
  u64 digest;
};

struct CorpusExpect {
  const char* name;
  u64 seed;
  u32 count, maxn;
  std::int32_t lo, hi;
  u32 wmax, hmax;
  std::int32_t tlo, thi, hwmax;
  u64 digest;
  u32 status[14];
};


static const NamedCase kNamed[] = {
  {"square_fill_nz", {{Op::Move,{512,512},{0,0},{0,0}},{Op::Line,{3072,512},{0,0},{0,0}},{Op::Line,{3072,3072},{0,0},{0,0}},{Op::Line,{512,3072},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 0, {1,1,0,0}, 16, 16, 0, 13958939384220271153ull},
  {"square_fill_eo", {{Op::Move,{512,512},{0,0},{0,0}},{Op::Line,{3072,512},{0,0},{0,0}},{Op::Line,{3072,3072},{0,0},{0,0}},{Op::Line,{512,3072},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 1, {1,1,0,0}, 16, 16, 0, 13958939384220271153ull},
  {"square_stroke_miter", {{Op::Move,{512,512},{0,0},{0,0}},{Op::Line,{3072,512},{0,0},{0,0}},{Op::Line,{3072,3072},{0,0},{0,0}},{Op::Line,{512,3072},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 2, {200,4,1,0}, 16, 16, 0, 7111810553705395105ull},
  {"square_stroke_bevel_square_cap_open", {{Op::Move,{512,512},{0,0},{0,0}},{Op::Line,{3072,512},{0,0},{0,0}},{Op::Line,{3072,3072},{0,0},{0,0}},{Op::Line,{512,3072},{0,0},{0,0}}}, 8, 2, {200,4,0,1}, 16, 16, 0, 11842932093572364021ull},
  {"star_nz", {{Op::Move,{2048,200},{0,0},{0,0}},{Op::Line,{3400,3800},{0,0},{0,0}},{Op::Line,{300,1400},{0,0},{0,0}},{Op::Line,{3800,1400},{0,0},{0,0}},{Op::Line,{700,3800},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 0, {1,1,0,0}, 16, 16, 0, 15908096348695326086ull},
  {"star_eo", {{Op::Move,{2048,200},{0,0},{0,0}},{Op::Line,{3400,3800},{0,0},{0,0}},{Op::Line,{300,1400},{0,0},{0,0}},{Op::Line,{3800,1400},{0,0},{0,0}},{Op::Line,{700,3800},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 1, {1,1,0,0}, 16, 16, 0, 9839163011977070099ull},
  {"star_stroke", {{Op::Move,{2048,200},{0,0},{0,0}},{Op::Line,{3400,3800},{0,0},{0,0}},{Op::Line,{300,1400},{0,0},{0,0}},{Op::Line,{3800,1400},{0,0},{0,0}},{Op::Line,{700,3800},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 2, {128,3,1,1}, 16, 16, 0, 13207930545810700611ull},
  {"open_two_segments_fill_skipped", {{Op::Move,{100,100},{0,0},{0,0}},{Op::Line,{1500,1800},{0,0},{0,0}},{Op::Move,{3000,200},{0,0},{0,0}},{Op::Line,{3000,3800},{0,0},{0,0}}}, 8, 0, {1,1,0,0}, 16, 16, 0, 15567473145155401509ull},
  {"open_two_segments_stroke", {{Op::Move,{100,100},{0,0},{0,0}},{Op::Line,{1500,1800},{0,0},{0,0}},{Op::Move,{3000,200},{0,0},{0,0}},{Op::Line,{3000,3800},{0,0},{0,0}}}, 8, 2, {150,4,1,1}, 16, 16, 0, 4896933232219156683ull},
  {"nested_nz", {{Op::Move,{256,256},{0,0},{0,0}},{Op::Line,{3840,256},{0,0},{0,0}},{Op::Line,{3840,3840},{0,0},{0,0}},{Op::Line,{256,3840},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{1024,1024},{0,0},{0,0}},{Op::Line,{1024,3072},{0,0},{0,0}},{Op::Line,{3072,3072},{0,0},{0,0}},{Op::Line,{3072,1024},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 0, {1,1,0,0}, 16, 16, 0, 11947655345746221833ull},
  {"nested_eo", {{Op::Move,{256,256},{0,0},{0,0}},{Op::Line,{3840,256},{0,0},{0,0}},{Op::Line,{3840,3840},{0,0},{0,0}},{Op::Line,{256,3840},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{1024,1024},{0,0},{0,0}},{Op::Line,{1024,3072},{0,0},{0,0}},{Op::Line,{3072,3072},{0,0},{0,0}},{Op::Line,{3072,1024},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 8, 1, {1,1,0,0}, 16, 16, 0, 11947655345746221833ull},
  {"curves_fill", {{Op::Move,{256,2048},{0,0},{0,0}},{Op::Quad,{2048,-1500},{3800,2048},{0,0}},{Op::Cubic,{3000,5000},{1000,5000},{256,2048}},{Op::Close,{0,0},{0,0},{0,0}}}, 4, 0, {1,1,0,0}, 16, 16, 0, 1178287409026236845ull},
  {"curves_stroke_closed", {{Op::Move,{256,2048},{0,0},{0,0}},{Op::Quad,{2048,-1500},{3800,2048},{0,0}},{Op::Cubic,{3000,5000},{1000,5000},{256,2048}},{Op::Close,{0,0},{0,0},{0,0}}}, 4, 2, {96,6,1,0}, 16, 16, 0, 12495837272423558617ull},
  {"lone_move_clears", {{Op::Move,{5,5},{0,0},{0,0}}}, 1, 0, {1,1,0,0}, 9, 5, 0, 17814397539202711727ull},
  {"lone_move_stroke_clears", {{Op::Move,{5,5},{0,0},{0,0}}}, 1, 2, {10,4,1,1}, 9, 5, 0, 17814397539202711727ull},
  {"all_equal_square_cap_stroke", {{Op::Move,{512,512},{0,0},{0,0}},{Op::Line,{512,512},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {256,4,0,1}, 4, 4, 0, 9808874869469701221ull},
  {"two_point_closed_stroke", {{Op::Move,{256,256},{0,0},{0,0}},{Op::Line,{2048,2048},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {200,4,1,0}, 10, 10, 0, 16326351926101328042ull},
  {"outside_left_fill", {{Op::Move,{-3000,100},{0,0},{0,0}},{Op::Line,{900,100},{0,0},{0,0}},{Op::Line,{900,900},{0,0},{0,0}},{Op::Line,{-3000,900},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 0, {1,1,0,0}, 6, 6, 0, 3393425679255776185ull},
  {"outside_all_fill", {{Op::Move,{-3000,-3000},{0,0},{0,0}},{Op::Line,{-100,-3000},{0,0},{0,0}},{Op::Line,{-100,-100},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 0, {1,1,0,0}, 6, 6, 0, 10681679083240377973ull},
  {"huge_triangle_covers", {{Op::Move,{-16777216,-16777216},{0,0},{0,0}},{Op::Line,{16777216,-16777216},{0,0},{0,0}},{Op::Line,{0,16777216},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 0, {1,1,0,0}, 8, 8, 0, 9569108661638188517ull},
  {"extreme_stroke_boundary_ok", {{Op::Move,{-16777216,16252928},{0,0},{0,0}},{Op::Line,{16777216,16252928},{0,0},{0,0}}}, 1, 2, {524288,4,0,0}, 8, 8, 0, 13380826962402805797ull},
  {"extreme_stroke_boundary_bad", {{Op::Move,{-16777216,16252929},{0,0},{0,0}},{Op::Line,{16777216,16252929},{0,0},{0,0}}}, 1, 2, {524288,4,0,0}, 8, 8, 8, 0ull},
  {"extreme_stroke_negative_boundary_ok", {{Op::Move,{-16777216,-16252928},{0,0},{0,0}},{Op::Line,{16777216,-16252928},{0,0},{0,0}}}, 1, 2, {524288,4,0,0}, 8, 8, 0, 13380826962402805797ull},
  {"extreme_stroke_negative_boundary_bad", {{Op::Move,{-16777216,-16252929},{0,0},{0,0}},{Op::Line,{16777216,-16252929},{0,0},{0,0}}}, 1, 2, {524288,4,0,0}, 8, 8, 8, 0ull},
  {"extreme_square_cap_bad", {{Op::Move,{-16777216,0},{0,0},{0,0}},{Op::Line,{16777216,0},{0,0},{0,0}}}, 1, 2, {524288,4,0,1}, 8, 8, 8, 0ull},
  {"extreme_miter_tip_bad", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{16477216,0},{0,0},{0,0}},{Op::Line,{16477216,16177216},{0,0},{0,0}}}, 1, 2, {524288,64,1,0}, 8, 8, 8, 0ull},
  {"contour_1024_points_stroke", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}}}, 1, 2, {40,4,1,0}, 8, 8, 0, 5443496006789828093ull},
  {"contour_1025_points_stroke", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376346,8364084},{8351822,8376346},{8339560,8302774}}}, 1, 2, {40,4,1,0}, 8, 8, 12, 0ull},
  {"contour_1025_points_fill", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376346,8364084},{8351822,8376346},{8339560,8302774}}}, 1, 0, {1,1,0,0}, 8, 8, 0, 2994531734910074109ull},
  {"contour_1024_closed_stroke", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {40,4,0,0}, 8, 8, 0, 5443496006789828093ull},
  {"contour_1025_closed_stroke", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376346,8364084},{8351822,8376346},{8339560,8302774}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {40,4,0,0}, 8, 8, 12, 0ull},
  {"too_many_points_before_bad_point", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{16777216,16777216},{0,0},{0,0}},{Op::Move,{-16777216,-16777216},{0,0},{0,0}},{Op::Cubic,{16777216,16777216},{-16777216,-16777216},{16777216,16777216}},{Op::Cubic,{-16777216,16777216},{16777216,-16777216},{16777216,16777216}},{Op::Cubic,{16777216,16777216},{-16777216,-16777216},{16777216,16777216}},{Op::Cubic,{8376346,8364084},{8351822,8376346},{8339560,8302774}}}, 1, 2, {524288,4,1,1}, 8, 8, 12, 0ull},
  {"stroke_contours_4096_ok", {{Op::Move,{-8288608,-8388608},{0,0},{0,0}},{Op::Cubic,{8288608,8388608},{-8388608,8318608},{8388608,-8328608}},{Op::Move,{-8188608,-8388608},{0,0},{0,0}},{Op::Cubic,{8188608,8388608},{-8388608,8248608},{8388608,-8278608}},{Op::Move,{-8088608,-8388608},{0,0},{0,0}},{Op::Cubic,{8088608,8388608},{-8388608,8178608},{8388608,-8228608}},{Op::Move,{-7988608,-8388608},{0,0},{0,0}},{Op::Cubic,{7988608,8388608},{-8388608,8108608},{8388608,-8178608}},{Op::Move,{-7888608,-8388608},{0,0},{0,0}},{Op::Cubic,{7888608,8388608},{-8388608,8038608},{8388608,-8128608}},{Op::Move,{-7788608,-8388608},{0,0},{0,0}},{Op::Cubic,{7788608,8388608},{-8388608,7968608},{8388608,-8078608}},{Op::Move,{-7688608,-8388608},{0,0},{0,0}},{Op::Cubic,{7688608,8388608},{-8388608,7898608},{8388608,-8028608}},{Op::Move,{-7588608,-8388608},{0,0},{0,0}},{Op::Cubic,{7588608,8388608},{-8388608,7828608},{8388608,-7978608}},{Op::Move,{100,100},{0,0},{0,0}},{Op::Line,{100,2000},{0,0},{0,0}},{Op::Move,{400,100},{0,0},{0,0}},{Op::Line,{400,2000},{0,0},{0,0}},{Op::Move,{700,100},{0,0},{0,0}},{Op::Line,{700,2000},{0,0},{0,0}},{Op::Move,{1000,100},{0,0},{0,0}},{Op::Line,{1000,2000},{0,0},{0,0}},{Op::Move,{1300,100},{0,0},{0,0}},{Op::Line,{1300,2000},{0,0},{0,0}},{Op::Move,{1600,100},{0,0},{0,0}},{Op::Line,{1600,2000},{0,0},{0,0}},{Op::Move,{1900,100},{0,0},{0,0}},{Op::Line,{1900,2000},{0,0},{0,0}},{Op::Move,{2200,100},{0,0},{0,0}},{Op::Line,{2200,2000},{0,0},{0,0}}}, 1, 2, {40,4,0,0}, 8, 8, 0, 6067565649389956684ull},
  {"stroke_contours_4097_too_many", {{Op::Move,{-8288608,-8388608},{0,0},{0,0}},{Op::Cubic,{8288608,8388608},{-8388608,8318608},{8388608,-8328608}},{Op::Move,{-8188608,-8388608},{0,0},{0,0}},{Op::Cubic,{8188608,8388608},{-8388608,8248608},{8388608,-8278608}},{Op::Move,{-8088608,-8388608},{0,0},{0,0}},{Op::Cubic,{8088608,8388608},{-8388608,8178608},{8388608,-8228608}},{Op::Move,{-7988608,-8388608},{0,0},{0,0}},{Op::Cubic,{7988608,8388608},{-8388608,8108608},{8388608,-8178608}},{Op::Move,{-7888608,-8388608},{0,0},{0,0}},{Op::Cubic,{7888608,8388608},{-8388608,8038608},{8388608,-8128608}},{Op::Move,{-7788608,-8388608},{0,0},{0,0}},{Op::Cubic,{7788608,8388608},{-8388608,7968608},{8388608,-8078608}},{Op::Move,{-7688608,-8388608},{0,0},{0,0}},{Op::Cubic,{7688608,8388608},{-8388608,7898608},{8388608,-8028608}},{Op::Move,{-7588608,-8388608},{0,0},{0,0}},{Op::Cubic,{7588608,8388608},{-8388608,7828608},{8388608,-7978608}},{Op::Move,{100,100},{0,0},{0,0}},{Op::Line,{100,2000},{0,0},{0,0}},{Op::Move,{400,100},{0,0},{0,0}},{Op::Line,{400,2000},{0,0},{0,0}},{Op::Move,{700,100},{0,0},{0,0}},{Op::Line,{700,2000},{0,0},{0,0}},{Op::Move,{1000,100},{0,0},{0,0}},{Op::Line,{1000,2000},{0,0},{0,0}},{Op::Move,{1300,100},{0,0},{0,0}},{Op::Line,{1300,2000},{0,0},{0,0}},{Op::Move,{1600,100},{0,0},{0,0}},{Op::Line,{1600,2000},{0,0},{0,0}},{Op::Move,{1900,100},{0,0},{0,0}},{Op::Line,{1900,2000},{0,0},{0,0}},{Op::Move,{2200,100},{0,0},{0,0}},{Op::Line,{2200,2000},{0,0},{0,0}},{Op::Move,{2500,100},{0,0},{0,0}},{Op::Line,{2500,2000},{0,0},{0,0}}}, 1, 2, {40,4,0,0}, 8, 8, 13, 0ull},
  {"stroke_contours_4097_fill_ok", {{Op::Move,{-8288608,-8388608},{0,0},{0,0}},{Op::Cubic,{8288608,8388608},{-8388608,8318608},{8388608,-8328608}},{Op::Move,{-8188608,-8388608},{0,0},{0,0}},{Op::Cubic,{8188608,8388608},{-8388608,8248608},{8388608,-8278608}},{Op::Move,{-8088608,-8388608},{0,0},{0,0}},{Op::Cubic,{8088608,8388608},{-8388608,8178608},{8388608,-8228608}},{Op::Move,{-7988608,-8388608},{0,0},{0,0}},{Op::Cubic,{7988608,8388608},{-8388608,8108608},{8388608,-8178608}},{Op::Move,{-7888608,-8388608},{0,0},{0,0}},{Op::Cubic,{7888608,8388608},{-8388608,8038608},{8388608,-8128608}},{Op::Move,{-7788608,-8388608},{0,0},{0,0}},{Op::Cubic,{7788608,8388608},{-8388608,7968608},{8388608,-8078608}},{Op::Move,{-7688608,-8388608},{0,0},{0,0}},{Op::Cubic,{7688608,8388608},{-8388608,7898608},{8388608,-8028608}},{Op::Move,{-7588608,-8388608},{0,0},{0,0}},{Op::Cubic,{7588608,8388608},{-8388608,7828608},{8388608,-7978608}},{Op::Move,{100,100},{0,0},{0,0}},{Op::Line,{100,2000},{0,0},{0,0}},{Op::Move,{400,100},{0,0},{0,0}},{Op::Line,{400,2000},{0,0},{0,0}},{Op::Move,{700,100},{0,0},{0,0}},{Op::Line,{700,2000},{0,0},{0,0}},{Op::Move,{1000,100},{0,0},{0,0}},{Op::Line,{1000,2000},{0,0},{0,0}},{Op::Move,{1300,100},{0,0},{0,0}},{Op::Line,{1300,2000},{0,0},{0,0}},{Op::Move,{1600,100},{0,0},{0,0}},{Op::Line,{1600,2000},{0,0},{0,0}},{Op::Move,{1900,100},{0,0},{0,0}},{Op::Line,{1900,2000},{0,0},{0,0}},{Op::Move,{2200,100},{0,0},{0,0}},{Op::Line,{2200,2000},{0,0},{0,0}},{Op::Move,{2500,100},{0,0},{0,0}},{Op::Line,{2500,2000},{0,0},{0,0}}}, 1, 0, {1,1,0,0}, 8, 8, 0, 9569108661638188517ull},
  {"stroke_contours_4095_ok", {{Op::Move,{-8288608,-8388608},{0,0},{0,0}},{Op::Cubic,{8288608,8388608},{-8388608,8318608},{8388608,-8328608}},{Op::Move,{-8188608,-8388608},{0,0},{0,0}},{Op::Cubic,{8188608,8388608},{-8388608,8248608},{8388608,-8278608}},{Op::Move,{-8088608,-8388608},{0,0},{0,0}},{Op::Cubic,{8088608,8388608},{-8388608,8178608},{8388608,-8228608}},{Op::Move,{-7988608,-8388608},{0,0},{0,0}},{Op::Cubic,{7988608,8388608},{-8388608,8108608},{8388608,-8178608}},{Op::Move,{-7888608,-8388608},{0,0},{0,0}},{Op::Cubic,{7888608,8388608},{-8388608,8038608},{8388608,-8128608}},{Op::Move,{-7788608,-8388608},{0,0},{0,0}},{Op::Cubic,{7788608,8388608},{-8388608,7968608},{8388608,-8078608}},{Op::Move,{-7688608,-8388608},{0,0},{0,0}},{Op::Cubic,{7688608,8388608},{-8388608,7898608},{8388608,-8028608}},{Op::Move,{-7588608,-8388608},{0,0},{0,0}},{Op::Cubic,{7588608,8388608},{-8388608,7828608},{8388608,-7978608}},{Op::Move,{100,100},{0,0},{0,0}},{Op::Line,{100,2000},{0,0},{0,0}},{Op::Move,{400,100},{0,0},{0,0}},{Op::Line,{400,2000},{0,0},{0,0}},{Op::Move,{700,100},{0,0},{0,0}},{Op::Line,{700,2000},{0,0},{0,0}},{Op::Move,{1000,100},{0,0},{0,0}},{Op::Line,{1000,2000},{0,0},{0,0}},{Op::Move,{1300,100},{0,0},{0,0}},{Op::Line,{1300,2000},{0,0},{0,0}},{Op::Move,{1600,100},{0,0},{0,0}},{Op::Line,{1600,2000},{0,0},{0,0}},{Op::Move,{1900,100},{0,0},{0,0}},{Op::Line,{1900,2000},{0,0},{0,0}}}, 1, 2, {40,4,0,0}, 8, 8, 0, 6067565649389956684ull},
  {"three_loops_too_many_contours_control", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {1000,4,0,0}, 8, 8, 13, 0ull},
  {"y_only_bad_vertex_first_of_three_loops", {{Op::Move,{-8388608,-100},{0,0},{0,0}},{Op::Cubic,{8388608,16777116},{-8388608,-100},{8388608,16777116}},{Op::Cubic,{-8388608,16777116},{8388608,-100},{8388608,16777116}},{Op::Cubic,{8388608,16777116},{-8388608,-100},{8388608,16777116}},{Op::Cubic,{8376442,16752784},{8352110,16764950},{8339944,16691954}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {1000,4,0,0}, 8, 8, 8, 0ull},
  {"x_only_bad_vertex_first_of_three_loops", {{Op::Move,{-100,-8388608},{0,0},{0,0}},{Op::Cubic,{16777116,8388608},{-100,-8388608},{16777116,8388608}},{Op::Cubic,{16777116,-8388608},{-100,8388608},{16777116,8388608}},{Op::Cubic,{16777116,8388608},{-100,-8388608},{16777116,8388608}},{Op::Cubic,{16752784,8376442},{16764950,8352110},{16691954,8339944}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,-8388608},{-8388608,8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8364276,8376442},{8376442,8352110},{8303446,8339944}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,-8388608},{-8388608,8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8364276,8376442},{8376442,8352110},{8303446,8339944}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {1000,4,0,0}, 8, 8, 8, 0ull},
  {"y_only_bad_vertex_second_of_three_loops", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-100},{0,0},{0,0}},{Op::Cubic,{8388608,16777116},{-8388608,-100},{8388608,16777116}},{Op::Cubic,{-8388608,16777116},{8388608,-100},{8388608,16777116}},{Op::Cubic,{8388608,16777116},{-8388608,-100},{8388608,16777116}},{Op::Cubic,{8376442,16752784},{8352110,16764950},{8339944,16691954}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{-8388608,8388608},{8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8376442,8364276},{8352110,8376442},{8339944,8303446}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {1000,4,0,0}, 8, 8, 8, 0ull},
  {"x_only_bad_vertex_second_of_three_loops", {{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,-8388608},{-8388608,8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8364276,8376442},{8376442,8352110},{8303446,8339944}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-100,-8388608},{0,0},{0,0}},{Op::Cubic,{16777116,8388608},{-100,-8388608},{16777116,8388608}},{Op::Cubic,{16777116,-8388608},{-100,8388608},{16777116,8388608}},{Op::Cubic,{16777116,8388608},{-100,-8388608},{16777116,8388608}},{Op::Cubic,{16752784,8376442},{16764950,8352110},{16691954,8339944}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8388608,-8388608},{-8388608,8388608},{8388608,8388608}},{Op::Cubic,{8388608,8388608},{-8388608,-8388608},{8388608,8388608}},{Op::Cubic,{8364276,8376442},{8376442,8352110},{8303446,8339944}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, 2, {1000,4,0,0}, 8, 8, 8, 0ull},
  {"bad_point_wins_over_contour_count_in_one_contour", {{Op::Move,{-8288608,-8388608},{0,0},{0,0}},{Op::Cubic,{8288608,8388608},{-8388608,8318608},{8388608,-8328608}},{Op::Move,{-8188608,-8388608},{0,0},{0,0}},{Op::Cubic,{8188608,8388608},{-8388608,8248608},{8388608,-8278608}},{Op::Move,{-8088608,-8388608},{0,0},{0,0}},{Op::Cubic,{8088608,8388608},{-8388608,8178608},{8388608,-8228608}},{Op::Move,{-7988608,-8388608},{0,0},{0,0}},{Op::Cubic,{7988608,8388608},{-8388608,8108608},{8388608,-8178608}},{Op::Move,{-7888608,-8388608},{0,0},{0,0}},{Op::Cubic,{7888608,8388608},{-8388608,8038608},{8388608,-8128608}},{Op::Move,{-7788608,-8388608},{0,0},{0,0}},{Op::Cubic,{7788608,8388608},{-8388608,7968608},{8388608,-8078608}},{Op::Move,{-7688608,-8388608},{0,0},{0,0}},{Op::Cubic,{7688608,8388608},{-8388608,7898608},{8388608,-8028608}},{Op::Move,{-7588608,-8388608},{0,0},{0,0}},{Op::Cubic,{7588608,8388608},{-8388608,7828608},{8388608,-7978608}},{Op::Move,{-16777216,-16777216},{0,0},{0,0}},{Op::Cubic,{16777216,16777216},{-16777216,16777216},{16777216,-16777216}}}, 1, 2, {40,4,0,0}, 8, 8, 8, 0ull},
  {"too_many_contours_after_bad_point_free_contours", {{Op::Move,{-8288608,-8388608},{0,0},{0,0}},{Op::Cubic,{8288608,8388608},{-8388608,8318608},{8388608,-8328608}},{Op::Move,{-8188608,-8388608},{0,0},{0,0}},{Op::Cubic,{8188608,8388608},{-8388608,8248608},{8388608,-8278608}},{Op::Move,{-8088608,-8388608},{0,0},{0,0}},{Op::Cubic,{8088608,8388608},{-8388608,8178608},{8388608,-8228608}},{Op::Move,{-7988608,-8388608},{0,0},{0,0}},{Op::Cubic,{7988608,8388608},{-8388608,8108608},{8388608,-8178608}},{Op::Move,{-7888608,-8388608},{0,0},{0,0}},{Op::Cubic,{7888608,8388608},{-8388608,8038608},{8388608,-8128608}},{Op::Move,{-7788608,-8388608},{0,0},{0,0}},{Op::Cubic,{7788608,8388608},{-8388608,7968608},{8388608,-8078608}},{Op::Move,{-7688608,-8388608},{0,0},{0,0}},{Op::Cubic,{7688608,8388608},{-8388608,7898608},{8388608,-8028608}},{Op::Move,{-7588608,-8388608},{0,0},{0,0}},{Op::Cubic,{7588608,8388608},{-8388608,7828608},{8388608,-7978608}},{Op::Move,{-8388608,-8388608},{0,0},{0,0}},{Op::Cubic,{8388608,8388608},{-8388608,8388608},{8388608,-8388608}}}, 1, 2, {40,4,0,0}, 8, 8, 13, 0ull},
};
static const CorpusExpect kCorpora[] = {
  {"A", 0xA1ull, 500, 10, -1000, 5000, 24, 24, 1, 100, 700, 17833531100346150488ull, {500,0,0,0,0,0,0,0,0,0,0,0,0,0}},
  {"B", 0xB2ull, 250, 8, -16777216, 16777216, 8, 8, 1, 65536, 524288, 13625539109641321964ull, {243,0,0,0,0,0,0,0,7,0,0,0,0,0}},
  {"C", 0xC3ull, 250, 24, -30000, 30000, 40, 40, 1, 40, 1500, 6778190588094767078ull, {250,0,0,0,0,0,0,0,0,0,0,0,0,0}},
};
static const u64 kExhaustiveDigest = 1802659142287457003ull; static const u32 kExhaustiveCases = 14460;
static const u32 kExhaustiveStatus[14] = {2460,0,0,0,0,0,0,0,0,12000,0,0,0,0};

stroke::Style make_style(const StyleArgs& a) {
  return stroke::Style{a.hw, a.ml, static_cast<stroke::Join>(a.join), static_cast<stroke::Cap>(a.cap), false};
}

// Oracle: the sub-module APIs called in sequence with std::vector storage (the module under test uses a carved work area).
// Returns false when a sub-module step fails or a limit is exceeded; `mask` is filled on success.
bool oracle(const Cmds& cmds, std::int32_t tol, int mode, const StyleArgs& sa, u32 w, u32 h, std::vector<std::uint8_t>& mask) {
  const u32 n = static_cast<u32>(cmds.size());
  std::vector<Point> pts(outline::point_bound(n));
  std::vector<u32> ends(outline::contour_bound(n));
  std::vector<std::uint8_t> closed(outline::contour_bound(n));
  u32 cc = 0, pc = 0;
  if (outline::build_outline(cmds.data(), n, tol, pts.data(), static_cast<u32>(pts.size()), ends.data(), static_cast<u32>(ends.size()),
                             closed.data(), static_cast<u32>(closed.size()), &cc, &pc) != outline::Status::Ok)
    return false;
  std::vector<std::vector<raster::P>> polys;
  if (mode < 2) {
    u32 b = 0;
    for (u32 i = 0; i < cc; ++i) {
      if (ends[i] - b >= 3) {
        polys.emplace_back();
        for (u32 k = b; k < ends[i]; ++k) polys.back().push_back(raster::P{pts[k].x, pts[k].y});
      }
      b = ends[i];
    }
  } else {
    u32 b = 0;
    for (u32 i = 0; i < cc; ++i) {
      const u32 m = ends[i] - b;
      if (m > stroke::kMaxPoints) return false;
      std::vector<stroke::Point> in(m);
      for (u32 k = 0; k < m; ++k) in[k] = stroke::Point{pts[b + k].x, pts[b + k].y};
      b = ends[i];
      stroke::Style st = make_style(sa);
      st.closed = closed[i] != 0;
      std::vector<stroke::Point> sp(stroke::point_bound(m, st.closed));
      std::vector<u32> se(stroke::contour_bound(m, st.closed));
      u32 scc = 0, spc = 0;
      if (stroke::stroke_polyline(in.data(), m, st, sp.data(), static_cast<u32>(sp.size()), se.data(), static_cast<u32>(se.size()), &scc,
                                  &spc) != stroke::Status::Ok)
        return false;
      u32 sb = 0;
      for (u32 c = 0; c < scc; ++c) {
        std::vector<raster::P> poly;
        for (u32 k = sb; k < se[c]; ++k) poly.push_back(raster::P{sp[k].x, sp[k].y});
        sb = se[c];
        polys.push_back(poly);
      }
    }
  }
  std::vector<raster::Contour> desc;
  for (auto& p : polys) desc.push_back(raster::Contour{p.data(), p.size()});
  mask.assign(static_cast<std::size_t>(w) * h, 0xEE);
  const raster::Target t{mask.data(), w, h, w};
  const raster::Fill rule = (mode == 1) ? raster::Fill::EvenOdd : raster::Fill::NonZero;
  // The oracle reports the same limit failures as the module (data dependent), by the same rules.
  if (desc.size() > raster::kMaxContours) return false;
  for (auto& p : polys)
    for (auto& q : p)
      if (q.x < -raster::kMaxCoord || q.x > raster::kMaxCoord || q.y < -raster::kMaxCoord || q.y > raster::kMaxCoord) return false;
  return raster::fill_polygons(desc.data(), desc.size(), rule, t) == raster::RasterStatus::Ok;
}

constexpr std::uint8_t kSent = 0xA5;

struct Result {
  Status st;
  u64 digest;  // 0 on error
};

Status call(const Cmds& cmds, std::uint32_t n, std::int32_t tol, int mode, const StyleArgs& sa, const raster::Target& t, void* work,
            std::size_t bytes) {
  if (mode < 2) return fill_path(cmds.data(), n, tol, mode == 1 ? raster::Fill::EvenOdd : raster::Fill::NonZero, t, work, bytes);
  return stroke_path(cmds.data(), n, tol, make_style(sa), t, work, bytes);
}

// Exact-size work (so the sanitizer sees any overrun), a padded target with a sentinel in every byte. Checks that an error leaves
// the target untouched, that the padding is never written, and (on success) the oracle mask.
Result run_case(const Cmds& cmds, std::int32_t tol, int mode, const StyleArgs& sa, u32 w, u32 h, bool use_oracle = true) {
  const u32 n = static_cast<u32>(cmds.size());
  const std::size_t need = work_bytes_needed(n, mode == 2);
  std::vector<u64> work(need / 8);
  const u32 stride = w + 3;
  std::vector<std::uint8_t> tgt(static_cast<std::size_t>(stride) * h, kSent);
  const raster::Target t{tgt.data(), w, h, stride};
  const Status s = call(cmds, n, tol, mode, sa, t, work.data(), need);
  if (s != Status::Ok) {
    for (std::uint8_t b : tgt) REQUIRE(b == kSent);
    return {s, 0};
  }
  for (u32 y = 0; y < h; ++y)
    for (u32 x = w; x < stride; ++x) REQUIRE(tgt[static_cast<std::size_t>(y) * stride + x] == kSent);
  if (use_oracle) {
    std::vector<std::uint8_t> m;
    REQUIRE(oracle(cmds, tol, mode, sa, w, h, m));
    for (u32 y = 0; y < h; ++y)
      for (u32 x = 0; x < w; ++x) REQUIRE(tgt[static_cast<std::size_t>(y) * stride + x] == m[static_cast<std::size_t>(y) * w + x]);
  }
  return {s, digest_mask(tgt.data(), w, h, stride)};
}

Cmds mkrand(Lcg& r, u32 maxn, std::int32_t lo, std::int32_t hi) {
  const std::int32_t n = r.rng(1, static_cast<std::int32_t>(maxn));
  Cmds cmds;
  bool closed = true;
  for (std::int32_t i = 0; i < n; ++i) {
    Op op;
    if (closed) {
      op = Op::Move;
    } else {
      const std::int32_t k = r.rng(0, 9);
      op = k < 2 ? Op::Move : (k == 2 || k == 3 || k == 4 || k == 7 || k == 8) ? Op::Line : k == 5 ? Op::Quad : k == 6 ? Op::Cubic : Op::Close;
    }
    const int np = op == Op::Move ? 1 : op == Op::Line ? 1 : op == Op::Quad ? 2 : op == Op::Cubic ? 3 : 0;
    Command c{op, {0, 0}, {0, 0}, {0, 0}};
    Point* f[3] = {&c.a, &c.b, &c.c};
    for (int j = 0; j < np; ++j) {
      f[j]->x = r.rng(lo, hi);
      f[j]->y = r.rng(lo, hi);
    }
    cmds.push_back(c);
    closed = (op == Op::Close);
  }
  return cmds;
}

constexpr std::size_t r8(std::size_t v) { return (v + 7) / 8 * 8; }

Cmds line_path(u32 n) {  // valid path of n commands
  Cmds c;
  c.push_back(Command{Op::Move, {256, 256}, {0, 0}, {0, 0}});
  for (u32 i = 1; i < n; ++i) c.push_back(Command{Op::Line, {static_cast<std::int32_t>(256 + 40 * i), static_cast<std::int32_t>(256 + ((i & 1u) ? 900 : 100))}, {0, 0}, {0, 0}});
  return c;
}

}  // namespace

TEST_CASE("status_text names every status", "[vecdraw]") {
  const char* names[] = {"ok",         "null buffer",   "bad size",      "bad count",     "bad tolerance",
                         "bad fill",   "bad style",     "bad command",   "bad point",     "bad path",
                         "work too small", "overlap",   "too many points", "too many contours"};
  for (int i = 0; i < 14; ++i) REQUIRE(std::string_view(status_text(static_cast<Status>(i))) == names[i]);
  REQUIRE(std::string_view(status_text(static_cast<Status>(14))) == "unknown");
  REQUIRE(std::string_view(status_text(static_cast<Status>(255))) == "unknown");
}

TEST_CASE("work_bytes_needed matches the derived formula", "[vecdraw]") {
  REQUIRE(work_bytes_needed(0, false) == 0);
  REQUIRE(work_bytes_needed(0, true) == 0);
  REQUIRE(work_bytes_needed(65, false) == 0);
  REQUIRE(work_bytes_needed(65, true) == 0);
  REQUIRE(work_bytes_needed(1000, true) == 0);
  // Independent closed forms (written from docs/vecdraw.md, not from the module).
  for (u32 n = 1; n <= 64; ++n) {
    const std::size_t fill = 2048u * n + r8(4u * n) + r8(n) + 2048u * n + 16u * n;
    const std::size_t strk = 2048u * n + r8(4u * n) + r8(n) + 8192 + 65536 + 8192 + 131072 + 65536;
    REQUIRE(work_bytes_needed(n, false) == fill);
    REQUIRE(work_bytes_needed(n, true) == strk);
    REQUIRE(work_bytes_needed(n, false) % 8 == 0);
    REQUIRE(work_bytes_needed(n, true) % 8 == 0);
  }
  // Constants from the reference script.
  REQUIRE(work_bytes_needed(1, false) == 4096 + 8 + 8 + 16);
  REQUIRE(work_bytes_needed(1, true) == 2048 + 8 + 8 + 278528);
  REQUIRE(work_bytes_needed(64, false) == 4096 * 64 + 256 + 64 + 1024);
  REQUIRE(work_bytes_needed(64, true) == 2048 * 64 + 256 + 64 + 278528);
}

TEST_CASE("named cases match the independent reference", "[vecdraw]") {
  for (const auto& nc : kNamed) {
    INFO(nc.name);
    const Result r = run_case(nc.cmds, nc.tol, nc.mode, nc.st, nc.w, nc.h);
    REQUIRE(static_cast<int>(r.st) == nc.status);
    REQUIRE(r.digest == nc.digest);
  }
}

TEST_CASE("random corpora match the independent reference", "[vecdraw]") {
  for (const auto& ce : kCorpora) {
    INFO(ce.name);
    Lcg r{ce.seed};
    FnvW f;
    u32 counts[14] = {};
    for (u32 i = 0; i < ce.count; ++i) {
      const Cmds cm = mkrand(r, ce.maxn, ce.lo, ce.hi);
      const u32 w = static_cast<u32>(r.rng(1, static_cast<std::int32_t>(ce.wmax)));
      const u32 h = static_cast<u32>(r.rng(1, static_cast<std::int32_t>(ce.hmax)));
      const std::int32_t tol = r.rng(ce.tlo, ce.thi);
      const int mode = r.rng(0, 2);
      StyleArgs sa{r.rng(1, ce.hwmax), r.rng(1, 8), r.rng(0, 1), r.rng(0, 1)};
      const Result res = run_case(cm, tol, mode, sa, w, h);
      ++counts[static_cast<int>(res.st)];
      f.word(static_cast<u32>(res.st));
      f.word(static_cast<u32>(res.digest & 0xFFFFFFFFu));
      f.word(static_cast<u32>(res.digest >> 32));
    }
    REQUIRE(f.h == ce.digest);
    for (int s = 0; s < 14; ++s) REQUIRE(counts[s] == ce.status[s]);
  }
}

TEST_CASE("exhaustive small paths match the independent reference", "[vecdraw]") {
  const Op all[5] = {Op::Move, Op::Line, Op::Quad, Op::Cubic, Op::Close};
  const Point pre[3][3] = {{{0, 0}, {0, 0}, {0, 0}}, {{8, 0}, {8, 8}, {0, 8}}, {{0, 0}, {8, 0}, {8, 8}}};
  const struct {
    int mode;
    StyleArgs st;
  } modes[4] = {{0, {1, 1, 0, 0}}, {1, {1, 1, 0, 0}}, {2, {300, 4, 1, 1}}, {2, {128, 4, 0, 0}}};
  FnvW f;
  u32 counts[14] = {};
  u32 total = 0;
  for (int ln = 1; ln <= 3; ++ln) {
    int ops_total = 1, pats_total = 1;
    for (int i = 0; i < ln; ++i) {
      ops_total *= 5;
      pats_total *= 3;
    }
    for (int oi = 0; oi < ops_total; ++oi)
      for (int pi = 0; pi < pats_total; ++pi) {
        Cmds cm;
        int ov = oi, pv = pi;
        int opsel[3], patsel[3];
        for (int i = ln - 1; i >= 0; --i) {  // itertools.product order: the last position varies fastest
          opsel[i] = ov % 5;
          ov /= 5;
          patsel[i] = pv % 3;
          pv /= 3;
        }
        for (int i = 0; i < ln; ++i) {
          auto sc = [](Point p) { return Point{256 + 400 * p.x, 256 + 400 * p.y}; };
          Command c{all[opsel[i]], {0, 0}, {0, 0}, {0, 0}};
          const Point* pp = pre[patsel[i]];
          if (c.op != Op::Close) c.a = sc(pp[0]);
          if (c.op == Op::Quad || c.op == Op::Cubic) c.b = sc(pp[1]);
          if (c.op == Op::Cubic) c.c = sc(pp[2]);
          cm.push_back(c);
        }
        for (const auto& m : modes) {
          const Result r = run_case(cm, 3, m.mode, m.st, 16, 16);
          ++counts[static_cast<int>(r.st)];
          ++total;
          f.word(static_cast<u32>(r.st));
          f.word(static_cast<u32>(r.digest & 0xFFFFFFFFu));
          f.word(static_cast<u32>(r.digest >> 32));
        }
      }
  }
  REQUIRE(total == kExhaustiveCases);
  REQUIRE(f.h == kExhaustiveDigest);
  for (int s = 0; s < 14; ++s) REQUIRE(counts[s] == kExhaustiveStatus[s]);
}

TEST_CASE("unused command fields and style.closed are ignored", "[vecdraw]") {
  Cmds a = {Command{Op::Move, {256, 256}, {0, 0}, {0, 0}}, Command{Op::Line, {3000, 300}, {0, 0}, {0, 0}},
            Command{Op::Quad, {3500, 3000}, {300, 3000}, {0, 0}}, Command{Op::Close, {0, 0}, {0, 0}, {0, 0}}};
  Cmds b = a;
  b[0].b = {0x7FFFFFFF, -0x7FFFFFFF};
  b[0].c = {123456789, 987654321};
  b[1].b = {K + 5, K + 5};
  b[1].c = {-K - 5, 0x7FFFFFFF};
  b[2].c = {0x7FFFFFFF, 0x7FFFFFFF};
  b[3].a = {0x7FFFFFFF, 0x7FFFFFFF};
  b[3].b = {-0x7FFFFFFF, 0};
  b[3].c = {K + 1, K + 1};
  for (int mode = 0; mode < 3; ++mode) {
    const StyleArgs sa{150, 4, 1, 1};
    const Result ra = run_case(a, 5, mode, sa, 16, 16);
    const Result rb = run_case(b, 5, mode, sa, 16, 16);
    REQUIRE(ra.st == Status::Ok);
    REQUIRE(rb.st == Status::Ok);
    REQUIRE(ra.digest == rb.digest);
  }
  // style.closed is replaced by the contour flag.
  const Cmds open_path = {Command{Op::Move, {256, 256}, {0, 0}, {0, 0}}, Command{Op::Line, {3000, 300}, {0, 0}, {0, 0}},
                          Command{Op::Line, {3000, 3000}, {0, 0}, {0, 0}}};
  for (int closed_flag = 0; closed_flag < 2; ++closed_flag) {
    for (const Cmds* cp : {&open_path, static_cast<const Cmds*>(&a)}) {
      const std::size_t need = work_bytes_needed(static_cast<u32>(cp->size()), true);
      std::vector<u64> work(need / 8);
      std::vector<std::uint8_t> m0(16 * 16), m1(16 * 16);
      stroke::Style s0{150, 4, stroke::Join::Miter, stroke::Cap::Square, false};
      stroke::Style s1 = s0;
      s1.closed = closed_flag != 0;
      REQUIRE(stroke_path(cp->data(), static_cast<u32>(cp->size()), 5, s0, raster::Target{m0.data(), 16, 16, 16}, work.data(), need) == Status::Ok);
      REQUIRE(stroke_path(cp->data(), static_cast<u32>(cp->size()), 5, s1, raster::Target{m1.data(), 16, 16, 16}, work.data(), need) == Status::Ok);
      REQUIRE(m0 == m1);
    }
  }
}

TEST_CASE("check order over every combination of faults", "[vecdraw]") {
  const Status order[9] = {Status::BadSize, Status::BadCount, Status::BadTolerance, Status::Ok /*fill or style*/, Status::BadCommand,
                           Status::BadPoint, Status::BadPath, Status::WorkTooSmall, Status::Overlap};
  std::vector<u64> arena(1 << 17);
  const std::size_t wbase = 1000;  // work offset in words
  for (int mode = 0; mode < 2; ++mode) {  // 0 fill, 1 stroke
    const bool is_stroke = mode == 1;
    const std::size_t need = work_bytes_needed(4, is_stroke);
    for (int nulls = 0; nulls < 4; ++nulls)
      for (int mask = 0; mask < 512; ++mask) {
        Cmds cmds = {Command{Op::Move, {256, 256}, {0, 0}, {0, 0}}, Command{Op::Line, {3000, 300}, {0, 0}, {0, 0}},
                     Command{Op::Line, {3000, 3000}, {0, 0}, {0, 0}}, Command{Op::Line, {300, 3000}, {0, 0}, {0, 0}}};
        std::uint32_t count = 4;
        std::int32_t tol = 5;
        raster::Target t{nullptr, 8, 4, 11};
        std::vector<std::uint8_t> own(11 * 4, kSent);
        t.data = own.data();
        raster::Fill rule = raster::Fill::NonZero;
        stroke::Style st{100, 4, stroke::Join::Miter, stroke::Cap::Butt, false};
        std::size_t bytes = need;
        std::fill(arena.begin(), arena.end(), 0x5A5A5A5A5A5A5A5Aull);
        void* work = &arena[wbase];
        if (mask & 1) t.width = 0;
        if (mask & 2) count = 65;
        if (mask & 4) tol = 0;
        if (mask & 8) {
          if (is_stroke) st.half_width = 0;
          else rule = static_cast<raster::Fill>(7);
        }
        if (mask & 16) cmds[3].op = static_cast<Op>(9);
        if (mask & 32) cmds[2].a.x = K + 1;
        if (mask & 64) cmds[0].op = Op::Line;
        if (mask & 128) bytes = need - 1;
        std::uint8_t* tdata = own.data();
        if (mask & 256) tdata = reinterpret_cast<std::uint8_t*>(work) + 64;
        t.data = tdata;
        const Command* cptr = cmds.data();
        if (nulls == 1) cptr = nullptr;
        if (nulls == 2) t.data = nullptr;
        if (nulls == 3) work = nullptr;
        Status expect = Status::Ok;
        if (nulls != 0) {
          expect = Status::NullBuffer;
        } else {
          for (int b = 0; b < 9; ++b)
            if (mask & (1 << b)) {
              expect = b == 3 ? (is_stroke ? Status::BadStyle : Status::BadFill) : order[b];
              break;
            }
        }
        const std::vector<u64> before = arena;
        const std::vector<std::uint8_t> own_before = own;
        Status s;
        if (is_stroke) s = stroke_path(cptr, count, tol, st, t, work, bytes);
        else s = fill_path(cptr, count, tol, rule, t, work, bytes);
        INFO("mode " << mode << " nulls " << nulls << " mask " << mask);
        REQUIRE(s == expect);
        if (expect != Status::Ok) {
          REQUIRE(own == own_before);
          REQUIRE(arena == before);  // nothing in the arena (work, commands, overlapped target) is written on a check error
        }
      }
  }
}

TEST_CASE("work size boundary, guard bytes and alignment", "[vecdraw]") {
  for (int mode = 0; mode < 3; mode += 2) {
    for (u32 n : {1u, 2u, 7u, 64u}) {
      const Cmds cmds = line_path(n);
      const std::size_t need = work_bytes_needed(n, mode == 2);
      const std::size_t guard = 64;  // words
      std::vector<u64> buf(guard + need / 8 + 8 + guard, 0xC3C3C3C3C3C3C3C3ull);
      void* work = &buf[guard];
      const StyleArgs sa{100, 4, 1, 0};
      std::vector<std::uint8_t> tgt(12 * 5, kSent);
      const raster::Target t{tgt.data(), 12, 5, 12};
      // exactly the needed bytes: success, and nothing outside the needed bytes is written
      REQUIRE(call(cmds, n, 3, mode, sa, t, work, need) == Status::Ok);
      for (std::size_t i = 0; i < guard; ++i) REQUIRE(buf[i] == 0xC3C3C3C3C3C3C3C3ull);
      for (std::size_t i = guard + need / 8; i < buf.size(); ++i) REQUIRE(buf[i] == 0xC3C3C3C3C3C3C3C3ull);
      // more than needed: the surplus is untouched
      std::fill(buf.begin(), buf.end(), 0xC3C3C3C3C3C3C3C3ull);
      REQUIRE(call(cmds, n, 3, mode, sa, t, work, need + 8) == Status::Ok);
      for (std::size_t i = guard + need / 8; i < buf.size(); ++i) REQUIRE(buf[i] == 0xC3C3C3C3C3C3C3C3ull);
      // below the need by 1, 7, 8 and the whole size: WorkTooSmall and the target is untouched
      std::fill(tgt.begin(), tgt.end(), kSent);
      for (std::size_t less : {std::size_t{1}, std::size_t{7}, std::size_t{8}, std::size_t{4096}, need}) {
        REQUIRE(call(cmds, n, 3, mode, sa, t, work, need - less) == Status::WorkTooSmall);
        for (std::uint8_t b : tgt) REQUIRE(b == kSent);
      }
      // misaligned work with plenty of bytes
      std::uint8_t* base = reinterpret_cast<std::uint8_t*>(work);
      for (std::size_t off : {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{7}}) {
        REQUIRE(call(cmds, n, 3, mode, sa, t, base + off, need + 8) == Status::WorkTooSmall);
        for (std::uint8_t b : tgt) REQUIRE(b == kSent);
      }
      // a smaller command count needs less: the bound grows with n
      if (n > 1) REQUIRE(work_bytes_needed(n, mode == 2) > work_bytes_needed(n - 1, mode == 2));
    }
  }
  // The stroke bound is always above the fill bound.
  for (u32 n = 1; n <= 64; ++n) REQUIRE(work_bytes_needed(n, true) > work_bytes_needed(n, false));
}

TEST_CASE("overlap of commands, target and work", "[vecdraw]") {
  const std::size_t kArenaWords = 1 << 17;
  for (int mode = 0; mode < 3; mode += 2) {
    const Cmds cmds = line_path(4);
    const std::size_t ce = cmds.size() * sizeof(Command);
    const std::size_t need = work_bytes_needed(4, mode == 2);
    const std::size_t B = need + 64;  // supplied work bytes: more than needed, the whole supplied size is the extent
    const std::size_t W0 = 8 * 20000;  // work offset in bytes
    const u32 w = 8, h = 4, stride = 11;
    const std::size_t T = static_cast<std::size_t>(stride) * (h - 1) + w;
    const StyleArgs sa{100, 4, 1, 0};
    std::vector<u64> arena(kArenaWords);
    std::uint8_t* base = reinterpret_cast<std::uint8_t*>(arena.data());
    auto reset = [&] {
      for (std::size_t i = 0; i < arena.size(); ++i) arena[i] = 0x7777777777777777ull;
    };
    // place the commands at `coff` (bytes), the target at `toff`
    auto attempt = [&](std::size_t coff, std::size_t toff, std::size_t work_off) -> Status {
      reset();
      std::memcpy(base + coff, cmds.data(), ce);
      const std::vector<std::uint8_t> snap(base, base + arena.size() * 8);
      const raster::Target t{base + toff, w, h, stride};
      Status s;
      if (mode < 2) s = fill_path(reinterpret_cast<const Command*>(base + coff), 4, 3, raster::Fill::NonZero, t, base + work_off, B);
      else s = stroke_path(reinterpret_cast<const Command*>(base + coff), 4, 3, make_style(sa), t, base + work_off, B);
      if (s == Status::Overlap) {
        for (std::size_t i = 0; i < snap.size(); ++i) REQUIRE(base[i] == snap[i]);
      } else {
        REQUIRE(s == Status::Ok);
        // commands are never written; outside work and the target rows nothing changes
        for (std::size_t i = 0; i < snap.size(); ++i) {
          const bool in_work = i >= work_off && i < work_off + B;
          bool in_t = false;
          for (u32 y = 0; y < h; ++y)
            if (i >= toff + static_cast<std::size_t>(y) * stride && i < toff + static_cast<std::size_t>(y) * stride + w) in_t = true;
          if (!in_work && !in_t) REQUIRE(base[i] == snap[i]);
        }
      }
      return s;
    };
    const std::size_t C0 = 8 * 60000;  // commands far from the work
    const std::size_t T0 = 8 * 90000;
    // target against the work
    REQUIRE(attempt(C0, W0 + B, W0) == Status::Ok);
    REQUIRE(attempt(C0, W0 + B - 1, W0) == Status::Overlap);
    REQUIRE(attempt(C0, W0 + need + 32, W0) == Status::Overlap);  // inside the supplied bytes, beyond the needed ones
    REQUIRE(attempt(C0, W0 - T, W0) == Status::Ok);
    REQUIRE(attempt(C0, W0 - T + 1, W0) == Status::Overlap);
    REQUIRE(attempt(C0, W0, W0) == Status::Overlap);
    // commands against the work
    REQUIRE(attempt(W0 + B, T0, W0) == Status::Ok);
    REQUIRE(attempt(W0 + B - 4, T0, W0) == Status::Overlap);
    REQUIRE(attempt(W0 - ce, T0, W0) == Status::Ok);
    REQUIRE(attempt(W0 - ce + 4, T0, W0) == Status::Overlap);
    REQUIRE(attempt(W0 + 8, T0, W0) == Status::Overlap);
    // commands against the target
    REQUIRE(attempt(C0, C0 + ce, W0) == Status::Ok);
    REQUIRE(attempt(C0, C0 + ce - 1, W0) == Status::Overlap);
    REQUIRE(attempt(C0, C0 - T, W0) == Status::Ok);
    REQUIRE(attempt(C0, C0 - T + 1, W0) == Status::Overlap);
    REQUIRE(attempt(C0, C0 + 3, W0) == Status::Overlap);
  }
}

TEST_CASE("overlap extent of the target is stride*(height-1)+width", "[vecdraw]") {
  // A command array placed in the unwritten gap of the last row's stride padding does not exist (the extent is one range), but a
  // region just after the last row's width is outside the extent.
  const Cmds cmds = line_path(3);
  const std::size_t need = work_bytes_needed(3, false);
  std::vector<u64> arena(1 << 16, 0x1111111111111111ull);
  std::uint8_t* base = reinterpret_cast<std::uint8_t*>(arena.data());
  const std::size_t W0 = 8 * 20000;
  const u32 w = 5, h = 3, stride = 9;
  const std::size_t T = static_cast<std::size_t>(stride) * (h - 1) + w;  // 23, not stride*h = 27
  const std::size_t toff = W0 - T;  // last extent byte is the one just before the work
  std::memcpy(base + 8 * 40000, cmds.data(), cmds.size() * sizeof(Command));
  const raster::Target t{base + toff, w, h, stride};
  REQUIRE(fill_path(reinterpret_cast<const Command*>(base + 8 * 40000), 3, 3, raster::Fill::NonZero, t, base + W0, need) == Status::Ok);
  const raster::Target t2{base + toff + 1, w, h, stride};
  REQUIRE(fill_path(reinterpret_cast<const Command*>(base + 8 * 40000), 3, 3, raster::Fill::NonZero, t2, base + W0, need) == Status::Overlap);
}

TEST_CASE("largest stroke and fill workloads fit the exact work", "[vecdraw]") {
  // 64 commands of curves with the smallest tolerance: the fill outline fills its whole bound.
  Lcg r{0x5151};
  Cmds big;
  big.push_back(Command{Op::Move, {-K, -K}, {0, 0}, {0, 0}});
  for (int i = 1; i < 64; ++i)
    big.push_back(Command{Op::Cubic, {r.rng(-K, K), r.rng(-K, K)}, {r.rng(-K, K), r.rng(-K, K)}, {r.rng(-K, K), r.rng(-K, K)}});
  const auto t0 = std::chrono::steady_clock::now();
  const Result rf = run_case(big, 1, 0, {1, 1, 0, 0}, 64, 64);
  REQUIRE(rf.st == Status::Ok);
  const Result re = run_case(big, 1, 1, {1, 1, 0, 0}, 33, 7);
  REQUIRE(re.st == Status::Ok);
  // The same path stroked: the first contour has far more than 1024 points.
  const Result rs = run_case(big, 1, 2, {40, 4, 1, 1}, 16, 16);
  REQUIRE(rs.st == Status::TooManyPoints);
  // 32 subpaths of a move and a long quadrilateral-like cubic: every stroke contour list is near the 4096 cap.
  Cmds many;
  for (int i = 0; i < 32; ++i) {
    many.push_back(Command{Op::Move, {-K / 2 + i * 1000, -K / 2}, {0, 0}, {0, 0}});
    many.push_back(Command{Op::Cubic, {K / 2, K / 2}, {-K / 2, K / 2 - i * 5000}, {K / 2, -K / 2 + i * 3000}});
  }
  const Result rm = run_case(many, 1, 2, {40, 4, 0, 0}, 16, 16);
  REQUIRE(rm.st == Status::TooManyContours);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  REQUIRE(ms < 60000);  // a loose guard against a quadratic blow-up, also under the sanitizers
}

TEST_CASE("a path with no qualifying fill contour clears the mask", "[vecdraw]") {
  const Cmds cmds = {Command{Op::Move, {256, 256}, {0, 0}, {0, 0}}, Command{Op::Line, {900, 900}, {0, 0}, {0, 0}}};
  const std::size_t need = work_bytes_needed(2, false);
  std::vector<u64> work(need / 8);
  std::vector<std::uint8_t> tgt(7 * 4, 0x99);
  REQUIRE(fill_path(cmds.data(), 2, 3, raster::Fill::NonZero, raster::Target{tgt.data(), 5, 4, 7}, work.data(), need) == Status::Ok);
  for (u32 y = 0; y < 4; ++y)
    for (u32 x = 0; x < 7; ++x) REQUIRE(tgt[y * 7 + x] == (x < 5 ? 0 : 0x99));
}

TEST_CASE("stroke style limits", "[vecdraw]") {
  const Cmds cmds = {Command{Op::Move, {256, 256}, {0, 0}, {0, 0}}, Command{Op::Line, {3000, 300}, {0, 0}, {0, 0}},
                     Command{Op::Line, {3000, 3000}, {0, 0}, {0, 0}}};
  const std::size_t need = work_bytes_needed(3, true);
  std::vector<u64> work(need / 8);
  auto go = [&](std::int32_t hw, std::int32_t ml, std::uint8_t join, std::uint8_t cap) {
    std::vector<std::uint8_t> tgt(8 * 8, kSent);
    stroke::Style st{hw, ml, static_cast<stroke::Join>(join), static_cast<stroke::Cap>(cap), false};
    const Status s = stroke_path(cmds.data(), 3, 3, st, raster::Target{tgt.data(), 8, 8, 8}, work.data(), need);
    if (s == Status::BadStyle)
      for (std::uint8_t b : tgt) REQUIRE(b == kSent);
    return s;
  };
  const std::int32_t kHw = 1 << 19;
  REQUIRE(go(1, 1, 0, 0) == Status::Ok);
  REQUIRE(go(kHw, 64, 1, 1) == Status::Ok);
  REQUIRE(go(0, 4, 0, 0) == Status::BadStyle);
  REQUIRE(go(-1, 4, 0, 0) == Status::BadStyle);
  REQUIRE(go(kHw + 1, 4, 0, 0) == Status::BadStyle);
  REQUIRE(go(10, 0, 0, 0) == Status::BadStyle);
  REQUIRE(go(10, -1, 0, 0) == Status::BadStyle);
  REQUIRE(go(10, 65, 0, 0) == Status::BadStyle);
  REQUIRE(go(10, 4, 2, 0) == Status::BadStyle);
  REQUIRE(go(10, 4, 255, 0) == Status::BadStyle);
  REQUIRE(go(10, 4, 0, 2) == Status::BadStyle);
  REQUIRE(go(10, 4, 0, 255) == Status::BadStyle);
}

TEST_CASE("tolerance, size and count limits", "[vecdraw]") {
  const Cmds cmds = {Command{Op::Move, {256, 256}, {0, 0}, {0, 0}}, Command{Op::Quad, {900, 3000}, {3000, 300}, {0, 0}}};
  for (int mode = 0; mode < 3; mode += 2) {
    const std::size_t need = work_bytes_needed(2, mode == 2);
    std::vector<u64> work(need / 8);
    const StyleArgs sa{50, 4, 1, 1};
    std::vector<std::uint8_t> tgt(16, kSent);
    const raster::Target t{tgt.data(), 4, 4, 4};
    REQUIRE(call(cmds, 2, 1, mode, sa, t, work.data(), need) == Status::Ok);
    REQUIRE(call(cmds, 2, 65536, mode, sa, t, work.data(), need) == Status::Ok);
    std::fill(tgt.begin(), tgt.end(), kSent);
    for (std::int32_t bad : {0, -1, 65537, 0x7FFFFFFF, -0x7FFFFFFF - 1}) {
      REQUIRE(call(cmds, 2, bad, mode, sa, t, work.data(), need) == Status::BadTolerance);
      for (std::uint8_t b : tgt) REQUIRE(b == kSent);
    }
    // target size limits: 8192 x 1 and 1 x 8192 are valid, anything beyond is BadSize
    std::vector<std::uint8_t> wide(8192, kSent), tall(8192, kSent);
    REQUIRE(call(cmds, 2, 5, mode, sa, raster::Target{wide.data(), 8192, 1, 8192}, work.data(), need) == Status::Ok);
    REQUIRE(call(cmds, 2, 5, mode, sa, raster::Target{tall.data(), 1, 8192, 1}, work.data(), need) == Status::Ok);
    std::vector<std::uint8_t> big(8200, kSent);
    for (const raster::Target& bt : {raster::Target{big.data(), 8193, 1, 8193}, raster::Target{big.data(), 1, 8193, 1},
                                     raster::Target{big.data(), 0, 1, 1}, raster::Target{big.data(), 1, 0, 1},
                                     raster::Target{big.data(), 4, 2, 3}, raster::Target{big.data(), 0x7FFFFFFF, 1, 0x7FFFFFFF}}) {
      REQUIRE(call(cmds, 2, 5, mode, sa, bt, work.data(), need) == Status::BadSize);
    }
    for (std::uint8_t b : big) REQUIRE(b == kSent);
    // command count limits (data is never read once the count is bad)
    REQUIRE(call(cmds, 0, 5, mode, sa, t, work.data(), need) == Status::BadCount);
    REQUIRE(call(cmds, 65, 5, mode, sa, t, work.data(), need) == Status::BadCount);
    REQUIRE(call(cmds, 0xFFFFFFFFu, 5, mode, sa, t, work.data(), need) == Status::BadCount);
    const Cmds sixty4 = line_path(64);
    const std::size_t n64 = work_bytes_needed(64, mode == 2);
    std::vector<u64> w64(n64 / 8);
    REQUIRE(call(sixty4, 64, 5, mode, sa, t, w64.data(), n64) == Status::Ok);
  }
}
