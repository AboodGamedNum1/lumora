# Android runtime and wallpaper lifecycle (Section 10)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 10. Lumora Runtime (Android app)

### 10.1 Components

1. **Wallpaper engine** (`WallpaperService.Engine` subclass hosting the native core):
 - Creates/destroys the GL/Vulkan surface from the service callbacks, drives the frame loop (Choreographer-aligned), forwards touch and offset events, manages sensor listeners, honors visibility (renders only while visible, a hard battery rule).
 - Frame pacing: user-selected cap (30/60/90/120/auto). The app **paces its own loop and requests, but cannot force, a display mode**: the system decides whether the panel runs at 90/120 Hz; Lumora auto-detects the current refresh (90 Hz on the Realme 6) and never exceeds it unless the user insists.
2. **App (library + settings):**
 - Scene library: installed scenes with thumbnails, import `.lumora` via the Storage Access Framework document picker or share-sheet, delete, reorder favorites.
 - Set-as-wallpaper flow (home, or home+lock, with the OEM caveat explained). The app opens the system live-wallpaper chooser through `ACTION_CHANGE_LIVE_WALLPAPER` with Lumora preselected and the user confirms home/lock in the system dialog; the plan does not assume an app can set a lock-only live wallpaper itself (reviewer claim, unverified).
 - Per-scene settings screen generated from `settings.json`.
 - Global settings: default frame cap, battery-saver behavior, master audio switch, permissions center (mic, notifications) with clear Arabic/English explanations.
 - Language: Arabic (RTL) / English, in-app switch, follows system by default.
3. **Lifecycle & power policy (hard rules, D14):**
 - Never render while *known* invisible (screen off, surface destroyed, `onVisibilityChanged(false)`), and treat visibility as **best-effort, not a guarantee**: Android does not reliably report "another app fullscreen" or "covered by AOD" to a wallpaper, and OEM skins (incl. the Realme 6 on API 29/30) deliver visibility callbacks inconsistently (**unverified:** one reviewer found no official document for an Android 16 `onVisibilityChanged` restriction; another reports that AOSP `WallpaperService` gates visibility reports on the display being fully on. The plan states no Android 16 fact and relies on the fallbacks below). The hard fallbacks are: the dynamically registered `ACTION_SCREEN_OFF` receiver, a watchdog that stops rendering when the frame loop runs hidden with no offsets/touch activity, and the per-OEM behavior test on the Realme 6 in the phone script.
 - Battery-saver mode → drop to 30 fps and the scene's low-quality preset (`PowerManager.isPowerSaveMode` + `ACTION_POWER_SAVE_MODE_CHANGED`).
 - Thermal guard: if the device reports thermal throttling, step render-scale down one notch and log it to the in-app diagnostics screen. `PowerManager.getCurrentThermalStatus` exists since API 29 but many OEMs (incl. MediaTek devices) report few or no statuses, so a frame-time fallback is added: a sustained frame-time overrun triggers the same render-scale step even when the device reports nothing.
 - The single "< 1% battery/hour visible at 60 fps" figure is deleted: display + OS alone exceed it before any wallpaper renders. Battery targets are **per workload, expressed as incremental drain over a static-wallpaper baseline** and re-baselined from the first G4 measurement (methodology in Section 16.3).

### 10.2 Tech

Kotlin UI (Jetpack Compose, RTL-correct), native core via JNI (swig-free, hand-written thin JNI), pinned NDK (Section 14), Gradle + CMake. **ABI policy:** the sole shipping Android release ABI is `arm64-v8a`. `armeabi-v7a` (32-bit ARM) is excluded: never built, shipped, or supported; no 32-bit ARM release binaries, APK splits, or packages are produced. `x86_64` is permitted for emulator testing only and is labeled test-only wherever it appears; it is not a shipping ABI. CI asserts that the release ABI set is exactly `{ arm64-v8a }`, and the APK/package inspection step confirms that no `armeabi-v7a` native libraries are present (Sections 16.1, 16.4). The Windows editor targets x64 only. minSdk 29 (D10), targetSdk per the Play deadline table in Section 10.6.

---

### 10.3 Wallpaper lifecycle, threading, and surface management

The wallpaper engine runs an explicit state machine instead of ad-hoc callbacks:

`Detached → SurfaceReady → RendererInitializing → RunningVisible ⇄ PausedHidden → SurfaceLost → Recovering → ShuttingDown`, plus `CrashDisabled`, entered by the crash-loop breaker (Section 10.7)

- **Thread ownership:** `WallpaperService.Engine` callbacks arrive on the Android main thread; the native renderer and the RHI device live on a dedicated render thread; scene simulation and Lua run on a logic thread; asset I/O on worker threads. Cross-thread communication is command queues processed at frame boundaries; the scene graph is mutated on the logic thread only. Sensor data and settings flow in as immutable snapshots; completed assets are published atomically.
- **Surface rules:** surface creation starts `RendererInitializing`; swapchain (re)creation happens only on the render thread; before a native window is destroyed, the render thread is drained and the swapchain released, and no Vulkan object may reference a destroyed surface. Resolution/format/orientation/refresh changes recreate the swapchain without reloading the scene.
- **Failure handling:** `OUT_OF_DATE` and `SUBOPTIMAL` trigger swapchain recreation; `SurfaceLost` pauses rendering and keeps scene state; device-lost moves to `Recovering` (recreate device and GPU resources from the retained scene) and falls back to the GLES backend if Vulkan init or recreation repeatedly fails; unrecoverable failure shows a static fallback and logs to diagnostics.
- **Multiple engines:** preview and active engines are independent Engine instances sharing nothing mutable; each owns its render thread. `getWallpaperFlags` (the first public way for an engine to learn which screen it draws) was added in API 34, verified in the API 34 diff. Different scenes on home and lock are therefore an **API 34+ feature**, tested on an API 34+ emulator (no owned device has it). On API 29-33 the behavior is OEM-defined and **unverified**; the engine design tolerates two engines appearing, but offers no per-screen scene there, and the phone script records what the Realme 6 does (G3). GPU memory budgets assume up to **three** concurrent engines (preview + home + lock) on API 34+ and two otherwise; per-engine and aggregate budgets are in Section 16.3.
- **Main-thread teardown budget:** `onSurfaceDestroyed`/`onDestroy` are main-thread calls, and the render thread must release the swapchain before the surface dies. The state machine blocks the main thread on that handoff with a **bounded wait** (a slow Vulkan teardown on Mali must not ANR the process); on timeout it leaks the swapchain deliberately, logs to diagnostics, and recovers at the next surface creation. A deliberately leaked Vulkan swapchain on Mali can leave the surface BufferQueue connected so that the next surface creation fails; the last resort is restarting the wallpaper process through the service, covered by a test.
- **Opt-ins:** touch delivery (`setTouchEventsEnabled`) and offset notifications are explicitly enabled where the scene needs them; visibility callbacks drive the hard render/pause rule and the registration/unregistration of sensor listeners and audio resources. Process death restores the last selected scene and its settings on service recreation.
- The full contract (thread marshalling, transition table, GLES-fallback triggers) is written as `docs/android-lifecycle.md` at M2.

### 10.4 Simulation timing and GPU resource lifetime

- **Timing model:** fixed-timestep physics (e.g. 60 Hz), bounded variable-step scene/Lua update, render interpolation between simulation states, a maximum catch-up step count (no spiral of death), time-delta clamping after background pauses, and deterministic playback mode in the editor. Lua callbacks run on the update tick; unbounded script work can never block the render thread (Section 9.1 budgets).
- **Frame evaluation order:** one fixed pipeline per frame: (1) input sampling (touch, sensors, OS events as immutable snapshots), (2) Lua scripts (`on_update` plus queued events), (3) animation evaluation, (4) physics fixed steps, (5) transform resolution, (6) render submission. Ownership: physics-owned transforms (dynamic bodies) are read-only to scripts and animation; animation writes only its animated properties and loses to physics on physics-driven nodes; local-versus-world writes resolve at the transform-resolution step; two writers hitting the same property in one frame resolve in pipeline order (later stage wins) and the editor flags author-created conflicts. Events dispatch in a defined order at stage boundaries; physics events queue into the next script stage.
- **Resource lifetime:** deferred destruction via binary fences plus per-frame-slot counters (resources die only after the GPU finishes them; timeline semaphores are an optional Vulkan capability only), per-frame allocators, defined BindingSet/descriptor lifetimes, staged uploads with transfer synchronization, pipeline-cache persistence on Vulkan (GLES: `glGetProgramBinary` cache keyed by `GL_RENDERER` + driver version with recompile-from-source fallback, and first-run link time counts inside the cold-load budget), per-backend memory heaps with budget tracking. Persistent mapping is a queried strategy per backend, not an assumption.
- **Refresh and thermal behavior:** refresh rate is queried from the display; the app caps/paces but does not force panel modes. 90 fps is opportunistic: Android 10 has no `Surface.setFrameRate` (API 30+), so on the Realme 6 the system decides the panel mode and a wallpaper surface often runs at 60; pacing is derived from the actually measured Choreographer vsync period, with 3 swapchain images and 2 frames in flight, and `setFrameRate` is called on API 30+. The 120 fps budget row is marked **unverifiable on owned hardware** (no 120 Hz device). Android's Frame Pacing library (Swappy) is evaluated at M2 and either adopted or the decision documented. Thermal status steps render-scale down one notch with hysteresis (no oscillation between tiers) and recovers gradually; every step is logged to diagnostics.
- **Pre-rotation:** the Vulkan swapchain handles `currentTransform` (pre-rotation) and folds the rotation into the camera matrix; without it SurfaceFlinger adds a composition pass (measurable ms + GPU frequency cost) on every frame. Tested with a rotated surface.

### 10.5 Runtime scene UI / Canvas scope

v1 scenes get a **minimal 2D UI set**, distinct from the editor's UI (which is full ImGui):

- Text labels and dynamic text (clocks, counters) with the Section 13 text stack.
- Sprite/image elements in ordered 2D layers (Section 7.1).
- Touch regions: named hit areas with anchors, routed to Lua events (tap/drag), with defined priority over scene-level touch handlers.
- Anchored layout: 9-anchor placement plus pixel/percentage offsets and safe-area insets; positioning is resolution- and render-scale-independent.

Not in v1: buttons, panels, scroll views, input fields, and full responsive layout engines - post-v1. User configuration of a scene happens through `settings.json` screens, not in-wallpaper widgets.

### 10.6 Android platform checklist

All items come from the Android review unless noted; Android facts not re-fetched are marked unverified.

- **16 KB page size.** Play requires native code in apps targeting Android 15+ to support 16 KB pages (cited from developer.android.com page-sizes guide by the reviewer, not re-fetched). Pin an NDK revision that aligns shared libraries by default (r28 or newer, to be confirmed at M0) or set `-Wl,-z,max-page-size=16384`; CI checks every `.so` for 16 KB alignment and runs an Android 15 16 KB emulator image. Applies to vendored Lua, Oboe, HarfBuzz, Jolt and all other native libraries.
- **Target SDK and behavior table.** Per the Play policy page the reviewer cited, new apps and updates must target API 36 from 2026-08-31 (unverified here; re-check at M10). `docs/android-lifecycle.md` carries a table "behavior changes API 30..36 versus which device or emulator tests them". CI lanes: API 29 (minSdk, every PR), API 34 and API 36 (nightly, required), x86_64 emulators on KVM-enabled Linux runners. The Realme 6 can never exercise API 34-36 behavior (typed foreground services, edge-to-edge enforcement, the lock-engine API).
- **Signing and AAB decisions move to M0.** AAB build path, Play App Signing, keystore and secret policy are decided early (Section 14.2); a Play developer account and Play privacy policy, Data safety form and permission declarations are M10 deliverables.
- **Direct Boot.** The wallpaper service and all code touching credential-protected storage are not `directBootAware`. After reboot, state is read only after `ACTION_USER_UNLOCKED` or `ACTION_BOOT_COMPLETED`; last-scene restore tolerates storage that was unavailable when the service first bound; tested with a reboot on the Realme 6 and on an emulator with a secure lock.
- **Unlock events.** A manifest `ACTION_USER_PRESENT` receiver cold-starts a dead process on every unlock, costs battery and can trigger OEM autostart blocks. Register dynamically from the live engine; use a manifest receiver only for scenes that need wake-on-unlock. `USER_PRESENT` may not fire on devices with no secure lock; the no-lock case is stated in `docs/android-lifecycle.md`.
- **Thermal and power.** Use `OnThermalStatusChangedListener` and, on API 30+, `getThermalHeadroom` in addition to `getCurrentThermalStatus`; handle ambient/AOD (stop rendering); no wakelocks or timers while invisible. OEM "super power saving" modes (for example ColorOS) can kill the process: an added risk row. CI cannot validate any battery or thermal number.
- **Manifest and metadata (M2 checklist).** `android.software.live_wallpaper` uses-feature (consider `required=false` for Play device targeting), `BIND_WALLPAPER` service permission, wallpaper XML (thumbnail, `settingsActivity`, `supportsMultipleDisplays`), `onComputeColors` for system theming.

### 10.7 Crash isolation, crash-loop breaker and microphone state table

Acceptance criterion 8 (crashes never cause unrecoverable crash loops) needs a mechanism for native crashes, not only Lua errors.

1. **Process split.** The WallpaperService runs in its own process (`android:process` set to a separate `:wallpaper` process), apart from the app UI process. A native crash in a decoder, Jolt or a GPU driver then takes down the engine only, and the system restarts the service. The memory cost on a 4 GB phone is measured at M2 (not measured yet).
2. **Crash-loop breaker.** A crash counter is persisted per scene and per scene+backend. After N crashes within a window (N and the window are pinned at M2; the initial proposal is 3 in 24 hours) the scene enters `CrashDisabled`, the engine falls back to the previous scene or the system default wallpaper, and the app tells the user. A scene+backend pair that crashes repeatedly on Vulkan is moved to GLES before the scene itself is disabled.
3. **Import-time validation in an isolated process.** Parsing of untrusted packages (ZIP, glTF, KTX2, images, audio) is evaluated in an `isolatedProcess` service with no permissions, data passed by file descriptor, and only a validated result returned. Its IPC and startup cost is measured at M3. The fallback is in-process parsing under the limits and fuzzing of Section 15.
4. **Crash evidence.** On API 30+ the app reads `ApplicationExitInfo` at the next start. On API 29 it uses a crash marker written by an async-signal-safe handler. Both go into the diagnostics export and are mentioned in the phone test script.
5. **Lua VM location.** The VM stays in the engine process by default. The residual stuck-native-call case is handled by process-level restart (Section 9.1). Running the VM in a third process is measured at M5, with IPC cost and memory reported, and is adopted only if the numbers allow.

**Microphone capture states** (supports Section 9.3 item 5; written out in full in this file at M6):

| State | Foreground service | AudioRecord | Entered by | Left by |
|---|---|---|---|---|
| Inactive | not running | closed | initial state | user activation in the visible Lumora app |
| Active | running | recording | activation, or resume from Paused | wallpaper hidden, screen off or lock screen covers it (to Paused); service killed or capture refused (to Stopped) |
| Paused | running | not capturing | visibility loss while Active | wallpaper visible again (to Active, if the platform allows resuming; unverified) |
| Stopped | not running | closed | process killed, permission revoked, user stop in the Task Manager, start refused | user-driven activation only; there is never an automatic restart loop |
