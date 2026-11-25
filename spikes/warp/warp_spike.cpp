// M1 spike: can a D3D12 device be created on the WARP software adapter on a hosted Windows
// runner, and can it run a command list and signal a fence? Evidence label if it works:
// "WARP software evidence". It is never RX 570 evidence.
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

#define CHECK(call)                                                      \
  do {                                                                   \
    HRESULT hr_ = (call);                                                \
    if (FAILED(hr_)) {                                                   \
      std::printf("WARP SPIKE FAILED at %s: hr=0x%08lx\n", #call, (unsigned long)hr_); \
      return 2;                                                          \
    }                                                                    \
  } while (0)

int main() {
  ComPtr<IDXGIFactory4> factory;
  CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
  ComPtr<IDXGIAdapter> warp;
  CHECK(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
  DXGI_ADAPTER_DESC ad{};
  warp->GetDesc(&ad);
  std::printf("adapter: %ls\n", ad.Description);

  ComPtr<ID3D12Device> dev;
  CHECK(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev)));
  D3D12_COMMAND_QUEUE_DESC qd{};
  qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  ComPtr<ID3D12CommandQueue> queue;
  CHECK(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));
  ComPtr<ID3D12CommandAllocator> alloc;
  CHECK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> list;
  CHECK(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr,
                               IID_PPV_ARGS(&list)));

  // Clear a small render target on WARP and read it back.
  D3D12_HEAP_PROPERTIES hp{};
  hp.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC rd{};
  rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  rd.Width = 4;
  rd.Height = 4;
  rd.DepthOrArraySize = 1;
  rd.MipLevels = 1;
  rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  rd.SampleDesc.Count = 1;
  rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_CLEAR_VALUE cv{};
  cv.Format = rd.Format;
  cv.Color[0] = 1.0f;  // red
  cv.Color[3] = 1.0f;
  ComPtr<ID3D12Resource> rt;
  CHECK(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                     D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, IID_PPV_ARGS(&rt)));
  D3D12_DESCRIPTOR_HEAP_DESC dh{};
  dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  dh.NumDescriptors = 1;
  ComPtr<ID3D12DescriptorHeap> heap;
  CHECK(dev->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&heap)));
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = heap->GetCPUDescriptorHandleForHeapStart();
  dev->CreateRenderTargetView(rt.Get(), nullptr, rtv);
  const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  list->ClearRenderTargetView(rtv, red, 0, nullptr);

  D3D12_RESOURCE_BARRIER bar{};
  bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  bar.Transition.pResource = rt.Get();
  bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  bar.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  list->ResourceBarrier(1, &bar);

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
  UINT64 total = 0;
  dev->GetCopyableFootprints(&rd, 0, 1, 0, &fp, nullptr, nullptr, &total);
  D3D12_HEAP_PROPERTIES rp{};
  rp.Type = D3D12_HEAP_TYPE_READBACK;
  D3D12_RESOURCE_DESC bd{};
  bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bd.Width = total;
  bd.Height = 1;
  bd.DepthOrArraySize = 1;
  bd.MipLevels = 1;
  bd.SampleDesc.Count = 1;
  bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  ComPtr<ID3D12Resource> rb;
  CHECK(dev->CreateCommittedResource(&rp, D3D12_HEAP_FLAG_NONE, &bd,
                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb)));
  D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
  dst.pResource = rb.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.PlacedFootprint = fp;
  src.pResource = rt.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.SubresourceIndex = 0;
  list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  CHECK(list->Close());
  ID3D12CommandList* lists[] = {list.Get()};
  queue->ExecuteCommandLists(1, lists);

  ComPtr<ID3D12Fence> fence;
  CHECK(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  CHECK(queue->Signal(fence.Get(), 1));
  CHECK(fence->SetEventOnCompletion(1, ev));
  if (WaitForSingleObject(ev, 30000) != WAIT_OBJECT_0) {
    std::printf("WARP SPIKE FAILED: fence timeout\n");
    return 2;
  }
  CloseHandle(ev);

  void* p = nullptr;
  CHECK(rb->Map(0, nullptr, &p));
  const unsigned char* px = static_cast<const unsigned char*>(p);
  bool ok = px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255;
  std::printf("pixel(0,0)=%u,%u,%u,%u\n", px[0], px[1], px[2], px[3]);
  rb->Unmap(0, nullptr);
  std::printf(ok ? "WARP SPIKE OK (WARP software evidence, not GPU evidence)\n"
                 : "WARP SPIKE FAILED: wrong pixel\n");
  return ok ? 0 : 2;
}
