#include <catch2/catch_test_macros.hpp>

#include "lumora/rhi/rhi.h"

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

TEST_CASE("unimplemented backends report an error and no device") {
  std::string err;
  CHECK(create_device({Backend::Vulkan, true}, &err) == nullptr);
  CHECK_FALSE(err.empty());
  CHECK(backend_state(Backend::D3d12) != BackendState::Supported);
}

#if defined(LUMORA_HAS_VULKAN)
TEST_CASE("vulkan backend on a software ICD is validation clean") {
  std::string err;
  auto dev = create_device({Backend::Vulkan, true}, &err);
  REQUIRE_MESSAGE(dev, err);
  WARN("vulkan device: " << dev->capabilities().device_name);
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
