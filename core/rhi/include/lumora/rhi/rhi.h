// Lumora RHI, first slice (M1): backend-agnostic types and the Device interface.
// See docs/rhi-spec.md (Section 6). Only the Null backend exists so far; the Vulkan,
// GLES and D3D12 backends are separate M1 steps.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace lumora::rhi {

enum class Backend : std::uint8_t { Null, Vulkan, Gles, D3d12 };

// Section 6.1: D3D12 is gated by an explicit support state. Nothing is "supported" before
// the owner's real-GPU gate (G5) has passed.
enum class BackendState : std::uint8_t { NotImplemented, Unvalidated, CiVerified, Supported };

enum class Format : std::uint8_t { Rgba8Unorm, Bgra8Unorm, R8Unorm, Depth32Float };
enum class BufferUsage : std::uint8_t { Vertex, Index, Uniform, Storage };

struct BufferDesc {
  std::string name;  // every resource is named (Section 6.1, debuggability)
  std::uint64_t size = 0;
  BufferUsage usage = BufferUsage::Vertex;
};

struct TextureDesc {
  std::string name;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t mip_levels = 1;
  Format format = Format::Rgba8Unorm;
};

struct Handle {
  std::uint32_t index = 0;  // 0 is invalid
  std::uint32_t generation = 0;
  bool valid() const { return index != 0; }
  friend bool operator==(const Handle&, const Handle&) = default;
};

struct Capabilities {
  Backend backend = Backend::Null;
  std::string device_name;
  std::uint32_t max_texture_size = 0;
  bool astc = false;
  bool etc2 = false;
  bool timeline_semaphores = false;  // optional, never required (Section 6.2)
  bool supports_compute_shaders = false;
  bool supports_independent_compute_queues = false;
  bool supports_async_compute = false;
  bool supports_explicit_queue_synchronization = false;
};

struct DeviceDesc {
  Backend backend = Backend::Null;
  bool enable_validation = true;
};

class Device {
 public:
  virtual ~Device() = default;
  virtual const Capabilities& capabilities() const = 0;
  // Returns an invalid handle and sets last_error() when the description is rejected.
  virtual Handle create_buffer(const BufferDesc& desc) = 0;
  virtual Handle create_texture(const TextureDesc& desc) = 0;
  virtual void destroy(Handle h) = 0;
  virtual std::uint32_t live_resource_count() const = 0;
  virtual std::string_view last_error() const = 0;
  // Number of validation-layer warnings and errors seen so far (0 when validation is off).
  virtual std::uint32_t validation_message_count() const { return 0; }
};

BackendState backend_state(Backend b);
std::string_view backend_name(Backend b);
// Returns nullptr and fills error for backends that are not implemented yet.
std::unique_ptr<Device> create_device(const DeviceDesc& desc, std::string* error = nullptr);

}  // namespace lumora::rhi
