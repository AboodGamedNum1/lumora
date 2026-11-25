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
