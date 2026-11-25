#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "lumora/rhi/rhi.h"

#if defined(LUMORA_HAS_VULKAN) && defined(LUMORA_HAS_TEST_SHADERS)
#include "triangle_frag_spv.h"
#include "triangle_vert_spv.h"
#define LUMORA_VULKAN_RENDER_TEST 1
#endif
#if defined(LUMORA_HAS_D3D12) && defined(LUMORA_HAS_TEST_DXIL)
#include "triangle_ps_dxil.h"
#include "triangle_vs_dxil.h"
#define LUMORA_D3D12_RENDER_TEST 1
#endif

using namespace lumora::rhi;

TEST_CASE("null device validates descriptions and tracks lifetime") {
  auto dev = create_device({Backend::Null, true});
  REQUIRE(dev);
  CHECK_FALSE(dev->create_buffer({"", 16, BufferUsage::Vertex}).valid());
  CHECK_FALSE(dev->create_buffer({"b", 0, BufferUsage::Vertex}).valid());
  CHECK_FALSE(dev->create_texture({"t", 0, 4, 1, Format::Rgba8Unorm}).valid());
  CHECK_FALSE(dev->create_texture({"t", 8192, 8192, 1, Format::Rgba8Unorm}).valid());
  Handle b = dev->create_buffer({"vb", 64, BufferUsage::Vertex});
  Handle t = dev->create_texture({"tex", 16, 16, 1, Format::Rgba8Unorm});
  REQUIRE(b.valid());
  REQUIRE(t.valid());
  CHECK(dev->live_resource_count() == 2);
  dev->destroy(b);
  dev->destroy(b);  // double destroy is ignored
  CHECK(dev->live_resource_count() == 1);
  Handle b2 = dev->create_buffer({"vb2", 64, BufferUsage::Index});
  CHECK(b2.index == b.index);
  CHECK(b2.generation != b.generation);  // stale handle cannot alias the new resource
  dev->destroy(b);                       // stale handle, ignored
  CHECK(dev->live_resource_count() == 2);
  dev->destroy(b2);
  dev->destroy(t);
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("unknown backends report an error and no device") {
  std::string err;
  CHECK(create_device({static_cast<Backend>(99), true}, &err) == nullptr);
  CHECK_FALSE(err.empty());
  CHECK(backend_state(Backend::D3d12) != BackendState::Supported);
}

#if defined(LUMORA_HAS_VULKAN)
TEST_CASE("vulkan backend on a software ICD is validation clean") {
  std::string err;
  auto dev = create_device({Backend::Vulkan, true}, &err);
  INFO(err);
  REQUIRE(dev);
  WARN("vulkan device: " << dev->capabilities().device_name);
  // The Khronos validation layer must really be loaded, except where a spike machine has none.
  if (!std::getenv("LUMORA_ALLOW_NO_VALIDATION")) CHECK(dev->validation_active());
  CHECK(dev->capabilities().max_texture_size >= 4096);
  CHECK_FALSE(dev->create_buffer({"", 16, BufferUsage::Vertex}).valid());
  Handle b = dev->create_buffer({"vb", 256, BufferUsage::Vertex});
  Handle u = dev->create_buffer({"ub", 64, BufferUsage::Uniform});
  Handle t = dev->create_texture({"tex", 64, 64, 3, Format::Rgba8Unorm});
  Handle d = dev->create_texture({"depth", 64, 64, 1, Format::Depth32Float});
  REQUIRE(b.valid());
  REQUIRE(u.valid());
  REQUIRE(t.valid());
  REQUIRE(d.valid());
  CHECK(dev->live_resource_count() == 4);
  dev->destroy(b);
  dev->destroy(b);
  dev->destroy(u);
  dev->destroy(t);
  dev->destroy(d);
  CHECK(dev->live_resource_count() == 0);
  CHECK(dev->validation_message_count() == 0);
}
#endif

#if defined(LUMORA_HAS_GLES)
#define LUMORA_GLES_RENDER_TEST 1
#endif

#if defined(LUMORA_VULKAN_RENDER_TEST) || defined(LUMORA_D3D12_RENDER_TEST) || defined(LUMORA_GLES_RENDER_TEST)
// Renders a blue clear plus a red triangle on `backend`, reads the pixels back, checks them and
// compares with the golden baseline of the given renderer.
static void run_triangle_test(Backend backend, const PipelineDesc& shaders, const char* renderer) {
  std::string err;
  auto dev = create_device({backend, true}, &err);
  INFO(err);
  REQUIRE(dev);
  WARN("device: " << dev->capabilities().device_name);
  constexpr std::uint32_t kSize = 64;
  Handle target = dev->create_texture({"target", kSize, kSize, 1, Format::Rgba8Unorm});
  Handle readback = dev->create_buffer({"readback", kSize * kSize * 4, BufferUsage::Readback});
  REQUIRE(target.valid());
  REQUIRE(readback.valid());
  PipelineDesc pd = shaders;
  pd.name = "triangle";
  Handle pipe = dev->create_graphics_pipeline(pd);
  INFO(dev->last_error());
  REQUIRE(pipe.valid());

  const float clear[4] = {0.0f, 0.0f, 1.0f, 1.0f};  // blue
  auto cb = dev->begin_commands();
  REQUIRE(cb);
  cb->begin_render_pass(target, clear);
  cb->bind_pipeline(pipe);
  cb->draw(3);
  cb->end_render_pass();
  cb->copy_texture_to_buffer(target, readback);
  INFO(dev->last_error());
  REQUIRE(dev->submit(*cb));
  cb.reset();

  const auto* px = static_cast<const unsigned char*>(dev->map(readback));
  REQUIRE(px != nullptr);
  auto at = [&](unsigned x, unsigned y) { return px + (y * kSize + x) * 4; };
  // Center is inside the triangle: red. Corners are outside: the blue clear color.
  CHECK(at(32, 32)[0] == 255);
  CHECK(at(32, 32)[1] == 0);
  CHECK(at(32, 32)[2] == 0);
  CHECK(at(1, 1)[0] == 0);
  CHECK(at(1, 1)[2] == 255);
  CHECK(at(kSize - 2, 1)[2] == 255);
  const golden::Result g = golden::check(
      std::string("triangle_") + renderer, golden::from_rgba(px, kSize, kSize),
      std::string(LUMORA_GOLDEN_DIR "/") + renderer, LUMORA_GOLDEN_OUT_DIR, 2);
  INFO(g.message << ", max delta " << g.max_delta << ", bad pixels " << g.bad_pixels);
  CHECK(g.ok);
  dev->unmap(readback);
  dev->destroy(pipe);
  dev->destroy(readback);
  dev->destroy(target);
  CHECK(dev->live_resource_count() == 0);
  CHECK(dev->validation_message_count() == 0);
}
#endif

#if defined(LUMORA_VULKAN_RENDER_TEST)
TEST_CASE("vulkan renders a clear and a triangle and reads the pixels back") {
  PipelineDesc pd;
  pd.vertex_spirv = {triangle_vert_spv, sizeof(triangle_vert_spv) / 4};
  pd.fragment_spirv = {triangle_frag_spv, sizeof(triangle_frag_spv) / 4};
  run_triangle_test(Backend::Vulkan, pd, "lavapipe");
}
#endif

#if defined(LUMORA_D3D12_RENDER_TEST)
TEST_CASE("d3d12 on WARP renders a clear and a triangle and reads the pixels back") {
  PipelineDesc pd;
  pd.vertex_dxil = std::as_bytes(std::span(triangle_vs_dxil));
  pd.fragment_dxil = std::as_bytes(std::span(triangle_ps_dxil));
  run_triangle_test(Backend::D3d12, pd, "warp");
}
#endif

#if defined(LUMORA_GLES_RENDER_TEST)
TEST_CASE("gles on a software context renders a clear and a triangle and reads the pixels back") {
  PipelineDesc pd;
  pd.vertex_glsl =
      "#version 300 es\n"
      "void main() {\n"
      "  vec2 p[3] = vec2[](vec2(0.0, 0.8), vec2(0.8, -0.8), vec2(-0.8, -0.8));\n"
      "  gl_Position = vec4(p[gl_VertexID], 0.0, 1.0);\n"
      "}\n";
  pd.fragment_glsl =
      "#version 300 es\n"
      "precision mediump float;\n"
      "out vec4 out_color;\n"
      "void main() { out_color = vec4(1.0, 0.0, 0.0, 1.0); }\n";
  run_triangle_test(Backend::Gles, pd, "llvmpipe-gles");
}
#endif
