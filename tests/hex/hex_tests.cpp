// Tests for core/hex (scope AL). Expected values come from an independent Python reference (tools/hex_ref.py: nibble tables, with binascii only as
// a second check, and its own FNV) and are pasted below: seeded encode
// cases (the input generator is mirrored here) and decode strings embedded as arrays, each checked by a digest of the status, the length
// and the bytes for several destination capacities. RFC 4648 vectors, the check orders, every byte value at every position, the no-write
// rules and the size limits sit beside them. Every destination is an exact-size heap vector, so ASan sees any write past it; a failure
// must leave the sentinel bytes and out_len as they were. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "hex.h"

using namespace lumora::hex;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u8 = std::uint8_t;

constexpr u32 kSent = 0xDEADBEEFu;
constexpr u8 kFill = 0xA5;

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
  u32 rng(u32 lo, u32 hi) { return static_cast<u32>(lo + next() % (static_cast<u64>(hi) - lo + 1)); }
};

struct EncCase {
  int mode;
  u32 n;
  u64 seed, digest;
  u32 need;
  bool upper;
};

struct DecCase {
  const char* data;
  u32 n;
  u32 len;  // 0xFFFFFFFF when the string is rejected
  u64 digest;
};

const EncCase kEncCases[] = {{0, 0, 100, 4672095441862941765ull, 0u, false}, {0, 0, 100, 4672095441862941765ull, 0u, true}, {0, 1, 101, 7301904995717529632ull, 2u, false}, {0, 1, 101, 16878313872277637216ull, 2u, true}, {0, 2, 102, 5506110393027903040ull, 4u, false}, {0, 2, 102, 5936236515596179840ull, 4u, true}, {0, 3, 103, 13057985789804883808ull, 6u, false}, {0, 3, 103, 7115830028703714144ull, 6u, true}, {0, 4, 104, 13580816087081565696ull, 8u, false}, {0, 4, 104, 9507416264712556544ull, 8u, true}, {0, 5, 105, 11057542859239672544ull, 10u, false}, {0, 5, 105, 13684218828852822816ull, 10u, true}, {0, 6, 106, 17206230411191054880ull, 12u, false}, {0, 6, 106, 17019959863740966560ull, 12u, true}, {0, 7, 107, 3361713494883177888ull, 14u, false}, {0, 7, 107, 3270651035069449184ull, 14u, true}, {0, 8, 108, 14471850485349607584ull, 16u, false}, {0, 8, 108, 3277109815837124576ull, 16u, true}, {0, 9, 109, 15587381012703845120ull, 18u, false}, {0, 9, 109, 14004780890852171904ull, 18u, true}, {0, 10, 110, 15886308280091206528ull, 20u, false}, {0, 10, 110, 971305549691721088ull, 20u, true}, {0, 11, 111, 5065633919981725088ull, 22u, false}, {0, 11, 111, 14552606380599962912ull, 22u, true}, {0, 12, 112, 3337199143976048768ull, 24u, false}, {0, 12, 112, 10519731481048923008ull, 24u, true}, {1, 5, 1, 9178115812378751712ull, 10u, false}, {2, 7, 2, 7791451735112153632ull, 14u, true}, {3, 256, 3, 6067340568400475872ull, 512u, false}, {3, 256, 3, 14924939064447251040ull, 512u, true}, {0, 255, 4, 6670688580666611132ull, 510u, false}, {0, 257, 5, 6386512899835961732ull, 514u, true}, {1, 3000, 6, 12534534064463484640ull, 6000u, false}, {2, 3001, 7, 8483098929142320052ull, 6002u, true}, {3, 4097, 8, 11117478112984178976ull, 8194u, false}, {0, 10000, 9, 9482103348978783252ull, 20000u, true}, {0, 65536, 10, 10075794576157997856ull, 131072u, false}};
const char kDec0[] = {0};
const char kDec1[] = {0};
const char kDec2[] = {49, 48};
const char kDec3[] = {67, 50};
const char kDec4[] = {51, 100, 101, 54};
const char kDec5[] = {69, 70, 50, 70};
const char kDec6[] = {54, 97, 56, 56, 55, 52};
const char kDec7[] = {49, 67, 68, 49, 69, 65};
const char kDec8[] = {57, 55, 50, 97, 101, 98, 52, 50};
const char kDec9[] = {52, 57, 55, 51, 54, 49, 50, 55};
const char kDec10[] = {99, 53, 99, 99, 54, 50, 50, 53, 98, 49};
const char kDec11[] = {55, 55, 49, 53, 68, 56, 48, 65, 70, 66};
const char kDec12[] = {102, 50, 54, 101, 100, 57, 48, 57, 56, 98, 101, 52};
const char kDec13[] = {65, 52, 66, 55, 52, 70, 69, 69, 68, 53, 65, 66};
const char kDec14[] = {49, 102, 49, 48, 52, 102, 101, 99, 54, 54, 57, 52, 54, 100};
const char kDec15[] = {68, 49, 53, 57, 67, 54, 68, 50, 65, 70, 53, 66, 51, 67};
const char kDec16[] = {52, 100, 98, 50, 99, 54, 100, 48, 52, 48, 52, 52, 51, 50, 101, 98};
const char kDec17[] = {70, 70, 70, 66, 51, 68, 66, 53, 56, 65, 48, 66, 48, 48, 49, 68};
const char kDec18[] = {55, 97, 53, 52, 51, 100, 98, 52, 49, 97, 102, 52, 102, 54, 52, 98, 100, 53};
const char kDec19[] = {50, 67, 57, 68, 66, 52, 57, 57, 54, 52, 66, 66, 67, 53, 55, 68, 53, 52};
const char kDec20[] = {97, 66};
const char kDec21[] = {65, 98, 57, 70};
const char kDec22[] = {48, 48, 102, 70, 102, 70, 48, 48};
const char kDec23[] = {48, 48, 48, 49, 48, 50, 48, 51, 48, 52, 48, 53, 48, 54, 48, 55, 48, 56, 48, 57, 48, 97, 48, 98, 48, 99, 48, 100, 48, 101, 48, 102, 49, 48, 49, 49, 49, 50, 49, 51, 49, 52, 49, 53, 49, 54, 49, 55, 49, 56, 49, 57, 49, 97, 49, 98, 49, 99, 49, 100, 49, 101, 49, 102, 50, 48, 50, 49, 50, 50, 50, 51, 50, 52, 50, 53, 50, 54, 50, 55, 50, 56, 50, 57, 50, 97, 50, 98, 50, 99, 50, 100, 50, 101, 50, 102, 51, 48, 51, 49, 51, 50, 51, 51, 51, 52, 51, 53, 51, 54, 51, 55, 51, 56, 51, 57, 51, 97, 51, 98, 51, 99, 51, 100, 51, 101, 51, 102, 52, 48, 52, 49, 52, 50, 52, 51, 52, 52, 52, 53, 52, 54, 52, 55, 52, 56, 52, 57, 52, 97, 52, 98, 52, 99, 52, 100, 52, 101, 52, 102, 53, 48, 53, 49, 53, 50, 53, 51, 53, 52, 53, 53, 53, 54, 53, 55, 53, 56, 53, 57, 53, 97, 53, 98, 53, 99, 53, 100, 53, 101, 53, 102, 54, 48, 54, 49, 54, 50, 54, 51, 54, 52, 54, 53, 54, 54, 54, 55, 54, 56, 54, 57, 54, 97, 54, 98, 54, 99, 54, 100, 54, 101, 54, 102, 55, 48, 55, 49, 55, 50, 55, 51, 55, 52, 55, 53, 55, 54, 55, 55, 55, 56, 55, 57, 55, 97, 55, 98, 55, 99, 55, 100, 55, 101, 55, 102, 56, 48, 56, 49, 56, 50, 56, 51, 56, 52, 56, 53, 56, 54, 56, 55, 56, 56, 56, 57, 56, 97, 56, 98, 56, 99, 56, 100, 56, 101, 56, 102, 57, 48, 57, 49, 57, 50, 57, 51, 57, 52, 57, 53, 57, 54, 57, 55, 57, 56, 57, 57, 57, 97, 57, 98, 57, 99, 57, 100, 57, 101, 57, 102, 97, 48, 97, 49, 97, 50, 97, 51, 97, 52, 97, 53, 97, 54, 97, 55, 97, 56, 97, 57, 97, 97, 97, 98, 97, 99, 97, 100, 97, 101, 97, 102, 98, 48, 98, 49, 98, 50, 98, 51, 98, 52, 98, 53, 98, 54, 98, 55, 98, 56, 98, 57, 98, 97, 98, 98, 98, 99, 98, 100, 98, 101, 98, 102, 99, 48, 99, 49, 99, 50, 99, 51, 99, 52, 99, 53, 99, 54, 99, 55, 99, 56, 99, 57, 99, 97, 99, 98, 99, 99, 99, 100, 99, 101, 99, 102, 100, 48, 100, 49, 100, 50, 100, 51, 100, 52, 100, 53, 100, 54, 100, 55, 100, 56, 100, 57, 100, 97, 100, 98, 100, 99, 100, 100, 100, 101, 100, 102, 101, 48, 101, 49, 101, 50, 101, 51, 101, 52, 101, 53, 101, 54, 101, 55, 101, 56, 101, 57, 101, 97, 101, 98, 101, 99, 101, 100, 101, 101, 101, 102, 102, 48, 102, 49, 102, 50, 102, 51, 102, 52, 102, 53, 102, 54, 102, 55, 102, 56, 102, 57, 102, 97, 102, 98, 102, 99, 102, 100, 102, 101, 102, 102, 48, 48, 48, 49, 48, 50, 48, 51, 48, 52, 48, 53, 48, 54, 48, 55, 48, 56, 48, 57, 48, 97, 48, 98, 48, 99, 48, 100, 48, 101, 48, 102, 49, 48, 49, 49, 49, 50, 49, 51, 49, 52, 49, 53, 49, 54, 49, 55, 49, 56, 49, 57, 49, 97, 49, 98, 49, 99, 49, 100, 49, 101, 49, 102, 50, 48, 50, 49, 50, 50, 50, 51, 50, 52, 50, 53, 50, 54, 50, 55, 50, 56, 50, 57, 50, 97, 50, 98};
const char kDec24[] = {68, 57, 53, 53, 49, 48, 66, 48, 56, 70, 57, 54, 70, 57, 52, 67, 69, 51, 66, 52, 52, 69, 50, 53, 69, 53, 48, 53, 56, 48, 70, 53, 68, 53, 53, 66, 49, 67, 56, 69, 53, 48, 67, 67, 68, 57, 66, 53, 68, 65, 52, 67, 65, 70, 53, 52, 49, 70, 56, 54, 65, 67, 65, 56, 66, 66, 52, 67, 70, 55, 70, 50, 48, 51, 55, 57, 49, 49, 65, 56, 55, 70, 53, 57, 52, 54, 51, 70, 66, 57, 67, 56, 54, 49, 51, 69, 49, 53, 55, 68, 57, 55, 65, 51, 55, 68, 70, 56, 53, 48, 51, 50, 65, 53, 66, 54, 53, 55, 48, 52, 48, 52, 49, 67, 52, 53, 50, 66, 52, 65, 69, 50, 67, 54, 53, 57, 69, 49, 50, 51, 52, 55, 69, 56, 53, 52, 67, 66, 54, 53, 50, 48, 57, 67, 49, 67, 69, 65, 70, 49, 54, 53, 67, 50, 70, 68, 51, 52, 67, 70, 55, 68, 49, 52, 68, 66, 69, 68, 55, 68, 51, 65, 66, 51, 49, 69, 65, 54, 56, 66, 55, 67, 69, 53, 49, 53, 69, 50, 48, 48, 68, 65, 52, 55, 57, 52, 57, 65, 68, 56, 53, 48, 49, 68, 48, 55, 57, 70, 53, 49, 50, 67, 48, 68, 56, 56, 65, 55, 54, 67, 52, 69, 52, 52, 57, 54, 57, 49, 52, 54, 66, 68, 66, 51, 50, 53, 55, 68, 69, 55, 53, 54, 50, 52, 66, 67, 50, 53, 68, 68, 65, 65, 54, 53, 70, 55, 52, 66, 69, 51, 66, 68, 51, 53, 56, 69, 65, 70, 66, 68, 55, 49, 69, 69, 48, 51, 56, 67, 56, 49, 51, 65, 50, 70, 48, 48, 67, 65, 49, 52, 54, 52, 68, 68, 48, 52, 70, 57, 48, 49, 52, 54, 50, 68, 49, 70, 69, 51, 49, 52, 50, 70, 57, 52, 49, 69, 69, 54, 48, 54, 52, 70, 68, 52, 68, 51, 68, 68, 50, 70, 48, 55, 52, 68, 48, 66, 48, 50, 49, 56, 66, 68, 54, 67, 70, 70, 69, 49, 53, 51, 51, 48, 69, 65, 57, 69, 56, 50, 57, 70, 65, 69, 51, 56, 70, 49, 52, 52, 55, 68, 52, 56, 57, 48, 48, 56, 54, 53, 55, 67, 51, 54, 49, 53, 51, 55, 55, 67, 48, 48, 49, 70, 57, 48, 65, 51, 67, 50, 54, 56, 54, 66, 69, 50, 57, 68, 52, 66, 68, 68, 56, 68, 52, 51, 49, 66, 66, 50, 48, 57, 50, 49, 55, 49, 49, 53, 49, 53, 57, 69, 51, 55, 52, 50, 48, 50, 70, 53, 69, 52, 66, 48, 69, 53, 68, 56, 68, 65, 66, 57, 50, 65, 50, 49, 66, 55, 54, 53, 55, 50, 68, 69, 51, 50, 50, 49, 65, 65, 51, 52, 68, 56, 48, 67, 68, 48, 66, 57, 70, 50, 51, 68, 54, 56, 53, 48, 55, 50, 56, 49, 50, 53, 68, 70, 70, 67, 53, 70, 55, 50, 65, 49, 55, 56, 55, 57, 70, 67, 57, 67, 50, 66, 54, 50, 50, 52, 69, 56, 53, 68, 54, 65, 66, 66, 68, 52, 55, 54, 56, 48, 56, 65, 69, 57, 53, 57, 70, 53, 70, 69, 56, 65, 68, 54, 49, 56, 66, 56, 69, 52, 67, 50, 55, 69, 57, 68, 70, 54, 65, 53, 65, 68, 50, 68, 54, 48, 53, 48, 67, 69, 56, 66, 49, 55, 55, 48, 65, 52, 55, 48, 70, 54, 53, 67, 48, 54, 70, 55, 48, 50, 53, 56, 57, 49, 55, 49, 69, 51, 55, 50, 50, 55, 52, 49, 54, 65, 56, 70, 49, 66, 55, 52, 56, 69, 66, 65, 70, 67, 66, 55, 48, 69, 57, 50, 66, 56, 55, 69, 52, 56, 49, 67, 51, 55, 50, 53, 49, 67, 49, 48, 68, 66, 56, 69, 70, 51, 50, 50, 70, 67, 66, 57, 65, 69, 67, 48, 53, 53, 50, 54, 54, 48, 70, 54, 51, 57, 66, 66, 69, 57, 56, 53, 69, 68, 68, 55, 55, 53, 48, 48, 55, 56, 49, 54, 55, 51, 65, 51, 56, 56, 69, 54, 57, 51, 48, 54, 49, 54, 67, 54, 51, 55, 69, 66, 53, 70, 52, 50, 65, 67, 68, 49, 48, 70, 57, 49, 66, 65, 49, 49, 48, 70, 52, 55, 48, 66, 66, 57, 66, 57, 65, 66, 54, 69, 65, 56, 57, 52, 49, 65, 68, 48, 57, 55, 50, 55, 50, 67, 67, 69, 54, 65, 54, 69, 65, 50, 55, 56, 48, 48, 49, 67, 55, 54, 50, 57, 67, 52, 56, 53, 53, 48, 50, 54, 68, 49, 65, 70, 69, 54, 54, 52, 68, 65, 56, 68, 52, 68, 53, 70, 54, 50, 49, 49, 56, 57, 51, 49, 54, 67, 70, 57, 66, 68, 55, 67, 70, 50, 49, 70, 57, 53, 54, 49, 53, 67, 67, 56, 50, 56, 48, 56, 49, 66, 65, 67, 69, 57, 53, 68, 66, 56, 65, 70, 52, 48, 66, 70, 54, 51, 65, 51, 69, 51, 70, 69, 56, 52, 55, 51, 56, 70, 70, 57, 51, 68, 55, 55, 51, 48, 51, 50, 66, 52, 55, 57, 67, 51, 57, 69, 48, 53, 56, 50, 55, 67, 51, 66, 48, 54, 68, 50, 65, 69, 48, 52, 68, 52, 52, 55, 50, 54, 54, 56, 65, 69, 53, 51, 50, 49, 52, 54, 51, 53, 66, 50, 53, 48, 65, 54, 48, 57, 56, 53, 52, 48, 70, 52, 48, 70, 68, 50, 70, 65, 51, 55, 67, 55, 54, 56, 70, 54, 51, 52, 53, 65, 65, 54, 48, 49, 55, 55, 51, 66, 54, 66, 65, 56, 66, 65, 54, 66, 70, 53, 50, 52, 55, 70, 67, 65, 48, 57, 52, 66, 49, 52, 69, 70, 52, 68, 55, 56, 48, 50, 65, 53, 70, 49, 55, 54, 52, 53, 69, 52, 65, 56, 69, 53, 67, 50, 52, 50, 65, 49, 50, 65, 66, 55, 49, 68, 52, 51, 57, 52, 55, 54, 52, 57, 69, 49, 52, 67, 68, 67, 50, 69, 66, 49, 54, 70, 70, 50, 68, 69, 57, 48, 49, 68, 50, 52, 67, 57, 51, 66, 48, 65, 70, 65, 56, 52, 57, 56, 50, 57, 49, 53, 69, 53, 48, 51, 66, 49, 49, 48, 48, 65, 48, 65, 55, 69, 65, 70, 51, 54, 57, 56, 53, 68, 49, 53, 50, 66, 66, 66, 49, 53, 53, 69, 68, 51, 68, 51, 67, 55, 65, 53, 68, 67, 68, 65, 67, 53, 51, 49, 49, 67, 51, 70, 70, 69, 55, 49, 55, 57, 53, 56, 51, 54, 49, 56, 50, 56, 54, 49, 48, 57, 48, 67, 51, 66, 50, 52, 57, 69, 52, 70, 53, 51, 57, 56, 57, 55, 69, 67, 66, 56, 56, 50, 52, 50, 52, 67, 67, 53, 55, 55, 67, 54, 56, 68, 52, 66, 49, 54, 70, 69, 68, 48, 70, 51, 67, 55, 55, 49, 48, 49, 65, 70, 53, 57, 53, 69, 69, 65, 56, 49, 57, 57, 57, 69, 53, 49, 50, 49, 57, 49, 48, 53, 49, 49, 48, 49, 49, 53, 70, 68, 56, 52, 68, 56, 56, 68, 53, 65, 65, 55, 55, 55, 69, 65, 56, 48, 50, 55, 66, 52, 66, 68, 68, 52, 56, 53, 57, 69, 57, 50, 56, 66, 66, 49, 53, 55, 53, 51, 52, 70, 54, 54, 53, 54, 67, 55, 52, 68, 49, 70, 50, 48, 57, 70, 51, 69, 52, 68, 66, 57, 69, 54, 49, 70, 57, 51, 66, 54, 67, 65, 65, 68, 51, 68, 66, 54, 49, 51, 50, 49, 70, 52, 49, 56, 57, 53, 54, 52, 69, 51, 52, 52, 53, 67, 54, 51, 66, 53, 54, 48, 68, 65, 57, 70, 48, 53, 55, 49, 51, 56, 51, 70, 53, 51, 66, 48, 68, 50, 50, 65, 68, 54, 70, 51, 49, 54, 67, 69, 51, 54, 69, 69, 56, 52, 68, 57, 56, 54, 70, 68, 68, 67, 57, 57, 70, 48, 53, 69, 67, 51, 65, 48, 65, 50, 55, 53, 56, 54, 68, 67, 50, 50, 50, 53, 65, 55, 68, 48, 51, 50, 68, 49, 52, 69, 48, 67, 54, 65, 52, 56, 55, 48, 51, 68, 57, 49, 51, 66, 48, 65, 55, 65, 50, 65, 67, 54, 51, 53, 52, 56, 55, 56, 67, 52, 57, 70, 51, 51, 49, 48, 67, 66, 67, 70, 49, 48, 66, 66, 66, 66, 67, 53, 56, 65, 65, 65, 49, 70, 69, 52, 53, 52, 65, 55, 55, 66, 55, 51, 68, 70, 54, 70, 53, 52, 48, 56, 67, 51, 49, 48, 51, 69, 65, 67, 57, 51, 48, 54, 53, 65, 52, 65, 66, 51, 54, 56, 68, 50, 56, 66, 51, 69, 66, 65, 54, 69, 66, 49, 57, 51, 56, 48, 50, 68, 67, 52, 57, 51, 53, 57, 57, 57, 55, 52, 56, 69, 67, 57, 52, 69, 70, 65, 57, 52, 48, 48, 50, 52, 70, 67, 50, 48, 50, 68, 69, 50, 57, 54, 69, 70, 55, 56, 67, 70, 69, 69, 65, 53, 69, 48, 57, 50, 49, 51, 65, 67, 51, 69, 57, 55, 68, 57, 50, 52, 66, 57, 48, 50, 68, 54, 57, 49, 52, 67, 54, 48, 51, 70, 68, 51, 70, 69, 68, 54, 48, 68, 68, 66, 51, 70, 69, 50, 48, 69, 49, 52, 65, 52, 53, 51, 51, 68, 50, 54, 55, 66, 56, 55, 68, 55, 70, 69, 55, 57, 54, 52, 53, 54, 66, 54, 53, 50, 52, 50, 51, 68, 57, 65, 66, 67, 50, 48, 57, 65, 55, 55, 54, 65, 55, 57, 55, 56, 54, 66, 48, 57, 49, 65, 57, 66, 53, 68, 68, 51, 55, 69, 69, 55, 70, 68, 65, 65, 49, 54, 49, 54, 70, 57, 57, 48, 53, 51, 48, 65, 55, 50, 66, 55, 69, 67, 56, 56, 55, 66, 56, 65, 57, 51, 53, 68, 54, 56, 56, 55, 66, 48, 65, 57, 54, 69, 50, 48, 67, 67, 69, 67, 57, 56, 54, 52, 55, 48, 51, 57, 68, 66, 68, 69, 65, 55, 65, 52, 49, 54, 56, 67, 49, 68, 67, 55, 66, 52, 48, 48, 65, 51, 65, 68, 48, 69, 69, 48, 54, 56, 53, 69, 65, 56, 68, 53, 51, 70, 53, 50, 54, 50, 68, 57, 66, 49, 51, 69, 66, 57, 49, 68, 68, 67, 52, 56, 57, 50, 57, 65, 53, 50, 49, 52, 53, 48, 70, 55, 66, 55, 53, 67, 66, 68, 52, 53, 68, 55, 52, 67, 56, 67, 48, 68, 66, 56, 67, 57, 66, 56, 68, 66, 70, 70, 55, 53, 49, 48, 70, 52, 48, 50, 68, 54, 56, 49, 52, 70, 56, 56, 69, 49, 57, 65, 48, 54, 55, 56, 54, 56, 56, 68, 65, 49, 50, 65, 68, 55, 49, 54, 50, 56, 65, 57, 65, 68, 66, 50, 49, 65, 52, 52, 50, 49, 48, 53, 50, 70, 68, 65, 49, 65, 66, 48, 48, 52, 49, 53, 67, 49, 53, 56, 70, 51, 49, 50, 52, 69, 57, 69, 55, 69, 57, 51, 48, 70, 50, 68, 51, 55, 54, 57, 57, 57, 66, 57, 51, 52, 50, 65, 57, 54, 48, 52, 69, 50, 48, 57, 52, 53, 53, 67, 51, 50, 48, 52, 55, 48, 53, 48, 53, 49, 56, 52, 68, 54, 49, 55, 48, 65, 57, 68, 70, 50, 57, 53, 65, 65, 56, 68, 68, 51, 51, 65, 48, 54, 66, 65, 69, 54, 53, 52, 55, 54, 50, 66, 69, 66, 52, 53, 50, 67, 49, 57, 66, 56, 57, 50, 70, 67, 69, 53, 54, 69, 48, 54, 51, 50, 54, 68, 51, 55, 66, 51, 48, 55, 57, 65, 55, 55, 54, 66, 67, 55, 54, 57, 68, 70, 69, 66, 50, 67, 70, 49, 52, 48, 51, 70, 66, 55};
const char kDec25[] = {48};
const char kDec26[] = {97, 98, 99};
const char kDec27[] = {48, 120, 48, 48};
const char kDec28[] = {48, 88, 49, 102};
const char kDec29[] = {48, 48, 32};
const char kDec30[] = {32, 48, 48};
const char kDec31[] = {48, 32, 48};
const char kDec32[] = {10, 48, 48};
const char kDec33[] = {48, 48, 10};
const char kDec34[] = {48, 103};
const char kDec35[] = {103, 48};
const char kDec36[] = {71, 48};
const char kDec37[] = {48, 71};
const char kDec38[] = {48, 122};
const char kDec39[] = {48, 47};
const char kDec40[] = {48, 58};
const char kDec41[] = {48, 64};
const char kDec42[] = {48, 96};
const char kDec43[] = {48, 70, 48, 102, 48};
const char kDec44[] = {103, 103};
const char kDec45[] = {48, 45};
const char kDec46[] = {48, 95};
const char kDec47[] = {48, 48, 58};
const char kDec48[] = {97, 98, 58, 99, 100};
const char kDec49[] = {97, 98, 32, 99, 100};
const char kDec50[] = {97, 98, 0, 100};
const char kDec51[] = {97, 0};
const char kDec52[] = {0, 97};
const char kDec53[] = {-56, 97};
const char kDec54[] = {97, -56};
const char kDec55[] = {-1, -1};
const char kDec56[] = {97, -128};
const char kDec57[] = {61, 48};
const char kDec58[] = {48, 61};
const char kDec59[] = {48, 48, 48, 48, 48, 48, 48, 103};
const char kDec60[] = {103, 48, 48, 48, 48, 48, 48, 48};
const char kDec61[] = {48, 48, 48, 103, 48, 48, 48, 48};
const char kDec62[] = {48, 48, 48, 48, 48, 48, 48, 48, 104};
const char kDec63[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 71};
const char kDec64[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 48};
const char kDec65[] = {52, 54, 51, 51, 102, 99, 55, 101, 48, 49, 53, 57};
const char kDec66[] = {55, 51, 68, 53, 55, 51, 54, 49, 68, 66, 48, 50, 54, 66};
const char kDec67[] = {52, 49, 55, 55, 101, 97, 52, 53, 98, 53, 98, 50, 51, 51, 49, 52, 100, 101, 55, 53, 101, 55, 55, 51};
const char kDec68[] = {67, 69, 49, 98, 54, 49, 50, 57, 57, 48, 54, 50, 70, 56, 55, 51};
const char kDec69[] = {102, 98, 98, 98, 100, 97, 48, 99, 54, 97, 49, 50, 98, 99};
const char kDec70[] = {50, 50};
const char kDec71[] = {53, 54, -56, 102, 99, 54, 100, 51, 49, 102, 55, 50, 52, 54};
const char kDec72[] = {56, 51, 65, 50, 51, 68, 66, 55};
const char kDec73[] = {98, 49, 52, 52, 98, 52, 57, 97, 100, 52, 100, 103, 99, 102, 52, 102};
const char kDec74[] = {69, 69};
const char kDec75[] = {48, 98, 49, 56, 97, 49, 54, 50, 56, 56, 51, 50};
const char kDec76[] = {51, 56, 50, 65, 49, 56, 52, 53, 54, 51, 0, 50, 49, 68, 54, 68};
const char kDec77[] = {54, 54, 99, 98, 56, 102, 50, 57};
const char kDec78[] = {57, 51, 54, 69, 48, 54, 48, 67, 49, 55, 52, 50, 99, 54, 50, 66, 49, 53, 50, 53, 66, 48};
const char kDec79[] = {99, 48, 49, 48, 55, 100, 102, 48, 102, 50, 102, 53};
const char kDec80[] = {69, 64, 66, 50, 70, 52, 68, 51};
const char kDec81[] = {49, 98, 53, 52, 54, 98, 98, 55, 97, 54, 53, 51, 102, 52, -128, 57, 99, 55};
const char kDec82[] = {53, 56};
const char kDec83[] = {55, 54, 57, 56, 53, 57, 55, 101, 53, 66, 98, 51, 55, 100};
const char kDec84[] = {65, 51, 51, 65, 68, 48, 54, 10, 51, 53, 54, 51, 52, 50};
const char kDec85[] = {100, 53, 100, 99, 52, 55, 52, 53};
const char kDec86[] = {70, 68, 55, 69, 66, 69, 50, 57, 69, 65, 67, 53, 67, 66};
const char kDec87[] = {50, 98, 50, 48, 50, 53, 48, 99};
const char kDec88[] = {53, 56, 67, 52, 65, 67};
const char kDec89[] = {56, 53, 54, 52, 50, 51, 100, 52, 55, 102};
const char kDec90[] = {66, 51, 48, 54, 57, 65, 53, 55, 53, 51, 56, 51, 68, 68, 65, 50};
const char kDec91[] = {52, 48, 97, 56};
const char kDec92[] = {48, 68, 52, 65, 56, 56, 55, 69, 48, 56, 69, 51, 54, -1};
const char kDec93[] = {51, 98, 101, -1};
const char kDec94[] = {54, 56, 66, 69, 55, 53};
const char kDec95[] = {57, 53, 51, 48, 101, 99, 50, 57, 57, 55, 57, 51};
const char kDec96[] = {67, 0, 68, 50, 54, 51, 48, 68, 55, 49, 65, 52, 55, 57, 68, 68, 52, 48};
const char kDec97[] = {100, 48, 55, 52, 100, 97, 102, 48, 52, 99, 53, 52, 51, 101, 51, 100, 100, 49, 99, 97, 51, 57};
const char kDec98[] = {49, 68, 49, 54, 53, 61};
const char kDec99[] = {52, 97, 98, 56, 99, 56, 98, -56, 48, 48, 98, 52, 99, 55, 102, 98, 102, 50, 52, 55};
const char kDec100[] = {55, 56, 53, 65, 51, 70, 57, 66, 68, 66, 54, 52, 56, 67, 53, 65, 56, 51, 56, 98, 52, 49};
const char kDec101[] = {97, 53, 102, 99, 98, 54, 55, 101, 98, 53, 0, 52, 53, 48, 98, 97, 49, 52, 99, 52, 57, 57};
const char kDec102[] = {68, 98, 57, 69, 50, 68};
const char kDec103[] = {64, 102};
const char kDec104[] = {50, 68, 69, 50, 49, 51, 50, 57, 52, 52, 50, 52, 57, 69, 68, 55, 67, 53, 56, 48, 65, 49};
const char kDec105[] = {53, 65, 56, 52};
const char kDec106[] = {56, 55, 67, 54, 48, 57, 70, 48, 70, 57, 56, 52, 50, 55, 57, 54, 69, 55, 70, 68, 53, 49, 51, 66};
const char kDec107[] = {98, 53, 99, 56, 56, 48, 100, 52, 100, 68, 51, 52, 101, 99, 102, 53, 55, 55, 51, 99, 97, 57, 55, 98};
const char kDec108[] = {69, 50, 54, 65, 70, -56, 66, 55};
const char kDec109[] = {48, -1, 48, 99};
const char kDec110[] = {51, 68, 65, 69, 69, 48, 55, 70, 54, 50, 52, 53};
const char kDec111[] = {54, 97, 53, 48, 53, 99, 54, 50, 70, 99, 102, 53, 102, 102, 55, 50, 98, 97};
const char kDec112[] = {57, 55, 70, 50, 68, 50, 52, 54, 49, 55, 65, 53, 67, 51, 68, 49, 68, 66, 55, 53, 54, 50, 66, 67};
const char kDec113[] = {99, 52, 57, 52, 52, 57, 53, 57};
const char kDec114[] = {70, 50, 51, 54, 67, 64, 48, 68};
const char kDec115[] = {49, 102, 100, 56, 51, 55, 102, 48, 97, 54, 98, 53, 49, 49, 101, 102, 102, 99, 51, 48, 97, 97};
const char kDec116[] = {52, 67, 55, 65, 71, 69, 68, 52, 56, 48, 54, 53, 68, 54, 52, 69, 56, 68, 54, 70, 67, 50};
const char kDec117[] = {100, 97};
const char kDec118[] = {58, 55, 66, 69, 57, 67, 57, 66, 51, 53, 67, 53, 53, 70};
const char kDec119[] = {100, 52, 54, 48, 49, 51, 55, 102, 48, 102, 52, 53, 50, 52, 54, 99, 51, 102, 50, 98};
const char kDec120[] = {48, 50, 48, 50, 56, 65, 102, 50, 69, 57, 50, 53, 69, 56};
const char kDec121[] = {50, 102, 52, 52, 48, 49, 52, 54};
const char kDec122[] = {53, 67, 52, 54, 55, 56, 50, 57, 57, 69, 56, 53, 55, 49, 56, 57, 70, 49, 69, 100, 68, 50, 51, 69};
const char kDec123[] = {56, 57, 101, 56, 101, 102, 48, 100, 55, 56, 51, -1, 51, 54};
const char kDec124[] = {66, 55, 50, 65, 54, 54, 70, 49, 53, 51, 69, 53};
const DecCase kDecCases[] = {{kDec0, 0u, 0u, 4524767293552705221ull}, {kDec1, 0u, 0u, 4524767293552705221ull}, {kDec2, 2u, 1u, 7683015633971457516ull}, {kDec3, 2u, 1u, 11445100099366647678ull}, {kDec4, 4u, 2u, 383179561786199584ull}, {kDec5, 4u, 2u, 15591634385678275840ull}, {kDec6, 6u, 3u, 4448030216371800312ull}, {kDec7, 6u, 3u, 9871161530112685337ull}, {kDec8, 8u, 4u, 4609796269223772376ull}, {kDec9, 8u, 4u, 6131876794659557528ull}, {kDec10, 10u, 5u, 4627538877958484343ull}, {kDec11, 10u, 5u, 15799772586978912171ull}, {kDec12, 12u, 6u, 6515550822200808144ull}, {kDec13, 12u, 6u, 9934585259166127239ull}, {kDec14, 14u, 7u, 13152397093481182705ull}, {kDec15, 14u, 7u, 8812388967052586206ull}, {kDec16, 16u, 8u, 3994777786260504617ull}, {kDec17, 16u, 8u, 9370849702791536693ull}, {kDec18, 18u, 9u, 1066199781109046757ull}, {kDec19, 18u, 9u, 2493836215882278339ull}, {kDec20, 2u, 1u, 14153331964878080903ull}, {kDec21, 4u, 2u, 9588122965112602528ull}, {kDec22, 8u, 4u, 6498394134325704216ull}, {kDec23, 600u, 300u, 16905095908573249578ull}, {kDec24, 2000u, 1000u, 15039579908561950988ull}, {kDec25, 1u, 4294967295u, 14222729515248215621ull}, {kDec26, 3u, 4294967295u, 14222729515248215621ull}, {kDec27, 4u, 4294967295u, 14222729515248215621ull}, {kDec28, 4u, 4294967295u, 14222729515248215621ull}, {kDec29, 3u, 4294967295u, 14222729515248215621ull}, {kDec30, 3u, 4294967295u, 14222729515248215621ull}, {kDec31, 3u, 4294967295u, 14222729515248215621ull}, {kDec32, 3u, 4294967295u, 14222729515248215621ull}, {kDec33, 3u, 4294967295u, 14222729515248215621ull}, {kDec34, 2u, 4294967295u, 14222729515248215621ull}, {kDec35, 2u, 4294967295u, 14222729515248215621ull}, {kDec36, 2u, 4294967295u, 14222729515248215621ull}, {kDec37, 2u, 4294967295u, 14222729515248215621ull}, {kDec38, 2u, 4294967295u, 14222729515248215621ull}, {kDec39, 2u, 4294967295u, 14222729515248215621ull}, {kDec40, 2u, 4294967295u, 14222729515248215621ull}, {kDec41, 2u, 4294967295u, 14222729515248215621ull}, {kDec42, 2u, 4294967295u, 14222729515248215621ull}, {kDec43, 5u, 4294967295u, 14222729515248215621ull}, {kDec44, 2u, 4294967295u, 14222729515248215621ull}, {kDec45, 2u, 4294967295u, 14222729515248215621ull}, {kDec46, 2u, 4294967295u, 14222729515248215621ull}, {kDec47, 3u, 4294967295u, 14222729515248215621ull}, {kDec48, 5u, 4294967295u, 14222729515248215621ull}, {kDec49, 5u, 4294967295u, 14222729515248215621ull}, {kDec50, 4u, 4294967295u, 14222729515248215621ull}, {kDec51, 2u, 4294967295u, 14222729515248215621ull}, {kDec52, 2u, 4294967295u, 14222729515248215621ull}, {kDec53, 2u, 4294967295u, 14222729515248215621ull}, {kDec54, 2u, 4294967295u, 14222729515248215621ull}, {kDec55, 2u, 4294967295u, 14222729515248215621ull}, {kDec56, 2u, 4294967295u, 14222729515248215621ull}, {kDec57, 2u, 4294967295u, 14222729515248215621ull}, {kDec58, 2u, 4294967295u, 14222729515248215621ull}, {kDec59, 8u, 4294967295u, 14222729515248215621ull}, {kDec60, 8u, 4294967295u, 14222729515248215621ull}, {kDec61, 8u, 4294967295u, 14222729515248215621ull}, {kDec62, 9u, 4294967295u, 14222729515248215621ull}, {kDec63, 17u, 4294967295u, 14222729515248215621ull}, {kDec64, 18u, 4294967295u, 14222729515248215621ull}, {kDec65, 12u, 6u, 13160504989842046196ull}, {kDec66, 14u, 7u, 424286975680793100ull}, {kDec67, 24u, 12u, 14864509208344660199ull}, {kDec68, 16u, 8u, 16505036345574860217ull}, {kDec69, 14u, 7u, 1544545953965213200ull}, {kDec70, 2u, 1u, 8805014163152529118ull}, {kDec71, 14u, 4294967295u, 14222729515248215621ull}, {kDec72, 8u, 4u, 12476535275680273368ull}, {kDec73, 16u, 4294967295u, 14222729515248215621ull}, {kDec74, 2u, 1u, 3406967472091464186ull}, {kDec75, 12u, 6u, 12136924438187672745ull}, {kDec76, 16u, 4294967295u, 14222729515248215621ull}, {kDec77, 8u, 4u, 15303394201574659032ull}, {kDec78, 22u, 11u, 13446609608420684641ull}, {kDec79, 12u, 6u, 9019689974084275561ull}, {kDec80, 8u, 4294967295u, 14222729515248215621ull}, {kDec81, 18u, 4294967295u, 14222729515248215621ull}, {kDec82, 2u, 1u, 6873044296901794948ull}, {kDec83, 14u, 7u, 2855867653725976390ull}, {kDec84, 14u, 4294967295u, 14222729515248215621ull}, {kDec85, 8u, 4u, 5981513092619856536ull}, {kDec86, 14u, 7u, 4572566323083175906ull}, {kDec87, 8u, 4u, 11092804779628328024ull}, {kDec88, 6u, 3u, 9239641542677260726ull}, {kDec89, 10u, 5u, 633235310813136921ull}, {kDec90, 16u, 8u, 6542145949105422058ull}, {kDec91, 4u, 2u, 13094795582731051552ull}, {kDec92, 14u, 4294967295u, 14222729515248215621ull}, {kDec93, 4u, 4294967295u, 14222729515248215621ull}, {kDec94, 6u, 3u, 7402133985441706661ull}, {kDec95, 12u, 6u, 2599954397964562127ull}, {kDec96, 18u, 4294967295u, 14222729515248215621ull}, {kDec97, 22u, 11u, 15507838426545789305ull}, {kDec98, 6u, 4294967295u, 14222729515248215621ull}, {kDec99, 20u, 4294967295u, 14222729515248215621ull}, {kDec100, 22u, 11u, 6240176817320902664ull}, {kDec101, 22u, 4294967295u, 14222729515248215621ull}, {kDec102, 6u, 3u, 17289181269450102542ull}, {kDec103, 2u, 4294967295u, 14222729515248215621ull}, {kDec104, 22u, 11u, 4578857732834984510ull}, {kDec105, 4u, 2u, 10663652196423164512ull}, {kDec106, 24u, 12u, 13775905551389584709ull}, {kDec107, 24u, 12u, 5901392731054157001ull}, {kDec108, 8u, 4294967295u, 14222729515248215621ull}, {kDec109, 4u, 4294967295u, 14222729515248215621ull}, {kDec110, 12u, 6u, 17459043399224555032ull}, {kDec111, 18u, 9u, 8939474047772626990ull}, {kDec112, 24u, 12u, 1248412682699208360ull}, {kDec113, 8u, 4u, 3631798797364297304ull}, {kDec114, 8u, 4294967295u, 14222729515248215621ull}, {kDec115, 22u, 11u, 5419804049100518813ull}, {kDec116, 22u, 4294967295u, 14222729515248215621ull}, {kDec117, 2u, 1u, 12325467877452809350ull}, {kDec118, 14u, 4294967295u, 14222729515248215621ull}, {kDec119, 20u, 10u, 9220053643299591873ull}, {kDec120, 14u, 7u, 10813527271716266726ull}, {kDec121, 8u, 4u, 3318152073323258968ull}, {kDec122, 24u, 12u, 381585809752210913ull}, {kDec123, 14u, 4294967295u, 14222729515248215621ull}, {kDec124, 12u, 6u, 2695287536018889367ull}};

std::vector<u8> gen(int mode, u32 n, u64 seed) {
  Lcg r(seed);
  std::vector<u8> out;
  for (u32 i = 0; i < n; ++i) {
    if (mode == 0) out.push_back(static_cast<u8>(r.rng(0, 255)));
    else if (mode == 1) out.push_back(0);
    else if (mode == 2) out.push_back(255);
    else out.push_back(static_cast<u8>(i & 255));
  }
  return out;
}

struct EncOut {
  HexResult st;
  u32 len;
  std::vector<u8> dst;  // bytes of the destination, characters or decoded bytes
};

// dst is an exact-size vector of cap bytes filled with a sentinel; a failure must leave it and len alone.
EncOut run_encode(const std::vector<u8>& in, u32 cap, bool upper) {
  EncOut o;
  o.dst.assign(cap, kFill);
  o.len = kSent;
  o.st = encode(in.data(), static_cast<u32>(in.size()), cap ? reinterpret_cast<char*>(o.dst.data()) : nullptr, cap, &o.len, upper);
  return o;
}

EncOut run_decode(const char* src, u32 n, u32 cap) {
  EncOut o;
  o.dst.assign(cap, kFill);
  o.len = kSent;
  o.st = decode(src, n, cap ? o.dst.data() : nullptr, cap, &o.len);
  return o;
}

EncOut run_decode_str(const std::string& s, u32 cap) { return run_decode(s.data(), static_cast<u32>(s.size()), cap); }

// Folds one result like the reference: status, length (the sentinel on failure), then the bytes on success.
void fold(Fnv& f, const EncOut& o) {
  f.word(static_cast<u32>(o.st));
  f.word(o.len);
  if (o.st == HexResult::Ok) {
    for (u32 i = 0; i < o.len; ++i) f.word(o.dst[i]);
    for (std::size_t i = o.len; i < o.dst.size(); ++i) REQUIRE(o.dst[i] == kFill);  // nothing past out_len
  } else {
    for (u8 b : o.dst) REQUIRE(b == kFill);
  }
}

std::string chars_of(const EncOut& o) { return std::string(o.dst.begin(), o.dst.begin() + o.len); }

std::string enc_str(const std::string& raw, bool upper) {
  const std::vector<u8> in(raw.begin(), raw.end());
  const EncOut o = run_encode(in, static_cast<u32>(2 * raw.size()), upper);
  REQUIRE(o.st == HexResult::Ok);
  return chars_of(o);
}

}  // namespace

TEST_CASE("hex: encode cases match the independent reference") {
  for (const EncCase& c : kEncCases) {
    const std::vector<u8> in = gen(c.mode, c.n, c.seed);
    Fnv f;
    const u32 need = c.need;
    const u32 caps[] = {0, 1, need > 0 ? need - 1 : 0, need, need + 3};
    for (u32 cap : caps) {
      const EncOut o = run_encode(in, cap, c.upper);
      fold(f, o);
      CHECK(o.st == (need > cap ? HexResult::BufferTooSmall : HexResult::Ok));
    }
    CHECK(f.h == c.digest);
    const EncOut e = run_encode(in, need, c.upper);
    REQUIRE(e.st == HexResult::Ok);
    const EncOut d = run_decode(reinterpret_cast<const char*>(e.dst.data()), need, c.n);
    REQUIRE(d.st == HexResult::Ok);
    CHECK(d.len == c.n);
    CHECK(d.dst == in);
  }
}

TEST_CASE("hex: decode strings match the independent reference") {
  for (const DecCase& c : kDecCases) {
    Fnv f;
    std::vector<u32> caps = {0, 1, 2, 3, 4, 300};
    if (c.len != 0xFFFFFFFFu) {
      caps.push_back(c.len > 0 ? c.len - 1 : 0);
      caps.push_back(c.len);
      caps.push_back(c.len + 3);
    }
    for (u32 cap : caps) fold(f, run_decode(c.data, c.n, cap));
    CHECK(f.h == c.digest);
  }
}

TEST_CASE("hex: known vectors and case") {
  const char* raw[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
  const char* low[] = {"", "66", "666f", "666f6f", "666f6f62", "666f6f6261", "666f6f626172"};
  const char* up[] = {"", "66", "666F", "666F6F", "666F6F62", "666F6F6261", "666F6F626172"};
  for (int i = 0; i < 7; ++i) {
    CHECK(enc_str(raw[i], false) == low[i]);
    CHECK(enc_str(raw[i], true) == up[i]);
    for (const char* s : {low[i], up[i]}) {
      const EncOut d = run_decode_str(s, static_cast<u32>(std::strlen(raw[i])));
      REQUIRE(d.st == HexResult::Ok);
      CHECK(std::string(d.dst.begin(), d.dst.end()) == raw[i]);
    }
  }
  const u8 b[] = {0x00, 0x0A, 0xA0, 0xFF, 0xAB, 0x1F};
  CHECK(enc_str(std::string(reinterpret_cast<const char*>(b), 6), false) == "000aa0ffab1f");
  CHECK(enc_str(std::string(reinterpret_cast<const char*>(b), 6), true) == "000AA0FFAB1F");
  // mixed case decodes to the same bytes
  const EncOut m = run_decode_str("aBcDeF01", 4);
  REQUIRE(m.st == HexResult::Ok);
  CHECK(m.dst == std::vector<u8>({0xAB, 0xCD, 0xEF, 0x01}));
  const EncOut m2 = run_decode_str("AbCdEf01", 4);
  REQUIRE(m2.st == HexResult::Ok);
  CHECK(m2.dst == m.dst);
}

TEST_CASE("hex: encode checks and the no-write rule") {
  const std::vector<u8> in(10, 0xAB);
  char dst[24];
  std::memset(dst, kFill, sizeof dst);
  u32 len = kSent;
  auto untouched = [&]() {
    CHECK(len == kSent);
    for (char c : dst) CHECK(static_cast<u8>(c) == kFill);
  };
  CHECK(encode(in.data(), 10, dst, 24, nullptr, false) == HexResult::NullArgument);
  CHECK(encode(nullptr, 10, dst, 24, &len, false) == HexResult::NullArgument);
  CHECK(encode(in.data(), 10, nullptr, 24, &len, true) == HexResult::NullArgument);
  CHECK(encode(nullptr, kMaxInput + 1, dst, 24, &len, false) == HexResult::NullArgument);  // null before size
  CHECK(encode(in.data(), kMaxInput + 1, nullptr, 24, &len, false) == HexResult::NullArgument);
  untouched();
  CHECK(encode(in.data(), kMaxInput + 1, dst, 0, &len, false) == HexResult::TooLarge);  // before the capacity check, reads nothing
  CHECK(encode(in.data(), 0xFFFFFFFFu, dst, 24, &len, true) == HexResult::TooLarge);
  CHECK(encode(in.data(), kMaxInput, dst, 0, &len, false) == HexResult::BufferTooSmall);  // exactly at the limit, reads nothing
  CHECK(encode(in.data(), kMaxInput, dst, 24, &len, false) == HexResult::BufferTooSmall);
  CHECK(encode(in.data(), 10, nullptr, 0, &len, false) == HexResult::BufferTooSmall);
  CHECK(encode(in.data(), 10, dst, 19, &len, false) == HexResult::BufferTooSmall);  // needs 20
  CHECK(encode(in.data(), 1, dst, 1, &len, true) == HexResult::BufferTooSmall);
  untouched();
  CHECK(encode(in.data(), 10, dst, 20, &len, false) == HexResult::Ok);
  CHECK(len == 20u);
  CHECK(std::string(dst, 20) == "abababababababababab");
  CHECK(static_cast<u8>(dst[20]) == kFill);
  CHECK(encode(in.data(), 10, dst, 24, &len, true) == HexResult::Ok);
  CHECK(std::string(dst, 20) == "ABABABABABABABABABAB");
  len = kSent;
  CHECK(encode(nullptr, 0, nullptr, 0, &len, false) == HexResult::Ok);
  CHECK(len == 0u);
  std::memset(dst, kFill, sizeof dst);
  len = kSent;
  CHECK(encode(in.data(), 0, dst, 4, &len, true) == HexResult::Ok);
  CHECK(len == 0u);
  CHECK(static_cast<u8>(dst[0]) == kFill);
}

TEST_CASE("hex: every byte value encodes to its two digits") {
  std::vector<u8> all(256);
  for (u32 i = 0; i < 256; ++i) all[i] = static_cast<u8>(i);
  const char* lo = "0123456789abcdef";
  const char* hi = "0123456789ABCDEF";
  for (int upper = 0; upper < 2; ++upper) {
    const EncOut o = run_encode(all, 512, upper != 0);
    REQUIRE(o.st == HexResult::Ok);
    REQUIRE(o.len == 512u);
    const char* d = upper ? hi : lo;
    for (u32 i = 0; i < 256; ++i) {
      REQUIRE(o.dst[2 * i] == static_cast<u8>(d[i / 16]));
      REQUIRE(o.dst[2 * i + 1] == static_cast<u8>(d[i % 16]));
    }
    const EncOut back = run_decode(reinterpret_cast<const char*>(o.dst.data()), 512, 256);
    REQUIRE(back.st == HexResult::Ok);
    CHECK(back.dst == all);
  }
}

TEST_CASE("hex: decode check order") {
  u8 dst[8];
  std::memset(dst, kFill, sizeof dst);
  u32 len = kSent;
  auto untouched = [&]() {
    CHECK(len == kSent);
    for (u8 b : dst) CHECK(b == kFill);
  };
  const char* ok = "abcd";
  CHECK(decode(ok, 4, dst, 8, nullptr) == HexResult::NullArgument);
  CHECK(decode(nullptr, 4, dst, 8, &len) == HexResult::NullArgument);
  CHECK(decode(ok, 4, nullptr, 8, &len) == HexResult::NullArgument);
  CHECK(decode(nullptr, 5, dst, 8, &len) == HexResult::NullArgument);  // null before length
  CHECK(decode(nullptr, kMaxEncoded + 1, dst, 8, &len) == HexResult::NullArgument);
  CHECK(decode(ok, 3, dst, 8, &len) == HexResult::Malformed);
  CHECK(decode(ok, 5, dst, 0, &len) == HexResult::Malformed);
  CHECK(decode(ok, kMaxEncoded - 1, dst, 8, &len) == HexResult::Malformed);  // odd, at or below the limit: Malformed, reads nothing
  CHECK(decode(ok, kMaxEncoded + 1, dst, 8, &len) == HexResult::TooLarge);  // odd and above the limit: TooLarge, reads nothing
  CHECK(decode(ok, kMaxEncoded + 2, dst, 8, &len) == HexResult::TooLarge);
  CHECK(decode(ok, 0xFFFFFFFFu, dst, 8, &len) == HexResult::TooLarge);
  CHECK(decode("0g", 2, nullptr, 0, &len) == HexResult::Malformed);  // scan before capacity
  CHECK(decode("0g", 2, dst, 0, &len) == HexResult::Malformed);
  CHECK(decode("abcg", 4, dst, 1, &len) == HexResult::Malformed);
  CHECK(decode("gbcd", 4, dst, 1, &len) == HexResult::Malformed);
  CHECK(decode(ok, 4, dst, 1, &len) == HexResult::BufferTooSmall);
  CHECK(decode(ok, 4, nullptr, 0, &len) == HexResult::BufferTooSmall);
  untouched();
  CHECK(decode("ab", 2, dst, 1, &len) == HexResult::Ok);
  CHECK(len == 1u);
  CHECK(dst[0] == 0xAB);
  CHECK(dst[1] == kFill);
  len = kSent;
  CHECK(decode(nullptr, 0, nullptr, 0, &len) == HexResult::Ok);
  CHECK(len == 0u);
  len = kSent;
  CHECK(decode(ok, 0, dst, 8, &len) == HexResult::Ok);
  CHECK(len == 0u);
}

TEST_CASE("hex: every byte value at every position") {
  const std::string alphabet = "0123456789abcdefABCDEF";
  for (u32 pos = 0; pos < 6; ++pos) {
    for (u32 c = 0; c < 256; ++c) {
      std::string s(6, '0');
      s[pos] = static_cast<char>(c);
      const EncOut o = run_decode_str(s, 3);
      const bool alpha = c != 0 && alphabet.find(static_cast<char>(c)) != std::string::npos;
      if (alpha) {
        REQUIRE(o.st == HexResult::Ok);
        REQUIRE(o.len == 3u);
      } else {
        REQUIRE(o.st == HexResult::Malformed);
        REQUIRE(o.len == kSent);
      }
    }
  }
  // the value of every digit in the high and in the low position
  const char* d = "0123456789abcdefABCDEF";
  for (u32 i = 0; i < 22; ++i) {
    const u32 v = i < 16 ? i : i - 6;
    const std::string hi = std::string(1, d[i]) + "0";
    const std::string lo = std::string("0") + d[i];
    CHECK(run_decode_str(hi, 1).dst[0] == v * 16);
    CHECK(run_decode_str(lo, 1).dst[0] == v);
  }
}

TEST_CASE("hex: size limits") {
  // exactly at and just above the encode limit: nothing is read or written
  u8 one = 1;
  char c = static_cast<char>(kFill);
  u32 len = kSent;
  CHECK(encode(&one, kMaxInput, &c, 1, &len, false) == HexResult::BufferTooSmall);
  CHECK(encode(&one, kMaxInput + 1, &c, 1, &len, false) == HexResult::TooLarge);
  CHECK(len == kSent);
  CHECK(static_cast<u8>(c) == kFill);
  // decode: odd lengths around the limit return before reading anything, and a length above it is TooLarge
  CHECK(decode(&c, kMaxEncoded - 1, &one, 1, &len) == HexResult::Malformed);
  CHECK(decode(&c, kMaxEncoded + 1, &one, 1, &len) == HexResult::TooLarge);
  CHECK(decode(&c, kMaxEncoded + 2, &one, 1, &len) == HexResult::TooLarge);
  CHECK(decode(&c, 0xFFFFFFFFu, &one, 1, &len) == HexResult::TooLarge);
  CHECK(len == kSent);
  CHECK(static_cast<u8>(c) == kFill);
  CHECK(one == 1);
}

TEST_CASE("hex: structural oracle on random buffers") {
  Lcg r(0x4E5);
  for (int k = 0; k < 300; ++k) {
    const u32 n = r.rng(0, 200);
    const std::vector<u8> in = gen(0, n, r.next());
    for (int upper = 0; upper < 2; ++upper) {
      const EncOut e = run_encode(in, 2 * n, upper != 0);
      REQUIRE(e.st == HexResult::Ok);
      REQUIRE(e.len == 2 * n);
      std::string s = chars_of(e);
      for (char ch : s) {
        const bool digit = ch >= '0' && ch <= '9';
        const bool letter = upper ? (ch >= 'A' && ch <= 'F') : (ch >= 'a' && ch <= 'f');
        REQUIRE((digit || letter));
      }
      REQUIRE(run_decode_str(s, n).dst == in);
      // flip the case of each letter at random: still the same bytes
      for (char& ch : s) {
        if (ch >= 'a' && ch <= 'f' && (r.next() & 1u)) ch = static_cast<char>(ch - 32);
        else if (ch >= 'A' && ch <= 'F' && (r.next() & 1u)) ch = static_cast<char>(ch + 32);
      }
      REQUIRE(run_decode_str(s, n).dst == in);
      if (n > 0) {
        // one bad character anywhere is Malformed, whatever the capacity
        std::string t = s;
        t[r.rng(0, 2 * n - 1)] = 'x';
        REQUIRE(run_decode_str(t, n).st == HexResult::Malformed);
        REQUIRE(run_decode_str(t, 0).st == HexResult::Malformed);
        // odd length
        REQUIRE(run_decode_str(s.substr(1), n).st == HexResult::Malformed);
      }
    }
  }
}
