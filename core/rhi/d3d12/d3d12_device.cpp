// Direct3D 12 backend, first slice (FL 12_0): device, buffers, textures, command recording,
// one render pass (clear and store), graphics pipeline from DXIL, submit with a fence, readback.
// In CI it runs on the WARP software adapter ("WARP software evidence"), never on a real GPU.
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <wrl/client.h>

#include <climits>
#include <cstring>
#include <string>
#include <vector>

#include "lumora/rhi/rhi.h"

namespace lumora::rhi {
namespace {

using Microsoft::WRL::ComPtr;

DXGI_FORMAT to_dxgi(Format f) {
  switch (f) {
    case Format::Rgba8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case Format::Bgra8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case Format::R8Unorm: return DXGI_FORMAT_R8_UNORM;
    case Format::Depth32Float: return DXGI_FORMAT_D32_FLOAT;
  }
  return DXGI_FORMAT_UNKNOWN;
}

struct Slot {
  bool live = false;
  std::uint32_t generation = 1;
  ComPtr<ID3D12Resource> resource;
  ComPtr<ID3D12DescriptorHeap> rtv_heap;  // textures that can be render targets
  ComPtr<ID3D12PipelineState> pso;
  ComPtr<ID3D12RootSignature> root_sig;
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  std::uint64_t size = 0;
  std::uint32_t width = 0, height = 0;
  Format format = Format::Rgba8Unorm;
  bool is_texture = false;
  void* mapped = nullptr;
};

class D3d12Device;

class D3d12CommandBuffer final : public CommandBuffer {
 public:
  explicit D3d12CommandBuffer(D3d12Device* d) : dev_(d) {}
  void begin_render_pass(Handle color, const float clear[4]) override;
  void bind_pipeline(Handle p) override;
  void draw(std::uint32_t n) override;
  void end_render_pass() override;
  void copy_texture_to_buffer(Handle tex, Handle buf) override;
  ComPtr<ID3D12CommandAllocator> alloc_;
  ComPtr<ID3D12GraphicsCommandList> list_;
  D3d12Device* dev_;
  Slot* pass_target_ = nullptr;
  bool bad_ = false;
};

class D3d12Device final : public Device {
 public:
  ~D3d12Device() override {
    wait_idle();
    for (auto& s : slots_)
      if (s.mapped && s.resource) s.resource->Unmap(0, nullptr);
    if (event_) CloseHandle(event_);
  }

  bool init(const DeviceDesc& desc, std::string* error) {
    auto fail_init = [&](const char* m) {
      if (error) *error = m;
      return false;
    };
    UINT flags = 0;
    if (desc.enable_validation) {
      ComPtr<ID3D12Debug> dbg;
      if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) {
        dbg->EnableDebugLayer();
        debug_ = true;
        flags = DXGI_CREATE_FACTORY_DEBUG;
      }
    }
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)))) return fail_init("CreateDXGIFactory2 failed");
    // This slice always uses the WARP software adapter: it is what CI can run, and it keeps
    // evidence labels honest. A hardware adapter path is added with the real-GPU gate (G5).
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))) return fail_init("EnumWarpAdapter failed");
    DXGI_ADAPTER_DESC ad{};
    adapter->GetDesc(&ad);
    char name[128];
    WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, name, sizeof(name), nullptr, nullptr);
    caps_.device_name = name;
    if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_))))
      return fail_init("D3D12CreateDevice (FL 12_0) failed");
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device_->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue_)))) return fail_init("CreateCommandQueue failed");
    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) return fail_init("CreateFence failed");
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    caps_.backend = Backend::D3d12;
    caps_.max_texture_size = 16384;
    caps_.supports_compute_shaders = true;
    slots_.emplace_back();
    return true;
  }

  const Capabilities& capabilities() const override { return caps_; }
  std::string_view last_error() const override { return error_; }
  std::uint32_t live_resource_count() const override { return live_; }
  bool validation_active() const override { return debug_; }

  Handle create_buffer(const BufferDesc& d) override {
    if (d.name.empty()) return fail("buffer needs a name");
    if (d.size == 0) return fail("buffer size is zero");
    Slot s;
    s.size = d.size;
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = d.usage == BufferUsage::Readback ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = d.size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    s.state = d.usage == BufferUsage::Readback ? D3D12_RESOURCE_STATE_COPY_DEST
                                               : D3D12_RESOURCE_STATE_GENERIC_READ;
    if (FAILED(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, s.state, nullptr,
                                                IID_PPV_ARGS(&s.resource))))
      return fail("buffer creation failed");
    set_name(s.resource.Get(), d.name);
    return store(std::move(s));
  }

  Handle create_texture(const TextureDesc& d) override {
    if (d.name.empty()) return fail("texture needs a name");
    if (d.width == 0 || d.height == 0) return fail("texture extent is zero");
    if (d.width > caps_.max_texture_size || d.height > caps_.max_texture_size)
      return fail("texture exceeds max_texture_size");
    if (d.mip_levels == 0) return fail("texture needs at least one mip level");
    Slot s;
    s.is_texture = true;
    s.width = d.width;
    s.height = d.height;
    s.format = d.format;
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = d.width;
    rd.Height = d.height;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = static_cast<UINT16>(d.mip_levels);
    rd.Format = to_dxgi(d.format);
    rd.SampleDesc.Count = 1;
    const bool depth = d.format == Format::Depth32Float;
    rd.Flags = depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE cv{};
    cv.Format = rd.Format;
    if (depth) cv.DepthStencil.Depth = 1.0f;
    s.state = D3D12_RESOURCE_STATE_COMMON;
    if (FAILED(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, s.state, &cv,
                                                IID_PPV_ARGS(&s.resource))))
      return fail("texture creation failed");
    set_name(s.resource.Get(), d.name);
    if (!depth && d.mip_levels == 1) {
      D3D12_DESCRIPTOR_HEAP_DESC dh{};
      dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      dh.NumDescriptors = 1;
      if (FAILED(device_->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&s.rtv_heap)))) return fail("RTV heap failed");
      device_->CreateRenderTargetView(s.resource.Get(), nullptr, s.rtv_heap->GetCPUDescriptorHandleForHeapStart());
    }
    return store(std::move(s));
  }

  Handle create_graphics_pipeline(const PipelineDesc& d) override {
    if (d.name.empty()) return fail("pipeline needs a name");
    if (d.vertex_dxil.empty() || d.fragment_dxil.empty()) return fail("pipeline needs both DXIL shaders");
    if (d.color_format != Format::Rgba8Unorm) return fail("only Rgba8Unorm color is supported");
    Slot s;
    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err)))
      return fail("root signature serialization failed");
    if (FAILED(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                            IID_PPV_ARGS(&s.root_sig))))
      return fail("CreateRootSignature failed");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = s.root_sig.Get();
    pd.VS = {d.vertex_dxil.data(), d.vertex_dxil.size()};
    pd.PS = {d.fragment_dxil.data(), d.fragment_dxil.size()};
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = UINT_MAX;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pd.SampleDesc.Count = 1;
    if (FAILED(device_->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&s.pso))))
      return fail("CreateGraphicsPipelineState failed (bad DXIL?)");
    set_name(s.pso.Get(), d.name);
    return store(std::move(s));
  }

  std::unique_ptr<CommandBuffer> begin_commands() override {
    auto cb = std::make_unique<D3d12CommandBuffer>(this);
    if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cb->alloc_))) ||
        FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cb->alloc_.Get(), nullptr,
                                          IID_PPV_ARGS(&cb->list_)))) {
      error_ = "creating a command list failed";
      return nullptr;
    }
    return cb;
  }

  bool submit(CommandBuffer& base) override {
    auto& c = static_cast<D3d12CommandBuffer&>(base);
    if (c.bad_) {
      error_ = "command buffer recorded an invalid command";
      return false;
    }
    if (FAILED(c.list_->Close())) {
      error_ = "closing the command list failed";
      return false;
    }
    ID3D12CommandList* lists[] = {c.list_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    if (!wait_idle()) {
      error_ = "fence wait failed";
      return false;
    }
    return true;
  }

  void* map(Handle h) override {
    Slot* s = get(h);
    if (!s || s->is_texture || !s->resource) return nullptr;
    if (!s->mapped && FAILED(s->resource->Map(0, nullptr, &s->mapped))) return nullptr;
    return s->mapped;
  }
  void unmap(Handle) override {}  // buffers stay persistently mapped until destroyed

  void destroy(Handle h) override {
    Slot* s = get(h);
    if (!s) return;
    wait_idle();
    if (s->mapped && s->resource) s->resource->Unmap(0, nullptr);
    *s = Slot{.generation = s->generation + 1};
    free_.push_back(h.index);
    --live_;
  }

  Slot* get(Handle h) {
    if (h.index == 0 || h.index >= slots_.size()) return nullptr;
    Slot& s = slots_[h.index];
    return (s.live && s.generation == h.generation) ? &s : nullptr;
  }
  ID3D12GraphicsCommandList* list_of(D3d12CommandBuffer& c) { return c.list_.Get(); }

  bool wait_idle() {
    if (!queue_) return true;
    const UINT64 v = ++fence_value_;
    if (FAILED(queue_->Signal(fence_.Get(), v))) return false;
    if (fence_->GetCompletedValue() < v) {
      if (FAILED(fence_->SetEventOnCompletion(v, event_))) return false;
      if (WaitForSingleObject(event_, 30000) != WAIT_OBJECT_0) return false;
    }
    return true;
  }

 private:
  Handle fail(const char* m) {
    error_ = m;
    return {};
  }
  static void set_name(ID3D12Object* o, const std::string& n) {
    std::wstring w(n.begin(), n.end());
    o->SetName(w.c_str());
  }
  Handle store(Slot&& s) {
    std::uint32_t i;
    if (!free_.empty()) {
      i = free_.back();
      free_.pop_back();
      s.generation = slots_[i].generation;
    } else {
      i = static_cast<std::uint32_t>(slots_.size());
      slots_.emplace_back();
    }
    s.live = true;
    const std::uint32_t gen = s.generation;
    slots_[i] = std::move(s);
    ++live_;
    return {i, gen};
  }

  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12Fence> fence_;
  HANDLE event_ = nullptr;
  UINT64 fence_value_ = 0;
  bool debug_ = false;
  Capabilities caps_;
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::uint32_t live_ = 0;
  std::string error_;
};

void transition(ID3D12GraphicsCommandList* l, Slot& s, D3D12_RESOURCE_STATES to) {
  if (s.state == to) return;
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = s.resource.Get();
  b.Transition.StateBefore = s.state;
  b.Transition.StateAfter = to;
  b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  l->ResourceBarrier(1, &b);
  s.state = to;
}

void D3d12CommandBuffer::begin_render_pass(Handle color, const float clear[4]) {
  Slot* t = dev_->get(color);
  if (!t || !t->rtv_heap || t->format != Format::Rgba8Unorm) {
    bad_ = true;
    return;
  }
  transition(list_.Get(), *t, D3D12_RESOURCE_STATE_RENDER_TARGET);
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = t->rtv_heap->GetCPUDescriptorHandleForHeapStart();
  list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  list_->ClearRenderTargetView(rtv, clear, 0, nullptr);
  D3D12_VIEWPORT vp{0, 0, static_cast<float>(t->width), static_cast<float>(t->height), 0.0f, 1.0f};
  D3D12_RECT sc{0, 0, static_cast<LONG>(t->width), static_cast<LONG>(t->height)};
  list_->RSSetViewports(1, &vp);
  list_->RSSetScissorRects(1, &sc);
  pass_target_ = t;
}

void D3d12CommandBuffer::bind_pipeline(Handle p) {
  Slot* s = dev_->get(p);
  if (!s || !s->pso) {
    bad_ = true;
    return;
  }
  list_->SetGraphicsRootSignature(s->root_sig.Get());
  list_->SetPipelineState(s->pso.Get());
  list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3d12CommandBuffer::draw(std::uint32_t n) { list_->DrawInstanced(n, 1, 0, 0); }

void D3d12CommandBuffer::end_render_pass() {
  if (pass_target_) transition(list_.Get(), *pass_target_, D3D12_RESOURCE_STATE_COPY_SOURCE);
  pass_target_ = nullptr;
}

void D3d12CommandBuffer::copy_texture_to_buffer(Handle tex, Handle buf) {
  Slot* t = dev_->get(tex);
  Slot* b = dev_->get(buf);
  if (!t || !b || !t->is_texture || b->is_texture ||
      b->size < std::uint64_t{t->width} * t->height * 4) {
    bad_ = true;
    return;
  }
  transition(list_.Get(), *t, D3D12_RESOURCE_STATE_COPY_SOURCE);
  // Tightly packed rows are required here: D3D12 pitch alignment is 256 bytes.
  if ((t->width * 4) % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0) {
    bad_ = true;
    return;
  }
  D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
  dst.pResource = b->resource.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, t->width, t->height, 1, t->width * 4};
  src.pResource = t->resource.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  list_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

}  // namespace

std::unique_ptr<Device> create_d3d12_device(const DeviceDesc& desc, std::string* error) {
  auto d = std::make_unique<D3d12Device>();
  if (!d->init(desc, error)) return nullptr;
  return d;
}

}  // namespace lumora::rhi
