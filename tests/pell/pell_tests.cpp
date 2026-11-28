// Tests for core/pell. Evidence label: unit tests only.
// Expected values come from known solutions, from digests and tables made by tools/pell_ref.py (standard library only: exact Python integers, the fundamental
// solution being the first convergent with p^2 - d q^2 = 1, the period the first return of the state (m, dd), checked against a modular route by quotient
// matrices on every non-square d up to 2^20), and from an in-test oracle that does not use the implementation's rules: the period is the first return of
// the state (m, dd), the solution is the product of the 2 x 2 quotient matrices [[a, 1], [1, 0]] modulo p (kept in a vector, the second period run
// explicitly for an odd period), and the norm identity x^2 - d y^2 = 1 (mod p) is checked on every result. Extents, named in the test names and in docs/pell.md:
//   every non-square d from 2 to 2^20 - 1 (exhaustive, 8 cases of 2^17 values of d): the implementation against the in-test oracle, the norm identity, the reference digest;
//   every non-square d from 2 to 30000 against the digest of the exact Python big-integer solutions; 167 targeted d (longest periods, d near squares, random up to 2^20)
//   against exact big-integer residues; hand-checked values; the failure matrix (all 8 null combinations, every perfect square up to 2^20, 0, 1, 2^20 + 1, UINT32_MAX).
// Every call passes cells between guard words, pre-filled with a sentinel. No test asserts anything about time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pell.h"

using lumora::pell::kMaxD;
using lumora::pell::kMod;
using lumora::pell::PellResult;
using lumora::pell::pell_fundamental;

namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr std::uint32_t kExactMax = 30000u;
constexpr std::uint64_t kExactDigest = 0xfaf3b71d80505423ull;
constexpr std::uint32_t kChunkSize = 131072u;
constexpr std::uint64_t kChunkDigest[8] = {0xa00204af18d0dfa6ull, 0x44e759e687464595ull, 0x88d75a422eb51dfbull, 0xe2d344ce2f7d2918ull, 0xeb39a1fdc436ce50ull, 0xef7e2e4f75aca748ull, 0x21522219637d2112ull, 0x85ec401049b4db1bull};
constexpr std::uint32_t kChunkCount[8] = {130709u, 130923u, 130956u, 130975u, 130987u, 130995u, 131001u, 131006u};
constexpr std::uint32_t kMaxPeriod = 2672u;
constexpr std::uint32_t kMaxM = 1023u;
constexpr std::uint32_t kMaxDd = 2046u;
constexpr std::uint32_t kMaxA = 2046u;
constexpr std::uint32_t kZeroResidues = 0u;
struct Target { std::uint32_t d, x, y, period; };
constexpr Target kTargets[167] = {
    {2u, 3u, 2u, 1u},
    {3u, 2u, 1u, 2u},
    {5u, 9u, 4u, 1u},
    {6u, 5u, 2u, 2u},
    {7u, 8u, 3u, 4u},
    {8u, 3u, 1u, 2u},
    {10u, 19u, 6u, 1u},
    {11u, 10u, 3u, 2u},
    {47u, 48u, 7u, 4u},
    {48u, 7u, 1u, 2u},
    {50u, 99u, 14u, 1u},
    {51u, 50u, 7u, 2u},
    {61u, 768074696u, 226153980u, 11u},
    {109u, 675177405u, 52353149u, 15u},
    {177u, 62423u, 4692u, 8u},
    {181u, 968857279u, 863105108u, 21u},
    {277u, 36354522u, 802155320u, 21u},
    {661u, 229740983u, 254969640u, 39u},
    {959u, 960u, 31u, 4u},
    {960u, 31u, 1u, 2u},
    {962u, 1923u, 62u, 1u},
    {963u, 962u, 31u, 2u},
    {1000u, 39480499u, 1248483u, 18u},
    {1019u, 632768430u, 111765116u, 26u},
    {9998u, 9999u, 100u, 4u},
    {9999u, 100u, 1u, 2u},
    {10001u, 20001u, 200u, 1u},
    {10002u, 10001u, 100u, 2u},
    {53108u, 409752319u, 199079080u, 26u},
    {56988u, 138203906u, 853525812u, 60u},
    {65023u, 65024u, 255u, 4u},
    {65024u, 255u, 1u, 2u},
    {65026u, 130051u, 510u, 1u},
    {65027u, 65026u, 255u, 2u},
    {65534u, 65535u, 256u, 4u},
    {65535u, 256u, 1u, 2u},
    {65537u, 131073u, 512u, 1u},
    {65538u, 65537u, 256u, 2u},
    {78484u, 123089725u, 45223397u, 68u},
    {85236u, 987367039u, 502887069u, 140u},
    {85379u, 515262847u, 222649275u, 84u},
    {89999u, 300u, 1u, 2u},
    {90001u, 180001u, 600u, 1u},
    {90869u, 275002035u, 756784968u, 198u},
    {97146u, 679587184u, 73709283u, 26u},
    {98051u, 897759843u, 179448977u, 130u},
    {100370u, 670521049u, 548147201u, 49u},
    {105729u, 590119031u, 685800689u, 176u},
    {114408u, 307859466u, 886542864u, 32u},
    {124759u, 878661525u, 607065787u, 456u},
    {132621u, 775413787u, 590298120u, 136u},
    {133510u, 688173467u, 676223588u, 52u},
    {138054u, 588545u, 1584u, 10u},
    {141279u, 164115181u, 226744617u, 162u},
    {142563u, 819244977u, 786917736u, 56u},
    {142967u, 282448u, 747u, 4u},
    {154929u, 750309932u, 660520946u, 104u},
    {201925u, 545478109u, 426925587u, 17u},
    {220401u, 366612750u, 511298272u, 150u},
    {231742u, 727997845u, 236840555u, 104u},
    {241396u, 432911740u, 324921155u, 246u},
    {246895u, 397856896u, 763651436u, 146u},
    {249999u, 500u, 1u, 2u},
    {250001u, 500001u, 1000u, 1u},
    {251847u, 504713417u, 227048505u, 52u},
    {261119u, 261120u, 511u, 4u},
    {261120u, 511u, 1u, 2u},
    {261122u, 522243u, 1022u, 1u},
    {261123u, 261122u, 511u, 2u},
    {262142u, 262143u, 512u, 4u},
    {262143u, 512u, 1u, 2u},
    {262145u, 524289u, 1024u, 1u},
    {262146u, 262145u, 512u, 2u},
    {278505u, 649200606u, 217828454u, 126u},
    {284797u, 66047156u, 465311678u, 624u},
    {285955u, 574955064u, 139659464u, 256u},
    {294961u, 149473528u, 811989831u, 490u},
    {320937u, 641224390u, 302775901u, 464u},
    {330648u, 28751u, 50u, 2u},
    {347958u, 475330533u, 55391664u, 138u},
    {352378u, 197573776u, 465356586u, 187u},
    {379940u, 465086282u, 167951184u, 42u},
    {399870u, 61570024u, 934769020u, 190u},
    {419978u, 184210615u, 487039534u, 5u},
    {430996u, 742556605u, 728591755u, 58u},
    {453252u, 526270461u, 993092513u, 124u},
    {462164u, 314722935u, 462946u, 14u},
    {476941u, 266713532u, 855703544u, 558u},
    {478393u, 36981784u, 807842247u, 209u},
    {490112u, 153160001u, 218775u, 8u},
    {494810u, 838884539u, 648406937u, 57u},
    {518441u, 531879090u, 343265811u, 320u},
    {532757u, 947754117u, 196363715u, 511u},
    {568457u, 316372164u, 485671131u, 398u},
    {578083u, 513305973u, 714905682u, 462u},
    {601232u, 18828406u, 694025912u, 130u},
    {607410u, 789212418u, 2293478u, 20u},
    {621945u, 832704730u, 729181253u, 80u},
    {631337u, 809106150u, 534069631u, 420u},
    {641926u, 906216619u, 755171991u, 510u},
    {680931u, 245536170u, 243245917u, 250u},
    {684886u, 547349359u, 683744357u, 250u},
    {685732u, 604285510u, 246570705u, 58u},
    {686963u, 496387364u, 180128665u, 22u},
    {690953u, 497361337u, 768232816u, 461u},
    {692061u, 277392719u, 967693981u, 290u},
    {695160u, 105366543u, 908533130u, 136u},
    {704523u, 783787056u, 754184857u, 26u},
    {728724u, 466154833u, 109169670u, 46u},
    {733277u, 696350090u, 41036452u, 109u},
    {733606u, 226729807u, 736846790u, 314u},
    {749989u, 298321194u, 993632729u, 180u},
    {760638u, 924781594u, 731773608u, 60u},
    {761551u, 163218884u, 962819968u, 620u},
    {763239u, 676779079u, 943339681u, 618u},
    {765560u, 38082711u, 254913354u, 166u},
    {768562u, 563671044u, 4526840u, 642u},
    {770459u, 650895800u, 176190613u, 862u},
    {774424u, 616438096u, 666542156u, 226u},
    {788122u, 754829333u, 198058827u, 494u},
    {788458u, 866041131u, 997857992u, 430u},
    {800483u, 574926462u, 811057943u, 130u},
    {801520u, 295351068u, 399095764u, 156u},
    {802016u, 479269135u, 555792795u, 64u},
    {816139u, 777213048u, 170462364u, 778u},
    {831733u, 204382170u, 356162454u, 866u},
    {838886u, 418097228u, 682710562u, 606u},
    {842111u, 703279360u, 491618947u, 136u},
    {844102u, 3858751u, 4200u, 6u},
    {851173u, 254531418u, 280108975u, 112u},
    {858565u, 433003177u, 509216872u, 635u},
    {859271u, 955177772u, 61846528u, 72u},
    {881047u, 866421888u, 325969734u, 96u},
    {882625u, 737926043u, 415622575u, 702u},
    {888860u, 420736050u, 982090540u, 72u},
    {894934u, 99855567u, 447405659u, 1274u},
    {930487u, 769774554u, 204492935u, 98u},
    {934479u, 590307325u, 29342636u, 124u},
    {936498u, 723216334u, 723909357u, 190u},
    {937820u, 810861422u, 150241552u, 204u},
    {943476u, 361441366u, 869547492u, 168u},
    {952200u, 339867077u, 411350168u, 92u},
    {956223u, 70864929u, 448145369u, 82u},
    {965984u, 289971692u, 201827165u, 328u},
    {984989u, 911830810u, 867768942u, 374u},
    {997999u, 998000u, 999u, 4u},
    {998000u, 999u, 1u, 2u},
    {998002u, 1996003u, 1998u, 1u},
    {998003u, 998002u, 999u, 2u},
    {999998u, 999999u, 1000u, 4u},
    {999999u, 1000u, 1u, 2u},
    {1000001u, 2000001u, 2000u, 1u},
    {1000002u, 1000001u, 1000u, 2u},
    {1003874u, 580717050u, 333170520u, 178u},
    {1006759u, 784666042u, 218370781u, 374u},
    {1018879u, 471186199u, 718495884u, 2672u},
    {1044482u, 1044483u, 1022u, 4u},
    {1044483u, 1022u, 1u, 2u},
    {1044485u, 2088969u, 2044u, 1u},
    {1044486u, 1044485u, 1022u, 2u},
    {1046527u, 1046528u, 1023u, 4u},
    {1046528u, 1023u, 1u, 2u},
    {1046530u, 2093059u, 2046u, 1u},
    {1046531u, 1046530u, 1023u, 2u},
    {1048573u, 893676295u, 494002569u, 1645u},
    {1048574u, 1048575u, 1024u, 4u},
    {1048575u, 1024u, 1u, 2u}
};

constexpr u32 kSent = 0x5A5A5A5Au;
constexpr u64 kInit = 0xcbf29ce484222325ull;
u64 fnv(u64 h, u32 w) { return (h ^ static_cast<u64>(w)) * 0x100000001b3ull; }

u32 isqrt(u32 d) {
  u32 r = 0;
  while ((r + 1u) * (r + 1u) <= d) ++r;
  return r;
}
bool is_square(u32 d) {
  const u32 r = isqrt(d);
  return r * r == d;
}

struct Res {
  PellResult r;
  u32 x, y, per;
  bool guards;
};
// A call through three cells, each between guard words.
Res call(u32 d) {
  u32 xc[5] = {kSent, kSent, kSent, kSent, kSent};
  u32 yc[5] = {kSent, kSent, kSent, kSent, kSent};
  u32 pc[5] = {kSent, kSent, kSent, kSent, kSent};
  Res o;
  o.r = pell_fundamental(d, xc + 2, yc + 2, pc + 2);
  o.x = xc[2];
  o.y = yc[2];
  o.per = pc[2];
  o.guards = true;
  for (int i : {0, 1, 3, 4}) o.guards = o.guards && xc[i] == kSent && yc[i] == kSent && pc[i] == kSent;
  return o;
}

struct Mat {
  u64 a, b, c, e;  // [[a, b], [c, e]]
};
Mat mul(const Mat& l, const Mat& r) {
  return Mat{(l.a * r.a + l.b * r.c) % kMod, (l.a * r.b + l.b * r.e) % kMod, (l.c * r.a + l.e * r.c) % kMod, (l.c * r.b + l.e * r.e) % kMod};
}
Mat quot(u64 a) { return Mat{a % kMod, 1u, 1u, 0u}; }

// In-test oracle: the period by the first return of the state (m, dd); the solution by the matrix product A(a0) A(a1) ...; odd period: through the second period.
struct Oracle {
  u32 x, y, per;
};
Oracle oracle(u32 d) {
  const u32 a0 = isqrt(d);
  u32 m = a0, dd = d - a0 * a0;
  const u32 m1 = m, d1 = dd;
  std::vector<u32> qs;
  for (;;) {
    const u32 a = (a0 + m) / dd;
    qs.push_back(a);
    m = a * dd - m;
    dd = (d - m * m) / dd;
    if (m == m1 && dd == d1) break;
  }
  const u32 per = static_cast<u32>(qs.size());
  Mat acc = quot(a0);
  for (u32 i = 0; i + 1u < per; ++i) acc = mul(acc, quot(qs[i]));
  if (per % 2u == 1u) {
    acc = mul(acc, quot(qs[per - 1u]));
    for (u32 i = 0; i + 1u < per; ++i) acc = mul(acc, quot(qs[i]));
  }
  return Oracle{static_cast<u32>(acc.a), static_cast<u32>(acc.c), per};
}

bool norm_ok(u32 d, u32 x, u32 y) {
  const u64 xx = (static_cast<u64>(x) * x) % kMod;
  const u64 yy = (static_cast<u64>(y) * y) % kMod;
  const u64 dy = (static_cast<u64>(d) * yy) % kMod;
  return (xx + kMod - dy) % kMod == 1u;
}

struct Known {
  u32 d, x, y, per;
};

TEST_CASE("hand-checked values", "[pell]") {
  // exact values from the definition: (x, y, period)
  const Known k[] = {{2u, 3u, 2u, 1u}, {3u, 2u, 1u, 2u}, {5u, 9u, 4u, 1u}, {6u, 5u, 2u, 2u}, {7u, 8u, 3u, 4u}, {8u, 3u, 1u, 2u}, {10u, 19u, 6u, 1u},
                     {11u, 10u, 3u, 2u}, {12u, 7u, 2u, 2u}, {13u, 649u, 180u, 5u}, {14u, 15u, 4u, 4u}, {15u, 4u, 1u, 2u}, {17u, 33u, 8u, 1u},
                     {18u, 17u, 4u, 2u}, {19u, 170u, 39u, 6u}, {20u, 9u, 2u, 2u}, {21u, 55u, 12u, 6u}, {22u, 197u, 42u, 6u}, {23u, 24u, 5u, 4u},
                     {29u, 9801u, 1820u, 5u}, {31u, 1520u, 273u, 8u}, {41u, 2049u, 320u, 3u}, {43u, 3482u, 531u, 10u}, {61u, 768074696u, 226153980u, 11u}};
  for (const Known& e : k) {
    const Res r = call(e.d);
    CHECK(r.r == PellResult::Ok);
    CHECK(r.x == e.x);
    CHECK(r.y == e.y);
    CHECK(r.per == e.per);
    CHECK(r.guards);
  }
  // the header's example: d = 61 gives (1766319049, 226153980) reduced mod p
  CHECK(1766319049u % kMod == 768074696u);
}

TEST_CASE("failure matrix: null first, then BadD, nothing written", "[pell]") {
  std::vector<u32> ds = {0u, 1u, 2u, 3u, 4u, 5u, 61u, kMaxD - 1u, kMaxD, kMaxD + 1u, kMaxD + 2u, 2u * kMaxD, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu};
  for (u32 r = 2; r <= 1024u; ++r) ds.push_back(r * r);  // every perfect square up to 2^20 (4 .. 2^20); 2^20 is a square
  for (u32 mask = 1; mask < 8u; ++mask) {                    // bit 0: x null, bit 1: y null, bit 2: period null
    for (u32 d : ds) {
      u32 xc = kSent, yc = kSent, pc = kSent;
      const PellResult r = pell_fundamental(d, (mask & 1u) ? nullptr : &xc, (mask & 2u) ? nullptr : &yc, (mask & 4u) ? nullptr : &pc);
      CHECK(r == PellResult::NullArgument);
      CHECK(xc == kSent);
      CHECK(yc == kSent);
      CHECK(pc == kSent);
    }
  }
  for (u32 d : ds) {
    const bool bad = (d < 2u || d > kMaxD || is_square(d));
    const Res r = call(d);
    CHECK((r.r == (bad ? PellResult::BadD : PellResult::Ok)));
    CHECK(r.guards);
    if (bad) {
      CHECK(r.x == kSent);
      CHECK(r.y == kSent);
      CHECK(r.per == kSent);
    } else {
      CHECK(r.x < kMod);
      CHECK(r.y < kMod);
      CHECK(r.per >= 1u);
    }
  }
  const Res last = call(kMaxD - 1u);  // the largest legal d
  CHECK(last.r == PellResult::Ok);
}

TEST_CASE("targeted d against exact big-integer residues", "[pell]") {
  for (const Target& t : kTargets) {
    const Res r = call(t.d);
    CHECK(r.r == PellResult::Ok);
    CHECK(r.x == t.x);
    CHECK(r.y == t.y);
    CHECK(r.per == t.period);
    CHECK(r.per <= kMaxPeriod);
  }
  const Res longest = call(1018879u);  // the longest period below 2^20
  CHECK(longest.per == kMaxPeriod);
}

TEST_CASE("every non-square d to 30000 against the exact big-integer digest", "[pell][exhaustive]") {
  u64 h = kInit;
  bool ok = true;
  for (u32 d = 2; d <= kExactMax; ++d) {
    if (is_square(d)) continue;
    const Res r = call(d);
    ok = ok && r.r == PellResult::Ok && r.guards;
    h = fnv(fnv(fnv(fnv(h, d), r.x), r.y), r.per);
  }
  CHECK(ok);
  CHECK(h == kExactDigest);
}

void sweep(u32 k) {
  const u32 lo = (k * kChunkSize < 2u) ? 2u : k * kChunkSize;
  const u32 hi = (k + 1u) * kChunkSize - 1u;  // at most 2^20 - 1
  u64 h = kInit;
  u32 count = 0, bad_impl = 0, bad_norm = 0, bad_bound = 0;
  for (u32 d = lo; d <= hi; ++d) {
    if (is_square(d)) continue;
    const Res r = call(d);
    const Oracle o = oracle(d);
    if (r.r != PellResult::Ok || !r.guards || r.x != o.x || r.y != o.y || r.per != o.per) ++bad_impl;
    if (!norm_ok(d, r.x, r.y)) ++bad_norm;
    if (r.per < 1u || r.per > kMaxPeriod || r.x >= kMod || r.y >= kMod) ++bad_bound;
    h = fnv(fnv(fnv(fnv(h, d), r.x), r.y), r.per);
    ++count;
  }
  CHECK(bad_impl == 0u);
  CHECK(bad_norm == 0u);
  CHECK(bad_bound == 0u);
  CHECK(count == kChunkCount[k]);
  CHECK(h == kChunkDigest[k]);
}

}  // namespace

TEST_CASE("sweep chunk 0: every non-square d from 2 to 131071", "[pell][exhaustive][sweep]") { sweep(0); }
TEST_CASE("sweep chunk 1: every non-square d from 131072 to 262143", "[pell][exhaustive][sweep]") { sweep(1); }
TEST_CASE("sweep chunk 2: every non-square d from 262144 to 393215", "[pell][exhaustive][sweep]") { sweep(2); }
TEST_CASE("sweep chunk 3: every non-square d from 393216 to 524287", "[pell][exhaustive][sweep]") { sweep(3); }
TEST_CASE("sweep chunk 4: every non-square d from 524288 to 655359", "[pell][exhaustive][sweep]") { sweep(4); }
TEST_CASE("sweep chunk 5: every non-square d from 655360 to 786431", "[pell][exhaustive][sweep]") { sweep(5); }
TEST_CASE("sweep chunk 6: every non-square d from 786432 to 917503", "[pell][exhaustive][sweep]") { sweep(6); }
TEST_CASE("sweep chunk 7: every non-square d from 917504 to 1048575", "[pell][exhaustive][sweep]") { sweep(7); }
