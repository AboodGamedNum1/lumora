// Lumora RHI, first slice (M1): backend-agnostic types and the Device interface.
// See docs/rhi-spec.md (Section 6). Only the Null backend exists so far; the Vulkan,
// GLES and D3D12 backends are separate M1 steps.
#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace lumora::rhi {

enum class Backend : std::uint8_t { Null, Vulkan, Gles, D3d12 };

// Section 6.1: D3D12 is gated by an explicit support state. Nothing is "supported" before
// the owner's real-GPU gate (G5) has passed.
enum class BackendState : std::uint8_t { NotImplemented, Unvalidated, CiVerified, Supported };

enum class Format : std::uint8_t { Rgba8Unorm, Bgra8Unorm, R8Unorm, Depth32Float };
enum class BufferUsage : std::uint8_t { Vertex, Index, Uniform, Storage, Readback };

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

// Graphics pipeline, first slice: SPIR-V for the Vulkan backend, no vertex input (vertices come
// from the shader), triangle list, one color attachment, no blending or depth.
struct PipelineDesc {
  std::string name;
  std::span<const std::uint32_t> vertex_spirv;
  std::span<const std::uint32_t> fragment_spirv;
  // D3D12 consumes DXIL, one artifact per shader per backend (Section 6.2.1).
  std::span<const std::byte> vertex_dxil;
  std::span<const std::byte> fragment_dxil;
  // GLES consumes GLSL ES source (version 300 es or newer).
  std::string_view vertex_glsl;
  std::string_view fragment_glsl;
  Format color_format = Format::Rgba8Unorm;
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

// Records commands for one submission. Render passes always clear their color attachment first
// (LoadOp Clear) and leave it ready for copy-out (StoreOp Store); other ops come later.
class CommandBuffer {
 public:
  virtual ~CommandBuffer() = default;
  virtual void begin_render_pass(Handle color_texture, const float clear_rgba[4]) = 0;
  virtual void bind_pipeline(Handle pipeline) = 0;
  virtual void draw(std::uint32_t vertex_count) = 0;
  virtual void end_render_pass() = 0;
  // Copies a render target (left by a render pass) into a Readback buffer.
  virtual void copy_texture_to_buffer(Handle texture, Handle buffer) = 0;
};

class Device {
 public:
  virtual ~Device() = default;
  Handle fail_unsupported() { return {}; }
  virtual const Capabilities& capabilities() const = 0;
  // Returns an invalid handle and sets last_error() when the description is rejected.
  virtual Handle create_buffer(const BufferDesc& desc) = 0;
  virtual Handle create_texture(const TextureDesc& desc) = 0;
  virtual Handle create_graphics_pipeline(const PipelineDesc&) { return fail_unsupported(); }
  // Window surface: `native_window` is the platform window (ANativeWindow* on Android). The
  // returned handle can be the color target of a render pass; present() shows the result.
  // Backends without window support return an invalid handle.
  virtual Handle create_window_surface(void* native_window) { (void)native_window; return {}; }
  virtual bool present(Handle window_surface) { (void)window_surface; return false; }
  virtual std::unique_ptr<CommandBuffer> begin_commands() { return nullptr; }
  // Submits and waits for completion. Returns false on failure (see last_error()).
  virtual bool submit(CommandBuffer&) { return false; }
  // Host pointer of a buffer (all buffers are host visible in this slice), or nullptr.
  virtual void* map(Handle) { return nullptr; }
  virtual void unmap(Handle) {}
  virtual bool validation_active() const { return false; }
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
