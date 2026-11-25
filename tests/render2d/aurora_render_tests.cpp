// M3S groundwork: the packed sample scene goes through the loader and the CPU reference renderer.
// Software evidence only (CPU reference, no GPU, no device).
#include <catch2/catch_test_macros.hpp>

#include "../golden.h"
#include "cpu_renderer.h"
#include "pack_dir.h"
#include "zip_store.h"

using namespace lumora;

namespace {
render2d::Image render_aurora(float offset_x, float offset_y) {
  const auto p = scene::pack_directory(LUMORA_SAMPLES_DIR "/aurora");
  REQUIRE(p.ok);
  // Through the archive, as a device would load it.
  const auto z = scene::write_zip(p.files, 6);
  const auto r = scene::read_zip_store(z.data(), z.size());
  REQUIRE(r.ok);
  const auto l = scene::load_package(r.files);
  REQUIRE(l.ok());
  render2d::TextureMap tex;
  std::string err;
  REQUIRE(render2d::load_textures(l.scene, r.files, tex, err));
  render2d::View v;
  v.width = v.height = 128;
  v.offset_x = offset_x;
  v.offset_y = offset_y;
  return render2d::render_scene(l.scene, tex, v);
}
const std::uint8_t* px(const render2d::Image& i, unsigned x, unsigned y) { return &i.rgba[(std::size_t{y} * i.width + x) * 4]; }
}  // namespace

TEST_CASE("aurora sample renders: sky on top, hills below, deterministic", "[render2d][m3s]") {
  const auto a = render_aurora(0, 0);
  REQUIRE(a.valid());
  // Hills sit 0.2 lower: the top rows show only the sky (blue dominant), the bottom rows are green hills.
  CHECK(px(a, 64, 2)[2] > px(a, 64, 2)[1]);
  CHECK(px(a, 64, 2)[1] > px(a, 64, 2)[0]);
  CHECK(px(a, 64, 125)[0] == 0);
  CHECK(px(a, 64, 125)[1] > px(a, 64, 125)[2]);
  CHECK(px(a, 64, 125)[3] == 255);
  CHECK(render_aurora(0, 0).rgba == a.rgba);
}

TEST_CASE("aurora sample parallax: far sky stays, near hills move", "[render2d][m3s]") {
  const auto a = render_aurora(0, 0);
  const auto b = render_aurora(1, 0);
  REQUIRE(b.valid());
  CHECK(a.rgba != b.rgba);
  // The sky has parallax depth 1, so rows that only show sky stay identical.
  for (unsigned x = 0; x < 128; ++x) CHECK(px(a, x, 1)[0] == px(b, x, 1)[0]);
}

// Full-image goldens for the packed sample, loaded the way the product loads it (directory -> archive -> loader).
// Same harness and policy as the synthetic scenes (docs/golden-cpu.md): per-channel tolerance 2, bless with
// LUMORA_BLESS_GOLDEN=1 and look at the image before committing. CPU software rendering evidence only.
TEST_CASE("aurora sample matches its golden images", "[render2d][golden][m3s]") {
  struct Shot {
    const char* name;
    float ox, oy;
  };
  for (const Shot& s : {Shot{"aurora_rest", 0, 0}, Shot{"aurora_parallax_x1", 1, 0}, Shot{"aurora_parallax_y_neg1", 0, -1}}) {
    CAPTURE(s.name);
    const auto img = render_aurora(s.ox, s.oy);
    REQUIRE(img.valid());
    const golden::Image actual = golden::from_rgba(img.rgba.data(), img.width, img.height);
    const golden::Result r = golden::check(s.name, actual, LUMORA_GOLDEN_CPU_DIR, LUMORA_GOLDEN_CPU_OUT_DIR, 2);
    INFO(r.message << ", max delta " << r.max_delta << ", bad pixels " << r.bad_pixels);
    CHECK(r.ok);
  }
}
