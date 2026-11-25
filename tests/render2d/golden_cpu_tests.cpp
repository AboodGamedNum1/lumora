// Golden-image tests for the CPU reference renderer (core/render2d). Every scene is built in code from small
// in-memory textures, rendered, and compared with a binary P6 baseline under tests/golden/cpu-reference/ using
// tests/golden.h unchanged. Scenes only use features documented in cpu_renderer.h and docs/render2d.md.
// The evidence is CPU software rendering only. See docs/golden-cpu.md for the scene list and the tolerance policy.
// Set LUMORA_BLESS_GOLDEN=1 to rewrite the baselines, then look at every image before committing.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../golden.h"
#include "cpu_renderer.h"

namespace fs = std::filesystem;
using namespace lumora::render2d;
using lumora::scene::Node;
using lumora::scene::Scene;

namespace {

// Per-channel tolerance for every scene. Raising it for a scene needs a one-line reason in docs/golden-cpu.md.
constexpr unsigned kTolerance = 2;

Image solid(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
  Image i;
  i.width = i.height = 1;
  i.rgba = {r, g, b, a};
  return i;
}

// 2x2: top-left red, top-right green, bottom-left blue, bottom-right white.
Image quad_texture() {
  Image i;
  i.width = i.height = 2;
  i.rgba = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
  return i;
}

// 4x4 grid: red rises to the right (30..210), green rises downwards (30..210), blue is 128 everywhere.
Image grid_texture() {
  Image i;
  i.width = i.height = 4;
  for (std::uint32_t y = 0; y < 4; ++y)
    for (std::uint32_t x = 0; x < 4; ++x)
      for (std::uint8_t v : {static_cast<std::uint8_t>(30 + 60 * x), static_cast<std::uint8_t>(30 + 60 * y),
                             std::uint8_t{128}, std::uint8_t{255}})
        i.rgba.push_back(v);
  return i;
}

// 4x1 yellow strip with alpha 0, 85, 170, 255 from left to right.
Image alpha_ramp_texture() {
  Image i;
  i.width = 4;
  i.height = 1;
  i.rgba = {255, 220, 0, 0, 255, 220, 0, 85, 255, 220, 0, 170, 255, 220, 0, 255};
  return i;
}

Node layer(const std::string& id, const std::string& asset, float depth = 0) {
  Node n;
  n.id = id;
  n.layer_asset = asset;
  n.parallax_depth = depth;
  return n;
}

Node at(Node n, float x, float y, float sx = 1, float sy = 1, float rot = 0) {
  n.transform.position[0] = x;
  n.transform.position[1] = y;
  n.transform.scale[0] = sx;
  n.transform.scale[1] = sy;
  n.transform.rotation_z_degrees = rot;
  return n;
}

Scene scene_of(std::vector<Node> children) {
  Scene s;
  s.root.id = "root";
  s.root.children = std::move(children);
  return s;
}

View make_view(std::uint32_t w, std::uint32_t h, float ox = 0, float oy = 0,
               std::initializer_list<std::uint8_t> clear = {0, 0, 0, 255}) {
  View v;
  v.width = w;
  v.height = h;
  v.offset_x = ox;
  v.offset_y = oy;
  int k = 0;
  for (std::uint8_t c : clear) v.clear_rgba[k++] = c;
  return v;
}

struct Case {
  std::string name;
  Scene scene;
  TextureMap textures;
  View view;
};

// Three bars at different depths, used by the parallax scenes. Depth 0 moves most, depth 1 not at all.
Case parallax_case(const std::string& name, float ox, float oy) {
  Case c{name, scene_of({at(layer("sky", "blue", 1.0f), 0, 0, 1, 1),
                         at(layer("mid", "green", 0.5f), -0.3f, 0, 0.25f, 0.6f),
                         at(layer("near", "red", 0.0f), 0.3f, 0, 0.25f, 0.6f)}),
         {}, make_view(96, 64, ox, oy, {10, 10, 10, 255})};
  c.textures["blue"] = solid(40, 70, 160);
  c.textures["green"] = solid(60, 180, 80);
  c.textures["red"] = solid(220, 50, 50);
  return c;
}

std::vector<Case> all_cases() {
  std::vector<Case> cs;
  auto add = [&](std::string name, std::vector<Node> nodes, TextureMap tex, View v) {
    cs.push_back(Case{std::move(name), scene_of(std::move(nodes)), std::move(tex), v});
  };
  const TextureMap grid{{"grid", grid_texture()}};

  add("identity_grid", {layer("n", "grid")}, grid, make_view(64, 64));
  add("scale_down_half", {at(layer("n", "grid"), 0, 0, 0.5f, 0.5f)}, grid, make_view(64, 64, 0, 0, {40, 40, 60, 255}));
  add("scale_up_2x", {at(layer("n", "grid"), 0, 0, 2, 2)}, grid, make_view(64, 64));
  add("scale_nonuniform", {at(layer("n", "grid"), 0, 0, 0.75f, 0.4f)}, grid, make_view(64, 64, 0, 0, {40, 40, 60, 255}));
  add("rotate_45", {at(layer("n", "grid"), 0, 0, 0.6f, 0.6f, 45)}, grid, make_view(64, 64, 0, 0, {40, 40, 60, 255}));
  add("rotate_90_wide", {at(layer("n", "q"), 0, 0, 0.8f, 0.8f, 90)}, {{"q", quad_texture()}},
      make_view(96, 48, 0, 0, {40, 40, 60, 255}));
  add("translate_clip", {at(layer("n", "grid"), 0.5f, -0.4f, 0.8f, 0.8f)}, grid,
      make_view(64, 64, 0, 0, {90, 20, 20, 255}));

  add("layer_order",
      {at(layer("a", "red"), -0.2f, 0.2f, 0.5f, 0.5f), at(layer("b", "green"), 0, 0, 0.5f, 0.5f),
       at(layer("c", "blue"), 0.2f, -0.2f, 0.5f, 0.5f)},
      {{"red", solid(220, 40, 40)}, {"green", solid(40, 200, 60)}, {"blue", solid(50, 70, 230)}},
      make_view(64, 64, 0, 0, {235, 235, 235, 255}));

  {  // Pre-order: A, then A's child C, then B. B is drawn last and covers C.
    Node a = at(layer("a", "red"), -0.25f, 0.25f, 0.45f, 0.45f);
    a.children.push_back(at(layer("c", "green"), 0.9f, -0.9f, 0.7f, 0.7f));
    Node b = at(layer("b", "blue"), 0.25f, 0.0f, 0.25f, 0.25f);
    add("preorder_nested", {a, b},
        {{"red", solid(220, 40, 40)}, {"green", solid(40, 200, 60)}, {"blue", solid(50, 70, 230)}},
        make_view(64, 64, 0, 0, {235, 235, 235, 255}));
  }
  {  // The child's transform is relative to its parent: scaled, rotated and moved with it.
    Node parent = at(layer("p", "grid"), -0.3f, 0.3f, 0.5f, 0.5f, 20);
    parent.children.push_back(at(layer("k", "q"), 1.2f, 0, 0.4f, 0.4f));
    add("parent_child_transform", {parent}, {{"grid", grid_texture()}, {"q", quad_texture()}},
        make_view(64, 64, 0, 0, {40, 40, 60, 255}));
  }

  add("alpha_ramp", {layer("bg", "grid"), at(layer("fg", "ramp"), 0, 0, 0.7f, 0.5f)},
      {{"grid", grid_texture()}, {"ramp", alpha_ramp_texture()}}, make_view(64, 64));
  add("alpha_stack",
      {at(layer("a", "blue"), 0.2f, -0.2f, 0.6f, 0.6f), at(layer("b", "red"), -0.2f, -0.2f, 0.6f, 0.6f),
       at(layer("c", "green"), 0, 0.2f, 0.6f, 0.6f)},
      {{"red", solid(255, 0, 0, 128)}, {"green", solid(0, 255, 0, 128)}, {"blue", solid(0, 0, 255, 128)}},
      make_view(64, 64, 0, 0, {255, 255, 255, 255}));

  cs.push_back(parallax_case("parallax_x_pos1", 1, 0));
  cs.push_back(parallax_case("parallax_x_neg1", -1, 0));
  cs.push_back(parallax_case("parallax_y_half", 0, 0.5f));
  cs.push_back(parallax_case("parallax_zero", 0, 0));

  add("view_tall", {layer("n", "q")}, {{"q", quad_texture()}}, make_view(48, 128));
  add("view_1x1", {layer("n", "grid")}, grid, make_view(1, 1));
  add("view_large", {at(layer("n", "grid"), 0.1f, -0.1f, 0.9f, 0.9f, 15)}, grid,
      make_view(128, 160, 0, 0, {40, 40, 60, 255}));
  return cs;
}

golden::Image render_golden(const Case& c) {
  const Image img = render_scene(c.scene, c.textures, c.view);
  REQUIRE(img.valid());
  return golden::from_rgba(img.rgba.data(), img.width, img.height);
}

bool bless_mode() {
  const char* b = std::getenv("LUMORA_BLESS_GOLDEN");
  return b && *b == '1';
}

// Fresh, unique folder in the system temp directory for the negative tests. Removed when it goes out of scope.
struct TempDir {
  fs::path path;
  TempDir() {
    static std::atomic<unsigned> counter{0};
    path = fs::temp_directory_path() / ("lumora_golden_cpu_" + std::to_string(counter++) + "_" +
                                        std::to_string(reinterpret_cast<std::uintptr_t>(this)));
    fs::remove_all(path);
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

}  // namespace

TEST_CASE("cpu renderer matches its golden baselines", "[render2d][golden]") {
  const auto cases = all_cases();
  REQUIRE(cases.size() >= 12);
  REQUIRE(cases.size() <= 20);
  for (const auto& c : cases) {
    CAPTURE(c.name);
    const golden::Image actual = render_golden(c);
    const golden::Result r = golden::check(c.name, actual, LUMORA_GOLDEN_CPU_DIR, LUMORA_GOLDEN_CPU_OUT_DIR, kTolerance);
    INFO(r.message << ", max delta " << r.max_delta << ", bad pixels " << r.bad_pixels);
    CHECK(r.ok);
  }
}

TEST_CASE("cpu renderer output is byte-identical across repeated renders", "[render2d][golden]") {
  for (const auto& c : all_cases()) {
    CAPTURE(c.name);
    const Image a = render_scene(c.scene, c.textures, c.view);
    const Image b = render_scene(c.scene, c.textures, c.view);
    REQUIRE(a.valid());
    CHECK(a.rgba == b.rgba);
    CHECK(encode_ppm(a) == encode_ppm(b));
  }
}

TEST_CASE("golden check fails on a missing baseline and writes the actual image", "[render2d][golden]") {
  if (bless_mode()) SKIP("bless mode writes baselines instead of failing");
  const auto cases = all_cases();
  const golden::Image actual = render_golden(cases.front());
  TempDir base, out;
  const golden::Result r = golden::check("nothing_here", actual, base.path.string(), out.path.string(), kTolerance);
  CHECK_FALSE(r.ok);
  CHECK(r.message.find("baseline missing") != std::string::npos);
  CHECK(fs::exists(out.path / "nothing_here.actual.ppm"));
  CHECK_FALSE(fs::exists(base.path / "nothing_here.ppm"));
}

TEST_CASE("golden check fails on one pixel beyond tolerance and passes within it", "[render2d][golden]") {
  if (bless_mode()) SKIP("bless mode writes baselines instead of comparing");
  const auto cases = all_cases();
  const golden::Image actual = render_golden(cases.front());
  TempDir base, out;
  REQUIRE(golden::write_ppm(base.path / "scene.ppm", actual));

  // Find a channel that can move up by tolerance + 1 without wrapping.
  std::size_t idx = 0;
  while (idx < actual.rgb.size() && actual.rgb[idx] > 250) ++idx;
  REQUIRE(idx < actual.rgb.size());

  golden::Image changed = actual;
  SECTION("unchanged image matches") {
    const auto r = golden::check("scene", changed, base.path.string(), out.path.string(), kTolerance);
    CHECK(r.ok);
    CHECK(r.max_delta == 0);
  }
  SECTION("change equal to the tolerance passes") {
    changed.rgb[idx] = static_cast<std::uint8_t>(changed.rgb[idx] + kTolerance);
    const auto r = golden::check("scene", changed, base.path.string(), out.path.string(), kTolerance);
    CHECK(r.ok);
    CHECK(r.max_delta == kTolerance);
    CHECK_FALSE(fs::exists(out.path / "scene.diff.ppm"));
  }
  SECTION("change of tolerance plus one fails and writes actual and diff") {
    changed.rgb[idx] = static_cast<std::uint8_t>(changed.rgb[idx] + kTolerance + 1);
    const auto r = golden::check("scene", changed, base.path.string(), out.path.string(), kTolerance);
    CHECK_FALSE(r.ok);
    CHECK(r.bad_pixels == 1);
    CHECK(r.max_delta == kTolerance + 1);
    CHECK(fs::exists(out.path / "scene.actual.ppm"));
    CHECK(fs::exists(out.path / "scene.diff.ppm"));
  }
  SECTION("a different size fails") {
    golden::Image smaller{1, 1, {actual.rgb[0], actual.rgb[1], actual.rgb[2]}};
    const auto r = golden::check("scene", smaller, base.path.string(), out.path.string(), kTolerance);
    CHECK_FALSE(r.ok);
  }
}
