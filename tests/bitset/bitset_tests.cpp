// Tests for core/bitset (scope AM). Expected values come from an independent Python reference (tools/bitset_ref.py: a plain list of bits
// as the model, never words and masks, and its own FNV) and are pasted below: scenarios that run scripted operations (valid and invalid
// indices) on seeded words and fold every result and the final words, padding included, into a digest. Direct cases cover words_for,
// the argument rules, the padding rules and find_next against a bit-by-bit oracle. Every set is an exact-size heap vector, so ASan sees
// any access past the last word. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "bitset.h"

using namespace lumora::bitset;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kSent = 0xDEADBEEFu;

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
  u64 next64() {
    const u64 hi = next();
    return (hi << 32) | next();
  }
  u32 rng(u32 lo, u32 hi) { return static_cast<u32>(lo + next() % (static_cast<u64>(hi) - lo + 1)); }
};

struct ScCase {
  u32 nbits;
  u64 seed;
  int mode;
  u32 nops;
  u64 digest;
};

const ScCase kScCases[] = {{0, 1001, 0, 300, 9860798605671301772ull}, {0, 1002, 1, 300, 9352614959628805508ull}, {0, 1003, 2, 300, 6758727963576751006ull}, {0, 1004, 3, 300, 15595282598708154849ull}, {1, 1005, 0, 300, 6382656981109435933ull}, {1, 1006, 1, 300, 5343628953266976065ull}, {1, 1007, 2, 300, 2076508256874854556ull}, {1, 1008, 3, 300, 4191175856732598871ull}, {2, 1009, 0, 300, 882057627379582037ull}, {2, 1010, 1, 300, 398427935226532086ull}, {2, 1011, 2, 300, 8988328691518787923ull}, {2, 1012, 3, 300, 10657494755765837779ull}, {7, 1013, 0, 300, 4070819573334124346ull}, {7, 1014, 1, 300, 11083267839478149585ull}, {7, 1015, 2, 300, 1642676753987700057ull}, {7, 1016, 3, 300, 8962329949989470452ull}, {31, 1017, 0, 300, 15604591478496706734ull}, {31, 1018, 1, 300, 6788570400684433501ull}, {31, 1019, 2, 300, 1710776554942675769ull}, {31, 1020, 3, 300, 11018572253838444293ull}, {32, 1021, 0, 300, 2865572673003332816ull}, {32, 1022, 1, 300, 11177496131284008746ull}, {32, 1023, 2, 300, 14824525289303629878ull}, {32, 1024, 3, 300, 5695217589220939861ull}, {33, 1025, 0, 300, 9901521296065376228ull}, {33, 1026, 1, 300, 2518605065658628783ull}, {33, 1027, 2, 300, 2644840139594357452ull}, {33, 1028, 3, 300, 5806420902808959401ull}, {63, 1029, 0, 300, 9559717117025771806ull}, {63, 1030, 1, 300, 1024629904681651609ull}, {63, 1031, 2, 300, 3926011301112169669ull}, {63, 1032, 3, 300, 9341233474012580367ull}, {64, 1033, 0, 300, 13295873210192172095ull}, {64, 1034, 1, 300, 9113284969450766633ull}, {64, 1035, 2, 300, 16434691742495849850ull}, {64, 1036, 3, 300, 7384693789818717293ull}, {65, 1037, 0, 300, 18026044997035542584ull}, {65, 1038, 1, 300, 5452694499953959301ull}, {65, 1039, 2, 300, 17842403146996837177ull}, {65, 1040, 3, 300, 11708948070358431554ull}, {100, 1041, 0, 300, 5948440641464788105ull}, {100, 1042, 1, 300, 7258571232953322276ull}, {100, 1043, 2, 300, 875746559628940751ull}, {100, 1044, 3, 300, 15796093205732023244ull}, {127, 1045, 0, 300, 2098365707913003906ull}, {127, 1046, 1, 300, 10415326935816183169ull}, {127, 1047, 2, 300, 9824775011843457669ull}, {127, 1048, 3, 300, 14815288979939094079ull}, {128, 1049, 0, 300, 10691255785190316715ull}, {128, 1050, 1, 300, 17364834024329134680ull}, {128, 1051, 2, 300, 580634143036049432ull}, {128, 1052, 3, 300, 11614090824325954519ull}, {129, 1053, 0, 300, 18050122208072759888ull}, {129, 1054, 1, 300, 1035211368287309780ull}, {129, 1055, 2, 300, 13066944922983881937ull}, {129, 1056, 3, 300, 10604257969689628829ull}, {191, 1057, 0, 300, 10977620980836544682ull}, {191, 1058, 1, 300, 5436231459184523429ull}, {191, 1059, 2, 300, 3815386051417013035ull}, {191, 1060, 3, 300, 15996496666741000265ull}, {192, 1061, 0, 300, 945865443411886890ull}, {192, 1062, 1, 300, 9092234323592626827ull}, {192, 1063, 2, 300, 5649548052146900516ull}, {192, 1064, 3, 300, 12458736589744298098ull}, {193, 1065, 0, 300, 10844692630638373540ull}, {193, 1066, 1, 300, 26386585339408943ull}, {193, 1067, 2, 300, 6199694479975308314ull}, {193, 1068, 3, 300, 3663147967066416988ull}, {255, 1069, 0, 300, 14852288657850444717ull}, {255, 1070, 1, 300, 50174488273470613ull}, {255, 1071, 2, 300, 6602537122238339257ull}, {255, 1072, 3, 300, 12176683410487037143ull}, {256, 1073, 0, 300, 16654940948695215440ull}, {256, 1074, 1, 300, 1617341596262151934ull}, {256, 1075, 2, 300, 370737796351378700ull}, {256, 1076, 3, 300, 13991284524308674851ull}, {257, 1077, 0, 300, 14328210259244177755ull}, {257, 1078, 1, 300, 8144047258085723758ull}, {257, 1079, 2, 300, 6324498528199220ull}, {257, 1080, 3, 300, 5389855319726021294ull}, {1000, 1081, 0, 300, 5192220701263225473ull}, {1000, 1082, 1, 300, 14093090904543933296ull}, {1000, 1083, 2, 300, 2155562170736710832ull}, {1000, 1084, 3, 300, 5298393288268542048ull}, {4095, 1085, 0, 300, 16922581934011917300ull}, {4095, 1086, 1, 300, 6241722463847110096ull}, {4095, 1087, 2, 300, 3035049584754264612ull}, {4095, 1088, 3, 300, 6912401213087037668ull}, {4096, 1089, 0, 300, 3622732453508055929ull}, {4096, 1090, 1, 300, 16072625854729240996ull}, {4096, 1091, 2, 300, 16682513358105604161ull}, {4096, 1092, 3, 300, 11730382200033629861ull}, {4097, 1093, 0, 300, 5809877727668248775ull}, {4097, 1094, 1, 300, 14117030018663623895ull}, {4097, 1095, 2, 300, 4529281566232007838ull}, {4097, 1096, 3, 300, 17813267019161169088ull}, {20000, 1097, 0, 300, 6458267798097965981ull}, {20000, 1098, 1, 300, 1039502622016512959ull}, {20000, 1099, 2, 300, 7707562318913685696ull}, {20000, 1100, 3, 300, 4476203502218219582ull}};

std::vector<u64> init_words(u32 nbits, u64 seed, int mode) {
  Lcg r(seed);
  std::vector<u64> w(words_for(nbits));
  for (u64& x : w) {
    if (mode == 0) x = r.next64();
    else if (mode == 1) x = 0;
    else if (mode == 2) x = ~0ull;
    else {
      const u64 a = r.next64();
      const u64 b = r.next64();
      x = a & b & r.next64();
    }
  }
  return w;
}

bool bit_of(const std::vector<u64>& w, u32 i) { return ((w[i >> 6] >> (i & 63u)) & 1u) != 0; }

}  // namespace

TEST_CASE("bitset: scenarios match the independent reference") {
  for (const ScCase& c : kScCases) {
    std::vector<u64> w = init_words(c.nbits, c.seed, c.mode);
    u64* p = w.empty() ? nullptr : w.data();
    Lcg r(c.seed + 1);
    Fnv f;
    for (u32 idx = 0; idx < c.nops; ++idx) {
      const u32 kind = r.rng(0, 19);
      if (kind <= 6) {
        const u32 i = r.rng(0, c.nbits + 2);
        const bool v = r.rng(0, 1) != 0;
        const bool ok = set(p, c.nbits, i, v);
        f.word(0);
        f.word(ok ? 1u : 0u);
        f.word(kSent);
      } else if (kind <= 9) {
        const u32 i = r.rng(0, c.nbits + 2);
        bool b = (idx & 1u) != 0;
        const bool ok = test(p, c.nbits, i, &b);
        f.word(1);
        f.word(ok ? 1u : 0u);
        f.word(b ? 1u : 0u);
      } else if (kind == 10) {
        const bool v = r.rng(0, 1) != 0;
        const bool ok = fill(p, c.nbits, v);
        f.word(2);
        f.word(ok ? 1u : 0u);
        f.word(kSent);
      } else if (kind <= 12) {
        u32 out = kSent;
        const bool ok = popcount(p, c.nbits, &out);
        f.word(3);
        f.word(ok ? 1u : 0u);
        f.word(out);
      } else {
        const u32 from = r.rng(0, c.nbits + 2);
        const bool v = r.rng(0, 1) != 0;
        u32 out = kSent;
        const bool ok = find_next(p, c.nbits, from, v, &out);
        f.word(4);
        f.word(ok ? 1u : 0u);
        f.word(out);
      }
    }
    for (u64 x : w) {
      f.word(static_cast<u32>(x));
      f.word(static_cast<u32>(x >> 32));
    }
    CHECK(f.h == c.digest);
  }
}

TEST_CASE("bitset: words_for") {
  CHECK(words_for(0) == 0u);
  CHECK(words_for(1) == 1u);
  CHECK(words_for(63) == 1u);
  CHECK(words_for(64) == 1u);
  CHECK(words_for(65) == 2u);
  CHECK(words_for(127) == 2u);
  CHECK(words_for(128) == 2u);
  CHECK(words_for(129) == 3u);
  CHECK(words_for(kMaxBits - 1) == (1u << 24));
  CHECK(words_for(kMaxBits) == (1u << 24));
  CHECK(words_for(kMaxBits + 1) == 0u);
  CHECK(words_for(0xFFFFFFFFu) == 0u);
  for (u32 n = 0; n < 1000; ++n) CHECK(words_for(n) == (n + 63) / 64);
}

TEST_CASE("bitset: invalid arguments change nothing") {
  u64 w[2] = {0x0123456789ABCDEFull, 0xFEDCBA9876543210ull};
  const u64 keep[2] = {w[0], w[1]};
  auto same = [&]() {
    CHECK(w[0] == keep[0]);
    CHECK(w[1] == keep[1]);
  };
  u32 n = kSent;
  bool b = true;
  // nbits above the limit, small dummy buffer, returns before reading
  CHECK_FALSE(set(w, kMaxBits + 1, 0, true));
  CHECK_FALSE(set(w, 0xFFFFFFFFu, 5, false));
  CHECK_FALSE(test(w, kMaxBits + 1, 0, &b));
  CHECK_FALSE(fill(w, kMaxBits + 1, true));
  CHECK_FALSE(fill(w, 0xFFFFFFFFu, false));
  CHECK_FALSE(popcount(w, kMaxBits + 1, &n));
  CHECK_FALSE(find_next(w, kMaxBits + 1, 0, true, &n));
  CHECK_FALSE(find_next(w, 0xFFFFFFFFu, 0xFFFFFFFFu, false, &n));
  CHECK(n == kSent);
  CHECK(b);
  same();
  // null words with nbits above 0
  CHECK_FALSE(set(nullptr, 1, 0, true));
  CHECK_FALSE(test(nullptr, 1, 0, &b));
  CHECK_FALSE(fill(nullptr, 1, true));
  CHECK_FALSE(fill(nullptr, 64, false));
  CHECK_FALSE(popcount(nullptr, 1, &n));
  CHECK_FALSE(find_next(nullptr, 1, 0, true, &n));
  CHECK_FALSE(find_next(nullptr, 1, 1, true, &n));
  CHECK(n == kSent);
  // null out
  CHECK_FALSE(test(w, 128, 0, nullptr));
  CHECK_FALSE(popcount(w, 128, nullptr));
  CHECK_FALSE(find_next(w, 128, 0, true, nullptr));
  CHECK_FALSE(find_next(w, 128, 128, true, nullptr));
  // index at or above nbits
  CHECK_FALSE(set(w, 100, 100, true));
  CHECK_FALSE(set(w, 100, 127, true));
  CHECK_FALSE(set(w, 100, 0xFFFFFFFFu, false));
  CHECK_FALSE(test(w, 100, 100, &b));
  CHECK_FALSE(test(w, 100, 0xFFFFFFFFu, &b));
  CHECK(b);
  CHECK_FALSE(find_next(w, 100, 101, true, &n));
  CHECK_FALSE(find_next(w, 100, 0xFFFFFFFFu, false, &n));
  CHECK(n == kSent);
  same();
}

TEST_CASE("bitset: nbits 0") {
  u32 n = kSent;
  bool b = false;
  CHECK_FALSE(set(nullptr, 0, 0, true));
  CHECK_FALSE(test(nullptr, 0, 0, &b));
  CHECK(fill(nullptr, 0, true));
  CHECK(fill(nullptr, 0, false));
  CHECK(popcount(nullptr, 0, &n));
  CHECK(n == 0u);
  n = kSent;
  CHECK(find_next(nullptr, 0, 0, true, &n));
  CHECK(n == 0u);
  n = kSent;
  CHECK(find_next(nullptr, 0, 0, false, &n));
  CHECK(n == 0u);
  CHECK_FALSE(find_next(nullptr, 0, 1, false, &n));
  u64 w = 0x5555ull;
  CHECK(fill(&w, 0, true));
  CHECK(w == 0x5555ull);
  CHECK(popcount(&w, 0, &n));
  CHECK(n == 0u);
}

TEST_CASE("bitset: padding is never read into a result and never changed") {
  for (u32 nbits = 1; nbits <= 200; ++nbits) {
    const u32 nw = words_for(nbits);
    const u32 rem = nbits & 63u;
    const u64 padmask = rem == 0 ? 0ull : (~0ull << rem);
    for (int padbit = 0; padbit < 2; ++padbit) {
      Lcg r(nbits * 2u + static_cast<u32>(padbit));
      std::vector<u64> w(nw);
      for (u64& x : w) x = r.next64();
      w[nw - 1] = (w[nw - 1] & ~padmask) | (padbit ? padmask : 0ull);
      // an all-zero data region with padding ones: popcount 0, find_next(true) finds nothing, find_next(false) finds 0
      std::vector<u64> z(nw, 0);
      z[nw - 1] = padbit ? padmask : 0ull;
      u32 n = kSent;
      REQUIRE(popcount(z.data(), nbits, &n));
      REQUIRE(n == 0u);
      REQUIRE(find_next(z.data(), nbits, 0, true, &n));
      REQUIRE(n == nbits);
      // an all-one data region with padding zeros: find_next(false) finds nothing
      std::vector<u64> o(nw, ~0ull);
      o[nw - 1] = (~0ull & ~padmask) | (padbit ? padmask : 0ull);
      REQUIRE(popcount(o.data(), nbits, &n));
      REQUIRE(n == nbits);
      REQUIRE(find_next(o.data(), nbits, 0, false, &n));
      REQUIRE(n == nbits);
      // set and fill keep the padding bits they had
      const u64 padkeep = w[nw - 1] & padmask;
      REQUIRE(fill(w.data(), nbits, true));
      REQUIRE((w[nw - 1] & padmask) == padkeep);
      REQUIRE(popcount(w.data(), nbits, &n));
      REQUIRE(n == nbits);
      REQUIRE(fill(w.data(), nbits, false));
      REQUIRE((w[nw - 1] & padmask) == padkeep);
      REQUIRE(popcount(w.data(), nbits, &n));
      REQUIRE(n == 0u);
      REQUIRE(set(w.data(), nbits, nbits - 1, true));
      REQUIRE(set(w.data(), nbits, nbits - 1, false));
      REQUIRE((w[nw - 1] & padmask) == padkeep);
      for (u32 k = 0; k + 1 < nw; ++k) REQUIRE(w[k] == 0ull);
    }
  }
}

TEST_CASE("bitset: set changes exactly one bit") {
  for (u32 nbits : {1u, 63u, 64u, 65u, 130u}) {
    const u32 nw = words_for(nbits);
    for (u32 i = 0; i < nbits; ++i) {
      for (int base = 0; base < 2; ++base) {
        for (int v = 0; v < 2; ++v) {
          std::vector<u64> w(nw, base ? ~0ull : 0ull);
          const std::vector<u64> before = w;
          REQUIRE(set(w.data(), nbits, i, v != 0));
          for (u32 j = 0; j < nw * 64; ++j) {
            const bool expect = j == i ? v != 0 : bit_of(before, j);
            REQUIRE(bit_of(w, j) == expect);
          }
          bool b = !v;
          REQUIRE(test(w.data(), nbits, i, &b));
          REQUIRE(b == (v != 0));
        }
      }
    }
  }
}

TEST_CASE("bitset: find_next against a bit-by-bit oracle") {
  Lcg r(0xB175);
  for (u32 nbits = 0; nbits <= 200; ++nbits) {
    const u32 nw = words_for(nbits);
    for (int mode = 0; mode < 4; ++mode) {
      std::vector<u64> w(nw);
      for (u64& x : w) {
        const u64 a = r.next64();
        const u64 b = r.next64();
        x = mode == 0 ? a : mode == 1 ? (a & b & r.next64()) : mode == 2 ? (a | b | r.next64()) : 0ull;
      }
      if (mode == 3 && nbits > 0) w[(nbits - 1) >> 6] |= 1ull << ((nbits - 1) & 63u);  // a single last bit
      u64* p = w.empty() ? nullptr : w.data();
      for (u32 from = 0; from <= nbits; ++from) {
        for (int v = 0; v < 2; ++v) {
          u32 want = nbits;
          for (u32 j = from; j < nbits; ++j) {
            if (bit_of(w, j) == (v != 0)) {
              want = j;
              break;
            }
          }
          u32 got = kSent;
          REQUIRE(find_next(p, nbits, from, v != 0, &got));
          REQUIRE(got == want);
          // content below the word of from does not matter
          std::vector<u64> w2 = w;
          for (u32 k = 0; k < (from >> 6); ++k) w2[k] = ~w2[k];
          u32 got2 = kSent;
          REQUIRE(find_next(w2.empty() ? nullptr : w2.data(), nbits, from, v != 0, &got2));
          REQUIRE(got2 == want);
        }
      }
      u32 pc = kSent;
      u32 want_pc = 0;
      for (u32 j = 0; j < nbits; ++j) want_pc += bit_of(w, j) ? 1u : 0u;
      REQUIRE(popcount(p, nbits, &pc));
      REQUIRE(pc == want_pc);
    }
  }
}

TEST_CASE("bitset: fill sets exactly the bits below nbits") {
  for (u32 nbits = 1; nbits <= 200; ++nbits) {
    const u32 nw = words_for(nbits);
    for (int v = 0; v < 2; ++v) {
      std::vector<u64> w(nw, v ? 0ull : ~0ull);
      REQUIRE(fill(w.data(), nbits, v != 0));
      for (u32 j = 0; j < nw * 64; ++j) {
        const bool expect = j < nbits ? v != 0 : v == 0;  // padding keeps its starting value
        REQUIRE(bit_of(w, j) == expect);
      }
    }
  }
}
