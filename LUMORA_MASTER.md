# Lumora Master Plan

**Project:** Lumora (working name; the owner may rename it at any time; the name is a single constant in one config file, never hard-coded).
**Document:** Master specification, v2.0. Date: 2026-11-25. Language: English (the product UI is Arabic + English with full RTL).
**Status:** Awaiting owner review. Implementation starts only after the owner's explicit "start" (ابدأ), per decision D24.
**License:** Apache-2.0. The license file is part of the repository from the first commit.

## How to use this document

Read in this order: this file, then `PROJECT_STATE.md` (where the work stands), then only the normative files for the milestone in hand (map below). This file is the plan of record for vision, decisions, scope, milestones, risks and open questions. The files under `docs/` are normative for their topic and win over any summary here. `docs/review-history.md` is history and never normative. Section numbers (for example "Section 10.3") are stable IDs across all files and are not contiguous inside one file. Every session starts by reading this file and `PROJECT_STATE.md` and ends by updating `PROJECT_STATE.md`. Evidence labels are used everywhere: CI-verified, WARP software evidence, owner-hardware-verified. Nothing in this plan is claimed verified on real Windows, Android or GNOME hardware. Run `python3 tools/check_docs.py` before committing any document change.

## Open owner questions

There are 10 open owner questions. None blocks M0. Each is a question for the owner. The "Reviewer recommendation" lines come from the external review of the previous version, they are recommendations only and are not decisions.

1. **OQ1 Arabic text engine scope.** Dear ImGui has no shaping or bidi, so a fully Arabic editor needs its own text engine (milestone MT, 4-8 weeks). Options: (a) full engine with editable Arabic fields everywhere (plan of record); (b) reduced: shaped read-only Arabic labels in the editor, editable Arabic only in scene-text fields. The code editor stays left-to-right either way. Reviewer recommendation: (a), with (b) as an automatic cut if MT passes 8 weeks, reported at that boundary.
2. **OQ2 Lock-screen scope.** A different scene per screen needs API 34+. On the Realme 6 (Android 10/11) lock-screen behavior is OEM-defined and unverified. Does "home screen works, lock screen best-effort on old firmware" count as D4 met on that phone? Reviewer recommendation: yes, with gate G3 recording the real behavior.
3. **OQ3 Google Play features and flavors.** Microphone, notification listener and audio capture are the biggest Play-policy risk. Proposal: two flavors from day one, `play` (playback-spectrum audio only) and `full` (everything, for direct APK and GitHub). Acceptable for v1? Reviewer recommendation: yes.
4. **OQ4 Repository visibility.** Public repos get free CI minutes and fit open source (D19); private repos spend a limited minute budget. Proposal: public, after the hygiene steps in Section 14.2. Reviewer recommendation: public.
5. **OQ5 Windows signing route.** Options: (a) unsigned zip with a README warning, (b) an OV certificate later, (c) Microsoft Store MSIX. Facts about eligibility and SmartScreen are reviewer-sourced and unverified. Reviewer recommendation: (a) for v1.
6. **OQ6 CI artifacts and the start word.** D23 stays as recorded. Does the owner want an optional smoke APK from CI only at the M2 and M4 checkpoints, never required? And D24 stays as recorded: only the owner's explicit start word starts implementation. Reviewer recommendation: allow the optional smoke APK; treat no other message as the start.
7. **OQ7 GNOME extension and Ubuntu targets.** Approve the GNOME extension companion design (labeled experimental)? How does the extension reach the owner's machine: installed by the `.deb`, or published on extensions.gnome.org? Validation targets: Ubuntu 26.04 LTS and 26.10? Reviewer recommendation: approve; the `.deb` installs it; validate on both.
8. **OQ8 Windows 10.** Hosted CI cannot test Windows 10. Proposal: Windows 11 is the tested baseline; Windows 10 is best-effort and untested. Reviewer recommendation: yes.
9. **OQ9 Hardware named in a public repository.** The test plan names the owner's phone and PC models (Sections 4.1, 4.2). Keep them in public docs, or generalize? No recommendation; the owner's privacy call.
10. **OQ10 Second review of the GNOME extension code.** The extension placement code is written from GNOME documentation, not from the GPL reference extension (Section 10A.7). Does the owner want a second reviewer pass before release? Reviewer recommendation: yes.

Technical question TQ1 (shader packages versus decision D8, Section 15.1) is decided at M3 and reported to the owner. It is not an owner question yet.

## 1. Vision

Lumora is a **live wallpaper engine**: a system that lets a user build a fully custom, interactive 2D/3D scene, with their own models, shaders, scripts, animations, audio, and physics, and then run that scene as a live wallpaper: on an Android phone (home screen and lock screen) and on the Linux desktop.

The owner's own words define the product: the user must be able to create and draw the whole scene from their imagination with no artificial limits, interact with it through anything (touch, gyroscope, screen on/off, etc.), include 3D models, and write behavior in a script (Lua) and shaders (GLSL). "It's exactly like an engine, except after you build the scene it runs it as a live wallpaper."

The system is deliberately split into two programs:

1. **Lumora Editor**: a desktop application for **Windows and Linux** where scenes are authored.
2. **Lumora Runtime**: an Android app that loads an exported scene package and renders it as the live wallpaper, plus a **Linux desktop wallpaper runtime** that renders the same package as the desktop wallpaper.

The editor and the runtime are separate programs, but they share one C++ core (RHI, renderer, scene format, scripting VM) so that what the author sees in the editor preview is what the phone renders.


## 2. Decision log (D1-D31)

Decisions D1 to D31 are recorded here as received. They are not changed by this version. This file records them and cannot authenticate them. Only message identifiers, timestamps and quotes were removed from the recorded wording (the original wording is in the owner's private file, not in the repository). Entries in the Notes column are proposals or remarks, never decisions.

| # | Topic | Decision | Source | Notes (not part of the decision) |
|---|---|---|---|---|
| D1 | Product shape | Two programs: Editor (authoring) + Runtime (Android live wallpaper). Scenes exported as one compressed package file. | Owner, explicitly |  |
| D2 | Editor platform | Windows (Windows 10/11 x64). **Amended by D26: Linux is added.** Phone-based editing is a possible later addition, not v1. | Owner | Proposal (OQ8, not a decision): Windows 11 is the tested baseline; Windows 10 is best-effort and untested because hosted CI images are Server 2022/2025 only. |
| D3 | Editor tech | C++26 with Dear ImGui. | Owner |  |
| D4 | Runtime platform | Android, both home screen and lock screen. Android-first; iOS is out of scope (no public API for custom interactive wallpapers). | Owner |  |
| D5 | Scripting language | **Lua** (chosen over JavaScript). Reasons: tiny footprint, trivial C++ embedding, fast enough for wallpaper workloads, easy to sandbox so scenes from other people cannot harm the phone. JavaScript requires a much heavier engine (V8/QuickJS integration cost, larger attack surface). | Delegated to agent; chosen |  |
| D6 | Lua flavor | **Lua 5.4** (stock PUC-Rio), not LuaJIT. Reasons: LuaJIT is frozen at the Lua 5.1 language level, its future maintenance is uncertain, and wallpaper scripts are not the frame-rate bottleneck. 5.4 is current, portable to every Android ABI, and simpler to audit for sandboxing. | Delegated to agent; chosen |  |
| D7 | 3D asset format | **glTF 2.0** (`.glb` inside the package), including animations and skinning. The open standard for models + PBR materials + animation. | Owner approved the agent's pick |  |
| D8 | Shader pipeline | **GLSL is the single authoring language.** Shaders are written in GLSL and cross-compiled **at export time** to SPIR-V (for Vulkan), GLSL ES 3.2 (for OpenGL ES), and HLSL (for D3D12). Toolchain: glslang → SPIR-V, then SPIRV-Cross → GLSL ES / HLSL. | Owner confirmed the agent's default |  |
| D9 | Graphics APIs | One new **RHI** (render hardware interface) with three backends: **Vulkan**, **OpenGL ES**, **D3D12**. Built from scratch for this project, salvaging whatever is useful from the previous (Generals) RHI. | Owner |  |
| D10 | Minimum Android | The owner's own phone is the floor: **Realme 6** → `minSdk 29` (Android 10, which the Realme 6 shipped with; it is upgradeable to Android 11 / API 30, so nothing above API 29 is assumed). | Owner |  |
| D11 | Reference phone | Realme 6: MediaTek Helio G90T, Mali-G76 MC4 GPU, 1080×2400 display at 90 Hz, Android 10 (upgradable to 11). Vulkan 1.1 and OpenGL ES 3.2 are both available on this GPU. This phone is the performance baseline: if a scene runs well here, stronger phones fly. | Owner + verified specs |  |
| D12 | Windows test machine | Windows 11 64-bit, Intel Core i7-6700 @ 3.40 GHz, 16 GB RAM, Radeon RX 570 4 GB (Vulkan and D3D12 FL 12_0 both supported), ~360 GB free disk. Used to build and run the Editor. | Owner |  |
| D13 | Interaction model | The wallpaper can react to **anything the OS exposes**; the scene author decides, per scene, which inputs to use and how (through the editor and the Lua API). | Owner |  |
| D14 | Performance | 60 fps baseline; 90 fps and 120 fps where the display allows (the Realme 6 has a real 90 Hz panel, so 90 fps is a genuine target; 120 fps is for devices that support it). The end user can pick quality and frame-rate caps per scene. | Owner |  |
| D15 | Editor scope | The editor must be **powerful and complete**: everything needed for comfort and freedom to build literally any scene: scene graph, live viewport, animation timeline, particles, materials, shaders, physics, audio, per-scene settings UI. | Owner |  |
| D16 | Assets | All of them: glTF models with animation and skinning, images, and video. | Owner |  |
| D17 | Audio & physics | Both required. | Owner |  |
| D18 | Android app scope | Scene library + per-scene settings + lock screen support. The end user should be able to craft a scene as detailed as a game cutscene; it just happens to be a live wallpaper. | Owner |  |
| D19 | Distribution | Open source. Direct APK sharing now; Google Play later. | Owner |  |
| D20 | UI languages | Arabic + English, with proper RTL layout support. | Owner |  |
| D21 | GitHub / CI | **Superseded by D31.** The earlier text read: "Deferred: everything is built and tested locally first; the GitHub repo and CI come at the end." The owner has since made the repo and CI available from the start. | Owner (superseded by D31) |  |
| D22 | Testing duty split | The agent runs every test it can run locally (unit, golden-image, emulator, Wine where applicable). Final on-device verification, on the Realme 6 and the Windows 11 PC, is done by the owner, following prepared step-by-step test instructions. | Owner | Note (not a decision): Tests now run in CI first (D31); the local loop calls the same script. The owner-side split (real Realme 6 and RX 570 checks) is unchanged. |
| D23 | Progress protocol | No files are sent until everything is literally done. When a large, important part is finished, the agent sends a short progress message ("I finished X"). Re-confirmed 2026-11-24 against the first review's mid-project hardware gates: nothing goes on the owner's phone mid-project; phone testing happens at the very end. | Owner | Note (not a decision): D23 itself is unchanged. The earlier "one early owner Windows build" exception is **obsolete**: Windows CI replaces it. CI-built artifacts now exist from M0; whether the owner wants to download any of them mid-project is open question OQ6. Nothing is delivered to the phone before the end unless the owner says so. |
| D24 | Start condition | No implementation before the owner's explicit "ابدأ". This plan is the last deliverable before that word. | Owner |  |
| D25 | Architecture bitness | **64-bit only, both platforms**. Decided by the owner in conversation: 64-bit only on Android and Windows. Android ships exactly one release ABI: `arm64-v8a`. `armeabi-v7a` (32-bit ARM) is never built, shipped, or supported; no 32-bit ARM release binaries, APK splits, or packages are produced. `x86_64` may be used for emulator testing only and is labeled test-only everywhere; it is not a shipping ABI. The Windows editor targets x64 only (already implied by D2/D12; recorded explicitly). CI asserts the Android release ABI set is exactly `{ arm64-v8a }`, and the APK inspection step fails any release containing an `armeabi-v7a` native library. | Owner |  |
| D26 | Editor platforms | The Editor runs on **Windows and Linux**, not Windows only. Stated by the owner: the editor works on Windows and Linux, not Windows only. The distribution is decided (D29: Ubuntu); desktop environment and display server **decided (D29: GNOME; Wayland-only on the newest Ubuntu)**. | Owner |  |
| D27 | Linux wallpaper runtime | The live wallpaper itself also runs on **Linux** (same statement). It renders the same `.lumora` packages as the desktop wallpaper. Distribution is Ubuntu (D29); desktop **decided (D29: GNOME)**, display server Wayland-only on the newest Ubuntu. The Windows desktop wallpaper runtime was not requested and stays out of scope. | Owner |  |
| D28 | Linux architecture and packaging | Linux **x86_64 only**; Linux arm64 is not built. Packaging: **AppImage and a `.deb`**. The owner agreed to the question that proposed exactly these. D25 itself still covers only Android and Windows. | Owner | Proposal (not an owner decision): Build and test matrix amended (agent proposal, not an owner decision): AppImage built on the oldest supported baseline; `.deb` built and tested on Ubuntu 26.04 and 26.10 (Section 10A.5). |
| D29 | Linux distribution | **Ubuntu**, plus "the distribution the agent works on". The agent's environment was checked and is **Ubuntu 22.04.5 LTS (x86_64, glibc 2.35)**, so both halves of his answer name Ubuntu and the tested distribution is Ubuntu. Build baseline: Ubuntu 22.04 **(superseded: see Section 10A.5; the agent's own environment being 22.04 does not make it the target of the owner's machine, which runs 26.04 or newer)**. The owner runs the **newest Ubuntu release** (he does not remember the number). The exact release is read with `lsb_release -a` at the first Linux test and recorded; the plan does not guess it. A build on the 22.04 baseline runs on newer releases. **Desktop: GNOME (default Ubuntu desktop), confirmed by the owner.** Other distributions are best-effort only, not supported. | Owner |  |
| D30 | Open-source library reuse | The project may use open-source, cross-platform libraries wherever that avoids building something from scratch. Permission given by the owner in conversation. Limits that stay in force (agent policy, Section 14.1): licenses must be compatible with Apache-2.0, versions pinned, each library wrapped behind a thin interface where it touches the core, security-reviewed for anything that parses untrusted input. D9 (own RHI) is unchanged; the RHI question is closed. | Owner |  |
| D31 | GitHub repo and CI from the start | The GitHub repository and GitHub Actions are available from the start of the project, and the agent has wide freedom to test. A repository token, when it arrives, goes only through the secure vault; the token is never placed in this plan, in the repository, in workflows or in any message. CI secrets, if ever needed, live in GitHub environment secrets only (Section 14.2). This supersedes D21 and makes the "no Windows runner, owner-side only" design obsolete. | Owner |  |

### 2.1 Proposals log (agent proposals, not decisions)

| ID | Proposal | Status | Date | Where |
|---|---|---|---|---|
| P1 | Vertical-slice milestone M3S after M3 | accepted into plan (agent-level, owner may veto) | 2026-11-25 | Section 17 |
| P2 | `PROJECT_STATE.md`, ADR log and CI staleness check | accepted into plan | 2026-11-25 | Section 14.2 |
| P3 | Own wallpaper process, crash-loop breaker, isolated import validation | accepted into plan, costs measured at M2, M3 | 2026-11-25 | Section 10.7 |
| P4 | Shader static limits at export and load | accepted into plan; TQ1 open | 2026-11-25 | Section 15.1 |
| P5 | Reduced motion and flash-rate lint | accepted into plan | 2026-11-25 | Section 9.5 |
| P6 | Capability tiers T0 and T1 | accepted into plan | 2026-11-25 | Section 6.7 |
| P7 | Authoring experience work item by M7a | accepted into plan | 2026-11-25 | Section 11.5 |
| P8 | Lua API additions (logging, storage, seeded random, l10n, handles) | accepted into plan; values pinned later | 2026-11-25 | Section 9.4 |
| P9 | SheenBidi as the bidi library | accepted into plan | 2026-11-25 | Section 13 |
| P10 | Microphone: service stays running, capture pauses | accepted into plan; resume unverified | 2026-11-25 | Section 9.3, 10.7 |
| P11 | CI fast and nightly split, flavor lane matrix | accepted into plan | 2026-11-25 | Section 14.2 |
| P12 | Deferring Linux wallpaper as a spike fallback | rejected (not a listed outcome) | 2026-11-25 | Section 17 |
| P13 | Play and full flavors; public repo; Windows 11 baseline | proposed, awaiting owner | 2026-11-25 | OQ3, OQ4, OQ8 |

## 3. Scope

### 3.1 In scope (v1)

- Lumora Editor for Windows and Linux (C++26, Dear ImGui): complete scene authoring.
- Lumora Runtime for Android 10+ (API 29+): live wallpaper on home and lock screen, scene library, per-scene settings.
- **Lumora Desktop Runtime for Linux:** the live wallpaper on the Linux desktop, with a small control app/CLI for choosing scenes (Section 10A).
- New RHI with Vulkan, OpenGL ES 3.x, and D3D12 backends (Vulkan is the Linux backend; Section 6.3).
- Scene package format (single compressed file), open and documented.
- Lua 5.4 scripting with a sandboxed engine API.
- glTF 2.0 models with skeletal animation; images; video textures.
- GLSL shader authoring with export-time cross-compilation to SPIR-V / GLSL ES / HLSL.
- Full interaction set (Section 10): touch, motion sensors, screen/lock events, time, battery, audio-reactive input, notifications, and more.
- Audio playback in scenes (opt-in per scene) and audio-reactive scenes.
- Physics (rigid bodies, constraints, colliders) via an embedded physics engine.
- Arabic + English UI with RTL, in both the editor and the Android app.
- Direct APK distribution, then Google Play.

### 3.2 Out of scope (v1)

- iOS anything (Apple exposes no API for third-party interactive live wallpapers).
- Phone-based scene editing (post-v1 candidate).
- A **Windows** desktop-wallpaper runtime (the D3D12 backend and the shared core keep this door open; post-v1 candidate; the owner asked for Linux only, so Windows stays out unless he says otherwise). macOS anything.
- Linux on non-x86_64 CPUs and Linux phones/embedded targets (proposed exclusion, D28).
- Online scene marketplace / cloud sync (post-v1; v1 shares packages as plain files).
- Monetization, accounts, ads, analytics. None.
- Screen-reader and OS accessibility-tree support in the editor (Dear ImGui has none; keyboard-only operation and focus visibility are in scope).
- Floating editor panels as separate OS windows (multi-viewport); the editor is a single OS window on every platform.

## Architecture summary

Two programs share one C++ core. The Editor (Windows and Linux, Dear ImGui) authors scenes and exports one `.lumora` package. The Runtime renders the package as the Android live wallpaper and, separately, as the Linux desktop wallpaper. The core holds the RHI (Vulkan, OpenGL ES, D3D12), the renderer, the scene format, the sandboxed Lua 5.4 VM and API, assets, physics (Jolt) and animation. Shaders are written in GLSL and cross-compiled at export (D8). Untrusted packages are contained by the sandbox, archive defenses, capability grants and resource limits (Section 15). Details: `docs/architecture.md`, `docs/rhi-spec.md`, `docs/renderer.md`, `docs/format.md`, `docs/lua-api.md`, `docs/android-lifecycle.md`, `docs/linux-runtime.md`.

## Milestones (summary)

Order follows `docs/milestones.md` (Section 17), which holds the exit criteria. Total effort: 80-135 weeks serial, about 18-31 months. These are judgment estimates; the first calibration comes from M0-M2 and re-estimates happen at the M2 and M4 boundaries.

| ID | Milestone |
|---|---|
| M0 | Foundations and CI: repo, superbuild, pins, CI, shared test entry point |
| M0-M1 | GNOME wallpaper-method spike (agent-side evidence only) |
| M1 | RHI core: Vulkan, GLES 3.2, D3D12 build and WARP spike |
| M2 | Runtime MVP: WallpaperService, pause rules, own process, crash breaker; checkpoint |
| M3 | Scene format and packer |
| M3S | Vertical slice: 2D parallax scene end to end in CI |
| M4 | Renderer features (a: glTF, PBR, 2D, text; b: shadows, post-FX, particles, video); checkpoint |
| M5 | Scripting: Lua sandbox, cancellation and recovery, shader static limits |
| M6 | Interaction layer (non-audio), flavors |
| MT | Lumora text engine (Arabic shaping and bidi) |
| M7a | Editor MVP, authoring experience |
| M8 | Physics and audio |
| M7b | Editor full |
| M9 | Android app polish |
| ML | Linux editor and Linux desktop runtime |
| M10 | Release prep and owner gates G1-G7 |

## Top risks

Full table: `docs/risks.md` (Section 18).

- Scope is large for one executor: strict milestone order, editor MVP before full, progress report per milestone (D23), `PROJECT_STATE.md` for session continuity.
- Arabic RTL in Dear ImGui is weak upstream: own text engine milestone MT, reduced scope as an option (OQ1).
- Linux wallpaper on GNOME depends on an extension and a spike that cannot be proven without the owner's hardware: labeled experimental, gate G7.
- Realme 6 driver and lock-screen behavior are unknown until the owner gates at M10 (D23): GLES fallback, honest labels.
- Hostile friend scenes: sandbox, archive defenses, capability grants, shader static limits, crash isolation.
- CI-only feedback is slow and Windows cannot be reproduced locally: fast required checks, debug artifacts, a CI latency reserve.
- Owner availability for the M10 gates and Play policy risk for microphone and notification access (flavors).

## Documents map

Files marked planned do not exist yet and are written at the named milestone.

| File | Content | State |
|---|---|---|
| `docs/PROJECT_STATE.md` | Where the work stands | exists |
| `CHANGELOG.md` | Changes in this version | exists |
| `docs/environment.md` | Section 3.3 platform limits, Section 4 hardware and environments | exists |
| `docs/architecture.md` | Section 5 architecture and repository layout | exists |
| `docs/rhi-spec.md` | Section 6 RHI | exists |
| `docs/renderer.md` | Section 7 renderer | exists |
| `docs/format.md` | Section 8 package format | exists |
| `docs/lua-api.md` | Section 9 scripting and interaction | exists |
| `docs/android-lifecycle.md` | Section 10 Android runtime | exists |
| `docs/linux-runtime.md` | Section 10A Linux desktop runtime | exists |
| `docs/editor.md` | Section 11 editor | exists |
| `docs/physics-audio.md` | Section 12 | exists |
| `docs/localization-text.md` | Section 13 | exists |
| `docs/build-and-deps.md` | Section 14 and 14.1 | exists |
| `docs/ci.md` | Section 14.2 | exists |
| `docs/security-model.md` | Section 15 | exists |
| `docs/testing.md` | Section 16 | exists |
| `docs/milestones.md` | Section 17 and effort table | exists |
| `docs/risks.md` | Section 18 | exists |
| `docs/distribution.md` | Section 19 | exists |
| `docs/requirements-traceability.md` | Section 20 | exists |
| `docs/review-history.md` | Sections 22-31, history only | exists |
| `docs/dependency-manifest.md` | pinned versions, licenses, update policy | planned, M0 |
| `docs/cpp-standard.md` | C++ feature-subset rule | planned, M0 |
| `docs/linux-wallpaper-matrix.md` | spike result per desktop | planned, M0-M1 |
| `docs/shader-pipeline.md` | shader toolchain detail | planned, M1 |
| `docs/performance-budgets.md` | frame, memory and battery budgets | planned, M2 |
| `docs/project-format.md` | editor project layout | planned, M7a |
| `docs/rtl-support.md` | RTL behavior spec | planned, MT |
| `docs/release-and-signing.md` | release signing | planned, M10 |

## Other open items

Final product name; Google Play developer account (needed at M10); optional post-v1 inputs (weather, network) deferred; Linux multi-monitor mode (per-monitor scene or span) and confirmation that no Windows desktop wallpaper runtime is wanted; the word ابدأ from the owner is required before M0 starts (D24).
