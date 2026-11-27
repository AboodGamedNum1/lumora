// Tests for core/histogram (scope AI). Expected values come from an independent Python reference (tools/histogram_ref.py: it follows the
// rule text literally with Python integers, finds image percentiles a second way by sorting the pixels, and has its own FNV) and are pasted
// below: seeded image cases (strides with garbage between rows) and seeded hand-built histograms, each checked by a digest of the bins,
// total, mean, percentiles and stretch tables. Hand-computed cases, the check order, the no-write rules and a structural oracle sit beside
// them. Image buffers are exact-size heap vectors, so ASan sees any read past the last row. Evidence label: local unit tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

#include "histogram.h"

using namespace lumora::histogram;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u8 = std::uint8_t;

constexpr u32 kPms[] = {0, 1, 10, 100, 250, 333, 500, 501, 750, 900, 990, 999, 1000};
constexpr u32 kPairs[][2] = {{0, 1000}, {10, 990}, {250, 750}, {0, 1}, {999, 1000}, {500, 501}, {0, 500}, {500, 1000}, {5, 5}, {700, 300}};

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

struct ImgCase {
  u32 w, h, stride;
  int mode;
  u64 seed, digest;
  u32 words;
};

struct HistCase {
  u64 seed;
  u32 max_bin;
  int empty;
  u64 digest;
  u32 words;
};

const ImgCase kImgCases[] = {{1, 1, 1, 0, 1, 16096108539111137485ull, 2344u}, {7, 5, 7, 0, 2, 9284576725111993431ull, 2344u}, {7, 5, 11, 0, 3, 8585976056018319747ull, 2344u}, {16, 16, 16, 3, 4, 13418393677192853521ull, 2344u}, {33, 9, 40, 1, 5, 1128687788101960491ull, 2344u}, {64, 64, 64, 2, 6, 8250419249089300422ull, 2344u}, {50, 20, 77, 4, 7, 7011426539852855622ull, 2344u}, {100, 31, 128, 5, 8, 16203132341808707048ull, 2344u}, {255, 3, 300, 6, 9, 676075363438213627ull, 2344u}, {256, 256, 256, 0, 10, 3799674145788987627ull, 2344u}, {257, 17, 260, 0, 11, 9152935187820371479ull, 2344u}, {1024, 1, 1024, 3, 12, 3376199367541429206ull, 2344u}, {1, 300, 1, 0, 13, 3722249947823089308ull, 2344u}, {1, 300, 9, 6, 14, 1848291839410978109ull, 2344u}, {4096, 2, 65536, 0, 15, 8789258834406513124ull, 2344u}, {4096, 4, 4096, 2, 16, 8417951310054482627ull, 2344u}, {300, 300, 513, 6, 17, 354222105876813825ull, 2344u}, {128, 128, 128, 4, 18, 13218490210005195263ull, 2344u}};
const HistCase kHistCases[] = {{101, 5u, 0, 17839991638135811927ull, 2344u}, {102, 5u, 8, 12451881054693526114ull, 2344u}, {103, 1000u, 5, 11980698532569324688ull, 2344u}, {104, 4294967295u, 0, 13358912280450520947ull, 2344u}, {105, 4294967295u, 9, 10014373999386396755ull, 2344u}, {106, 1u, 9, 10727358975654176468ull, 2344u}, {107, 65535u, 3, 8541532692905622574ull, 2344u}, {108, 4294967295u, 6, 65282391754618273ull, 2344u}, {109, 3u, 10, 7323732142835020101ull, 282u}, {110, 4294967295u, 2, 12305138599450539773ull, 2344u}, {111, 100u, 7, 5986821134335178778ull, 2344u}, {112, 2147483648u, 4, 15607391362561609318ull, 2344u}};

struct Digest {
  u64 h;
  u32 words;
};

// Same word order as the reference: bins, total low and high, mean (flag, value), each percentile (flag, value), each stretch (flag, lut).
Digest digest_of(const Histogram& hist) {
  Fnv f;
  u32 n = 0;
  auto w = [&](u32 v) {
    f.word(v);
    ++n;
  };
  for (u32 v = 0; v < kBins; ++v) w(hist.bins[v]);
  const u64 t = total(&hist);
  w(static_cast<u32>(t & 0xFFFFFFFFu));
  w(static_cast<u32>(t >> 32));
  u32 m = 0;
  if (mean_q8(&hist, &m)) {
    w(1);
    w(m);
  } else {
    w(0);
  }
  for (u32 pm : kPms) {
    u8 p = 0;
    if (percentile(&hist, pm, &p)) {
      w(1);
      w(p);
    } else {
      w(0);
    }
  }
  for (const auto& pr : kPairs) {
    u8 lut[kBins];
    std::memset(lut, 0xA5, sizeof lut);
    if (stretch_lut(&hist, pr[0], pr[1], lut)) {
      w(1);
      for (u32 v = 0; v < kBins; ++v) w(lut[v]);
    } else {
      w(0);
      for (u32 v = 0; v < kBins; ++v) REQUIRE(lut[v] == 0xA5);
    }
  }
  return {f.h, n};
}

u8 gen_pixel(int mode, u64 seed, Lcg& r, u32 x, u32 y) {
  switch (mode) {
    case 0: return static_cast<u8>(r.rng(0, 255));
    case 1: return static_cast<u8>(seed & 255);
    case 2: return r.rng(0, 3) ? 17 : 200;
    case 3: return static_cast<u8>((x + y) & 255);
    case 4: return static_cast<u8>(r.rng(100, 103));
    case 5: return r.rng(0, 9) ? 255 : 0;
    default: {
      const u32 a = r.rng(0, 255);
      const u32 b = r.rng(0, 255);
      return static_cast<u8>(a & b);
    }
  }
}

// Exact-size buffer of (h - 1) * stride + w bytes; the bytes between rows are garbage that must never be counted.
std::vector<u8> make_image(const ImgCase& c) {
  Lcg r(c.seed);
  std::vector<u8> buf(static_cast<std::size_t>(c.h - 1) * c.stride + c.w);
  for (u32 y = 0; y < c.h; ++y) {
    for (u32 x = 0; x < c.w; ++x) buf[static_cast<std::size_t>(y) * c.stride + x] = gen_pixel(c.mode, c.seed, r, x, y);
    if (y + 1 < c.h) {
      for (u32 x = c.w; x < c.stride; ++x) buf[static_cast<std::size_t>(y) * c.stride + x] = static_cast<u8>(r.rng(0, 255));
    }
  }
  return buf;
}

Histogram filled(u32 v) {
  Histogram h;
  for (u32 i = 0; i < kBins; ++i) h.bins[i] = v;
  return h;
}

bool same_hist(const Histogram& a, const Histogram& b) { return std::memcmp(&a, &b, sizeof(Histogram)) == 0; }

}  // namespace

TEST_CASE("histogram: image cases match the independent reference") {
  for (const ImgCase& c : kImgCases) {
    const std::vector<u8> buf = make_image(c);
    Histogram h = filled(0xDEADBEEFu);  // must be overwritten, not added to
    REQUIRE(build(buf.data(), c.w, c.h, c.stride, &h) == HistResult::Ok);
    CHECK(total(&h) == static_cast<u64>(c.w) * c.h);
    const Digest d = digest_of(h);
    CHECK(d.h == c.digest);
    CHECK(d.words == c.words);
  }
}

TEST_CASE("histogram: hand-built histograms match the independent reference") {
  for (const HistCase& c : kHistCases) {
    Lcg r(c.seed);
    Histogram h;
    for (u32 v = 0; v < kBins; ++v) h.bins[v] = r.rng(0, 9) < static_cast<u32>(c.empty) ? 0u : r.rng(0, c.max_bin);
    const Digest d = digest_of(h);
    CHECK(d.h == c.digest);
    CHECK(d.words == c.words);
  }
}

TEST_CASE("histogram: build counts a small image by hand") {
  // 3 x 2 image with stride 5: the two bytes after each row are padding
  const u8 img[] = {1, 1, 2, 99, 99, 255, 0, 1};
  Histogram h = filled(7);
  REQUIRE(build(img, 3, 2, 5, &h) == HistResult::Ok);
  for (u32 v = 0; v < kBins; ++v) {
    const u32 want = v == 1 ? 3 : (v == 2 || v == 255 || v == 0) ? 1 : 0;
    CHECK(h.bins[v] == want);
  }
  CHECK(total(&h) == 6u);
  // a second build overwrites
  const u8 one[] = {9};
  REQUIRE(build(one, 1, 1, 1, &h) == HistResult::Ok);
  CHECK(h.bins[9] == 1u);
  CHECK(h.bins[1] == 0u);
  CHECK(total(&h) == 1u);
}

TEST_CASE("histogram: build check order and no-write failures") {
  const std::vector<u8> img(64, 3);
  Histogram h = filled(5);
  const Histogram before = h;
  CHECK(build(nullptr, 4, 4, 4, &h) == HistResult::NullArgument);
  CHECK(build(img.data(), 4, 4, 4, nullptr) == HistResult::NullArgument);
  CHECK(build(nullptr, 0, 0, 0, &h) == HistResult::NullArgument);  // null beats size
  CHECK(build(img.data(), 0, 4, 4, &h) == HistResult::BadSize);
  CHECK(build(img.data(), 4, 0, 4, &h) == HistResult::BadSize);
  CHECK(build(img.data(), 4097, 4, 4097, &h) == HistResult::BadSize);
  CHECK(build(img.data(), 4, 4097, 4, &h) == HistResult::BadSize);
  CHECK(build(img.data(), 0, 4, 0, &h) == HistResult::BadSize);          // size beats stride
  CHECK(build(img.data(), 4097, 4, 70000, &h) == HistResult::BadSize);   // size beats stride
  CHECK(build(img.data(), 4, 4, 3, &h) == HistResult::BadStride);
  CHECK(build(img.data(), 4, 4, 65537, &h) == HistResult::BadStride);
  CHECK(build(img.data(), 4, 4, 0, &h) == HistResult::BadStride);
  CHECK(build(img.data(), 4096, 1, 65537, &h) == HistResult::BadStride);
  CHECK(same_hist(h, before));
  // limits that are accepted
  std::vector<u8> wide(4096, 9);
  CHECK(build(wide.data(), 4096, 1, 4096, &h) == HistResult::Ok);
  CHECK(h.bins[9] == 4096u);
  std::vector<u8> tall(4096, 8);
  CHECK(build(tall.data(), 1, 4096, 1, &h) == HistResult::Ok);
  CHECK(h.bins[8] == 4096u);
  std::vector<u8> wide_stride(65536 + 4, 1);
  CHECK(build(wide_stride.data(), 4, 2, 65536, &h) == HistResult::Ok);
  CHECK(total(&h) == 8u);
  CHECK(build(wide_stride.data(), 4, 1, 4, &h) == HistResult::Ok);
}

TEST_CASE("histogram: the largest image counts 2^24 pixels") {
  std::vector<u8> big(static_cast<std::size_t>(4096) * 4096, 7);
  Histogram h;
  REQUIRE(build(big.data(), 4096, 4096, 4096, &h) == HistResult::Ok);
  CHECK(h.bins[7] == 16777216u);
  CHECK(total(&h) == 16777216ull);
  CHECK(h.bins[6] == 0u);
}

TEST_CASE("histogram: total is the 64 bit sum of the bins") {
  CHECK(total(nullptr) == 0u);
  Histogram h;
  CHECK(total(&h) == 0u);
  h.bins[0] = 1;
  h.bins[255] = 2;
  CHECK(total(&h) == 3u);
  const Histogram m = filled(0xFFFFFFFFu);
  CHECK(total(&m) == 256ull * 0xFFFFFFFFull);
  Histogram two;
  two.bins[3] = 0xFFFFFFFFu;
  two.bins[4] = 0xFFFFFFFFu;
  CHECK(total(&two) == 2ull * 0xFFFFFFFFull);  // above 2^32: a 32 bit sum would wrap
}

TEST_CASE("histogram: percentile by hand") {
  Histogram h;
  h.bins[10] = 3;
  h.bins[20] = 1;
  h.bins[200] = 6;  // total 10
  struct Row {
    u32 pm;
    u8 want;
  };
  const Row rows[] = {{0, 10}, {1, 10}, {300, 10}, {301, 20}, {400, 20}, {401, 200}, {500, 200}, {999, 200}, {1000, 200}};
  for (const Row& r : rows) {
    u8 out = 77;
    REQUIRE(percentile(&h, r.pm, &out));
    CHECK(out == r.want);
  }
  Histogram lo;
  lo.bins[0] = 5;
  u8 out = 77;
  CHECK(percentile(&lo, 0, &out));
  CHECK(out == 0);
  CHECK(percentile(&lo, 1000, &out));
  CHECK(out == 0);
  Histogram hi;
  hi.bins[255] = 5;
  CHECK(percentile(&hi, 0, &out));
  CHECK(out == 255);
  CHECK(percentile(&hi, 500, &out));
  CHECK(out == 255);
  CHECK(percentile(&hi, 1000, &out));
  CHECK(out == 255);
  // per_mille 0 is the smallest used value even when the lowest bins are empty and rank 1 would also give it; 1000 is the largest used value
  Histogram g;
  g.bins[40] = 1;
  g.bins[41] = 1;
  CHECK(percentile(&g, 0, &out));
  CHECK(out == 40);
  CHECK(percentile(&g, 500, &out));
  CHECK(out == 40);
  CHECK(percentile(&g, 501, &out));
  CHECK(out == 41);
  CHECK(percentile(&g, 1000, &out));
  CHECK(out == 41);
  // ceil: total 3, per_mille 334 gives ceil(1.002) = 2
  Histogram c;
  c.bins[1] = 1;
  c.bins[2] = 1;
  c.bins[3] = 1;
  CHECK(percentile(&c, 333, &out));
  CHECK(out == 1);
  CHECK(percentile(&c, 334, &out));
  CHECK(out == 2);
  CHECK(percentile(&c, 667, &out));
  CHECK(out == 3);
  CHECK(percentile(&c, 666, &out));
  CHECK(out == 2);
}

TEST_CASE("histogram: percentile of a huge hand-built histogram needs 64 bit arithmetic") {
  const Histogram m = filled(0xFFFFFFFFu);
  u8 out = 77;
  CHECK(percentile(&m, 500, &out));
  CHECK(out == 127);
  CHECK(percentile(&m, 1000, &out));
  CHECK(out == 255);
  CHECK(percentile(&m, 1, &out));
  CHECK(out == 0);
  CHECK(percentile(&m, 999, &out));
  CHECK(out == 255);
}

TEST_CASE("histogram: percentile failures leave the output alone") {
  Histogram h;
  h.bins[5] = 2;
  const Histogram empty;
  u8 out = 77;
  CHECK_FALSE(percentile(nullptr, 500, &out));
  CHECK_FALSE(percentile(&h, 500, nullptr));
  CHECK_FALSE(percentile(&h, 1001, &out));
  CHECK_FALSE(percentile(&h, 0xFFFFFFFFu, &out));
  CHECK_FALSE(percentile(&empty, 0, &out));
  CHECK_FALSE(percentile(&empty, 500, &out));
  CHECK_FALSE(percentile(&empty, 1000, &out));
  CHECK_FALSE(percentile(&empty, 1001, &out));
  CHECK(out == 77);
}

TEST_CASE("histogram: mean_q8 by hand") {
  Histogram h;
  u32 out = 99999;
  h.bins[0] = 1;
  h.bins[255] = 1;
  CHECK(mean_q8(&h, &out));
  CHECK(out == 32640u);  // 127.5 * 256
  Histogram a;
  a.bins[1] = 1;
  a.bins[2] = 2;
  CHECK(mean_q8(&a, &out));
  CHECK(out == 427u);  // 5/3 * 256 = 426.67
  Histogram half;      // exactly 0.5 in Q8: rounds up to 1
  half.bins[0] = 511;
  half.bins[1] = 1;
  CHECK(mean_q8(&half, &out));
  CHECK(out == 1u);
  Histogram below;  // 255 / 512 * 256 = 127.5 is a tie again, check a value just below a tie: 1 / 513 * 256 = 0.499
  below.bins[0] = 512;
  below.bins[1] = 1;
  CHECK(mean_q8(&below, &out));
  CHECK(out == 0u);
  Histogram top;
  top.bins[255] = 0xFFFFFFFFu;
  CHECK(mean_q8(&top, &out));
  CHECK(out == 255u * 256u);
  const Histogram m = filled(0xFFFFFFFFu);
  CHECK(mean_q8(&m, &out));
  CHECK(out == 32640u);
  Histogram z;
  z.bins[0] = 5;
  CHECK(mean_q8(&z, &out));
  CHECK(out == 0u);
}

TEST_CASE("histogram: mean_q8 failures leave the output alone") {
  Histogram h;
  h.bins[5] = 2;
  const Histogram empty;
  u32 out = 99999;
  CHECK_FALSE(mean_q8(nullptr, &out));
  CHECK_FALSE(mean_q8(&h, nullptr));
  CHECK_FALSE(mean_q8(&empty, &out));
  CHECK(out == 99999u);
}

TEST_CASE("histogram: stretch table by hand") {
  Histogram h;
  h.bins[10] = 10;
  h.bins[110] = 10;
  u8 lut[kBins];
  REQUIRE(stretch_lut(&h, 0, 1000, lut));
  CHECK(lut[0] == 0);
  CHECK(lut[9] == 0);
  CHECK(lut[10] == 0);
  CHECK(lut[11] == 3);  // (1 * 255 + 50) / 100
  CHECK(lut[60] == 128);
  CHECK(lut[109] == 252);
  CHECK(lut[110] == 255);
  CHECK(lut[111] == 255);
  CHECK(lut[255] == 255);
  // hi - lo == 1: no interior values
  Histogram t;
  t.bins[50] = 1;
  t.bins[51] = 1;
  REQUIRE(stretch_lut(&t, 0, 1000, lut));
  CHECK(lut[49] == 0);
  CHECK(lut[50] == 0);
  CHECK(lut[51] == 255);
  CHECK(lut[52] == 255);
  // identity when both percentiles land on one value, even outside it
  Histogram one;
  one.bins[77] = 5;
  REQUIRE(stretch_lut(&one, 0, 1000, lut));
  for (u32 v = 0; v < kBins; ++v) CHECK(lut[v] == v);
  // lo = 0 and hi = 255
  Histogram e;
  e.bins[0] = 1;
  e.bins[255] = 1;
  REQUIRE(stretch_lut(&e, 0, 1000, lut));
  for (u32 v = 0; v < kBins; ++v) CHECK(lut[v] == v);  // (v * 255 + 127) / 255 == v
  // percentiles inside the data: lo = 20, hi = 40 from bins 20..40 each 1 (total 21)
  Histogram r;
  for (u32 v = 20; v <= 40; ++v) r.bins[v] = 1;
  REQUIRE(stretch_lut(&r, 0, 1000, lut));
  CHECK(lut[20] == 0);
  CHECK(lut[21] == 13);  // (255 + 10) / 20 = 13
  CHECK(lut[30] == 128);
  CHECK(lut[39] == 242);  // (19 * 255 + 10) / 20
  CHECK(lut[40] == 255);
}

TEST_CASE("histogram: stretch failures leave the table alone") {
  Histogram h;
  h.bins[10] = 3;
  h.bins[90] = 3;
  const Histogram empty;
  u8 lut[kBins];
  std::memset(lut, 0xA5, sizeof lut);
  auto untouched = [&]() {
    for (u32 v = 0; v < kBins; ++v) REQUIRE(lut[v] == 0xA5);
  };
  CHECK_FALSE(stretch_lut(nullptr, 0, 1000, lut));
  CHECK_FALSE(stretch_lut(&h, 0, 1000, nullptr));
  CHECK_FALSE(stretch_lut(&h, 500, 500, lut));
  CHECK_FALSE(stretch_lut(&h, 600, 500, lut));
  CHECK_FALSE(stretch_lut(&h, 0, 1001, lut));
  CHECK_FALSE(stretch_lut(&h, 1000, 1001, lut));
  CHECK_FALSE(stretch_lut(&h, 1001, 1002, lut));
  CHECK_FALSE(stretch_lut(&h, 0, 0, lut));
  CHECK_FALSE(stretch_lut(&empty, 0, 1000, lut));
  CHECK_FALSE(stretch_lut(&empty, 10, 20, lut));
  untouched();
  CHECK(stretch_lut(&h, 999, 1000, lut));
  CHECK(stretch_lut(&h, 0, 1, lut));
}

TEST_CASE("histogram: structural oracle on random images") {
  Lcg r(0xBEEF);
  for (int k = 0; k < 200; ++k) {
    ImgCase c{r.rng(1, 40), r.rng(1, 12), 0, static_cast<int>(r.rng(0, 6)), r.next(), 0, 0};
    c.stride = c.w + r.rng(0, 9);
    const std::vector<u8> buf = make_image(c);
    u32 want[kBins] = {};
    for (u32 y = 0; y < c.h; ++y)
      for (u32 x = 0; x < c.w; ++x) ++want[buf[static_cast<std::size_t>(y) * c.stride + x]];
    Histogram h;
    REQUIRE(build(buf.data(), c.w, c.h, c.stride, &h) == HistResult::Ok);
    for (u32 v = 0; v < kBins; ++v) REQUIRE(h.bins[v] == want[v]);
    REQUIRE(total(&h) == static_cast<u64>(c.w) * c.h);
    u8 prev = 0;
    for (u32 pm = 0; pm <= 1000; pm += 7) {
      u8 p = 0;
      REQUIRE(percentile(&h, pm, &p));
      REQUIRE(h.bins[p] != 0);  // always a used value
      REQUIRE(p >= prev);
      prev = p;
    }
    u8 lut[kBins];
    u8 lo = 0, hi = 0;
    REQUIRE(percentile(&h, 50, &lo));
    REQUIRE(percentile(&h, 950, &hi));
    REQUIRE(stretch_lut(&h, 50, 950, lut));
    for (u32 v = 1; v < kBins; ++v) REQUIRE((lut[v] >= lut[v - 1] || lo == hi));
    if (lo != hi) {
      REQUIRE(lut[lo] == 0);
      REQUIRE(lut[hi] == 255);
    }
  }
}
