// Tests for the outline builder (scope Y). Expected values come from an independent Python reference (a subpath state machine over
// explicit lists, exact-integer Bernstein flattening, its own FNV and generator) and are pasted here as constants. Every successful
// output is also checked by structural properties written in this file. See docs/outline.md.
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <array>
#include <cstring>
#include <string_view>
#include <vector>

#include "outline.h"

namespace lumora::flatten {
inline bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Point& a, const Point& b) { return !(a == b); }
}  // namespace lumora::flatten

using namespace lumora::outline;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;
using Pts = std::vector<Point>;
using Cmds = std::vector<Command>;

constexpr std::int32_t K = lumora::flatten::kMaxCoord;
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

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

struct NamedPath {
  const char* name;
  Cmds cmds;
  std::int32_t tol;
  Pts pts;
  std::vector<u32> ends;
  std::vector<std::uint8_t> closed;
};
struct RandGold {
  u32 seed;
  i64 range;
  u64 ok, bad, contours, points, digest;
};
struct DigestCase {
  const char* name;
  u32 count;
  Pts pts;
  std::vector<u32> ends;
  std::vector<std::uint8_t> closed;
  u64 digest;
};

static const NamedPath kNamed[] = {
  {"single_move", {{Op::Move,{3,3},{0,0},{0,0}}}, 1, {}, {}, {}},
  {"move_close", {{Op::Move,{3,3},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {}, {}, {}},
  {"line_open", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{10,0},{0,0},{0,0}}}, 1, {{0,0},{10,0}}, {2}, {0}},
  {"polyline_open", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{10,0},{0,0},{0,0}},{Op::Line,{10,10},{0,0},{0,0}}}, 1, {{0,0},{10,0},{10,10}}, {3}, {0}},
  {"triangle_closed", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{10,0},{0,0},{0,0}},{Op::Line,{10,10},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{10,0},{10,10}}, {3}, {1}},
  {"closing_point_dropped", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{10,0},{0,0},{0,0}},{Op::Line,{10,10},{0,0},{0,0}},{Op::Line,{0,0},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{10,0},{10,10}}, {3}, {1}},
  {"closing_not_closed_keeps_last", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{10,0},{0,0},{0,0}},{Op::Line,{10,10},{0,0},{0,0}},{Op::Line,{0,0},{0,0},{0,0}}}, 1, {{0,0},{10,0},{10,10},{0,0}}, {4}, {0}},
  {"two_point_closed", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{5,0},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{5,0}}, {2}, {1}},
  {"two_point_closed_by_return", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{5,0},{0,0},{0,0}},{Op::Line,{0,0},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{5,0}}, {2}, {1}},
  {"dup_lines_collapse", {{Op::Move,{1,1},{0,0},{0,0}},{Op::Line,{1,1},{0,0},{0,0}},{Op::Line,{2,2},{0,0},{0,0}},{Op::Line,{2,2},{0,0},{0,0}},{Op::Line,{2,2},{0,0},{0,0}},{Op::Line,{3,3},{0,0},{0,0}}}, 1, {{1,1},{2,2},{3,3}}, {3}, {0}},
  {"all_equal_dropped", {{Op::Move,{4,4},{0,0},{0,0}},{Op::Line,{4,4},{0,0},{0,0}},{Op::Line,{4,4},{0,0},{0,0}}}, 1, {}, {}, {}},
  {"all_equal_closed_dropped", {{Op::Move,{4,4},{0,0},{0,0}},{Op::Line,{4,4},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {}, {}, {}},
  {"line_back_to_start_closed", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{4,4},{0,0},{0,0}},{Op::Line,{0,0},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{4,4}}, {2}, {1}},
  {"two_subpaths", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{5,0},{0,0},{0,0}},{Op::Move,{9,9},{0,0},{0,0}},{Op::Line,{9,20},{0,0},{0,0}},{Op::Line,{20,20},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{5,0},{9,9},{9,20},{20,20}}, {2,5}, {0,1}},
  {"move_move", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Move,{1,1},{0,0},{0,0}},{Op::Line,{5,5},{0,0},{0,0}}}, 1, {{1,1},{5,5}}, {2}, {0}},
  {"move_after_close", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{5,0},{0,0},{0,0}},{Op::Line,{5,5},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}},{Op::Move,{7,7},{0,0},{0,0}},{Op::Line,{8,8},{0,0},{0,0}}}, 1, {{0,0},{5,0},{5,5},{7,7},{8,8}}, {3,5}, {1,0}},
  {"open_then_move_emits", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Line,{5,0},{0,0},{0,0}},{Op::Move,{1,1},{0,0},{0,0}}}, 1, {{0,0},{5,0}}, {2}, {0}},
  {"quad_open", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Quad,{50,100},{100,0},{0,0}}}, 5, {{0,0},{25,38},{50,50},{75,38},{100,0}}, {5}, {0}},
  {"quad_closed", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Quad,{50,100},{100,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 5, {{0,0},{25,38},{50,50},{75,38},{100,0}}, {5}, {1}},
  {"cubic_open", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Cubic,{0,100},{100,-100},{100,0}}}, 5, {{0,0},{6,26},{20,26},{39,10},{61,-10},{80,-26},{94,-26},{100,0}}, {8}, {0}},
  {"cubic_closed_loop", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Cubic,{100,100},{-100,100},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 5, {{0,0},{26,37},{26,61},{10,73},{-10,73},{-26,61},{-26,37}}, {7}, {1}},
  {"curve_to_same_point", {{Op::Move,{7,7},{0,0},{0,0}},{Op::Quad,{7,7},{7,7},{0,0}}}, 1, {}, {}, {}},
  {"curve_line_curve", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Quad,{30,60},{60,0},{0,0}},{Op::Line,{60,30},{0,0},{0,0}},{Op::Cubic,{80,60},{40,60},{0,30}}}, 4, {{0,0},{20,27},{40,27},{60,0},{60,30},{65,47},{53,53},{29,47},{0,30}}, {9}, {0}},
  {"curve_then_line_dedup", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Quad,{10,0},{20,0},{0,0}},{Op::Line,{20,0},{0,0},{0,0}},{Op::Line,{30,0},{0,0},{0,0}}}, 1, {{0,0},{20,0},{30,0}}, {3}, {0}},
  {"curve_end_equals_start_open", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Quad,{10,10},{0,0},{0,0}}}, 1, {{0,0},{4,4},{0,0}}, {3}, {0}},
  {"close_after_curve_to_start", {{Op::Move,{0,0},{0,0},{0,0}},{Op::Quad,{20,40},{0,0},{0,0}},{Op::Close,{0,0},{0,0},{0,0}}}, 1, {{0,0},{6,13},{10,19},{6,13}}, {4}, {1}},
  {"extreme_line", {{Op::Move,{-16777216,-16777216},{0,0},{0,0}},{Op::Line,{16777216,16777216},{0,0},{0,0}}}, 1, {{-16777216,-16777216},{16777216,16777216}}, {2}, {0}},
  {"extreme_curves", {{Op::Move,{-16777216,-16777216},{0,0},{0,0}},{Op::Quad,{16777216,16777216},{-16777216,16777216},{0,0}},{Op::Cubic,{16777216,-16777216},{-16777216,-16777216},{16777216,16777216}},{Op::Close,{0,0},{0,0},{0,0}}}, 3000, {{-16777216,-16777216},{-15948841,-15943598},{-15141437,-15120466},{-14355005,-14307820},{-13589545,-13505659},{-12845056,-12713984},{-12121539,-11932795},{-11418993,-11162092},{-10737418,-10401874},{-10076815,-9652142},{-9437184,-8912896},{-8818524,-8184136},{-8220836,-7465861},{-7644119,-6758072},{-7088374,-6060769},{-6553600,-5373952},{-6039798,-4697620},{-5546967,-4031775},{-5075108,-3376415},{-4624220,-2731540},{-4194304,-2097152},{-3785359,-1473249},{-3397386,-859832},{-3030385,-256901},{-2684355,335544},{-2359296,917504},{-2055209,1488978},{-1772093,2049966},{-1509949,2600468},{-1268777,3140485},{-1048576,3670016},{-849347,4189061},{-671089,4697620},{-513802,5195694},{-377487,5683282},{-262144,6160384},{-167772,6627000},{-94372,7083131},{-41943,7528776},{-10486,7963935},{0,8388608},{-10486,8802796},{-41943,9206497},{-94372,9599713},{-167772,9982444},{-262144,10354688},{-377487,10716447},{-513802,11067720},{-671089,11408507},{-849347,11738808},{-1048576,12058624},{-1268777,12367954},{-1509949,12666798},{-1772093,12955156},{-2055209,13233029},{-2359296,13500416},{-2684355,13757317},{-3030385,14003732},{-3397386,14239662},{-3785359,14465106},{-4194304,14680064},{-4624220,14884536},{-5075108,15078523},{-5546967,15262024},{-6039798,15435039},{-6553600,15597568},{-7088374,15749612},{-7644119,15891169},{-8220836,16022241},{-8818524,16142828},{-9437184,16252928},{-10076815,16352543},{-10737418,16441672},{-11418993,16520315},{-12121539,16588472},{-12845056,16646144},{-13589545,16693330},{-14355005,16730030},{-15141437,16756244},{-15948841,16771973},{-16777216,16777216},{-16053122,16047811},{-15350167,15329132},{-14668040,14621180},{-14006426,13923954},{-13365012,13237455},{-12743486,12561682},{-12141533,11896636},{-11558841,11242317},{-10995097,10598724},{-10449988,9965858},{-9923199,9343718},{-9414419,8732304},{-8923334,8131618},{-8449630,7541658},{-7992995,6962424},{-7553115,6393917},{-7129677,5836136},{-6722369,5289082},{-6330876,4752755},{-5954885,4227154},{-5594084,3712280},{-5248160,3208132},{-4916798,2714711},{-4599686,2232016},{-4296511,1760048},{-4006960,1298806},{-3730718,848291},{-3467474,408503},{-3216914,-20559},{-2978725,-438895},{-2752593,-846503},{-2538205,-1243386},{-2335249,-1629542},{-2143411,-2004971},{-1962378,-2369673},{-1791836,-2723649},{-1631473,-3066899},{-1480975,-3399422},{-1340029,-3721219},{-1208322,-4032288},{-1085541,-4332632},{-971372,-4622249},{-865503,-4901139},{-767620,-5169303},{-677410,-5426740},{-594560,-5673450},{-518757,-5909434},{-449686,-6134692},{-387037,-6349223},{-330494,-6553027},{-279745,-6746105},{-234477,-6928457},{-194376,-7100081},{-159130,-7260980},{-128425,-7411151},{-101948,-7550596},{-79386,-7679315},{-60425,-7797307},{-44753,-7904572},{-32056,-8001111},{-22021,-8086924},{-14335,-8162010},{-8684,-8226369},{-4756,-8280002},{-2238,-8322908},{-816,-8355088},{-176,-8376541},{-7,-8387267},{7,-8387267},{176,-8376541},{816,-8355088},{2238,-8322908},{4756,-8280002},{8684,-8226369},{14335,-8162010},{22021,-8086924},{32056,-8001111},{44753,-7904572},{60425,-7797307},{79386,-7679315},{101948,-7550596},{128425,-7411151},{159130,-7260980},{194376,-7100081},{234477,-6928457},{279745,-6746105},{330494,-6553027},{387037,-6349223},{449686,-6134692},{518757,-5909434},{594560,-5673450},{677410,-5426740},{767620,-5169303},{865503,-4901139},{971372,-4622249},{1085541,-4332632},{1208322,-4032288},{1340029,-3721219},{1480975,-3399422},{1631473,-3066899},{1791836,-2723649},{1962378,-2369673},{2143411,-2004971},{2335249,-1629542},{2538205,-1243386},{2752593,-846503},{2978725,-438895},{3216914,-20559},{3467474,408503},{3730718,848291},{4006960,1298806},{4296511,1760048},{4599686,2232016},{4916798,2714711},{5248160,3208132},{5594084,3712280},{5954885,4227154},{6330876,4752755},{6722369,5289082},{7129677,5836136},{7553115,6393917},{7992995,6962424},{8449630,7541658},{8923334,8131618},{9414419,8732304},{9923199,9343718},{10449988,9965858},{10995097,10598724},{11558841,11242317},{12141533,11896636},{12743486,12561682},{13365012,13237455},{14006426,13923954},{14668040,14621180},{15350167,15329132},{16053122,16047811},{16777216,16777216}}, {218}, {1}},
  {"extreme_clamped_curve", {{Op::Move,{16777216,16777216},{0,0},{0,0}},{Op::Quad,{-16777216,-16777216},{16777216,-16777216},{0,0}}}, 1, {{16777216,16777216},{16516096,16515584},{16257024,16254976},{16000000,15995392},{15745024,15736832},{15492096,15479296},{15241216,15222784},{14992384,14967296},{14745600,14712832},{14500864,14459392},{14258176,14206976},{14017536,13955584},{13778944,13705216},{13542400,13455872},{13307904,13207552},{13075456,12960256},{12845056,12713984},{12616704,12468736},{12390400,12224512},{12166144,11981312},{11943936,11739136},{11723776,11497984},{11505664,11257856},{11289600,11018752},{11075584,10780672},{10863616,10543616},{10653696,10307584},{10445824,10072576},{10240000,9838592},{10036224,9605632},{9834496,9373696},{9634816,9142784},{9437184,8912896},{9241600,8684032},{9048064,8456192},{8856576,8229376},{8667136,8003584},{8479744,7778816},{8294400,7555072},{8111104,7332352},{7929856,7110656},{7750656,6889984},{7573504,6670336},{7398400,6451712},{7225344,6234112},{7054336,6017536},{6885376,5801984},{6718464,5587456},{6553600,5373952},{6390784,5161472},{6230016,4950016},{6071296,4739584},{5914624,4530176},{5760000,4321792},{5607424,4114432},{5456896,3908096},{5308416,3702784},{5161984,3498496},{5017600,3295232},{4875264,3092992},{4734976,2891776},{4596736,2691584},{4460544,2492416},{4326400,2294272},{4194304,2097152},{4064256,1901056},{3936256,1705984},{3810304,1511936},{3686400,1318912},{3564544,1126912},{3444736,935936},{3326976,745984},{3211264,557056},{3097600,369152},{2985984,182272},{2876416,-3584},{2768896,-188416},{2663424,-372224},{2560000,-555008},{2458624,-736768},{2359296,-917504},{2262016,-1097216},{2166784,-1275904},{2073600,-1453568},{1982464,-1630208},{1893376,-1805824},{1806336,-1980416},{1721344,-2153984},{1638400,-2326528},{1557504,-2498048},{1478656,-2668544},{1401856,-2838016},{1327104,-3006464},{1254400,-3173888},{1183744,-3340288},{1115136,-3505664},{1048576,-3670016},{984064,-3833344},{921600,-3995648},{861184,-4156928},{802816,-4317184},{746496,-4476416},{692224,-4634624},{640000,-4791808},{589824,-4947968},{541696,-5103104},{495616,-5257216},{451584,-5410304},{409600,-5562368},{369664,-5713408},{331776,-5863424},{295936,-6012416},{262144,-6160384},{230400,-6307328},{200704,-6453248},{173056,-6598144},{147456,-6742016},{123904,-6884864},{102400,-7026688},{82944,-7167488},{65536,-7307264},{50176,-7446016},{36864,-7583744},{25600,-7720448},{16384,-7856128},{9216,-7990784},{4096,-8124416},{1024,-8257024},{0,-8388608},{1024,-8519168},{4096,-8648704},{9216,-8777216},{16384,-8904704},{25600,-9031168},{36864,-9156608},{50176,-9281024},{65536,-9404416},{82944,-9526784},{102400,-9648128},{123904,-9768448},{147456,-9887744},{173056,-10006016},{200704,-10123264},{230400,-10239488},{262144,-10354688},{295936,-10468864},{331776,-10582016},{369664,-10694144},{409600,-10805248},{451584,-10915328},{495616,-11024384},{541696,-11132416},{589824,-11239424},{640000,-11345408},{692224,-11450368},{746496,-11554304},{802816,-11657216},{861184,-11759104},{921600,-11859968},{984064,-11959808},{1048576,-12058624},{1115136,-12156416},{1183744,-12253184},{1254400,-12348928},{1327104,-12443648},{1401856,-12537344},{1478656,-12630016},{1557504,-12721664},{1638400,-12812288},{1721344,-12901888},{1806336,-12990464},{1893376,-13078016},{1982464,-13164544},{2073600,-13250048},{2166784,-13334528},{2262016,-13417984},{2359296,-13500416},{2458624,-13581824},{2560000,-13662208},{2663424,-13741568},{2768896,-13819904},{2876416,-13897216},{2985984,-13973504},{3097600,-14048768},{3211264,-14123008},{3326976,-14196224},{3444736,-14268416},{3564544,-14339584},{3686400,-14409728},{3810304,-14478848},{3936256,-14546944},{4064256,-14614016},{4194304,-14680064},{4326400,-14745088},{4460544,-14809088},{4596736,-14872064},{4734976,-14934016},{4875264,-14994944},{5017600,-15054848},{5161984,-15113728},{5308416,-15171584},{5456896,-15228416},{5607424,-15284224},{5760000,-15339008},{5914624,-15392768},{6071296,-15445504},{6230016,-15497216},{6390784,-15547904},{6553600,-15597568},{6718464,-15646208},{6885376,-15693824},{7054336,-15740416},{7225344,-15785984},{7398400,-15830528},{7573504,-15874048},{7750656,-15916544},{7929856,-15958016},{8111104,-15998464},{8294400,-16037888},{8479744,-16076288},{8667136,-16113664},{8856576,-16150016},{9048064,-16185344},{9241600,-16219648},{9437184,-16252928},{9634816,-16285184},{9834496,-16316416},{10036224,-16346624},{10240000,-16375808},{10445824,-16403968},{10653696,-16431104},{10863616,-16457216},{11075584,-16482304},{11289600,-16506368},{11505664,-16529408},{11723776,-16551424},{11943936,-16572416},{12166144,-16592384},{12390400,-16611328},{12616704,-16629248},{12845056,-16646144},{13075456,-16662016},{13307904,-16676864},{13542400,-16690688},{13778944,-16703488},{14017536,-16715264},{14258176,-16726016},{14500864,-16735744},{14745600,-16744448},{14992384,-16752128},{15241216,-16758784},{15492096,-16764416},{15745024,-16769024},{16000000,-16772608},{16257024,-16775168},{16516096,-16776704},{16777216,-16777216}}, {257}, {0}},
};
constexpr std::uint64_t kExhRuns = 336840, kExhOk = 48424, kExhBad = 288416, kExhPoints = 175157, kExhDigest = 0x9bae14de88008594ull;
static const RandGold kRand[] = {
  {21, 20, 239, 61, 794, 4404, 0x567267226a5836f7ull},
  {22, 2000, 247, 53, 816, 22735, 0x8f9ffdcd50ec2287ull},
  {23, 16777216, 248, 52, 839, 184148, 0x5cec743cec56e8d1ull},
};
constexpr std::uint64_t kPerfContours = 1, kPerfPoints = 196864, kPerfDigest = 0xb3f2ee15e0204dd6ull;
static const DigestCase kDigest[] = {
  {"empty", 0, {}, {}, {}, 0x4d25767f9dce13f5ull},
  {"one_open", 1, {{0,0},{5,5}}, {2}, {0}, 0x9167be256945b0f6ull},
  {"one_closed", 1, {{0,0},{5,5},{5,0}}, {3}, {1}, 0x8c462009ddc350e3ull},
  {"two_mixed", 2, {{0,0},{5,5},{-1,-2},{3,4},{7,-8}}, {2,5}, {0,1}, 0x49d228422e0e2d2bull},
};

struct Result {
  Status st;
  Pts pts;
  std::vector<u32> ends;
  std::vector<std::uint8_t> closed;
  u32 nc, np;
};

Result run(const Cmds& cmds, std::int32_t tol) {
  const u32 n = static_cast<u32>(cmds.size());
  const u32 cb = contour_bound(n), pb = point_bound(n);
  Result r;
  r.pts.assign(pb, Point{kSentinelCoord, kSentinelCoord});
  r.ends.assign(cb, kSentinelWord);
  r.closed.assign(cb, 0xEE);
  r.nc = r.np = kSentinelWord;
  r.st = build_outline(cmds.data(), n, tol, r.pts.data(), pb, r.ends.data(), cb, r.closed.data(), cb, &r.nc, &r.np);
  return r;
}

Command mv(std::int32_t x, std::int32_t y) { return Command{Op::Move, {x, y}, {0, 0}, {0, 0}}; }
Command ln(std::int32_t x, std::int32_t y) { return Command{Op::Line, {x, y}, {0, 0}, {0, 0}}; }
Command cl() { return Command{Op::Close, {0, 0}, {0, 0}, {0, 0}}; }

// Structural properties of a successful result.
bool well_formed(const Result& r, const char** what) {
  *what = "";
  if (r.st != Status::Ok) { *what = "status"; return false; }
  u32 begin = 0;
  for (u32 c = 0; c < r.nc; ++c) {
    const u32 e = r.ends[c];
    if (e < begin + 2) { *what = "contour under two points"; return false; }
    if (r.closed[c] > 1) { *what = "closed flag"; return false; }
    for (u32 i = begin + 1; i < e; ++i)
      if (r.pts[i] == r.pts[i - 1]) { *what = "consecutive duplicate"; return false; }
    if (r.closed[c] == 1 && r.pts[e - 1] == r.pts[begin]) { *what = "closed last equals first"; return false; }
    for (u32 i = begin; i < e; ++i)
      if (r.pts[i].x < -K || r.pts[i].x > K || r.pts[i].y < -K || r.pts[i].y > K) { *what = "range"; return false; }
    begin = e;
  }
  if (r.np != begin) { *what = "point count"; return false; }
  // pts past np may hold dropped contours' scratch points; ends and closed are written only for emitted contours
  for (u32 i = r.nc; i < r.ends.size(); ++i)
    if (r.ends[i] != kSentinelWord || r.closed[i] != 0xEE) { *what = "wrote past nc"; return false; }
  return true;
}

void feed(Fnv& f, const Result& r) {
  f.word(static_cast<u32>(r.st));
  if (r.st != Status::Ok) return;
  f.word(r.nc);
  f.word(r.np);
  for (u32 i = 0; i < r.nc; ++i) {
    f.word(r.ends[i]);
    f.word(r.closed[i]);
  }
  for (u32 i = 0; i < r.np; ++i) {
    f.word(static_cast<u32>(r.pts[i].x));
    f.word(static_cast<u32>(r.pts[i].y));
  }
}

void require_well_formed(const Result& r) {
  const char* what = "";
  const bool ok = well_formed(r, &what);
  if (!ok) INFO("property failed: " << what);
  REQUIRE(ok);
}

}  // namespace

TEST_CASE("status_text and bounds", "[outline]") {
  REQUIRE(std::string_view(status_text(Status::Ok)) == "ok");
  REQUIRE(std::string_view(status_text(Status::NullBuffer)) == "null buffer");
  REQUIRE(std::string_view(status_text(Status::BadCount)) == "bad count");
  REQUIRE(std::string_view(status_text(Status::BadTolerance)) == "bad tolerance");
  REQUIRE(std::string_view(status_text(Status::BadCommand)) == "bad command");
  REQUIRE(std::string_view(status_text(Status::BadPoint)) == "bad point");
  REQUIRE(std::string_view(status_text(Status::BadPath)) == "bad path");
  REQUIRE(std::string_view(status_text(Status::BufferTooSmall)) == "buffer too small");
  REQUIRE(std::string_view(status_text(Status::Overlap)) == "overlap");
  REQUIRE(std::string_view(status_text(static_cast<Status>(99))) == "unknown");
  REQUIRE(contour_bound(0) == 0);
  REQUIRE(point_bound(0) == 0);
  REQUIRE(contour_bound(1025) == 0);
  REQUIRE(point_bound(1025) == 0);
  REQUIRE(contour_bound(0xFFFFFFFFu) == 0);
  for (u32 n = 1; n <= 1024; ++n) {
    REQUIRE(contour_bound(n) == n);
    REQUIRE(point_bound(n) == 256 * n);
  }
  REQUIRE(point_bound(1024) == 262144);
}

TEST_CASE("digest_outline: binding hashing rules", "[outline]") {
  for (const DigestCase& d : kDigest) {
    INFO(d.name);
    REQUIRE(digest_outline(d.pts.data(), d.ends.data(), d.closed.data(), d.count) == d.digest);
  }
  Fnv zero;
  zero.word(0);
  REQUIRE(digest_outline(nullptr, nullptr, nullptr, 0) == zero.h);       // one zero word, not the bare basis
  REQUIRE(zero.h != 0xcbf29ce484222325ull);
  const Pts p{{1, 2}, {3, 4}, {5, 6}};
  const std::vector<u32> e{2, 3};
  const std::vector<std::uint8_t> c{1, 0};
  Fnv count_only;
  count_only.word(2);
  REQUIRE(digest_outline(p.data(), nullptr, c.data(), 2) == count_only.h);  // contour_ends null: stop after the count
  REQUIRE(digest_outline(p.data(), e.data(), nullptr, 2) == count_only.h);  // closed null: stop after the count
  REQUIRE(digest_outline(nullptr, nullptr, nullptr, 2) == count_only.h);
  Fnv no_pts;  // pts null: the count and the per-contour words only
  no_pts.word(2);
  no_pts.word(2);
  no_pts.word(1);
  no_pts.word(3);
  no_pts.word(0);
  REQUIRE(digest_outline(nullptr, e.data(), c.data(), 2) == no_pts.h);
  Fnv full = no_pts;
  for (const Point& q : p) {
    full.word(static_cast<u32>(q.x));
    full.word(static_cast<u32>(q.y));
  }
  REQUIRE(digest_outline(p.data(), e.data(), c.data(), 2) == full.h);
  // a nonzero closed byte hashes as 1
  const std::vector<std::uint8_t> c2{200, 0};
  REQUIRE(digest_outline(p.data(), e.data(), c2.data(), 2) == full.h);
  // points are hashed up to the last end, exclusive, not up to the buffer size
  const Pts longer{{1, 2}, {3, 4}, {5, 6}, {99, 99}};
  REQUIRE(digest_outline(longer.data(), e.data(), c.data(), 2) == full.h);
}

TEST_CASE("named paths: explicit expected outlines", "[outline]") {
  for (const NamedPath& n : kNamed) {
    INFO(n.name);
    const Result r = run(n.cmds, n.tol);
    require_well_formed(r);
    REQUIRE(r.nc == n.ends.size());
    REQUIRE(r.np == n.pts.size());
    for (u32 i = 0; i < r.nc; ++i) {
      REQUIRE(r.ends[i] == n.ends[i]);
      REQUIRE(r.closed[i] == n.closed[i]);
    }
    for (u32 i = 0; i < r.np; ++i) REQUIRE(r.pts[i] == n.pts[i]);
    REQUIRE(digest_outline(r.pts.data(), r.ends.data(), r.closed.data(), r.nc) != 0);
  }
}

TEST_CASE("exhaustive command sequences up to length four match the reference", "[outline]") {
  struct Pre { Point a, b, c; };
  const Pre pre[4] = {{{0, 0}, {0, 0}, {0, 0}}, {{8, 0}, {8, 8}, {0, 8}}, {{0, 0}, {8, 0}, {8, 8}}, {{4, 4}, {4, 4}, {8, 0}}};
  const Op ops[5] = {Op::Move, Op::Line, Op::Quad, Op::Cubic, Op::Close};
  Fnv f;
  u64 runs = 0, ok = 0, bad = 0, points = 0;
  for (int len = 1; len <= 4; ++len) {
    u64 op_total = 1, pat_total = 1;
    for (int i = 0; i < len; ++i) { op_total *= 5; pat_total *= 4; }
    for (u64 oc = 0; oc < op_total; ++oc) {
      int o[4];
      u64 t = oc;
      for (int i = len - 1; i >= 0; --i) { o[i] = static_cast<int>(t % 5); t /= 5; }
      for (u64 pc = 0; pc < pat_total; ++pc) {
        int pi[4];
        u64 u = pc;
        for (int i = len - 1; i >= 0; --i) { pi[i] = static_cast<int>(u % 4); u /= 4; }
        Cmds cm;
        for (int i = 0; i < len; ++i) {
          // fields not used by the op carry the preset values too; they must not matter
          cm.push_back(Command{ops[o[i]], pre[pi[i]].a, pre[pi[i]].b, pre[pi[i]].c});
        }
        for (std::int32_t tol : {1, 3}) {
          const Result r = run(cm, tol);
          ++runs;
          if (r.st == Status::Ok) {
            const char* what = "";
            if (!well_formed(r, &what)) { INFO(what << " run " << runs); REQUIRE(false); }
            ++ok;
            points += r.np;
          } else {
            REQUIRE(r.st == Status::BadPath);
            ++bad;
          }
          feed(f, r);
        }
      }
    }
  }
  REQUIRE(runs == kExhRuns);
  REQUIRE(ok == kExhOk);
  REQUIRE(bad == kExhBad);
  REQUIRE(points == kExhPoints);
  REQUIRE(f.h == kExhDigest);
}

TEST_CASE("random corpora match the reference", "[outline]") {
  const std::int32_t tols[5] = {1, 2, 5, 100, 65536};
  const Op ops[10] = {Op::Move, Op::Move, Op::Line, Op::Line, Op::Line, Op::Quad, Op::Cubic, Op::Cubic, Op::Close, Op::Move};
  for (const RandGold& gold : kRand) {
    Lcg g{gold.seed};
    const std::int32_t R = static_cast<std::int32_t>(gold.range);
    Fnv f;
    u64 ok = 0, bad = 0, nc = 0, tp = 0;
    for (int i = 0; i < 300; ++i) {
      const std::int32_t n = g.rng(1, 30);
      Cmds cm;
      bool after_close = false;
      for (std::int32_t j = 0; j < n; ++j) {
        std::int32_t r = g.rng(0, 9);
        if (j == 0 && g.rng(0, 7) != 0) r = 0;
        else if (after_close && g.rng(0, 7) != 0) r = 0;
        Command c{ops[r], {0, 0}, {0, 0}, {0, 0}};
        std::int32_t v[6];
        for (auto& x : v) x = g.rng(-R, R);
        // each of a, b, c is drawn x then y; unused fields are drawn too but cleared, as in the reference
        const int used = c.op == Op::Move || c.op == Op::Line ? 1 : (c.op == Op::Quad ? 2 : (c.op == Op::Cubic ? 3 : 0));
        if (used >= 1) c.a = Point{v[0], v[1]};
        if (used >= 2) c.b = Point{v[2], v[3]};
        if (used >= 3) c.c = Point{v[4], v[5]};
        cm.push_back(c);
        after_close = c.op == Op::Close;
      }
      const std::int32_t tol = tols[g.rng(0, 4)];
      const Result r = run(cm, tol);
      feed(f, r);
      if (r.st == Status::Ok) {
        require_well_formed(r);
        ++ok;
        nc += r.nc;
        tp += r.np;
      } else {
        REQUIRE(r.st == Status::BadPath);
        ++bad;
      }
    }
    REQUIRE(ok == gold.ok);
    REQUIRE(bad == gold.bad);
    REQUIRE(nc == gold.contours);
    REQUIRE(tp == gold.points);
    REQUIRE(f.h == gold.digest);
  }
}

TEST_CASE("unused fields are ignored, never validated", "[outline]") {
  const std::int32_t bad = 0x7FFFFFFF;
  Cmds clean{mv(0, 0), ln(10, 0), Command{Op::Quad, {20, 20}, {30, 0}, {0, 0}}, Command{Op::Cubic, {40, 10}, {50, -10}, {60, 0}}, cl()};
  const Result base = run(clean, 2);
  REQUIRE(base.st == Status::Ok);
  Cmds dirty = clean;
  dirty[0].b = dirty[0].c = Point{bad, -bad};
  dirty[1].b = dirty[1].c = Point{-bad, bad};
  dirty[2].c = Point{bad, bad};
  dirty[4].a = dirty[4].b = dirty[4].c = Point{bad, bad};
  const Result r = run(dirty, 2);
  REQUIRE(r.st == Status::Ok);
  REQUIRE(r.nc == base.nc);
  REQUIRE(r.np == base.np);
  for (u32 i = 0; i < r.np; ++i) REQUIRE(r.pts[i] == base.pts[i]);
  // the used fields are validated: each one, both coordinates, both signs
  for (int cmd = 0; cmd < 3; ++cmd)
    for (int field = 0; field < 3; ++field)
      for (int comp = 0; comp < 2; ++comp)
        for (std::int32_t v : {K + 1, -K - 1, 0x7FFFFFFF, static_cast<std::int32_t>(0x80000000)}) {
          Cmds m = clean;
          Point* p = field == 0 ? &m[static_cast<std::size_t>(cmd) + 1].a : (field == 1 ? &m[static_cast<std::size_t>(cmd) + 1].b : &m[static_cast<std::size_t>(cmd) + 1].c);
          if (cmd == 0 && field > 0) continue;  // a Line uses a only
          if (cmd == 1 && field > 1) continue;  // a Quad uses a and b
          (comp ? p->y : p->x) = v;
          const Result rr = run(m, 2);
          REQUIRE(rr.st == Status::BadPoint);
          REQUIRE(rr.nc == kSentinelWord);  // nothing written, so the check came before any flattening
          REQUIRE(rr.np == kSentinelWord);
          REQUIRE(rr.pts[0].x == kSentinelCoord);
        }
  {
    Cmds m = clean;
    m[0].a = Point{K + 1, 0};
    REQUIRE(run(m, 2).st == Status::BadPoint);
    m[0].a = Point{K, -K};
    m[1].a = Point{-K, K};
    REQUIRE(run(m, 2).st == Status::Ok);  // the limits themselves are valid
  }
}

TEST_CASE("path rules: the state machine", "[outline]") {
  REQUIRE(run({ln(1, 1)}, 1).st == Status::BadPath);                        // first command must be Move
  REQUIRE(run({cl()}, 1).st == Status::BadPath);
  REQUIRE(run({Command{Op::Quad, {1, 1}, {2, 2}, {0, 0}}}, 1).st == Status::BadPath);
  REQUIRE(run({Command{Op::Cubic, {1, 1}, {2, 2}, {3, 3}}}, 1).st == Status::BadPath);
  REQUIRE(run({mv(0, 0), cl(), cl()}, 1).st == Status::BadPath);            // Close after Close
  REQUIRE(run({mv(0, 0), ln(1, 1), cl(), ln(2, 2)}, 1).st == Status::BadPath);
  REQUIRE(run({mv(0, 0), ln(1, 1), cl(), Command{Op::Quad, {1, 1}, {2, 2}, {0, 0}}}, 1).st == Status::BadPath);
  REQUIRE(run({mv(0, 0), ln(1, 1), cl(), Command{Op::Cubic, {1, 1}, {2, 2}, {3, 3}}}, 1).st == Status::BadPath);
  REQUIRE(run({mv(0, 0), ln(1, 1), cl(), cl()}, 1).st == Status::BadPath);
  REQUIRE(run({mv(0, 0), mv(1, 1), mv(2, 2)}, 1).st == Status::Ok);         // repeated Move
  REQUIRE(run({mv(0, 0), cl(), mv(1, 1), cl()}, 1).st == Status::Ok);       // Move then Close
  REQUIRE(run({mv(0, 0), ln(1, 1), cl(), mv(5, 5), ln(6, 6), cl()}, 1).st == Status::Ok);
  // the first violation decides, and BadPath is independent of coordinates and tolerance validity order
  {
    Result r = run({ln(1, 1), mv(0, 0)}, 1);
    REQUIRE(r.st == Status::BadPath);
  }
  // no outputs on BadPath
  const Cmds bad{mv(0, 0), ln(5, 5), cl(), ln(9, 9)};
  const Result r = run(bad, 1);
  REQUIRE(r.st == Status::BadPath);
  REQUIRE(r.nc == kSentinelWord);
  REQUIRE(r.np == kSentinelWord);
  for (const Point& p : r.pts) REQUIRE(p.x == kSentinelCoord);
}

TEST_CASE("curves come from core/flatten", "[outline]") {
  // the outline of a lone curve is exactly the flatten output (after the first point is shared) with consecutive duplicates removed
  const Point p0{0, 0}, c{50, 100}, p1{100, 0};
  Pts fl(lumora::flatten::kMaxOutputPoints);
  u32 n = 0;
  REQUIRE(lumora::flatten::flatten_quad(p0, c, p1, 3, fl.data(), static_cast<u32>(fl.size()), &n) == lumora::flatten::Status::Ok);
  const Result r = run({mv(0, 0), Command{Op::Quad, c, p1, {0, 0}}}, 3);
  REQUIRE(r.st == Status::Ok);
  Pts want;
  for (u32 i = 0; i < n; ++i)
    if (want.empty() || want.back() != fl[i]) want.push_back(fl[i]);
  REQUIRE(r.np == want.size());
  for (u32 i = 0; i < r.np; ++i) REQUIRE(r.pts[i] == want[i]);
  const Point q0{0, 0}, d0{0, 100}, d1{100, -100}, q1{100, 0};
  REQUIRE(lumora::flatten::flatten_cubic(q0, d0, d1, q1, 3, fl.data(), static_cast<u32>(fl.size()), &n) == lumora::flatten::Status::Ok);
  const Result r2 = run({mv(0, 0), Command{Op::Cubic, d0, d1, q1}}, 3);
  want.clear();
  for (u32 i = 0; i < n; ++i)
    if (want.empty() || want.back() != fl[i]) want.push_back(fl[i]);
  REQUIRE(r2.np == want.size());
  for (u32 i = 0; i < r2.np; ++i) REQUIRE(r2.pts[i] == want[i]);
  // a curve starts at the current point of the contour: after a line
  const Result r3 = run({mv(0, 0), ln(10, 10), Command{Op::Quad, {20, 40}, {30, 10}, {0, 0}}}, 2);
  REQUIRE(r3.st == Status::Ok);
  REQUIRE(r3.pts[0] == Point{0, 0});
  REQUIRE(r3.pts[1] == Point{10, 10});
  REQUIRE(r3.pts[r3.np - 1] == Point{30, 10});
}

TEST_CASE("statuses, check order and no write on error", "[outline]") {
  const Cmds ok{mv(0, 0), ln(10, 0), ln(10, 10), cl()};
  const u32 n = 4, cb = contour_bound(n), pb = point_bound(n);
  std::vector<Point> pts(pb, Point{kSentinelCoord, kSentinelCoord});
  std::vector<u32> ends(cb, kSentinelWord);
  std::vector<std::uint8_t> cls(cb, 0xEE);
  u32 nc = kSentinelWord, np = kSentinelWord;
  auto untouched = [&] {
    for (const Point& p : pts) if (p.x != kSentinelCoord) return false;
    for (u32 e : ends) if (e != kSentinelWord) return false;
    for (auto c : cls) if (c != 0xEE) return false;
    return nc == kSentinelWord && np == kSentinelWord;
  };
  auto call = [&](const Command* c, u32 cnt, std::int32_t tol, Point* p, u32 pc, u32* e, u32 ec, std::uint8_t* cl_, u32 cc, u32* a, u32* b) {
    return build_outline(c, cnt, tol, p, pc, e, ec, cl_, cc, a, b);
  };
  auto good = [&](std::int32_t tol) { return call(ok.data(), n, tol, pts.data(), pb, ends.data(), cb, cls.data(), cb, &nc, &np); };
  // NullBuffer: every pointer, and before BadCount and BadTolerance
  REQUIRE(call(nullptr, n, 1, pts.data(), pb, ends.data(), cb, cls.data(), cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(ok.data(), n, 1, nullptr, pb, ends.data(), cb, cls.data(), cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb, nullptr, cb, cls.data(), cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb, ends.data(), cb, nullptr, cb, &nc, &np) == Status::NullBuffer);
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb, ends.data(), cb, cls.data(), cb, nullptr, &np) == Status::NullBuffer);
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb, ends.data(), cb, cls.data(), cb, &nc, nullptr) == Status::NullBuffer);
  REQUIRE(call(nullptr, 0, 0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr) == Status::NullBuffer);
  // BadCount before BadTolerance
  for (u32 bad : {0u, 1025u, 0xFFFFFFFFu})
    REQUIRE(call(ok.data(), bad, 0, pts.data(), 0, ends.data(), 0, cls.data(), 0, &nc, &np) == Status::BadCount);
  // BadTolerance before BadCommand
  Cmds badop = ok;
  badop[2].op = static_cast<Op>(5);
  for (std::int32_t t : {0, -1, 65537, 0x7FFFFFFF}) REQUIRE(call(badop.data(), n, t, pts.data(), 0, ends.data(), 0, cls.data(), 0, &nc, &np) == Status::BadTolerance);
  // BadCommand: any command, before BadPoint and BadPath
  for (std::uint8_t v : {5, 6, 127, 255}) {
    Cmds m = ok;
    m[3].op = static_cast<Op>(v);
    m[1].a = Point{K + 1, 0};  // a bad point earlier in the list
    REQUIRE(call(m.data(), n, 1, pts.data(), 0, ends.data(), 0, cls.data(), 0, &nc, &np) == Status::BadCommand);
  }
  // BadPoint before BadPath
  {
    Cmds m{ln(K + 1, 0), mv(0, 0)};  // BadPath too (first is a Line)
    std::vector<Point> p2(point_bound(2));
    std::vector<u32> e2(2);
    std::vector<std::uint8_t> c2(2);
    u32 a = 0, b = 0;
    REQUIRE(call(m.data(), 2, 1, p2.data(), 0, e2.data(), 0, c2.data(), 0, &a, &b) == Status::BadPoint);
    // BadPath before BufferTooSmall
    Cmds m2{ln(1, 1), mv(0, 0)};
    REQUIRE(call(m2.data(), 2, 1, p2.data(), 0, e2.data(), 0, c2.data(), 0, &a, &b) == Status::BadPath);
  }
  // BufferTooSmall: each capacity one short, data independent
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb - 1, ends.data(), cb, cls.data(), cb, &nc, &np) == Status::BufferTooSmall);
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb, ends.data(), cb - 1, cls.data(), cb, &nc, &np) == Status::BufferTooSmall);
  REQUIRE(call(ok.data(), n, 1, pts.data(), pb, ends.data(), cb, cls.data(), cb - 1, &nc, &np) == Status::BufferTooSmall);
  REQUIRE(call(ok.data(), n, 1, pts.data(), 0, ends.data(), 0, cls.data(), 0, &nc, &np) == Status::BufferTooSmall);
  REQUIRE(untouched());
  REQUIRE(good(1) == Status::Ok);
  REQUIRE(nc == 1);
  REQUIRE(np == 3);
  REQUIRE(cls[0] == 1);
  // the maximum count and extent
  {
    Cmds big;
    big.push_back(mv(0, 0));
    for (u32 i = 1; i < 1024; ++i) big.push_back(ln(static_cast<std::int32_t>(i), static_cast<std::int32_t>(i % 7)));
    const Result r = run(big, 1);
    REQUIRE(r.st == Status::Ok);
    REQUIRE(r.np == 1024);
    REQUIRE(r.pts.size() == 262144);
  }
}

TEST_CASE("Overlap: all fifteen pairs of the six extents", "[outline]") {
  const Cmds cmds{mv(0, 0), ln(10, 0), ln(10, 10), mv(30, 30), ln(40, 40)};
  const u32 n = 5;
  const u32 cb = contour_bound(n), pb = point_bound(n);
  const u64 size[6] = {n * sizeof(Command), pb * sizeof(Point), cb * 4ull, cb * 1ull, 4, 4};
  constexpr std::size_t kSlot = 16384;
  for (int a = 0; a < 6; ++a)
    for (int b = 0; b < 6; ++b) {
      if (a == b) continue;
      const i64 sa = static_cast<i64>(size[a]), sb = static_cast<i64>(size[b]);
      // offset of b from the start of a: first byte, last byte of a, b's last byte on a's first byte, then two touching cases
      const bool al = b != 3;  // every region but the closed flags needs 4 byte alignment
      auto up = [&](i64 v) { return al ? (v + 3) / 4 * 4 : v; };
      auto dn = [&](i64 v) { return al ? v / 4 * 4 : v; };
      const i64 offs[5] = {0, dn(sa - 1), -dn(sb - 1), up(sa), -up(sb)};
      for (int o = 0; o < 5; ++o) {
        std::vector<u64> arena((7 * kSlot + 20000) / 8 + 64, 0);
        auto base = reinterpret_cast<unsigned char*>(arena.data());
        std::size_t pos[6];
        for (int i = 0; i < 6; ++i) pos[i] = 20000 + static_cast<std::size_t>(i) * kSlot;
        const std::size_t want = static_cast<std::size_t>(static_cast<i64>(pos[a]) + offs[o]);
        pos[b] = want;
        std::memset(base, 0xA5, arena.size() * 8);
        Command* pc = reinterpret_cast<Command*>(base + pos[0]);
        Point* pp = reinterpret_cast<Point*>(base + pos[1]);
        u32* pe = reinterpret_cast<u32*>(base + pos[2]);
        std::uint8_t* pcl = base + pos[3];
        u32* c1 = reinterpret_cast<u32*>(base + pos[4]);
        u32* c2 = reinterpret_cast<u32*>(base + pos[5]);
        // regions are placed first, so b may overwrite part of a when they overlap; write the commands last
        std::memcpy(pc, cmds.data(), n * sizeof(Command));
        std::vector<unsigned char> before(base, base + arena.size() * 8);
        const Status st = build_outline(pc, n, 1, pp, pb, pe, cb, pcl, cb, c1, c2);
        // the real extents of a and b decide the expectation
        const i64 ia = static_cast<i64>(pos[a]), ib = static_cast<i64>(pos[b]);
        const bool expect = ia < ib + sb && ib < ia + sa;
        INFO("a=" << a << " b=" << b << " o=" << o);
        if (expect) {
          REQUIRE(st == Status::Overlap);
          REQUIRE(std::memcmp(before.data(), base, arena.size() * 8) == 0);
        } else {
          REQUIRE(st == Status::Ok);
        }
      }
    }
  // BufferTooSmall before Overlap, Overlap with exact capacities
  std::vector<u64> arena(4096, 0);
  Command* pc = reinterpret_cast<Command*>(arena.data());
  std::memcpy(pc, cmds.data(), n * sizeof(Command));
  u32 cnt[2] = {0, 0};
  std::vector<u32> ends(cb);
  std::vector<std::uint8_t> cls(cb);
  Point* same_pts = reinterpret_cast<Point*>(arena.data());
  REQUIRE(build_outline(pc, n, 1, same_pts, pb - 1, ends.data(), cb, cls.data(), cb, &cnt[0], &cnt[1]) == Status::BufferTooSmall);
  REQUIRE(build_outline(pc, n, 1, same_pts, pb, ends.data(), cb, cls.data(), cb, &cnt[0], &cnt[1]) == Status::Overlap);
  // the two counters may not alias
  std::vector<Point> p2(pb);
  REQUIRE(build_outline(cmds.data(), n, 1, p2.data(), pb, ends.data(), cb, cls.data(), cb, &cnt[0], &cnt[0]) == Status::Overlap);
  // a larger capacity does not widen an extent: the counter just past the bound is fine
  std::vector<Point> p3(pb + 8);
  u32* tail = reinterpret_cast<u32*>(p3.data() + pb);
  REQUIRE(build_outline(cmds.data(), n, 1, p3.data(), pb + 8, ends.data(), cb, cls.data(), cb, tail, tail + 1) == Status::Ok);
}

TEST_CASE("performance: 1024 commands of extreme curves (sanitizer safe)", "[outline][perf]") {
  Lcg g{5};
  auto rpt = [&] {
    const std::int32_t x = g.rng(-K, K);
    const std::int32_t y = g.rng(-K, K);
    return Point{x, y};
  };
  Cmds cm;
  cm.push_back(Command{Op::Move, rpt(), {0, 0}, {0, 0}});
  for (int j = 0; j < 1023; ++j) {
    const Op op = std::array<Op, 4>{Op::Cubic, Op::Quad, Op::Cubic, Op::Line}[static_cast<std::size_t>(j % 4)];
    Command c{op, {0, 0}, {0, 0}, {0, 0}};
    const Point a = rpt(), b = rpt(), cc = rpt();
    c.a = a;
    if (op == Op::Quad || op == Op::Cubic) c.b = b;
    if (op == Op::Cubic) c.c = cc;
    cm.push_back(c);
  }
  const Result r = run(cm, 1);
  require_well_formed(r);
  REQUIRE(r.nc == kPerfContours);
  REQUIRE(r.np == kPerfPoints);
  REQUIRE(digest_outline(r.pts.data(), r.ends.data(), r.closed.data(), r.nc) == kPerfDigest);
}
