// Tests for the raw-DEFLATE module: RFC 1951 known-answer vectors from zlib
// (wbits -15), round trips at every level, determinism, and hostile input.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include "deflate.h"
#include "deflate_fixtures.h"

using lumora::package::InflateStatus;
using lumora::package::deflate_raw;
using lumora::package::inflate_raw;
using namespace lumora::package::test;

namespace {

constexpr std::uint64_t kBigLimit = std::uint64_t{1} << 30;

// Same generator that produced the checked-in random fixtures.
std::vector<std::uint8_t> xorshift_bytes(std::size_t n, std::uint32_t seed = 123456789) {
  std::vector<std::uint8_t> out(n);
  std::uint32_t state = seed;
  for (std::size_t i = 0; i < n; ++i) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    out[i] = static_cast<std::uint8_t>(state & 0xFF);
  }
  return out;
}

std::vector<std::uint8_t> lorem_bytes(std::size_t n) {
  static const char kSentence[] = "The quick brown fox jumps over the lazy dog. ";
  std::vector<std::uint8_t> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = static_cast<std::uint8_t>(kSentence[i % (sizeof(kSentence) - 1)]);
  }
  return out;
}

std::vector<std::uint8_t> as_vector(const std::uint8_t* data, std::size_t size) {
  return std::vector<std::uint8_t>(data, data + size);
}

void check_fixture(const std::uint8_t* raw, std::size_t raw_size,
                   const std::vector<std::uint8_t>& plain) {
  const auto r = inflate_raw(raw, raw_size, kBigLimit);
  REQUIRE(r.status == InflateStatus::Ok);
  CHECK(r.data == plain);
  CHECK(r.consumed == raw_size);
}

}  // namespace

TEST_CASE("inflate decodes zlib-generated known-answer vectors", "[deflate][kat]") {
  check_fixture(kFixtureStored, sizeof(kFixtureStored),
                as_vector(kFixtureStoredPlain, kFixtureStoredPlainSize));
  check_fixture(kFixtureFixed, sizeof(kFixtureFixed),
                as_vector(kFixtureFixedPlain, kFixtureFixedPlainSize));
  check_fixture(kFixtureDynamic, sizeof(kFixtureDynamic), lorem_bytes(kFixtureDynamicPlainSize));
  check_fixture(kFixtureEmpty, sizeof(kFixtureEmpty), {});
  check_fixture(kFixtureOneByte, sizeof(kFixtureOneByte),
                as_vector(kFixtureOneBytePlain, kFixtureOneBytePlainSize));
  check_fixture(kFixture65535Zeros, sizeof(kFixture65535Zeros),
                std::vector<std::uint8_t>(kFixture65535ZerosPlainSize, 0));
  check_fixture(kFixture65536Zeros, sizeof(kFixture65536Zeros),
                std::vector<std::uint8_t>(kFixture65536ZerosPlainSize, 0));
  std::vector<std::uint8_t> stored_plain;
  const std::vector<std::uint8_t> block = xorshift_bytes(4096);
  for (int i = 0; i < 16; ++i) {
    stored_plain.insert(stored_plain.end(), block.begin(), block.end());
  }
  check_fixture(kFixture65536Stored, sizeof(kFixture65536Stored), stored_plain);
  check_fixture(kFixtureMatch258, sizeof(kFixtureMatch258),
                std::vector<std::uint8_t>(kFixtureMatch258PlainSize, 'A'));
  check_fixture(kFixtureDist32768, sizeof(kFixtureDist32768),
                std::vector<std::uint8_t>(kFixtureDist32768PlainSize, 'B'));
  check_fixture(kFixtureRandom4k, sizeof(kFixtureRandom4k),
                xorshift_bytes(kFixtureRandom4kPlainSize));
}

TEST_CASE("inflate reports consumed bytes and tolerates trailing input", "[deflate]") {
  std::vector<std::uint8_t> with_tail(kFixtureFixed, kFixtureFixed + sizeof(kFixtureFixed));
  with_tail.push_back(0xDE);
  with_tail.push_back(0xAD);
  const auto r = inflate_raw(with_tail.data(), with_tail.size(), kBigLimit);
  REQUIRE(r.status == InflateStatus::Ok);
  CHECK(r.consumed == sizeof(kFixtureFixed));
}

TEST_CASE("deflate round-trips through inflate at every level", "[deflate][roundtrip]") {
  const std::vector<std::pair<std::string, std::vector<std::uint8_t>>> corpora = {
      {"empty", {}},
      {"one byte", {'x'}},
      {"zeros", std::vector<std::uint8_t>(100000, 0)},
      {"random", xorshift_bytes(100000, 42)},
      {"repetitive text", lorem_bytes(100000)},
      {"binary mix", [] {
         std::vector<std::uint8_t> mix(100000);
         for (std::size_t i = 0; i < mix.size(); ++i) {
           mix[i] = static_cast<std::uint8_t>(((i * 7) ^ (i >> 3)) & 0xFF);
         }
         return mix;
       }()},
      {"stored boundary 65535", xorshift_bytes(65535, 7)},
      {"stored boundary 65536", xorshift_bytes(65536, 7)},
      {"stored boundary 131073", xorshift_bytes(131073, 7)},
  };
  for (const auto& [name, plain] : corpora) {
    for (int level : {0, 1, 6}) {
      INFO("corpus: " << name << ", level: " << level);
      const std::vector<std::uint8_t> packed = deflate_raw(plain.data(), plain.size(), level);
      const auto r = inflate_raw(packed.data(), packed.size(), kBigLimit);
      REQUIRE(r.status == InflateStatus::Ok);
      CHECK(r.data == plain);
      CHECK(r.consumed == packed.size());
    }
  }
}

TEST_CASE("deflate output is deterministic", "[deflate][determinism]") {
  const std::vector<std::uint8_t> plain = lorem_bytes(50000);
  for (int level : {0, 1, 6}) {
    INFO("level: " << level);
    const std::vector<std::uint8_t> a = deflate_raw(plain.data(), plain.size(), level);
    const std::vector<std::uint8_t> b = deflate_raw(plain.data(), plain.size(), level);
    CHECK(a == b);
  }
}

TEST_CASE("deflate clamps unknown levels to the nearest supported one", "[deflate]") {
  const std::vector<std::uint8_t> plain = lorem_bytes(4096);
  const std::vector<std::uint8_t> ref0 = deflate_raw(plain.data(), plain.size(), 0);
  const std::vector<std::uint8_t> ref1 = deflate_raw(plain.data(), plain.size(), 1);
  const std::vector<std::uint8_t> ref6 = deflate_raw(plain.data(), plain.size(), 6);
  CHECK(deflate_raw(plain.data(), plain.size(), -9) == ref0);
  CHECK(deflate_raw(plain.data(), plain.size(), 3) == ref1);
  CHECK(deflate_raw(plain.data(), plain.size(), 4) == ref6);
  CHECK(deflate_raw(plain.data(), plain.size(), 99) == ref6);
}

namespace {

// LSB-first bit writer for hand-crafting streams.
struct TestBits {
  std::vector<std::uint8_t> out;
  std::uint64_t buf = 0;
  int count = 0;
  void put(std::uint32_t v, int n) {
    buf |= static_cast<std::uint64_t>(v) << count;
    count += n;
    while (count >= 8) {
      out.push_back(static_cast<std::uint8_t>(buf & 0xFF));
      buf >>= 8;
      count -= 8;
    }
  }
  void put_code(std::uint32_t code, int n) {  // canonical code, MSB-first
    std::uint32_t rev = 0;
    for (int i = 0; i < n; ++i) {
      rev |= ((code >> i) & 1u) << (n - 1 - i);
    }
    put(rev, n);
  }
  std::vector<std::uint8_t> finish() {
    if (count > 0) {
      out.push_back(static_cast<std::uint8_t>(buf & 0xFF));
    }
    return out;
  }
};

void fixed_lit(TestBits* w, std::uint32_t sym) {
  if (sym < 144) {
    w->put_code(0x30 + sym, 8);
  } else if (sym < 256) {
    w->put_code(0x190 + sym - 144, 9);
  } else if (sym < 280) {
    w->put_code(sym - 256, 7);
  } else {
    w->put_code(0xC0 + sym - 280, 8);
  }
}

InflateStatus status_of(const std::vector<std::uint8_t>& stream) {
  return inflate_raw(stream.data(), stream.size(), kBigLimit).status;
}

}  // namespace

TEST_CASE("inflate rejects truncation at every byte offset", "[deflate][hostile]") {
  const std::pair<const std::uint8_t*, std::size_t> streams[] = {
      {kFixtureStored, sizeof(kFixtureStored)},
      {kFixtureDynamic, sizeof(kFixtureDynamic)},
      {kFixtureMatch258, sizeof(kFixtureMatch258)},
  };
  for (const auto& [raw, size] : streams) {
    for (std::size_t cut = 0; cut < size; ++cut) {
      INFO("prefix length " << cut << " of " << size);
      const auto r = inflate_raw(raw, cut, kBigLimit);
      CHECK(r.status != InflateStatus::Ok);
    }
  }
  CHECK(inflate_raw(nullptr, 0, kBigLimit).status == InflateStatus::Truncated);
}

TEST_CASE("inflate survives bit flips without crashing", "[deflate][hostile]") {
  const std::pair<const std::uint8_t*, std::size_t> streams[] = {
      {kFixtureFixed, sizeof(kFixtureFixed)},
      {kFixtureDynamic, sizeof(kFixtureDynamic)},
  };
  for (const auto& [raw, size] : streams) {
    for (std::size_t bit = 0; bit < size * 8; ++bit) {
      std::vector<std::uint8_t> mutated(raw, raw + size);
      mutated[bit / 8] ^= static_cast<std::uint8_t>(1u << (bit % 8));
      // Any status is acceptable; this must simply never crash or overflow.
      const auto r = inflate_raw(mutated.data(), mutated.size(), kBigLimit);
      (void)r;
    }
  }
  SUCCEED("all bit flips handled");
}

TEST_CASE("inflate rejects the reserved block type", "[deflate][hostile]") {
  TestBits w;
  w.put(1, 1);  // BFINAL
  w.put(3, 2);  // reserved BTYPE
  CHECK(status_of(w.finish()) == InflateStatus::BadBlockType);
}

TEST_CASE("inflate rejects a stored block whose LEN and NLEN disagree", "[deflate][hostile]") {
  TestBits w;
  w.put(1, 1);
  w.put(0, 2);
  while (w.count % 8 != 0) {
    w.put(0, 1);
  }
  w.put(5, 16);   // LEN
  w.put(0, 16);   // NLEN should be ~5
  CHECK(status_of(w.finish()) == InflateStatus::BadStoredLen);
}

TEST_CASE("inflate rejects over-subscribed and incomplete Huffman codes", "[deflate][hostile]") {
  {
    // Code-length code with nineteen 1-bit codes: wildly over-subscribed.
    TestBits w;
    w.put(1, 1);
    w.put(2, 2);    // dynamic
    w.put(0, 5);    // HLIT 257
    w.put(0, 5);    // HDIST 1
    w.put(15, 4);   // HCLEN 19
    for (int i = 0; i < 19; ++i) {
      w.put(1, 3);
    }
    CHECK(status_of(w.finish()) == InflateStatus::BadHuffman);
  }
  {
    // Literal/length lengths 1, 2, (254 zeros), 2: an incomplete code.
    TestBits w;
    w.put(1, 1);
    w.put(2, 2);
    w.put(0, 5);    // HLIT 257
    w.put(0, 5);    // HDIST 1
    w.put(14, 4);   // HCLEN 18 (covers order positions 0..17)
    const std::uint8_t cl[18] = {0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
    for (std::uint8_t v : cl) {
      w.put(v, 3);
    }
    // cl symbols 0, 1, 2, 18 get 2-bit canonical codes 00, 01, 10, 11.
    auto cl_code = [](int sym) -> std::pair<std::uint32_t, int> {
      switch (sym) {
        case 0: return {0, 2};
        case 1: return {1, 2};
        case 2: return {2, 2};
        default: return {3, 2};  // 18
      }
    };
    auto emit = [&](int sym, std::uint32_t extra = 0, int extra_bits = 0) {
      auto [code, len] = cl_code(sym);
      w.put_code(code, len);
      if (extra_bits != 0) {
        w.put(extra, extra_bits);
      }
    };
    emit(1);              // literal 0: length 1
    emit(18, 127, 7);     // 138 zeros
    emit(18, 106, 7);     // 117 zeros (255 total, literals 1..255)
    emit(2);              // literal 256: length 2
    emit(0);              // single distance length 0
    CHECK(status_of(w.finish()) == InflateStatus::BadHuffman);
  }
}

TEST_CASE("inflate rejects invalid literal/length and distance symbols", "[deflate][hostile]") {
  {
    // Fixed block whose first symbol is 286 (exists in the code, invalid).
    TestBits w;
    w.put(1, 1);
    w.put(1, 2);
    fixed_lit(&w, 286);
    fixed_lit(&w, 256);
    CHECK(status_of(w.finish()) == InflateStatus::BadSymbol);
  }
  {
    // Fixed block using distance symbol 30 (exists in the code, invalid).
    TestBits w;
    w.put(1, 1);
    w.put(1, 2);
    fixed_lit(&w, 'A');
    fixed_lit(&w, 257);  // length 3
    w.put_code(30, 5);   // invalid distance symbol
    fixed_lit(&w, 256);
    CHECK(status_of(w.finish()) == InflateStatus::BadSymbol);
  }
}

TEST_CASE("inflate rejects a distance reaching before the output start", "[deflate][hostile]") {
  TestBits w;
  w.put(1, 1);
  w.put(1, 2);
  fixed_lit(&w, 'A');
  fixed_lit(&w, 257);  // length 3
  w.put_code(1, 5);    // distance 2 with only 1 byte of output
  fixed_lit(&w, 256);
  CHECK(status_of(w.finish()) == InflateStatus::BadDistance);
}

TEST_CASE("inflate enforces max_output before growing", "[deflate][hostile]") {
  const std::vector<std::uint8_t> zeros(16 << 20, 0);  // 16 MiB, compresses to KiBs
  const std::vector<std::uint8_t> bomb = deflate_raw(zeros.data(), zeros.size(), 6);
  REQUIRE(bomb.size() < (1u << 16));
  const auto r = inflate_raw(bomb.data(), bomb.size(), 1 << 20);
  CHECK(r.status == InflateStatus::OutputLimit);
  CHECK(r.data.empty());
  CHECK(inflate_raw(kFixtureStored, sizeof(kFixtureStored), 5).status ==
        InflateStatus::OutputLimit);
}

TEST_CASE("performance sanity: 16 MiB inflate and level-6 deflate", "[deflate][perf]") {
  std::vector<std::uint8_t> plain(16u << 20);
  const std::vector<std::uint8_t> pattern = lorem_bytes(1 << 16);
  for (std::size_t off = 0; off < plain.size(); off += pattern.size()) {
    std::memcpy(plain.data() + off, pattern.data(),
                std::min(pattern.size(), plain.size() - off));
  }
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<std::uint8_t> packed = deflate_raw(plain.data(), plain.size(), 6);
  const auto t1 = std::chrono::steady_clock::now();
  const auto r = inflate_raw(packed.data(), packed.size(), kBigLimit);
  const auto t2 = std::chrono::steady_clock::now();
  REQUIRE(r.status == InflateStatus::Ok);
  CHECK(r.data == plain);
  std::cerr << "level-6 deflate of 16 MiB: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
            << " ms (" << packed.size() << " bytes); inflate: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms"
            << std::endl;
}
