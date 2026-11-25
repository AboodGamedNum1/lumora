# RHI specification (Section 6)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 6. The RHI (new, from scratch)

### 6.1 Design goals

1. **One mental model across three APIs.** A thin, explicit abstraction: device, swapchain/surface, command buffers, pipelines, buffers, textures, samplers, descriptor/binding sets, synchronization primitives. No magic, no hidden state; the renderer always knows what the GPU is doing.
2. **Mobile-first.** The lowest common denominator is GLES 3.2 on a 2019 mid-range Mali. The RHI's required feature set is what GLES 3.2 + Vulkan 1.1 + D3D12 FL 12_0 all do well; anything above that is an optional, queried capability.
3. **Backend selection at runtime.** Android: Vulkan preferred, GLES fallback (auto-selected if the Vulkan driver is broken or missing). Windows and Linux: Vulkan only on Linux (no GLES or D3D12 path on Linux; if Vulkan is unavailable the Linux runtime reports it and shows a static fallback); Windows: Vulkan default; D3D12 is **gated by an explicit backend support state**: `unvalidated` until it passes the Windows CI build/link/DXIL jobs and, if the M1 WARP spike works, the WARP suite (state `ci-verified`, software evidence only), and `supported` only after M10 gate G5 passes on the RX 570 (real-GPU evidence). The promotion condition is exactly: the G5 report is recorded for the pinned build, and only then does the Experimental label drop. While `unvalidated` or `ci-verified`, D3D12 is hidden in the normal backend selector and available only behind an Experimental / Not Runtime-Validated toggle with an unmistakable label (the `ci-verified` state changes the label text to say software evidence only, nothing else); it is never implied to be a working workflow. The enabled backend list derives from this per-backend support state (a build-time/config table), not from whether the backend was compiled in. If G5 fails, D3D12 stays `unvalidated` or `ci-verified`, unavailable as a normal selection, and the failure is recorded. Vulkan remains the default. (D3D12 stays in v1 scope.)
4. **Debuggability.** Validation layers / debug markers / GPU capture compatibility (RenderDoc on Windows and Android) are first-class: every resource gets a name.

### 6.2 Interface sketch (C++)

```cpp
namespace lm::rhi {
struct DeviceDesc { BackendPreference pref; bool enable_validation; };
class Device; // create/destroy, capabilities query
class Swapchain; // per-surface; frame-cap and pacing hints, not a present-interval control (Android Vulkan has none; pacing is by submission timing, Section 10.4)
class CommandBuffer; // record/submit, compute + graphics
class ShaderModule; // one compiled shader artifact for ONE backend (Section 6.2.1); there is no universal shader input
struct PipelineDesc; // ShaderModules + blend, depth, raster, vertex layout
class Buffer; // vertex/index/uniform/storage, mapped persistently where possible
class Texture; // 2D/array/cube, mip chains, compressed formats
class BindingSet; // descriptor set / bind group equivalent; supports immutable samplers
class Fence; class Semaphore;
struct RenderPassDesc; // attachments with explicit LoadOp/StoreOp + transient attachments, first-class; no subpass concept in v1, so "subpass-friendly" means single-pass-friendly with load/store ops and transient attachments
struct Capabilities { bool astc; bool etc2; uint32_t max_texture_size; bool timeline_semaphores; bool srgb_swapchain;... };
}
```

Key rules:

- **Each backend consumes its own compiled shader artifact** (SPIR-V is not the RHI's only shader input). The export toolchain produces one artifact per shader per backend; the RHI loads the artifact matching the active backend and never cross-compiles at runtime. See Section 6.2.1.
- Explicit frame pipelining (2 frames in flight default, 3 swapchain images), no implicit sync leaks.
- Uniform updates through per-frame ring buffers; the renderer never stalls the GPU.
- **Render passes and attachment ops are first-class, not Vulkan-only internals.** Every pass declares its attachments with explicit `LoadOp`/`StoreOp`; on a tile GPU these decide whether a full-resolution attachment round-trips through memory. Mapping: Vulkan render pass/subpass (dynamic rendering only where queried), GLES `glClear` + `glInvalidateFramebuffer` from `Clear`/`DontCare` ops, D3D12 `BeginRenderPass`. Transient (in-tile-only) attachments are expressible. Without this the GLES fallback and Vulkan differ in bandwidth by a large factor on the Mali-G76.
- **Resource lifetime rests on binary fences plus a per-frame-slot counter owned by the RHI** (works identically on GLES sync objects and D3D12 fences). Timeline semaphores are Vulkan 1.2 / `VK_KHR_timeline_semaphore`, not guaranteed on the Vulkan 1.1 baseline (Mali-G76, Android 10), so they are an optional queried capability only, never required.
- **`BindingSet` supports immutable samplers** (a sampler baked into the binding layout), required for Vulkan YCbCr video conversion samplers (Section 7).

### 6.2.1 Shader artifact contract and compatibility spec

```cpp
enum class ShaderFormat { SpirV, GlslEsSource, Dxil };
struct ShaderArtifact {
 Backend target; // Vulkan / GLES / D3D12
 ShaderStage stage;
 ShaderFormat format;
 std::vector<std::byte> data;
 ShaderMetadata metadata; // normalized reflection (bindings, uniforms, inputs)
};
```

- **Vulkan** consumes SPIR-V 1.3 (the SPIR-V version Vulkan 1.1 accepts), produced by glslang from the authoring GLSL.
- **OpenGL ES** consumes **GLSL ES 3.20 source** produced by SPIRV-Cross at export time; the GLES driver's own compiler builds it at program creation via `glCompileShader`. Corrected wording: Lumora ships no shader toolchain on the phone, but the GLES driver compiles GLSL ES source at load, exactly as in every GLES app. `glShaderBinary`/SPIR-V ingestion is **not** a general GLES path and is dropped from the plan.
- **D3D12** consumes DXIL, produced by DXC at export time from the SPIRV-Cross HLSL output, shader model 6.0. DXC is not Windows-only: official DXC releases ship Linux binaries (`dxc`, `libdxcompiler.so`, and the `libdxil.so` validator; since DXC v1.8.2505 (May 2025, verified on the DXC releases page) the compiler always uses its internal validator instead of searching for an external `DXIL.dll`), so the pinned Linux DXC build in the dependency manifest lets the Linux editor and the Linux packer produce validated DXIL like Windows. "Signed" is replaced by "validated and hash-stamped" (the DXIL validator hash is open source). The pin is DXC v1.9.2609 (published 29 Sep 2026, seen on the releases page) or newer, confirmed at M0; a reviewer reports a Linux release-archive packaging fault fixed in 1.9.2609 (unverified), so the Linux archive is smoke-tested in CI. That DXIL from the Linux DXC and from the Windows DXC both load through the D3D12 runtime is a CI test (load-equivalence on WARP if available), not an assumption. The GLSL, SPIR-V, SPIRV-Cross HLSL, DXC chain is lossy: `docs/shader-pipeline.md` defines explicit binding-to-register/space remapping and a root-signature budget (see 6.3), and every corpus shader is compiled for D3D12 in CI. Minimum shader model stays 6.0; the Agility SDK is added only if a newer model is needed (then `D3D12SDKVersion`/`D3D12SDKPath` and the redistribution layout are specified). The artifact cache key includes the DXC/validator version.
- **Authoring profile:** GLSL 4.60 core subset, documented in `docs/shader-pipeline.md`: allowed stages (vertex, fragment, compute), binding conventions with explicit `layout(binding=N)`, explicit `layout(location=N)` on every vertex input, varying and fragment output, uniform/storage block layout rules (std140/std430), precision requirements for mobile, and the unsupported-construct list with export-time diagnostics.
- **Semantic equivalence, not just source portability:** the spec pins texture-coordinate origin, clip-space depth range and Y-convention per backend, sampler/comparison-sampler behavior, derivative and interpolation behavior, and compute workgroup limits. Reflection data is normalized through SPIRV-Cross so binding layouts match across artifacts. A shader using a feature one backend lacks fails export with a clear diagnostic (no silent reduced variants in v1).
- **GLES-safe authoring subset, pinned in `docs/rhi-spec.md`:** no reverse-Z (GLES 3.2 has no clip control); fixed vertex-shader fixups for the clip-space z range ([0,1] to [-1,1]) and Y flip, with a defined render-to-texture Y-flip policy; separate image/sampler pairs are combined for GLES; SSBO use in vertex/fragment stages is queried (some drivers report zero limits); no subgroup operations and no other 4.60-only constructs. The unsupported-construct list is enforced by actually translating a shader corpus in CI, not by documentation alone.
- **External video sampler:** the contract has an explicit external/YCbCr sampler construct for video frames. GLSL 4.60 + SPIRV-Cross cannot express `samplerExternalOES`, so the GLES path uses a generated special shader variant (or a conversion post-pass); Vulkan uses an immutable YCbCr conversion sampler.
- **Module contract:** a `ShaderModule` carries target backend, format, stage, entry point, variant defines, and a cache key (hash of source + toolchain versions + variant defines + target). Compilation and translation happen only at export time (editor / `shaderc` CLI); the RHI never cross-compiles at runtime, the sole exception being the GLES driver's own `glCompileShader` of the packaged GLSL ES source. Diagnostics from every stage (glslang, SPIRV-Cross, DXC, GLES driver) map back to authoring GLSL line numbers. The packer emits exactly one artifact per shader per requested backend profile, and the loader selects the artifact matching the active backend from the package's shader table.
- **Editor preview:** the preview renders on the backend the author selects and warns when a shader or feature is known to differ on other backends. Golden-image tests (Section 16.1) are the real equivalence gate.

### 6.3 Backends

| Backend | Platforms | Role | Notes |
|---|---|---|---|
| Vulkan 1.1 | Android, Windows, Linux | Primary on Android; default on Windows; the only backend on Linux | Render passes are the baseline. `VK_KHR_dynamic_rendering` (core in Vulkan 1.3, an extension on 1.1) is an optional path enabled only when a runtime capability query reports it; never assumed. |
| OpenGL ES 3.2 | Android | Fallback | Selected automatically when Vulkan init fails or the driver is blocklisted; feature-parity subset. |
| D3D12 (FL 12_0) | Windows | Windows editor alternative + future Windows runtime | BindingSets map to one descriptor table per set plus a few root CBVs for per-draw data, within the 64-DWORD root-signature limit; the budget and mapping are pinned in `docs/rhi-spec.md` at M1. Compile/link/DXIL are CI-verified on the Windows runner; runtime evidence is WARP software evidence if the M1 spike works, and real-GPU evidence first at M10 gate G5 on the RX 570; editor selection gated until G5. Candidate to evaluate at M1 (unverified, not adopted): a direct SPIR-V to DXIL route such as Mesa's `spirv_to_dxil`; DXC itself consumes HLSL and emits SPIR-V, it does not consume SPIR-V. |

### 6.4 Milestone-owned validation

Each backend ships with its own golden-image test scene set (Section 16.1): the same reference scenes rendered on the **runnable** backends and compared within tolerance, with **separate baselines per renderer** (lavapipe, SwiftShader, ANGLE, WARP). Vulkan-on-Android is the reference implementation. **Coverage by backend:** Vulkan = software golden images (lavapipe/SwiftShader) in CI; GLES = software golden images (SwiftShader/ANGLE) in CI; D3D12 = CI-verified build/link/DXIL on Windows, plus WARP software golden images if the M1 spike works (labeled "WARP software evidence"); real-GPU D3D12 evidence first exists at M10 gate G5 on the RX 570 (owner-hardware-verified). D3D12 is never implied to be covered by the Vulkan/GLES software results, and WARP results are never reported as RX 570 results.

---

### 6.5 Capability model and feature matrix

The RHI exposes queried capabilities and feature tiers instead of assuming device equivalence. `docs/rhi-spec.md` (written at M1) pins the required/baseline/optional matrix across GLES 3.2, Vulkan 1.1, and D3D12 FL 12_0: storage buffers and image load/store, indirect draw, instancing, texture arrays/cubemaps, depth-stencil formats, MSAA, sampler features, compressed formats (ASTC/ETC2), `EXT_color_buffer_float` (needed for R11G11B10F render targets on GLES 3.2; documented LDR fallback), `EXT_multisampled_render_to_texture` (named probe in the phone script), render-target limits, sync/barriers, timestamp queries, pipeline cache, descriptor limits, max texture/buffer sizes, anisotropy, and compute workgroup/shared-memory limits. Compute shaders are GLES 3.1+, but actual limits are queried per device and the CPU particle fallback covers devices below the required tier.

**Compute capabilities are four separate queries, never inferred from each other:**

- `supportsComputeShaders` - can run compute shaders at all (GLES 3.1+, Vulkan 1.0+, D3D12 FL 12_0: yes).
- `supportsIndependentComputeQueues` - a compute queue distinct from the graphics queue (Vulkan/D3D12: queried per device; GLES: always false, GLES has a single implicit queue).
- `supportsAsyncCompute` - the driver actually overlaps compute with graphics work (vendor/driver-specific; GLES: false).
- `supportsExplicitQueueSynchronization` - cross-queue semaphores/fences (Vulkan/D3D12: queried; GLES: false, GLES synchronization is `glFenceSync`/`glMemoryBarrier` within one queue).

Fallback when a capability is absent: compute passes run on the graphics queue (serial with rendering) or fall back to the CPU path; no scene may assume overlap. Test reports record all four flags per device (Section 16.1).

### 6.6 GPU resource and workload limits

Shader validation alone cannot stop GPU resource exhaustion or driver hangs, so the RHI enforces per-scene quotas, checked at load and at runtime:

- Compute: max dispatch dimensions and workgroup counts, max shared memory per dispatch, max compute passes per frame.
- Textures and render targets: max dimensions, per-scene GPU memory budget per quality tier, capped offscreen pass count and total offscreen memory.
- Pipelines: per-scene pipeline and shader-variant caps; shader compile/link time budget with timeout; buffer size caps.
- Uploads: per-frame staging budget so a scene cannot stall the render thread.

Recovery ladder on GPU hang, device loss, or driver crash: drain and recreate the device and GPU resources from retained scene state (Section 10.3); on repeated failure fall Vulkan to GLES; finally render a static fallback frame and log to diagnostics. A scene that repeatedly triggers hangs is demoted to its lowest quality tier and the user is told.

### 6.7 Capability tiers

Because export fails when any backend lacks a feature and the GLES-safe subset is pinned, GLES 3.2 effectively defines the authoring language. This is stated up front so authors do not learn it from an export error.

- **T0:** the GLES 3.2 baseline. Every scene must be expressible in T0 on every backend.
- **T1:** optional features (for example compute-driven particles). Each T1 feature has a CPU or reduced fallback, and that fallback is part of the scene contract: a scene that uses a T1 feature declares it, and the editor shows the T0 fallback result in the preview.
