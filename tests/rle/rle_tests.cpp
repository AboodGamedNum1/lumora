// Tests for core/rle (scope AJ). Expected values come from an independent Python reference (tools/rle_ref.py: it cuts runs with
// itertools.groupby, checks every accepted stream by re-encoding it, and has its own FNV) and are pasted below: seeded encode cases (the
// input generator is mirrored here) and decode streams embedded as byte arrays, each checked by a digest of the status, the length and the
// bytes for several destination capacities. Hand-computed cases, the check orders, the no-write rules and the size limits sit beside them.
// Every destination is an exact-size heap vector, so ASan sees any write past it; a failure must leave the sentinel bytes and out_len as
// they were. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "rle.h"

using namespace lumora::rle;

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
  const u8* data;
  u32 n;
  u32 raw;  // 0xFFFFFFFF when the stream is rejected
  u64 digest;
};

const EncCase kEncCases[] = {{0, 0, 1, 4672095441862941765ull, 0u}, {1, 1, 2, 6028312369429139424ull, 2u}, {0, 300, 3, 11417174773689601280ull, 32u}, {0, 700, 4, 1850971443168630944ull, 68u}, {1, 2000, 5, 5178304130840365460ull, 3976u}, {2, 1, 6, 3394818208672855456ull, 2u}, {2, 255, 7, 1566483053777456896ull, 2u}, {2, 256, 8, 18152857059708110048ull, 2u}, {2, 257, 9, 8451656197103249472ull, 4u}, {2, 512, 10, 6939086184048589408ull, 4u}, {2, 513, 11, 3235853964093931488ull, 6u}, {2, 70000, 12, 12185516473294726024ull, 548u}, {3, 100, 13, 48147162537034016ull, 200u}, {4, 5000, 14, 862296447179131680ull, 46u}, {5, 4000, 15, 18258526247568468896ull, 34u}, {0, 9000, 16, 3288025266192831988ull, 874u}, {4, 30000, 17, 1784095140344121500ull, 288u}, {5, 20000, 18, 164030676365570272ull, 188u}, {1, 65537, 19, 1257844923357252580ull, 130570u}};
const std::uint8_t kDec0[] = {0};
const std::uint8_t kDec1[] = {0, 5};
const std::uint8_t kDec2[] = {255, 7};
const std::uint8_t kDec3[] = {255, 7, 255, 7, 3, 7};
const std::uint8_t kDec4[] = {0, 7, 0, 7};
const std::uint8_t kDec5[] = {3, 7, 0, 8, 0, 8};
const std::uint8_t kDec6[] = {10, 1, 20, 2, 30, 1};
const std::uint8_t kDec7[] = {0, 1, 0};
const std::uint8_t kDec8[] = {0, 7, 0, 7, 0};
const std::uint8_t kDec9[] = {254, 7, 0, 7};
const std::uint8_t kDec10[] = {255, 7, 0, 7, 0, 7};
const std::uint8_t kDec11[] = {255, 7, 255, 7, 255, 7};
const std::uint8_t kDec12[] = {0, 7, 1, 8, 0, 7};
const std::uint8_t kDec13[] = {5, 9};
const std::uint8_t kDec14[] = {15, 251, 24, 137, 24, 74, 6, 63, 18, 69, 10, 72, 30, 160, 9, 215, 19, 24, 20, 23, 36, 25, 9, 43, 19, 191, 27, 83, 32, 38, 25, 17, 17, 161, 12, 179, 22, 172, 6, 10};
const std::uint8_t kDec15[] = {0, 40, 0, 81, 0, 0, 0, 91, 0, 37, 0, 56, 0, 4, 0, 117, 0, 214, 0, 9, 0, 160, 0, 171, 0, 140, 0, 44, 0, 226, 0, 206, 0, 185, 0, 159, 0, 137, 0, 176, 0, 147, 0, 141, 0, 72, 0, 209, 0, 31, 0, 206, 0, 185, 0, 142, 0, 162, 0, 165, 0, 127, 0, 29, 0, 37, 0, 11, 0, 141, 0, 42, 0, 28, 0, 228, 0, 98, 0, 149, 0, 108, 0, 90, 0, 32, 0, 99, 0, 112, 0, 250, 0, 156, 0, 81, 0, 110, 0, 13, 0, 123, 0, 59, 0, 41, 0, 118, 0, 175, 0, 148, 0, 197, 0, 249, 0, 172, 0, 244};
const std::uint8_t kDec16[] = {255, 1, 255, 1, 255, 1, 255, 1, 255, 1, 77, 1, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 105, 0};
const std::uint8_t kDec17[] = {255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 0, 0, 255, 1, 194, 1};
const std::uint8_t kDec18[] = {0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9};
const std::uint8_t kDec19[] = {15, 251, 24, 251, 24, 74, 6, 63, 18, 69, 10, 72, 30, 160, 9, 215, 19, 24, 20, 23, 36, 25, 9, 43, 19, 191, 27, 83, 32, 38, 25, 17, 17, 161, 12, 179, 22, 172, 6, 10};
const std::uint8_t kDec20[] = {101, 1, 201, 1, 169, 1, 218, 2};
const std::uint8_t kDec21[] = {252, 2, 42, 0, 191, 0, 9, 1, 24, 2};
const std::uint8_t kDec22[] = {79, 2, 221, 0, 7, 0, 44, 2, 178, 0, 255, 2, 255, 1, 15, 2, 237, 0, 227, 1, 255, 0, 180, 2, 255, 2, 87, 0, 231, 2, 51, 2};
const std::uint8_t kDec23[] = {247, 0, 138, 0, 4, 1, 247, 1, 3, 1, 130, 2, 255, 0, 255, 0, 255, 1};
const std::uint8_t kDec24[] = {204, 1, 149, 0, 112, 1, 255, 1, 157, 0, 14, 2, 248, 0, 52, 0, 154, 1, 121, 1, 255, 2, 164, 0, 13, 0, 225, 1, 240, 0, 32, 1, 64, 2, 34, 0, 255, 2, 245, 2, 255, 1, 184, 0, 255, 1, 128, 2, 31, 2};
const std::uint8_t kDec25[] = {159, 2, 255, 0, 255, 1, 152, 0, 20, 1, 70, 2, 17, 2, 148, 0, 255, 2, 23, 2, 88, 2, 255, 2, 129, 1, 146, 0, 141, 1, 220, 0, 95, 1, 255, 1, 112, 2, 0, 1, 138, 2, 229, 2, 37, 1, 237, 0, 136, 1, 144, 0, 255, 1, 255, 1, 129, 1};
const std::uint8_t kDec26[] = {36, 0, 140, 0, 78, 2, 61, 2, 225, 1, 157, 2, 255, 1, 14, 1, 229, 1, 61, 1, 214, 0, 69, 2, 15, 2, 31, 1, 210, 2, 146, 1, 171, 0, 21, 1, 85, 0, 214, 0, 255, 1, 16, 1, 236, 1, 237, 1, 7, 2, 255, 2};
const std::uint8_t kDec27[] = {208, 0, 54, 0, 41, 0};
const std::uint8_t kDec28[] = {255, 1, 175, 1, 36, 0, 21, 2, 255, 2, 191, 2, 5, 2, 225, 1, 115, 0, 255, 1, 255, 2, 112, 1, 161, 0, 225, 0, 0, 2, 255, 2, 255, 1};
const std::uint8_t kDec29[] = {8, 1, 217, 1, 72, 1, 78, 1, 39, 2, 255, 0};
const DecCase kDecCases[] = {{kDec0, 0u, 0u, 4524767293552705221ull}, {kDec1, 2u, 1u, 3480270565111477841ull}, {kDec2, 2u, 256u, 2606861482198064046ull}, {kDec3, 6u, 516u, 4503055675462150535ull}, {kDec4, 4u, 4294967295u, 3088575930823903669ull}, {kDec5, 6u, 4294967295u, 3088575930823903669ull}, {kDec6, 6u, 63u, 8372866425699334128ull}, {kDec7, 3u, 4294967295u, 14222729515248215621ull}, {kDec8, 5u, 4294967295u, 14222729515248215621ull}, {kDec9, 4u, 4294967295u, 3088575930823903669ull}, {kDec10, 6u, 4294967295u, 3088575930823903669ull}, {kDec11, 6u, 768u, 7048530396159978716ull}, {kDec12, 6u, 4u, 7001812906817325312ull}, {kDec13, 2u, 6u, 13197205067103476672ull}, {kDec14, 40u, 400u, 4918941307026327914ull}, {kDec15, 120u, 60u, 9258806886636001024ull}, {kDec16, 26u, 3000u, 3301026993721701580ull}, {kDec17, 22u, 2500u, 1872320805193848687ull}, {kDec18, 80u, 40u, 7679581034675182240ull}, {kDec19, 40u, 4294967295u, 3088575930823903669ull}, {kDec20, 8u, 4294967295u, 3088575930823903669ull}, {kDec21, 10u, 4294967295u, 3088575930823903669ull}, {kDec22, 32u, 4294967295u, 3088575930823903669ull}, {kDec23, 18u, 4294967295u, 3088575930823903669ull}, {kDec24, 50u, 4294967295u, 3088575930823903669ull}, {kDec25, 58u, 4294967295u, 3088575930823903669ull}, {kDec26, 52u, 4294967295u, 3088575930823903669ull}, {kDec27, 6u, 4294967295u, 3088575930823903669ull}, {kDec28, 34u, 4294967295u, 3088575930823903669ull}, {kDec29, 12u, 4294967295u, 3088575930823903669ull}};

constexpr u32 kRunLens[] = {255, 256, 257, 512, 511, 513};

std::vector<u8> gen(int mode, u32 n, u64 seed) {
  Lcg r(seed);
  std::vector<u8> out;
  if (mode == 1) {
    for (u32 i = 0; i < n; ++i) out.push_back(static_cast<u8>(r.rng(0, 255)));
  } else if (mode == 2) {
    out.assign(n, static_cast<u8>(seed & 255));
  } else if (mode == 3) {
    for (u32 i = 0; i < n; ++i) out.push_back((i & 1) ? 9 : 7);
  } else {
    while (out.size() < n) {
      if (mode == 0) {
        const u8 v = static_cast<u8>(r.rng(0, 255));
        out.insert(out.end(), r.rng(1, 40), v);
      } else if (mode == 4) {
        const u8 v = static_cast<u8>(r.rng(0, 1));
        out.insert(out.end(), r.rng(1, 600), v);
      } else {
        const u8 v = static_cast<u8>(r.rng(0, 2));
        out.insert(out.end(), kRunLens[r.rng(0, 5)], v);
      }
    }
    out.resize(n);
  }
  return out;
}

struct EncOut {
  RleResult st;
  u32 len;
  std::vector<u8> dst;
};

// dst is an exact-size vector of cap bytes filled with a sentinel; a failure must leave it and len alone.
EncOut run_encode(const std::vector<u8>& in, u32 cap) {
  EncOut o;
  o.dst.assign(cap, kFill);
  o.len = kSent;
  o.st = encode(in.data(), static_cast<u32>(in.size()), cap ? o.dst.data() : nullptr, cap, &o.len);
  return o;
}

EncOut run_decode(const u8* src, u32 n, u32 cap) {
  EncOut o;
  o.dst.assign(cap, kFill);
  o.len = kSent;
  o.st = decode(src, n, cap ? o.dst.data() : nullptr, cap, &o.len);
  return o;
}

// Folds one result like the reference: status, length (the sentinel on failure), then the bytes on success.
void fold(Fnv& f, const EncOut& o) {
  f.word(static_cast<u32>(o.st));
  f.word(o.len);
  if (o.st == RleResult::Ok) {
    for (u32 i = 0; i < o.len; ++i) f.word(o.dst[i]);
    for (std::size_t i = o.len; i < o.dst.size(); ++i) REQUIRE(o.dst[i] == kFill);  // nothing past out_len
  } else {
    for (u8 b : o.dst) REQUIRE(b == kFill);
  }
}

}  // namespace

TEST_CASE("rle: encode cases match the independent reference") {
  for (const EncCase& c : kEncCases) {
    const std::vector<u8> in = gen(c.mode, c.n, c.seed);
    REQUIRE(in.size() == c.n);
    Fnv f;
    const u32 need = c.need;
    const u32 caps[] = {0, 1, need > 0 ? need - 1 : 0, need, need + 3};
    for (u32 cap : caps) {
      const EncOut o = run_encode(in, cap);
      fold(f, o);
      CHECK(o.st == (need > cap ? RleResult::BufferTooSmall : RleResult::Ok));
    }
    CHECK(f.h == c.digest);
    // round trip through the strict decoder
    const EncOut e = run_encode(in, need);
    REQUIRE(e.st == RleResult::Ok);
    REQUIRE(e.len == need);
    const EncOut d = run_decode(e.dst.data(), need, c.n);
    REQUIRE(d.st == RleResult::Ok);
    CHECK(d.len == c.n);
    CHECK(d.dst == in);
  }
}

TEST_CASE("rle: decode streams match the independent reference") {
  for (const DecCase& c : kDecCases) {
    Fnv f;
    std::vector<u32> caps = {0, 1, 255, 256, 300, 5000};
    if (c.raw != 0xFFFFFFFFu) {
      caps.push_back(c.raw > 0 ? c.raw - 1 : 0);
      caps.push_back(c.raw);
      caps.push_back(c.raw + 3);
    }
    for (u32 cap : caps) fold(f, run_decode(c.data, c.n, cap));
    CHECK(f.h == c.digest);
  }
}

TEST_CASE("rle: encode by hand") {
  const u8 a[] = {5, 5, 5, 9};
  EncOut o = run_encode(std::vector<u8>(a, a + 4), 4);
  REQUIRE(o.st == RleResult::Ok);
  CHECK(o.len == 4u);
  CHECK(o.dst == std::vector<u8>({2, 5, 0, 9}));
  struct Row {
    u32 n;
    std::vector<u8> want;
  };
  const Row rows[] = {{1, {0, 3}}, {255, {254, 3}}, {256, {255, 3}}, {257, {255, 3, 0, 3}}, {511, {255, 3, 254, 3}}, {512, {255, 3, 255, 3}},
                      {513, {255, 3, 255, 3, 0, 3}}};
  for (const Row& r : rows) {
    const std::vector<u8> in(r.n, 3);
    o = run_encode(in, static_cast<u32>(r.want.size()));
    REQUIRE(o.st == RleResult::Ok);
    CHECK(o.dst == r.want);
    CHECK(o.len == r.want.size());
  }
  // a different byte after a full run starts a new pair; equal bytes after a cut continue the stream
  std::vector<u8> in(256, 1);
  in.push_back(2);
  o = run_encode(in, 4);
  CHECK(o.dst == std::vector<u8>({255, 1, 0, 2}));
  // empty input
  u32 len = kSent;
  CHECK(encode(nullptr, 0, nullptr, 0, &len) == RleResult::Ok);
  CHECK(len == 0u);
  len = kSent;
  u8 spare[3] = {kFill, kFill, kFill};
  CHECK(encode(a, 0, spare, 3, &len) == RleResult::Ok);
  CHECK(len == 0u);
  CHECK(spare[0] == kFill);
}

TEST_CASE("rle: encode checks and the no-write rule") {
  const std::vector<u8> in(10, 4);
  u8 dst[8];
  for (u8& b : dst) b = kFill;
  u32 len = kSent;
  auto untouched = [&]() {
    CHECK(len == kSent);
    for (u8 b : dst) CHECK(b == kFill);
  };
  CHECK(encode(in.data(), 10, dst, 8, nullptr) == RleResult::NullArgument);
  CHECK(encode(nullptr, 10, dst, 8, &len) == RleResult::NullArgument);
  CHECK(encode(in.data(), 10, nullptr, 8, &len) == RleResult::NullArgument);
  CHECK(encode(nullptr, 10, nullptr, 8, nullptr) == RleResult::NullArgument);
  untouched();
  // null checks come before the size checks
  CHECK(encode(nullptr, kMaxInput + 1, dst, 8, &len) == RleResult::NullArgument);
  CHECK(encode(in.data(), kMaxInput + 1, nullptr, 8, &len) == RleResult::NullArgument);
  // TooLarge comes before BufferTooSmall and reads nothing
  CHECK(encode(in.data(), kMaxInput + 1, dst, 0, &len) == RleResult::TooLarge);
  CHECK(encode(in.data(), 0xFFFFFFFFu, dst, 8, &len) == RleResult::TooLarge);
  CHECK(encode(in.data(), kMaxInput + 1, nullptr, 0, &len) == RleResult::TooLarge);
  untouched();
  CHECK(encode(in.data(), 10, nullptr, 0, &len) == RleResult::BufferTooSmall);
  CHECK(encode(in.data(), 10, dst, 1, &len) == RleResult::BufferTooSmall);
  untouched();
  CHECK(encode(in.data(), 10, dst, 2, &len) == RleResult::Ok);
  CHECK(len == 2u);
  CHECK(dst[0] == 9);
  CHECK(dst[1] == 4);
  CHECK(dst[2] == kFill);
  // the size is exact: two runs need 4 bytes
  const u8 two[] = {1, 1, 2};
  for (u8& b : dst) b = kFill;
  len = kSent;
  CHECK(encode(two, 3, dst, 3, &len) == RleResult::BufferTooSmall);
  untouched();
  CHECK(encode(two, 3, dst, 4, &len) == RleResult::Ok);
  CHECK(len == 4u);
}

TEST_CASE("rle: encode at the input limit needs exactly kMaxEncoded bytes") {
  std::vector<u8> in(kMaxInput);
  for (u32 i = 0; i < kMaxInput; ++i) in[i] = static_cast<u8>(i & 1);
  std::vector<u8> small(kMaxEncoded - 1, kFill);
  u32 len = kSent;
  CHECK(encode(in.data(), kMaxInput, small.data(), kMaxEncoded - 1, &len) == RleResult::BufferTooSmall);
  CHECK(len == kSent);
  CHECK(small[0] == kFill);
  std::vector<u8> dst(kMaxEncoded, kFill);
  REQUIRE(encode(in.data(), kMaxInput, dst.data(), kMaxEncoded, &len) == RleResult::Ok);
  CHECK(len == kMaxEncoded);
  CHECK(dst[0] == 0);
  CHECK(dst[1] == 0);
  CHECK(dst[2] == 0);
  CHECK(dst[3] == 1);
  CHECK(dst[kMaxEncoded - 2] == 0);
  CHECK(dst[kMaxEncoded - 1] == 1);
  std::vector<u8> same(kMaxInput, 8);
  REQUIRE(encode(same.data(), kMaxInput, dst.data(), kMaxEncoded, &len) == RleResult::Ok);
  CHECK(len == 2u * (kMaxInput / 256));
  CHECK(dst[len - 2] == 255);
  CHECK(dst[len - 1] == 8);
}

TEST_CASE("rle: decode by hand") {
  struct Row {
    std::vector<u8> in;
    std::vector<u8> want;
  };
  const Row rows[] = {{{2, 5, 0, 9}, {5, 5, 5, 9}},
                      {{0, 1}, {1}},
                      {{255, 7}, std::vector<u8>(256, 7)},
                      {{255, 7, 0, 7}, std::vector<u8>(257, 7)},
                      {{255, 7, 255, 7, 255, 7}, std::vector<u8>(768, 7)},
                      {{1, 3, 0, 4, 2, 3}, {3, 3, 4, 3, 3, 3}}};
  for (const Row& r : rows) {
    const u32 cap = static_cast<u32>(r.want.size());
    const EncOut o = run_decode(r.in.data(), static_cast<u32>(r.in.size()), cap);
    REQUIRE(o.st == RleResult::Ok);
    CHECK(o.len == cap);
    CHECK(o.dst == r.want);
  }
  u32 len = kSent;
  CHECK(decode(nullptr, 0, nullptr, 0, &len) == RleResult::Ok);
  CHECK(len == 0u);
}

TEST_CASE("rle: decode check order") {
  u8 dst[8];
  for (u8& b : dst) b = kFill;
  u32 len = kSent;
  auto untouched = [&]() {
    CHECK(len == kSent);
    for (u8 b : dst) CHECK(b == kFill);
  };
  const u8 ok[] = {1, 7};
  const u8 odd[] = {0, 7, 0};
  const u8 same[] = {0, 7, 0, 7};
  CHECK(decode(ok, 2, dst, 8, nullptr) == RleResult::NullArgument);
  CHECK(decode(nullptr, 2, dst, 8, &len) == RleResult::NullArgument);
  CHECK(decode(ok, 2, nullptr, 8, &len) == RleResult::NullArgument);
  CHECK(decode(nullptr, 3, dst, 8, &len) == RleResult::NullArgument);  // null before odd
  CHECK(decode(odd, 3, nullptr, 8, &len) == RleResult::NullArgument);
  CHECK(decode(odd, 3, dst, 8, &len) == RleResult::Malformed);
  CHECK(decode(odd, 3, dst, 0, &len) == RleResult::Malformed);
  CHECK(decode(odd, kMaxEncoded + 1, dst, 8, &len) == RleResult::Malformed);  // odd before too large, reads nothing
  CHECK(decode(odd, 0xFFFFFFFFu, dst, 8, &len) == RleResult::Malformed);
  CHECK(decode(odd, kMaxEncoded + 2, dst, 8, &len) == RleResult::TooLarge);  // checked before the scan, reads nothing
  CHECK(decode(odd, 0xFFFFFFFEu, dst, 8, &len) == RleResult::TooLarge);
  CHECK(decode(same, 4, dst, 8, &len) == RleResult::NonCanonical);
  CHECK(decode(same, 4, dst, 0, &len) == RleResult::NonCanonical);  // before the capacity check
  CHECK(decode(same, 4, nullptr, 0, &len) == RleResult::NonCanonical);
  CHECK(decode(ok, 2, dst, 1, &len) == RleResult::BufferTooSmall);
  CHECK(decode(ok, 2, nullptr, 0, &len) == RleResult::BufferTooSmall);
  untouched();
  CHECK(decode(ok, 2, dst, 2, &len) == RleResult::Ok);
  CHECK(len == 2u);
  CHECK(dst[0] == 7);
  CHECK(dst[1] == 7);
  CHECK(dst[2] == kFill);
  // a different value after a short pair, or the same value after a full pair, is canonical
  const u8 diff[] = {0, 7, 0, 8};
  const u8 full[] = {255, 7, 0, 7};
  u8 big[257];
  CHECK(decode(diff, 4, big, 257, &len) == RleResult::Ok);
  CHECK(len == 2u);
  CHECK(decode(full, 4, big, 257, &len) == RleResult::Ok);
  CHECK(len == 257u);
  // the previous pair is the one right before, not the first one with that value
  const u8 skip[] = {0, 7, 0, 8, 0, 7};
  CHECK(decode(skip, 6, big, 257, &len) == RleResult::Ok);
  CHECK(len == 3u);
}

TEST_CASE("rle: decode raw size limit and the order inside the scan") {
  // 65536 full pairs with alternating values: exactly kMaxInput raw bytes
  std::vector<u8> s;
  for (u32 i = 0; i < 65536; ++i) {
    s.push_back(255);
    s.push_back(static_cast<u8>(1 + (i & 1)));
  }
  std::vector<u8> dst(kMaxInput, kFill);
  u32 len = kSent;
  REQUIRE(decode(s.data(), static_cast<u32>(s.size()), dst.data(), kMaxInput, &len) == RleResult::Ok);
  CHECK(len == kMaxInput);
  CHECK(dst[0] == 1);
  CHECK(dst[255] == 1);
  CHECK(dst[256] == 2);
  CHECK(dst[kMaxInput - 1] == 2);
  std::vector<u8> tight(kMaxInput - 1, kFill);
  len = kSent;
  CHECK(decode(s.data(), static_cast<u32>(s.size()), tight.data(), kMaxInput - 1, &len) == RleResult::BufferTooSmall);
  CHECK(len == kSent);
  CHECK(tight[0] == kFill);
  // one more pair: the running total passes kMaxInput, even with a tiny destination
  std::vector<u8> over = s;
  over.push_back(0);
  over.push_back(1);  // different value from the last pair (2), canonical
  u8 one[1] = {kFill};
  CHECK(decode(over.data(), static_cast<u32>(over.size()), one, 1, &len) == RleResult::TooLarge);
  CHECK(len == kSent);
  CHECK(one[0] == kFill);
  // a pair that is both non-canonical and over the limit: the canonical check comes first
  std::vector<u8> both(s.begin(), s.end() - 2);  // 65535 full pairs, raw 2^24 - 256
  both.push_back(254);
  both.push_back(2);  // raw 2^24 - 1, value differs from the previous pair (1 + (65534 & 1) = 1)
  both.push_back(1);
  both.push_back(2);  // same value as the previous pair, previous b0 is 254: non-canonical, and the total would be 2^24 + 1
  CHECK(decode(both.data(), static_cast<u32>(both.size()), one, 1, &len) == RleResult::NonCanonical);
  // an earlier overflow wins over a later non-canonical pair
  std::vector<u8> early = over;
  early.push_back(0);
  early.push_back(1);  // would be non-canonical (same value as the previous pair, which has b0 == 0)
  CHECK(decode(early.data(), static_cast<u32>(early.size()), one, 1, &len) == RleResult::TooLarge);
  // n == kMaxEncoded passes the size check and is scanned: pairs of b0 0 give 2^24 raw bytes, which is accepted but does not fit one byte
  std::vector<u8> huge(kMaxEncoded, 0);
  for (u32 i = 1; i < kMaxEncoded; i += 2) huge[i] = static_cast<u8>((i / 2) & 1);
  CHECK(decode(huge.data(), kMaxEncoded, one, 1, &len) == RleResult::BufferTooSmall);
  CHECK(len == kSent);
  std::vector<u8> huge_full(kMaxEncoded, 255);
  for (u32 i = 1; i < kMaxEncoded; i += 2) huge_full[i] = static_cast<u8>((i / 2) & 1);
  CHECK(decode(huge_full.data(), kMaxEncoded, one, 1, &len) == RleResult::TooLarge);
}

TEST_CASE("rle: structural oracle on random buffers") {
  Lcg r(0xFACE);
  for (int k = 0; k < 300; ++k) {
    const int mode = static_cast<int>(r.rng(0, 5));
    const u32 n = r.rng(0, 1500);
    const std::vector<u8> in = gen(mode, n, r.next());
    const EncOut e = run_encode(in, 2 * n);
    REQUIRE(e.st == RleResult::Ok);
    REQUIRE(e.len % 2 == 0);
    REQUIRE(e.len <= 2 * n);
    u64 total_raw = 0;
    for (u32 i = 0; i < e.len; i += 2) {
      total_raw += static_cast<u64>(e.dst[i]) + 1;
      if (i > 0 && e.dst[i + 1] == e.dst[i - 1]) REQUIRE(e.dst[i - 2] == 255);  // canonical: same value only after a full run
    }
    REQUIRE(total_raw == n);
    const EncOut d = run_decode(e.dst.data(), e.len, n);
    REQUIRE(d.st == RleResult::Ok);
    REQUIRE(d.dst == in);
    // any single pair repeated after itself with a short first copy is rejected
    if (e.len >= 2 && e.dst[e.len - 2] != 255) {
      std::vector<u8> bad(e.dst.begin(), e.dst.begin() + e.len);
      bad.push_back(0);
      bad.push_back(bad[e.len - 1]);
      const EncOut x = run_decode(bad.data(), static_cast<u32>(bad.size()), n + 1);
      REQUIRE(x.st == RleResult::NonCanonical);
    }
  }
}
