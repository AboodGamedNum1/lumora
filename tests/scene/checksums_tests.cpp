// SHA-256 known-answer vectors and checksums.txt rules.
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

#include "checksums.h"

using namespace lumora::scene;

namespace {
std::string h(const std::string& s) { return to_hex(sha256(s.data(), s.size())); }
}

TEST_CASE("sha256 known answers", "[checksums]") {
  CHECK(h("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(h("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(h("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  CHECK(h(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  // Padding boundaries: 55, 56, 63, 64 bytes must not crash and must differ.
  CHECK(h(std::string(55, 'x')) != h(std::string(56, 'x')));
  CHECK(h(std::string(63, 'x')) != h(std::string(64, 'x')));
}

TEST_CASE("checksums.txt parsing and verification", "[checksums]") {
  const std::string abc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  std::map<std::string, Sha256Digest> m;
  std::string err;
  REQUIRE(parse_checksums(abc + "  scene.json\r\n\n" + abc + "  assets/a b.png\n", m, err));
  CHECK(m.size() == 2);
  CHECK(verify_file(m, "scene.json", "abc", 3));
  CHECK_FALSE(verify_file(m, "scene.json", "abd", 3));
  CHECK_FALSE(verify_file(m, "unlisted", "abc", 3));
  CHECK_FALSE(parse_checksums(abc + "  a\n" + abc + "  a\n", m, err));
  CHECK_FALSE(parse_checksums("ABC" + abc.substr(3) + "  a\n", m, err));
  CHECK_FALSE(parse_checksums(abc + " a\n", m, err));
  CHECK_FALSE(parse_checksums(abc + "  \n", m, err));
  CHECK_FALSE(parse_checksums("zz\n", m, err));
  CHECK_FALSE(err.empty());
}
