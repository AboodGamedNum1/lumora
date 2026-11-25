#include "lumora/rhi/rhi.h"

#include <vector>

namespace lumora::rhi {

namespace {

// Null backend: validates descriptions and tracks resource lifetime, no GPU work.
// It exists so the interface rules (named resources, valid sizes, no leaks) are tested in CI.
class NullDevice final : public Device {
 public:
  NullDevice() {
    caps_.backend = Backend::Null;
    caps_.max_texture_size = 4096;
    slots_.push_back({false, 0});  // slot 0 is the invalid handle
  }
  const Capabilities& capabilities() const override { return caps_; }

  Handle create_buffer(const BufferDesc& d) override {
    if (d.name.empty()) return fail("buffer needs a name");
    if (d.size == 0) return fail("buffer size is zero");
    return alloc();
  }
  Handle create_texture(const TextureDesc& d) override {
    if (d.name.empty()) return fail("texture needs a name");
    if (d.width == 0 || d.height == 0) return fail("texture extent is zero");
    if (d.width > caps_.max_texture_size || d.height > caps_.max_texture_size)
      return fail("texture exceeds max_texture_size");
    if (d.mip_levels == 0) return fail("texture needs at least one mip level");
    return alloc();
  }
  void destroy(Handle h) override {
    if (h.index == 0 || h.index >= slots_.size()) return;
    Slot& s = slots_[h.index];
    if (!s.live || s.generation != h.generation) return;  // stale or double destroy
    s.live = false;
    ++s.generation;
    free_.push_back(h.index);
    --live_;
  }
  std::uint32_t live_resource_count() const override { return live_; }
  std::string_view last_error() const override { return error_; }

 private:
  struct Slot {
    bool live;
    std::uint32_t generation;
  };
  Handle fail(const char* msg) {
    error_ = msg;
    return {};
  }
  Handle alloc() {
    std::uint32_t i;
    if (!free_.empty()) {
      i = free_.back();
      free_.pop_back();
    } else {
      i = static_cast<std::uint32_t>(slots_.size());
      slots_.push_back({false, 1});
    }
    slots_[i].live = true;
    ++live_;
    return {i, slots_[i].generation};
  }
  Capabilities caps_;
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::uint32_t live_ = 0;
  std::string error_;
};

}  // namespace

BackendState backend_state(Backend b) {
  switch (b) {
    case Backend::Null: return BackendState::CiVerified;
#if defined(LUMORA_HAS_VULKAN)
    case Backend::Vulkan: return BackendState::CiVerified;  // lavapipe software evidence only
#endif
#if defined(LUMORA_HAS_D3D12)
    case Backend::D3d12: return BackendState::CiVerified;  // WARP software evidence only, never RX 570
#else
    case Backend::D3d12: return BackendState::NotImplemented;
#endif
    default: return BackendState::NotImplemented;
  }
}

std::string_view backend_name(Backend b) {
  switch (b) {
    case Backend::Null: return "null";
    case Backend::Vulkan: return "vulkan";
    case Backend::Gles: return "gles";
    case Backend::D3d12: return "d3d12";
  }
  return "unknown";
}

#if defined(LUMORA_HAS_D3D12)
std::unique_ptr<Device> create_d3d12_device(const DeviceDesc& desc, std::string* error);
#endif
#if defined(LUMORA_HAS_VULKAN)
std::unique_ptr<Device> create_vulkan_device(const DeviceDesc& desc, std::string* error);
#endif

std::unique_ptr<Device> create_device(const DeviceDesc& desc, std::string* error) {
  if (desc.backend == Backend::Null) return std::make_unique<NullDevice>();
#if defined(LUMORA_HAS_VULKAN)
  if (desc.backend == Backend::Vulkan) return create_vulkan_device(desc, error);
#endif
#if defined(LUMORA_HAS_D3D12)
  if (desc.backend == Backend::D3d12) return create_d3d12_device(desc, error);
#endif
  if (error) *error = std::string(backend_name(desc.backend)) + " backend is not implemented yet";
  return nullptr;
}

}  // namespace lumora::rhi
