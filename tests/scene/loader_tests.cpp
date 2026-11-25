// Loader tests over a hand-built in-memory package (no ZIP, no file system).
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "checksums.h"
#include "scene.h"

using namespace lumora::scene;

namespace {
const char* kManifest = R"({"format_version":1,"scene_schema_version":1,"lua_api_version":1,
 "min_engine_version":"0.0.1","name":"Sample","entry":{"scene":"scene.json"}})";
const char* kScene = R"({"assets":[{"id":"sky","kind":"texture","path":"assets/sky.png"}],
 "root":{"id":"root","children":[
  {"id":"bg","name":"Sky","layer":{"asset":"sky","parallax_depth":0.8},"transform":{"position":[0,0,-1],"scale":[2,2,1]}},
  {"id":"fg","transform":{"rotation_z_degrees":15}}]}})";

void put(FileMap& f, const std::string& p, const std::string& s) { f[p] = std::vector<std::uint8_t>(s.begin(), s.end()); }

FileMap build() {
  FileMap f;
  put(f, "manifest.json", kManifest);
  put(f, "scene.json", kScene);
  put(f, "assets/sky.png", "not really a png");
  std::string sums;
  for (const auto& [p, d] : f) sums += to_hex(sha256(d.data(), d.size())) + "  " + p + "\n";
  put(f, "checksums.txt", sums);
  return f;
}
}  // namespace

TEST_CASE("a well-formed package loads", "[loader]") {
  auto r = load_package(build());
  REQUIRE(r.ok());
  CHECK(r.manifest.name == "Sample");
  REQUIRE(r.scene.root.children.size() == 2);
  CHECK(r.scene.root.children[0].layer_asset == "sky");
  CHECK(r.scene.root.children[0].parallax_depth == 0.8f);
  CHECK(r.scene.root.children[0].transform.scale[0] == 2.0f);
  CHECK(r.scene.root.children[1].transform.rotation_z_degrees == 15.0f);
}

TEST_CASE("verification happens before parsing", "[loader]") {
  { auto f = build(); f.erase("checksums.txt"); CHECK(load_package(f).error == LoadError::MissingChecksums); }
  { auto f = build(); put(f, "assets/sky.png", "tampered"); CHECK(load_package(f).error == LoadError::ChecksumMismatch); }
  { auto f = build(); put(f, "extra.bin", "x"); CHECK(load_package(f).error == LoadError::UnlistedFile); }
  { auto f = build(); f.erase("assets/sky.png"); CHECK(load_package(f).error == LoadError::MissingFile); }
  { auto f = build(); put(f, "checksums.txt", "garbage\n"); CHECK(load_package(f).error == LoadError::BadChecksums); }
  // A broken manifest that is correctly checksummed is still rejected, after verification.
  {
    auto f = build();
    put(f, "manifest.json", "{");
    std::string sums;
    for (const auto& [p, d] : f) if (p != "checksums.txt") sums += to_hex(sha256(d.data(), d.size())) + "  " + p + "\n";
    put(f, "checksums.txt", sums);
    CHECK(load_package(f).error == LoadError::BadManifest);
  }
}

TEST_CASE("a signature block is rejected, never ignored", "[loader]") {
  auto f = build();
  put(f, kSignatureEntry, "sig");
  CHECK(load_package(f).error == LoadError::SignatureUnsupported);
}

TEST_CASE("scene.json rules", "[loader]") {
  CHECK(parse_scene(R"({"root":{"id":"a","children":[{"id":"a"}]}})").error == SceneError::DuplicateId);
  CHECK(parse_scene(R"({"root":{"id":"a","layer":{"asset":"nope"}}})").error == SceneError::UnknownAsset);
  CHECK(parse_scene(R"({"root":{"name":"x"}})").error == SceneError::MissingField);
  CHECK(parse_scene(R"({})").error == SceneError::MissingField);
  CHECK(parse_scene(R"({"root":{"id":"a","transform":{"position":[1,2]}}})").error == SceneError::WrongType);
  CHECK(parse_scene(R"({"assets":[{"id":"s","kind":"texture","path":"p"}],"root":{"id":"a","layer":{"asset":"s","parallax_depth":2}}})").error == SceneError::BadValue);
  CHECK(parse_scene(R"({"root":{"id":"a","children":[{"id":"b","children":[{"id":"c"}]}]}})", 2).error == SceneError::TooDeep);
  CHECK(parse_scene("nope").error == SceneError::NotJson);
  CHECK(parse_scene(R"({"root":{"id":"a","future":1}})").ok());
}
