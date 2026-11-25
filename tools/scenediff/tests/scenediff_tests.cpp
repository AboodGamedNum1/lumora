// Tests for lumora-scenediff. Packages are built in memory or in temporary directories; samples are only read.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "checksums.h"
#include "pack_dir.h"
#include "scene_diff.h"
#include "zip_store.h"

namespace fs = std::filesystem;
using namespace lumora::scenediff;
using lumora::scene::FileMap;

namespace {

std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

std::string f2s(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
  return buf;
}

// A scene node model that is easy to mutate and to write as scene.json. Values are multiples of 0.125 so the text
// round-trips exactly through the JSON parser.
struct TNode {
  std::string id, name, asset;
  float pos[3] = {0, 0, 0}, scale[3] = {1, 1, 1}, rot = 0, depth = 0;
  std::vector<TNode> children;
};

TNode leaf(const std::string& id, const std::string& name, const std::string& asset) {
  TNode n;
  n.id = id;
  n.name = name;
  n.asset = asset;
  return n;
}

std::string node_json(const TNode& n) {
  std::string j = "{\"id\":\"" + n.id + "\",\"name\":\"" + n.name + "\",\"transform\":{\"position\":[" + f2s(n.pos[0]) +
                  "," + f2s(n.pos[1]) + "," + f2s(n.pos[2]) + "],\"scale\":[" + f2s(n.scale[0]) + "," + f2s(n.scale[1]) +
                  "," + f2s(n.scale[2]) + "],\"rotation_z_degrees\":" + f2s(n.rot) + "}";
  if (!n.asset.empty()) j += ",\"layer\":{\"asset\":\"" + n.asset + "\",\"parallax_depth\":" + f2s(n.depth) + "}";
  j += ",\"children\":[";
  for (std::size_t i = 0; i < n.children.size(); ++i) j += (i ? "," : "") + node_json(n.children[i]);
  return j + "]}";
}

std::string scene_json(const TNode& root, const std::vector<std::pair<std::string, std::string>>& assets) {
  std::string j = "{\"assets\":[";
  for (std::size_t i = 0; i < assets.size(); ++i)
    j += std::string(i ? "," : "") + "{\"id\":\"" + assets[i].first + "\",\"kind\":\"texture\",\"path\":\"" +
         assets[i].second + "\"}";
  return j + "],\"root\":" + node_json(root) + "}";
}

std::string manifest_json(const std::string& name = "Test", const std::string& author = "Me",
                          const std::string& extra = "") {
  return "{\"format_version\":1,\"scene_schema_version\":1,\"lua_api_version\":1,\"min_engine_version\":\"0.0.1\","
         "\"name\":\"" + name + "\",\"author\":\"" + author + "\",\"entry\":{\"scene\":\"scene.json\"}" + extra + "}";
}

TNode base_tree() {
  TNode root;
  root.id = "root";
  TNode a;
  a.id = "sky";
  a.name = "Sky";
  a.asset = "tex";
  a.depth = 1;
  TNode b;
  b.id = "hills";
  b.name = "Hills";
  b.asset = "tex";
  b.depth = 0.5f;
  b.pos[1] = -0.25f;
  root.children = {a, b};
  return root;
}

using Assets = std::vector<std::pair<std::string, std::string>>;
const Assets kAssets = {{"tex", "assets/tex.ppm"}};

FileMap with_checksums(FileMap files) {
  files.erase("checksums.txt");
  std::string sums;
  for (const auto& [p, d] : files) sums += lumora::scene::to_hex(lumora::scene::sha256(d.data(), d.size())) + "  " + p + "\n";
  files["checksums.txt"] = bytes(sums);
  return files;
}

FileMap make_files(const std::string& manifest, const std::string& scene, const FileMap& extra = {}) {
  FileMap f = extra;
  f["manifest.json"] = bytes(manifest);
  f["scene.json"] = bytes(scene);
  if (!f.count("assets/tex.ppm")) f["assets/tex.ppm"] = bytes("P6\n1 1\n255\n\x10\x20\x30");
  return with_checksums(f);
}

FileMap base_files() { return make_files(manifest_json(), scene_json(base_tree(), kAssets)); }

LoadedPackage load(const FileMap& files, const char* side = "A") {
  LoadOutcome o = load_files(files, side);
  INFO(o.error);
  REQUIRE(o.ok);
  return o.package;
}

DiffResult diff_of(const FileMap& a, const FileMap& b) { return diff_packages(load(a, "A"), load(b, "B")); }

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> v;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const auto nl = text.find('\n', pos);
    v.push_back(text.substr(pos, nl - pos));
    pos = nl == std::string::npos ? text.size() : nl + 1;
  }
  return v;
}

struct TempDir {
  fs::path path;
  TempDir() {
    static std::atomic<unsigned> counter{0};
    path = fs::temp_directory_path() /
           ("lumora_scenediff_" + std::to_string(counter++) + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
    fs::remove_all(path);
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

void write_tree(const fs::path& dir, const FileMap& files) {
  for (const auto& [p, d] : files) {
    if (p == "checksums.txt") continue;
    fs::create_directories((dir / p).parent_path());
    std::ofstream f(dir / p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
  }
}

int cli(const std::vector<std::string>& args, std::string* out = nullptr, std::string* err = nullptr) {
  std::string o, e;
  const int code = run_cli(args, o, e);
  if (out) *out = o;
  if (err) *err = e;
  return code;
}

}  // namespace

TEST_CASE("identical packages report no findings", "[scenediff]") {
  const DiffResult r = diff_of(base_files(), base_files());
  CHECK(r.identical());
  CHECK(format_text(r) == "identical\n");
}

TEST_CASE("file addition, removal and change are reported by path, checksums.txt excluded", "[scenediff]") {
  FileMap a = base_files();
  a["docs/old.txt"] = bytes("old");
  a["keep.txt"] = bytes("same");
  a = with_checksums(a);
  FileMap b = base_files();
  b["docs/new.txt"] = bytes("new");
  b["keep.txt"] = bytes("same");
  b["assets/tex.ppm"] = bytes("P6\n1 1\n255\n\x11\x22\x33");
  b = with_checksums(b);
  const auto text = lines(format_text(diff_of(a, b)));
  REQUIRE(text.size() == 3);
  CHECK(text[0].rfind("~ file assets/tex.ppm sha256: ", 0) == 0);
  CHECK(text[1] == "+ file docs/new.txt");
  CHECK(text[2] == "- file docs/old.txt");
}

TEST_CASE("every manifest field change is reported old to new", "[scenediff]") {
  const std::string scene = scene_json(base_tree(), kAssets);
  std::string ma = manifest_json("Test", "Me", ",\"backends\":[\"vulkan\",\"gles\"]");
  ma.replace(ma.find("0.0.1"), 5, "0.0.0");
  const FileMap a = make_files(ma, scene);
  const FileMap b = make_files(
      "{\"format_version\":1,\"scene_schema_version\":1,\"lua_api_version\":1,\"min_engine_version\":\"0.0.1\","
      "\"name\":\"Other \\\"name\\\"\",\"entry\":{\"scene\":\"scene.json\",\"settings\":\"settings.json\"},"
      "\"backends\":[\"vulkan\"]}",
      scene, {{"settings.json", bytes("{}")}});
  const auto text = lines(format_text(diff_of(a, b)));
  bool backends = false, author = false, engine = false, name = false, settings = false, file_added = false;
  for (const auto& l : text) {
    backends |= l == "~ manifest backends: [gles, vulkan] -> [vulkan]";
    author |= l == "~ manifest author: \"Me\" -> \"\"";
    engine |= l == "~ manifest min_engine_version: 0.0.0 -> 0.0.1";
    name |= l == "~ manifest name: \"Test\" -> \"Other \\\"name\\\"\"";
    settings |= l == "~ manifest settings_path: \"\" -> \"settings.json\"";
    file_added |= l == "+ file settings.json";
  }
  CHECK(backends);
  CHECK(author);
  CHECK(engine);
  CHECK(name);
  CHECK(settings);
  CHECK(file_added);
  CHECK(text.size() == 7);  // the six above plus manifest.json changed
}

TEST_CASE("manifest version numbers and scene path changes are reported", "[scenediff]") {
  const std::string scene = scene_json(base_tree(), kAssets);
  const FileMap a = make_files(manifest_json(), scene);
  FileMap extra;
  extra["other.json"] = bytes(scene);
  const FileMap b = make_files(
      "{\"format_version\":1,\"scene_schema_version\":1,\"lua_api_version\":1,\"min_engine_version\":\"0.0.1\","
      "\"name\":\"Test\",\"author\":\"Me\",\"entry\":{\"scene\":\"other.json\"}}",
      scene, extra);
  const auto text = lines(format_text(diff_of(a, b)));
  bool path = false;
  for (const auto& l : text) path |= l == "~ manifest scene_path: \"scene.json\" -> \"other.json\"";
  CHECK(path);
}

TEST_CASE("node addition, removal and field changes are reported with id paths", "[scenediff]") {
  TNode ta = base_tree();
  TNode tb = base_tree();
  ta.children[0].children.push_back(leaf("cloud", "Cloud", "tex"));
  tb.children[1].children.push_back(leaf("bird", "Bird", ""));
  tb.children[0].pos[0] = 0.5f;
  tb.children[0].scale[1] = 2;
  tb.children[0].rot = 45;
  tb.children[0].depth = 0.25f;
  tb.children[0].name = "Sky 2";
  tb.children[1].asset = "tex2";
  const Assets assets = {{"tex", "assets/tex.ppm"}, {"tex2", "assets/tex2.ppm"}};
  FileMap extra;
  extra["assets/tex2.ppm"] = bytes("P6\n1 1\n255\n\x01\x02\x03");
  const FileMap a = make_files(manifest_json(), scene_json(ta, assets), extra);
  const FileMap b = make_files(manifest_json(), scene_json(tb, assets), extra);
  const auto text = lines(format_text(diff_of(a, b)));
  const std::set<std::string> got(text.begin(), text.end());
  CHECK(got.count("+ node root/hills/bird"));
  CHECK(got.count("- node root/sky/cloud"));
  CHECK(got.count("~ node root/sky position: [0, 0, 0] -> [0.5, 0, 0]"));
  CHECK(got.count("~ node root/sky scale: [1, 1, 1] -> [1, 2, 1]"));
  CHECK(got.count("~ node root/sky rotation_z_degrees: 0 -> 45"));
  CHECK(got.count("~ node root/sky parallax_depth: 1 -> 0.25"));
  CHECK(got.count("~ node root/sky name: \"Sky\" -> \"Sky 2\""));
  CHECK(got.count("~ node root/hills layer_asset: \"tex\" -> \"tex2\""));
  std::size_t node_lines = 0;
  for (const auto& l : text) node_lines += l.find(" node ") != std::string::npos;
  CHECK(node_lines == 8);
}

TEST_CASE("a node moved to another parent is removed plus added; sibling order is reported", "[scenediff]") {
  TNode ta = base_tree();
  TNode tb = base_tree();
  std::swap(tb.children[0], tb.children[1]);  // order only
  {
    const auto text = lines(format_text(diff_of(make_files(manifest_json(), scene_json(ta, kAssets)),
                                                make_files(manifest_json(), scene_json(tb, kAssets)))));
    bool order = false;
    for (const auto& l : text) order |= l == "~ node root children_order: [sky, hills] -> [hills, sky]";
    CHECK(order);
  }
  tb = base_tree();
  TNode moved = tb.children[0];
  tb.children.erase(tb.children.begin());
  tb.children[0].children.push_back(moved);  // sky now lives under hills
  const auto text = lines(format_text(diff_of(make_files(manifest_json(), scene_json(ta, kAssets)),
                                              make_files(manifest_json(), scene_json(tb, kAssets)))));
  const std::set<std::string> got(text.begin(), text.end());
  CHECK(got.count("- node root/sky"));
  CHECK(got.count("+ node root/hills/sky"));
}

TEST_CASE("asset addition, removal and path change are reported", "[scenediff]") {
  const Assets before = {{"tex", "assets/tex.ppm"}, {"gone", "assets/gone.ppm"}};
  const Assets after = {{"tex", "assets/tex2.ppm"}, {"fresh", "assets/fresh.ppm"}};
  FileMap files;
  for (const char* p : {"assets/gone.ppm", "assets/tex2.ppm", "assets/fresh.ppm"}) files[p] = bytes("P6\n1 1\n255\n\x01\x02\x03");
  const FileMap a = make_files(manifest_json(), scene_json(base_tree(), before), files);
  const FileMap b = make_files(manifest_json(), scene_json(base_tree(), after), files);
  const auto text = lines(format_text(diff_of(a, b)));
  const std::set<std::string> got(text.begin(), text.end());
  CHECK(got.count("+ asset fresh kind=texture path=assets/fresh.ppm"));
  CHECK(got.count("- asset gone kind=texture path=assets/gone.ppm"));
  CHECK(got.count("~ asset tex path: \"assets/tex.ppm\" -> \"assets/tex2.ppm\""));
}

TEST_CASE("a directory and its packed archive are identical", "[scenediff]") {
  TempDir tmp;
  const fs::path dir = tmp.path / "pkg";
  write_tree(dir, base_files());
  const auto packed = lumora::scene::pack_directory(dir);
  REQUIRE(packed.ok);
  const auto zip = lumora::scene::write_zip(packed.files, 6);
  const fs::path archive = tmp.path / "pkg.lumora";
  {
    std::ofstream f(archive, std::ios::binary);
    f.write(reinterpret_cast<const char*>(zip.data()), static_cast<std::streamsize>(zip.size()));
  }
  const LoadOutcome a = load_input(dir.string(), "A");
  const LoadOutcome b = load_input(archive.string(), "B");
  REQUIRE(a.ok);
  REQUIRE(b.ok);
  CHECK(diff_packages(a.package, b.package).identical());
  CHECK(cli({dir.string(), archive.string(), "--quiet"}) == 0);
}

TEST_CASE("samples/aurora and samples/aurora.lumora are identical", "[scenediff]") {
  const LoadOutcome a = load_input(LUMORA_SAMPLES_DIR "/aurora", "A");
  const LoadOutcome b = load_input(LUMORA_SAMPLES_DIR "/aurora.lumora", "B");
  INFO(a.error);
  INFO(b.error);
  REQUIRE(a.ok);
  REQUIRE(b.ok);
  CHECK(diff_packages(a.package, b.package).identical());
  CHECK(cli({LUMORA_SAMPLES_DIR "/aurora", LUMORA_SAMPLES_DIR "/aurora.lumora"}) == 0);
}

TEST_CASE("a broken package is a clean error naming the side", "[scenediff]") {
  FileMap tampered = base_files();
  tampered["scene.json"] = bytes("{}");  // checksum no longer matches
  const LoadOutcome bad = load_files(tampered, "B");
  CHECK_FALSE(bad.ok);
  CHECK(bad.error.rfind("B: ", 0) == 0);
  CHECK(bad.error.find("scene.json") != std::string::npos);

  const LoadOutcome empty = load_files({}, "A");
  CHECK_FALSE(empty.ok);
  CHECK(empty.error.rfind("A: ", 0) == 0);

  TempDir tmp;
  const fs::path junk = tmp.path / "junk.lumora";
  {
    std::ofstream f(junk, std::ios::binary);
    f << "this is not a zip file";
  }
  const LoadOutcome j = load_input(junk.string(), "A");
  CHECK_FALSE(j.ok);
  CHECK(j.error.rfind("A (" + junk.string() + "): ", 0) == 0);
  const LoadOutcome missing = load_input((tmp.path / "nope").string(), "B");
  CHECK_FALSE(missing.ok);
  CHECK(missing.error.rfind("B (", 0) == 0);

  std::string out, err;
  const std::string good = LUMORA_SAMPLES_DIR "/aurora";
  CHECK(cli({good, junk.string()}, &out, &err) == 2);
  CHECK(out.empty());  // never a partial diff
  CHECK(err.rfind("error: B (", 0) == 0);
}

TEST_CASE("output is deterministic byte for byte", "[scenediff]") {
  TNode tb = base_tree();
  tb.children[0].pos[0] = 0.5f;
  tb.children.push_back(leaf("extra", "Extra", ""));
  const FileMap a = base_files();
  const FileMap b = make_files(manifest_json("Changed"), scene_json(tb, kAssets), {{"z.txt", bytes("z")}});
  const std::string t1 = format_text(diff_of(a, b)), t2 = format_text(diff_of(a, b));
  const std::string j1 = format_json(diff_of(a, b), "a", "b"), j2 = format_json(diff_of(a, b), "a", "b");
  CHECK(t1 == t2);
  CHECK(j1 == j2);
  CHECK_FALSE(t1.empty());
  // Sorted: files first, then manifest, node, asset.
  const auto l = lines(t1);
  std::vector<std::string> cats;
  for (const auto& x : l) cats.push_back(x.substr(2, x.find(' ', 2) - 2));
  CHECK(std::is_sorted(cats.begin(), cats.end(), [](const std::string& p, const std::string& q) {
    auto rank = [](const std::string& c) { return c == "file" ? 0 : c == "manifest" ? 1 : c == "node" ? 2 : 3; };
    return rank(p) < rank(q);
  }));
}

TEST_CASE("json escaping covers quotes, backslashes and control characters", "[scenediff]") {
  CHECK(json_escape("plain") == "plain");
  CHECK(json_escape("a\"b") == "a\\\"b");
  CHECK(json_escape("a\\b") == "a\\\\b");
  CHECK(json_escape("l1\nl2\r\t\b\f") == "l1\\nl2\\r\\t\\b\\f");
  CHECK(json_escape(std::string("x\x01y\x1f", 4)) == "x\\u0001y\\u001f");
  CHECK(json_escape("\x7f") == "\\u007f");
  CHECK(json_escape("caf\xc3\xa9") == "caf\xc3\xa9");  // valid UTF-8 passes through
  CHECK(json_escape("bad\xff") == "bad\\ufffd");       // invalid UTF-8 is replaced
  CHECK(json_escape("\xc3") == "\\ufffd");             // truncated sequence

  DiffResult r;
  r.findings.push_back(Finding{'~', "manifest", "name", "", "a\"b\\c\n", "tab\there", true});
  const std::string j = format_json(r, "in\"put", "b\\path");
  CHECK(j.find("\"a\": \"in\\\"put\"") != std::string::npos);
  CHECK(j.find("\"b\": \"b\\\\path\"") != std::string::npos);
  CHECK(j.find("\"new\": \"tab\\there\"") != std::string::npos);
  CHECK(j.find("\"old\": \"a\\\"b\\\\c\\n\"") != std::string::npos);
  CHECK(j.find("\"identical\": false") != std::string::npos);
  // Text output keeps one finding per line even with a newline in a value.
  CHECK(lines(format_text(r)).size() == 1);
}

TEST_CASE("cli: exit codes, formats, quiet and usage", "[scenediff]") {
  TempDir tmp;
  const fs::path da = tmp.path / "a", db = tmp.path / "b";
  write_tree(da, base_files());
  TNode tb = base_tree();
  tb.children[1].pos[1] = 0.5f;
  write_tree(db, make_files(manifest_json(), scene_json(tb, kAssets)));
  const std::string A = da.string(), B = db.string();
  std::string out, err;

  CHECK(cli({A, A}, &out, &err) == 0);
  CHECK(out == "identical\n");
  CHECK(err.empty());

  CHECK(cli({A, B}, &out, &err) == 1);
  CHECK(out.find("~ node root/hills position: [0, -0.25, 0] -> [0, 0.5, 0]\n") != std::string::npos);
  CHECK(out.find("~ file scene.json sha256: ") != std::string::npos);
  CHECK(err.empty());

  CHECK(cli({A, B, "--format", "json"}, &out, &err) == 1);
  CHECK(out.rfind("{\n  \"a\": ", 0) == 0);
  CHECK(out.find("\"identical\": false") != std::string::npos);
  CHECK(out.find("\"summary\": {\"added\": 0, \"changed\": 2, \"removed\": 0}") != std::string::npos);
  CHECK(cli({A, A, "--format", "json"}, &out, &err) == 0);
  CHECK(out.find("\"findings\": [],") != std::string::npos);
  CHECK(out.find("\"identical\": true") != std::string::npos);

  CHECK(cli({A, B, "--quiet"}, &out, &err) == 1);
  CHECK(out.empty());
  CHECK(cli({"--quiet", A, A}, &out, &err) == 0);
  CHECK(out.empty());

  // Usage errors.
  for (const std::vector<std::string>& bad :
       std::vector<std::vector<std::string>>{{}, {A}, {A, B, A}, {A, B, "--nope"}, {A, B, "--format"},
                                              {A, B, "--format", "xml"}}) {
    CHECK(cli(bad, &out, &err) == 2);
    CHECK(out.empty());
    CHECK(err.rfind("usage: lumora-scenediff", 0) == 0);
  }
  // A load error still prints to stderr under --quiet and exits 2.
  CHECK(cli({A, (tmp.path / "missing").string(), "--quiet"}, &out, &err) == 2);
  CHECK(out.empty());
  CHECK_FALSE(err.empty());
}

TEST_CASE("cli output names only the paths it was given", "[scenediff]") {
  TempDir tmp;
  write_tree(tmp.path / "pkg", base_files());
  struct Cwd {
    fs::path old = fs::current_path();
    ~Cwd() { fs::current_path(old); }
  } restore;
  fs::current_path(tmp.path);
  const std::string abs = tmp.path.generic_string();
  std::string out, err;
  CHECK(cli({"pkg", "pkg", "--format", "json"}, &out, &err) == 0);
  CHECK(out.find("\"a\": \"pkg\"") != std::string::npos);
  CHECK(out.find(abs) == std::string::npos);
  CHECK(cli({"pkg", "no/such/dir"}, &out, &err) == 2);
  CHECK(err.find("B (no/such/dir): ") != std::string::npos);
  CHECK(err.find(abs) == std::string::npos);
  write_tree(tmp.path / "other", make_files(manifest_json("Other"), scene_json(base_tree(), kAssets)));
  CHECK(cli({"pkg", "other"}, &out, &err) == 1);
  CHECK(out.find(abs) == std::string::npos);
}

TEST_CASE("property: random field mutations are reported exactly", "[scenediff]") {
  std::mt19937 rng(20240607);  // fixed seed
  auto pick = [&](unsigned n) { return static_cast<unsigned>(rng() % n); };
  auto grid = [&]() { return static_cast<float>(static_cast<int>(pick(41)) - 20) * 0.125f; };

  for (int round = 0; round < 60; ++round) {
    // Random tree of 2 to 12 nodes, ids n0..nK, random own fields.
    TNode root;
    root.id = "n0";
    std::vector<TNode*> all{&root};
    std::vector<std::string> paths{"n0"};
    const unsigned count = 2 + pick(11);
    for (unsigned i = 1; i < count; ++i) {
      const unsigned parent = pick(static_cast<unsigned>(all.size()));
      TNode node;
      node.id = "n" + std::to_string(i);
      node.name = "name" + std::to_string(i);
      node.pos[0] = grid();
      node.pos[1] = grid();
      node.scale[0] = grid();
      node.rot = grid();
      if (pick(2)) {
        node.asset = pick(2) ? "tex" : "tex2";
        node.depth = static_cast<float>(pick(9)) * 0.125f;
      }
      all[parent]->children.push_back(node);
      // Child vectors may reallocate, so rebuild the pointer list and the path list.
      all.clear();
      paths.clear();
      std::vector<std::pair<TNode*, std::string>> stack{{&root, "n0"}};
      while (!stack.empty()) {
        auto [n, p] = stack.back();
        stack.pop_back();
        all.push_back(n);
        paths.push_back(p);
        for (auto& c : n->children) stack.push_back({&c, p + "/" + c.id});
      }
    }
    const Assets assets = {{"tex", "assets/tex.ppm"}, {"tex2", "assets/tex2.ppm"}};
    FileMap extra;
    extra["assets/tex2.ppm"] = bytes("P6\n1 1\n255\n\x01\x02\x03");
    const FileMap a = make_files(manifest_json(), scene_json(root, assets), extra);

    // Mutate each chosen node at most once, in one random field.
    TNode mutated = root;
    std::vector<TNode*> mall;
    std::vector<std::string> mpaths;
    {
      std::vector<std::pair<TNode*, std::string>> stack{{&mutated, "n0"}};
      while (!stack.empty()) {
        auto [n, p] = stack.back();
        stack.pop_back();
        mall.push_back(n);
        mpaths.push_back(p);
        for (auto& c : n->children) stack.push_back({&c, p + "/" + c.id});
      }
    }
    std::set<std::string> expected;
    for (std::size_t i = 0; i < mall.size(); ++i) {
      if (pick(3) != 0) continue;
      TNode& n = *mall[i];
      switch (pick(6)) {
        case 0: n.name += "!"; expected.insert("~ node " + mpaths[i] + " name"); break;
        case 1: n.pos[2] += 0.5f; expected.insert("~ node " + mpaths[i] + " position"); break;
        case 2: n.scale[1] += 0.5f; expected.insert("~ node " + mpaths[i] + " scale"); break;
        case 3: n.rot += 0.5f; expected.insert("~ node " + mpaths[i] + " rotation_z_degrees"); break;
        case 4:
          if (n.asset.empty()) break;
          n.asset = n.asset == "tex" ? "tex2" : "tex";
          expected.insert("~ node " + mpaths[i] + " layer_asset");
          break;
        default:
          if (n.asset.empty()) break;
          n.depth = n.depth >= 0.5f ? n.depth - 0.25f : n.depth + 0.25f;
          expected.insert("~ node " + mpaths[i] + " parallax_depth");
          break;
      }
    }
    // Sometimes add a fresh leaf under the root.
    if (pick(2)) {
      mutated.children.push_back(leaf("fresh", "Fresh", ""));
      expected.insert("+ node n0/fresh");
    }
    const FileMap b = make_files(manifest_json(), scene_json(mutated, assets), extra);

    const DiffResult r = diff_of(a, b);
    std::set<std::string> got;
    for (const auto& f : r.findings) {
      if (f.category == "node") {
        std::string s = std::string(1, f.change) + " node " + f.key;
        if (!f.field.empty()) s += " " + f.field;
        got.insert(s);
      } else {
        // The only other finding allowed is the changed scene.json file, and only when something changed.
        CHECK(f.category == "file");
        CHECK(f.key == "scene.json");
        CHECK(f.field == "sha256");
      }
    }
    INFO("round " << round);
    CHECK(got == expected);
    CHECK(r.identical() == expected.empty());
  }
}
