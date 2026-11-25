// Tests for the lumora-atlas command line logic (tools/atlas/atlas_cli.*), run in-process on temporary files.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "atlas_cli.h"
#include "json_value.h"
#include "png.h"

namespace fs = std::filesystem;
using lumora::image::Rgba8;

namespace {

struct TempDir {
  fs::path path;
  TempDir() {
    static std::atomic<int> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path = fs::temp_directory_path() / ("lumora_atlas_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  std::string file(const std::string& name) const { return (path / name).string(); }
};

Rgba8 solid(std::uint32_t w, std::uint32_t h, std::uint8_t v) {
  Rgba8 im;
  im.width = w;
  im.height = h;
  im.rgba.assign(std::size_t{w} * h * 4, v);
  return im;
}

void write_bytes(const std::string& path, const std::vector<std::uint8_t>& b) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

void write_png(const std::string& path, const Rgba8& im) { write_bytes(path, lumora::image::encode_png(im)); }

std::vector<std::uint8_t> read_bytes(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct Run {
  int code;
  std::string out, err;
};

Run run(const std::vector<std::string>& args) {
  std::ostringstream o, e;
  const int code = lumora::atlas_tool::run(args, o, e);
  return {code, o.str(), e.str()};
}

}  // namespace

TEST_CASE("cli: success writes a decodable atlas and a parsable index with base names", "[atlas][cli]") {
  TempDir d;
  fs::create_directories(d.path / "sub");
  write_png(d.file("hero.png"), solid(8, 6, 200));
  write_png(d.file("sub/tree.v2.png"), solid(5, 9, 50));
  write_png(d.file("noext"), solid(2, 2, 9));
  const auto r = run({"--out", d.file("atlas.png"), "--index", d.file("atlas.json"), "--padding", "1", d.file("hero.png"), d.file("sub/tree.v2.png"), d.file("noext")});
  REQUIRE(r.code == 0);
  CHECK(r.err.empty());
  CHECK(r.out.find("wrote") != std::string::npos);
  const auto png = read_bytes(d.file("atlas.png"));
  const auto decoded = lumora::image::decode_png(png.data(), png.size());
  REQUIRE(decoded.ok());
  const auto json_bytes = read_bytes(d.file("atlas.json"));
  const auto parsed = lumora::scene::parse_json(std::string(json_bytes.begin(), json_bytes.end()));
  REQUIRE(parsed.ok);
  CHECK(parsed.value.find("width")->as_number() == decoded.image.width);
  CHECK(parsed.value.find("height")->as_number() == decoded.image.height);
  const auto& entries = parsed.value.find("entries")->as_array();
  REQUIRE(entries.size() == 3);
  CHECK(entries[0].find("name")->as_string() == "hero");
  CHECK(entries[1].find("name")->as_string() == "tree.v2");
  CHECK(entries[2].find("name")->as_string() == "noext");
  // The hero sprite sits where the index says.
  const auto x = static_cast<std::uint32_t>(entries[0].find("x")->as_number());
  const auto y = static_cast<std::uint32_t>(entries[0].find("y")->as_number());
  CHECK(decoded.image.rgba[(std::size_t{y} * decoded.image.width + x) * 4] == 200);
  CHECK_FALSE(fs::exists(d.file("atlas.png.tmp")));
  CHECK_FALSE(fs::exists(d.file("atlas.json.tmp")));
}

TEST_CASE("cli: the same inputs give identical output files, and --pot and --max-size are honoured", "[atlas][cli]") {
  TempDir d;
  write_png(d.file("a.png"), solid(33, 5, 1));
  write_png(d.file("b.png"), solid(10, 10, 2));
  const std::vector<std::string> base = {"--index", d.file("i.json"), "--pot", "--padding", "0", "--max-size", "128", d.file("a.png"), d.file("b.png")};
  std::vector<std::string> a1 = {"--out", d.file("o1.png")}, a2 = {"--out", d.file("o2.png")};
  a1.insert(a1.end(), base.begin(), base.end());
  a2.insert(a2.end(), base.begin(), base.end());
  REQUIRE(run(a1).code == 0);
  const auto json1 = read_bytes(d.file("i.json"));
  REQUIRE(run(a2).code == 0);
  CHECK(read_bytes(d.file("o1.png")) == read_bytes(d.file("o2.png")));
  CHECK(read_bytes(d.file("i.json")) == json1);
  const auto png = read_bytes(d.file("o1.png"));
  const auto dec = lumora::image::decode_png(png.data(), png.size());
  REQUIRE(dec.ok());
  CHECK((dec.image.width & (dec.image.width - 1)) == 0);
  CHECK((dec.image.height & (dec.image.height - 1)) == 0);
  CHECK(dec.image.width <= 128);
}

TEST_CASE("cli: usage errors exit 2", "[atlas][cli]") {
  TempDir d;
  write_png(d.file("a.png"), solid(2, 2, 1));
  const std::string a = d.file("a.png"), o = d.file("o.png"), i = d.file("i.json");
  CHECK(run({}).code == 2);
  CHECK(run({"--index", i, a}).code == 2);                         // no --out
  CHECK(run({"--out", o, a}).code == 2);                           // no --index
  CHECK(run({"--out", o, "--index", i}).code == 2);                // no inputs
  CHECK(run({"--out", o, "--index", i, "--bogus", a}).code == 2);  // unknown option
  CHECK(run({"--out", o, "--index", i, "--max-size", "abc", a}).code == 2);
  CHECK(run({"--out", o, "--index", i, "--padding", "-1", a}).code == 2);
  CHECK(run({"--out", o, "--index", i, "--padding", "", a}).code == 2);
  CHECK(run({"--out", o, "--index", i, "--max-size", "99999999999", a}).code == 2);
  CHECK(run({"--out", o, "--index", i, a, "--max-size"}).code == 2);  // missing value
  CHECK(run({"--out"}).code == 2);
  CHECK(run({"--out", o, "--index", o, a}).code == 2);  // same file twice
  const auto r = run({});
  CHECK(r.err.find("usage") != std::string::npos);
  CHECK_FALSE(fs::exists(o));
  CHECK_FALSE(fs::exists(i));
}

TEST_CASE("cli: packer and input errors exit 1 and write nothing", "[atlas][cli]") {
  TempDir d;
  write_png(d.file("a.png"), solid(8, 8, 1));
  write_png(d.file("b.png"), solid(8, 8, 2));
  fs::create_directories(d.path / "x");
  fs::create_directories(d.path / "y");
  write_png(d.file("x/same.png"), solid(2, 2, 3));
  write_png(d.file("y/same.png"), solid(2, 2, 4));
  write_bytes(d.file("junk.png"), {1, 2, 3, 4, 5});
  write_bytes(d.file("empty.png"), {});
  const std::string o = d.file("o.png"), i = d.file("i.json");
  auto fails = [&](std::vector<std::string> extra) {
    std::vector<std::string> args = {"--out", o, "--index", i};
    args.insert(args.end(), extra.begin(), extra.end());
    const auto r = run(args);
    CHECK(r.code == 1);
    CHECK_FALSE(r.err.empty());
    CHECK_FALSE(fs::exists(o));
    CHECK_FALSE(fs::exists(i));
    CHECK_FALSE(fs::exists(o + ".tmp"));
    CHECK_FALSE(fs::exists(i + ".tmp"));
  };
  fails({d.file("missing.png")});
  fails({d.file("a.png"), d.file("junk.png")});
  fails({d.file("empty.png")});
  fails({d.path.string()});                                  // a directory
  fails({d.file("x/same.png"), d.file("y/same.png")});       // duplicate base names
  fails({"--max-size", "4", d.file("a.png")});               // DoesNotFit
  fails({"--max-size", "0", d.file("a.png")});               // BadLimits
  fails({"--max-size", "16385", d.file("a.png")});
  fails({"--padding", "17", d.file("a.png")});
  fails({"--max-size", "10", "--padding", "2", d.file("a.png")});
}

TEST_CASE("cli: input bounds on file size and count", "[atlas][cli]") {
  TempDir d;
  const std::string big = d.file("big.png");
  {
    std::ofstream f(big, std::ios::binary);
    f.seekp(33ll << 20);
    f.put('x');
  }
  CHECK(run({"--out", d.file("o.png"), "--index", d.file("i.json"), big}).code == 1);
  std::vector<std::string> args = {"--out", d.file("o.png"), "--index", d.file("i.json")};
  for (int k = 0; k < 4097; ++k) args.push_back(d.file("n" + std::to_string(k) + ".png"));
  const auto r = run(args);
  CHECK(r.code == 1);
  CHECK(r.err.find("more than") != std::string::npos);
  CHECK_FALSE(fs::exists(d.file("o.png")));
}

TEST_CASE("cli: a failed run never touches existing outputs, a good run replaces them", "[atlas][cli]") {
  TempDir d;
  write_png(d.file("a.png"), solid(4, 4, 7));
  write_bytes(d.file("keep.png"), {'o', 'l', 'd'});
  write_bytes(d.file("keep.json"), {'o', 'l', 'd', '2'});
  const auto old_png = read_bytes(d.file("keep.png"));
  const auto old_json = read_bytes(d.file("keep.json"));
  CHECK(run({"--out", d.file("keep.png"), "--index", d.file("keep.json"), d.file("a.png"), d.file("nope.png")}).code == 1);
  CHECK(read_bytes(d.file("keep.png")) == old_png);
  CHECK(read_bytes(d.file("keep.json")) == old_json);
  CHECK(run({"--out", d.file("keep.png"), "--index", d.file("keep.json"), "--max-size", "2", d.file("a.png")}).code == 1);
  CHECK(read_bytes(d.file("keep.png")) == old_png);
  CHECK(read_bytes(d.file("keep.json")) == old_json);
  // The index path cannot be written (its parent folder does not exist): the atlas file is not replaced either.
  CHECK(run({"--out", d.file("keep.png"), "--index", d.file("no_such_dir/i.json"), d.file("a.png")}).code == 1);
  CHECK(read_bytes(d.file("keep.png")) == old_png);
  CHECK_FALSE(fs::exists(d.file("keep.png.tmp")));
  // A success replaces both.
  CHECK(run({"--out", d.file("keep.png"), "--index", d.file("keep.json"), d.file("a.png")}).code == 0);
  CHECK(read_bytes(d.file("keep.png")) != old_png);
  CHECK(read_bytes(d.file("keep.json")) != old_json);
}

TEST_CASE("cli: an unwritable output path exits 1", "[atlas][cli]") {
  TempDir d;
  write_png(d.file("a.png"), solid(4, 4, 7));
  fs::create_directories(d.path / "isdir.png");
  CHECK(run({"--out", d.file("isdir.png"), "--index", d.file("i.json"), d.file("a.png")}).code == 1);
  CHECK_FALSE(fs::exists(d.file("i.json")));
}
