// Pack the sample scene from disk, write and re-read the archive, load it.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "pack_dir.h"
#include "zip_store.h"

using namespace lumora::scene;

TEST_CASE("sample scene packs, round-trips and loads", "[pack]") {
  const auto p = pack_directory(LUMORA_SAMPLES_DIR "/aurora");
  REQUIRE(p.ok);
  CHECK(p.files.count("checksums.txt") == 1);
  const auto z = write_zip_store(p.files);
  const auto r = read_zip_store(z.data(), z.size());
  REQUIRE(r.ok);
  const auto l = load_package(r.files);
  REQUIRE(l.ok());
  CHECK(l.manifest.name == "Aurora (sample)");
  CHECK(l.scene.assets.size() == 2);
  REQUIRE(l.scene.root.children.size() == 2);
  CHECK(l.scene.root.children[1].parallax_depth == 0.4f);
  // Packing is reproducible.
  CHECK(write_zip_store(pack_directory(LUMORA_SAMPLES_DIR "/aurora").files) == z);
}

TEST_CASE("packer refuses bad sources", "[pack]") {
  namespace fs = std::filesystem;
  CHECK_FALSE(pack_directory("/definitely/not/here").ok);
  const auto d = fs::temp_directory_path() / "lumora_pack_bad";
  fs::remove_all(d);
  fs::create_directories(d);
  {
    std::ofstream(d / "manifest.json") << "{}";
  }
  CHECK_FALSE(pack_directory(d).ok);  // manifest is not valid
  {
    std::ofstream(d / "checksums.txt") << "";
  }
  CHECK_FALSE(pack_directory(d).ok);  // pre-existing checksums.txt
  fs::remove_all(d);
}
