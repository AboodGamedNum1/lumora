// Tests for core/tilings. Evidence label: unit tests only.
// Expected values come from an in-test definitional backtracking count (the first free cell is covered by a horizontal or a vertical domino; exact
// counts; not the method of the implementation), from an in-test column-by-column profile DP whose transitions are generated row by row (a different
// construction from the run-parity rule and from the fast power of the implementation), from closed forms (h = 1, h = 2 as Fibonacci by fast doubling,
// odd area), from known values (2 x 2 = 2, 4 x 4 = 36, 6 x 6 = 6728, 4 x 5 = 95, 3 x 2k = 3, 11, 41, 153), from digests made by tools/tilings_ref.py
// (standard library only: exact brute force and column DP on the small extents; at huge widths there is no brute force, so the oracle is the
// linear recurrence found by Berlekamp-Massey from the exact column DP and evaluated by Kitamasa, which the reference checks against the column DP up to
// w = 3000 and against an independent matrix power at the huge widths), and from the recurrence coefficients embedded below, applied through the
// implementation itself at neighbouring widths. Extents, named in the test names and in docs/tilings.md:
//   exhaustive vs the in-test backtracking brute force: every h = 1..6 and every w = 0..8; symmetry for every h, w = 1..6;
//   reference digests: every h with w = 0..300 (the exact column DP), and every h with 135 huge widths (2^j - 1, 2^j, 2^j + 1 for j = 0..31, UINT32_MAX - 1,
//               UINT32_MAX and forty random widths);
//   huge widths: the recurrence identity at sampled widths, h = 2 against fast-doubling Fibonacci at all of them, odd h with odd w gives 0, h = 1 parity.
// Every call passes the result cell between guard words. No test asserts anything about time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tilings.h"

using lumora::tilings::domino_tilings;
using lumora::tilings::kMod;
using lumora::tilings::TileResult;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr std::uint64_t kSmallDigest[6] = {  // per h = 1..6: w = 0..300 from the exact column DP
    0xf32ef33b52b13794ull, 0x6bfc02802040bb48ull, 0x36ecd503f34514baull, 0x98f0979cd1e8a8afull, 0x0674a2ac1e4a9f22ull, 0xedd7a90b7fa1ad18ull,
};
constexpr std::uint64_t kHugeDigest[6] = {  // per h = 1..6: huge widths (2^j - 1, 2^j, 2^j + 1 for j = 0..31, UINT32_MAX - 1, UINT32_MAX, forty random) from the recurrence route
    0xc4d4cedd73c3ee71ull, 0x91d3fcebd6a17898ull, 0x29417056d7cb96ecull, 0xc3ea20ebfdd01715ull, 0x0445569ea2a1ce55ull, 0x4cacca31aa469bf1ull,
};
// linear recurrence of each h found by Berlekamp-Massey from a(0..599): a(n) = sum over i = 1..order of c[i - 1] * a(n - i) mod p
constexpr std::uint32_t kRecOrder1 = 2u;
constexpr std::uint32_t kRec1[2] = {0u, 1u};
constexpr std::uint32_t kRecOrder2 = 2u;
constexpr std::uint32_t kRec2[2] = {1u, 1u};
constexpr std::uint32_t kRecOrder3 = 4u;
constexpr std::uint32_t kRec3[4] = {0u, 4u, 0u, 998244352u};
constexpr std::uint32_t kRecOrder4 = 4u;
constexpr std::uint32_t kRec4[4] = {1u, 5u, 1u, 998244352u};
constexpr std::uint32_t kRecOrder5 = 8u;
constexpr std::uint32_t kRec5[8] = {0u, 15u, 0u, 998244321u, 0u, 15u, 0u, 998244352u};
constexpr std::uint32_t kRecOrder6 = 8u;
constexpr std::uint32_t kRec6[8] = {1u, 20u, 10u, 998244315u, 998244343u, 20u, 998244352u, 998244352u};
constexpr std::uint32_t kHugeCount = 135u;
constexpr std::uint32_t kHugeW[6][135] = {
    {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1048575u, 1048576u, 1048577u, 2097151u, 2097152u, 2097153u, 4194303u, 4194304u, 4194305u, 8388607u, 8388608u, 8388609u, 16777215u, 16777216u, 16777217u, 33554431u, 33554432u, 33554433u, 67108863u, 67108864u, 67108865u, 90116359u, 134217727u, 134217728u, 134217729u, 268435455u, 268435456u, 268435457u, 298376560u, 373937620u, 376396980u, 536870911u, 536870912u, 536870913u, 557870245u, 680690447u, 695525879u, 696465050u, 769876761u, 866383490u, 906383739u, 987462341u, 1073741823u, 1073741824u, 1073741825u, 1187278887u, 1232801509u, 1294352630u, 1398451819u, 1407953905u, 1427415270u, 1428215415u, 1467303359u, 1515500415u, 1578643213u, 1742020852u, 1849508767u, 1904665016u, 1965438901u, 2138687531u, 2147483647u, 2147483648u, 2147483649u, 2417042905u, 2524069592u, 2682057207u, 3108497947u, 3155985019u, 3157742063u, 3227996626u, 3396676974u, 3616572873u, 3774178620u, 3955139724u, 3983047699u, 4226158794u, 4294967294u, 4294967295u},
    {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1048575u, 1048576u, 1048577u, 2097151u, 2097152u, 2097153u, 4194303u, 4194304u, 4194305u, 8388607u, 8388608u, 8388609u, 16777215u, 16777216u, 16777217u, 33554431u, 33554432u, 33554433u, 67108863u, 67108864u, 67108865u, 134217727u, 134217728u, 134217729u, 166761233u, 202061351u, 241728667u, 268435455u, 268435456u, 268435457u, 319572363u, 433546426u, 443435253u, 525047415u, 536870911u, 536870912u, 536870913u, 572283496u, 693249309u, 800848092u, 815663396u, 901048354u, 995822363u, 1004467909u, 1039574651u, 1073741823u, 1073741824u, 1073741825u, 1436992704u, 1700197282u, 1790530520u, 1808890871u, 1992092322u, 2059261823u, 2061769362u, 2086352941u, 2147483647u, 2147483648u, 2147483649u, 2331446575u, 2562111817u, 2667956594u, 2769689549u, 2963081692u, 3053019514u, 3090545718u, 3328114024u, 3439703543u, 3499461944u, 3507788826u, 3545581946u, 3640944151u, 3690210140u, 3701988960u, 4011874894u, 4216104179u, 4294967294u, 4294967295u},
    {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1048575u, 1048576u, 1048577u, 2097151u, 2097152u, 2097153u, 4194303u, 4194304u, 4194305u, 8388607u, 8388608u, 8388609u, 16777215u, 16777216u, 16777217u, 33554431u, 33554432u, 33554433u, 67108863u, 67108864u, 67108865u, 134217727u, 134217728u, 134217729u, 268435455u, 268435456u, 268435457u, 458034337u, 499491710u, 536870911u, 536870912u, 536870913u, 585520631u, 620805558u, 768656303u, 965427647u, 994801423u, 1052874661u, 1073741823u, 1073741824u, 1073741825u, 1145105885u, 1298722528u, 1443573563u, 1523314149u, 1535560663u, 1584367421u, 1646160360u, 1821601162u, 1854040529u, 1966290803u, 2068038177u, 2147483647u, 2147483648u, 2147483649u, 2162678012u, 2252935501u, 2374580152u, 2546252614u, 2568142887u, 2686131352u, 2858196728u, 2944088386u, 2962342805u, 3046484045u, 3210126674u, 3296369093u, 3434087580u, 3495562343u, 3506033536u, 3518586314u, 3558219249u, 3715526703u, 4029100125u, 4110076285u, 4258079664u, 4294967294u, 4294967295u},
    {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1048575u, 1048576u, 1048577u, 2097151u, 2097152u, 2097153u, 4194303u, 4194304u, 4194305u, 8388607u, 8388608u, 8388609u, 16777215u, 16777216u, 16777217u, 33554431u, 33554432u, 33554433u, 67108863u, 67108864u, 67108865u, 127740195u, 134217727u, 134217728u, 134217729u, 141603890u, 162767449u, 233492129u, 268435455u, 268435456u, 268435457u, 287141368u, 292608551u, 413023153u, 464851290u, 520493569u, 536870911u, 536870912u, 536870913u, 569981881u, 710355580u, 719899498u, 752261928u, 915591529u, 1006086711u, 1014037534u, 1023809598u, 1073741823u, 1073741824u, 1073741825u, 1214970832u, 1382978428u, 1599495595u, 1619415635u, 1625611830u, 1662422742u, 1899786834u, 1992317450u, 2147483647u, 2147483648u, 2147483649u, 2367324284u, 2422520913u, 2509469435u, 3068255118u, 3378236294u, 3387382567u, 3447249510u, 3577730635u, 3583969166u, 3630783889u, 3783203290u, 3878895977u, 4033489059u, 4122571613u, 4212438645u, 4294967294u, 4294967295u},
    {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1048575u, 1048576u, 1048577u, 2097151u, 2097152u, 2097153u, 4194303u, 4194304u, 4194305u, 8388607u, 8388608u, 8388609u, 16777215u, 16777216u, 16777217u, 33554431u, 33554432u, 33554433u, 55547400u, 67108863u, 67108864u, 67108865u, 134217727u, 134217728u, 134217729u, 206313273u, 212383760u, 218795820u, 235609108u, 268435455u, 268435456u, 268435457u, 324702142u, 338874674u, 344840689u, 379123159u, 536870911u, 536870912u, 536870913u, 844126800u, 867970539u, 925353181u, 936006140u, 996675258u, 1073741823u, 1073741824u, 1073741825u, 1181724606u, 1336949343u, 1541638450u, 1625526707u, 1917510741u, 2147483647u, 2147483648u, 2147483649u, 2290364455u, 2455587365u, 2498272770u, 2505804270u, 2688376673u, 2739684177u, 2896195300u, 2945700231u, 2975781687u, 3160289175u, 3183507428u, 3251651232u, 3405307295u, 3411930464u, 3427569163u, 3429674594u, 3525675812u, 3642862564u, 3683471216u, 3843580247u, 4115957117u, 4294967294u, 4294967295u},
    {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1048575u, 1048576u, 1048577u, 2097151u, 2097152u, 2097153u, 4194303u, 4194304u, 4194305u, 8388607u, 8388608u, 8388609u, 12342100u, 16777215u, 16777216u, 16777217u, 33554431u, 33554432u, 33554433u, 67108863u, 67108864u, 67108865u, 67685519u, 94653636u, 134217727u, 134217728u, 134217729u, 268435455u, 268435456u, 268435457u, 454422025u, 536870911u, 536870912u, 536870913u, 589007141u, 691733552u, 731821405u, 1025859979u, 1073741823u, 1073741824u, 1073741825u, 1074667222u, 1160128904u, 1232615809u, 1248771071u, 1589879730u, 1626064709u, 1886257225u, 1973080852u, 1987401591u, 2147483647u, 2147483648u, 2147483649u, 2165342233u, 2242939314u, 2300999191u, 2327013269u, 2364444195u, 2505997880u, 2520063942u, 2552725234u, 2678414778u, 2961558341u, 3054143605u, 3055648631u, 3094951913u, 3229845337u, 3468392776u, 3470307731u, 3508363067u, 3567323099u, 3612623993u, 3656238113u, 3926226795u, 4001374245u, 4161196721u, 4294967294u, 4294967295u},
};

constexpr u32 kSent = 0x5A5A5A5Au;
constexpr u64 kInit = 0xcbf29ce484222325ull;
u64 fnv(u64 h, u32 w) { return (h ^ static_cast<u64>(w)) * 0x100000001b3ull; }

// Calls through a cell between guard words; false when the result is not Ok or a guard changed.
bool tile(u32 h, u32 w, u32& res) {
  u32 cell[5] = {kSent, kSent, kSent, kSent, kSent};
  const TileResult r = domino_tilings(h, w, cell + 2);
  res = cell[2];
  return r == TileResult::Ok && cell[0] == kSent && cell[1] == kSent && cell[3] == kSent && cell[4] == kSent;
}

// ---- definitional brute force: cover the first free cell with a horizontal or a vertical domino (exact counts) ----
u64 brute_count(u32 h, u32 w, u64 mask) {  // cell (r, c) is bit c * h + r
  const u32 cells = h * w;
  if (cells == 0u) return 1u;
  const u64 full = (cells == 64u) ? ~0ull : ((1ull << cells) - 1u);
  if (mask == full) return 1u;
  u32 i = 0;
  while (((mask >> i) & 1u) != 0u) ++i;
  const u32 r = i % h, c = i / h;
  u64 total = 0;
  if (r + 1u < h && ((mask >> (i + 1u)) & 1u) == 0u) total += brute_count(h, w, mask | (3ull << i));
  if (c + 1u < w && ((mask >> (i + h)) & 1u) == 0u) total += brute_count(h, w, mask | (1ull << i) | (1ull << (i + h)));
  return total;
}

// ---- column DP: transitions generated row by row ----
struct Trans {
  std::vector<std::pair<u32, u32>> edges;  // (incoming profile, outgoing profile)
};
void gen_rows(u32 h, u32 r, u32 s, u32 t, u32 s0, Trans& tr) {
  if (r >= h) {
    tr.edges.push_back({s0, t});
    return;
  }
  if (((s >> r) & 1u) != 0u) {  // covered from the previous column
    gen_rows(h, r + 1u, s, t, s0, tr);
    return;
  }
  gen_rows(h, r + 1u, s, t | (1u << r), s0, tr);  // a horizontal domino starts here
  if (r + 1u < h && ((s >> (r + 1u)) & 1u) == 0u) gen_rows(h, r + 2u, s, t, s0, tr);  // a vertical domino
}
Trans make_trans(u32 h) {
  Trans tr;
  for (u32 s = 0; s < (1u << h); ++s) gen_rows(h, 0u, s, 0u, s, tr);
  return tr;
}
// a(0..nmax) mod p
std::vector<u32> column_dp(u32 h, u32 nmax) {
  const Trans tr = make_trans(h);
  std::vector<u64> v(1u << h, 0u), nv(1u << h, 0u);
  v[0] = 1u;
  std::vector<u32> a;
  a.push_back(1u);
  for (u32 w = 1; w <= nmax; ++w) {
    for (u64& x : nv) x = 0u;
    for (const auto& e : tr.edges) nv[e.second] = (nv[e.second] + v[e.first]) % kMod;
    v = nv;
    a.push_back(static_cast<u32>(v[0]));
  }
  return a;
}

// fast doubling Fibonacci mod p: F(n); a(2, w) = F(w + 1)
void fib_pair(u64 n, u64& f, u64& g) {  // f = F(n), g = F(n + 1)
  f = 0;
  g = 1;
  for (int bit = 63; bit >= 0; --bit) {
    const u64 c = (f * ((2u * g + kMod - f) % kMod)) % kMod;
    const u64 d = (f * f + g * g) % kMod;
    if (((n >> bit) & 1u) != 0u) {
      f = d;
      g = (c + d) % kMod;
    } else {
      f = c;
      g = d;
    }
  }
}

const u32* rec_of(u32 h) {
  switch (h) {
    case 1: return kRec1;
    case 2: return kRec2;
    case 3: return kRec3;
    case 4: return kRec4;
    case 5: return kRec5;
    default: return kRec6;
  }
}
u32 order_of(u32 h) {
  switch (h) {
    case 1: return kRecOrder1;
    case 2: return kRecOrder2;
    case 3: return kRecOrder3;
    case 4: return kRecOrder4;
    case 5: return kRecOrder5;
    default: return kRecOrder6;
  }
}

TEST_CASE("hand-checked values, symmetry and known tables", "[tilings]") {
  u32 r = 0;
  CHECK((tile(2, 2, r) && r == 2u));
  CHECK((tile(4, 4, r) && r == 36u));
  CHECK((tile(6, 6, r) && r == 6728u));
  CHECK((tile(4, 5, r) && r == 95u));
  CHECK((tile(5, 4, r) && r == 95u));
  const u32 three[4] = {3u, 11u, 41u, 153u};
  for (u32 k = 1; k <= 4u; ++k) CHECK((tile(3, 2u * k, r) && r == three[k - 1u]));
  const u32 fibs[8] = {1u, 2u, 3u, 5u, 8u, 13u, 21u, 34u};  // h = 2, w = 1..8
  for (u32 w = 1; w <= 8u; ++w) CHECK((tile(2, w, r) && r == fibs[w - 1u]));
  for (u32 h = 1; h <= 6u; ++h) CHECK((tile(h, 0, r) && r == 1u));  // the empty tiling
  for (u32 w = 0; w <= 12u; ++w) CHECK((tile(1, w, r) && r == ((w % 2u == 0u) ? 1u : 0u)));
  // symmetry for every h, w in 1..6
  for (u32 h = 1; h <= 6u; ++h) {
    for (u32 w = 1; w <= 6u; ++w) {
      u32 a = 0, b = 0;
      CHECK((tile(h, w, a) && tile(w, h, b) && a == b));
    }
  }
  // odd area is zero, for every height and small widths
  for (u32 h = 1; h <= 6u; ++h) {
    for (u32 w = 1; w <= 40u; ++w) {
      if ((h * w) % 2u == 1u) CHECK((tile(h, w, r) && r == 0u));
    }
  }
}

TEST_CASE("failure cases, check order, nothing written (sentinel in *out)", "[tilings]") {
  const u32 hs[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 0xFFFFFFFFu};
  const u32 ws[] = {0u, 1u, 2u, 3u, 10u, 0xFFFFFFFFu};
  for (u32 h : hs) {
    for (u32 w : ws) CHECK(domino_tilings(h, w, nullptr) == TileResult::NullArgument);  // null first, for every height
  }
  for (u32 h : {0u, 7u, 8u, 100u, 0xFFFFFFFFu}) {
    for (u32 w : ws) {
      u32 cell = kSent;
      CHECK(domino_tilings(h, w, &cell) == TileResult::BadHeight);
      CHECK(cell == kSent);
    }
  }
  for (u32 h = 1; h <= 6u; ++h) {  // every u32 width is valid
    for (u32 w : ws) {
      u32 r = 0;
      CHECK(tile(h, w, r));
      CHECK(r < kMod);
    }
  }
}

TEST_CASE("exhaustive h = 1..6 and w = 0..8 against the backtracking brute force", "[tilings][exhaustive]") {
  for (u32 h = 1; h <= 6u; ++h) {
    for (u32 w = 0; w <= 8u; ++w) {
      u32 r = 0;
      const u64 want = brute_count(h, w, 0u);
      CHECK((tile(h, w, r) && r == want % kMod));
    }
  }
  u32 r = 0;
  CHECK((tile(6, 8, r) && r == 167089u));
}

TEST_CASE("exhaustive every h = 1..6 with every w = 0..4096 against the column DP", "[tilings][exhaustive][slow]") {
  const u32 limits[6] = {4096u, 4096u, 4096u, 4096u, 4096u, 4096u};
  u64 bad = 0;
  for (u32 h = 1; h <= 6u; ++h) {
    const std::vector<u32> a = column_dp(h, limits[h - 1u]);
    for (u32 w = 0; w <= limits[h - 1u]; ++w) {
      u32 r = 0;
      if (!tile(h, w, r) || r != a[w]) ++bad;
    }
  }
  CHECK(bad == 0u);
}

TEST_CASE("reference digests of the exact column DP, w = 0..300 for every h", "[tilings][exhaustive]") {
  for (u32 h = 1; h <= 6u; ++h) {
    u64 d = kInit;
    bool ok = true;
    for (u32 w = 0; w <= 300u; ++w) {
      u32 r = 0;
      ok = ok && tile(h, w, r);
      d = fnv(d, r);
    }
    CHECK(ok);
    CHECK(d == kSmallDigest[h - 1u]);
  }
}

TEST_CASE("huge widths for h = 1..4: reference digests, recurrence identity, Fibonacci, parity", "[tilings][sampled]") {
  for (u32 h = 1; h <= 4u; ++h) {
    u64 d = kInit;
    bool ok = true;
    u32 bad = 0;
    for (u32 i = 0; i < kHugeCount; ++i) {
      const u32 w = kHugeW[h - 1u][i];
      u32 r = 0;
      ok = ok && tile(h, w, r);
      d = fnv(d, r);
      if (h == 2u) {
        u64 f = 0, g = 0;
        fib_pair(static_cast<u64>(w) + 1u, f, g);
        if (r != f) ++bad;
      }
      if (h == 1u && r != ((w % 2u == 0u) ? 1u : 0u)) ++bad;
      if ((h * w) % 2u == 1u && r != 0u) ++bad;  // odd area
      // the recurrence found by the reference, through the implementation at the neighbouring widths
      const u32 L = order_of(h);
      if (w >= L) {
        u64 acc = 0;
        for (u32 j = 1; j <= L; ++j) {
          u32 x = 0;
          ok = ok && tile(h, w - j, x);
          acc = (acc + static_cast<u64>(rec_of(h)[j - 1u]) * x) % kMod;
        }
        if (acc != r) ++bad;
      }
    }
    CHECK(ok);
    CHECK(bad == 0u);
    CHECK(d == kHugeDigest[h - 1u]);
  }
}

TEST_CASE("huge widths for h = 5: reference digest, recurrence identity, odd area", "[tilings][sampled]") {
  const u32 h = 5u;
  u64 d = kInit;
  bool ok = true;
  u32 bad = 0;
  for (u32 i = 0; i < kHugeCount; ++i) {
    const u32 w = kHugeW[h - 1u][i];
    u32 r = 0;
    ok = ok && tile(h, w, r);
    d = fnv(d, r);
    if (w % 2u == 1u && r != 0u) ++bad;
    if (w >= order_of(h) && true) {
      u64 acc = 0;
      for (u32 j = 1; j <= order_of(h); ++j) {
        u32 x = 0;
        ok = ok && tile(h, w - j, x);
        acc = (acc + static_cast<u64>(rec_of(h)[j - 1u]) * x) % kMod;
      }
      if (acc != r) ++bad;
    }
  }
  CHECK(ok);
  CHECK(bad == 0u);
  CHECK(d == kHugeDigest[h - 1u]);
}

TEST_CASE("huge widths for h = 6: reference digest, recurrence identity", "[tilings][sampled][slow]") {
  const u32 h = 6u;
  u64 d = kInit;
  bool ok = true;
  u32 bad = 0;
  for (u32 i = 0; i < kHugeCount; ++i) {
    const u32 w = kHugeW[h - 1u][i];
    u32 r = 0;
    ok = ok && tile(h, w, r);
    d = fnv(d, r);
    if (w >= order_of(h)) {
      u64 acc = 0;
      for (u32 j = 1; j <= order_of(h); ++j) {
        u32 x = 0;
        ok = ok && tile(h, w - j, x);
        acc = (acc + static_cast<u64>(rec_of(h)[j - 1u]) * x) % kMod;
      }
      if (acc != r) ++bad;
    }
  }
  CHECK(ok);
  CHECK(bad == 0u);
  CHECK(d == kHugeDigest[h - 1u]);
}

}  // namespace
