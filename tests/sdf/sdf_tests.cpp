// Tests for the exact integer SDF module (core/sdf).
//
// Expected values in the tables below were produced by an independent Python reference of the rules in docs/sdf.md (brute force
// over all set pixels, big-integer rounding) and pasted here as constants. The relation tests compare the implementation with a
// naive brute force written in this file, not with itself.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "sdf.h"

using namespace lumora::sdf;

namespace {

using Bytes = std::vector<std::uint8_t>;
using u64 = std::uint64_t;
constexpr u64 kBasis = 0xcbf29ce484222325ull;
constexpr std::uint8_t kSentinel = 0xA5;

struct Lcg {
  u64 s;
  std::uint32_t next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(s >> 32);
  }
};

// Test-local brute force: squared distance from every pixel to the nearest pixel with (mask >= thr) != invert.
std::vector<std::uint32_t> brute(const Bytes& m, std::uint32_t w, std::uint32_t h, int thr, bool invert) {
  std::vector<std::uint32_t> sx, sy;
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x)
      if ((m[y * w + x] >= thr) != invert) {
        sx.push_back(x);
        sy.push_back(y);
      }
  std::vector<std::uint32_t> out(static_cast<std::size_t>(w) * h, kNoSet);
  if (sx.empty()) return out;
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) {
      std::uint32_t best = kNoSet;
      for (std::size_t i = 0; i < sx.size(); ++i) {
        const std::int64_t dx = static_cast<std::int64_t>(x) - sx[i], dy = static_cast<std::int64_t>(y) - sy[i];
        const std::uint32_t d = static_cast<std::uint32_t>(dx * dx + dy * dy);
        if (d < best) best = d;
      }
      out[static_cast<std::size_t>(y) * w + x] = best;
    }
  return out;
}

// Mask shapes (mirrored by the Python reference).
Bytes shape(int kind, std::uint32_t w, std::uint32_t h) {
  Bytes m(static_cast<std::size_t>(w) * h, 0);
  const std::int64_t mn = static_cast<std::int64_t>(w < h ? w : h);
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::int64_t dx = static_cast<std::int64_t>(x) - w / 2, dy = static_cast<std::int64_t>(y) - h / 2;
      const std::int64_t d = dx * dx + dy * dy;
      std::uint32_t v = 0;
      switch (kind) {
        case 0: v = d < (mn / 3) * (mn / 3) ? 255 : 0; break;
        case 1: v = (d >= (mn / 4) * (mn / 4) && d <= (mn / 2 - 1) * (mn / 2 - 1)) ? 255 : 0; break;
        case 2: v = (((x / 4 + y / 8) % 2) == 1) ? 255 : 0; break;
        case 3: v = w > 1 ? (x * 255u) / (w - 1) : 0; break;
        case 4: v = (((x / 5 + y / 5) % 2) == 0) ? 255 : 0; break;
        case 5: v = (x == 0 || y == 0 || x == w - 1 || y == h - 1) ? 255 : 0; break;
        case 6: v = x == y ? 255 : 0; break;
        default: v = (x == w - 1 && y == h - 1) ? 255 : 0; break;
      }
      m[static_cast<std::size_t>(y) * w + x] = static_cast<std::uint8_t>(v);
    }
  return m;
}

u64 fnv_u32(const std::vector<std::uint32_t>& v) {
  u64 h = kBasis;
  for (std::uint32_t x : v)
    for (int k = 0; k < 4; ++k) h = (h ^ ((x >> (8 * k)) & 255u)) * 0x100000001b3ull;
  return h;
}

// Test-local encoding of the signed distance for one pixel (from the rules in docs/sdf.md).
std::uint8_t encode_ref(std::uint64_t d2, bool inside, std::uint32_t spread) {
  const u64 den = static_cast<u64>(spread) * 256u;
  const u64 q = d2 == kNoSet ? den : isqrt(d2 * 65536u);
  u64 r = 0;  // round(q * 127 / den), ties away from zero, by search
  while ((2 * r + 1) * den <= 2 * q * 127u) ++r;
  long v = 128 + (inside ? static_cast<long>(r) : -static_cast<long>(r));
  if (v < 1) v = 1;
  if (v > 255) v = 255;
  return static_cast<std::uint8_t>(v);
}

// Generated from the independent Python reference (see docs/sdf.md). Do not edit by hand.
static const std::uint8_t kDM0[] = {0,0,0,255,0,0,255,0};
static const std::uint32_t kDE0[] = {5,2,1,0,4,1,0,1};
static const std::uint8_t kDM1[] = {129,0,129,128,128,129,127,255,129,129,129,0,128,127,129,127,129,255};
static const std::uint32_t kDE1[] = {0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0};
static const std::uint8_t kDM2[] = {128,0,255,255,255,129,128,0,129,0};
static const std::uint32_t kDE2[] = {0,1,0,0,0,0,0,1,0,1};
static const std::uint8_t kDM3[] = {234,83};
static const std::uint32_t kDE3[] = {0,1};
static const std::uint8_t kDM4[] = {255,0,0};
static const std::uint32_t kDE4[] = {0,1,4};
static const std::uint8_t kDM5[] = {243,106,217};
static const std::uint32_t kDE5[] = {4294967295,4294967295,4294967295};
static const std::uint8_t kDM6[] = {0,0,0,0};
static const std::uint32_t kDE6[] = {4294967295,4294967295,4294967295,4294967295};
static const std::uint8_t kDM7[] = {255,255,0,0};
static const std::uint32_t kDE7[] = {0,0,1,1};
static const std::uint8_t kDM8[] = {127,169,219,197};
static const std::uint32_t kDE8[] = {1,0,0,0};
static const std::uint8_t kDM9[] = {100,115,148,223,103,85,106,8,225,255};
static const std::uint32_t kDE9[] = {1,1,0,0,1,1,1,1,0,0};
static const std::uint8_t kDM10[] = {144,210,100,25,85,183,158,28,92};
static const std::uint32_t kDE10[] = {4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295};
static const std::uint8_t kDM11[] = {128,126,247,108,63,113,162,195,188};
static const std::uint32_t kDE11[] = {0,0,0,0,0,0,0,0,0};
static const std::uint8_t kDM12[] = {128,129,0,255};
static const std::uint32_t kDE12[] = {0,0,1,0};
static const std::uint8_t kDM13[] = {65,18,102,62,21,166,46,143,36,6,68,173,81,103,86,2,231,46,86,31,58,10,181,58,78,120,105};
static const std::uint32_t kDE13[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const std::uint8_t kDM14[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const std::uint32_t kDE14[] = {4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295};
static const std::uint8_t kDM15[] = {255,255,255,255,255,255,255,255,255};
static const std::uint32_t kDE15[] = {0,0,0,0,0,0,0,0,0};
static const std::uint8_t kDM16[] = {0,0,0,0,255,0,0,0,0};
static const std::uint32_t kDE16[] = {2,1,2,1,0,1,2,1,2};
static const std::uint8_t kDM17[] = {255,0,0,0,0};
static const std::uint32_t kDE17[] = {0,1,4,9,16};
static const std::uint8_t kDM18[] = {127,128,129,0,255};
static const std::uint32_t kDE18[] = {1,0,0,1,0};
static const std::uint8_t kDM19[] = {0,0,255,0,0,0};
static const std::uint32_t kDE19[] = {4,1,0,1,4,9};
static const struct { std::uint32_t w, h; int thr; const std::uint8_t* mask; const std::uint32_t* exp; } kDist[] = {
  {4u, 2u, 128, kDM0, kDE0},
  {6u, 3u, 1, kDM1, kDE1},
  {2u, 5u, 1, kDM2, kDE2},
  {1u, 2u, 128, kDM3, kDE3},
  {1u, 3u, 255, kDM4, kDE4},
  {1u, 3u, 255, kDM5, kDE5},
  {4u, 1u, 128, kDM6, kDE6},
  {2u, 2u, 1, kDM7, kDE7},
  {4u, 1u, 129, kDM8, kDE8},
  {2u, 5u, 128, kDM9, kDE9},
  {9u, 1u, 255, kDM10, kDE10},
  {3u, 3u, 1, kDM11, kDE11},
  {4u, 1u, 128, kDM12, kDE12},
  {9u, 3u, 1, kDM13, kDE13},
  {5u, 4u, 129, kDM14, kDE14},
  {3u, 3u, 128, kDM15, kDE15},
  {3u, 3u, 1, kDM16, kDE16},
  {5u, 1u, 64, kDM17, kDE17},
  {5u, 1u, 128, kDM18, kDE18},
  {1u, 6u, 64, kDM19, kDE19},
};
static const std::uint8_t kSE0[] = {1,1,1,255,1,1,255,1};
static const std::uint8_t kSE1[] = {160,96,160,192,173,160,96,160,173,160,160,96,160,96,160,96,160,160};
static const std::uint8_t kSE2[] = {129,127,129,129,129,129,129,127,129,127};
static const std::uint8_t kSE3[] = {142,114};
static const std::uint8_t kSE4[] = {255,1,1};
static const std::uint8_t kSE5[] = {192,64,192};
static const std::uint8_t kSE6[] = {1,1,1,1};
static const std::uint8_t kSE7[] = {192,192,64,64};
static const std::uint8_t kSE8[] = {127,129,130,131};
static const std::uint8_t kSE9[] = {127,127,129,129,127,127,127,127,129,129};
static const std::uint8_t kSE10[] = {255,255,1,1,1,255,255,1,1};
static const std::uint8_t kSE11[] = {160,96,160,96,96,96,160,160,160};
static const std::uint8_t kSE12[] = {130,129,127,129};
static const std::uint8_t kSE13[] = {126,127,127,127,127,129,127,129,127,126,127,129,127,127,127,127,129,127,126,127,127,127,129,127,127,127,127};
static const std::uint8_t kSE14[] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static const std::uint8_t kSE15[] = {255,255,255,255,255,255,255,255,255};
static const std::uint8_t kSE16[] = {1,1,1,1,255,1,1,1,1};
static const std::uint8_t kSE17[] = {192,64,1,1,1};
static const std::uint8_t kSE18[] = {1,255,255,1,255};
static const std::uint8_t kSE19[] = {43,86,170,86,43,1};
static const struct { std::uint32_t w, h, spread; const std::uint8_t* mask; const std::uint8_t* exp; } kSdf[] = {
  {4u, 2u, 1u, kDM0, kSE0},
  {6u, 3u, 4u, kDM1, kSE1},
  {2u, 5u, 127u, kDM2, kSE2},
  {1u, 2u, 9u, kDM3, kSE3},
  {1u, 3u, 1u, kDM4, kSE4},
  {1u, 3u, 2u, kDM5, kSE5},
  {4u, 1u, 127u, kDM6, kSE6},
  {2u, 2u, 2u, kDM7, kSE7},
  {4u, 1u, 127u, kDM8, kSE8},
  {2u, 5u, 127u, kDM9, kSE9},
  {9u, 1u, 1u, kDM10, kSE10},
  {3u, 3u, 4u, kDM11, kSE11},
  {4u, 1u, 127u, kDM12, kSE12},
  {9u, 3u, 127u, kDM13, kSE13},
  {5u, 4u, 127u, kDM14, kSE14},
  {3u, 3u, 1u, kDM15, kSE15},
  {3u, 3u, 1u, kDM16, kSE16},
  {5u, 1u, 2u, kDM17, kSE17},
  {5u, 1u, 1u, kDM18, kSE18},
  {1u, 6u, 3u, kDM19, kSE19},
};
static const std::uint64_t kCovDigest = 13743694759186110030ull;
static const std::uint8_t kCovSamples[][3] = {{0,1,0},{0,2,0},{0,3,0},{0,64,0},{0,127,0},{1,1,0},{1,2,0},{1,3,0},{1,64,0},{1,127,0},{127,1,0},{127,2,63},{127,3,85},{127,64,125},{127,127,126},{128,1,127},{128,2,127},{128,3,127},{128,64,127},{128,127,127},{129,1,255},{129,2,191},{129,3,170},{129,64,129},{129,127,128},{200,1,255},{200,2,255},{200,3,255},{200,64,255},{200,127,199},{255,1,255},{255,2,255},{255,3,255},{255,64,255},{255,127,255}};
static const struct { int kind; std::uint32_t w, h, spread; std::uint64_t dist_digest, sdf_digest; } kGold[] = {
  {0, 64u, 64u, 1u, 9061214891431998486ull, 6164440177844274115ull},
  {0, 64u, 64u, 8u, 9061214891431998486ull, 1428297026368603867ull},
  {1, 64u, 48u, 6u, 5274170459843614774ull, 11037966199960627187ull},
  {1, 48u, 64u, 127u, 9901439568968259286ull, 12835232566326267290ull},
  {2, 60u, 40u, 3u, 5969189269644223717ull, 11723198418726795077ull},
  {3, 50u, 20u, 10u, 519307208960787173ull, 7583801299217426757ull},
  {4, 45u, 45u, 4u, 6625816341551578613ull, 5905777638998414070ull},
  {5, 30u, 20u, 5u, 14586779332095506277ull, 10768482033310756929ull},
  {6, 40u, 40u, 2u, 10522283314203910665ull, 17322478624477420295ull},
  {7, 33u, 17u, 127u, 13125478426388406050ull, 15508018853785031077ull},
  {0, 1u, 50u, 4u, 12761725431498649213ull, 7364158041744805863ull},
  {2, 50u, 1u, 4u, 16341051089537741356ull, 16302502538498630743ull},
};
static const std::uint64_t kPerfDist = 13625336600990385794ull;
static const std::uint64_t kPerfSdf = 17252063229295295759ull;

}  // namespace

TEST_CASE("status_text", "[sdf][status]") {
  REQUIRE(std::string_view(status_text(Status::Ok)) == "ok");
  REQUIRE(std::string_view(status_text(Status::NullBuffer)) == "null buffer");
  REQUIRE(std::string_view(status_text(Status::BadSize)) == "bad size");
  REQUIRE(std::string_view(status_text(Status::BadSpread)) == "bad spread");
  REQUIRE(std::string_view(status_text(Status::ScratchTooSmall)) == "scratch too small");
  REQUIRE(std::string_view(status_text(Status::Overlap)) == "overlap");
  REQUIRE(std::string_view(status_text(static_cast<Status>(99))) == "unknown");
}

TEST_CASE("isqrt is exact", "[sdf][isqrt]") {
  // Exhaustive 0 .. 2^20.
  std::uint32_t r = 0;
  for (std::uint32_t v = 0; v <= (1u << 20); ++v) {
    while (static_cast<u64>(r + 1) * (r + 1) <= v) ++r;
    REQUIRE(isqrt(v) == r);
  }
  // Squares and neighbours up to the largest 32-bit root, plus the extreme.
  const std::uint64_t ks[] = {1, 2, 3, 255, 256, 4095, 65535, 65536, 1000003, 2147483647ull, 2147483648ull, 4294967294ull, 4294967295ull};
  for (std::uint64_t k : ks) {
    const std::uint64_t sq = k * k;
    REQUIRE(isqrt(sq) == k);
    REQUIRE(isqrt(sq + 1) == k);
    REQUIRE(isqrt(sq - 1) == k - 1);
    if (k < 4294967295ull) REQUIRE(isqrt(sq + 2 * k) == k);
  }
  REQUIRE(isqrt(0xFFFFFFFFFFFFFFFFull) == 4294967295u);
  REQUIRE(isqrt(0xFFFFFFFFFFFFFFFEull) == 4294967295u);
  REQUIRE(isqrt(4294967295ull * 4294967295ull) == 4294967295u);
  REQUIRE(isqrt(4294967295ull * 4294967295ull - 1) == 4294967294u);
  // Large pseudo-random values against the definition r*r <= v < (r+1)^2 (in 128-safe form).
  Lcg g{3};
  for (int i = 0; i < 20000; ++i) {
    const u64 v = (static_cast<u64>(g.next()) << 32) | g.next();
    const u64 r2 = isqrt(v);
    REQUIRE(r2 * r2 <= v);
    REQUIRE(((r2 == 4294967295ull) || ((r2 + 1) * (r2 + 1) > v)));
  }
}

TEST_CASE("distance_squared matches the reference tables", "[sdf][tables]") {
  for (const auto& c : kDist) {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(c.w) * c.h, 7u);
    REQUIRE(distance_squared(c.mask, c.w, c.h, c.w, static_cast<std::uint8_t>(c.thr), out.data()) == Status::Ok);
    const std::vector<std::uint32_t> want(c.exp, c.exp + out.size());
    REQUIRE(out == want);
  }
}

TEST_CASE("distance_squared equals brute force on random and structured masks", "[sdf][brute]") {
  Lcg g{20261126};
  for (int it = 0; it < 400; ++it) {
    const std::uint32_t w = 1 + g.next() % 40, h = 1 + g.next() % 40;
    Bytes m(static_cast<std::size_t>(w) * h);
    const int density = static_cast<int>(g.next() % 5);
    for (auto& b : m) {
      const std::uint32_t r = g.next();
      b = static_cast<std::uint8_t>(density == 0 ? (r % 97 == 0 ? 255 : 0) : density == 1 ? (r % 7 == 0 ? 200 : 10) : density == 2 ? r : density == 3 ? (r % 2 ? 255 : 0) : (r % 300 == 0 ? 255 : 0));
    }
    const int thr = static_cast<int>(g.next() % 256u);
    std::vector<std::uint32_t> out(m.size());
    REQUIRE(distance_squared(m.data(), w, h, w, static_cast<std::uint8_t>(thr), out.data()) == Status::Ok);
    REQUIRE(out == brute(m, w, h, thr, false));
  }
  // Structured shapes, including non-square ones and 1 x N / N x 1 strips, up to 200.
  const std::uint32_t dims[][2] = {{1, 200}, {200, 1}, {2, 150}, {150, 2}, {64, 64}, {97, 131}, {200, 199}, {1, 1}, {7, 3}};
  for (const auto& d : dims) {
    for (int k = 0; k < 8; ++k) {
      if (d[0] * d[1] > 5000u && k < 5) continue;  // dense shapes at large sizes make the naive brute force too slow
      const Bytes m = shape(k, d[0], d[1]);
      std::vector<std::uint32_t> out(m.size());
      REQUIRE(distance_squared(m.data(), d[0], d[1], d[0], 128, out.data()) == Status::Ok);
      REQUIRE(out == brute(m, d[0], d[1], 128, false));
    }
  }
}

TEST_CASE("empty, full, single, line, checker and border masks", "[sdf][special]") {
  const std::uint32_t w = 9, h = 7;
  std::vector<std::uint32_t> out(static_cast<std::size_t>(w) * h);
  // Empty: every value is kNoSet.
  Bytes m(w * h, 0);
  REQUIRE(distance_squared(m.data(), w, h, w, 128, out.data()) == Status::Ok);
  for (std::uint32_t v : out) REQUIRE(v == kNoSet);
  // Full: every value is 0.
  m.assign(w * h, 255);
  REQUIRE(distance_squared(m.data(), w, h, w, 128, out.data()) == Status::Ok);
  for (std::uint32_t v : out) REQUIRE(v == 0u);
  // Single pixel: x^2 + y^2 from the pixel.
  m.assign(w * h, 0);
  m[3 * w + 4] = 255;
  REQUIRE(distance_squared(m.data(), w, h, w, 128, out.data()) == Status::Ok);
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) {
      const int dx = static_cast<int>(x) - 4, dy = static_cast<int>(y) - 3;
      REQUIRE(out[y * w + x] == static_cast<std::uint32_t>(dx * dx + dy * dy));
    }
  // Hand-computed 3 x 3 with the centre set: 2 1 2 / 1 0 1 / 2 1 2.
  const Bytes c3 = {0, 0, 0, 0, 255, 0, 0, 0, 0};
  std::vector<std::uint32_t> o3(9);
  REQUIRE(distance_squared(c3.data(), 3, 3, 3, 128, o3.data()) == Status::Ok);
  const std::vector<std::uint32_t> w3 = {2, 1, 2, 1, 0, 1, 2, 1, 2};
  REQUIRE(o3 == w3);
  // Hand-computed 1 x 5 with the first pixel set: 0 1 4 9 16.
  const Bytes row = {255, 0, 0, 0, 0};
  std::vector<std::uint32_t> o5(5);
  REQUIRE(distance_squared(row.data(), 5, 1, 5, 128, o5.data()) == Status::Ok);
  const std::vector<std::uint32_t> w5 = {0, 1, 4, 9, 16};
  REQUIRE(o5 == w5);
  // Largest values: one set pixel in the opposite corner of a 4096 x 4096 mask would give 2 * 4095^2 (checked in the large test).
  REQUIRE(2u * 4095u * 4095u < kNoSet);
}

TEST_CASE("threshold is inclusive", "[sdf][threshold]") {
  const Bytes m = {126, 127, 128, 129, 255, 0};
  std::vector<std::uint32_t> o(6);
  // threshold 128: pixels 128, 129 and 255 are set.
  REQUIRE(distance_squared(m.data(), 6, 1, 6, 128, o.data()) == Status::Ok);
  const std::vector<std::uint32_t> a = {4, 1, 0, 0, 0, 1};
  REQUIRE(o == a);
  // threshold 129: 128 is no longer set.
  REQUIRE(distance_squared(m.data(), 6, 1, 6, 129, o.data()) == Status::Ok);
  const std::vector<std::uint32_t> b = {9, 4, 1, 0, 0, 1};
  REQUIRE(o == b);
  // threshold 255: only the last full pixel.
  REQUIRE(distance_squared(m.data(), 6, 1, 6, 255, o.data()) == Status::Ok);
  const std::vector<std::uint32_t> c = {16, 9, 4, 1, 0, 1};
  REQUIRE(o == c);
  // threshold 0: every pixel is set.
  REQUIRE(distance_squared(m.data(), 6, 1, 6, 0, o.data()) == Status::Ok);
  for (std::uint32_t v : o) REQUIRE(v == 0u);
}

TEST_CASE("make_sdf matches the reference tables", "[sdf][tables]") {
  for (const auto& c : kSdf) {
    const std::size_t n = static_cast<std::size_t>(c.w) * c.h;
    Bytes out(n, kSentinel);
    std::vector<std::uint32_t> scratch(n);
    REQUIRE(make_sdf(c.mask, c.w, c.h, c.w, out.data(), c.w, c.spread, scratch.data(), scratch.size()) == Status::Ok);
    const Bytes want(c.exp, c.exp + n);
    REQUIRE(out == want);
  }
}

TEST_CASE("make_sdf against the local encoding rule and brute force", "[sdf][brute]") {
  Lcg g{91};
  for (int it = 0; it < 120; ++it) {
    const std::uint32_t w = 1 + g.next() % 30, h = 1 + g.next() % 30, spread = 1 + g.next() % 127;
    Bytes m(static_cast<std::size_t>(w) * h);
    const std::uint32_t mode = g.next() % 3;
    for (auto& b : m) {
      const std::uint32_t r = g.next();
      b = static_cast<std::uint8_t>(mode == 0 ? r : mode == 1 ? (r % 11 == 0 ? 255 : 0) : (r % 11 == 0 ? 0 : 255));
    }
    Bytes out(m.size(), kSentinel);
    std::vector<std::uint32_t> scratch(m.size());
    REQUIRE(make_sdf(m.data(), w, h, w, out.data(), w, spread, scratch.data(), scratch.size()) == Status::Ok);
    const auto dout = brute(m, w, h, 128, false);  // distance to the nearest inside pixel
    const auto din = brute(m, w, h, 128, true);    // distance to the nearest outside pixel
    for (std::size_t i = 0; i < m.size(); ++i) {
      const bool inside = m[i] >= 128;
      REQUIRE(out[i] == encode_ref(inside ? din[i] : dout[i], inside, spread));
      REQUIRE(out[i] != 128);                       // the edge value is never produced
      if (inside) REQUIRE(out[i] > 128); else REQUIRE(out[i] < 128);
    }
  }
}

TEST_CASE("spread 1 and 127, absent classes, edge-adjacent pixels", "[sdf][spread]") {
  // 1 x 4: outside, inside, inside, outside. Every pixel is at distance 1 from the other class (or 2 for the middle ones are 1).
  const Bytes m = {0, 255, 255, 0};
  Bytes out(4, 0);
  std::vector<std::uint32_t> sc(4);
  REQUIRE(make_sdf(m.data(), 4, 1, 4, out.data(), 4, 1, sc.data(), 4) == Status::Ok);
  // spread 1: distance 1 pixel = 256 = spread * 256, so r = 127: inside 255, outside 1.
  const Bytes s1 = {1, 255, 255, 1};
  REQUIRE(out == s1);
  REQUIRE(make_sdf(m.data(), 4, 1, 4, out.data(), 4, 127, sc.data(), 4) == Status::Ok);
  // spread 127: round(256 * 127 / (127 * 256)) = 1: inside 129, outside 127.
  const Bytes s127 = {127, 129, 129, 127};
  REQUIRE(out == s127);
  // Minimum encoded magnitude is round(127 / spread) for every spread; edge-adjacent pixels use it.
  for (std::uint32_t sp = 1; sp <= 127; ++sp) {
    REQUIRE(make_sdf(m.data(), 4, 1, 4, out.data(), 4, sp, sc.data(), 4) == Status::Ok);
    const std::uint32_t r = (2u * 127u + sp) / (2u * sp);
    REQUIRE(out[1] == 128 + r);
    REQUIRE(out[0] == 128 - r);
  }
  // Absent opposite class: no inside pixel at all gives 1 everywhere; no outside pixel gives 255 everywhere.
  Bytes allout(25, 127), allin(25, 128), o25(25, 0);
  std::vector<std::uint32_t> sc25(25);
  REQUIRE(make_sdf(allout.data(), 5, 5, 5, o25.data(), 5, 9, sc25.data(), 25) == Status::Ok);
  for (std::uint8_t v : o25) REQUIRE(v == 1);
  REQUIRE(make_sdf(allin.data(), 5, 5, 5, o25.data(), 5, 9, sc25.data(), 25) == Status::Ok);
  for (std::uint8_t v : o25) REQUIRE(v == 255);
  // Mask 128 is inside and 127 is outside.
  const Bytes edge = {127, 128};
  Bytes oe(2);
  std::vector<std::uint32_t> sce(2);
  REQUIRE(make_sdf(edge.data(), 2, 1, 2, oe.data(), 2, 4, sce.data(), 2) == Status::Ok);
  REQUIRE(oe[0] < 128);
  REQUIRE(oe[1] > 128);
}

TEST_CASE("inverted mask is symmetric around 128", "[sdf][symmetry]") {
  Lcg g{17};
  for (int it = 0; it < 60; ++it) {
    const std::uint32_t w = 1 + g.next() % 25, h = 1 + g.next() % 25, spread = 1 + g.next() % 127;
    Bytes m(static_cast<std::size_t>(w) * h), inv(m.size());
    for (std::size_t i = 0; i < m.size(); ++i) {
      m[i] = static_cast<std::uint8_t>(g.next());
      inv[i] = static_cast<std::uint8_t>(255 - m[i]);  // inside (>= 128) becomes outside (<= 127) exactly
    }
    Bytes a(m.size()), b(m.size());
    std::vector<std::uint32_t> sc(m.size());
    REQUIRE(make_sdf(m.data(), w, h, w, a.data(), w, spread, sc.data(), sc.size()) == Status::Ok);
    REQUIRE(make_sdf(inv.data(), w, h, w, b.data(), w, spread, sc.data(), sc.size()) == Status::Ok);
    // Rounding is ties away from zero (symmetric) and the clamp 1..255 is symmetric, so the pair sums to 256.
    for (std::size_t i = 0; i < m.size(); ++i) REQUIRE(static_cast<int>(a[i]) + static_cast<int>(b[i]) == 256);
  }
}

TEST_CASE("edge_coverage", "[sdf][coverage]") {
  for (const auto& s : kCovSamples) REQUIRE(edge_coverage(s[0], s[1]) == s[2]);
  // Local formula, independent of the implementation: t = (v - 128) * 255 + aa * 255, c = floor(t / (2 aa)), clamp 0..255.
  const std::uint32_t aas[] = {1, 2, 3, 7, 64, 126, 127};
  for (std::uint32_t aa : aas) {
    for (int v = 0; v < 256; ++v) {
      const long t = (v - 128) * 255L + static_cast<long>(aa) * 255L;
      long c = 0;
      if (t >= 0) c = t / (2L * aa);
      else c = -((-t + 2L * aa - 1) / (2L * aa));
      if (c < 0) c = 0;
      if (c > 255) c = 255;
      REQUIRE(edge_coverage(static_cast<std::uint8_t>(v), aa) == c);
    }
  }
  // aa outside 1..127 is a hard threshold at 128.
  const std::uint32_t bad[] = {0, 128, 1000, 0xFFFFFFFFu};
  for (std::uint32_t aa : bad)
    for (int v = 0; v < 256; ++v) REQUIRE(edge_coverage(static_cast<std::uint8_t>(v), aa) == (v < 128 ? 0 : 255));
  // Whole matrix digest from the Python reference (aa = 0, 1, 2, 3, 7, 64, 126, 127, 128, 1000, UINT32_MAX).
  const std::uint32_t all[] = {0, 1, 2, 3, 7, 64, 126, 127, 128, 1000, 0xFFFFFFFFu};
  Bytes buf;
  for (std::uint32_t aa : all)
    for (int v = 0; v < 256; ++v) buf.push_back(edge_coverage(static_cast<std::uint8_t>(v), aa));
  REQUIRE(digest_image(buf.data(), static_cast<std::uint32_t>(buf.size()), 1, static_cast<std::uint32_t>(buf.size())) == kCovDigest);
  // Monotone in v for every aa.
  for (std::uint32_t aa = 1; aa <= 127; ++aa) {
    int prev = 0;
    for (int v = 0; v < 256; ++v) {
      const int c = edge_coverage(static_cast<std::uint8_t>(v), aa);
      REQUIRE(c >= prev);
      prev = c;
    }
  }
}

TEST_CASE("digest_image", "[sdf][digest]") {
  REQUIRE(digest_image(nullptr, 4, 4, 4) == kBasis);
  const Bytes px = {1, 2, 3, 4};
  REQUIRE(digest_image(px.data(), 0, 1, 4) == kBasis);
  REQUIRE(digest_image(px.data(), 4, 0, 4) == kBasis);
  const std::uint8_t a = 'a';
  REQUIRE(digest_image(&a, 1, 1, 1) == 0xaf63dc4c8601ec8cull);
  const Bytes pad = {1, 2, 9, 3, 4, 9};
  const Bytes tight = {1, 2, 3, 4};
  REQUIRE(digest_image(pad.data(), 2, 2, 3) == digest_image(tight.data(), 2, 2, 2));
}

TEST_CASE("distance_squared statuses and overlap", "[sdf][status]") {
  Bytes m(64, 255);
  std::vector<std::uint32_t> out(64, 5u);
  REQUIRE(distance_squared(m.data(), 8, 8, 8, 128, out.data()) == Status::Ok);
  REQUIRE(distance_squared(nullptr, 0, 0, 0, 128, out.data()) == Status::NullBuffer);
  REQUIRE(distance_squared(m.data(), 0, 0, 0, 128, nullptr) == Status::NullBuffer);
  REQUIRE(distance_squared(m.data(), 0, 8, 8, 128, out.data()) == Status::BadSize);
  REQUIRE(distance_squared(m.data(), 8, 0, 8, 128, out.data()) == Status::BadSize);
  REQUIRE(distance_squared(m.data(), 4097, 1, 4097, 128, out.data()) == Status::BadSize);
  REQUIRE(distance_squared(m.data(), 1, 4097, 1, 128, out.data()) == Status::BadSize);
  REQUIRE(distance_squared(m.data(), 8, 8, 7, 128, out.data()) == Status::BadSize);
  // Nothing is written on error.
  std::vector<std::uint32_t> keep(64, 0x1234u);
  REQUIRE(distance_squared(m.data(), 8, 8, 7, 128, keep.data()) == Status::BadSize);
  for (std::uint32_t v : keep) REQUIRE(v == 0x1234u);
  // Overlap: the output extent is width * height * 4 contiguous bytes. Mask rows after it do not overlap; inside it they do.
  std::vector<std::uint32_t> big(4096, 0);
  std::uint8_t* base = reinterpret_cast<std::uint8_t*>(big.data());
  REQUIRE(distance_squared(base + 8 * 8 * 4, 8, 8, 8, 128, big.data()) == Status::Ok);        // mask starts right after the output
  REQUIRE(distance_squared(base + 8 * 8 * 4 - 1, 8, 8, 8, 128, big.data()) == Status::Overlap);
  REQUIRE(distance_squared(base + 100, 8, 8, 8, 128, big.data()) == Status::Overlap);
  REQUIRE(distance_squared(reinterpret_cast<std::uint8_t*>(big.data()), 8, 8, 8, 128, big.data()) == Status::Overlap);
  // A sparse mask (stride 400, 8 rows of 8 bytes). The output extent is contiguous, so it can hit a row even when it starts in a gap.
  std::vector<std::uint32_t> arena(2048, 0);
  std::uint8_t* ab = reinterpret_cast<std::uint8_t*>(arena.data());
  REQUIRE(distance_squared(ab, 8, 8, 400, 128, arena.data() + 2) == Status::Ok);      // output [8, 264) fits before row 1 at 400
  REQUIRE(distance_squared(ab, 8, 8, 400, 128, arena.data() + 40) == Status::Overlap);  // output [160, 416) reaches row 1 [400, 408)
  REQUIRE(distance_squared(ab, 8, 8, 400, 128, arena.data() + 704) == Status::Ok);    // output starts at 2816, after the last row (ends 2808)
  REQUIRE(distance_squared(ab, 8, 8, 400, 128, arena.data() + 700) == Status::Overlap);  // output starts at 2800, inside the last row
}

TEST_CASE("make_sdf statuses, order, padding and overlap", "[sdf][status]") {
  const std::uint32_t w = 8, h = 8;
  Bytes mask(w * h, 0);
  mask[27] = 255;
  Bytes out(w * h, kSentinel);
  std::vector<std::uint32_t> sc(w * h);
  REQUIRE(make_sdf(mask.data(), w, h, w, out.data(), w, 4, sc.data(), sc.size()) == Status::Ok);
  // NullBuffer for each pointer, before everything else.
  REQUIRE(make_sdf(nullptr, 0, 0, 0, out.data(), 0, 0, sc.data(), 0) == Status::NullBuffer);
  REQUIRE(make_sdf(mask.data(), 0, 0, 0, nullptr, 0, 0, sc.data(), 0) == Status::NullBuffer);
  REQUIRE(make_sdf(mask.data(), 0, 0, 0, out.data(), 0, 0, nullptr, 0) == Status::NullBuffer);
  // BadSize (any strides included) before BadSpread.
  REQUIRE(make_sdf(mask.data(), 0, h, w, out.data(), w, 0, sc.data(), 0) == Status::BadSize);
  REQUIRE(make_sdf(mask.data(), w, 0, w, out.data(), w, 0, sc.data(), 0) == Status::BadSize);
  REQUIRE(make_sdf(mask.data(), 4097, 1, 4097, out.data(), 4097, 0, sc.data(), 0) == Status::BadSize);
  REQUIRE(make_sdf(mask.data(), 1, 4097, 1, out.data(), 1, 0, sc.data(), 0) == Status::BadSize);
  REQUIRE(make_sdf(mask.data(), w, h, w - 1, out.data(), w, 0, sc.data(), 0) == Status::BadSize);
  REQUIRE(make_sdf(mask.data(), w, h, w, out.data(), w - 1, 0, sc.data(), 0) == Status::BadSize);
  // BadSpread before ScratchTooSmall.
  REQUIRE(make_sdf(mask.data(), w, h, w, out.data(), w, 0, sc.data(), 0) == Status::BadSpread);
  REQUIRE(make_sdf(mask.data(), w, h, w, out.data(), w, 128, sc.data(), 0) == Status::BadSpread);
  // ScratchTooSmall before Overlap, with the exact boundary.
  REQUIRE(make_sdf(mask.data(), w, h, w, out.data(), w, 4, sc.data(), w * h - 1) == Status::ScratchTooSmall);
  REQUIRE(make_sdf(mask.data(), w, h, w, mask.data(), w, 4, sc.data(), 0) == Status::ScratchTooSmall);
  REQUIRE(make_sdf(mask.data(), w, h, w, out.data(), w, 4, sc.data(), w * h) == Status::Ok);
  // Overlap last: out aliases the mask.
  REQUIRE(make_sdf(mask.data(), w, h, w, mask.data(), w, 4, sc.data(), sc.size()) == Status::Overlap);
  // Nothing is written on error.
  Bytes keep(w * h, kSentinel);
  REQUIRE(make_sdf(mask.data(), w, h, w, keep.data(), w, 0, sc.data(), sc.size()) == Status::BadSpread);
  for (std::uint8_t v : keep) REQUIRE(v == kSentinel);
  // Output stride padding is never written.
  const std::uint32_t os = 13;
  Bytes padded(os * h, kSentinel);
  REQUIRE(make_sdf(mask.data(), w, h, w, padded.data(), os, 4, sc.data(), sc.size()) == Status::Ok);
  Bytes tight(w * h);
  REQUIRE(make_sdf(mask.data(), w, h, w, tight.data(), w, 4, sc.data(), sc.size()) == Status::Ok);
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < os; ++x) REQUIRE(padded[y * os + x] == (x < w ? tight[y * w + x] : kSentinel));
  // Mask stride padding is respected (the padding bytes are not read as pixels).
  Bytes pm(os * h, 255);
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) pm[y * os + x] = mask[y * w + x];
  Bytes viapad(w * h);
  REQUIRE(make_sdf(pm.data(), w, h, os, viapad.data(), w, 4, sc.data(), sc.size()) == Status::Ok);
  REQUIRE(viapad == tight);
  // Scratch and the other buffers: any two of mask rows, out rows and scratch bytes that touch give Overlap.
  std::vector<std::uint32_t> arena(4096, 0);
  std::uint8_t* base = reinterpret_cast<std::uint8_t*>(arena.data());
  std::uint32_t* scr = arena.data();                       // 256 bytes at the start
  std::uint8_t* maskp = base + w * h * 4;                  // right after the scratch: no overlap
  std::uint8_t* outp = base + w * h * 4 + w * h;           // right after the mask
  for (std::uint32_t i = 0; i < w * h; ++i) maskp[i] = mask[i];
  REQUIRE(make_sdf(maskp, w, h, w, outp, w, 4, scr, w * h) == Status::Ok);
  REQUIRE(make_sdf(maskp - 1, w, h, w, outp, w, 4, scr, w * h) == Status::Overlap);        // scratch / mask by one byte
  REQUIRE(make_sdf(maskp, w, h, w, outp - 1, w, 4, scr, w * h) == Status::Overlap);        // out / mask by one byte
  REQUIRE(make_sdf(maskp, w, h, w, base + 100, w, 4, scr, w * h) == Status::Overlap);      // out inside the scratch
  REQUIRE(make_sdf(maskp, w, h, w, outp, w, 4, scr + 1, w * h) == Status::Overlap);        // scratch runs into the mask
  // Rows interleaved in one buffer with a large stride: no intersection, so Ok.
  Bytes inter(4096, 0);
  std::vector<std::uint32_t> sc2(w * h);
  for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) inter[y * 32 + x] = mask[y * w + x];
  REQUIRE(make_sdf(inter.data(), w, h, 32, inter.data() + 16, 32, 4, sc2.data(), sc2.size()) == Status::Ok);
  REQUIRE(make_sdf(inter.data(), w, h, 32, inter.data() + 4, 32, 4, sc2.data(), sc2.size()) == Status::Overlap);
}

TEST_CASE("golden digests", "[sdf][golden]") {
  for (const auto& c : kGold) {
    const Bytes m = shape(c.kind, c.w, c.h);
    std::vector<std::uint32_t> d(m.size());
    REQUIRE(distance_squared(m.data(), c.w, c.h, c.w, 128, d.data()) == Status::Ok);
    REQUIRE(fnv_u32(d) == c.dist_digest);
    Bytes out(m.size());
    std::vector<std::uint32_t> sc(m.size());
    REQUIRE(make_sdf(m.data(), c.w, c.h, c.w, out.data(), c.w, c.spread, sc.data(), sc.size()) == Status::Ok);
    REQUIRE(digest_image(out.data(), c.w, c.h, c.w) == c.sdf_digest);
  }
}

TEST_CASE("1024-square surface (runs under sanitizers)", "[sdf][perf]") {
  const std::uint32_t n = 1024;
  const Bytes m = shape(0, n, n);
  std::vector<std::uint32_t> d(m.size());
  REQUIRE(distance_squared(m.data(), n, n, n, 128, d.data()) == Status::Ok);
  REQUIRE(fnv_u32(d) == kPerfDist);
  Bytes out(m.size());
  std::vector<std::uint32_t> sc(m.size());
  REQUIRE(make_sdf(m.data(), n, n, n, out.data(), n, 20, sc.data(), sc.size()) == Status::Ok);
  REQUIRE(digest_image(out.data(), n, n, n) == kPerfSdf);
}

#if !defined(__SANITIZE_ADDRESS__)
// Not run under AddressSanitizer: the sanitized build covers the 1024-square case above. Evidence label: unsanitized only.
TEST_CASE("4096-square surface (unsanitized)", "[sdf][perf][large]") {
  const std::uint32_t n = 4096;
  Bytes m(static_cast<std::size_t>(n) * n, 0);
  m[0] = 255;  // a single set pixel in the top-left corner
  std::vector<std::uint32_t> d(m.size());
  REQUIRE(distance_squared(m.data(), n, n, n, 128, d.data()) == Status::Ok);
  for (std::uint32_t y = 0; y < n; ++y)
    for (std::uint32_t x = 0; x < n; ++x) REQUIRE(d[static_cast<std::size_t>(y) * n + x] == x * x + y * y);
  REQUIRE(d.back() == 2u * 4095u * 4095u);  // the largest possible squared distance
  Bytes out(m.size());
  REQUIRE(make_sdf(m.data(), n, n, n, out.data(), n, 127, d.data(), d.size()) == Status::Ok);
  REQUIRE(out[0] > 128);  // the only inside pixel
  for (std::uint32_t y = 0; y < n; y += 37)
    for (std::uint32_t x = 0; x < n; x += 41) {
      if (x == 0 && y == 0) continue;
      REQUIRE(out[static_cast<std::size_t>(y) * n + x] == encode_ref(static_cast<u64>(x) * x + static_cast<u64>(y) * y, false, 127));
    }
  // The one inside pixel: its nearest outside pixel is 1 pixel away.
  REQUIRE(out[0] == encode_ref(1, true, 127));
}
#endif
