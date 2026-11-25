# Architecture and repository layout (Section 5)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 5. High-level architecture

```
┌────────────────────────────┐ ┌─────────────────────────────┐
│ Lumora Editor │ │ Lumora Runtime │
│ (Windows + Linux, ImGui) │ │ Android app + Linux desktop │
│ │ export │ │
│ Scene authoring UI ───────┼────────► │ WallpaperService engine │
│ Export pipeline │.lumora │ Scene library + settings │
└───────────┬────────────────┘ └─────────────┬───────────────┘
 │ │
 └──────────────┬──────────────────────────┘
 ▼
 ┌─────────────────────┐
 │ Lumora Core (C++) │
 │ ┌─────────────────┐ │
 │ │ RHI │ │ Vulkan │ GLES │ D3D12
 │ │ Renderer │ │ scene graph → draw lists
 │ │ Scene format │ │ load/save/validate
 │ │ Lua VM + API │ │ sandboxed
 │ │ Assets │ │ glTF, KTX2, images, video
 │ │ Physics │ │ Jolt
 │ │ Animation │ │ skeletal + property
 │ └─────────────────┘ │
 └─────────────────────┘
```

- **Lumora Core** is a static/shared library written once. The editor links it for its live preview; the Android runtime links the same code via the NDK. One renderer, one scene format, one scripting API; no divergent reimplementations.
- The editor is desktop UI code (Dear ImGui through a platform window layer, Windows and Linux) on top of the core. The Android app is Kotlin/Java glue (WallpaperService, Activity, permissions, sensors) on top of the same core. The Linux desktop runtime is native C++ glue (desktop-wallpaper window, input, audio, power policy) on top of the same core; no divergent renderer.
- The RHI (Section 6) is new code written for this project. The previous Generals RHI (Vulkan backend pixel-tested; D3D12 backend written but untested) is mined for proven pieces (swapchain setup, descriptor management patterns, debug plumbing), but nothing is ported blindly.

### 5.1 Repository layout (monorepo)

```
lumora/
 LICENSE # Apache-2.0
 CMakeLists.txt
 cmake/ # toolchains, dependency fetching
 core/
 rhi/ # interface + vulkan/ + gles/ + d3d12/
 renderer/ # scene rendering on top of RHI
 scene/ # scene graph, format, (de)serialization
 script/ # Lua VM, API bindings, sandbox
 assets/ # glTF, images, KTX2, video decode
 anim/ # animation system
 physics/ # Jolt integration
 audio/ # playback + analysis core
 platform/ # OS abstraction used by core (android, windows, linux)
 editor/ # desktop app, Windows + Linux (Dear ImGui)
 desktop-linux/ # Linux wallpaper runtime + control app/CLI (Section 10A)
 android/
 app/ # Kotlin: library UI, settings, permissions
 wallpaper/ # WallpaperService + engine glue
 jni/ # Android platform layer for core
 tools/
 packer/ # CLI: validate + build.lumora packages
 shaderc/ # CLI wrapper: glslang + SPIRV-Cross steps
 tests/ # unit, golden-image, format round-trip
 docs/
 examples/ # sample scenes (also used by tests)
```

