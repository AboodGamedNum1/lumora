# Lua scripting and interaction surface (Section 9)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 9. Scripting: Lua 5.4, sandboxed

### 9.1 Execution model

- Each running scene owns one Lua state. Scripts receive lifecycle callbacks: `on_load`, `on_unload`, `on_update(dt)`, `on_event(name, data)`, `on_visibility(visible)`.
- The engine API is exposed as a single `lumora` table: scene graph access, input queries, tweening, particles, audio, physics handles, settings values, time. No `io`, no `os.execute`, no `package.loadlib`, no FFI: the sandbox is an allowlist: each scene environment is built by copying only listed functions into a fresh table (see the allowlist rule below), never by removing dangerous functions from the standard globals.
- Untrusted scenes (from friends, from the internet) must be safe by construction: sandbox + no native loading + integrity verification + a manifest capability declaration (e.g. `uses: [microphone, notifications]`) that the app shows to the user before enabling the scene, enforced at runtime (Section 15).
- **Sandbox hardening, enforced in release builds, not only debug:**
 - Custom Lua allocator with a per-scene memory quota; allocation failure is a script error, not a crash.
 - Instruction-count limits on every untrusted callback in all builds (debug-only watchdogs are not acceptable); a script that repeatedly blows its budget is throttled, then suspended, and the user is told in settings.
 - Caps on script count and size, table growth, coroutine count, event-handler registrations, callback frequency, and re-entrancy depth.
 - No access to JNI, native pointers, arbitrary filesystem paths, or the network; every API handle is validated with lifetime checks.
 - **Bytecode and debug-library closure (the two classic escape/defeat vectors):** binary bytecode loading is disabled - `load` is either removed or exposed only wrapped with `mode="t"` (stock `load` accepts precompiled chunks, and the Lua 5.4 manual itself warns that maliciously crafted binary chunks can crash the interpreter; crafted bytecode is demonstrably escalable to native code execution) - and `string.dump` is removed. The `debug` library is never opened for scene states (`debug.sethook` with no hook would strip the instruction-count hook enforcing the budgets above, and `debug.getinfo`/`getupvalue` break per-scene isolation). Each scene state gets a whitelisted `_ENV`. Bytecode-chunk rejection and hook-tampering attempts are explicit sandbox escape tests in Section 16.1.
 - **Native standard-library calls are not bounded by the instruction hook** (reviewer finding, from the Lua 5.4 manual; not re-verified by running it here): `string.rep`, pattern functions (`find`, `match`, `gmatch`, `gsub`) with backtracking patterns, `table.concat`, `table.sort` and `string.format` each run as one native call. Mitigation: scene states receive wrapped or replaced versions with length caps, a pattern-complexity and match-step limit (or pattern matching removed), a cap on `string.rep` output, and a bounded `table.sort`; a watchdog detects a logic thread stuck in a native call, but a thread cannot be safely killed from outside (Android's bionic does not implement `pthread_cancel`, checked against the bionic status document, and on any platform killing a thread inside a native call can leave locks, allocators and the Lua state inconsistent). Recovery is layered: (1) cooperative cancellation wherever possible (instruction-hook counters, bounded stdlib replacements, bounded engine calls); (2) for the residual case, process-level recovery: the watchdog terminates and restarts the wallpaper engine process and rebuilds the scene from the retained scene state, under the crash-loop breaker of Section 10.7; (3) running the Lua VM in its own process is an option measured at M5 (IPC cost and extra memory on a 4 GB phone are not yet measured), not the default. These cases are added to the escape suite (16.1).
 - **Allowlist, not blocklist:** each scene `_ENV` is built by copying only listed functions into a fresh table. Removed explicitly: `dofile`, `loadfile`, `require`, all of `package`, all of `io`, all of `os`, `print`, `rawset` on shared tables, and `collectgarbage` (replaced by a safe stub). Shared tables and the string metatable are frozen; `__gc` and `__close` are forbidden on engine-visible objects, and a hostile `__index` chain must not re-enter the engine. The coroutine cap is both a count and a per-coroutine time and memory bound; that new coroutines inherit the instruction hook is verified by a coroutine-wrapped-loop test.
 - A defined error policy: one script failure is logged and surfaced; repeated failures suspend the scene's scripts while rendering continues.
 - **Native API call budgets:** a Lua instruction counter cannot interrupt a long-running native C++ call, so every engine API function gets its own contract: bounded iteration (particle spawns, node scans, raycasts have per-call maxima), payload size limits, a per-call time budget after which the call returns a partial result or a `busy` status instead of blocking, asynchronous jobs (loading, baking) that are cancellable at frame boundaries, per-source callback/event frequency limits, and reentrancy rules (an API called from inside an event it can itself raise is rejected with a script error). The full per-function contract table lives in `docs/lua-api.md`.

### 9.2 API sketch

```lua
function on_update(dt)
 local tilt = lumora.input.gyroscope -- vec3
 lumora.node("camera_rig"):set_rotation(tilt * 0.2)
end

function on_event(name, data)
 if name == "screen_unlocked" then lumora.audio.play("swoosh") end
 if name == "touch_tap" then lumora.particles.burst("fireflies", data.x, data.y) end
end

lumora.on("battery_low", function lumora.settings.set("quality", "low") end)
```

### 9.3 Interaction/event surface (the "react to anything" requirement, D13)

Every input below is exposed both as a pollable API and as named events; the author picks per scene.

| Source | Events / API | Permission and availability |
|---|---|---|
| Touch | tap, double-tap, drag, long-press, multi-touch positions, home-screen page-scroll offset | none (requires `setTouchEventsEnabled` / offset-notification opt-ins on the Engine) |
| Motion sensors | gyroscope, accelerometer, magnetometer/compass, linear acceleration | none, but hardware-dependent: every sensor query can return `unavailable`; magnetometer may need calibration/fusion |
| Step counter / detector | step count, step events | **`ACTIVITY_RECOGNITION` runtime permission (API 29+)**, opt-in; absent on devices without the sensor |
| Screen | screen_on, screen_off (dynamically registered `ACTION_SCREEN_ON`/`ACTION_SCREEN_OFF`; these cannot be manifest-registered), wallpaper shown/hidden (`Engine.onVisibilityChanged`) | none |
| Lock | device_locked (keyguard shown: `KeyguardManager.isKeyguardLocked`/`isDeviceLocked` sampled on visibility and screen events), device_unlocked (`ACTION_USER_PRESENT`, a manifest-registrable exempt broadcast, delivered only after the user actually passes the keyguard) | none |
| Time | wall clock, date, sunrise/sunset | sunrise/sunset is **computed locally** from date/time + coordinates: coarse location (opt-in permission), a manually set location, or a timezone-only approximation; no network dependence, coordinates never transmitted |
| Battery | level, charging state, low-battery, power-save mode | none |
| Audio | see the three distinct capabilities below | per capability, below |
| Notifications | count changed; specific-app arrival (filtered, **count/metadata only by default, no notification text to Lua**) | Notification Listener, opt-in, revocable; sensitive notifications excluded |
| Network weather | post-v1 candidate (needs network permission policy) | n/a |
| Phone state | ringer mode, wallpaper offset, orientation | none |

**State semantics:** screen on/off, keyguard shown/hidden, device locked/unlocked, wallpaper surface created/destroyed, and wallpaper visible/invisible are five distinct states and are never conflated. `onVisibilityChanged(true)` is wallpaper visibility, not an unlock event; `device_unlocked` comes only from `ACTION_USER_PRESENT`. The full state x source x API-level x registration x delay/restriction x fallback matrix is specified in `docs/android-lifecycle.md` (M2), and scenes that gate behavior on unlock handle the wallpaper-visible-while-locked case explicitly.

**Audio reactivity is three separate capabilities, not one:**

1. `lumora.audio.lumora_playback_spectrum` - analysis of audio Lumora itself plays. No permission, always available when the scene plays audio.
2. `lumora.audio.microphone_level` / `microphone_spectrum` - environmental sound. **Service contract:** capture never runs inside the WallpaperService. It runs in a dedicated foreground service of type `microphone` (mandatory type declaration plus the `FOREGROUND_SERVICE_MICROPHONE` permission when targeting Android 14+; the plan targets the latest SDK). `RECORD_AUDIO` is a **while-in-use** permission: since Android 12 the OS blocks starting a foreground service from the background, and since Android 14 creating a microphone-type service without an active while-in-use grant throws `SecurityException` immediately. A running WallpaperService does not put the app in the foreground, so **a wallpaper visibility callback is never an authorization to start or restart capture** - `Engine.onVisibilityChanged(true)` is not a valid start trigger on any supported Android version. The activation contract: (1) the scene declares the microphone capability; (2) the user explicitly enables microphone reactivity for that scene in the Lumora app's scene settings or permissions center (a visible Activity) and grants `RECORD_AUDIO`; (3) the service is started from that user-visible Activity flow, while the app is in the foreground and the while-in-use grant is active; (4) the running service owns all capture, and the enabled state is communicated to the wallpaper engine/runtime (bound service or shared state) - the WallpaperService itself never opens the microphone; (5) wallpaper visibility governs only the privacy/power policy of an already-running service. There are two separate states: the foreground service stays running, and only the `AudioRecord` capture is paused when the wallpaper is hidden, the screen turns off, or the lock screen covers it, and resumed on the next visible edge while the service is still running (the resume is not a service start and needs no new activation). Whether resuming capture inside an already-running microphone-type service is allowed on every API level from 29 to 36 is **unverified**: Android's documentation only confirms that such a service cannot be started from the background. M6 tests it on the emulator lanes and in the phone script, and if a level refuses, capture goes to the stopped state of item (6). The transition table is in Section 10.7; (6) if the service is killed, the process dies, permission is revoked, or Android refuses a start, the API returns `temporarily_unavailable` / `permission_denied`, the scene keeps rendering without mic data, and the permissions center explains the state; (7) restart happens only through the valid user-driven activation path again - automatic background restart is never promised unless the exact Android exemption that permits it is documented and tested. A persistent notification plus the system mic indicator make active capture visible, and capture can be stopped immediately from either. The notification needs `POST_NOTIFICATIONS` granted to be visible (Android 13+); it is added to the permissions center, and if denied the service still runs but its notification is hidden (Task Manager only). Android 14+ also gives the user a Task Manager stop button for foreground services, so the runtime has a defined "service killed by user" state that never loops a restart. Rendering never blocks or crashes because capture is unavailable. The full contract, re-verified against the target SDK and the minSdk-29-to-latest range at M6, lives in `docs/android-lifecycle.md`.
3. `lumora.audio.external_playback_spectrum` - other apps' audio. Android's Audio Playback Capture (API 29+) needs a MediaProjection consent prompt, a foreground service (typed on Android 14+), and the source app must not opt out of capture; the Visualizer path (where supported) needs `RECORD_AUDIO`. **Lumora never promises capture of arbitrary app audio.**

Every audio call returns an explicit status: `available`, `permission_denied`, `unsupported`, `source_not_capturable`, or `temporarily_unavailable`. Exact API behavior is re-verified against the target SDK at implementation time (M6).

**Build flavors and permission onboarding.** Two Gradle flavors are built from day one: `play` (audio-reactive limited to `lumora_playback_spectrum`; no microphone service, no Notification Listener, no MediaProjection capture) and `full` (everything above; direct APK and GitHub releases). Rationale: Play requires a declared foreground-service type, a Console declaration and a demo for each, and a microphone service in a wallpaper app, a Notification Listener and MediaProjection each draw separate policy scrutiny (reviewer assessment; open question OQ3). Because the direct APK is sideloaded, Android 13+ shows "Restricted setting" blocks on enabling the Notification Listener until the user allows restricted settings in App info (Google support page cited by the security reviewer, not re-fetched); the permissions center and the phone test script include that step. Capability grants are keyed by package hash plus declared capability set, and a grant is reset when a scene update declares more capabilities.


### 9.4 API additions (proposals, finalized at M5)

- **Logging.** `print` is removed from scene environments, so logging is an explicit API: `lumora.log.debug/info/warn/error`, with per-callback rate and size limits. Output goes to the editor script console and to diagnostics. Release builds do not write to the system log unless the user enables it.
- **Persistent storage.** `lumora.storage` is a per-scene key-value store with a small quota (value pinned at M3). Values are numbers, strings, booleans and plain tables. It is cleared when the scene is removed. Native calls are bounded as in 9.1.
- **Seeded random.** `lumora.random` is a seeded generator, and the standard `math.random` is replaced by it. The editor's deterministic playback (10.4) depends on it.
- **Scene localization.** Scene-level string tables keyed by locale with fallback, read through `lumora.l10n`. Settings already have localization keys; scene text now does too.
- **Stable node handles.** `lumora.scene` returns opaque node handles valid until the node is removed, so scripts do not need per-frame string lookups. Handles use the lifetime checks of 9.1.
- **One source for the API reference.** `docs/lua-api.md` is the single source from which the API reference and the LuaLS type stubs are generated (Section 11.5).

### 9.5 Reduced motion and photosensitivity

- **`lumora.system.reduced_motion`** is a capability that follows the OS "remove animations" setting where the OS exposes one (the exact Android and GNOME settings are to be confirmed at M6 and ML; not verified here). An app-level reduce-motion switch also sets it.
- When it is on, the runtime clamps parallax amplitude from gyro and offset input, caps animation and camera speed, and reduces particle emission. Scenes may read the flag and must not override the clamps.
- **Flash-rate lint.** The editor's pre-export validation report warns when a scene can flash more than three times per second over a large area. This is an advisory lint based on the common three-flashes-per-second guideline (not verified here against the current WCAG text). It does not guarantee safety, and the import screen shows the scene's motion warnings.
