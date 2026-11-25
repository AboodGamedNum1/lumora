# Lumora project plan and status

This file is the live view of the plan: what is done, what is in progress and what is missing. It is updated at every milestone and whenever the plan changes. The full specification is [LUMORA_MASTER.md](LUMORA_MASTER.md) and the topic documents under [docs/](docs/); where this file and a normative document differ, the normative document wins. Session-level detail is in [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md).

Last updated: 2026-11-25. Current milestone: **M0, foundations and CI (in progress).**

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
| M0 | Foundations and CI | in progress |
| M0-M1 | GNOME wallpaper-method spike (agent-side evidence only) | not started |
| M1 | RHI core: Vulkan, GLES 3.2, D3D12 build and WARP spike | not started |
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

Exit criteria (from `docs/milestones.md`): CI green on Linux and Windows and builds the Android app; the Windows editor empty-window smoke test passes on the Windows runner (CI-verified, no GPU); an API 29 emulator boots in CI and runs one instrumented test.

| Item | Status |
|---|---|
| Repository, Apache-2.0 license, README.md, PLAN.md | done |
| Plan documents, `tools/check_docs.py`, PROJECT_STATE.md | done |
| CMake superbuild skeleton (core, tests, editor) | done (needs CI confirmation) |
| Dependency pins (Catch2, SDL3, Dear ImGui) and manifest | done (needs CI confirmation) |
| Coding standards and C++ standard docs | done |
| Shared test entry point (`tools/ci/test.py`) | done |
| CI: docs check, Linux build and tests with ASan and UBSan | in progress |
| CI: Windows build and editor smoke test | in progress |
| CI: Android Gradle build, ABI and 16 KB alignment checks | not started |
| CI: API 29 emulator boot and one instrumented test, KVM answer recorded | not started |
| Secret scan in CI, Dependabot, CodeQL | not started |
| Branch protection requiring green CI (owner repository setting) | not started, repository settings are not changed by the agent |
| DXC and glslang version pins, NDK choice for 16 KB alignment | not started |
| WARP and software-Vulkan-on-Windows spike | not started |
| GNOME spike start | not started |
| Feature-test header check for `docs/cpp-standard.md` | not started |

## 5. What is missing overall

Everything after M0. The product does not render anything yet: no RHI, no scene format, no Lua, no Android wallpaper service, no Linux wallpaper runtime, no editor beyond an empty window.

## 6. Open owner questions

OQ1 to OQ10 in `LUMORA_MASTER.md` are open. None blocks M0. Reviewer recommendations in that file are recommendations, not decisions.

## 7. Owner hardware gates (end of project)

G1 first render on the physical phone, G2 a Windows-exported package on the phone, G3 lock-screen and lifecycle on real firmware, G4 thermal and battery baseline, G5 D3D12 and Vulkan on the real GPU, G6 cross-backend fallback validation, G7 Linux real-session validation. Until these run, no claim of hardware verification is made.

## 8. Risks

Top risks are in `docs/risks.md`: scope for one executor, Arabic RTL in Dear ImGui, GNOME wallpaper method unproven without real hardware, unknown phone driver behavior, hostile shared scenes, slow CI-only feedback on Windows, owner availability at the end.

## 9. Change log of this file

- 2026-11-25: created at M0 start.
