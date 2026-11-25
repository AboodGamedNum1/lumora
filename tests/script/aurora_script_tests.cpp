// M3S slice: directory -> archive -> loader -> settings -> Lua script -> view offset -> CPU render -> golden image.
// The script scales the parallax input with the "parallax_strength" setting. Software evidence only (CPU reference
// renderer and the sandboxed Lua host on the CI hosts), not device evidence.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "../golden.h"
#include "cpu_renderer.h"
#include "pack_dir.h"
#include "script_host.h"
#include "settings.h"
#include "zip_store.h"

using namespace lumora;

namespace {

struct Slice {
  scene::FileMap files;
  scene::LoadResult loaded;
  scene::SettingsSchema schema;
  render2d::TextureMap textures;
};

Slice load_slice() {
  Slice s;
  const auto p = scene::pack_directory(LUMORA_SAMPLES_DIR "/aurora");
  REQUIRE(p.ok);
  const auto z = scene::write_zip(p.files, 6);
  const auto r = scene::read_zip_store(z.data(), z.size());
  REQUIRE(r.ok);
  s.files = r.files;
  s.loaded = scene::load_package(s.files);
  REQUIRE(s.loaded.ok());
  REQUIRE_FALSE(s.loaded.manifest.script_path.empty());
  const auto& sj = s.files.at(s.loaded.manifest.settings_path);
  const auto sp = scene::parse_settings_schema(std::string(sj.begin(), sj.end()));
  REQUIRE(sp.ok());
  s.schema = sp.schema;
  std::string err;
  REQUIRE(render2d::load_textures(s.loaded.scene, s.files, s.textures, err));
  return s;
}

// Renders one frame with the given parallax input and stored setting values.
render2d::Image render_with_script(const Slice& s, double in_x, double in_y, const scene::SettingsValues& stored) {
  const auto& sc = s.files.at(s.loaded.manifest.script_path);
  const std::string src(sc.begin(), sc.end());
  const auto values = scene::resolve_values(s.schema, stored);
  std::string err;
  auto host = script::ScriptHost::create(src, values, {}, err);
  REQUIRE(host);
  script::ParallaxOffset off;
  REQUIRE(host->on_parallax(in_x, in_y, off, err));
  render2d::View v;
  v.width = v.height = 128;
  v.offset_x = static_cast<float>(off.x);
  v.offset_y = static_cast<float>(off.y);
  return render2d::render_scene(s.loaded.scene, s.textures, v);
}

scene::SettingsValues strength(double k) {
  scene::SettingValue v;
  v.type = scene::SettingType::Float;
  v.f = k;
  return {{"parallax_strength", v}};
}

bool matches(const render2d::Image& img, const char* name) {
  REQUIRE(img.valid());
  const golden::Image actual = golden::from_rgba(img.rgba.data(), img.width, img.height);
  const golden::Result r = golden::check(name, actual, LUMORA_GOLDEN_CPU_DIR, LUMORA_GOLDEN_CPU_OUT_DIR, 2);
  INFO(name << ": " << r.message << ", max delta " << r.max_delta << ", bad pixels " << r.bad_pixels);
  return r.ok;
}

}  // namespace

TEST_CASE("aurora script: full strength equals the unscripted parallax goldens", "[script][golden][m3s]") {
  const Slice s = load_slice();
  // Default strength is 1: the script is the identity, so the existing baselines apply unchanged.
  CHECK(matches(render_with_script(s, 0, 0, {}), "aurora_rest"));
  CHECK(matches(render_with_script(s, 1, 0, {}), "aurora_parallax_x1"));
  CHECK(matches(render_with_script(s, 0, -1, {}), "aurora_parallax_y_neg1"));
}

TEST_CASE("aurora script: the strength setting changes the picture", "[script][golden][m3s]") {
  const Slice s = load_slice();
  CHECK(matches(render_with_script(s, 1, 0, strength(0.5)), "aurora_script_strength_half"));
  // Strength 0 switches the parallax off: any input renders like the rest pose.
  CHECK(matches(render_with_script(s, 1, 0, strength(0)), "aurora_rest"));
  CHECK(render_with_script(s, 1, 0, strength(0.5)).rgba != render_with_script(s, 1, 0, strength(1)).rgba);
}

TEST_CASE("aurora script: an out-of-range stored value falls back to the default", "[script][m3s]") {
  const Slice s = load_slice();
  CHECK(render_with_script(s, 1, 0, strength(7)).rgba == render_with_script(s, 1, 0, {}).rgba);
}
