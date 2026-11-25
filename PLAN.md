# Lumora project plan and status

This file is the live view of the plan: what is done, what is in progress and what is missing. It is updated at every milestone and whenever the plan changes. The full specification is [LUMORA_MASTER.md](LUMORA_MASTER.md) and the topic documents under [docs/](docs/); where this file and a normative document differ, the normative document wins. Session-level detail is in [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md).

Last updated: 2026-11-25. Current milestone: **M1, RHI core (in progress).** M0 exit criteria are met (CI-verified); a few M0 carry-over items remain, listed in section 4.

## 1. Vision

Lumora is a live wallpaper engine. A user authors a fully custom 2D or 3D scene (glTF models, GLSL shaders, Lua scripts, animation, audio, physics) in a desktop editor, exports one `.lumora` package, and the runtime renders it as the live wallpaper on Android (home and lock screen) and on the Linux desktop (GNOME on Ubuntu). Two programs, one shared C++ core, one renderer, one scene format, one scripting API.

## 2. Key decisions (summary)

Full decision log D1-D31 is in `LUMORA_MASTER.md`.

- Editor: Windows and Linux, C++ with Dear ImGui. Runtime: Android (minSdk 29, 64-bit `arm64-v8a` only) and a Linux desktop runtime. iOS is out of scope.
- Scripting: Lua 5.4, sandboxed. 3D assets: glTF 2.0. Shaders: GLSL, cross-compiled at export to SPIR-V, GLSL ES and HLSL.
- Rendering: one new in-house RHI with Vulkan, OpenGL ES 3.2 and Direct3D 12 backends.
- Performance: 60 fps baseline, 90 and 120 fps where the display allows; per-scene quality and frame cap for the end user.
- Interface languages: Arabic and English with full RTL support.
- Open source under Apache-2.0. Repository and CI exist from M0.
- Evidence honesty: CI-verified, WARP software evidence and owner-hardware-verified are different labels. Nothing is claimed verified on real Windows, Android or GNOME hardware without a real test.

## 3. Milestone status

Status values: not started, in progress, done (exit criteria met in CI, with the run linked in PROJECT_STATE.md).

| ID | Milestone | Status |
|---|---|---|
| M0 | Foundations and CI | done (exit criteria CI-verified, carry-over items open) |
| M0-M1 | GNOME wallpaper-method spike (agent-side evidence only) | not started |
| M1 | RHI core: Vulkan, GLES 3.2, D3D12 build and WARP spike | in progress (see section 5) |
| M2 | Runtime MVP: WallpaperService, pause rules, own process, crash breaker | not started |
| M3 | Scene format and packer | not started |
| M3S | Vertical slice: 2D parallax scene end to end in CI | not started |
| M4 | Renderer features (glTF, PBR, 2D, text; shadows, post-FX, particles, video) | not started |
| M5 | Scripting: Lua sandbox, cancellation and recovery | not started |
| M6 | Interaction layer (non-audio), flavors | not started |
| MT | Lumora text engine (Arabic shaping and bidi) | not started |
| M7a | Editor MVP | not started |
| M8 | Physics and audio | not started |
| M7b | Editor full | not started |
| M9 | Android app polish | not started |
| ML | Linux editor and Linux desktop runtime | not started |
| M10 | Release prep and owner hardware gates G1-G7 | not started |

Effort estimate: 80-135 weeks serial, about 18-31 months, re-estimated at the M2 and M4 boundaries (`docs/milestones.md`).

## 4. M0 detail

CI evidence: https://github.com/Captain3BoOd/lumora/actions/runs/45568499611 (commit 8b14281, all five jobs green; evidence label CI-verified, software graphics only, no GPU, no real device).

Exit criteria (from `docs/milestones.md`): CI green on Linux and Windows and builds the Android app; the Windows editor empty-window smoke test passes on the Windows runner (CI-verified, no GPU); an API 29 emulator boots in CI and runs one instrumented test.

| Item | Status |
|---|---|
| Repository, Apache-2.0 license, README.md, PLAN.md | done |
| Plan documents, `tools/check_docs.py`, PROJECT_STATE.md | done |
| CMake superbuild skeleton (core, tests, editor) | done (CI-verified) |
| Dependency pins (Catch2, SDL3, Dear ImGui) and manifest | done (CI-verified) |
| Coding standards and C++ standard docs | done |
| Shared test entry point (`tools/ci/test.py`) | done |
| CI: docs check, Linux build and tests with ASan and UBSan | done (CI-verified) |
| CI: Windows build and editor empty-window smoke test (no GPU) | done (CI-verified) |
| CI: Android Gradle release build, ABI set and 16 KB alignment checks | done (CI-verified) |
| CI: API 29 emulator boot and one instrumented test, KVM answer recorded | done (CI-verified; KVM works on ubuntu-24.04 hosted runners) |
| Secret scan in CI, Dependabot | done (CI-verified) |
| CodeQL | workflow added (C and C++, weekly and on push and PR); result recorded when the first run finishes. Uploading results needs code scanning enabled on the repository, which is an owner setting |
| Branch protection requiring green CI (owner repository setting) | not started, repository settings are not changed by the agent |
| NDK choice for 16 KB alignment | done (NDK 27.2.12479018 with flexible page sizes flag; alignment checked in CI) |
| DXC and glslang version pins | partly: SPIRV-Cross and DXC pins recorded in docs/dependency-manifest.md, not yet consumed by CI; glslang observed 15.1.0 |
| WARP and software-Vulkan-on-Windows spike | WARP done; software Vulkan on Windows: probe of the runner added, probe result (CI-verified observation): the Windows runner has the Vulkan loader (vulkan-1.dll in System32) but no Vulkan SDK and no software ICD installed, so Vulkan on Windows needs a downloaded software ICD (a Mesa build or SwiftShader); that download and a Vulkan test run are still to do |
| GNOME spike start | not started |
| Feature-test header check for `docs/cpp-standard.md` | not started |

## 5. M1 detail (in progress)

| Item | Status |
|---|---|
| RHI interface (backend-agnostic types, `Device`, capability flags, backend support states) and Null backend with unit tests | done (CI-verified) |
| WARP D3D12 spike on the Windows runner | done: a D3D12 device at feature level 12_0 on the WARP adapter, a render-target clear, copy and CPU readback all worked. Evidence label: WARP software evidence, CI-verified, never RX 570 evidence. Run: https://github.com/Captain3BoOd/lumora/actions/runs/45570530990. D3D12 backend state stays not implemented until the backend exists. |
| Software Vulkan on Windows spike | not started |
| GNOME wallpaper-method spike (source review, nested shell) | not started |
| Shader toolchain pins (glslang, SPIRV-Cross, DXC) | partly: glslang 15.1.0, validation layers and Mesa versions recorded as observed (Ubuntu packages), not hard-pinned; SPIRV-Cross vulkan-sdk-1.4.357.0 and DXC v1.9.2609 recorded as pins, not yet fetched by CI |
| Vulkan backend: instance, device, validation layer (logged active and asserted in the test), named buffers and textures, command recording, one render pass (clear and store), graphics pipeline from SPIR-V, submit and fence wait, texture readback | done as a first slice (CI-verified on the lavapipe software driver, run https://github.com/Captain3BoOd/lumora/actions/runs/45587423739). A test renders a clear plus a triangle and checks pixels, with zero validation messages. Missing: swapchain, descriptor sets and bindings, vertex buffers, depth, blending, sync beyond a fence, pipeline cache, other formats. |
| GLES 3.2 backend | not started |
| D3D12 backend build, link and DXIL on Windows CI | done (first slice): WARP-only device, buffers, textures, pipeline from DXIL (DXC from the Windows SDK, shader model 6.0), command recording, fence wait, readback. A triangle golden image matches its baseline in Windows CI. Evidence label: WARP software evidence, CI-verified, never RX 570 evidence. |
| Golden-image harness (lavapipe baseline, tolerance compare, diff artifacts, see `docs/golden-images.md`) | done (CI-verified, run https://github.com/Captain3BoOd/lumora/actions/runs/45590514447) |
| Emulator hello triangle through the RHI | not started |
| CodeQL | workflow added (C and C++, weekly and on push and PR); result recorded when the first run finishes. Uploading results needs code scanning enabled on the repository, which is an owner setting |

## 6. What is missing overall

Everything after M0. The product does not render anything yet: no RHI, no scene format, no Lua, no Android wallpaper service, no Linux wallpaper runtime, no editor beyond an empty window.

## 7. Open owner questions

OQ1 to OQ10 in `LUMORA_MASTER.md` are open. None blocks M0. Reviewer recommendations in that file are recommendations, not decisions.

## 8. Owner hardware gates (end of project)

G1 first render on the physical phone, G2 a Windows-exported package on the phone, G3 lock-screen and lifecycle on real firmware, G4 thermal and battery baseline, G5 D3D12 and Vulkan on the real GPU, G6 cross-backend fallback validation, G7 Linux real-session validation. Until these run, no claim of hardware verification is made.

## 9. Risks

Top risks are in `docs/risks.md`: scope for one executor, Arabic RTL in Dear ImGui, GNOME wallpaper method unproven without real hardware, unknown phone driver behavior, hostile shared scenes, slow CI-only feedback on Windows, owner availability at the end.

## 10. Change log of this file

- 2026-11-25: created at M0 start.
- 2026-11-25: M0 exit criteria met and recorded.
- 2026-11-25: M1 started: RHI interface and Null backend, WARP spike result.
- 2026-11-25: Vulkan backend first slice on lavapipe in Linux CI.
- 2026-11-25: Golden-image harness and first lavapipe baseline.
- 2026-11-25: Vulkan command recording, render pass, pipeline and pixel-checked triangle in CI. Leak detection is off in the ASan job because of third-party driver allocations (see ci.yml).
