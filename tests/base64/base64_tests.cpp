// Tests for core/base64 (scope AK). Expected values come from an independent Python reference (tools/base64_ref.py: bit strings and its
// own position-by-position checker, with the standard module only as a second check, and its own FNV) and are pasted below: seeded encode
// cases (the input generator is mirrored here) and decode strings embedded as arrays, each checked by a digest of the status, the length
// and the bytes for several destination capacities. RFC 4648 vectors, the check orders, every byte value at every position, the no-write
// rules and the size limits sit beside them. Every destination is an exact-size heap vector, so ASan sees any write past it; a failure
// must leave the sentinel bytes and out_len as they were. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "base64.h"

using namespace lumora::base64;

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
};

struct DecCase {
  const char* data;
  u32 n;
  u32 len;  // 0xFFFFFFFF when the string is rejected
  u64 digest;
};

const EncCase kEncCases[] = {{0, 0, 100, 4672095441862941765ull, 0u}, {0, 1, 101, 3131653334287708000ull, 4u}, {0, 2, 102, 15471232430604950720ull, 4u}, {0, 3, 103, 6698943082173389472ull, 4u}, {0, 4, 104, 15067680320497736096ull, 8u}, {0, 5, 105, 6264128184708992608ull, 8u}, {0, 6, 106, 14990677727349397024ull, 8u}, {0, 7, 107, 13096212855794608320ull, 12u}, {0, 8, 108, 4100640201916905280ull, 12u}, {0, 9, 109, 15445143560920578112ull, 12u}, {0, 10, 110, 9012416050691132640ull, 16u}, {0, 11, 111, 7807773538880843296ull, 16u}, {0, 12, 112, 13119165589562757408ull, 16u}, {1, 5, 1, 5025230470246138016ull, 8u}, {2, 7, 2, 12061843129921330720ull, 12u}, {3, 256, 3, 10221688551311218060ull, 344u}, {0, 255, 4, 15074054545333907648ull, 340u}, {0, 257, 5, 11197577295718600272ull, 344u}, {1, 3000, 6, 12224764765824634304ull, 4000u}, {2, 3001, 7, 5810235881328395536ull, 4004u}, {3, 4097, 8, 12000260344392035540ull, 5464u}, {0, 10000, 9, 11122930805716704016ull, 13336u}, {0, 65536, 10, 7694631492644026092ull, 87384u}, {3, 70001, 11, 270580793368953728ull, 93336u}};
const char kDec0[] = {0};
const char kDec1[] = {69, 65, 61, 61};
const char kDec2[] = {80, 101, 89, 61};
const char kDec3[] = {97, 111, 104, 48};
const char kDec4[] = {108, 121, 114, 114, 81, 103, 61, 61};
const char kDec5[] = {120, 99, 120, 105, 74, 98, 69, 61};
const char kDec6[] = {56, 109, 55, 90, 67, 89, 118, 107};
const char kDec7[] = {72, 120, 66, 80, 55, 71, 97, 85, 98, 81, 61, 61};
const char kDec8[] = {84, 98, 76, 71, 48, 69, 66, 69, 77, 117, 115, 61};
const char kDec9[] = {101, 108, 81, 57, 116, 66, 114, 48, 57, 107, 118, 86};
const char kDec10[] = {65, 65, 69, 67, 65, 119, 81, 70, 66, 103, 99, 73, 67, 81, 111, 76, 68, 65, 48, 79, 68, 120, 65, 82, 69, 104, 77, 85, 70, 82, 89, 88, 71, 66, 107, 97, 71, 120, 119, 100, 72, 104, 56, 103, 73, 83, 73, 106, 74, 67, 85, 109, 74, 121, 103, 112, 75, 105, 115, 115, 76, 83, 52, 118, 77, 68, 69, 121, 77, 122, 81, 49, 78, 106, 99, 52, 79, 84, 111, 55, 80, 68, 48, 43, 80, 48, 66, 66, 81, 107, 78, 69, 82, 85, 90, 72, 83, 69, 108, 75, 83, 48, 120, 78, 84, 107, 57, 81, 85, 86, 74, 84, 86, 70, 86, 87, 86, 49, 104, 90, 87, 108, 116, 99, 88, 86, 53, 102, 89, 71, 70, 105, 89, 50, 82, 108, 90, 109, 100, 111, 97, 87, 112, 114, 98, 71, 49, 117, 98, 51, 66, 120, 99, 110, 78, 48, 100, 88, 90, 51, 101, 72, 108, 54, 101, 51, 120, 57, 102, 110, 43, 65, 103, 89, 75, 68, 104, 73, 87, 71, 104, 52, 105, 74, 105, 111, 117, 77, 106, 89, 54, 80, 107, 74, 71, 83, 107, 53, 83, 86, 108, 112, 101, 89, 109, 90, 113, 98, 110, 74, 50, 101, 110, 54, 67, 104, 111, 113, 79, 107, 112, 97, 97, 110, 113, 75, 109, 113, 113, 54, 121, 116, 114, 113, 43, 119, 115, 98, 75, 122, 116, 76, 87, 50, 116, 55, 105, 53, 117, 114, 117, 56, 118, 98, 54, 47, 119, 77, 72, 67, 119, 56, 84, 70, 120, 115, 102, 73, 121, 99, 114, 76, 122, 77, 51, 79, 122, 57, 68, 82, 48, 116, 80, 85, 49, 100, 98, 88, 50, 78, 110, 97, 50, 57, 122, 100, 51, 116, 47, 103, 52, 101, 76, 106, 53, 79, 88, 109, 53, 43, 106, 112, 54, 117, 118, 115, 55, 101, 55, 118, 56, 80, 72, 121, 56, 47, 84, 49, 57, 118, 102, 52, 43, 102, 114, 55, 47, 80, 51, 43, 47, 119, 65, 66, 65, 103, 77, 69, 66, 81, 89, 72, 67, 65, 107, 75, 67, 119, 119, 78, 68, 103, 56, 81, 69, 82, 73, 84, 70, 66, 85, 87, 70, 120, 103, 90, 71, 104, 115, 99, 72, 82, 52, 102, 73, 67, 69, 105, 73, 121, 81, 108, 74, 105, 99, 111, 75, 83, 111, 114};
const char kDec11[] = {50, 86, 85, 81, 115, 73, 43, 87, 43, 85, 122, 106, 116, 69, 52, 108, 53, 81, 87, 65, 57, 100, 86, 98, 72, 73, 53, 81, 122, 78, 109, 49, 50, 107, 121, 118, 86, 66, 43, 71, 114, 75, 105, 55, 84, 80, 102, 121, 65, 51, 107, 82, 113, 72, 57, 90, 82, 106, 43, 53, 121, 71, 69, 43, 70, 88, 50, 88, 111, 51, 51, 52, 85, 68, 75, 108, 116, 108, 99, 69, 66, 66, 120, 70, 75, 48, 114, 105, 120, 108, 110, 104, 73, 48, 102, 111, 86, 77, 116, 108, 73, 74, 119, 99, 54, 118, 70, 108, 119, 118, 48, 48, 122, 51, 48, 85, 50, 43, 49, 57, 79, 114, 77, 101, 112, 111, 116, 56, 53, 82, 88, 105, 65, 78, 112, 72, 108, 74, 114, 89, 85, 66, 48, 72, 110, 49, 69, 115, 68, 89, 105, 110, 98, 69, 53, 69, 108, 112, 70, 71, 118, 98, 77, 108, 102, 101, 100, 87, 74, 76, 119, 108, 51, 97, 112, 108, 57, 48, 118, 106, 118, 84, 87, 79, 114, 55, 49, 120, 55, 103, 79, 77, 103, 84, 111, 118, 65, 77, 111, 85, 90, 78, 48, 69, 43, 81, 70, 71, 76, 82, 47, 106, 70, 67, 43, 85, 72, 117, 89, 71, 84, 57, 84, 84, 51, 83, 56, 72, 84, 81, 115, 67, 71, 76, 49, 115, 47, 43, 70, 84, 77, 79, 113, 101, 103, 112, 43, 117, 79, 80, 70, 69, 102, 85, 105, 81, 67, 71, 86, 56, 78, 104, 85, 51, 102, 65, 65, 102, 107, 75, 80, 67, 97, 71, 118, 105, 110, 85, 118, 100, 106, 85, 77, 98, 115, 103, 107, 104, 99, 82, 85, 86, 110, 106, 100, 67, 65, 118, 88, 107, 115, 79, 88, 89, 50, 114, 107, 113, 73, 98, 100, 108, 99, 116, 52, 121, 73, 97, 111, 48, 50, 65, 122, 81, 117, 102, 73, 57, 97, 70, 66, 121, 103, 83, 88, 102, 47, 70, 57, 121, 111, 88, 104, 53, 47, 74, 119, 114, 89, 105, 84, 111, 88, 87, 113, 55, 49, 72, 97, 65, 105, 117, 108, 90, 57, 102, 54, 75, 49, 104, 105, 52, 53, 77, 74, 43, 110, 102, 97, 108, 114, 83, 49, 103, 85, 77, 54, 76, 70, 51, 67, 107, 99, 80, 90, 99, 66, 118, 99, 67, 87, 74, 70, 120, 52, 51, 73, 110, 81, 87, 113, 80, 71, 51, 83, 79, 117, 118, 121, 51, 68, 112, 75, 52, 102, 107, 103, 99, 78, 121, 85, 99, 69, 78, 117, 79, 56, 121, 76, 56, 117, 97, 55, 65, 86, 83, 90, 103, 57, 106, 109, 55, 54, 89, 88, 116, 49, 51, 85, 65, 101, 66, 90, 122, 111, 52, 106, 109, 107, 119, 89, 87, 120, 106, 102, 114, 88, 48, 75, 115, 48, 81, 43, 82, 117, 104, 69, 80, 82, 119, 117, 53, 117, 97, 116, 117, 113, 74, 81, 97, 48, 74, 99, 110, 76, 77, 53, 113, 98, 113, 74, 52, 65, 66, 120, 50, 75, 99, 83, 70, 85, 67, 98, 82, 114, 43, 90, 107, 50, 111, 49, 78, 88, 50, 73, 82, 105, 84, 70, 115, 43, 98, 49, 56, 56, 104, 43, 86, 89, 86, 122, 73, 75, 65, 103, 98, 114, 79, 108, 100, 117, 75, 57, 65, 118, 50, 79, 106, 52, 47, 54, 69, 99, 52, 47, 53, 80, 88, 99, 119, 77, 114, 82, 53, 119, 53, 52, 70, 103, 110, 119, 55, 66, 116, 75, 117, 66, 78, 82, 72, 74, 109, 105, 117, 85, 121, 70, 71, 78, 98, 74, 81, 112, 103, 109, 70, 81, 80, 81, 80, 48, 118, 111, 51, 120, 50, 106, 50, 78, 70, 113, 109, 65, 88, 99, 55, 97, 54, 105, 54, 97, 47, 85, 107, 102, 56, 111, 74, 83, 120, 84, 118, 84, 88, 103, 67, 112, 102, 70, 50, 82, 101, 83, 111, 53, 99, 74, 67, 111, 83, 113, 51, 72, 85, 79, 85, 100, 107, 110, 104, 84, 78, 119, 117, 115, 87, 47, 121, 51, 112, 65, 100, 74, 77, 107, 55, 67, 118, 113, 69, 109, 67, 107, 86, 53, 81, 79, 120, 69, 65, 111, 75, 102, 113, 56, 50, 109, 70, 48, 86, 75, 55, 115, 86, 88, 116, 80, 84, 120, 54, 88, 99, 50, 115, 85, 120, 72, 68, 47, 43, 99, 88, 108, 89, 78, 104, 103, 111, 89, 81, 107, 77, 79, 121, 83, 101, 84, 49, 79, 89, 108, 43, 121, 52, 103, 107, 74, 77, 120, 88, 102, 71, 106, 85, 115, 87, 47, 116, 68, 122, 120, 51, 69, 66, 114, 49, 108, 101, 54, 111, 71, 90, 110, 108, 69, 104, 107, 81, 85, 82, 65, 82, 88, 57, 104, 78, 105, 78, 87, 113, 100, 51, 54, 111, 65, 110, 116, 76, 51, 85, 104, 90, 54, 83, 105, 55, 70, 88, 85, 48, 57, 109, 86, 115, 100, 78, 72, 121, 67, 102, 80, 107, 50, 53, 53, 104, 43, 84, 116, 115, 113, 116, 80, 98, 89, 84, 73, 102, 81, 89, 108, 87, 84, 106, 82, 70, 120, 106, 116, 87, 68, 97, 110, 119, 86, 120, 79, 68, 57, 84, 115, 78, 73, 113, 49, 118, 77, 87, 122, 106, 98, 117, 104, 78, 109, 71, 47, 100, 121, 90, 56, 70, 55, 68, 111, 75, 74, 49, 104, 116, 119, 105, 74, 97, 102, 81, 77, 116, 70, 79, 68, 71, 112, 73, 99, 68, 50, 82, 79, 119, 112, 54, 75, 115, 89, 49, 83, 72, 106, 69, 110, 122, 77, 81, 121, 56, 56, 81, 117, 55, 118, 70, 105, 113, 111, 102, 53, 70, 83, 110, 101, 51, 80, 102, 98, 49, 81, 73, 119, 120, 65, 43, 114, 74, 77, 71, 87, 107, 113, 122, 97, 78, 75, 76, 80, 114, 112, 117, 115, 90, 79, 65, 76, 99, 83, 84, 87, 90, 108, 48, 106, 115, 108, 79, 43, 112, 81, 65, 74, 80, 119, 103, 76, 101, 75, 87, 55, 51, 106, 80, 55, 113, 88, 103, 107, 104, 79, 115, 80, 112, 102, 90, 74, 76, 107, 67, 49, 112, 70, 77, 89, 68, 47, 84, 47, 116, 89, 78, 50, 122, 47, 105, 68, 104, 83, 107, 85, 122, 48, 109, 101, 52, 102, 88, 47, 110, 108, 107, 86, 114, 90, 83, 81, 106, 50, 97, 118, 67, 67, 97, 100, 50, 112, 53, 101, 71, 115, 74, 71, 112, 116, 100, 48, 51, 55, 110, 47, 97, 111, 87, 70, 118, 109, 81, 85, 119, 112, 121, 116, 43, 121, 73, 101, 52, 113, 84, 88, 87, 105, 72, 115, 75, 108, 117, 73, 77, 122, 115, 109, 71, 82, 119, 79, 100, 118, 101, 112, 54, 81, 87, 106, 66, 51, 72, 116, 65, 67, 106, 114, 81, 55, 103, 97, 70, 54, 111, 49, 84, 57, 83, 89, 116, 109, 120, 80, 114, 107, 100, 51, 69, 105, 83, 109, 108, 73, 85, 85, 80, 101, 51, 88, 76, 49, 70, 49, 48, 121, 77, 68, 98, 106, 74, 117, 78, 118, 47, 100, 82, 68, 48, 65, 116, 97, 66, 84, 52, 106, 104, 109, 103, 90, 52, 97, 73, 50, 104, 75, 116, 99, 87, 75, 75, 109, 116, 115, 104, 112, 69, 73, 81, 85, 118, 50, 104, 113, 119, 66, 66, 88, 66, 87, 80, 77, 83, 84, 112, 53, 43, 107, 119, 56, 116, 78, 50, 109, 90, 117, 84, 81, 113, 108, 103, 84, 105, 67, 85, 86, 99, 77, 103, 82, 119, 85, 70, 71, 69, 49, 104, 99, 75, 110, 102, 75, 86, 113, 111, 51, 84, 79, 103, 97, 54, 53, 108, 82, 50, 75, 43, 116, 70, 76, 66, 109, 52, 107, 118, 122, 108, 98, 103, 89, 121, 98, 84, 101, 122, 66, 53, 112, 51, 97, 56, 100, 112, 51, 43, 115, 115, 56, 85, 65, 47, 116, 119, 61, 61};
const char kDec12[] = {65};
const char kDec13[] = {65, 66};
const char kDec14[] = {65, 66, 67};
const char kDec15[] = {65, 66, 67, 68, 69};
const char kDec16[] = {81, 85, 74, 68};
const char kDec17[] = {81, 81, 61, 61};
const char kDec18[] = {81, 85, 73, 61};
const char kDec19[] = {81, 82, 61, 61};
const char kDec20[] = {81, 85, 74, 61};
const char kDec21[] = {81, 85, 75, 61};
const char kDec22[] = {81, 82, 61, 65};
const char kDec23[] = {81, 61, 61, 61};
const char kDec24[] = {61, 61, 61, 61};
const char kDec25[] = {61, 81, 85, 74};
const char kDec26[] = {81, 85, 61, 74};
const char kDec27[] = {81, 81, 61, 65};
const char kDec28[] = {81, 81, 61, 32};
const char kDec29[] = {81, 81, 61, 61, 81, 81, 61, 61};
const char kDec30[] = {81, 66, 61, 61};
const char kDec31[] = {81, 67, 61, 61};
const char kDec32[] = {81, 69, 61, 61};
const char kDec33[] = {81, 73, 61, 61};
const char kDec34[] = {81, 85, 69, 61};
const char kDec35[] = {81, 85, 73, 61};
const char kDec36[] = {81, 85, 77, 61};
const char kDec37[] = {81, 85, 74, 68, 61, 81, 81, 61};
const char kDec38[] = {81, 85, 74, 68, 81, 81, 61, 61};
const char kDec39[] = {81, 85, 74, 68, 81, 85, 73, 61};
const char kDec40[] = {81, 85, 74, 68, 10};
const char kDec41[] = {81, 85, 74, 10, 81, 81, 61, 61};
const char kDec42[] = {81, 85, 74, 68, 32, 81, 81, 61, 61};
const char kDec43[] = {81, 85, 45, 68};
const char kDec44[] = {81, 85, 95, 68};
const char kDec45[] = {81, 85, 0, 68};
const char kDec46[] = {81, 85, -56, 68};
const char kDec47[] = {81, 85, 74, 68, 81, 85, 74, 68};
const char kDec48[] = {43, 47, 43, 47};
const char kDec49[] = {47, 43, 47, 43};
const char kDec50[] = {43, 47, 61, 61};
const char kDec51[] = {43, 119, 61, 61};
const char kDec52[] = {43, 120, 61, 61};
const char kDec53[] = {47, 43, 56, 61};
const char kDec54[] = {47, 43, 57, 61};
const char kDec55[] = {65, 65, 65, 65};
const char kDec56[] = {65, 65, 65, 61};
const char kDec57[] = {65, 65, 61, 61};
const char kDec58[] = {65, 66, 61, 61};
const char kDec59[] = {65, 65, 66, 61};
const char kDec60[] = {65, 65, 67, 61};
const char kDec61[] = {65, 61, 61, 61};
const char kDec62[] = {65, 65, 65, -1};
const char kDec63[] = {81, 85, 74, 68, 81, 85, -1, 68};
const char kDec64[] = {81, 81, 61, 61, 0, 0, 0, 0};
const char kDec65[] = {108, 79, 117, 70, 109, 98, 101, 32};
const char kDec66[] = {119, 111, 51, 56, 102, 74, 69, 55, 111, 109, 61, 61};
const char kDec67[] = {0, 121, 57, 122, 89, 71, 122, 114, 90, 101, 74, 102, 66, 89, 70, 103};
const char kDec68[] = {72, 78, 72, 113, 81, 48, 97, 98, 75, 85, 69, 51};
const char kDec69[] = {83, 88, 78, 104, 74, 115, 66, 76, 55, 103, 61, 61};
const char kDec70[] = {100, 119, 61, 47};
const char kDec71[] = {112, 76, 100, 80, 55, 116, 72, 114, 100, 119, 61, 61};
const char kDec72[] = {48, 86, 110, 71, 48, 103, 61, 53};
const char kDec73[] = {47, 47, 115, 57, 116, 89, 111, 76, 65, 66, 48, 77};
const char kDec74[] = {76, 65, 116, 61};
const char kDec75[] = {87, 84, 65, 114, 102, 68, 53, 114};
const char kDec76[] = {104, 43, 71, 105, 89, 66, 97, 98, 84, 106, 115, 61};
const char kDec77[] = {116, 73, 77, 112, 81, 119, 61, 61};
const char kDec78[] = {52, 83, 87, 81, 74, 56, 49, 56, 49, 47, 113, 87, 32, 85, 111, 61};
const char kDec79[] = {68, 115, 99, 72, 67, 54, 103, 89};
const char kDec80[] = {80, 83, 108, 43, 55, 103, 61, 61};
const char kDec81[] = {97, 81, 118, 49, 48, 108, 121, 77, 57, 82, 100, 73};
const char kDec82[] = {80, 103, 61, 61};
const char kDec83[] = {120, 69, 47, 105, 109, 87, 72, 115, 114, 119, 61, 61};
const char kDec84[] = {56, 103, 70, 90, 102, 79, 117, 99, 99, 119, 61, 61};
const char kDec85[] = {72, 48, 80, 81, 89, 65, 61, 61};
const char kDec86[] = {83, -56, 86, 72, 82, 75, 68, 56, 47, 65, 61, 61};
const char kDec87[] = {101, 100, 101, 43, 117, 119, 61, 61};
const char kDec88[] = {112, 110, 107, 56};
const char kDec89[] = {48, 120, 117, 99, 55, 105, 56, 61};
const char kDec90[] = {65, 98, 48, 106, 48, 103, 51, 56, 68, 51, 65, 61};
const char kDec91[] = {109, 108, 56, 61};
const char kDec92[] = {87, 119, 69, 82, 109, 98, 52, 100, 109, 65, 61, 79};
const char kDec93[] = {105, 97, 77, 75};
const char kDec94[] = {70, 107, 88, 47};
const char kDec95[] = {52, 43, 68, 50, 82, 69, 48, 116};
const char kDec96[] = {69, 104, 110, 116, 74, 121, 106, 100, 113, 54, 118, 67};
const char kDec97[] = {80, 105, 116, 107, 67, 119, 75, 78, 98, 119, 112, 83, 87, 116, 77, 61};
const char kDec98[] = {97, 51, 51, 98};
const char kDec99[] = {109, 71, 57, 83, 48, 114, 102, 116, 43, 99, 108, 74, 49, 119, 61, 61};
const char kDec100[] = {120, 104, 72, 74, 116, 112, 71, 100, 118, 79, 103, 69, 70, 116, 115, 61};
const char kDec101[] = {56, 55, 77, 47, 82, 87, 116, 78, 103, 111, 105, 86, 86, 68, 77, 61};
const char kDec102[] = {73, 83, 87, 50};
const char kDec103[] = {45, 103, 61, 61};
const char kDec104[] = {101, 53, 109, 97, 82, 80, 112, 100, 48, 75, 86, 72, 69, 68, 115, 61};
const char kDec105[] = {113, 109, 115, 61};
const char kDec106[] = {49, 100, 50, 83, 67, 54, 43, 57, 87, 87, 54, 111, 106, 101, 115, 110};
const char kDec107[] = {65, 81, 56, 74, 55, 52, 108, 117, 72, 115, 80, 52, 121, 48, 78, 111};
const char kDec108[] = {77, 67, 71, 65, 48, 57, 61, 61};
const char kDec109[] = {88, 89, 77, 61};
const char kDec110[] = {105, 50, 86, 117, 109, 67, 104, 43};
const char kDec111[] = {117, 65, 102, 108, 102, 102, 77, 117, 98, 69, 65, 55};
const char kDec112[] = {53, 97, 108, 99, 89, 99, 51, 101, 53, 90, 47, 77, 66, 80, 121, 112};
const char kDec113[] = {69, 48, 118, 84, 82, 65, 83, 61};
const char kDec114[] = {81, 79, 49, 75, 75, 116, 61, 61};
const char kDec115[] = {98, 89, 47, 66, 67, 49, 111, 117, 81, 55, 49, 43, 119, 65, 81, 61};
const char kDec116[] = {77, 106, 69, 52, 55, 122, 97, 101, 66, 120, 119, 79, 47, 49, 119, 61};
const char kDec117[] = {121, 65, 119, 61};
const char kDec118[] = {57, 88, 85, 109, 116, 117, 118, 43, 107, 65, 57, 61};
const char kDec119[] = {73, 104, 101, 100, 109, 115, 61, 117, 86, 84, 114, 65, 117, 103, 61, 61};
const char kDec120[] = {115, 76, 107, 84, 102, 97, 66, 101, 71, 103, 61, 61};
const char kDec121[] = {102, 86, 84, 75, 89, 81, 61, 61};
const char kDec122[] = {113, 118, 48, 66, 82, 70, 83, 47, 111, 49, 100, 50, 100, 109, 119, 114};
const char kDec123[] = {50, 110, 57, 52, 75, 67, 57, 118, 97, 65, 61, 61};
const char kDec124[] = {66, 85, 72, 118, 67, 119, -1, 102};
const DecCase kDecCases[] = {{kDec0, 0u, 0u, 4524767293552705221ull}, {kDec1, 4u, 1u, 7683015633971457516ull}, {kDec2, 4u, 2u, 383179561786199584ull}, {kDec3, 4u, 3u, 4448030216371800312ull}, {kDec4, 8u, 4u, 4609796269223772376ull}, {kDec5, 8u, 5u, 4627538877958484343ull}, {kDec6, 8u, 6u, 6515550822200808144ull}, {kDec7, 12u, 7u, 13152397093481182705ull}, {kDec8, 12u, 8u, 3994777786260504617ull}, {kDec9, 12u, 9u, 1066199781109046757ull}, {kDec10, 400u, 300u, 16905095908573249578ull}, {kDec11, 1336u, 1000u, 15039579908561950988ull}, {kDec12, 1u, 4294967295u, 14222729515248215621ull}, {kDec13, 2u, 4294967295u, 14222729515248215621ull}, {kDec14, 3u, 4294967295u, 14222729515248215621ull}, {kDec15, 5u, 4294967295u, 14222729515248215621ull}, {kDec16, 4u, 3u, 13914568807832079238ull}, {kDec17, 4u, 1u, 3366967647248189197ull}, {kDec18, 4u, 2u, 152205622689983584ull}, {kDec19, 4u, 4294967295u, 3088575930823903669ull}, {kDec20, 4u, 4294967295u, 3088575930823903669ull}, {kDec21, 4u, 4294967295u, 3088575930823903669ull}, {kDec22, 4u, 4294967295u, 14222729515248215621ull}, {kDec23, 4u, 4294967295u, 14222729515248215621ull}, {kDec24, 4u, 4294967295u, 14222729515248215621ull}, {kDec25, 4u, 4294967295u, 14222729515248215621ull}, {kDec26, 4u, 4294967295u, 14222729515248215621ull}, {kDec27, 4u, 4294967295u, 14222729515248215621ull}, {kDec28, 4u, 4294967295u, 14222729515248215621ull}, {kDec29, 8u, 4294967295u, 14222729515248215621ull}, {kDec30, 4u, 4294967295u, 3088575930823903669ull}, {kDec31, 4u, 4294967295u, 3088575930823903669ull}, {kDec32, 4u, 4294967295u, 3088575930823903669ull}, {kDec33, 4u, 4294967295u, 3088575930823903669ull}, {kDec34, 4u, 2u, 10034160538403565696ull}, {kDec35, 4u, 2u, 152205622689983584ull}, {kDec36, 4u, 2u, 8460343253022371584ull}, {kDec37, 8u, 4294967295u, 14222729515248215621ull}, {kDec38, 8u, 4u, 6028246091274427608ull}, {kDec39, 8u, 5u, 4414829773109824147ull}, {kDec40, 5u, 4294967295u, 14222729515248215621ull}, {kDec41, 8u, 4294967295u, 14222729515248215621ull}, {kDec42, 9u, 4294967295u, 14222729515248215621ull}, {kDec43, 4u, 4294967295u, 14222729515248215621ull}, {kDec44, 4u, 4294967295u, 14222729515248215621ull}, {kDec45, 4u, 4294967295u, 14222729515248215621ull}, {kDec46, 4u, 4294967295u, 14222729515248215621ull}, {kDec47, 8u, 6u, 11028327598397648595ull}, {kDec48, 4u, 3u, 15609336007601384525ull}, {kDec49, 4u, 3u, 9199484291019079200ull}, {kDec50, 4u, 4294967295u, 3088575930823903669ull}, {kDec51, 4u, 1u, 2031348524015955895ull}, {kDec52, 4u, 4294967295u, 3088575930823903669ull}, {kDec53, 4u, 2u, 3280998435680519072ull}, {kDec54, 4u, 4294967295u, 3088575930823903669ull}, {kDec55, 4u, 3u, 7248011572806334598ull}, {kDec56, 4u, 2u, 10800462565536406176ull}, {kDec57, 4u, 1u, 12880525495385071484ull}, {kDec58, 4u, 4294967295u, 3088575930823903669ull}, {kDec59, 4u, 4294967295u, 3088575930823903669ull}, {kDec60, 4u, 4294967295u, 3088575930823903669ull}, {kDec61, 4u, 4294967295u, 14222729515248215621ull}, {kDec62, 4u, 4294967295u, 14222729515248215621ull}, {kDec63, 8u, 4294967295u, 14222729515248215621ull}, {kDec64, 8u, 4294967295u, 14222729515248215621ull}, {kDec65, 8u, 4294967295u, 14222729515248215621ull}, {kDec66, 12u, 4294967295u, 3088575930823903669ull}, {kDec67, 16u, 4294967295u, 14222729515248215621ull}, {kDec68, 12u, 9u, 2376551609480221514ull}, {kDec69, 12u, 7u, 11837587816924176346ull}, {kDec70, 4u, 4294967295u, 14222729515248215621ull}, {kDec71, 12u, 7u, 418547375469040773ull}, {kDec72, 8u, 4294967295u, 14222729515248215621ull}, {kDec73, 12u, 9u, 6523093024498951840ull}, {kDec74, 4u, 4294967295u, 3088575930823903669ull}, {kDec75, 8u, 6u, 5781072850007205832ull}, {kDec76, 12u, 8u, 12177137814009471601ull}, {kDec77, 8u, 4u, 5897568337198900568ull}, {kDec78, 16u, 4294967295u, 14222729515248215621ull}, {kDec79, 8u, 6u, 8045009471386603502ull}, {kDec80, 8u, 4u, 13641737654518650456ull}, {kDec81, 12u, 9u, 83369062221684787ull}, {kDec82, 4u, 1u, 8351566466925018922ull}, {kDec83, 12u, 7u, 16755159201532017568ull}, {kDec84, 12u, 7u, 16056806989913664656ull}, {kDec85, 8u, 4u, 12700354927045574296ull}, {kDec86, 12u, 4294967295u, 14222729515248215621ull}, {kDec87, 8u, 4u, 3185753302514896408ull}, {kDec88, 4u, 3u, 17686848061011111189ull}, {kDec89, 8u, 5u, 10953621629677750941ull}, {kDec90, 12u, 8u, 3737221677755525030ull}, {kDec91, 4u, 2u, 10522268930549023104ull}, {kDec92, 12u, 4294967295u, 14222729515248215621ull}, {kDec93, 4u, 3u, 16442947452248252598ull}, {kDec94, 4u, 3u, 14439304073487576418ull}, {kDec95, 8u, 6u, 17097543785720806722ull}, {kDec96, 12u, 9u, 11297316833971370426ull}, {kDec97, 16u, 11u, 13471094848540905453ull}, {kDec98, 4u, 3u, 10878729658972691427ull}, {kDec99, 16u, 10u, 9512178025093830804ull}, {kDec100, 16u, 11u, 3069295197682081679ull}, {kDec101, 16u, 11u, 10133530312777171386ull}, {kDec102, 4u, 3u, 3364800475813092580ull}, {kDec103, 4u, 4294967295u, 14222729515248215621ull}, {kDec104, 16u, 11u, 9088114587871805588ull}, {kDec105, 4u, 2u, 10789924440371826784ull}, {kDec106, 16u, 12u, 5960454730630608060ull}, {kDec107, 16u, 12u, 4573766448107933459ull}, {kDec108, 8u, 4294967295u, 3088575930823903669ull}, {kDec109, 4u, 2u, 18103423114289666784ull}, {kDec110, 8u, 6u, 4353907612942092661ull}, {kDec111, 12u, 9u, 7164176137927842433ull}, {kDec112, 16u, 12u, 7089067284007265428ull}, {kDec113, 8u, 4294967295u, 3088575930823903669ull}, {kDec114, 8u, 4294967295u, 3088575930823903669ull}, {kDec115, 16u, 11u, 548634915132975886ull}, {kDec116, 16u, 11u, 1517066580696893628ull}, {kDec117, 4u, 2u, 5903355435347296160ull}, {kDec118, 12u, 4294967295u, 3088575930823903669ull}, {kDec119, 16u, 4294967295u, 14222729515248215621ull}, {kDec120, 12u, 7u, 2854700989309700449ull}, {kDec121, 8u, 4u, 6397161876037052696ull}, {kDec122, 16u, 12u, 3610830374642482099ull}, {kDec123, 12u, 7u, 545170320141132775ull}, {kDec124, 8u, 4294967295u, 14222729515248215621ull}};

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
  Base64Result st;
  u32 len;
  std::vector<u8> dst;  // bytes of the destination, characters or decoded bytes
};

// dst is an exact-size vector of cap bytes filled with a sentinel; a failure must leave it and len alone.
EncOut run_encode(const std::vector<u8>& in, u32 cap) {
  EncOut o;
  o.dst.assign(cap, kFill);
  o.len = kSent;
  o.st = encode(in.data(), static_cast<u32>(in.size()), cap ? reinterpret_cast<char*>(o.dst.data()) : nullptr, cap, &o.len);
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
  if (o.st == Base64Result::Ok) {
    for (u32 i = 0; i < o.len; ++i) f.word(o.dst[i]);
    for (std::size_t i = o.len; i < o.dst.size(); ++i) REQUIRE(o.dst[i] == kFill);  // nothing past out_len
  } else {
    for (u8 b : o.dst) REQUIRE(b == kFill);
  }
}

std::string chars_of(const EncOut& o) { return std::string(o.dst.begin(), o.dst.begin() + o.len); }

std::string enc_str(const std::string& raw) {
  const std::vector<u8> in(raw.begin(), raw.end());
  const EncOut o = run_encode(in, static_cast<u32>(4 * ((raw.size() + 2) / 3)));
  REQUIRE(o.st == Base64Result::Ok);
  return chars_of(o);
}

}  // namespace

TEST_CASE("base64: encode cases match the independent reference") {
  for (const EncCase& c : kEncCases) {
    const std::vector<u8> in = gen(c.mode, c.n, c.seed);
    Fnv f;
    const u32 need = c.need;
    const u32 caps[] = {0, 1, need > 0 ? need - 1 : 0, need, need + 3};
    for (u32 cap : caps) {
      const EncOut o = run_encode(in, cap);
      fold(f, o);
      CHECK(o.st == (need > cap ? Base64Result::BufferTooSmall : Base64Result::Ok));
    }
    CHECK(f.h == c.digest);
    // round trip through the strict decoder
    const EncOut e = run_encode(in, need);
    REQUIRE(e.st == Base64Result::Ok);
    const EncOut d = run_decode(reinterpret_cast<const char*>(e.dst.data()), need, c.n);
    REQUIRE(d.st == Base64Result::Ok);
    CHECK(d.len == c.n);
    CHECK(d.dst == in);
  }
}

TEST_CASE("base64: decode strings match the independent reference") {
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

TEST_CASE("base64: RFC 4648 vectors") {
  const char* raw[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
  const char* enc[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
  for (int i = 0; i < 7; ++i) {
    CHECK(enc_str(raw[i]) == enc[i]);
    const std::string e = enc[i];
    const EncOut d = run_decode_str(e, static_cast<u32>(std::strlen(raw[i])));
    REQUIRE(d.st == Base64Result::Ok);
    CHECK(d.len == std::strlen(raw[i]));
    CHECK(std::string(d.dst.begin(), d.dst.end()) == raw[i]);
  }
  // all 64 characters
  const u8 all[] = {0x00, 0x10, 0x83, 0x10, 0x51, 0x87, 0x20, 0x92, 0x8B, 0x30, 0xD3, 0x8F, 0x41, 0x14, 0x93, 0x51, 0x55, 0x97, 0x61, 0x96, 0x9B,
                    0x71, 0xD7, 0x9F, 0x82, 0x18, 0xA3, 0x92, 0x59, 0xA7, 0xA2, 0x9A, 0xAB, 0xB2, 0xDB, 0xAF, 0xC3, 0x1C, 0xB3, 0xD3, 0x5D, 0xB7,
                    0xE3, 0x9E, 0xBB, 0xF3, 0xDF, 0xBF};
  const EncOut o = run_encode(std::vector<u8>(all, all + sizeof all), 64);
  REQUIRE(o.st == Base64Result::Ok);
  CHECK(chars_of(o) == "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
  // standard alphabet only: 0xFB 0xFF 0xBF is "+/+/" not "-_-_"
  const u8 sp[] = {0xFB, 0xEF, 0xBE};
  CHECK(chars_of(run_encode(std::vector<u8>(sp, sp + 3), 4)) == "++++");
  const u8 sl[] = {0xFF, 0xFF, 0xFF};
  CHECK(chars_of(run_encode(std::vector<u8>(sl, sl + 3), 4)) == "////");
}

TEST_CASE("base64: encode checks and the no-write rule") {
  const std::vector<u8> in(10, 4);
  char dst[16];
  std::memset(dst, kFill, sizeof dst);
  u32 len = kSent;
  auto untouched = [&]() {
    CHECK(len == kSent);
    for (char c : dst) CHECK(static_cast<u8>(c) == kFill);
  };
  CHECK(encode(in.data(), 10, dst, 16, nullptr) == Base64Result::NullArgument);
  CHECK(encode(nullptr, 10, dst, 16, &len) == Base64Result::NullArgument);
  CHECK(encode(in.data(), 10, nullptr, 16, &len) == Base64Result::NullArgument);
  CHECK(encode(nullptr, kMaxInput + 1, dst, 16, &len) == Base64Result::NullArgument);  // null before size
  CHECK(encode(in.data(), kMaxInput + 1, nullptr, 16, &len) == Base64Result::NullArgument);
  untouched();
  CHECK(encode(in.data(), kMaxInput + 1, dst, 0, &len) == Base64Result::TooLarge);  // before the capacity check, reads nothing
  CHECK(encode(in.data(), 0xFFFFFFFFu, dst, 16, &len) == Base64Result::TooLarge);
  CHECK(encode(in.data(), 10, nullptr, 0, &len) == Base64Result::BufferTooSmall);
  CHECK(encode(in.data(), 10, dst, 15, &len) == Base64Result::BufferTooSmall);  // needs 16
  CHECK(encode(in.data(), 1, dst, 3, &len) == Base64Result::BufferTooSmall);
  untouched();
  CHECK(encode(in.data(), 10, dst, 16, &len) == Base64Result::Ok);
  CHECK(len == 16u);
  CHECK(std::string(dst, 16) == "BAQEBAQEBAQEBA==");
  // empty input
  len = kSent;
  CHECK(encode(nullptr, 0, nullptr, 0, &len) == Base64Result::Ok);
  CHECK(len == 0u);
  std::memset(dst, kFill, sizeof dst);
  len = kSent;
  CHECK(encode(in.data(), 0, dst, 4, &len) == Base64Result::Ok);
  CHECK(len == 0u);
  CHECK(static_cast<u8>(dst[0]) == kFill);
}

TEST_CASE("base64: encode at the input limit needs exactly kMaxEncoded characters") {
  std::vector<u8> in(kMaxInput);
  for (u32 i = 0; i < kMaxInput; ++i) in[i] = static_cast<u8>(i * 7);
  std::vector<char> small(kMaxEncoded - 1, static_cast<char>(kFill));
  u32 len = kSent;
  CHECK(encode(in.data(), kMaxInput, small.data(), kMaxEncoded - 1, &len) == Base64Result::BufferTooSmall);
  CHECK(len == kSent);
  CHECK(static_cast<u8>(small[0]) == kFill);
  std::vector<char> dst(kMaxEncoded);
  REQUIRE(encode(in.data(), kMaxInput, dst.data(), kMaxEncoded, &len) == Base64Result::Ok);
  CHECK(len == kMaxEncoded);
  std::vector<u8> back(kMaxInput, kFill);
  u32 blen = kSent;
  REQUIRE(decode(dst.data(), kMaxEncoded, back.data(), kMaxInput, &blen) == Base64Result::Ok);
  CHECK(blen == kMaxInput);
  CHECK(back == in);
}

TEST_CASE("base64: decode check order") {
  u8 dst[8];
  std::memset(dst, kFill, sizeof dst);
  u32 len = kSent;
  auto untouched = [&]() {
    CHECK(len == kSent);
    for (u8 b : dst) CHECK(b == kFill);
  };
  const char* ok = "QUJD";
  CHECK(decode(ok, 4, dst, 8, nullptr) == Base64Result::NullArgument);
  CHECK(decode(nullptr, 4, dst, 8, &len) == Base64Result::NullArgument);
  CHECK(decode(ok, 4, nullptr, 8, &len) == Base64Result::NullArgument);
  CHECK(decode(nullptr, 5, dst, 8, &len) == Base64Result::NullArgument);  // null before length
  CHECK(decode(ok, 3, dst, 8, &len) == Base64Result::Malformed);
  CHECK(decode(ok, 5, dst, 0, &len) == Base64Result::Malformed);
  CHECK(decode(ok, kMaxEncoded + 1, dst, 8, &len) == Base64Result::Malformed);  // length before size, reads nothing
  CHECK(decode(ok, 0xFFFFFFFFu, dst, 8, &len) == Base64Result::Malformed);
  CHECK(decode(ok, kMaxEncoded + 4, dst, 8, &len) == Base64Result::TooLarge);  // before the scan, reads nothing
  CHECK(decode(ok, 0xFFFFFFFCu, dst, 8, &len) == Base64Result::TooLarge);
  CHECK(decode("QR==", 4, dst, 8, &len) == Base64Result::NonCanonical);
  CHECK(decode("QR==", 4, dst, 0, &len) == Base64Result::NonCanonical);  // before the capacity check
  CHECK(decode("QR==", 4, nullptr, 0, &len) == Base64Result::NonCanonical);
  CHECK(decode("QU-D", 4, nullptr, 0, &len) == Base64Result::Malformed);
  CHECK(decode(ok, 4, dst, 2, &len) == Base64Result::BufferTooSmall);
  CHECK(decode(ok, 4, nullptr, 0, &len) == Base64Result::BufferTooSmall);
  CHECK(decode("QQ==", 4, dst, 0, &len) == Base64Result::BufferTooSmall);
  untouched();
  CHECK(decode("QQ==", 4, dst, 1, &len) == Base64Result::Ok);
  CHECK(len == 1u);
  CHECK(dst[0] == 'A');
  CHECK(dst[1] == kFill);
  len = kSent;
  CHECK(decode(nullptr, 0, nullptr, 0, &len) == Base64Result::Ok);
  CHECK(len == 0u);
  len = kSent;
  CHECK(decode(ok, 0, dst, 8, &len) == Base64Result::Ok);
  CHECK(len == 0u);
}

TEST_CASE("base64: decode padding forms, trailing bits and the error priority") {
  struct Row {
    const char* s;
    Base64Result want;
  };
  const Row rows[] = {{"QQ==", Base64Result::Ok},          {"QUI=", Base64Result::Ok},          {"QUJD", Base64Result::Ok},
                      {"QR==", Base64Result::NonCanonical}, {"QUK=", Base64Result::NonCanonical}, {"QUJ=", Base64Result::NonCanonical},
                      {"+x==", Base64Result::NonCanonical}, {"+w==", Base64Result::Ok},           {"/+9=", Base64Result::NonCanonical},
                      {"/+8=", Base64Result::Ok},          {"AB==", Base64Result::NonCanonical}, {"AAC=", Base64Result::NonCanonical},
                      {"Q===", Base64Result::Malformed},   {"====", Base64Result::Malformed},    {"=QUJ", Base64Result::Malformed},
                      {"QU=J", Base64Result::Malformed},   {"QQ=A", Base64Result::Malformed},    {"Q=Q=", Base64Result::Malformed},
                      {"QQ==QQ==", Base64Result::Malformed}, {"QUJDQUI=", Base64Result::Ok},      {"QUJDQR==", Base64Result::NonCanonical},
                      {"QR==QUJD", Base64Result::Malformed}, {"QU-DQR==", Base64Result::Malformed}, {"QUJDQR=A", Base64Result::Malformed},
                      {"QUJDQUJD", Base64Result::Ok},      {"QUJDQUJ=", Base64Result::NonCanonical}, {"QUJDQUF=", Base64Result::NonCanonical}, {"QUJDQUE=", Base64Result::Ok}};
  for (const Row& r : rows) {
    const EncOut o = run_decode(r.s, 4 * static_cast<u32>(std::strlen(r.s) / 4), 64);
    CHECK(o.st == r.want);
  }
}

TEST_CASE("base64: every byte value at every position") {
  const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (u32 pos = 0; pos < 8; ++pos) {
    for (u32 c = 0; c < 256; ++c) {
      std::string s(8, 'A');  // all zero bits, so padding in the last group is canonical
      s[pos] = static_cast<char>(c);
      const EncOut o = run_decode_str(s, 6);
      const bool alpha = alphabet.find(static_cast<char>(c)) != std::string::npos && c != 0;
      if (alpha) {
        REQUIRE(o.st == Base64Result::Ok);
        REQUIRE(o.len == 6u);
      } else if (c == '=' && pos == 7) {
        REQUIRE(o.st == Base64Result::Ok);
        REQUIRE(o.len == 5u);
      } else {
        REQUIRE(o.st == Base64Result::Malformed);
      }
    }
  }
  // "AA==" and "AAA=" with a second padding or a bad character after it
  CHECK(run_decode_str("AA==", 3).st == Base64Result::Ok);
  CHECK(run_decode_str("AAA=", 3).st == Base64Result::Ok);
  CHECK(run_decode_str(std::string("AA=\0", 4), 3).st == Base64Result::Malformed);
  CHECK(run_decode_str(std::string("A\0==", 4), 3).st == Base64Result::Malformed);
}

TEST_CASE("base64: decode at the size limits") {
  std::string big(kMaxEncoded, 'A');
  std::vector<u8> dst(kMaxInput, kFill);
  u32 len = kSent;
  REQUIRE(decode(big.data(), kMaxEncoded, dst.data(), kMaxInput, &len) == Base64Result::Ok);
  CHECK(len == kMaxInput);
  CHECK(dst[0] == 0);
  CHECK(dst[kMaxInput - 1] == 0);
  std::vector<u8> tight(kMaxInput - 1, kFill);
  len = kSent;
  CHECK(decode(big.data(), kMaxEncoded, tight.data(), kMaxInput - 1, &len) == Base64Result::BufferTooSmall);
  CHECK(len == kSent);
  CHECK(tight[0] == kFill);
  // a bad character in the very last group is found after scanning everything before it
  big[kMaxEncoded - 1] = '-';
  CHECK(decode(big.data(), kMaxEncoded, dst.data(), kMaxInput, &len) == Base64Result::Malformed);
  big[kMaxEncoded - 1] = '=';
  big[kMaxEncoded - 2] = '=';
  len = kSent;
  REQUIRE(decode(big.data(), kMaxEncoded, dst.data(), kMaxInput, &len) == Base64Result::Ok);
  CHECK(len == kMaxInput - 2);
}

TEST_CASE("base64: structural oracle on random buffers") {
  const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  Lcg r(0xB64);
  for (int k = 0; k < 300; ++k) {
    const u32 n = r.rng(0, 200);
    const std::vector<u8> in = gen(0, n, r.next());
    const u32 need = 4 * ((n + 2) / 3);
    const EncOut e = run_encode(in, need);
    REQUIRE(e.st == Base64Result::Ok);
    REQUIRE(e.len == need);
    const std::string s = chars_of(e);
    const u32 pads = n % 3 == 0 ? 0 : 3 - n % 3;
    for (u32 i = 0; i < need; ++i) {
      if (i + pads >= need) REQUIRE(s[i] == '=');
      else REQUIRE(alphabet.find(s[i]) != std::string::npos);
    }
    const EncOut d = run_decode_str(s, n);
    REQUIRE(d.st == Base64Result::Ok);
    REQUIRE(d.dst == in);
    // flipping one trailing bit of a padded string is rejected as non-canonical
    if (pads == 1 || pads == 2) {
      std::string t = s;
      const u32 at = need - pads - 1;
      const int v = static_cast<int>(alphabet.find(t[at]));
      t[at] = alphabet[static_cast<std::size_t>(v ^ 1)];
      REQUIRE(run_decode_str(t, n + 3).st == Base64Result::NonCanonical);
    }
  }
}
