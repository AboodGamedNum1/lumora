// Null-backend RHI unit tests. They use only the public RHI header and Catch2, need no GPU and
// no files, and run the same on every platform. One behavior per test case.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "lumora/rhi/rhi.h"

using namespace lumora::rhi;

namespace {

std::unique_ptr<Device> make_null_device() {
  auto dev = create_device({Backend::Null, true});
  REQUIRE(dev);
  return dev;
}

}  // namespace

TEST_CASE("null buffer with an empty name is rejected") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->create_buffer({"", 16, BufferUsage::Vertex}).valid());
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("null buffer with size zero is rejected") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->create_buffer({"empty", 0, BufferUsage::Uniform}).valid());
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("null texture with an empty name is rejected") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->create_texture({"", 4, 4, 1, Format::Rgba8Unorm}).valid());
}

TEST_CASE("null texture with a zero width or height is rejected") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->create_texture({"w0", 0, 4, 1, Format::Rgba8Unorm}).valid());
  CHECK_FALSE(dev->create_texture({"h0", 4, 0, 1, Format::Rgba8Unorm}).valid());
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("null texture larger than max_texture_size is rejected") {
  auto dev = make_null_device();
  const std::uint32_t too_big = dev->capabilities().max_texture_size + 1;
  CHECK_FALSE(dev->create_texture({"wide", too_big, 1, 1, Format::Rgba8Unorm}).valid());
  CHECK_FALSE(dev->create_texture({"tall", 1, too_big, 1, Format::Rgba8Unorm}).valid());
}

TEST_CASE("null texture at exactly max_texture_size is accepted") {
  auto dev = make_null_device();
  const std::uint32_t limit = dev->capabilities().max_texture_size;
  CHECK(dev->create_texture({"max", limit, limit, 1, Format::Rgba8Unorm}).valid());
}

TEST_CASE("null texture with zero mip levels is rejected") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->create_texture({"mips0", 4, 4, 0, Format::Rgba8Unorm}).valid());
}

TEST_CASE("null device rejects a graphics pipeline") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->create_graphics_pipeline({}).valid());
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("null device cannot record or submit commands") {
  auto dev = make_null_device();
  CHECK(dev->begin_commands() == nullptr);
  Handle buf = dev->create_buffer({"mapped", 16, BufferUsage::Readback});
  REQUIRE(buf.valid());
  CHECK(dev->map(buf) == nullptr);
}

TEST_CASE("null live_resource_count starts at zero") {
  auto dev = make_null_device();
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("null live_resource_count rises with each created resource") {
  auto dev = make_null_device();
  REQUIRE(dev->create_buffer({"b", 8, BufferUsage::Vertex}).valid());
  CHECK(dev->live_resource_count() == 1);
  REQUIRE(dev->create_texture({"t", 2, 2, 1, Format::R8Unorm}).valid());
  CHECK(dev->live_resource_count() == 2);
}

TEST_CASE("null live_resource_count falls with each destroyed resource") {
  auto dev = make_null_device();
  Handle b = dev->create_buffer({"b", 8, BufferUsage::Index});
  Handle t = dev->create_texture({"t", 2, 2, 1, Format::Bgra8Unorm});
  dev->destroy(b);
  CHECK(dev->live_resource_count() == 1);
  dev->destroy(t);
  CHECK(dev->live_resource_count() == 0);
}

TEST_CASE("null failed creation does not change live_resource_count") {
  auto dev = make_null_device();
  REQUIRE(dev->create_buffer({"ok", 8, BufferUsage::Storage}).valid());
  CHECK_FALSE(dev->create_buffer({"", 8, BufferUsage::Storage}).valid());
  CHECK_FALSE(dev->create_texture({"bad", 0, 0, 1, Format::Rgba8Unorm}).valid());
  CHECK(dev->live_resource_count() == 1);
}

TEST_CASE("null destroy of a handle twice counts once") {
  auto dev = make_null_device();
  Handle b = dev->create_buffer({"b", 8, BufferUsage::Vertex});
  Handle keep = dev->create_buffer({"keep", 8, BufferUsage::Vertex});
  REQUIRE(keep.valid());
  dev->destroy(b);
  dev->destroy(b);
  CHECK(dev->live_resource_count() == 1);
}

TEST_CASE("null destroy of the invalid handle is ignored") {
  auto dev = make_null_device();
  REQUIRE(dev->create_buffer({"b", 8, BufferUsage::Vertex}).valid());
  dev->destroy(Handle{});
  CHECK(dev->live_resource_count() == 1);
}

TEST_CASE("null destroy of an out-of-range handle is ignored") {
  auto dev = make_null_device();
  REQUIRE(dev->create_buffer({"b", 8, BufferUsage::Vertex}).valid());
  dev->destroy(Handle{9999, 1});
  CHECK(dev->live_resource_count() == 1);
}

TEST_CASE("null stale handle is not destroyed again after its slot is reused") {
  auto dev = make_null_device();
  Handle old_handle = dev->create_buffer({"old", 8, BufferUsage::Vertex});
  dev->destroy(old_handle);
  Handle fresh = dev->create_buffer({"fresh", 8, BufferUsage::Vertex});
  REQUIRE(fresh.valid());
  dev->destroy(old_handle);  // stale: must not touch the new resource
  CHECK(dev->live_resource_count() == 1);
}

TEST_CASE("null reused slot gets a new generation") {
  auto dev = make_null_device();
  Handle first = dev->create_buffer({"first", 8, BufferUsage::Vertex});
  dev->destroy(first);
  Handle second = dev->create_buffer({"second", 8, BufferUsage::Vertex});
  CHECK(second.index == first.index);
  CHECK(second.generation != first.generation);
  CHECK_FALSE(second == first);
}

TEST_CASE("null created handles are valid and distinct") {
  auto dev = make_null_device();
  Handle a = dev->create_buffer({"a", 8, BufferUsage::Vertex});
  Handle b = dev->create_buffer({"b", 8, BufferUsage::Vertex});
  CHECK(a.valid());
  CHECK(b.valid());
  CHECK_FALSE(a == b);
}

TEST_CASE("null last_error is empty on a new device") {
  auto dev = make_null_device();
  CHECK(dev->last_error().empty());
}

TEST_CASE("null last_error names a nameless buffer") {
  auto dev = make_null_device();
  dev->create_buffer({"", 8, BufferUsage::Vertex});
  CHECK(dev->last_error() == "buffer needs a name");
}

TEST_CASE("null last_error names a zero-size buffer") {
  auto dev = make_null_device();
  dev->create_buffer({"b", 0, BufferUsage::Vertex});
  CHECK(dev->last_error() == "buffer size is zero");
}

TEST_CASE("null last_error names a nameless texture") {
  auto dev = make_null_device();
  dev->create_texture({"", 4, 4, 1, Format::Rgba8Unorm});
  CHECK(dev->last_error() == "texture needs a name");
}

TEST_CASE("null last_error names a zero texture extent") {
  auto dev = make_null_device();
  dev->create_texture({"t", 0, 4, 1, Format::Rgba8Unorm});
  CHECK(dev->last_error() == "texture extent is zero");
}

TEST_CASE("null last_error names an oversized texture") {
  auto dev = make_null_device();
  const std::uint32_t too_big = dev->capabilities().max_texture_size + 1;
  dev->create_texture({"t", too_big, 4, 1, Format::Rgba8Unorm});
  CHECK(dev->last_error() == "texture exceeds max_texture_size");
}

TEST_CASE("null last_error names a texture with no mip levels") {
  auto dev = make_null_device();
  dev->create_texture({"t", 4, 4, 0, Format::Rgba8Unorm});
  CHECK(dev->last_error() == "texture needs at least one mip level");
}

TEST_CASE("null last_error keeps the newest failure") {
  auto dev = make_null_device();
  dev->create_buffer({"", 8, BufferUsage::Vertex});
  dev->create_buffer({"b", 0, BufferUsage::Vertex});
  CHECK(dev->last_error() == "buffer size is zero");
}

TEST_CASE("null capabilities report the Null backend") {
  auto dev = make_null_device();
  CHECK(dev->capabilities().backend == Backend::Null);
}

TEST_CASE("null capabilities report a positive max_texture_size") {
  auto dev = make_null_device();
  CHECK(dev->capabilities().max_texture_size > 0);
}

TEST_CASE("null capabilities report no optional features") {
  auto dev = make_null_device();
  const Capabilities& caps = dev->capabilities();
  CHECK_FALSE(caps.astc);
  CHECK_FALSE(caps.etc2);
  CHECK_FALSE(caps.timeline_semaphores);
}

TEST_CASE("null capabilities report no compute support") {
  auto dev = make_null_device();
  const Capabilities& caps = dev->capabilities();
  CHECK_FALSE(caps.supports_compute_shaders);
  CHECK_FALSE(caps.supports_independent_compute_queues);
  CHECK_FALSE(caps.supports_async_compute);
  CHECK_FALSE(caps.supports_explicit_queue_synchronization);
}

TEST_CASE("null device has no active validation layer and no messages") {
  auto dev = make_null_device();
  CHECK_FALSE(dev->validation_active());
  CHECK(dev->validation_message_count() == 0);
}

TEST_CASE("null backend is named and CI-verified") {
  CHECK(backend_name(Backend::Null) == "null");
  CHECK(backend_state(Backend::Null) == BackendState::CiVerified);
}
