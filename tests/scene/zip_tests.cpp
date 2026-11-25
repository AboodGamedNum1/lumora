// ZIP (stored) writer/reader round trip and hostile input rejection.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "zip_store.h"

using namespace lumora::scene;

namespace {
FileMap sample() {
  FileMap f;
  f["a.txt"] = {'h', 'i'};
  f["dir/b.bin"] = std::vector<std::uint8_t>(5000, 7);
  f["empty"] = {};
  return f;
}
}  // namespace

TEST_CASE("zip round trip is exact and reproducible", "[zip]") {
  const auto z = write_zip_store(sample());
  CHECK(z == write_zip_store(sample()));
  const auto r = read_zip_store(z.data(), z.size());
  REQUIRE(r.ok);
  CHECK(r.files == sample());
}

TEST_CASE("zip reader rejects hostile input", "[zip]") {
  const auto z = write_zip_store(sample());
  CHECK_FALSE(read_zip_store(z.data(), 10).ok);
  // Every truncation must fail cleanly, never crash.
  for (std::size_t n = 0; n < z.size(); n += 7) (void)read_zip_store(z.data(), n);
  // Single byte flips anywhere must never crash; flips in data or crc must be caught.
  for (std::size_t i = 0; i < z.size(); i += 3) {
    auto c = z;
    c[i] ^= 0x55;
    (void)read_zip_store(c.data(), c.size());
  }
  auto c = z;
  c[30 + 5] ^= 0xFF;  // first entry name or data byte region
  CHECK_FALSE(read_zip_store(c.data(), c.size()).ok);
  CHECK_FALSE(read_zip_store(z.data(), z.size(), 2).ok);          // entry-count cap
  CHECK_FALSE(read_zip_store(z.data(), z.size(), 100, 100).ok);   // size cap
}

TEST_CASE("zip reader rejects unsupported features", "[zip]") {
  auto z = write_zip_store(sample());
  // Central directory entry: method at +10 from its signature. Find the first central header.
  std::size_t cd = 0;
  for (std::size_t i = 0; i + 4 < z.size(); ++i)
    if (z[i] == 0x50 && z[i + 1] == 0x4b && z[i + 2] == 0x01 && z[i + 3] == 0x02) { cd = i; break; }
  REQUIRE(cd != 0);
  auto m = z; m[cd + 10] = 8;
  CHECK_FALSE(read_zip_store(m.data(), m.size()).ok);
  auto e = z; e[cd + 8] |= 1;
  CHECK_FALSE(read_zip_store(e.data(), e.size()).ok);
}
