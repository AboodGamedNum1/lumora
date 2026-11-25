// Tests for the CPU reference renderer: hand-computed known-answer pixels, the aurora sample, determinism,
// and hostile PPM input. Expected values in comments are worked out by hand from the conventions in
// cpu_renderer.h (texel centres at half-integers, edge clamping inside the quad, round-to-nearest per layer).
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "cpu_renderer.h"
#include "pack_dir.h"
#include "png.h"

using namespace lumora::render2d;
using lumora::scene::Node;
using lumora::scene::Scene;

namespace {

Image solid(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
  Image i;
  i.width = i.height = 1;
  i.rgba = {r, g, b, a};
  return i;
}

// 2x2 test texture: top-left red, top-right green, bottom-left blue, bottom-right white.
Image quad_texture() {
  Image i;
  i.width = i.height = 2;
  i.rgba = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
  return i;
}

Node layer(const std::string& id, const std::string& asset, float depth = 0) {
  Node n;
  n.id = id;
  n.layer_asset = asset;
  n.parallax_depth = depth;
  return n;
}

Scene scene_of(std::vector<Node> children) {
  Scene s;
  s.root.id = "root";
  s.root.children = std::move(children);
  return s;
}

View view(std::uint32_t w, std::uint32_t h) {
  View v;
  v.width = w;
  v.height = h;
  return v;
}

struct Px {
  int r, g, b, a;
  bool operator==(const Px&) const = default;
};

Px at(const Image& img, std::uint32_t x, std::uint32_t y) {
  const auto* p = &img.rgba[(std::size_t{y} * img.width + x) * 4];
  return {p[0], p[1], p[2], p[3]};
}

std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

bool decode(const std::string& s, std::string* err = nullptr, std::uint32_t max_dim = 8192) {
  Image img;
  std::string e;
  const auto b = bytes(s);
  const bool ok = decode_ppm(b.data(), b.size(), img, e, max_dim);
  if (err) *err = e;
  if (!ok) CHECK_FALSE(img.valid());
  return ok;
}

}  // namespace

TEST_CASE("empty scene renders the clear colour", "[render2d]") {
  View v = view(4, 3);
  v.clear_rgba[0] = 10;
  v.clear_rgba[1] = 20;
  v.clear_rgba[2] = 30;
  v.clear_rgba[3] = 200;
  const Image img = render_scene(scene_of({}), {}, v);
  REQUIRE(img.valid());
  CHECK(img.width == 4);
  CHECK(img.height == 3);
  for (std::uint32_t y = 0; y < 3; ++y)
    for (std::uint32_t x = 0; x < 4; ++x) CHECK(at(img, x, y) == Px{10, 20, 30, 200});
}

TEST_CASE("identity layer copies texels exactly", "[render2d]") {
  const TextureMap tex{{"t", quad_texture()}};
  const Image img = render_scene(scene_of({layer("a", "t")}), tex, view(2, 2));
  CHECK(at(img, 0, 0) == Px{255, 0, 0, 255});
  CHECK(at(img, 1, 0) == Px{0, 255, 0, 255});
  CHECK(at(img, 0, 1) == Px{0, 0, 255, 255});
  CHECK(at(img, 1, 1) == Px{255, 255, 255, 255});
}

TEST_CASE("2x scale samples bilinearly between texel centres", "[render2d]") {
  Node n = layer("a", "t");
  n.transform.scale[0] = n.transform.scale[1] = 2;
  const TextureMap tex{{"t", quad_texture()}};
  const Image img = render_scene(scene_of({n}), tex, view(2, 2));
  // Pixel (0,0): texel coordinate (0.25, 0.25), weights TL .5625, TR .1875, BL .1875, BR .0625.
  // R = 255*(.5625+.0625) = 159.4, G = B = 255*(.1875+.0625) = 63.75.
  CHECK(at(img, 0, 0) == Px{159, 64, 64, 255});
  // Pixel (1,1): texel coordinate (0.75, 0.75): R = 159.4, G = B = 255*(.1875+.5625) = 191.25.
  CHECK(at(img, 1, 1) == Px{159, 191, 191, 255});
}

TEST_CASE("90 degree rotation is counter-clockwise", "[render2d]") {
  Node n = layer("a", "t");
  n.transform.rotation_z_degrees = 90;
  const TextureMap tex{{"t", quad_texture()}};
  const Image img = render_scene(scene_of({n}), tex, view(2, 2));
  // Rotating the picture counter-clockwise moves top-right to top-left, white to top-right,
  // red to bottom-left and blue to bottom-right.
  CHECK(at(img, 0, 0) == Px{0, 255, 0, 255});
  CHECK(at(img, 1, 0) == Px{255, 255, 255, 255});
  CHECK(at(img, 0, 1) == Px{255, 0, 0, 255});
  CHECK(at(img, 1, 1) == Px{0, 0, 255, 255});
}

TEST_CASE("translation moves the quad and clips at the view edge", "[render2d]") {
  Node n = layer("a", "t");
  n.transform.position[0] = 1;  // quad now covers x in [0, 2]
  const TextureMap tex{{"t", solid(200, 100, 50)}};
  const Image img = render_scene(scene_of({n}), tex, view(4, 4));
  for (std::uint32_t y = 0; y < 4; ++y) {
    CHECK(at(img, 0, y) == Px{0, 0, 0, 255});
    CHECK(at(img, 1, y) == Px{0, 0, 0, 255});
    CHECK(at(img, 2, y) == Px{200, 100, 50, 255});
    CHECK(at(img, 3, y) == Px{200, 100, 50, 255});
  }
  n.transform.position[0] = 5;  // fully outside the view
  const Image gone = render_scene(scene_of({n}), tex, view(4, 4));
  CHECK(at(gone, 3, 3) == Px{0, 0, 0, 255});
}

TEST_CASE("child transforms compose with the parent", "[render2d]") {
  Node parent;
  parent.id = "p";
  parent.transform.scale[0] = parent.transform.scale[1] = 0.5f;  // covers [-0.5, 0.5]
  parent.transform.position[0] = 0.5f;
  Node child = layer("c", "t");
  child.transform.position[0] = 1;  // child centre at 0.5 + 0.5*1 = 1.0, half-size 0.25 -> [0.75, 1.25]
  child.transform.scale[0] = child.transform.scale[1] = 0.5f;
  parent.children.push_back(child);
  const TextureMap tex{{"t", solid(9, 8, 7)}};
  // 8x8 view: pixel centres at +-0.125, 0.375, 0.625, 0.875. Child covers x in [0.75,1.0] => column 7 only
  // (centre 0.875); y in [-0.25,0.25] => rows 3 and 4 (centres +-0.125).
  const Image img = render_scene(scene_of({parent}), tex, view(8, 8));
  CHECK(at(img, 7, 3) == Px{9, 8, 7, 255});
  CHECK(at(img, 7, 4) == Px{9, 8, 7, 255});
  CHECK(at(img, 6, 3) == Px{0, 0, 0, 255});
  CHECK(at(img, 7, 2) == Px{0, 0, 0, 255});
  CHECK(at(img, 7, 5) == Px{0, 0, 0, 255});
}

TEST_CASE("parallax offset shifts by (1-depth)*0.1 in scene space", "[render2d]") {
  const TextureMap tex{{"t", solid(255, 255, 255)}};
  Node shrink;
  shrink.id = "s";
  shrink.transform.scale[0] = shrink.transform.scale[1] = 0.5f;  // layer covers [-0.5, 0.5]
  struct Case {
    float depth;
    std::uint32_t first, last;
  };
  // 100 px wide: centre of column i is (i-49.5)/50. Shift s = offset(1) * (1-d) * 0.1.
  // Inside when -0.5+s < x < 0.5+s, so first = floor(24.5+50s)+1 and last = ceil(74.5+50s)-1.
  // d=0: s=.1 -> columns 30..79. d=.4: s=.06 -> 28..77. d=1: s=0 -> 25..74.
  for (const Case c : {Case{0.0f, 30, 79}, Case{0.4f, 28, 77}, Case{1.0f, 25, 74}}) {
    Node n = shrink;
    n.layer_asset = "t";
    n.parallax_depth = c.depth;
    View v = view(100, 100);
    v.offset_x = 1;
    const Image img = render_scene(scene_of({n}), tex, v);
    INFO("depth " << c.depth);
    CHECK(at(img, c.first - 1, 50) == Px{0, 0, 0, 255});
    CHECK(at(img, c.first, 50) == Px{255, 255, 255, 255});
    CHECK(at(img, c.last, 50) == Px{255, 255, 255, 255});
    CHECK(at(img, c.last + 1, 50) == Px{0, 0, 0, 255});
  }
  // Vertical: positive offset_y moves the layer up. 100 px high, row j centre is 1-(j+.5)/50.
  // d=0, s=.1: top edge 0.6 -> first inside row 20 (centre 0.59), row 19 (0.61) outside.
  Node n = shrink;
  n.layer_asset = "t";
  View v = view(100, 100);
  v.offset_y = 1;
  const Image img = render_scene(scene_of({n}), tex, v);
  CHECK(at(img, 50, 19) == Px{0, 0, 0, 255});
  CHECK(at(img, 50, 20) == Px{255, 255, 255, 255});
}

TEST_CASE("layers blend source-over in draw order with straight alpha", "[render2d]") {
  const TextureMap tex{{"a", solid(200, 0, 0, 128)}, {"b", solid(0, 0, 200, 128)}};
  const Image img = render_scene(scene_of({layer("a", "a"), layer("b", "b")}), tex, view(2, 2));
  // sa = 128/255 = .50196. After A over opaque black: R = 200*.50196 = 100.4 -> 100.
  // After B: R = 100*(1-.50196) = 49.8 -> 50, B = 200*.50196 = 100.4 -> 100.
  CHECK(at(img, 0, 0) == Px{50, 0, 100, 255});
  // Reversed order gives a different result: B first (B=100), then A: R = 100.4 -> 100, B = 100*.498 = 49.8 -> 50.
  const Image rev = render_scene(scene_of({layer("b", "b"), layer("a", "a")}), tex, view(2, 2));
  CHECK(at(rev, 0, 0) == Px{100, 0, 50, 255});
  // Over a transparent clear colour the colour stays straight and alpha is the source alpha.
  View v = view(2, 2);
  v.clear_rgba[3] = 0;
  const TextureMap one{{"c", solid(200, 100, 50, 128)}};
  const Image t = render_scene(scene_of({layer("c", "c")}), one, v);
  CHECK(at(t, 1, 1) == Px{200, 100, 50, 128});
}

TEST_CASE("missing textures are skipped and bad view sizes are rejected", "[render2d]") {
  const Image img = render_scene(scene_of({layer("a", "nope")}), {}, view(2, 2));
  REQUIRE(img.valid());
  CHECK(at(img, 0, 0) == Px{0, 0, 0, 255});
  CHECK_FALSE(render_scene(scene_of({}), {}, view(0, 4)).valid());
  CHECK_FALSE(render_scene(scene_of({}), {}, view(4, 0)).valid());
  CHECK_FALSE(render_scene(scene_of({}), {}, view(8193, 1)).valid());
  CHECK_FALSE(render_scene(scene_of({}), {}, view(1, 8193)).valid());
  CHECK(render_scene(scene_of({}), {}, view(8192, 1)).valid());
  Node zero = layer("z", "t");
  zero.transform.scale[0] = 0;  // singular transform draws nothing
  const TextureMap tex{{"t", solid(1, 2, 3)}};
  CHECK(at(render_scene(scene_of({zero}), tex, view(2, 2)), 0, 0) == Px{0, 0, 0, 255});
}

TEST_CASE("aurora sample renders to known pixels at 128x128", "[render2d][aurora]") {
  const auto packed = lumora::scene::pack_directory(LUMORA_SAMPLES_DIR "/aurora");
  REQUIRE(packed.ok);
  const auto loaded = lumora::scene::load_package(packed.files);
  REQUIRE(loaded.ok());
  TextureMap tex;
  std::string err;
  REQUIRE(load_textures(loaded.scene, packed.files, tex, err));
  REQUIRE(tex.size() == 2);
  const Image img = render_scene(loaded.scene, tex, view(128, 128));
  REQUIRE(img.valid());
  // Sky (depth 1, top row 10,20,80, bottom row 40,60,140). Hills quad top edge is y=0.8, i.e. row 13 is the
  // first hills row. Rows 0..12 show sky clamped to its top row.
  CHECK(at(img, 64, 0) == Px{10, 20, 80, 255});
  CHECK(at(img, 64, 12) == Px{10, 20, 80, 255});
  // Row 13 centre y = 0.789, hills local y = 0.989 -> top hills row (0,90,40).
  CHECK(at(img, 64, 13) == Px{0, 90, 40, 255});
  // Row 76: hills texel y = 0.4953, G = 90*.5047+60*.4953 = 75.1, B = 40*.5047+30*.4953 = 35.05.
  CHECK(at(img, 64, 76) == Px{0, 75, 35, 255});
  // Bottom row: hills bottom row (0,60,30).
  CHECK(at(img, 64, 127) == Px{0, 60, 30, 255});
  // Tilting up by 1 moves hills (depth 0.4) up by 0.06: first hills row becomes 9, row 8 is sky.
  View v = view(128, 128);
  v.offset_y = 1;
  const Image tilted = render_scene(loaded.scene, tex, v);
  CHECK(at(tilted, 64, 8) == Px{10, 20, 80, 255});
  CHECK(at(tilted, 64, 9) == Px{0, 90, 40, 255});
}

TEST_CASE("rendering is deterministic", "[render2d]") {
  const auto packed = lumora::scene::pack_directory(LUMORA_SAMPLES_DIR "/aurora");
  REQUIRE(packed.ok);
  const auto loaded = lumora::scene::load_package(packed.files);
  REQUIRE(loaded.ok());
  TextureMap tex;
  std::string err;
  REQUIRE(load_textures(loaded.scene, packed.files, tex, err));
  View v = view(97, 61);
  v.offset_x = 0.37f;
  v.offset_y = -0.61f;
  CHECK(encode_ppm(render_scene(loaded.scene, tex, v)) == encode_ppm(render_scene(loaded.scene, tex, v)));
}

TEST_CASE("decode_ppm accepts valid P3 and P6", "[render2d][ppm]") {
  Image img;
  std::string err;
  const auto p3 = bytes("P3\n# comment\n2 1\n255\n1 2 3  4 5 6\n");
  REQUIRE(decode_ppm(p3.data(), p3.size(), img, err));
  CHECK(img.width == 2);
  CHECK(img.height == 1);
  CHECK(img.rgba == std::vector<std::uint8_t>{1, 2, 3, 255, 4, 5, 6, 255});
  auto p6 = bytes("P6\n1 2\n255\n");
  for (int b : {7, 8, 9, 10, 11, 12}) p6.push_back(static_cast<std::uint8_t>(b));
  REQUIRE(decode_ppm(p6.data(), p6.size(), img, err));
  CHECK(img.rgba == std::vector<std::uint8_t>{7, 8, 9, 255, 10, 11, 12, 255});
}

TEST_CASE("encode_ppm round trips and drops alpha", "[render2d][ppm]") {
  Image img = quad_texture();
  img.rgba[3] = 17;
  const auto enc = encode_ppm(img);
  CHECK(std::string(enc.begin(), enc.begin() + 11) == "P6\n2 2\n255\n");
  CHECK(enc.size() == 11 + 12);
  Image back;
  std::string err;
  REQUIRE(decode_ppm(enc.data(), enc.size(), back, err));
  CHECK(back.rgba[3] == 255);
  CHECK(back.rgba[0] == 255);
  CHECK(encode_ppm(Image{}).empty());
}

TEST_CASE("decode_ppm rejects hostile input", "[render2d][ppm][hostile]") {
  std::string err;
  CHECK_FALSE(decode(""));
  CHECK_FALSE(decode("P"));
  CHECK_FALSE(decode("P5\n1 1\n255\n\x01"));
  CHECK_FALSE(decode("P3\n1 1\n255\n1 2\n"));              // truncated P3
  CHECK_FALSE(decode(std::string("P6\n1 1\n255\n") + "ab"));  // truncated P6
  CHECK_FALSE(decode("P6\n1 1\n255"));                     // no separator or data
  CHECK_FALSE(decode("P3\n1 1\n255\n1 2 3 garbage"));      // trailing garbage
  CHECK_FALSE(decode(std::string("P6\n1 1\n255\n") + "abcd"));  // trailing byte
  CHECK_FALSE(decode("P3\n1 1\n65535\n1 2 3\n", &err));    // bad maxval
  CHECK_FALSE(decode("P3\n1 1\n15\n1 2 3\n"));
  CHECK_FALSE(decode("P3\n0 1\n255\n"));                   // zero dimension
  CHECK_FALSE(decode("P3\n1 0\n255\n"));
  CHECK_FALSE(decode("P3\n8193 1\n255\n"));                // over default max_dim
  CHECK_FALSE(decode("P3\n5 1\n255\n" + std::string(30, '1'), nullptr, 4));  // over custom max_dim
  CHECK_FALSE(decode("P6\n99999999999999999999 1\n255\n"));  // number too long
  CHECK_FALSE(decode("P6\n4294967295 4294967295\n255\n"));
  CHECK_FALSE(decode("P6\n8192 8192\n255\nxyz"));          // huge header, tiny body: no big allocation
  CHECK_FALSE(decode("P3\n8192 8192\n255\n1 2 3"));
  CHECK_FALSE(decode("P3\n1 1\n255\n1 2 256\n"));          // sample over maxval
  CHECK_FALSE(decode("P3\n1 1\n255\n1 2 -3\n"));
  CHECK_FALSE(decode("P3\n1 1\n255\n1 2 x\n"));
  CHECK_FALSE(decode("P3 # unterminated comment"));
  CHECK_FALSE(decode("P3\n1"));
  Image img;
  CHECK_FALSE(decode_ppm(nullptr, 5, img, err));
}

TEST_CASE("decode_ppm survives truncation at every offset and every bit flip", "[render2d][ppm][hostile]") {
  const std::vector<std::uint8_t> good = encode_ppm(quad_texture());
  Image img;
  std::string err;
  for (std::size_t cut = 0; cut < good.size(); ++cut) {
    CHECK_FALSE(decode_ppm(good.data(), cut, img, err));
  }
  for (std::size_t bit = 0; bit < good.size() * 8; ++bit) {
    std::vector<std::uint8_t> m = good;
    m[bit / 8] ^= static_cast<std::uint8_t>(1u << (bit % 8));
    (void)decode_ppm(m.data(), m.size(), img, err);  // any outcome is fine, it must not crash
  }
  SUCCEED();
}

TEST_CASE("load_textures reports missing and corrupt assets by name", "[render2d]") {
  Scene s;
  s.assets = {{"sky", "texture", "assets/sky.ppm"}, {"note", "other", "assets/x.bin"}};
  lumora::scene::FileMap files;
  TextureMap out;
  std::string err;
  CHECK_FALSE(load_textures(s, files, out, err));
  CHECK(err.find("sky") != std::string::npos);
  files["assets/sky.ppm"] = bytes("P3\n1 1\n255\n1 2\n");
  CHECK_FALSE(load_textures(s, files, out, err));
  CHECK(err.find("sky") != std::string::npos);
  CHECK(out.empty());
  files["assets/sky.ppm"] = bytes("P3\n1 1\n255\n1 2 3\n");
  REQUIRE(load_textures(s, files, out, err));  // non-texture kinds are ignored
  CHECK(out.size() == 1);
}

TEST_CASE("load_textures decodes PNG by signature and reports PNG errors by asset", "[render2d][png]") {
  lumora::image::Rgba8 src;
  src.width = 2;
  src.height = 1;
  src.rgba = {10, 20, 30, 255, 40, 50, 60, 128};
  const auto png = lumora::image::encode_png(src);
  REQUIRE_FALSE(png.empty());
  Scene s;
  s.assets = {{"tex", "texture", "assets/tex.png"}};
  lumora::scene::FileMap files;
  files["assets/tex.png"] = png;
  TextureMap out;
  std::string err;
  REQUIRE(load_textures(s, files, out, err));
  REQUIRE(out.count("tex") == 1);
  CHECK(out["tex"].width == 2);
  CHECK(out["tex"].height == 1);
  CHECK(out["tex"].rgba == src.rgba);
  // A damaged PNG is reported by asset name, and nothing is kept.
  auto bad = png;
  bad[bad.size() / 2] ^= 0xFF;
  files["assets/tex.png"] = bad;
  CHECK_FALSE(load_textures(s, files, out, err));
  CHECK(err.find("tex") != std::string::npos);
  CHECK(err.find("png") != std::string::npos);
  CHECK(out.empty());
  // A file name ending in .png whose content is a PPM still decodes as a PPM.
  files["assets/tex.png"] = bytes("P3\n1 1\n255\n1 2 3\n");
  CHECK(load_textures(s, files, out, err));
}
