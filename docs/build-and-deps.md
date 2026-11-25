# Build system and dependencies (Section 14, 14.1)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 14. Build system & dependencies

- **CMake + Ninja everywhere.** One superbuild: `core` is platform-agnostic; `editor` builds on Windows/MSVC and on Linux; `desktop-linux` builds on Linux; `android` builds via Gradle invoking CMake for the native part.
- **Dependencies** (exact versions/commits pinned in `docs/dependency-manifest.md` with license, upstream repo, vendored-vs-fetched, update and security-patch policy, and tested platforms; fetched by CMake FetchContent or vcpkg manifest on Windows): Dear ImGui (a pinned docking-branch commit or tag, 1.92.x or newer, FreeType, own RHI backend), an ImGui text-editor component, glslang, SPIRV-Cross, **DXC**, cgltf (glTF parsing), KTX-Software / Basis Universal (texture compression), minizip-ng (vetted ZIP read/write; no hand-rolled archive parser), Jolt Physics, VHACD (import-time convex decomposition), Oboe (Android), libogg + libopusfile + libvorbisfile, HarfBuzz + SheenBidi + FreeType (the Lumora text engine), a bundled Arabic font (Noto Naskh Arabic or Cairo, OFL), stb_image, miniaudio, Lua 5.4 (vendored source), nlohmann/json, Catch2 (tests). `THIRD_PARTY_NOTICES.md` and a license check for every direct and transitive dependency ship in the repo and the APK.
- **Library-first policy:** see Section 14.1.
- **Linux dependencies:** window/input layer (SDL3 or GLFW with X11 and Wayland, pinned at M0: SDL3 default, GLFW fallback, 11.2), Wayland protocol libraries, PipeWire/PulseAudio client, D-Bus client, Mesa/Vulkan loader as runtime requirements; all permissively licensed or dynamically linked with license recorded; miniaudio covers editor audition on Linux.
- **Windows validation:** a Windows job runs in CI on every push on an explicitly pinned runner image (Section 4.3, 14.2); MSVC toolset, Windows SDK, DXC, CMake and Ninja versions are read from the job log, recorded in the manifest, and the build fails on drift. Real-GPU checks stay owner-side (G5).
- **Toolchain pinning:** exact NDK revision (not "r27+"; one that produces 16 KB-aligned libraries, Section 10.6), exact CMake and Ninja versions, exact MSVC toolset and Windows SDK, exact Gradle/AGP/Kotlin/JDK versions, all recorded in the dependency manifest; build scripts fail with a clear message when the pinned toolchain is absent.
- **C++26 honesty note:** no compiler fully implements C++26 today. The code targets the C++26 mode of the pinned MSVC/Clang/NDK toolchains and uses C++26 features only where the pinned toolchain actually supports them (falling back to C++23 equivalents otherwise), tracked in a compiler-feature matrix in `docs/cpp-standard.md`. The matrix is executable: CI compiles a feature-test header on each toolchain (MSVC, GCC, Clang, NDK Clang) and fails when `docs/cpp-standard.md` disagrees; the rule is to use only C++26 features that all pinned compilers accept (MSVC `/std:c++latest` is only partial). Compiler versions are pinned by container or setup action, not by moving `-latest` labels.
- **Determinism:** pinned dependency versions, reproducible `.lumora` output (stable ZIP ordering/timestamps) so exports can be diffed.

---

### 14.1 Library-first policy

The owner permitted reusing open-source, cross-platform libraries wherever that saves building from scratch. Rule: **before writing any non-core component, the agent checks for a maintained open-source library and uses it unless it fails a criterion below; the reason for any "build it ourselves" is written in `docs/dependency-manifest.md`.**

Criteria (all must pass): (1) license compatible with Apache-2.0 distribution (MIT, BSD, zlib, Apache-2.0, MPL-2.0 file-level are fine; LGPL only dynamically linked and replaceable; GPL/AGPL never); (2) builds on every target the component needs (Android arm64, Windows x64, Linux x86_64) with CMake; (3) actively maintained, with a pinned version or commit; (4) acceptable size and startup cost on the Realme 6 for runtime code; (5) anything that parses untrusted scene data stays inside the Section 15 validation and fuzz coverage; (6) wrapped behind a thin interface where it touches the core, so a swap stays possible.

What this changes in practice (candidates, each confirmed or rejected at the milestone that needs it):

| Need | Reuse (instead of building) | Notes |
|---|---|---|
| Editor window, input, file dialogs (Windows and Linux) | SDL3 (zlib) or GLFW, plus nativefiledialog-extended | SDL3 default, GLFW fallback, pinned at M0; also serves the Linux runtime window |
| Editor UI extras | Dear ImGui add-ons: ImGuizmo (gizmos), ImPlot (graphs; not a timeline or curve editor), `imgui_test_engine` (UI tests); no node editor in v1 (a node shader graph is post-v1, 11.2) | Fewer custom widgets; RTL still verified separately (Section 13) |
| GPU memory and Vulkan helpers | Vulkan Memory Allocator, volk, SPIRV-Reflect; D3D12 Memory Allocator (Windows) | Used inside the RHI backends; the RHI itself stays ours (D9) |
| Scene entities | EnTT (MIT) for entity storage | Replaces a hand-written ECS; behind the scene API |
| Mesh and asset processing | meshoptimizer, MikkTSpace, cgltf, KTX-Software/Basis, stb | Mesh simplification, tangents, compression, no custom code |
| Archive, JSON, logging | minizip-ng, nlohmann/json (or simdjson for read-only hot paths), spdlog + fmt | Already partly planned |
| Lua bindings | Plain Lua C API with a small wrapper, or sol2, whichever keeps the sandbox auditable | Sandbox review (Section 15) decides |
| Profiling | Tracy (editor and dev builds only) | Not shipped in release builds |
| Video | Platform decoder on Android (MediaCodec); FFmpeg (LGPL, dynamic, replaceable, built without GPL or non-free components) or a platform decoder on desktop; Android uses MediaCodec | Final choice at the video milestone, license check first |
| Linux integration | libwayland and the wlr-layer-shell protocol, a D-Bus library (sdbus-c++ or libdbus), PipeWire/PulseAudio client | Not rewritten; all dynamically linked |
| Text | HarfBuzz, SheenBidi (Apache-2.0; FriBidi is LGPL-2.1, awkward to ship replaceably in an APK or an AppImage, so it is not used), FreeType or stb_truetype | Already planned |

Not replaced by a library: the scene format, the Lumora Lua API surface, the render graph and the wallpaper lifecycle (these are the product). Whether the RHI itself should be replaced by an existing abstraction (bgfx, Diligent, wgpu-native) was decided by the owner (D9): the RHI is built in-house.
