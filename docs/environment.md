# Platform limits, target hardware and environments (Sections 3.3 and 4)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

### 3.3 Known platform limitations (honesty list)

- **Fingerprint / Face ID as wallpaper input is not possible.** Android biometric APIs only authenticate inside an app's own dialog; a wallpaper receives no fingerprint or face events. Lumora exposes **screen on/off and device lock/unlock events** instead, which covers the creative intent ("react when I unlock my phone").
- Lock-screen wallpapers: whether a live wallpaper also appears on the lock screen is **device/firmware behavior to verify, not a platform guarantee**. Android lets the user set a wallpaper for home or home+lock; OEM skins and launchers differ. Lumora treats this as a compatibility contract: a tested-device matrix (Realme 6 on its actual firmware first), an honest in-app explanation when only home screen is available, and no claim that the app can force lock-screen wallpaper anywhere. Details in Section 10.4. **Android specifics:** (a) before the first unlock after boot the wallpaper service does not run unless it is direct-boot aware; Lumora accepts the system default wallpaper pre-unlock in v1 (direct-boot-aware rendering with device-protected storage for the last scene is a documented post-v1 candidate); (b) per-screen scenes (different scene on lock) are an API 34+ feature, because `WallpaperService.Engine.getWallpaperFlags` is new in API 34 (verified in the API 34 diff). On API 29-33, including the Realme 6, the system picks the screen(s); how a live wallpaper shows on the lock screen there is OEM-defined and **unverified** until G3, and the plan does not promise a separate lock scene on those devices (open question OQ2); (c) touch on the lock screen reaches the wallpaper on few devices, so touch-dependent scenes degrade there by design; (d) ColorOS/Realme and other OEM skins can restrict lock-screen live wallpapers further - covered by the device matrix.
- **Linux desktop wallpaper compatibility:** there is no single Linux API for desktop wallpapers. X11 allows a desktop-type window under all other windows; Wayland leaves it to each compositor (wlroots-based compositors and KDE Plasma expose a background layer protocol; GNOME does not). GNOME 50 (March 2026) removed the X11 session entirely and Ubuntu 26.04 LTS and 26.10 ship GNOME 50/51 Wayland-only, so on the owner's own machine there is no X11 session and no X11 desktop-window path (an Xwayland window cannot be placed below all other windows by a client). GNOME support therefore needs a GNOME Shell extension based mechanism (Section 10A.2); it is the primary Linux case, not an optional one. Lumora treats Linux wallpaper support as a per-desktop compatibility matrix, verified by a design spike at the start of the Linux work and never claimed for a desktop that has not been tested. Phone-only inputs (gyroscope, accelerometer, lock-screen) do not exist on a desktop (Section 10A.4). Details and unknowns in Section 10A.
- Notification access requires a special user-granted permission (Notification Listener). It is optional and off by default; scenes that use it degrade gracefully without it.
- Audio-reactive scenes need microphone or media-session access (permission-gated, opt-in).
- Video textures are memory- and battery-hungry on a Mali-G76-class GPU; supported, but the editor warns the author and offers automatic downscaling.

---

## 4. Target hardware and environment

### 4.1 Reference phone (performance baseline)

| Property | Value | Consequence for the plan |
|---|---|---|
| Device | Realme 6 | The floor. `minSdk 29`. |
| SoC | MediaTek Helio G90T (2× Cortex-A76 + 6× Cortex-A55) | Multithread the runtime (render thread + logic thread + asset workers), but keep the render thread hot-loop allocation-free. |
| GPU | Mali-G76 MC4 | Vulkan 1.1 and GLES 3.2 available. Mid-range tile-based GPU: minimize overdraw, prefer ASTC/ETC2 compressed textures, avoid giant fragment shaders, keep offscreen passes few and small. |
| Display | 1080×2400, 90 Hz | 90 fps is a real target. Resolution is high for this GPU: offer per-scene render-scale (e.g. 0.75×) in quality settings. |
| RAM | 4–8 GB variants | Budget: a typical scene's runtime footprint stays under ~350 MB PSS including the app process (measured per Section 16.3, GPU memory tracked separately). |
| OS | Android 10 (upgradeable to 11) | Test on API 29 semantics; nothing above API 29 may be assumed at runtime without a gate. |

### 4.2 Windows machine (editor host)

Windows 11 x64, i7-6700, 16 GB RAM, RX 570 4 GB. The RX 570 supports Vulkan (1.2+ with current drivers) and D3D12 feature level 12_0, so both Windows-side RHI backends are genuinely testable on the owner's machine. The editor builds with the pinned MSVC toolset recorded in `docs/dependency-manifest.md` and must run well on this hardware. Hosted CI cannot run a GPU, so the RX 570 remains the only source of real-GPU evidence for Vulkan and D3D12 on Windows (gate G5); everything else about the Windows build is checked on the CI Windows runner (Section 14.2).

### 4.3 Agent-side build/test environment

Linux containers with no GPU and no physical devices. **GitHub Actions (Linux, Windows, and Android-emulator jobs) is available from the start (D31) and is the main verification environment; the local container is the developer loop.** **This environment is itself Linux, so the Linux editor and Linux desktop runtime can be built, unit-tested and software-rendered here; what it cannot show is a real desktop session on real GPU drivers (Section 4.4).** This drives the testing strategy in Section 16: everything that can be verified without hardware is verified by the agent; everything that needs hardware gets a prepared, numbered test script for the owner.

**Windows build and validation:** a Windows job runs on every push on a pinned hosted runner image (`windows-2025` or `windows-2025-vs2026`, chosen explicitly, never `windows-latest`; both labels are listed in the runner-images README, verified). It configures and builds the editor with MSVC, compiles and links the D3D12 backend against the Windows SDK, runs DXC to produce DXIL for the shader corpus, runs the platform-independent tests and the packer tests, launches the editor in an automated smoke test, and uploads a zip artifact. Toolchain versions (MSVC toolset, Windows SDK, DXC, CMake, Ninja) are read from the job log and recorded in `docs/dependency-manifest.md`; a build fails on drift. Hosted runners have **no physical GPU**. Evidence labels: **CI-verified (Windows, no GPU)** for build, link, DXIL and CPU tests; **WARP software evidence** for D3D12 device/clear/draw if the WARP spike succeeds; **owner-hardware-verified (RX 570)** for real driver behavior, performance and visuals. The owner Windows script is cut down to those real-GPU checks and the GUI test on real hardware (Section 16.2). Risk: a Windows-only failure is now caught on the next push, not at M10.

**D3D12 consequence:** the Linux agent container cannot run D3D12. The Windows CI job can compile and link it and compile DXIL on every commit. Whether the hosted Windows runner can create a D3D12 device on the **WARP** software adapter (and run clear/draw with golden images) is **unverified**: upstream projects report doing this on GitHub-hosted Windows runners (the LLVM offload-test-suite documents WARP use and says its default WARP version is tested in GitHub Actions), but it has not been run here. The M1 spike decides. If it works, D3D12 gets agent-side runtime and golden-image evidence labeled "WARP software evidence" with its own baselines (WARP output will not equal lavapipe output). If it does not, D3D12 runtime evidence first exists at G5. In both cases WARP evidence is never reported as RX 570 evidence. The same spike checks a software Vulkan ICD on Windows (lavapipe or SwiftShader); it is not assumed to work.

---

### 4.4 Linux targets and what can be verified where

The owner's recorded machines are the Realme 6 and the Windows 11 PC (4.2). **The distribution is Ubuntu (D29). The agent's own container is Ubuntu 22.04.5, which is not the owner's target: his machine runs the newest Ubuntu (26.04 or 26.10, GNOME 50/51, Wayland-only), so results from the 22.04 container are not evidence for his desktop stack.** The owner has a real Linux dual boot on his PC (the i7-6700 with the RX 570, assumed, confirmed at the first Linux test) and WSL (owner message). Consequences: (a) gate G7 (M10) uses the dual boot, the only source of owner-hardware-verified Linux results; (b) WSL is build/CPU-only evidence, labeled "WSL, build/CPU only"; (c) Windows and Linux real-GPU checks happen on the same machine, so one session can cover both.

| Check | Agent-side: local container and CI | Owner-side |
|---|---|---|
| Linux editor and runtime build, unit, fuzz, packer tests | Yes, on `ubuntu-24.04` and `ubuntu-26.04` runners or containers (**CI-verified**) | Recorded at the final run |
| Vulkan rendering, golden images | Yes, software Vulkan (lavapipe built from a pinned source in a cached image); **agent-verified (software renderer)** | Real GPU drivers (Mesa AMD/Intel, NVIDIA proprietary) on his machine only |
| X11 desktop-wallpaper window behavior | Partly: Xvfb with a test window manager (window type, stacking, input pass-through logic) | Real session |
| GNOME Shell extension: placement, stacking, input-through, enable/disable, hidden toplevel | Partly and **unverified feasibility**: a nested or headless `gnome-shell` session in an Ubuntu 26.04 container or VM in CI, with screenshots via the shell's D-Bus screenshot interface; no real GPU | Real GNOME Wayland session (G7) |
| Other Wayland compositors (wlroots layer-shell path) | Headless wlroots-style compositor, layer-protocol path only; says nothing about GNOME | Only if he uses one |
| KDE, multi-monitor, HiDPI, fractional scaling, suspend/resume, fullscreen-app detection, IME on Wayland | **No** | Yes, only on desktops he has |
| Audio capture for audio-reactive scenes (PipeWire/PulseAudio monitor) | Unit-level with a synthetic source | Real audio session |

Evidence labels from Section 16.1 apply: a Linux desktop claim is never reported as passed on software evidence alone.

