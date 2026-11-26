// Tests for the stroke module (scope W). Expected values come from an independent Python reference (exact big integers, explicit
// lists, its own nearest square root by search, its own FNV and its own generator) and are pasted here as constants: explicit
// contours for the named cases and 64-bit digests for the grids and the random corpora. Every output of every run is also checked
// by structural properties written in this file (winding, convexity, bounds, ranges). See docs/stroke.md.
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "stroke.h"

namespace lumora::stroke {
inline bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Point& a, const Point& b) { return !(a == b); }
}  // namespace lumora::stroke

using namespace lumora::stroke;

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Pts = std::vector<Point>;

constexpr std::int32_t K = kMaxCoord;
constexpr std::int32_t kSentinelCoord = 0x5A5A5A5A;
constexpr u32 kSentinelWord = 0xA5A5A5A5u;

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

// FNV-1a 64 over 32-bit little-endian words, written here independently of the module.
struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

struct NamedCase {
  const char* name;
  Pts in;
  Style style;
  Pts pts;
  std::vector<u32> ends;
};
struct GridLine {
  Pts in;
  u64 contours, points, digest;
};
struct ExhGold {
  const char* name;
  u64 runs, contours, points, zero_m, nonconvex, over_limit, digest;  // the last three are reference statistics, listed for the docs
};
struct RandGold {
  u32 seed;
  i64 range;
  u64 contours, points, zero_m, nonconvex, over_limit, digest;
};

static const NamedCase kNamed[] = {
  {"zero_normal_miter", {{0,0},{0,3},{1,0}}, {1,64,Join::Miter,Cap::Butt,false}, {{1,0},{1,3},{-1,3},{-1,0},{-1,3},{0,0},{2,0},{1,3},{0,3},{-1,3},{1,3}}, {4,8,11}},
  {"reflex_miter_quad", {{0,0},{0,1},{1,2}}, {3,64,Join::Miter,Cap::Butt,false}, {{3,0},{3,1},{-3,1},{-3,0},{3,-2},{4,-1},{-2,5},{-3,4},{0,1},{-3,4},{-3,1}}, {4,8,11}},
  {"bowtie_miter_quad", {{0,0},{1,1},{2,3}}, {3,64,Join::Miter,Cap::Butt,false}, {{3,-3},{4,-2},{-2,4},{-3,3},{4,-1},{5,1},{-1,5},{-2,3},{1,1},{4,-2},{4,-1}}, {4,8,11}},
  {"zero_area_bevel", {{0,0},{0,1},{1,4}}, {1,64,Join::Bevel,Cap::Butt,false}, {{1,0},{1,1},{-1,1},{-1,0},{1,1},{2,4},{0,4},{-1,1},{0,1},{-1,1},{-1,1}}, {4,8,11}},
  {"segment_only_butt", {{0,0},{10,0}}, {2,4,Join::Bevel,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2}}, {4}},
  {"segment_only_square", {{0,0},{10,0}}, {2,4,Join::Bevel,Cap::Square,false}, {{0,-2},{10,-2},{10,2},{0,2},{0,-2},{0,2},{-2,2},{-2,-2},{10,2},{10,-2},{12,-2},{12,2}}, {4,8,12}},
  {"segment_diagonal_345", {{0,0},{3,4}}, {5,4,Join::Bevel,Cap::Square,false}, {{4,-3},{7,1},{-1,7},{-4,3},{4,-3},{-4,3},{-7,-1},{1,-7},{-1,7},{7,1},{10,5},{2,11}}, {4,8,12}},
  {"segment_rounded_normal", {{0,0},{1,1}}, {3,4,Join::Bevel,Cap::Butt,false}, {{3,-3},{4,-2},{-2,4},{-3,3}}, {4}},
  {"segment_vertical_down", {{5,5},{5,-5}}, {3,4,Join::Bevel,Cap::Square,false}, {{2,5},{2,-5},{8,-5},{8,5},{2,5},{8,5},{8,8},{2,8},{8,-5},{2,-5},{2,-8},{8,-8}}, {4,8,12}},
  {"left_turn_bevel", {{0,0},{10,0},{10,10}}, {2,4,Join::Bevel,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{10,0},{10,-2},{12,0}}, {4,8,11}},
  {"left_turn_miter", {{0,0},{10,0},{10,10}}, {2,4,Join::Miter,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{10,0},{10,-2},{12,-2},{12,0}}, {4,8,12}},
  {"right_turn_bevel", {{0,0},{10,0},{10,-10}}, {2,4,Join::Bevel,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2},{8,0},{8,-10},{12,-10},{12,0},{10,0},{12,0},{10,2}}, {4,8,11}},
  {"right_turn_miter", {{0,0},{10,0},{10,-10}}, {2,4,Join::Miter,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2},{8,0},{8,-10},{12,-10},{12,0},{10,0},{12,0},{12,2},{10,2}}, {4,8,12}},
  {"right_turn_miter_square", {{0,0},{10,0},{10,-10}}, {2,4,Join::Miter,Cap::Square,false}, {{0,-2},{10,-2},{10,2},{0,2},{8,0},{8,-10},{12,-10},{12,0},{10,0},{12,0},{12,2},{10,2},{0,-2},{0,2},{-2,2},{-2,-2},{12,-10},{8,-10},{8,-12},{12,-12}}, {4,8,12,16,20}},
  {"miter_over_limit", {{0,0},{100,0},{0,5}}, {4,2,Join::Miter,Cap::Butt,false}, {{0,-4},{100,-4},{100,4},{0,4},{100,4},{0,9},{0,1},{100,-4},{100,0},{100,-4},{100,4}}, {4,8,11}},
  {"miter_at_limit_ok", {{0,0},{100,0},{0,5}}, {4,64,Join::Miter,Cap::Butt,false}, {{0,-4},{100,-4},{100,4},{0,4},{100,4},{0,9},{0,1},{100,-4},{100,0},{100,-4},{100,4}}, {4,8,11}},
  {"miter_limit_one", {{0,0},{10,0},{10,10}}, {2,1,Join::Miter,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{10,0},{10,-2},{12,0}}, {4,8,11}},
  {"straight_through", {{0,0},{5,0},{10,0}}, {2,4,Join::Miter,Cap::Square,false}, {{0,-2},{5,-2},{5,2},{0,2},{5,-2},{10,-2},{10,2},{5,2},{0,-2},{0,2},{-2,2},{-2,-2},{10,2},{10,-2},{12,-2},{12,2}}, {4,8,12,16}},
  {"full_reversal", {{0,0},{10,0},{0,0}}, {2,4,Join::Miter,Cap::Square,false}, {{0,-2},{10,-2},{10,2},{0,2},{10,2},{0,2},{0,-2},{10,-2},{0,-2},{0,2},{-2,2},{-2,-2},{0,-2},{0,2},{-2,2},{-2,-2}}, {4,8,12,16}},
  {"closed_square_bevel", {{0,0},{10,0},{10,10},{0,10}}, {2,4,Join::Bevel,Cap::Butt,true}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{10,12},{0,12},{0,8},{10,8},{-2,10},{-2,0},{2,0},{2,10},{0,0},{-2,0},{0,-2},{10,0},{10,-2},{12,0},{10,10},{12,10},{10,12},{0,10},{0,12},{-2,10}}, {4,8,12,16,19,22,25,28}},
  {"closed_square_miter", {{0,0},{10,0},{10,10},{0,10}}, {2,4,Join::Miter,Cap::Square,true}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{10,12},{0,12},{0,8},{10,8},{-2,10},{-2,0},{2,0},{2,10},{0,0},{-2,0},{-2,-2},{0,-2},{10,0},{10,-2},{12,-2},{12,0},{10,10},{12,10},{12,12},{10,12},{0,10},{0,12},{-2,12},{-2,10}}, {4,8,12,16,20,24,28,32}},
  {"closed_triangle_cw", {{0,0},{0,10},{10,0}}, {2,4,Join::Miter,Cap::Butt,true}, {{2,0},{2,10},{-2,10},{-2,0},{-1,9},{9,-1},{11,1},{1,11},{10,2},{0,2},{0,-2},{10,-2},{0,0},{-2,0},{-2,-2},{0,-2},{0,10},{1,11},{-4,14},{-2,10},{10,0},{10,-2},{14,-4},{11,1}}, {4,8,12,16,20,24}},
  {"closed_two_points", {{0,0},{10,0}}, {2,4,Join::Miter,Cap::Square,true}, {{0,-2},{10,-2},{10,2},{0,2},{10,2},{0,2},{0,-2},{10,-2}}, {4,8}},
  {"closed_two_points_via_dups", {{0,0},{10,0},{0,0}}, {2,4,Join::Miter,Cap::Butt,true}, {{0,-2},{10,-2},{10,2},{0,2},{10,2},{0,2},{0,-2},{10,-2}}, {4,8}},
  {"open_three_back_to_start", {{0,0},{10,0},{0,0}}, {2,4,Join::Miter,Cap::Square,false}, {{0,-2},{10,-2},{10,2},{0,2},{10,2},{0,2},{0,-2},{10,-2},{0,-2},{0,2},{-2,2},{-2,-2},{0,-2},{0,2},{-2,2},{-2,-2}}, {4,8,12,16}},
  {"dedup_consecutive", {{0,0},{0,0},{10,0},{10,0},{10,10}}, {2,4,Join::Miter,Cap::Butt,false}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{10,0},{10,-2},{12,-2},{12,0}}, {4,8,12}},
  {"dedup_closed_endpoints", {{0,0},{10,0},{10,10},{0,0}}, {2,4,Join::Miter,Cap::Butt,true}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{9,11},{-1,1},{1,-1},{11,9},{0,0},{-1,1},{-4,-4},{0,-2},{10,0},{10,-2},{12,-2},{12,0},{10,10},{12,10},{14,14},{9,11}}, {4,8,12,16,20,24}},
  {"dedup_closed_endpoints_twice", {{0,0},{10,0},{10,10},{0,0},{0,0}}, {2,4,Join::Bevel,Cap::Butt,true}, {{0,-2},{10,-2},{10,2},{0,2},{12,0},{12,10},{8,10},{8,0},{9,11},{-1,1},{1,-1},{11,9},{0,0},{-1,1},{0,-2},{10,0},{10,-2},{12,0},{10,10},{12,10},{9,11}}, {4,8,12,15,18,21}},
  {"all_equal_butt", {{7,7},{7,7},{7,7}}, {3,4,Join::Miter,Cap::Butt,false}, {}, {}},
  {"all_equal_square", {{7,7},{7,7},{7,7}}, {3,4,Join::Miter,Cap::Square,false}, {{4,4},{10,4},{10,10},{4,10}}, {4}},
  {"all_equal_square_closed", {{-7,9},{-7,9}}, {3,4,Join::Bevel,Cap::Square,true}, {{-10,6},{-4,6},{-4,12},{-10,12}}, {4}},
  {"all_equal_butt_closed", {{-7,9},{-7,9}}, {3,4,Join::Bevel,Cap::Butt,true}, {}, {}},
  {"sharp_spike_miter", {{0,0},{100,1},{0,2}}, {3,64,Join::Miter,Cap::Butt,false}, {{0,-3},{100,-2},{100,4},{0,3},{100,4},{0,5},{0,-1},{100,-2},{100,1},{100,-2},{100,4}}, {4,8,11}},
  {"sharp_spike_bevel", {{0,0},{100,1},{0,2}}, {3,64,Join::Bevel,Cap::Butt,false}, {{0,-3},{100,-2},{100,4},{0,3},{100,4},{0,5},{0,-1},{100,-2},{100,1},{100,-2},{100,4}}, {4,8,11}},
  {"hairpin_nearly_reversal", {{0,0},{1000,0},{0,1}}, {5,64,Join::Miter,Cap::Square,false}, {{0,-5},{1000,-5},{1000,5},{0,5},{1000,5},{0,6},{0,-4},{1000,-5},{1000,0},{1000,-5},{1000,5},{0,-5},{0,5},{-5,5},{-5,-5},{0,-4},{0,6},{-5,6},{-5,-4}}, {4,8,11,15,19}},
  {"zigzag_five", {{0,0},{10,5},{20,0},{30,5},{40,0}}, {2,4,Join::Miter,Cap::Square,false}, {{1,-2},{11,3},{9,7},{-1,2},{9,3},{19,-2},{21,2},{11,7},{21,-2},{31,3},{29,7},{19,2},{29,3},{39,-2},{41,2},{31,7},{10,5},{11,7},{9,7},{20,0},{19,-2},{21,-2},{30,5},{31,7},{29,7},{1,-2},{-1,2},{-3,1},{-1,-3},{41,2},{39,-2},{41,-3},{43,1}}, {4,8,12,16,19,22,25,29,33}},
  {"closed_pentagon_miter", {{10,0},{3,9},{-8,6},{-8,-6},{3,-9}}, {2,2,Join::Miter,Cap::Butt,true}, {{12,1},{5,10},{1,8},{8,-1},{2,11},{-9,8},{-7,4},{4,7},{-10,6},{-10,-6},{-6,-6},{-6,6},{-9,-8},{2,-11},{4,-7},{-7,-4},{5,-10},{12,-1},{8,1},{1,-8},{10,0},{12,-1},{12,1},{3,9},{5,10},{4,11},{2,11},{-8,6},{-9,8},{-10,7},{-10,6},{-8,-6},{-10,-6},{-10,-7},{-9,-8},{3,-9},{2,-11},{4,-11},{5,-10}}, {4,8,12,16,20,23,27,31,35,39}},
  {"long_shallow_turn", {{0,0},{100000,1},{200000,3}}, {7,64,Join::Miter,Cap::Butt,false}, {{0,-7},{100000,-6},{100000,8},{0,7},{100000,-6},{200000,-4},{200000,10},{100000,8},{100000,1},{100000,-6},{100000,-6}}, {4,8,11}},
  {"long_shallow_turn_bevel", {{0,0},{100000,1},{200000,3}}, {7,64,Join::Bevel,Cap::Butt,false}, {{0,-7},{100000,-6},{100000,8},{0,7},{100000,-6},{200000,-4},{200000,10},{100000,8},{100000,1},{100000,-6},{100000,-6}}, {4,8,11}},
  {"extreme_horizontal", {{-268435456,0},{268435456,0}}, {524288,64,Join::Bevel,Cap::Square,false}, {{-268435456,-524288},{268435456,-524288},{268435456,524288},{-268435456,524288},{-268435456,-524288},{-268435456,524288},{-268959744,524288},{-268959744,-524288},{268435456,524288},{268435456,-524288},{268959744,-524288},{268959744,524288}}, {4,8,12}},
  {"extreme_diagonal", {{-268435456,-268435456},{268435456,268435456}}, {524288,64,Join::Bevel,Cap::Square,false}, {{-268064728,-268806184},{268806184,268064728},{268064728,268806184},{-268806184,-268064728},{-268064728,-268806184},{-268806184,-268064728},{-269176912,-268435456},{-268435456,-269176912},{268064728,268806184},{268806184,268064728},{269176912,268435456},{268435456,269176912}}, {4,8,12}},
  {"extreme_turn_miter", {{-268435456,-268435456},{268435456,-268435456},{268435456,268435456}}, {524288,64,Join::Miter,Cap::Square,false}, {{-268435456,-268959744},{268435456,-268959744},{268435456,-267911168},{-268435456,-267911168},{268959744,-268435456},{268959744,268435456},{267911168,268435456},{267911168,-268435456},{268435456,-268435456},{268435456,-268959744},{268959744,-268959744},{268959744,-268435456},{-268435456,-268959744},{-268435456,-267911168},{-268959744,-267911168},{-268959744,-268959744},{267911168,268435456},{268959744,268435456},{268959744,268959744},{267911168,268959744}}, {4,8,12,16,20}},
  {"extreme_turn_miter_closed", {{-268435456,-268435456},{268435456,-268435456},{268435456,268435456},{-268435456,268435456}}, {524288,64,Join::Miter,Cap::Butt,true}, {{-268435456,-268959744},{268435456,-268959744},{268435456,-267911168},{-268435456,-267911168},{268959744,-268435456},{268959744,268435456},{267911168,268435456},{267911168,-268435456},{268435456,268959744},{-268435456,268959744},{-268435456,267911168},{268435456,267911168},{-268959744,268435456},{-268959744,-268435456},{-267911168,-268435456},{-267911168,268435456},{-268435456,-268435456},{-268959744,-268435456},{-268959744,-268959744},{-268435456,-268959744},{268435456,-268435456},{268435456,-268959744},{268959744,-268959744},{268959744,-268435456},{268435456,268435456},{268959744,268435456},{268959744,268959744},{268435456,268959744},{-268435456,268435456},{-268435456,268959744},{-268959744,268959744},{-268959744,268435456}}, {4,8,12,16,20,24,28,32}},
  {"extreme_zigzag", {{-268435456,-268435456},{268435456,268435456},{-268435456,268435456},{268435456,-268435456}}, {524288,64,Join::Miter,Cap::Square,false}, {{-268064728,-268806184},{268806184,268064728},{268064728,268806184},{-268806184,-268064728},{268435456,268959744},{-268435456,268959744},{-268435456,267911168},{268435456,267911168},{-268806184,268064728},{268064728,-268806184},{268806184,-268064728},{-268064728,268806184},{268435456,268435456},{268806184,268064728},{269701199,268959742},{268435456,268959744},{-268435456,268435456},{-268435456,268959744},{-269701199,268959742},{-268806184,268064728},{-268064728,-268806184},{-268806184,-268064728},{-269176912,-268435456},{-268435456,-269176912},{268806184,-268064728},{268064728,-268806184},{268435456,-269176912},{269176912,-268435456}}, {4,8,12,16,20,24,28}},
  {"extreme_odd_slope", {{-268435456,268435456},{268435455,-268435454},{268435449,268435453}}, {524288,64,Join::Miter,Cap::Square,false}, {{-268806184,268064728},{268064727,-268806182},{268806183,-268064726},{-268064728,268806184},{268959743,-268435454},{268959737,268435453},{267911161,268435453},{267911167,-268435454},{268435455,-268435454},{268064727,-268806182},{268959741,-269701197},{268959743,-268435454},{-268806184,268064728},{-268064728,268806184},{-268435456,269176912},{-269176912,268435456},{267911161,268435453},{268959737,268435453},{268959737,268959741},{267911161,268959741}}, {4,8,12,16,20}},
  {"extreme_small_width", {{-268435456,-268435456},{268435456,268435455},{-268435453,268435456}}, {1,64,Join::Miter,Cap::Square,false}, {{-268435455,-268435457},{268435457,268435454},{268435455,268435456},{-268435457,-268435455},{268435456,268435456},{-268435453,268435457},{-268435453,268435455},{268435456,268435454},{268435456,268435455},{268435457,268435454},{268435458,268435455},{268435456,268435456},{-268435455,-268435457},{-268435457,-268435455},{-268435458,-268435456},{-268435456,-268435458},{-268435453,268435455},{-268435453,268435457},{-268435454,268435457},{-268435454,268435455}}, {4,8,12,16,20}},
  {"extreme_all_equal_square", {{268435456,268435456},{268435456,268435456}}, {524288,2,Join::Bevel,Cap::Square,false}, {{267911168,267911168},{268959744,267911168},{268959744,268959744},{267911168,268959744}}, {4}},
  {"extreme_all_equal_square_neg", {{-268435456,-268435456},{-268435456,-268435456},{-268435456,-268435456}}, {524288,2,Join::Bevel,Cap::Square,true}, {{-268959744,-268959744},{-267911168,-268959744},{-267911168,-267911168},{-268959744,-267911168}}, {4}},
  {"miter_limit_64_wide", {{0,0},{1000,0},{1000,1}}, {100,64,Join::Miter,Cap::Butt,false}, {{0,-100},{1000,-100},{1000,100},{0,100},{1100,0},{1100,1},{900,1},{900,0},{1000,0},{1000,-100},{1100,-100},{1100,0}}, {4,8,12}},
  {"odd_normals", {{0,0},{7,3},{9,11},{2,17}}, {5,3,Join::Miter,Cap::Square,false}, {{2,-4},{9,-1},{5,7},{-2,4},{12,2},{14,10},{4,12},{2,4},{12,15},{5,21},{-1,13},{6,7},{7,3},{9,-1},{12,0},{12,2},{9,11},{14,10},{14,13},{12,15},{2,-4},{-2,4},{-6,2},{-2,-6},{-1,13},{5,21},{1,24},{-5,16}}, {4,8,12,16,20,24,28}},
  {"odd_normals_closed", {{0,0},{7,3},{9,11},{2,17}}, {5,3,Join::Miter,Cap::Square,true}, {{2,-4},{9,-1},{5,7},{-2,4},{12,2},{14,10},{4,12},{2,4},{12,15},{5,21},{-1,13},{6,7},{-3,18},{-5,1},{5,-1},{7,16},{0,0},{-5,1},{-8,-8},{2,-4},{7,3},{9,-1},{12,0},{12,2},{9,11},{14,10},{14,13},{12,15},{2,17},{5,21},{-1,26},{-3,18}}, {4,8,12,16,20,24,28,32}},
};
static const GridLine kGridLines[] = {
  {{{0,0},{0,3},{1,0}}, 200, 740, 0x7a0a2f0fb90762f5ull},
  {{{0,0},{0,1},{1,2}}, 200, 726, 0x52b0d0a778ed161dull},
  {{{0,0},{1,1},{2,3}}, 200, 724, 0x49394805bfa743edull},
  {{{0,0},{0,1},{1,4}}, 200, 722, 0x82e7ce6de0db45d5ull},
  {{{0,0},{10,0}}, 80, 320, 0xc66c8c016b01b185ull},
  {{{0,0},{10,0}}, 80, 320, 0xc66c8c016b01b185ull},
  {{{0,0},{3,4}}, 80, 320, 0x75b4a05c7dc350e5ull},
  {{{0,0},{1,1}}, 80, 320, 0xd4027628df9470d5ull},
  {{{5,5},{5,-5}}, 80, 320, 0xe16764910e6a3555ull},
  {{{0,0},{10,0},{10,10}}, 200, 744, 0xa9b230800c670d35ull},
  {{{0,0},{10,0},{10,10}}, 200, 744, 0xa9b230800c670d35ull},
  {{{0,0},{10,0},{10,-10}}, 200, 744, 0x855a8801ff81b355ull},
  {{{0,0},{10,0},{10,-10}}, 200, 744, 0x855a8801ff81b355ull},
  {{{0,0},{10,0},{10,-10}}, 200, 744, 0x855a8801ff81b355ull},
  {{{0,0},{100,0},{0,5}}, 200, 736, 0x4a34566aa5c81655ull},
  {{{0,0},{100,0},{0,5}}, 200, 736, 0x4a34566aa5c81655ull},
  {{{0,0},{10,0},{10,10}}, 200, 744, 0xa9b230800c670d35ull},
  {{{0,0},{5,0},{10,0}}, 120, 480, 0x067bab29ae97de55ull},
  {{{0,0},{10,0},{0,0}}, 100, 400, 0x35db2d0b9e58a725ull},
  {{{0,0},{10,0},{10,10},{0,10}}, 280, 1048, 0x88cd5ec78115d5cdull},
  {{{0,0},{10,0},{10,10},{0,10}}, 280, 1048, 0x88cd5ec78115d5cdull},
  {{{0,0},{0,10},{10,0}}, 200, 740, 0x2400899cf8379f25ull},
  {{{0,0},{10,0}}, 80, 320, 0xc66c8c016b01b185ull},
  {{{0,0},{10,0},{0,0}}, 100, 400, 0x35db2d0b9e58a725ull},
  {{{0,0},{10,0},{0,0}}, 100, 400, 0x35db2d0b9e58a725ull},
  {{{0,0},{0,0},{10,0},{10,0},{10,10}}, 200, 744, 0xa9b230800c670d35ull},
  {{{0,0},{10,0},{10,10},{0,0}}, 240, 888, 0xdf1994c5e2530d55ull},
  {{{0,0},{10,0},{10,10},{0,0},{0,0}}, 240, 888, 0xdf1994c5e2530d55ull},
  {{{7,7},{7,7},{7,7}}, 20, 80, 0xed5c4b1faf2451a5ull},
  {{{7,7},{7,7},{7,7}}, 20, 80, 0xed5c4b1faf2451a5ull},
  {{{-7,9},{-7,9}}, 20, 80, 0x31c053f8e7dddca5ull},
  {{{-7,9},{-7,9}}, 20, 80, 0x31c053f8e7dddca5ull},
  {{{0,0},{100,1},{0,2}}, 200, 736, 0xb3b9c6ba150627c5ull},
  {{{0,0},{100,1},{0,2}}, 200, 736, 0xb3b9c6ba150627c5ull},
  {{{0,0},{1000,0},{0,1}}, 200, 736, 0xf10c603a50c69899ull},
  {{{0,0},{10,5},{20,0},{30,5},{40,0}}, 360, 1308, 0xb89d64c3a3de3065ull},
  {{{10,0},{3,9},{-8,6},{-8,-6},{3,-9}}, 360, 1342, 0x36b972cf3c03b79dull},
  {{{0,0},{100000,1},{200000,3}}, 200, 720, 0xffe99bb5945fcbd5ull},
  {{{0,0},{100000,1},{200000,3}}, 200, 720, 0xffe99bb5945fcbd5ull},
  {{{-268435456,0},{268435456,0}}, 80, 320, 0xad333c037e3f96a5ull},
  {{{-268435456,-268435456},{268435456,268435456}}, 80, 320, 0x82fffb571745bdb5ull},
  {{{-268435456,-268435456},{268435456,-268435456},{268435456,268435456}}, 200, 744, 0x316ac14efc2cb4a5ull},
  {{{-268435456,-268435456},{268435456,-268435456},{268435456,268435456},{-268435456,268435456}}, 280, 1048, 0x21ed9a46a2f729adull},
  {{{-268435456,-268435456},{268435456,268435456},{-268435456,268435456},{268435456,-268435456}}, 280, 1024, 0xe237c0f71b556c65ull},
  {{{-268435456,268435456},{268435455,-268435454},{268435449,268435453}}, 200, 740, 0xe97698694a2ec345ull},
  {{{-268435456,-268435456},{268435456,268435455},{-268435453,268435456}}, 200, 740, 0x22a88d35947d56c5ull},
  {{{268435456,268435456},{268435456,268435456}}, 20, 80, 0x94cc60597408e425ull},
  {{{-268435456,-268435456},{-268435456,-268435456},{-268435456,-268435456}}, 20, 80, 0xfcf473ac9f7035a5ull},
  {{{0,0},{1000,0},{1000,1}}, 200, 744, 0x2250c7f2de7338b5ull},
  {{{0,0},{7,3},{9,11},{2,17}}, 280, 1036, 0x551389931391493dull},
  {{{0,0},{7,3},{9,11},{2,17}}, 280, 1036, 0x551389931391493dull},
  {{{0,0},{4,0}}, 80, 320, 0xa578c3f963f60e35ull},
  {{{0,0},{0,0}}, 20, 80, 0xab520f69aeef7ae5ull},
  {{{0,0},{3,1},{6,0},{9,1}}, 280, 1004, 0xf58f7e7cc55afc45ull},
  {{{0,0},{10,0},{10,10},{0,10},{0,0}}, 320, 1196, 0x019b5641d9f9bb45ull},
  {{{5,5},{5,5},{6,6},{6,6},{5,5}}, 100, 400, 0x3d8030e73baa4305ull},
  {{{0,0},{2,1},{4,3},{5,6},{5,10},{3,12}}, 440, 1598, 0xa53876b3f377fc15ull},
  {{{-3,2},{8,-5},{0,0},{8,5},{-3,-2}}, 360, 1308, 0xaadbf2b38a4541bdull},
};
constexpr std::size_t kStyleGridCount = 40;
static const ExhGold kExh[] = {
  {"grid4_three_points", 131072, 570880, 2134912, 5504, 12928, 31104, 0xc43321c558146685ull},
  {"grid3_four_points", 104976, 594312, 2229040, 6608, 16048, 17936, 0xfb6dc6abc399a9d5ull},
  {"grid3_two_points", 1944, 3564, 14256, 0, 0, 0, 0x5c9e8c4f023e9d65ull},
};
static const RandGold kRand[] = {
  {41, 6, 7402, 27172, 107, 137, 599, 0xc595c3a0c9d17de3ull},
  {42, 1000, 7300, 26739, 124, 77, 722, 0xadc31c16e6cb5290ull},
  {43, 268435456, 7601, 27908, 126, 70, 675, 0xd078306ef0cf0f4dull},
};
constexpr std::size_t kIsqrtSamples = 15261; constexpr std::uint64_t kIsqrtDigest = 0x57adf90826a131f3ull;
constexpr std::uint32_t kPerfopenContours = 2047, kPerfopenPoints = 8177; constexpr std::uint64_t kPerfopenDigest = 0x760866de932104bdull;
constexpr std::uint32_t kPerfclosedContours = 2048, kPerfclosedPoints = 8181; constexpr std::uint64_t kPerfclosedDigest = 0xd0f129deaaba18b9ull;
constexpr std::uint32_t kPerfthinContours = 2047, kPerfthinPoints = 7734; constexpr std::uint64_t kPerfthinDigest = 0x2f3b0749267cef89ull;
constexpr std::uint64_t kBoundSumOpen = 1048575, kBoundSumClosed = 1049598;

struct Result {
  Status st;
  Pts pts;
  std::vector<u32> ends;
  u32 nc, np;
};

Result run(const Pts& in, const Style& s) {
  const u32 n = static_cast<u32>(in.size());
  const u32 cb = contour_bound(n, s.closed), pb = point_bound(n, s.closed);
  Result r;
  r.pts.assign(pb, Point{kSentinelCoord, kSentinelCoord});
  r.ends.assign(cb, kSentinelWord);
  r.nc = r.np = kSentinelWord;
  r.st = stroke_polyline(in.data(), n, s, r.pts.data(), pb, r.ends.data(), cb, &r.nc, &r.np);
  return r;
}

i64 cross3(Point o, Point a, Point b) {
  return (static_cast<i64>(a.x) - o.x) * (static_cast<i64>(b.y) - o.y) - (static_cast<i64>(a.y) - o.y) * (static_cast<i64>(b.x) - o.x);
}

// Structural properties of one successful output. Returns false (with the reason in what) instead of aborting so the exhaustive
// loops stay fast.
bool well_formed(const Pts& in, const Style& s, const Result& r, const char** what) {
  const u32 n = static_cast<u32>(in.size());
  *what = "";
  if (r.st != Status::Ok) { *what = "status"; return false; }
  if (r.nc > contour_bound(n, s.closed) || r.np > point_bound(n, s.closed)) { *what = "bound"; return false; }
  if (r.np != (r.nc == 0 ? 0u : r.ends[r.nc - 1])) { *what = "point count"; return false; }
  u32 begin = 0;
  for (u32 c = 0; c < r.nc; ++c) {
    const u32 e = r.ends[c];
    if (e < begin + 3 || e > begin + 4) { *what = "contour size"; return false; }
    const u32 m = e - begin;
    i64 area2 = 0;
    bool convex = true;
    for (u32 i = 0; i < m; ++i) {
      const Point a = r.pts[begin + i], b = r.pts[begin + (i + 1) % m], d = r.pts[begin + (i + 2) % m];
      area2 += static_cast<i64>(a.x) * b.y - static_cast<i64>(b.x) * a.y;
      if (cross3(a, b, d) <= 0) convex = false;
      if (a.x >= (1 << 29) || a.x <= -(1 << 29) || a.y >= (1 << 29) || a.y <= -(1 << 29)) { *what = "range29"; return false; }
    }
    if (area2 < 0) { *what = "clockwise"; return false; }
    if (m == 4 && !convex) { *what = "quad not strictly convex"; return false; }
    begin = e;
  }
  // everything past the used part is untouched
  for (u32 i = r.np; i < r.pts.size(); ++i)
    if (r.pts[i].x != kSentinelCoord || r.pts[i].y != kSentinelCoord) { *what = "wrote past np"; return false; }
  for (u32 i = r.nc; i < r.ends.size(); ++i)
    if (r.ends[i] != kSentinelWord) { *what = "wrote past nc"; return false; }
  return true;
}

void feed(Fnv& f, const Result& r) {
  f.word(r.nc);
  f.word(r.np);
  for (u32 i = 0; i < r.np; ++i) {
    f.word(static_cast<u32>(r.pts[i].x));
    f.word(static_cast<u32>(r.pts[i].y));
  }
  for (u32 i = 0; i < r.nc; ++i) {
    f.word(r.ends[i]);
    f.word(0);
  }
}

// The style grid used by kGridLines (same order as the Python generator).
std::vector<Style> style_grid() {
  const std::int32_t hm[5][2] = {{1, 1}, {2, 2}, {3, 4}, {7, 64}, {100, 2}};
  std::vector<Style> v;
  for (int cl = 0; cl < 2; ++cl)
    for (int c = 0; c < 2; ++c)
      for (int j = 0; j < 2; ++j)
        for (const auto& x : hm) v.push_back(Style{x[0], x[1], j ? Join::Miter : Join::Bevel, c ? Cap::Square : Cap::Butt, cl != 0});
  return v;
}

void require_well_formed(const Pts& in, const Style& s, const Result& r) {
  const char* what = "";
  const bool ok = well_formed(in, s, r, &what);
  if (!ok) INFO("property failed: " << what << " n=" << in.size() << " h=" << s.half_width);
  REQUIRE(ok);
}

}  // namespace

TEST_CASE("status_text and enum values", "[stroke]") {
  REQUIRE(std::string_view(status_text(Status::Ok)) == "ok");
  REQUIRE(static_cast<int>(Status::Overlap) == 6);
  REQUIRE(std::string_view(status_text(Status::NullBuffer)).size() > 0);
  REQUIRE(std::string_view(status_text(Status::BadCount)).size() > 0);
  REQUIRE(std::string_view(status_text(Status::BadStyle)).size() > 0);
  REQUIRE(std::string_view(status_text(Status::BadPoint)).size() > 0);
  REQUIRE(std::string_view(status_text(Status::BufferTooSmall)) == "buffer too small");
  REQUIRE(std::string_view(status_text(Status::Overlap)) == "overlap");
}

TEST_CASE("bounds: contour_bound and point_bound", "[stroke]") {
  REQUIRE(contour_bound(2, false) == 3);
  REQUIRE(contour_bound(2, true) == 4);
  REQUIRE(contour_bound(3, false) == 5);
  REQUIRE(contour_bound(1024, false) == 2047);
  REQUIRE(contour_bound(1024, true) == 2048);
  REQUIRE(point_bound(1024, true) == 8192);
  u64 so = 0, sc = 0;
  for (u32 n = 0; n <= 1030; ++n) {
    const bool valid = n >= 2 && n <= 1024;
    REQUIRE(contour_bound(n, false) == (valid ? 2 * n - 1 : 0));
    REQUIRE(contour_bound(n, true) == (valid ? 2 * n : 0));
    REQUIRE(point_bound(n, false) == 4 * contour_bound(n, false));
    REQUIRE(point_bound(n, true) == 4 * contour_bound(n, true));
    so += contour_bound(n, false);
    sc += contour_bound(n, true);
  }
  REQUIRE(so == kBoundSumOpen);
  REQUIRE(sc == kBoundSumClosed);
}

TEST_CASE("isqrt_nearest: definition, exhaustive small values and golden samples", "[stroke]") {
  REQUIRE(isqrt_nearest(0) == 0);
  REQUIRE(isqrt_nearest(1) == 1);
  REQUIRE(isqrt_nearest(2) == 1);  // 1 * 2 >= 2 > 0
  REQUIRE(isqrt_nearest(3) == 2);
  REQUIRE(isqrt_nearest(6) == 2);
  REQUIRE(isqrt_nearest(7) == 3);
  for (u64 d = 1; d <= 70000; ++d) {
    const u64 L = isqrt_nearest(d);
    REQUIRE(L * (L - 1) < d);
    REQUIRE(d <= L * (L + 1));
  }
  // beyond the contract (d2 > 2^60) the result saturates only where it must
  REQUIRE(isqrt_nearest(0xFFFFFFFFFFFFFFFFull) == 0xFFFFFFFFu);
  REQUIRE(isqrt_nearest(1ull << 62) == (1u << 31));
  REQUIRE(isqrt_nearest(1ull << 60) == (1u << 30));
  // golden samples (squares, neighbours, powers of two, random up to 2^60)
  std::vector<u64> vals;
  for (u64 L = 1; L <= 3000; ++L) {
    vals.push_back(L * L - L);
    vals.push_back(L * L - L + 1);
    vals.push_back(L * L + L);
    vals.push_back(L * L + L + 1);
  }
  {
    std::vector<u64> keep;
    for (u64 v : vals) if (v >= 1) keep.push_back(v);
    vals = keep;
  }
  for (int k = 1; k <= 30; ++k) {
    const u64 p = 1ull << k;
    for (u64 L : {p - 1, p, p + 1}) {
      if (L >= 2 && L <= (1ull << 30) - 1) {
        vals.push_back(L * L - L);
        vals.push_back(L * L - L + 1);
        vals.push_back(L * L + L);
      }
    }
  }
  Lcg g{77};
  for (int i = 0; i < 3000; ++i) {
    const u64 a = g.next();
    const u64 b = g.next();
    vals.push_back(((a << 28) ^ b) % (1ull << 60) + 1);
  }
  vals.push_back(1ull << 60);
  REQUIRE(vals.size() == kIsqrtSamples);
  Fnv f;
  for (u64 v : vals) {
    const u64 L = isqrt_nearest(v);
    REQUIRE(L * (L - 1) < v);
    REQUIRE(v <= L * (L + 1));
    f.word(static_cast<u32>(L));
  }
  REQUIRE(f.h == kIsqrtDigest);
}

TEST_CASE("named cases: explicit contours from the reference", "[stroke]") {
  for (const NamedCase& c : kNamed) {
    INFO(c.name);
    const Result r = run(c.in, c.style);
    require_well_formed(c.in, c.style, r);
    REQUIRE(r.nc == c.ends.size());
    REQUIRE(r.np == c.pts.size());
    for (u32 i = 0; i < r.nc; ++i) REQUIRE(r.ends[i] == c.ends[i]);
    for (u32 i = 0; i < r.np; ++i) {
      REQUIRE(r.pts[i].x == c.pts[i].x);
      REQUIRE(r.pts[i].y == c.pts[i].y);
    }
    // the digest of the module agrees with the words written here
    Fnv f;
    for (u32 i = 0; i < r.np; ++i) {
      f.word(static_cast<u32>(r.pts[i].x));
      f.word(static_cast<u32>(r.pts[i].y));
    }
    REQUIRE(digest_points(r.pts.data(), r.np) == f.h);
  }
}

TEST_CASE("the three join counterexamples become bevel triangles", "[stroke]") {
  // zero summed outer normal: (0,0),(0,3),(1,0) with h = 1
  {
    const Pts in{{0, 0}, {0, 3}, {1, 0}};
    const Result r = run(in, Style{1, 64, Join::Miter, Cap::Butt, false});
    REQUIRE(r.nc == 3);
    REQUIRE(r.ends[2] - r.ends[1] == 3);  // the join is a triangle, not a miter quad
  }
  // reflex and self-intersecting miter quads before the convexity rule
  for (const Pts& in : {Pts{{0, 0}, {0, 1}, {1, 2}}, Pts{{0, 0}, {1, 1}, {2, 3}}}) {
    const Result r = run(in, Style{3, 64, Join::Miter, Cap::Butt, false});
    REQUIRE(r.nc == 3);
    REQUIRE(r.ends[2] - r.ends[1] == 3);
    REQUIRE(cross3(r.pts[r.ends[1]], r.pts[r.ends[1] + 1], r.pts[r.ends[1] + 2]) > 0);
  }
  // a zero area bevel is still emitted, listed as p, a, b
  {
    const Pts in{{0, 0}, {0, 1}, {1, 4}};
    const Result r = run(in, Style{1, 1, Join::Bevel, Cap::Butt, false});
    REQUIRE(r.nc == 3);
    REQUIRE(r.ends[2] - r.ends[1] == 3);
    const Point* t = &r.pts[r.ends[1]];
    REQUIRE(cross3(t[0], t[1], t[2]) == 0);
  }
}

TEST_CASE("style grid aggregates per line", "[stroke]") {
  const std::vector<Style> sg = style_grid();
  REQUIRE(sg.size() == kStyleGridCount);
  for (const GridLine& g : kGridLines) {
    Fnv f;
    u64 nc = 0, np = 0;
    for (const Style& s : sg) {
      const Result r = run(g.in, s);
      require_well_formed(g.in, s, r);
      nc += r.nc;
      np += r.np;
      feed(f, r);
    }
    REQUIRE(nc == g.contours);
    REQUIRE(np == g.points);
    REQUIRE(f.h == g.digest);
  }
}

TEST_CASE("exhaustive small grids match the reference digests", "[stroke]") {
  auto sweep = [](const ExhGold& gold, int side, int npts, const std::vector<std::pair<int, int>>& hml) {
    INFO(gold.name);
    std::vector<Point> grid;
    for (int x = 0; x < side; ++x)
      for (int y = 0; y < side; ++y) grid.push_back(Point{x, y});
    const u32 g = static_cast<u32>(grid.size());
    u64 total = 1;
    for (int i = 0; i < npts; ++i) total *= g;
    Fnv f;
    u64 runs = 0, nc = 0, np = 0;
    Pts in(static_cast<std::size_t>(npts));
    for (u64 code = 0; code < total; ++code) {
      u64 c = code;
      for (int i = npts - 1; i >= 0; --i) {  // first point is the most significant digit (product order)
        in[static_cast<std::size_t>(i)] = grid[c % g];
        c /= g;
      }
      for (const auto& x : hml)
        for (int cl = 0; cl < 2; ++cl)
          for (int cp = 0; cp < 2; ++cp)
            for (int j = 0; j < 2; ++j) {
              const Style s{x.first, x.second, j ? Join::Miter : Join::Bevel, cp ? Cap::Square : Cap::Butt, cl != 0};
              const Result r = run(in, s);
              const char* what = "";
              if (!well_formed(in, s, r, &what)) {
                INFO("property failed: " << what << " code " << code);
                REQUIRE(false);
              }
              ++runs;
              nc += r.nc;
              np += r.np;
              feed(f, r);
            }
    }
    REQUIRE(runs == gold.runs);
    REQUIRE(nc == gold.contours);
    REQUIRE(np == gold.points);
    REQUIRE(f.h == gold.digest);
  };
  sweep(kExh[0], 4, 3, {{1, 64}, {2, 2}, {3, 64}, {5, 1}});
  sweep(kExh[1], 3, 4, {{1, 64}, {3, 2}});
  sweep(kExh[2], 3, 2, {{1, 64}, {2, 1}, {3, 64}});
}

TEST_CASE("random corpora match the reference digests", "[stroke]") {
  const std::int32_t hs[5] = {1, 2, 7, 100, 1 << 19};
  const std::int32_t ms[4] = {1, 2, 4, 64};
  for (const RandGold& gold : kRand) {
    Lcg g{gold.seed};
    Fnv f;
    u64 nc = 0, np = 0;
    for (int i = 0; i < 600; ++i) {
      const std::int32_t n = g.rng(2, 12);
      Pts in;
      for (int k = 0; k < n; ++k) {
        const std::int32_t x = g.rng(static_cast<std::int32_t>(-gold.range), static_cast<std::int32_t>(gold.range));
        const std::int32_t y = g.rng(static_cast<std::int32_t>(-gold.range), static_cast<std::int32_t>(gold.range));
        in.push_back(Point{x, y});
      }
      const std::int32_t sel = g.rng(0, 5);
      const std::int32_t h = sel < 5 ? hs[sel] : g.rng(1, 1 << 19);
      const std::int32_t ml = ms[g.rng(0, 3)];
      const std::int32_t j = g.rng(0, 1);
      const std::int32_t c = g.rng(0, 1);
      const bool cl = g.rng(0, 3) == 0;
      const Style s{h, ml, j ? Join::Miter : Join::Bevel, c ? Cap::Square : Cap::Butt, cl};
      const Result r = run(in, s);
      require_well_formed(in, s, r);
      nc += r.nc;
      np += r.np;
      feed(f, r);
    }
    REQUIRE(nc == gold.contours);
    REQUIRE(np == gold.points);
    REQUIRE(f.h == gold.digest);
  }
}

TEST_CASE("segment quads: exact normals and winding for every small direction", "[stroke]") {
  // quad (p - n, q - n, q + n, p + n); n is the nearest rounded left normal; check against the definition with 64-bit only
  for (int h = 1; h <= 9; ++h)
    for (int dx = -9; dx <= 9; ++dx)
      for (int dy = -9; dy <= 9; ++dy) {
        if (dx == 0 && dy == 0) continue;
        const Pts in{{0, 0}, {dx, dy}};
        const Result r = run(in, Style{h, 1, Join::Bevel, Cap::Butt, false});
        REQUIRE(r.st == Status::Ok);
        REQUIRE(r.nc == 1);
        const u64 d2 = static_cast<u64>(dx * dx + dy * dy);
        const i64 L = isqrt_nearest(d2);
        auto rnd = [&](i64 num) {
          const i64 a = num < 0 ? -num : num;
          i64 q = a / L;
          if (2 * (a % L) >= L) ++q;
          return num < 0 ? -q : q;
        };
        const i64 nx = rnd(-static_cast<i64>(dy) * h), ny = rnd(static_cast<i64>(dx) * h);
        REQUIRE(r.pts[0].x == -nx);
        REQUIRE(r.pts[0].y == -ny);
        REQUIRE(r.pts[1].x == dx - nx);
        REQUIRE(r.pts[2].y == dy + ny);
        REQUIRE(r.pts[3].x == nx);
        REQUIRE(cross3(r.pts[0], r.pts[1], r.pts[2]) > 0);
        REQUIRE(cross3(r.pts[1], r.pts[2], r.pts[3]) > 0);
        REQUIRE(nx <= h);
        REQUIRE(-nx <= h);
        REQUIRE(ny <= h);
        REQUIRE(-ny <= h);
      }
}

TEST_CASE("statuses, check order and no write on error", "[stroke]") {
  const Pts in{{0, 0}, {10, 0}, {10, 10}};
  const Style ok{2, 4, Join::Miter, Cap::Square, false};
  const u32 n = 3, cb = contour_bound(n, false), pb = point_bound(n, false);
  std::vector<Point> pts(pb, Point{kSentinelCoord, kSentinelCoord});
  std::vector<u32> ends(cb, kSentinelWord);
  u32 nc = kSentinelWord, np = kSentinelWord;
  auto untouched = [&] {
    for (const Point& p : pts) if (p.x != kSentinelCoord || p.y != kSentinelCoord) return false;
    for (u32 e : ends) if (e != kSentinelWord) return false;
    return nc == kSentinelWord && np == kSentinelWord;
  };
  auto call = [&](const Point* i, u32 cnt, const Style& s, Point* o, u32 oc, u32* e, u32 ec, u32* c1, u32* c2) {
    return stroke_polyline(i, cnt, s, o, oc, e, ec, c1, c2);
  };
  // NullBuffer: each pointer on its own, and before BadCount and BadStyle
  REQUIRE(call(nullptr, n, ok, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(in.data(), n, ok, nullptr, pb, ends.data(), cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(in.data(), n, ok, pts.data(), pb, nullptr, cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(in.data(), n, ok, pts.data(), pb, ends.data(), cb, nullptr, &np) == Status::NullBuffer);
  REQUIRE(call(in.data(), n, ok, pts.data(), pb, ends.data(), cb, &nc, nullptr) == Status::NullBuffer);
  REQUIRE(call(nullptr, 0, Style{0, 0, Join::Miter, Cap::Butt, false}, nullptr, 0, nullptr, 0, nullptr, nullptr) == Status::NullBuffer);
  // BadCount before BadStyle, BadPoint, capacity
  for (u32 bad : {0u, 1u, 1025u, 0xFFFFFFFFu})
    REQUIRE(call(in.data(), bad, Style{0, 0, Join::Miter, Cap::Butt, false}, pts.data(), 0, ends.data(), 0, &nc, &np) == Status::BadCount);
  // BadStyle: every field, also when the miter limit is unused (Bevel)
  REQUIRE(call(in.data(), n, Style{0, 4, Join::Miter, Cap::Square, false}, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  REQUIRE(call(in.data(), n, Style{-1, 4, Join::Miter, Cap::Square, false}, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  REQUIRE(call(in.data(), n, Style{(1 << 19) + 1, 4, Join::Miter, Cap::Square, false}, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  REQUIRE(call(in.data(), n, Style{2, 0, Join::Bevel, Cap::Butt, false}, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  REQUIRE(call(in.data(), n, Style{2, 65, Join::Bevel, Cap::Butt, false}, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  REQUIRE(call(in.data(), n, Style{2, -3, Join::Miter, Cap::Butt, true}, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  {
    Style s = ok;
    std::uint8_t raw = 2;
    std::memcpy(&s.join, &raw, 1);
    REQUIRE(call(in.data(), n, s, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
    s = ok;
    raw = 200;
    std::memcpy(&s.cap, &raw, 1);
    REQUIRE(call(in.data(), n, s, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BadStyle);
  }
  // BadStyle before BadPoint and capacity
  {
    Pts bad = in;
    bad[1].x = K + 1;
    REQUIRE(call(bad.data(), n, Style{0, 4, Join::Miter, Cap::Butt, false}, pts.data(), 0, ends.data(), 0, &nc, &np) == Status::BadStyle);
    // BadPoint: every coordinate, both signs, any index; the limit itself is fine
    for (u32 i = 0; i < n; ++i)
      for (int comp = 0; comp < 2; ++comp)
        for (std::int32_t v : {K + 1, -K - 1, 0x7FFFFFFF, static_cast<std::int32_t>(0x80000000)}) {
          Pts b2 = in;
          (comp ? b2[i].y : b2[i].x) = v;
          REQUIRE(call(b2.data(), n, ok, pts.data(), 0, ends.data(), 0, &nc, &np) == Status::BadPoint);  // before capacity
        }
    Pts edge{{-K, K}, {K, -K}, {K, K}};
    std::vector<Point> p2(point_bound(3, false));
    std::vector<u32> e2(contour_bound(3, false));
    u32 a = 0, b = 0;
    REQUIRE(stroke_polyline(edge.data(), 3, ok, p2.data(), static_cast<u32>(p2.size()), e2.data(), static_cast<u32>(e2.size()), &a, &b) == Status::Ok);
  }
  // BufferTooSmall: one word short each, data independent; the closed bounds differ from the open ones
  REQUIRE(call(in.data(), n, ok, pts.data(), pb - 1, ends.data(), cb, &nc, &np) == Status::BufferTooSmall);
  REQUIRE(call(in.data(), n, ok, pts.data(), pb, ends.data(), cb - 1, &nc, &np) == Status::BufferTooSmall);
  REQUIRE(call(in.data(), n, ok, pts.data(), 0, ends.data(), 0, &nc, &np) == Status::BufferTooSmall);
  {
    Style c = ok;
    c.closed = true;
    REQUIRE(call(in.data(), n, c, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::BufferTooSmall);  // open sized buffers
    // all points equal still needs the full bound
    const Pts same{{4, 4}, {4, 4}, {4, 4}};
    REQUIRE(call(same.data(), n, ok, pts.data(), pb - 1, ends.data(), cb, &nc, &np) == Status::BufferTooSmall);
  }
  REQUIRE(untouched());
  // Ok with exactly the bound
  REQUIRE(call(in.data(), n, ok, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::Ok);
  REQUIRE(nc == 5);
  REQUIRE(np == 20);
}

TEST_CASE("Overlap: every pair of regions, edges, counters and check order", "[stroke]") {
  const Pts in{{0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 5}};
  const u32 n = 5;
  for (int closed = 0; closed < 2; ++closed) {
    const Style s{2, 4, Join::Miter, Cap::Square, closed != 0};
    const u32 cb = contour_bound(n, closed != 0), pb = point_bound(n, closed != 0);
    const u64 size[5] = {n * 8ull, pb * 8ull, cb * 4ull, 4, 4};  // in, pts, ends, count, point count
    constexpr std::size_t kSlot = 8192;                          // bytes between the regions' home positions
    for (int a = 0; a < 5; ++a)
      for (int b = 0; b < 5; ++b) {
        if (a == b) continue;
        // offsets of b relative to the start of a: first word, last word of a, b ends on a's first word, then two touching cases
        const i64 sa = static_cast<i64>(size[a]), sb = static_cast<i64>(size[b]);
        const i64 offs[5] = {0, sa - 4, -(sb - 4), sa, -sb};
        for (int o = 0; o < 5; ++o) {
          std::vector<u64> arena(5 * kSlot / 8 + 64, 0);
          auto base = reinterpret_cast<unsigned char*>(arena.data());
          std::size_t pos[5];
          for (int i = 0; i < 5; ++i) pos[i] = 1024 + static_cast<std::size_t>(i) * kSlot;
          pos[b] = static_cast<std::size_t>(static_cast<i64>(pos[a]) + offs[o]);
          // regions other than a and b stay in their own slots; a and b's neighbourhood holds nothing else
          std::memset(base, 0xA5, arena.size() * 8);
          std::vector<unsigned char> before(base, base + arena.size() * 8);
          const Point* pin = reinterpret_cast<const Point*>(base + pos[0]);
          Point* pp = reinterpret_cast<Point*>(base + pos[1]);
          u32* pe = reinterpret_cast<u32*>(base + pos[2]);
          u32* c1 = reinterpret_cast<u32*>(base + pos[3]);
          u32* c2 = reinterpret_cast<u32*>(base + pos[4]);
          std::memcpy(const_cast<Point*>(pin), in.data(), n * sizeof(Point));
          before.assign(base, base + arena.size() * 8);
          const Status st = stroke_polyline(pin, n, s, pp, pb, pe, cb, c1, c2);
          const bool expect_overlap = o < 3;
          INFO("a=" << a << " b=" << b << " off=" << o << " closed=" << closed);
          if (expect_overlap) {
            REQUIRE(st == Status::Overlap);
            REQUIRE(std::memcmp(before.data(), base, arena.size() * 8) == 0);
          } else {
            REQUIRE(st == Status::Ok);
            REQUIRE(*c1 > 0);
          }
        }
      }
    // BufferTooSmall wins over Overlap, Overlap wins when the capacities are exact
    std::vector<Point> arena(pb + 8);
    std::memcpy(arena.data(), in.data(), n * sizeof(Point));
    u32 cnt[2] = {0, 0};
    std::vector<u32> ends(cb);
    REQUIRE(stroke_polyline(arena.data(), n, s, arena.data(), pb - 1, ends.data(), cb, &cnt[0], &cnt[1]) == Status::BufferTooSmall);
    REQUIRE(stroke_polyline(arena.data(), n, s, arena.data(), pb, ends.data(), cb, &cnt[0], &cnt[1]) == Status::Overlap);
    // the counters may not alias each other
    REQUIRE(stroke_polyline(in.data(), n, s, arena.data() + 0, pb, ends.data(), cb, &cnt[0], &cnt[0]) == Status::Overlap);
    // BadPoint is checked before Overlap
    Pts bad = in;
    bad[2].y = K + 1;
    std::vector<Point> shared(pb + 8);
    std::memcpy(shared.data(), bad.data(), n * sizeof(Point));
    REQUIRE(stroke_polyline(shared.data(), n, s, shared.data(), pb, ends.data(), cb, &cnt[0], &cnt[1]) == Status::BadPoint);
  }
}

TEST_CASE("point and ends extents are bounds, not capacities", "[stroke]") {
  // a larger capacity does not widen the touched extent: a region placed after the bound is no overlap
  const Pts in{{0, 0}, {10, 0}, {10, 10}};
  const u32 cb = contour_bound(3, false), pb = point_bound(3, false);
  std::vector<Point> pts(pb + 64);
  std::vector<u32> ends(cb);
  u32 nc = 0, np = 0;
  REQUIRE(stroke_polyline(in.data(), 3, Style{2, 4, Join::Miter, Cap::Square, false}, pts.data(), pb + 64, ends.data(), cb, &nc, &np) == Status::Ok);
  u32* tail = reinterpret_cast<u32*>(pts.data() + pb);  // inside the capacity, past the bound
  REQUIRE(stroke_polyline(in.data(), 3, Style{2, 4, Join::Miter, Cap::Square, false}, pts.data(), pb + 64, ends.data(), cb, tail, tail + 1) == Status::Ok);
}

TEST_CASE("results are deterministic and independent of buffer contents", "[stroke]") {
  Lcg g{99};
  for (int i = 0; i < 200; ++i) {
    Pts in;
    const int n = g.rng(2, 20);
    for (int k = 0; k < n; ++k) in.push_back(Point{g.rng(-50, 50), g.rng(-50, 50)});
    const Style s{g.rng(1, 20), g.rng(1, 64), g.rng(0, 1) ? Join::Miter : Join::Bevel, g.rng(0, 1) ? Cap::Square : Cap::Butt, g.rng(0, 1) != 0};
    const Result r1 = run(in, s);
    // a second run with a different fill pattern in the buffers
    const u32 cb = contour_bound(static_cast<u32>(n), s.closed), pb = point_bound(static_cast<u32>(n), s.closed);
    std::vector<Point> pts(pb, Point{123, -456});
    std::vector<u32> ends(cb, 77);
    u32 nc = 5, np = 6;
    REQUIRE(stroke_polyline(in.data(), static_cast<u32>(n), s, pts.data(), pb, ends.data(), cb, &nc, &np) == Status::Ok);
    REQUIRE(nc == r1.nc);
    REQUIRE(np == r1.np);
    for (u32 k = 0; k < np; ++k) REQUIRE(pts[k] == r1.pts[k]);
    for (u32 k = 0; k < nc; ++k) REQUIRE(ends[k] == r1.ends[k]);
  }
}

TEST_CASE("performance: 1024 points (sanitizer safe) match the reference", "[stroke][perf]") {
  Lcg g{7};
  Pts in;
  for (int i = 0; i < 1024; ++i) {
    const std::int32_t x = g.rng(-K, K);
    const std::int32_t y = g.rng(-K, K);
    in.push_back(Point{x, y});
  }
  struct Row { Style s; u64 contours, points, digest; };
  const Row rows[3] = {{Style{1 << 19, 64, Join::Miter, Cap::Square, false}, kPerfopenContours, kPerfopenPoints, kPerfopenDigest},
                       {Style{1 << 19, 64, Join::Miter, Cap::Butt, true}, kPerfclosedContours, kPerfclosedPoints, kPerfclosedDigest},
                       {Style{1, 64, Join::Miter, Cap::Square, false}, kPerfthinContours, kPerfthinPoints, kPerfthinDigest}};
  for (const Row& row : rows) {
    const Result r = run(in, row.s);
    require_well_formed(in, row.s, r);
    REQUIRE(r.nc == row.contours);
    REQUIRE(r.np == row.points);
    Fnv f;
    for (u32 i = 0; i < r.np; ++i) {
      f.word(static_cast<u32>(r.pts[i].x));
      f.word(static_cast<u32>(r.pts[i].y));
    }
    for (u32 i = 0; i < r.nc; ++i) {
      f.word(r.ends[i]);
      f.word(0);
    }
    REQUIRE(f.h == row.digest);
  }
}
