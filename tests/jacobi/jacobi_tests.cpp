// Tests for core/jacobi. Expected values come from the header's examples, from test-only oracles of a different method than the library (the symbol by factoring n
// with trial division and the Euler criterion; the square roots by a brute-force residue table for p < 8000 and by the checks x*x = a, the least-root rule and the
// Euler criterion above it; primality by Miller-Rabin with bases 2, 3, 5, 7, 11), from algebraic properties (jacobi equals kronecker for odd n, multiplicativity in a,
// periodicity in a modulo n, (-1/n), reciprocity, the Euler criterion for a prime n) and from FNV-1a 64-bit digests made by tools/jacobi_ref.py (Python standard
// library only: factoring by trial division, Miller-Rabin and Pollard rho, Euler-criterion Legendre symbols, a brute-force residue table below 8192, Cipolla above).
// Extents (see docs/jacobi.md): every a in [-1200, 1200] with every n in [0, 1200]; a list of edge a values with a list of edge n values; 537 base-2 pseudoprimes
// and Carmichael numbers as n and as p; 200000 sampled (a, n) pairs; every prime p below 2000 with a in [-p, 2p) and every composite below 2000 with edge a;
// every prime p in [2000, 8000) with every residue; 401 + 100 edge primes with 369 edge residues each; 200000 sampled (a, p) pairs; the check-order matrix.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "jacobi.h"

namespace {
using lumora::jacobi::JResult;
using lumora::jacobi::jacobi;
using lumora::jacobi::kronecker;
using lumora::jacobi::sqrt_mod_prime;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr i64 kI64Min = INT64_MIN;
constexpr i64 kI64Max = INT64_MAX;
constexpr i32 kGuard = static_cast<i32>(0xDEADBEEFu);
constexpr i32 kCell = 0x12345678;
constexpr u32 kGuardU = 0xDEADBEEFu;
constexpr u32 kCellU = 0x12345678u;

constexpr u64 kKron1Digest = 0xbb01d0c8b7634985ull;
constexpr u64 kKron1Count = 2883601u;
constexpr u64 kKron2Digest = 0x28dfd07e17b86e25ull;
constexpr u64 kKron2Count = 1736u;
constexpr u64 kKompDigest = 0xaf22ba763a5d56a7ull;
constexpr u64 kKompCount = 8055u;
constexpr u32 kComposites[537] = {341u, 561u, 645u, 1105u, 1387u, 1729u, 1905u, 2047u, 2465u, 2701u, 2821u, 3277u, 4033u, 4369u, 4371u, 4681u, 5461u, 6601u, 7957u, 8321u, 8481u, 8911u, 10261u, 10585u, 11305u, 12801u, 13741u, 13747u, 13981u, 14491u, 15709u, 15841u, 16705u, 18705u, 18721u, 19951u, 23001u, 23377u, 25761u, 29341u, 30121u, 30889u, 31417u, 31609u, 31621u, 33153u, 34945u, 35333u, 39865u, 41041u, 41665u, 42799u, 46657u, 49141u, 49981u, 52633u, 55245u, 57421u, 60701u, 60787u, 62745u, 63973u, 65077u, 65281u, 68101u, 72885u, 74665u, 75361u, 80581u, 83333u, 83665u, 85489u, 87249u, 88357u, 88561u, 90751u, 91001u, 93961u, 101101u, 104653u, 107185u, 113201u, 115921u, 121465u, 123251u, 126217u, 129889u, 129921u, 130561u, 131042u, 137149u, 149281u, 150851u, 154101u, 157641u, 158369u, 162193u, 162401u, 164737u, 172081u, 176149u, 181901u, 188057u, 188461u, 194221u, 196021u, 196093u, 204001u, 206601u, 208465u, 212421u, 215265u, 215749u, 219781u, 220729u, 223345u, 226801u, 228241u, 233017u, 241001u, 249841u, 252601u, 253241u, 256999u, 258511u, 264773u, 266305u, 271951u, 272251u, 275887u, 276013u, 278545u, 280601u, 282133u, 284581u, 285541u, 289941u, 294271u, 294409u, 314821u, 318361u, 323713u, 332949u, 334153u, 340561u, 341497u, 348161u, 357761u, 367081u, 387731u, 390937u, 396271u, 399001u, 401401u, 410041u, 422659u, 423793u, 427233u, 435671u, 443719u, 448921u, 449065u, 451905u, 452051u, 458989u, 464185u, 476971u, 481573u, 486737u, 488881u, 489997u, 493697u, 493885u, 512461u, 513629u, 514447u, 526593u, 530881u, 534061u, 552721u, 556169u, 563473u, 574561u, 574861u, 580337u, 582289u, 587861u, 588745u, 604117u, 611701u, 617093u, 622909u, 625921u, 635401u, 642001u, 647089u, 653333u, 656601u, 657901u, 658801u, 665281u, 665333u, 665401u, 670033u, 672487u, 679729u, 680627u, 683761u, 688213u, 710533u, 711361u, 721801u, 722201u, 722261u, 729061u, 738541u, 741751u, 742813u, 743665u, 745889u, 748657u, 757945u, 769567u, 769757u, 786961u, 800605u, 818201u, 825265u, 831405u, 838201u, 838861u, 841681u, 847261u, 852481u, 852841u, 873181u, 875161u, 877099u, 898705u, 915981u, 916327u, 934021u, 950797u, 976873u, 983401u, 997633u, 1004653u, 1016801u, 1018921u, 1023121u, 1024651u, 1033669u, 1050985u, 1052503u, 1052929u, 1053761u, 1064053u, 1073021u, 1082401u, 1082809u, 1092547u, 1093417u, 1104349u, 1106785u, 1109461u, 1128121u, 1132657u, 1139281u, 1141141u, 1145257u, 1152271u, 1157689u, 1168513u, 1193221u, 1194649u, 1207361u, 1246785u, 1251949u, 1252697u, 1275681u, 1277179u, 1293337u, 1302451u, 1306801u, 1325843u, 1333333u, 1357441u, 1357621u, 1373653u, 1394185u, 1397419u, 1398101u, 1419607u, 1433407u, 1441091u, 1457773u, 1459927u, 1461241u, 1463749u, 1472065u, 1472353u, 1472505u, 1485177u, 1489665u, 1493857u, 1500661u, 1507561u, 1507963u, 1509709u, 1520905u, 1529185u, 1530787u, 1533601u, 1533961u, 1534541u, 1537381u, 1549411u, 1569457u, 1579249u, 1584133u, 1608465u, 1615681u, 1620385u, 1643665u, 1678541u, 1690501u, 1711381u, 1719601u, 1730977u, 1735841u, 1746289u, 1755001u, 1773289u, 1801969u, 1809697u, 1811573u, 1815465u, 1826203u, 1827001u, 1830985u, 1837381u, 1839817u, 1840357u, 1857241u, 1876393u, 1892185u, 1896961u, 1907851u, 1908985u, 1909001u, 1937881u, 1969417u, 1987021u, 1993537u, 1994689u, 2004403u, 2008597u, 2035153u, 2077545u, 2081713u, 2085301u, 2089297u, 2100901u, 2113665u, 2113921u, 2121301u, 2134277u, 2142141u, 2144521u, 2162721u, 2163001u, 2165801u, 2171401u, 2181961u, 2184571u, 2205967u, 2213121u, 2232865u, 2233441u, 2261953u, 2264369u, 2269093u, 2284453u, 2288661u, 2290641u, 2299081u, 2304167u, 2313697u, 2327041u, 2350141u, 2387797u, 2414001u, 2419385u, 2433601u, 2434651u, 2455921u, 2487941u, 2491637u, 2503501u, 2508013u, 2510569u, 2531845u, 2628073u, 2704801u, 3057601u, 3146221u, 3224065u, 3581761u, 3664585u, 3828001u, 4335241u, 4463641u, 4767841u, 4903921u, 4909177u, 5031181u, 5049001u, 5148001u, 5310721u, 5444489u, 5481451u, 5632705u, 5968873u, 6049681u, 6054985u, 6189121u, 6313681u, 6733693u, 6840001u, 6868261u, 7207201u, 7306261u, 7306561u, 7414333u, 7416289u, 7428421u, 7429117u, 7455709u, 7462001u, 7516153u, 7519441u, 7546981u, 7656721u, 7674967u, 7693401u, 7724305u, 7725901u, 7759937u, 7803769u, 7808593u, 7814401u, 7820201u, 7883731u, 7995169u, 8012845u, 8036033u, 8041345u, 8043841u, 8095447u, 8134561u, 8137585u, 8137633u, 8180461u, 8209657u, 8231653u, 8239477u, 8280229u, 8321671u, 8322945u, 8341201u, 8355841u, 8362201u, 8384513u, 8388607u, 8462233u, 8534233u, 8640661u, 8646121u, 8650951u, 8656705u, 8719309u, 8719921u, 8725753u, 8727391u, 8745277u, 8812273u, 8830801u, 8902741u, 8916251u, 8927101u, 8992201u, 9006401u, 9037729u, 9040013u, 9046297u, 9056501u, 9063105u, 9069229u, 9073513u, 9084223u, 9106141u, 9131401u, 9143821u, 9223401u, 9224391u, 9273547u, 9345541u, 9371251u, 9439201u, 9480461u, 9494101u, 9533701u, 9564169u, 9567673u, 9582145u, 9585541u, 9588151u, 9591661u, 9613297u, 9638785u, 9692453u, 9724177u, 9729301u, 9774181u, 9816465u, 9834781u, 9863461u, 9890881u, 9908921u, 9920401u, 9995671u, 3215031751u, 4291428937u, 4292870399u, 4293001441u, 4293918703u, 4294967295u};
constexpr u32 kNumComposites = 537;
constexpr u32 kBigPrimes[501] = {65293u, 65309u, 65323u, 65327u, 65353u, 65357u, 65371u, 65381u, 65393u, 65407u, 65413u, 65419u, 65423u, 65437u, 65447u, 65449u, 65479u, 65497u, 65519u, 65521u, 7340033u, 13631489u, 23068673u, 26214401u, 28311553u, 69206017u, 70254593u, 81788929u, 101711873u, 104857601u, 111149057u, 113246209u, 120586241u, 132120577u, 136314881u, 138412033u, 141557761u, 147849217u, 155189249u, 158334977u, 163577857u, 167772161u, 169869313u, 185597953u, 186646529u, 199229441u, 204472321u, 211812353u, 221249537u, 230686721u, 246415361u, 249561089u, 257949697u, 270532609u, 274726913u, 290455553u, 305135617u, 311427073u, 330301441u, 347078657u, 359661569u, 361758721u, 377487361u, 383778817u, 387973121u, 399507457u, 409993217u, 415236097u, 447741953u, 459276289u, 463470593u, 468713473u, 469762049u, 493879297u, 531628033u, 576716801u, 581959681u, 595591169u, 597688321u, 605028353u, 635437057u, 639631361u, 645922817u, 648019969u, 655360001u, 666894337u, 683671553u, 710934529u, 715128833u, 718274561u, 740294657u, 745537537u, 754974721u, 770703361u, 786432001u, 799014913u, 800063489u, 802160641u, 818937857u, 824180737u, 833617921u, 850395137u, 862978049u, 880803841u, 883949569u, 897581057u, 899678209u, 907018241u, 913309697u, 918552577u, 919601153u, 924844033u, 925892609u, 935329793u, 938475521u, 940572673u, 943718401u, 950009857u, 957349889u, 962592769u, 972029953u, 975175681u, 976224257u, 985661441u, 998244353u, 1004535809u, 1007681537u, 1012924417u, 1045430273u, 1051721729u, 1053818881u, 1073741399u, 1073741419u, 1073741441u, 1073741467u, 1073741477u, 1073741503u, 1073741527u, 1073741561u, 1073741567u, 1073741621u, 1073741651u, 1073741663u, 1073741671u, 1073741689u, 1073741717u, 1073741719u, 1073741723u, 1073741741u, 1073741783u, 1073741789u, 1085276161u, 1092616193u, 1095761921u, 1102053377u, 1107296257u, 1127219201u, 1133510657u, 1138753537u, 1146093569u, 1151336449u, 1158676481u, 1161822209u, 1185939457u, 1192230913u, 1205862401u, 1211105281u, 1212153857u, 1214251009u, 1218445313u, 1224736769u, 1261436929u, 1281359873u, 1286602753u, 1300234241u, 1306525697u, 1315962881u, 1321205761u, 1327497217u, 1365245953u, 1366294529u, 1380974593u, 1397751809u, 1410334721u, 1412431873u, 1415577601u, 1431306241u, 1437597697u, 1438646273u, 1479540737u, 1484783617u, 1485832193u, 1492123649u, 1504706561u, 1513095169u, 1536163841u, 1541406721u, 1555038209u, 1558183937u, 1570766849u, 1572864001u, 1594884097u, 1601175553u, 1607467009u, 1630535681u, 1638924289u, 1643118593u, 1651507201u, 1654652929u, 1695547393u, 1699741697u, 1709178881u, 1711276033u, 1724907521u, 1790967809u, 1811939329u, 1835008001u, 1863319553u, 1866465281u, 1868562433u, 1888485377u, 1893728257u, 1894776833u, 1922039809u, 1931476993u, 1945108481u, 1953497089u, 1963982849u, 1978662913u, 1998585857u, 2013265921u, 2025848833u, 2035286017u, 2047868929u, 2070937601u, 2077229057u, 2088763393u, 2095054849u, 2099249153u, 2113929217u, 2114977793u, 2130706433u, 2147483137u, 2147483171u, 2147483179u, 2147483237u, 2147483249u, 2147483269u, 2147483323u, 2147483353u, 2147483399u, 2147483423u, 2147483477u, 2147483489u, 2147483497u, 2147483543u, 2147483549u, 2147483563u, 2147483579u, 2147483587u, 2147483629u, 2147483647u, 2151677953u, 2154823681u, 2168455169u, 2171600897u, 2183135233u, 2184183809u, 2187329537u, 2198863873u, 2203058177u, 2208301057u, 2212495361u, 2217738241u, 2220883969u, 2224029697u, 2233466881u, 2250244097u, 2268069889u, 2272264193u, 2281701377u, 2296381441u, 2299527169u, 2303721473u, 2313158657u, 2319450113u, 2322595841u, 2344615937u, 2349858817u, 2362441729u, 2369781761u, 2387607553u, 2403336193u, 2419064833u, 2448424961u, 2451570689u, 2470445057u, 2472542209u, 2475687937u, 2483027969u, 2511339521u, 2519728129u, 2533359617u, 2558525441u, 2593128449u, 2607808513u, 2629828609u, 2634022913u, 2645557249u, 2649751553u, 2656043009u, 2671771649u, 2673868801u, 2680160257u, 2705326081u, 2717908993u, 2718957569u, 2722103297u, 2727346177u, 2733637633u, 2739929089u, 2747269121u, 2766143489u, 2780823553u, 2805989377u, 2810183681u, 2821718017u, 2828009473u, 2844786689u, 2854223873u, 2868903937u, 2876243969u, 2885681153u, 2907701249u, 2909798401u, 2910846977u, 2912944129u, 2916089857u, 2925527041u, 2953838593u, 2967470081u, 2979004417u, 2980052993u, 2994733057u, 2998927361u, 3001024513u, 3002073089u, 3005218817u, 3035627521u, 3074424833u, 3079667713u, 3083862017u, 3085959169u, 3109027841u, 3111124993u, 3112173569u, 3114270721u, 3115319297u, 3121610753u, 3140485121u, 3164602369u, 3175088129u, 3177185281u, 3192913921u, 3208642561u, 3211788289u, 3214934017u, 3215982593u, 3221225473u, 3224371201u, 3238002689u, 3252682753u, 3255828481u, 3277848577u, 3285188609u, 3304062977u, 3306160129u, 3313500161u, 3318743041u, 3325034497u, 3331325953u, 3335520257u, 3340763137u, 3357540353u, 3360686081u, 3407872001u, 3414163457u, 3416260609u, 3429892097u, 3435134977u, 3442475009u, 3450863617u, 3451912193u, 3466592257u, 3476029441u, 3479175169u, 3483369473u, 3489660929u, 3508535297u, 3510632449u, 3526361089u, 3549429761u, 3552575489u, 3577741313u, 3582984193u, 3596615681u, 3602907137u, 3612344321u, 3614441473u, 3615490049u, 3628072961u, 3634364417u, 3665821697u, 3671064577u, 3686793217u, 3694133249u, 3706716161u, 3722444801u, 3725590529u, 3730833409u, 3738173441u, 3749707777u, 3757047809u, 3765436417u, 3771727873u, 3803185153u, 3818913793u, 3837788161u, 3845128193u, 3864002561u, 3872391169u, 3892314113u, 3898605569u, 3916431361u, 3923771393u, 3938451457u, 3942645761u, 3960471553u, 3970957313u, 4002414593u, 4013948929u, 4073717761u, 4076863489u, 4096786433u, 4106223617u, 4112515073u, 4147118081u, 4153409537u, 4183818241u, 4191158273u, 4194304001u, 4199546881u, 4210032641u, 4213178369u, 4221566977u, 4231004161u, 4241489921u, 4253024257u, 4260364289u, 4276092929u, 4293918721u, 4294966217u, 4294966231u, 4294966237u, 4294966243u, 4294966297u, 4294966337u, 4294966367u, 4294966373u, 4294966427u, 4294966441u, 4294966447u, 4294966477u, 4294966553u, 4294966583u, 4294966591u, 4294966619u, 4294966639u, 4294966651u, 4294966657u, 4294966661u, 4294966667u, 4294966769u, 4294966813u, 4294966829u, 4294966877u, 4294966909u, 4294966927u, 4294966943u, 4294966981u, 4294966997u, 4294967029u, 4294967087u, 4294967111u, 4294967143u, 4294967161u, 4294967189u, 4294967197u, 4294967231u, 4294967279u, 4294967291u};
constexpr u32 kNumBigPrimes = 501;
constexpr u64 kKronS0Digest = 0x2a42765d0c37cd59ull;
constexpr u64 kKronS0Count = 50000u;
constexpr u64 kKronS1Digest = 0x196d351e5dd7746cull;
constexpr u64 kKronS1Count = 50000u;
constexpr u64 kKronS2Digest = 0xca63eb50e2647929ull;
constexpr u64 kKronS2Count = 50000u;
constexpr u64 kKronS3Digest = 0x592e7a3d36507ed0ull;
constexpr u64 kKronS3Count = 50000u;
constexpr u64 kSq1aDigest = 0xd109a8f9bd5ca888ull;
constexpr u64 kSq1aCount = 844726u;
constexpr u64 kSq1bDigest = 0xaac30c66dbd7ec30ull;
constexpr u64 kSq1bCount = 3461516u;
constexpr u64 kSq2Digest = 0xa5f6d8a710326c42ull;
constexpr u64 kSq2Count = 178857u;
constexpr u64 kSqS0Digest = 0x8115375806fd26e1ull;
constexpr u64 kSqS0Count = 50000u;
constexpr u64 kSqS1Digest = 0x4f12b6c9cf7ba8a8ull;
constexpr u64 kSqS1Count = 50000u;
constexpr u64 kSqS2Digest = 0xae50a515eb96cae4ull;
constexpr u64 kSqS2Count = 50000u;
constexpr u64 kSqS3Digest = 0x86dfe4219b11763dull;
constexpr u64 kSqS3Count = 50000u;


u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }
u64 fi(i64 v) { return static_cast<u64>(v); }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// ---- oracles ----
// Powers by left-to-right square and multiply (the library goes right to left); p below 2^32 so every product is below 2^64.
u64 o_pow(u64 b, u64 e, u64 p) {
  u64 r = 1u % p;
  for (u32 i = 64u; i > 0u; --i) {
    r = r * r % p;
    if (((e >> (i - 1u)) & 1u) != 0u) r = r * (b % p) % p;
  }
  return r;
}
bool o_is_prime(u64 n) {
  if (n < 2u) return false;
  const u64 sm[5] = {2u, 3u, 5u, 7u, 11u};
  for (u32 i = 0u; i < 5u; ++i) {
    if (n % sm[i] == 0u) return n == sm[i];
  }
  u64 d = n - 1u;
  u32 s = 0u;
  while ((d & 1u) == 0u) {
    d >>= 1;
    ++s;
  }
  for (u32 i = 0u; i < 5u; ++i) {
    u64 x = o_pow(sm[i], d, n);
    if (x == 1u || x == n - 1u) continue;
    bool witness = true;
    for (u32 j = 1u; j < s; ++j) {
      x = x * x % n;
      if (x == n - 1u) {
        witness = false;
        break;
      }
    }
    if (witness) return false;
  }
  return true;
}
i64 floor_mod(i64 a, u64 m) {
  i64 r = a % static_cast<i64>(m);
  if (r < 0) r += static_cast<i64>(m);
  return r;
}
int o_leg(i64 a, u64 p) {
  const u64 r = static_cast<u64>(floor_mod(a, p));
  if (r == 0u) return 0;
  return o_pow(r, (p - 1u) / 2u, p) == 1u ? 1 : -1;
}
// The symbol by factoring n with trial division.
int o_kron(i64 a, u64 n) {
  if (n == 0u) return (a == 1 || a == -1) ? 1 : 0;
  int r = 1;
  u64 m = n;
  for (u64 d = 2u; d * d <= m; ++d) {
    while (m % d == 0u) {
      m /= d;
      if (d == 2u) {
        const i64 low = floor_mod(a, 8u);
        if (low % 2 == 0) return 0;
        if (low == 3 || low == 5) r = -r;
      } else {
        r *= o_leg(a, d);
        if (r == 0) return 0;
      }
    }
  }
  if (m > 1u) {
    if (m == 2u) {
      const i64 low = floor_mod(a, 8u);
      if (low % 2 == 0) return 0;
      if (low == 3 || low == 5) r = -r;
    } else {
      r *= o_leg(a, m);
    }
  }
  return r;
}

// ---- library calls between guard words; v is the value for Ok, otherwise a failure left the cell alone ----
struct Lk {
  JResult r;
  i32 v;
  bool guards;
};
Lk lk(bool strict, i64 a, u32 n) {
  i32 cell[3] = {kGuard, kCell, kGuard};
  const JResult r = strict ? jacobi(a, n, &cell[1]) : kronecker(a, n, &cell[1]);
  Lk x{r, 0, cell[0] == kGuard && cell[2] == kGuard};
  if (r == JResult::Ok) {
    x.v = cell[1];
  } else {
    x.guards = x.guards && cell[1] == kCell;
  }
  return x;
}
struct Ls {
  JResult r;
  u32 v;
  bool guards;
};
Ls ls(i64 a, u32 p) {
  u32 cell[3] = {kGuardU, kCellU, kGuardU};
  const JResult r = sqrt_mod_prime(a, p, &cell[1]);
  Ls x{r, 0u, cell[0] == kGuardU && cell[2] == kGuardU};
  if (r == JResult::Ok) {
    x.v = cell[1];
  } else {
    x.guards = x.guards && cell[1] == kCellU;
  }
  return x;
}
u64 code_of(JResult r) { return static_cast<u64>(r); }

// A kronecker result against the oracle value.
bool kron_ok(i64 a, u32 n, int want) {
  const Lk l = lk(false, a, n);
  return l.guards && l.r == JResult::Ok && l.v == want;
}

// ---- generators (identical to tools/jacobi_ref.py) ----
void kcase(u64 pos, i64& a, u32& n) {
  const u64 w0 = splitmix_at(pos);
  const u64 w1 = splitmix_at(pos + 1u);
  const u32 bits = 1u + static_cast<u32>(w0 % 63u);
  const u64 mag = w1 >> (64u - bits);
  const i64 m = static_cast<i64>(mag);
  a = ((w0 >> 8) & 1u) != 0u ? -m : m;
  if (((w0 >> 20) % 64u) == 0u) a = kI64Min;
  const u32 nb = 1u + static_cast<u32>((w0 >> 12) % 32u);
  u64 nn = w1 & ((u64{1} << nb) - 1u);
  const u64 t = (w0 >> 24) & 3u;
  if (t == 0u) {
    nn |= 1u;
  } else if (t == 1u) {
    nn &= ~u64{1};
  } else if (t == 2u) {
    nn = ((nn | 1u) << ((w0 >> 30) % 8u)) & 0xFFFFFFFFull;
  }
  n = static_cast<u32>(nn);
}
void scase(u64 pos, i64& a, u32& p) {
  const u64 w0 = splitmix_at(pos);
  const u64 w1 = splitmix_at(pos + 1u);
  const u32 bits = 2u + static_cast<u32>(w0 % 31u);
  u64 q = (w1 >> (64u - bits)) | (u64{1} << (bits - 1u)) | 1u;
  while (q < (u64{1} << 32) && !o_is_prime(q)) q += 2u;
  if (q >= (u64{1} << 32)) q = 4294967291ull;
  p = static_cast<u32>(q);
  const u64 w2 = splitmix_at(pos + 2u);
  const u32 mb = 1u + static_cast<u32>((w0 >> 8) % 64u);
  u64 mag = w2 >> (64u - mb);
  if (mag > static_cast<u64>(kI64Max)) mag = static_cast<u64>(kI64Max);
  const i64 m = static_cast<i64>(mag);
  a = ((w0 >> 16) & 1u) != 0u ? -m : m;
  const u64 t = (w0 >> 24) & 7u;
  if (t == 0u) {
    a = kI64Min;
  } else if (t == 1u) {
    const u64 x = w2 & 0xFFFFFFFFull;
    a = static_cast<i64>(x * x % q);
  }
}

// ---- kronecker families ----
// Folds (a, n, v + 1) for the case, and adds the digest plus the oracle verdict (when oracle is true).
void kron_case(i64 a, u32 n, u64& h, bool& ok, bool oracle) {
  const Lk l = lk(false, a, n);
  if (!l.guards || l.r != JResult::Ok) ok = false;
  if (oracle && l.v != o_kron(a, n)) ok = false;
  h = fold(fold(fold(h, fi(a)), n), static_cast<u64>(l.v + 1));
}
// The properties of one case (a, n): jacobi equals kronecker for odd n and refuses even n; multiplicativity in a; periodicity for odd n; (-1/n); reciprocity.
void kron_props(i64 a, u32 n, bool& ok) {
  const Lk k = lk(false, a, n);
  const Lk j = lk(true, a, n);
  if ((n & 1u) == 0u) {
    if (j.r != JResult::BadModulus || !j.guards) ok = false;
  } else {
    if (j.r != JResult::Ok || !j.guards || j.v != k.v) ok = false;
  }
  const i64 a1 = static_cast<i32>(static_cast<u32>(fi(a)));
  const i64 a2 = static_cast<i32>(static_cast<u32>(fi(a) >> 32));
  const Lk k1 = lk(false, a1, n);
  const Lk k2 = lk(false, a2, n);
  const Lk k12 = lk(false, a1 * a2, n);
  if (n != 0u && k12.v != k1.v * k2.v) ok = false;
  if ((n & 1u) != 0u) {
    if (a <= kI64Max - static_cast<i64>(n)) {
      const Lk kp = lk(false, a + static_cast<i64>(n), n);
      if (kp.v != k.v) ok = false;
    }
    if (a >= kI64Min + static_cast<i64>(n)) {
      const Lk kp = lk(false, a - static_cast<i64>(n), n);
      if (kp.v != k.v) ok = false;
    }
    const Lk km1 = lk(false, -1, n);
    if (km1.v != ((n % 4u == 1u) ? 1 : -1)) ok = false;
    const u32 m = static_cast<u32>(fi(a)) | 1u;
    const Lk r1 = lk(false, static_cast<i64>(m), n);
    const Lk r2 = lk(false, static_cast<i64>(n), m);
    const int sgn = (((m - 1u) / 2u) % 2u == 1u && ((n - 1u) / 2u) % 2u == 1u) ? -1 : 1;
    if (r1.v * r2.v != sgn * r1.v * r1.v) ok = false;
  }
}

// ---- sqrt checks ----
// An oracle verdict independent of the library: NotPrime, NoRoot by Euler, or Ok with a root that squares to a, is the least of the two and (below 8000) equals the table.
bool sqrt_ok(i64 a, u32 p, const Ls& l) {
  if (!l.guards) return false;
  if (!o_is_prime(p)) return l.r == JResult::NotPrime;
  const u64 r = static_cast<u64>(floor_mod(a, p));
  const bool has = p == 2u || r == 0u || o_pow(r, (p - 1u) / 2u, p) == 1u;
  if (!has) return l.r == JResult::NoRoot;
  if (l.r != JResult::Ok) return false;
  const u64 x = l.v;
  const bool sq = x < p && x * x % p == r;
  const bool least = x <= p - x;
  return sq && least;
}
void sq_case(i64 a, u32 p, u64& h, bool& ok) {
  const Ls l = ls(a, p);
  if (!sqrt_ok(a, p, l)) ok = false;
  h = fold(fold(fold(fold(h, fi(a)), p), code_of(l.r)), l.v);
}
// Brute residue table: least root of every residue below p.
struct Brute {
  u32 t[8000];
  u32 p = 0u;
  void build(u32 pp) {
    p = pp;
    for (u32 i = 0u; i < p; ++i) t[i] = 0xFFFFFFFFu;
    for (u32 x = p; x > 0u; --x) t[(static_cast<u64>(x - 1u) * (x - 1u)) % p] = x - 1u;
  }
};
void sq_brute_case(const Brute& b, i64 a, u64& h, bool& ok) {
  const Ls l = ls(a, b.p);
  const u32 x = b.t[floor_mod(a, b.p)];
  const u64 code = x == 0xFFFFFFFFu ? 4u : 0u;
  const u64 v = x == 0xFFFFFFFFu ? 0u : x;
  if (!l.guards || code_of(l.r) != code || l.v != v) ok = false;
  h = fold(fold(fold(fold(h, fi(a)), b.p), code_of(l.r)), l.v);
}

TEST_CASE("jacobi: header examples") {
  CHECK(kron_ok(1, 1, 1));
  CHECK(kron_ok(0, 1, 1));
  CHECK(kron_ok(1, 0, 1));
  CHECK(kron_ok(-1, 0, 1));
  CHECK(kron_ok(2, 0, 0));
  CHECK(kron_ok(0, 0, 0));
  CHECK(kron_ok(2, 7, 1));
  CHECK(kron_ok(3, 7, -1));
  CHECK(kron_ok(0, 3, 0));
  CHECK(kron_ok(-1, 3, -1));
  CHECK(kron_ok(2, 15, 1));
  CHECK(kron_ok(2, 2, 0));
  CHECK(kron_ok(1, 2, 1));
  CHECK(kron_ok(3, 2, -1));
  CHECK(kron_ok(5, 2, -1));
  CHECK(kron_ok(7, 2, 1));
  CHECK(kron_ok(-1, 2, 1));
  CHECK(kron_ok(3, 4, 1));
  CHECK(kron_ok(3, 8, -1));
  CHECK(kron_ok(kI64Min, 3, o_kron(kI64Min, 3)));
  CHECK(kron_ok(kI64Max, 4294967295u, o_kron(kI64Max, 4294967295u)));
  const Ls s1 = ls(2, 7);
  CHECK((s1.r == JResult::Ok && s1.v == 3u && s1.guards));
  const Ls s2 = ls(10, 13);
  CHECK((s2.r == JResult::Ok && s2.v == 6u && s2.guards));
  const Ls s3 = ls(15, 17);
  CHECK((s3.r == JResult::Ok && s3.v == 7u && s3.guards));
  const Ls s4 = ls(-1, 5);
  CHECK((s4.r == JResult::Ok && s4.v == 2u && s4.guards));
  const Ls s5 = ls(3, 5);
  CHECK((s5.r == JResult::NoRoot && s5.guards));
  const Ls s6 = ls(3, 2);
  CHECK((s6.r == JResult::Ok && s6.v == 1u && s6.guards));
  const Ls s7 = ls(0, 2);
  CHECK((s7.r == JResult::Ok && s7.v == 0u && s7.guards));
  const Ls s8 = ls(0, 11);
  CHECK((s8.r == JResult::Ok && s8.v == 0u && s8.guards));
  const Ls s9 = ls(9, 15);
  CHECK((s9.r == JResult::NotPrime && s9.guards));
}

TEST_CASE("jacobi: E check order, no write on failure") {
  const u32 ns[6] = {0u, 1u, 2u, 4u, 9u, 4294967295u};
  bool ok = true;
  for (u32 i = 0u; i < 6u; ++i) {
    if (kronecker(5, ns[i], nullptr) != JResult::NullArgument) ok = false;
    if (jacobi(5, ns[i], nullptr) != JResult::NullArgument) ok = false;
    if (sqrt_mod_prime(5, ns[i], nullptr) != JResult::NullArgument) ok = false;
  }
  CHECK(ok);
  const u32 evens[5] = {0u, 2u, 4u, 4294967294u, 1073741824u};
  for (u32 i = 0u; i < 5u; ++i) {
    const Lk l = lk(true, 3, evens[i]);
    CHECK((l.r == JResult::BadModulus && l.guards));
  }
  const Lk k0 = lk(false, 3, 0u);
  CHECK((k0.r == JResult::Ok && k0.v == 0 && k0.guards));
  const u32 comps[5] = {0u, 1u, 4u, 9u, 4294967295u};
  for (u32 i = 0u; i < 5u; ++i) {
    const Ls l = ls(4, comps[i]);
    CHECK((l.r == JResult::NotPrime && l.guards));
  }
  const Ls nr = ls(3, 7);
  CHECK((nr.r == JResult::NoRoot && nr.guards));
  const Ls nr2 = ls(kI64Min, 4294967291u);
  CHECK((nr2.guards && (nr2.r == JResult::NoRoot || nr2.r == JResult::Ok)));
}

TEST_CASE("jacobi: M every modular product is below 2^64") {
  // The library's products are of two residues below p < 2^32: at most (p - 1)^2. Check that the largest such product does not wrap and is below p * 2^32.
  bool ok = true;
  for (u32 pi = 0u; pi < kNumBigPrimes; ++pi) {
    const u64 m = u64{kBigPrimes[pi]} - 1u;
    const u64 sq = m * m;
    if (sq / m != m) ok = false;
    if (sq / (u64{kBigPrimes[pi]}) >= (u64{1} << 32)) ok = false;
  }
  const u64 top = 4294967294ull * 4294967294ull;
  if (top != 18446744056529682436ull) ok = false;
  CHECK(ok);
}

TEST_CASE("jacobi: S1a primes below 2000 with a in [-p, 2p), composites with edge a") {
  Brute* b = new Brute;
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 p = 0u; p < 2000u; ++p) {
    if (!o_is_prime(p)) {
      const i64 av[8] = {0, 1, -1, 2, static_cast<i64>(p), static_cast<i64>(p) + 1, 12345, kI64Min};
      for (u32 j = 0u; j < 8u; ++j) {
        const Ls l = ls(av[j], p);
        if (l.r != JResult::NotPrime || !l.guards) ok = false;
        h = fold(fold(fold(h, fi(av[j])), p), code_of(JResult::NotPrime));
        ++cnt;
      }
      continue;
    }
    b->build(p);
    for (i64 a = -static_cast<i64>(p); a < 2 * static_cast<i64>(p); ++a) {
      sq_brute_case(*b, a, h, ok);
      ++cnt;
    }
  }
  delete b;
  CHECK(ok);
  CHECK(cnt == kSq1aCount);
  CHECK(h == kSq1aDigest);
}

TEST_CASE("jacobi: K1 every a in [-1200, 1200] and n in [0, 1200]") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (i64 a = -1200; a <= 1200; ++a) {
    for (u32 n = 0u; n <= 1200u; ++n) {
      const bool oracle = a >= -120 && a <= 120 && n <= 300u;
      kron_case(a, n, h, ok, oracle);
      ++cnt;
    }
  }
  CHECK(ok);
  CHECK(cnt == kKron1Count);
  CHECK(h == kKron1Digest);
}

TEST_CASE("jacobi: K2 edge a values with edge n values") {
  i64 av[64];
  u32 na = 0u;
  const i64 first[7] = {kI64Min, kI64Min + 1, -(i64{1} << 62), -(i64{1} << 32) - 1, -(i64{1} << 32), -(i64{1} << 31) - 1, -(i64{1} << 31)};
  for (u32 i = 0u; i < 7u; ++i) av[na++] = first[i];
  for (i64 x = -65537; x < 0; x += 4099) av[na++] = x;
  av[na++] = -1;
  av[na++] = 0;
  av[na++] = 1;
  for (i64 x = 2; x < 65538; x += 4099) av[na++] = x;
  const i64 last[14] = {(i64{1} << 31) - 1, i64{1} << 31, (i64{1} << 32) - 1, i64{1} << 32, i64{1} << 40, i64{1} << 62, kI64Max - 1, kI64Max,
                        3 * (i64{1} << 30), -3 * (i64{1} << 30), 4294967291ll, -4294967291ll, 999999999999ll, -999999999999ll};
  for (u32 i = 0u; i < 14u; ++i) av[na++] = last[i];
  const u32 nv[31] = {0u, 1u, 2u, 3u, 4u, 5u, 8u, 9u, 15u, 16u, 255u, 256u, 65535u, 65536u, 65537u, 2147483647u, 2147483648u, 2147483649u, 4294967294u, 4294967295u, 4294967291u,
                      65521u * 65521u, 3221225472u, 223092870u, 1078282205u, 1073741824u, 1073741825u, 3145728u, 4293918721u, 4294967281u, 4294967279u};
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 i = 0u; i < 31u; ++i) {
    for (u32 j = 0u; j < na; ++j) {
      const bool oracle = nv[i] < 70000u || nv[i] == 65521u * 65521u || nv[i] == 4294967291u;
      kron_case(av[j], nv[i], h, ok, oracle);
      kron_props(av[j], nv[i], ok);
      ++cnt;
    }
  }
  CHECK(na == 56u);
  CHECK(ok);
  CHECK(cnt == kKron2Count);
  CHECK(h == kKron2Digest);
}

TEST_CASE("jacobi: komp pseudoprimes and Carmichael numbers as n and as p") {
  const i64 av[15] = {-1, 0, 1, 2, 3, -2, 5, 7, 11, 12345, -12345, i64{1} << 40, -(i64{1} << 40), kI64Min, kI64Max};
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 i = 0u; i < kNumComposites; ++i) {
    const u32 n = kComposites[i];
    if (o_is_prime(n)) ok = false;
    for (u32 j = 0u; j < 15u; ++j) {
      kron_case(av[j], n, h, ok, n < 1048576u);
      kron_props(av[j], n, ok);
      ++cnt;
      const Ls l = ls(av[j], n);
      if (l.r != JResult::NotPrime || !l.guards) ok = false;
    }
  }
  CHECK(ok);
  CHECK(cnt == kKompCount);
  CHECK(h == kKompDigest);
}

void kron_shard(u32 s, u64 want_digest, u64 want_count) {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 i = 0u; i < 50000u; ++i) {
    i64 a;
    u32 n;
    kcase((u64{1} << 40) + s * 100000u + 2u * i, a, n);
    kron_case(a, n, h, ok, false);
    kron_props(a, n, ok);
    ++cnt;
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want_digest);
}
TEST_CASE("jacobi: K3 shard 0") { kron_shard(0u, kKronS0Digest, kKronS0Count); }
TEST_CASE("jacobi: K3 shard 1") { kron_shard(1u, kKronS1Digest, kKronS1Count); }
TEST_CASE("jacobi: K3 shard 2") { kron_shard(2u, kKronS2Digest, kKronS2Count); }
TEST_CASE("jacobi: K3 shard 3") { kron_shard(3u, kKronS3Digest, kKronS3Count); }

TEST_CASE("jacobi: K4 prime n agrees with the Euler criterion") {
  bool ok = true;
  u32 primes = 0u;
  for (u32 pi = 0u; pi < kNumBigPrimes; ++pi) {
    const u32 p = kBigPrimes[pi];
    for (u32 i = 0u; i < 20u; ++i) {
      const i64 a = static_cast<i64>(splitmix_at((u64{1} << 44) + pi * 20u + i) >> 1) * ((i & 1u) != 0u ? -1 : 1);
      if (!kron_ok(a, p, o_leg(a, p))) ok = false;
    }
    ++primes;
  }
  CHECK(ok);
  CHECK(primes == kNumBigPrimes);
}

void sq_shard(u32 s, u64 want_digest, u64 want_count) {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 i = 0u; i < 50000u; ++i) {
    i64 a;
    u32 p;
    scase((u64{1} << 43) + s * 150000u + 3u * i, a, p);
    sq_case(a, p, h, ok);
    ++cnt;
  }
  CHECK(ok);
  CHECK(cnt == want_count);
  CHECK(h == want_digest);
}
TEST_CASE("jacobi: S3 shard 0") { sq_shard(0u, kSqS0Digest, kSqS0Count); }
TEST_CASE("jacobi: S3 shard 1") { sq_shard(1u, kSqS1Digest, kSqS1Count); }
TEST_CASE("jacobi: S3 shard 2") { sq_shard(2u, kSqS2Digest, kSqS2Count); }
TEST_CASE("jacobi: S3 shard 3") { sq_shard(3u, kSqS3Digest, kSqS3Count); }

TEST_CASE("jacobi: S1b every prime in [2000, 8000) with every residue") {
  Brute* b = new Brute;
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 p = 2000u; p < 8000u; ++p) {
    if (!o_is_prime(p)) continue;
    b->build(p);
    for (i64 a = 0; a < static_cast<i64>(p); ++a) {
      sq_brute_case(*b, a, h, ok);
      ++cnt;
    }
  }
  delete b;
  CHECK(ok);
  CHECK(cnt == kSq1bCount);
  CHECK(h == kSq1bDigest);
}

TEST_CASE("jacobi: S2 edge primes with edge residues") {
  u64 h = kFnv0;
  u64 cnt = 0u;
  bool ok = true;
  for (u32 pi = 0u; pi < kNumBigPrimes; ++pi) {
    const u32 p = kBigPrimes[pi];
    if (!o_is_prime(p)) ok = false;
    const i64 sp = static_cast<i64>(p);
    const i64 head[9] = {0, 1, -1, 2, sp - 1, sp, sp + 1, kI64Min, kI64Max};
    for (u32 i = 0u; i < 9u; ++i) {
      sq_case(head[i], p, h, ok);
      ++cnt;
    }
    for (i64 x = 1; x <= 40; ++x) {
      sq_case(x * x, p, h, ok);
      ++cnt;
    }
    for (i64 x = 1; x <= 8; ++x) {
      sq_case(-(x * x), p, h, ok);
      ++cnt;
    }
    for (u32 i = 0u; i < 300u; ++i) {
      sq_case(static_cast<i64>(splitmix_at((u64{1} << 42) + u64{p} * 7u + i) & ((u64{1} << 40) - 1u)), p, h, ok);
      ++cnt;
    }
  }
  CHECK(ok);
  CHECK(cnt == kSq2Count);
  CHECK(h == kSq2Digest);
}

}  // namespace
