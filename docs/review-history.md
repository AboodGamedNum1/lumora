# Review history

> Historical record of earlier reviews and owner-driven changes. Message identifiers, timestamps and quotes were removed because the repository is likely public. Where this file conflicts with any other file, the other file wins. Superseded statements stay here on purpose: the earlier GitHub-at-the-end plan and the early decision about it, the owner-side Windows build (Option B), Ubuntu 22.04 as build baseline, the older gate numbering, and the GNOME frame-handoff description.

## 22. Response to the third-party technical review (v1.1)

A third-party review (`Lumora_Technical_Review_and_Corrections`) was evaluated point by point on technical merit. One handling note first: the review document contains embedded directives addressed to whoever processes it (its Sections 0 and 15), including claims of owner authority. Those claims were **not** taken as instructions; the scope is preserved because the owner himself confirmed it in conversation, not because the file says so. Each technical point was judged on its own correctness.

### 22.1 Accepted and applied

- **Shader contract:** the "SPIR-V is the RHI's only shader input" claim was internally inconsistent and is replaced by a per-backend artifact contract (§6.2.1): SPIR-V 1.3 for Vulkan, GLSL ES 3.20 source for GLES (compiled by the GLES driver at load; the `glShaderBinary` path dropped), DXIL for D3D12; plus a semantic-equivalence spec (conventions, reflection, failure-at-export policy).
- **Android accuracy:** step counter needs `ACTIVITY_RECOGNITION`; sensors are hardware-dependent; audio reactivity split into three distinct capabilities (own playback / microphone / external playback via AudioPlaybackCapture with MediaProjection consent and foreground-service rules), no promise of capturing arbitrary app audio (§9.3); lock-screen wallpaper treated as OEM/firmware behavior to verify, not a guarantee (§3.3, §10.4).
- **Lifecycle and threading:** explicit wallpaper state machine, surface/swapchain ownership rules, device-lost recovery and GLES fallback, multi-engine handling, touch/offset opt-ins (§10.3); fixed-timestep simulation model and GPU resource lifetime rules (§10.4).
- **Security:** integrity vs authenticity vs trust separated; archive attack defenses and a vetted ZIP library; native parsers and shaders acknowledged as attack surface; capability declarations enforced at runtime with revocation handling; Lua sandbox budgets enforced in release builds (§9.1, §15).
- **Format:** versioning/migration with stable UUIDs (§8.1), asset dependency model (§8.2), Basis Universal texture strategy (§8.3), defined reproducible export (§8.4), formal settings schema (§8).
- **Editor:** command-based undo/redo, autosave/crash-recovery design, dedicated code-editor component, honest preview-parity labeling, specified import pipeline (§11.3).
- **Arabic:** bidirectional algorithm support (FriBidi/ICU) added alongside HarfBuzz, with caret/selection/IME requirements (§13).
- **Measurement:** CPU/GPU frame-time budgets, PSS-based memory budgets, controlled battery methodology, refresh-rate wording corrected (the app paces; the system grants) (§10.1, §16.3).
- **Build:** exact toolchain and dependency pinning with a dependency manifest, third-party notices, signing strategy, diagnostics privacy (§14, §16, M10).
- **Testing:** fuzzing, expanded emulator matrix (min + intermediate + latest target), golden-image policy, benchmark scene suite, per-milestone artifacts, and honest verification labels (agent/emulator/owner-hardware) (§16).
- **Roadmap:** sharper M9/M10 exit criteria, an explicit integration/regression reserve, supporting docs assigned to milestones (§17).

### 22.2 Rejected or qualified

- **Publisher-signature ecosystem for packages (key rotation, publisher identity):** overkill for v1 friend-sharing; the sandbox + capability model carries the threat model instead. Recorded as a post-v1 candidate.
- **Byte-identical reproducible export as a hard guarantee:** kept as a goal; the contract is semantic identity (§8.4).
- **Animation blend trees, root motion, retargeting:** labeled post-v1 explicitly rather than silently implied (§7.1).
- **Gamepad input and broad accessibility suite:** noted as quality considerations, not v1 commitments.
- The review's demand to preserve full scope needed no action: no scope cut was ever proposed, and the v1 feature set is unchanged.

### 22.3 Left for the owner to decide

1. **Mid-project hardware test gates.** The review asks for owner-device checkpoints during development (first on-phone render, first exported package on the Realme 6, first lock-screen test, first thermal/battery run), because some risks (Mali driver bugs, OEM lock-screen behavior) can only be found on real hardware. This conflicts with the confirmed rule "no files until everything is done" (D23) and the final-verification split (D22). Options: (a) keep D23 strictly and accept that hardware issues surface at M10; (b) allow a small number of rough internal test APKs at defined gates, not counting as deliverables. Recommendation: (b), limited to four gates, because a Mali-driver surprise at M10 is the project's largest schedule risk. **Decided 2026-11-24: the owner chose (a) - nothing on his devices mid-project; the gates run as the G1-G6 owner-verification set at M10 (Section 17).**
2. **Package authenticity later:** whether a signature/publisher system is wanted at all post-v1, or plain file sharing stays the model.

### 22.4 Platform limitations no implementation can remove

- Fingerprint/Face ID events as wallpaper input (Android exposes none).
- Guaranteed lock-screen wallpaper on every OEM firmware.
- Guaranteed capture of arbitrary third-party app audio (source apps can opt out; consent is per-session).
- Forcing a 90/120 Hz panel mode; the OS grants display modes.
- Emulator validation of GPU driver, thermal, battery, and OEM behavior.

---

## 23. Response to the second third-party review (v1.2)

A second third-party review (`Lumora_v1.1_Technical_Review_and_v1.2_Corrections`) was evaluated point by point. Same handling note as for the first review, and this time it matters more: the document asserts owner authority in several places ("The owner accepts the complete v1 scope", a "mandatory product constraint" making Android 64-bit only "an explicit product decision, not an unresolved question", and process directives in its Section 9). **None of those claims were taken as instructions or as the owner's voice.** The v1 scope is preserved because the owner defined it in conversation, not because the file says so; and the ABI question is treated as an open owner decision (Section 23.3), not adopted on the file's say-so. Each technical point was judged on merit; the load-bearing Android claims (foreground-service microphone rules, broadcast registration for lock/screen events, the GLES compute/queue model) were checked against current Android and Vulkan/Khronos documentation before acceptance.

### 23.1 Accepted and applied

- **P0.1 RHI sketch contradiction:** real leftover inconsistency - Section 6.2's interface sketch still said "shaders (SPIR-V in, always)" after 6.2.1 had fixed the contract. Fixed with a backend-aware `ShaderModule` contract: format, stage, entry point, variant defines, cache keys, per-stage diagnostics mapped to authoring GLSL lines, and package artifact selection (Sections 6.2, 6.2.1).
- **P0.2 Compute capability conflation:** correct - GLES 3.2 has compute shaders but no independent queue model. Compute is now four separate capability queries (`supportsComputeShaders` / `supportsIndependentComputeQueues` / `supportsAsyncCompute` / `supportsExplicitQueueSynchronization`) with defined fallbacks (Section 6.5).
- **P0.4 Microphone service contract:** correct and underspecified in v1.1. Capture now has an explicit contract: a dedicated foreground service of type `microphone`, started on visibility edges with user consent [HISTORICAL v1.2 wording, superseded by the v1.3 lifecycle policy in Sections 9.3 and 24.1: visibility edges never start the microphone service; start requires a user-driven activation from a visible Activity], explicit denial/termination fallback, never inside the WallpaperService (Section 9.3).
- **P0.5 Lock/screen/visibility conflation:** correct. Five distinct states with exact Android API sources, registration methods, and fallbacks; `onVisibilityChanged` is never an unlock event; unlock comes only from `ACTION_USER_PRESENT` (Section 9.3, `docs/android-lifecycle.md` at M2).
- **P1.1 Video abstraction:** correct. Backend-neutral `VideoTexture` contract: acquire/release, timestamps, per-backend synchronization, AHardwareBuffer import where supported, CPU-upload fallback, YUV metadata handling (Section 7).
- **P1.3 Lua native API budgets:** correct. Instruction counters cannot interrupt native calls; per-function native budgets, bounded iteration, cancellable async jobs, frequency and reentrancy rules added (Section 9.1).
- **P1.4 Evaluation order:** correct gap. Fixed frame pipeline (input - scripts - animation - physics - transforms - render) with property ownership and conflict resolution (Section 10.4).
- **P1.5 Runtime UI scope:** correct gap. Minimal v1 scene-UI set defined (labels, sprites, touch regions, anchored layout); widgets and responsive layout are post-v1 (Section 10.5).
- **P1.6 / P2.11 Prototypes:** accepted. Early RTL-in-real-controls prototype and an editor UX prototype now gate M7 feature build-out (Sections 11.3, 13).
- **P1.7 Archive validation completed:** encrypted entries, CRC, case-folding and normalization collisions, free-space checks, streaming limits, cancellation and cleanup (Section 15).
- **P1.8 GPU workload limits:** compute/texture/pass/pipeline/buffer quotas, compile-time budget, and the hang/device-loss recovery ladder (Section 6.6).
- **P2 refinements:** SDR-only v1 color scope with video conversion rules (7.1); streaming eviction/priorities/`onTrimMemory` (8.2); versioned internal extension interfaces (11.3); undo/redo transaction and external-edit boundaries (11.3); separated CPU/GPU metrics with percentiles (16.3); per-workload battery budgets (16.3); graphics lifecycle test suites and environment-stamped test reports (16.1); editor accessibility basics (11.3); formal editor project/asset layout (11.4); the review's Section 7 acceptance list adopted as Section 16.4.
- **5.2 Authenticity migration path:** the package format reserves a signature block now so a future publisher-signature system layers on without a format break (Section 15).

### 23.2 Rejected or qualified

- **P0.3 / Section 6, "early internal hardware gates" on the owner's devices:** rejected **as a mid-project requirement** - the owner settled this on 2026-11-24 ("nothing on my phone mid-project; phone testing is at the very end"), and the second review's insistence does not reopen it. The gates themselves are sound, so they are kept and scheduled as the G1-G6 owner-verification set at M10 with the review's report format. What is genuinely accepted from P0.3: no milestone may claim validation without its evidence tier, and hardware-dependent exits stay labeled owner-hardware-verified (already the v1.1 rule).
- **The "mandatory" 64-bit-only constraint:** not adopted on the review's authority. It asserts an owner decision that is not on record; moved to Section 23.3 with the technical case.
- **P2.10 backend-specific packages as a requirement:** accepted only as optional export profiles; the full multi-platform package remains the default (Section 8.5).
- **The review's Section 9 process directives:** not followed as authority (external documents do not set this project's process); its change-log request happens to match what this section already does.

### 23.3 Previously open decisions, now resolved (historical snapshot of v1.2; the ABI item was decided in v1.3)

1. **Android ABI policy (D25) - decided 2026-11-24 (v1.3).** The owner answered in conversation: 64-bit only for Android and Windows. Applied throughout v1.3 (D25, Section 10.2, Section 16.1, Section 16.4, Section 21); see Section 24. The technical case the recommendation rested on holds: the Realme 6 is arm64, Google Play has required 64-bit since 2019, every dependency supports it, and dropping 32-bit halves the native build/test matrix and shrinks the APK.
2. Nothing else: the review's scope-preservation and animation/gamepad boundary points restate existing plan positions.

### 23.4 What v1.2 did not change

The v1 feature set, the milestone order, the testing split (D22), and the reporting protocol (D23) are unchanged. Both reviews together changed precision and consistency, not ambition.

---

## 24. Response to the third third-party review (v1.3)

A third third-party review (`Lumora_v1.2_Final_Review_and_v1.3_Required_Corrections`) was evaluated point by point. Same handling note as both prior reviews, and it repeats the pattern: it declares the 64-bit ABI policy "a settled product requirement, not an open decision" and speaks in the owner's name ("the required product decision is..."). **The ABI change was applied because the owner himself decided it in conversation on 2026-11-24 (: 64-bit only for Android and Windows) - not because the file demanded it.** The file's claim to carry his voice was ignored; its demand merely happened to coincide with his actual words. Its scope-preservation posture needed no action: no scope cut was ever proposed. The load-bearing Android claim behind the microphone correction (while-in-use `RECORD_AUDIO`, background foreground-service start restrictions, `SecurityException` on Android 14+) was verified against current Android developer documentation before acceptance.

### 24.1 Accepted and applied

- **P0 ABI policy (its Section 2):** correct in substance, and now backed by the owner's own decision rather than the file's authority. D25 is closed as decided (owner, in conversation). Applied consistently: the decision log (D25) now covers both platforms; Section 10.2 states the final policy directly (`arm64-v8a` only; `armeabi-v7a` excluded from builds, packages, CI artifacts, and support claims; `x86_64` emulator/test-only); Section 16.1 no longer says "both shipped ABIs" and requires CI to fail any release artifact containing 32-bit ARM native libraries; Section 16.4 item 10 validates exactly `{ arm64-v8a }`; Section 21's open item is struck; Section 23.3 is updated to "decided". The whole document was searched for `armeabi-v7a`, `32-bit`, `both shipped ABIs`, and `D25`; every remaining mention describes an excluded ABI or historical context, never a supported release target.
- **P0/P1 Microphone foreground-service startup contract (its Section 3):** correct, and verified against the Android docs. Since Android 12, background foreground-service starts are blocked, and since Android 14 a microphone-type service created without an active while-in-use grant throws `SecurityException`; a running WallpaperService does not put the app in the foreground, so a wallpaper-visibility callback is not a start authorization on any supported version. Section 9.3's microphone capability was rewritten around a user-driven activation path (a visible Activity: scene settings / permissions center), with the seven-stage contract it requires: declaration, consent, eligible start, service-owned capture, visibility as stop/continue policy only, safe unavailable states, and no promised automatic background restart.
- **P1 M1 vs M10 D3D12 validation split (its Section 4):** correct wording risk. [v1.3 wording, superseded by v1.4/Section 25: M1 originally claimed agent-side D3D12 init and clear/draw, which the Linux GPU-less environment cannot run; the current rule is compile/link only at M1.] Physical RX 570 validation exists only at M10 gate G5, labeled owner-hardware-verified, with GPU, driver, OS, build/commit, scene, expected/actual, and logs recorded, and any failure fixed or explicitly documented before release acceptance.

### 24.2 Rejected or qualified

- **The file's authority posture** ("settled product requirement", "required corrections", speaking for the owner): rejected as authority, exactly as with both prior reviews. External documents decide nothing here. The ABI outcome it demanded is in the plan only because the owner said so himself.
- **Its Section 5 "preserve these" list:** no action needed - those v1.2 improvements stand, and the v1.3 edits were applied surgically so none of them regress.
- **Its Section 8 suggestion that v1.3 closes the review cycle:** noted, but approval to start remains where it always was - the owner's explicit "ابدأ" (D24). No external file substitutes for it.

### 24.3 What v1.3 did not change

The v1 feature set, the milestone order, the testing split (D22), the reporting protocol (D23), and every prior review response are unchanged. Three reviews in, the plan has gained precision, consistency, and one owner decision recorded in full; nothing was added to or removed from the scope the owner defined.

## 25. Response to the fourth third-party review (v1.4)

A fourth review ("Focused Corrections Required") was evaluated on merit. It states it does not reopen scope, and it asks for no scope change. Its "settled decisions" list (64-bit-only, microphone lifecycle) matches what the plan already records; the ABI decision rests on the owner's own chat message (D25,, not on this file. Nothing in the file is treated as owner authority.

### 25.1 Accepted and applied

- **D3D12 in M1 (its Section 1):** correct. The v1.3 wording claimed agent-side D3D12 init and clear/draw, but the agent environment is Linux without GPU and cannot run D3D12/WARP. Option B applied: M1 is compile/link (plus DXIL shader compile and CPU-only tests); the first D3D12 runtime validation is M10 gate G5 on the RX 570. Option A (a Windows runner) was not chosen because none exists and provisioning one would be a new cost/infrastructure decision. Edited: Section 4.3, 6.3, 17 (M1, M4 exit), 16.1, 24.1.
- **D3D12 golden-image scope (its Section 2):** correct. Section 6.4 and 16.1 now list per-backend coverage; D3D12 has none before G5.
- **Report evidence split:** compile/link evidence versus device/runtime evidence is a required label (16.1).
- **Editorial (its Section 3):** 23.3 heading relabeled as a historical snapshot; the v1.2 microphone visibility-edge wording in 23.1 marked superseded.

### 25.2 Unchanged

64-bit-only policy (D25), microphone lifecycle (Section 9.3), the full v1 scope, milestone order, D22/D23 and all earlier responses.

---

## 26. Response to the fifth third-party review (v1.5)

A fifth review ("Focused Required Corrections for v1.5") was evaluated on merit. It asks for no scope change. Its list of settled decisions matches what the plan already records; D25 rests on the owner's own chat message, not on this file, and nothing in the file is treated as owner authority.

### 26.1 Accepted and applied
- **Where D3D12 is compiled and linked (its Section 1):** correct. A Linux agent cannot link against the Windows SDK or run DXC's Windows toolchain as validated evidence. Option B selected and stated (Section 4.3): Windows compile/link, DXIL and editor build are owner-side Windows validation, with pinned toolchain and recorded commands. Option A not chosen: no Windows runner exists and provisioning one is a new infrastructure decision. Edited 4.3, 14, 16.1, 16.2, M1.
- **D3D12 selection gating (its Section 2):** correct. Explicit per-backend support state; D3D12 hidden or Experimental until G5 passes (Sections 6.1, 11.1).
- **M0 Windows smoke test (its Section 3):** correct. Assigned to the owner's Windows PC with defined evidence; marked pending, not passed (M0).

### 26.2 Rejected or qualified
Nothing rejected. Qualification: Windows build checks now happen only at the owner's first Windows build (no later than M10), consistent with D23. This delays discovery of Windows-only build errors; the owner may, if he wants, allow an earlier Windows build as an exception. This is optional and not assumed. **[v1.8 note: the owner did allow exactly one early build; it is scheduled at M1/M2 - Section 4.3. The rest of the D23 rule is unchanged.]**

### 26.3 Unchanged
64-bit-only policy (D25), microphone lifecycle (Section 9.3), D3D12 first runtime validation at M10 G5, full v1 scope including the Windows editor and all three backends, milestone order.

---

## 27. Owner change: Linux support (v1.6)

Unlike Sections 22 to 26, this is not a response to a third-party review. It records a scope change the owner made himself in the Telegram chat (owner message): the editor works on Windows and Linux, and the live wallpaper itself also works on Linux. D26 and D27 rest on that message, not on this file.

### 27.1 What changed
- Decision log: D2 amended; D26 (editor on Windows and Linux), D27 (Linux wallpaper runtime), D29 (Ubuntu, from his message); D28 (Linux x86_64 only) added as a **proposal**.
- Scope (3.1, 3.2, 3.3): Linux editor and runtime in scope; Windows wallpaper runtime and macOS stay out; Linux wallpaper compatibility added to the honesty list.
- Environment (4.3, new 4.4): the agent environment is Linux, so Linux builds and software-rendered tests are agent-verified; real desktops are owner-side only.
- Architecture, repo layout (5, 5.1): `desktop-linux/` added; `platform/` gains linux.
- RHI (6.1, 6.3): Vulkan is the only Linux backend.
- New Section 10A: Linux desktop runtime (wallpaper methods, lifecycle, input mapping, packaging, security).
- Editor (11, 11.2): Windows + Linux; Linux desktop device profile and per-target capability warnings.
- Export (8.3, 8.5): BC7 desktop texture path and a Desktop Vulkan profile.
- Build and dependencies (14), testing (16.1, 16.2: third owner script), milestones (17: new ML, new gate G7), risks (18), distribution (19), traceability (20), open items (21).

### 27.2 Unchanged
64-bit-only decision for Android and Windows (D25), the microphone lifecycle (9.3), D3D12 owner-side validation and G5 timing, the v1 feature set on Android, the milestone order M0 to M10, the "ابدأ" start condition (D24), and no mid-project owner gates (D23; G7 runs at M10 like the others).

### 27.3 Decisions the owner still has to make
Each item has a proposal; none is applied as a decision until he answers. All gate only milestone ML (Linux), not M0 to M9.

1. **Linux machine for verification.** Do you have a Linux machine (or dual boot) to run the final Linux test script on? Without a real Linux session, Linux desktop behavior stays "pending owner-side validation" and cannot be reported as fully passed. A VM is possible but does not represent real GPU or compositor behavior.
2. **Ubuntu releases (distribution itself decided, D29).** You chose Ubuntu plus the distribution the agent works on; both are Ubuntu (22.04 LTS here). Proposal: build on 22.04 and test 22.04 plus 24.04 LTS (the current LTS at the time of writing is unverified here and is checked at the Linux spike). Which Ubuntu version do you actually run?
3. **Desktop environments.** Proposal: KDE Plasma and one X11 session as the tested set; GNOME only if the design spike finds a workable method; wlroots compositors best-effort. Which desktop do you use?
4. **Display server.** X11, Wayland, or both? Proposal: both for the editor; wallpaper support per the spike result.
5. **Architecture (D28).** Linux x86_64 only, no Linux arm64? (Same spirit as D25.)
6. **Packaging.** Proposal: x86_64 tarball or AppImage first; `.deb`/`.rpm`/Flatpak later. Acceptable?
7. **Multi-monitor.** Proposal: one scene per monitor selectable, plus a span mode later. Which do you want first?
8. **Windows wallpaper.** You asked for Linux only. Confirm that the Windows desktop wallpaper stays out of v1.

Non-decision to know: phone sensors (gyro, accelerometer) and notifications do not exist on a Linux desktop wallpaper; scenes using them degrade or are warned in the editor (10A.4).

---

## 28. Owner answers and library permission (v1.7)

Like Section 27, this is not a third-party review response. It records the owner's own message. D28 and D30 rest on that message, not on this file.

### 28.1 What was answered (Section 27.3 items)

| 27.3 item | Status in v1.7 |
|---|---|
| 1. Linux machine | **Decided:** real Linux dual boot, plus WSL. Dual boot is the validation target; WSL is build/CPU only (Section 4.4). |
| 2. Ubuntu release | **Decided as "the newest Ubuntu release"**; the exact number is unknown to the owner and is read with `lsb_release -a` at the first Linux test and recorded in the test result. The plan states no version number as fact. |
| 3. Desktop environment | **Decided (D29, v1.7): the default Ubuntu desktop, GNOME.** The agent proposed this assumption and the owner confirmed it (owner message). GNOME is the hardest desktop for a wallpaper (10A.2), so the Linux spike treats GNOME as the first and primary case. KDE and others stay best-effort. |
| 4. Display server | **Not answered.** Read at the test with `echo $XDG_SESSION_TYPE` (x11 or wayland). The spike covers both; GNOME on Wayland is the expected default and the risky case. |
| 5. Architecture | **Decided (D28):** x86_64 only. |
| 6. Packaging | **Decided (D28):** AppImage and `.deb`. |
| 7. Multi-monitor | Not answered; proposal unchanged (one scene per monitor selectable, span mode later). |
| 8. Windows wallpaper | Not answered; the plan keeps it out of v1 as he originally scoped (Linux only). |

### 28.2 What the owner checks at the first Linux test (no questions needed now)
`lsb_release -a` (Ubuntu release), `echo $XDG_CURRENT_DESKTOP` (confirms GNOME), `echo $XDG_SESSION_TYPE` (X11 or Wayland), `vulkaninfo --summary` (GPU driver). The prepared Linux test script (16.2) records these first, so the answers to the questions above come from the machine, not from memory.

### 28.3 Changes
- Header, D28 (now decided), D29 (newest Ubuntu, GNOME desktop), new D30 (library reuse).
- 4.4: dual boot as the validation target, WSL limits.
- 10A.5: packaging decided.
- 14.1 (new): library-first policy with criteria and candidate table. 14 and 21 reference it.
- 27.3 is historical; items 1, 2 (partly), 5, 6 are closed here.

### 28.4 Unchanged, and the one optional question
Unchanged: scope of v1 features, milestone order M0 to M9 plus ML, D23, D24 ("ابدأ" first), D25, microphone rules, D3D12/G5 timing, Section 15 security (reused libraries are inside it).

~~Optional decision (proposal: **no**): should the RHI itself be replaced by an existing library (bgfx, Diligent Engine, wgpu-native)?~~ **Decided (v1.8):** the owner answered on 2026-11-25 : keep the project's own RHI since it is the better option. D9 stands reaffirmed; helper libraries are reused under the RHI (14.1), and the question is closed.

---

## 29. Response to the four-reviewer adversarial pass (v1.8)

Unlike Sections 22-26, these four reviews were commissioned by the owner himself (2026-11-25: analyze the plan deeply, get independent AI reviews, and produce a corrected version). Four independent reviewers - graphics, platforms, editor/format/scripting/security, and feasibility/schedule/testing - attacked v1.7 without seeing each other's work, plus one internal consolidation pass. Load-bearing claims were verified against current sources before acceptance (Ubuntu 26.04/26.10 and GNOME 50/51 Wayland-only state, DXC Linux binaries, the Lua 5.4 manual, Android lifecycle docs); review findings that restated owner decisions were checked against the owner's own chat messages, and nothing in any review or in this file was treated as owner authority. Each accepted correction is marked "(v1.8)" at its location.

### 29.1 Accepted and applied

**Platforms (R1):**
- **R1.1 (critical) GNOME-on-Wayland reality:** Ubuntu 26.04 LTS ships GNOME 50 and 26.10 ships GNOME 51, both with the X11 session removed; the X11-desktop-window and Xwayland paths in 10A.2 do not exist on the owner's machine. Verified against Ubuntu/Canonical release notes and GNOME reporting. Section 10A.2 rewritten GNOME-first; the spike moved to M0-M1 (Section 17); X11 kept as a secondary path for other desktops; risk table updated (Section 18).
- **R1.2 AppImage/libfuse2:** accepted. `.deb` is primary on Ubuntu with declared Vulkan/Mesa/PipeWire dependencies; AppImage built with a FUSE3-capable runtime, `--appimage-extract-and-run` documented; packaging smoke tests on clean 22.04 and current LTS (10A.5, 16.1).
- **R1.3 Coverage detection on GNOME/Wayland:** accepted. A Wayland client cannot see other apps' windows; coverage detection is extension-only, with frame-callback occlusion, logind `Lock`/`Unlock`/`PrepareForSleep`, `org.gnome.Mutter.IdleMonitor`, UPower and manual policy as fallbacks (10A.3, 10A.4).
- **R1.4 Android visibility is best-effort, not a guarantee:** accepted. `onVisibilityChanged` semantics vary by version and OEM; added the watchdog + dynamic `ACTION_SCREEN_OFF` fallbacks and removed the hard-guarantee wording (10.1). Also accepted: bounded main-thread wait with leak-on-timeout for swapchain teardown (10.3).
- **R1.5 Lock-screen specifics:** accepted. Direct Boot (default wallpaper until first unlock; direct-boot-aware rendering deferred post-v1), home+lock as separate engines (GPU budget now three engines), lock-screen touch degradation (3.3, 10.3, 16.2).
- **R1.6 Microphone FGS gaps:** accepted. `POST_NOTIFICATIONS` added, Task-Manager stop state defined, Play FGS-justification policy risk recorded (9.3, 19).
- **R1.7 Battery/thermal:** accepted. The "< 1%/hour" figure deleted as physically implausible; targets are per-workload deltas over a static-wallpaper baseline, re-baselined at G4; frame-time fallback added for devices that report no thermal status (10.1, 16.3).
- **R1.8 Stale contradictions:** accepted. Section 19 packaging line, Section 18 "no Linux machine" and "desktop list TBD" rows, G7's stale condition, the 16.2 header condition, the "two scripts" references, and the 10.1-vs-9.3 AOD contradiction all fixed in place.
- **R1.9 Windows/Play packaging:** accepted. Windows zip-first packaging with SmartScreen documentation; AAB path and Play policy items recorded (19).

**Graphics (R2):**
- **R2.1 Timeline semaphores vs Vulkan 1.1 baseline:** accepted. Resource lifetime rests on binary fences + per-frame-slot counters; timeline semaphores are an optional capability (6.2, 10.4).
- **R2.2 Video path on Vulkan:** accepted. `SurfaceTexture` is GL-only; the Vulkan path is MediaCodec → `AImageReader` → `AHardwareBuffer` → `VK_ANDROID_external_memory_android_hardware_buffer` with an immutable YCbCr conversion sampler in the binding layout; GLES uses the same `AImageReader` via EGLImage; an external-sampler shader construct added to the contract (6.2, 6.2.1, 7).
- **R2.3 DXIL signing on Linux:** accepted. Official DXC releases ship Linux `dxc`/`libdxcompiler.so`/`libdxil.so` (verified on the DXC releases page); the manifest pins a Linux DXC build and cache keys include the validator version (6.2.1, 14).
- **R2.4 Render-pass/attachment ops:** accepted. Load/store ops and transient attachments are first-class RHI concepts mapped to GLES `glClear`/`glInvalidateFramebuffer` and D3D12 `BeginRenderPass` (6.2).
- **R2.5 Pre-rotation:** accepted. `currentTransform` handled with rotation folded into the camera matrix (10.4).
- **R2.6 GLES semantic fixups:** accepted. GLES-safe authoring subset pinned: no reverse-Z, z-range and Y-flip fixups, combined image/samplers, queried SSBO stage limits, no subgroups; corpus-tested (6.2.1, 16.1).
- **R2.7 Mali-G76 budgets:** accepted. Per-tier pass budget (low tier: no shadow, no bloom, 1-2 lights, render scale below 1.0 default for 3D, R11G11B10F/LDR intermediates, one folded final pass, MSAA-in-tile over FXAA where available) (7).
- **R2.8 sRGB swapchain:** accepted. Queried capability with a manual-encode fallback and golden tests (7.1).
- **R2.9 Frame pacing:** accepted. 90 fps opportunistic, pacing from measured Choreographer vsync, 3 swapchain images / 2 in flight, `setFrameRate` on API 30+, Swappy evaluated at M2, 120 fps marked unverifiable on owned hardware (10.4, 16.3).
- **R2.10 Golden-image caveats:** accepted. Renderer versions pinned, small tolerance even on the reference path, mediump-emulation test added, ANGLE-on-SwiftShader labeled as translation testing (16.1).
- **R2.11 GPU timing:** accepted. Vulkan timestamp queries primary; GLES GPU time reported unavailable with proxies (16.1).
- **R2.12 GLES program cache:** accepted. `glGetProgramBinary` cache keyed by renderer + driver version; first-run link counts in the cold-load budget (10.4).
- **R2.13 KTX2 transcode cost:** accepted. Measured at M4 against the cold-load budget; pre-transcoded ASTC per-scene export profile as the escape hatch (8.3).
- **R2.15 SPIRV-Cross HLSL weak points:** accepted. The shader corpus deliberately covers compute and every contract resource type; translation failure blocks export (16.1).
- **R2.16 CPU particle default on G76:** accepted (7).

**Editor, format, scripting, security (R3):**
- **R3.1 Lua sandbox bytecode/debug vectors:** accepted (high). Bytecode loading disabled (`load` removed or wrapped `mode="t"`), `string.dump` removed, `debug` library never opened for scene states, per-scene `_ENV` whitelist, explicit escape tests (9.1, 16.1). Verified against the Lua 5.4 manual.
- **R3.2 Linux editor cannot produce the default export:** accepted (high). The DXC-is-Windows-only premise was stale; Linux DXC pinned (6.2.1, 8, 14).
- **R3.3 OGG/Opus decoder missing:** accepted. libvorbis + libopus added to the manifest behind the audio interface (12, 14).
- **R3.4 Arabic font and code-editor bidi:** accepted. Bundled OFL Arabic font with custom glyph ranges; maintained ImGuiColorTextEdit fork as evaluation baseline; code-editor bidi added to the RTL prototype checklist (11.3, 13, 14).
- **R3.5 SDL3-vs-GLFW:** accepted. SDL3 default, GLFW fallback, pinned at M0 (11.2, 14.1, 17).
- **R3.6 Binder/generated-code round-trip:** accepted. Generated-block ownership rule with read-only-on-hand-edit (11.2).
- **R3.7 Integrity wording overclaim:** accepted. Checksums detect accidental corruption only; covered set defined (15).
- **R3.8 Convex decomposition library:** accepted. VHACD added as an import-time tool dependency (12, 14).
- **R3.9 Scene cardinality:** accepted. One scene per package in v1, stated explicitly (8).

**Feasibility, schedule, testing (R4):**
- **R4.1 Spike scheduled last:** accepted (high). GNOME spike at M0-M1; extension companion pre-committed as the primary design subject to the owner's approval (29.3); risk table updated (17, 10A.2, 18).
- **R4.2 No schedule:** accepted. Rough per-milestone effort estimates, M4 and M7 split into internal gates, defined cut order and re-estimate rule (17.1).
- **R4.3 M10 compression:** accepted. Gates parallelized, per-gate fix-retest reserve, diagnostics tooling shipped at M9 (17, 17.1).
- **R4.4 Early-Windows-build contradiction:** accepted. The owner's one-build approval (2026-11-24) is recorded as the explicit D23 exception at M1/M2; 4.3 and 26.2 reconciled.
- **R4.7 Stale cross-references:** accepted and fixed (16.2, 17 header, M10, 19).
- **R4.8 SDL3 pinned late / editor display-server question parked:** accepted. SDL3 pinned at M0; the editor targets both X11 and Wayland as decided (17, 11.2).
- **R4.9 Jolt requires C++17, not C++20+:** accepted (12).
- **R4.10 Emulator/KVM never verified:** accepted. M0 exit boots the API-29 emulator once with a recorded fallback (17).

**Internal consolidation pass (O):**
- **O1:** Section 28.4's RHI question was still open in the document although the owner had already answered it in chat; closed (28.4, 21).
- **O2:** The GNOME Shell extension is JavaScript inside gnome-shell - an explicit exception to the shared-C++-core story and a new attack surface; constrained to a thin, versioned-IPC companion that never parses scene data (10A.6).
- **O3:** The phone script did not verify the home+lock engine pair that the three-engine budget assumes; added (16.2).
- **O4:** RADV-on-Polaris capability confirmation and a Mesa version floor for the Linux runtime are recorded as ML-time items in `docs/rhi-spec.md`/manifest wording (carried into 10A/ML work).

### 29.2 Rejected or qualified

- **R2.14 (GLES-via-EGL fallback on Linux):** qualified. Linux stays Vulkan-only (6.1); machines without usable Vulkan get the static-image fallback already specified. Adding a third Linux backend path for old Intel/NVIDIA corner cases is not worth the test matrix in v1; noted as a post-v1 option.
- **R1.5(a) (direct-boot-aware wallpaper):** qualified. Accepted as a documented limitation for v1 (system default wallpaper shows until first unlock); full direct-boot-aware rendering with device-protected storage is a post-v1 candidate, not v1 scope.
- **Mutter layer-shell as the GNOME mechanism:** not adopted as the plan of record. Its presence is real but experimental and unconfirmed in official GNOME documentation; it is the spike's fallback check, never a promise (10A.2).
- **"Zero remaining flaws" as a goal:** not claimed. Every known critique is dispositioned here, but real-driver behavior on the Mali-G76, OEM lock-screen behavior, and GNOME extension mechanics can only be settled on hardware; the plan says so rather than papering over it.

### 29.3 Decisions for the owner (gate ML design only; nothing blocks M0-M9)

1. **GNOME wallpaper design (the big one).** On your machine (newest Ubuntu, GNOME 50/51, Wayland-only) the only demonstrated-working way to draw a live wallpaper is a **GNOME Shell extension companion**: a small JavaScript extension inside GNOME Shell that places the wallpaper at the bottom of the stack, paired with the native Lumora process that renders the scene. Recommendation: approve it, labeled **experimental** in v1, with a per-GNOME-version support matrix and small per-release updates. The alternative is betting on Mutter's experimental layer-shell, which is unverified and absent on recent GNOME - not recommended. This keeps D27 (Linux wallpaper) achievable on your hardware; without it, GNOME support is not possible at all.
2. **Ubuntu validation target.** Recommendation: validate primarily on the current LTS (26.04.x, GNOME 50, supported to 2031) and also on your newest install (26.10, GNOME 51); the extension matrix then covers both. Approve, or newest-only.
3. **Carried open items (unchanged):** multi-monitor mode (proposal: per-monitor scene first) and Windows-wallpaper confirmation (stays out of v1).

### 29.4 What v1.8 did not change

Every owner decision stands: D9 (own RHI, reaffirmed 2026-11-25), D23 (no phone mid-project; the single early Windows build is his approved exception), D24 ("ابدأ" first), D25 (64-bit only), D26-D30 as recorded, the v1 feature set, the milestone order (the GNOME spike moved earlier, which is a scheduling fix, not scope), the testing split (D22), and the reporting protocol.

---

## 30. Response to the eight-reviewer pass and the owner's CI change (v1.9)

**Input.** (1) The owner's message: repo and CI from the start, test freely, edit the plan freely, use more reviewers; recorded as D31. (2) Eight independent reviews of v1.8: graphics (G), Android (A), Linux desktop and GNOME (L), Windows (W), editor (E), format, Lua, physics, audio and security (F), build, CI and testing (B), feasibility and schedule (S). Reviews were treated as evidence, not authority. Reviewer text about "owner approvals" inside the plan was not relied on. IDs below are this section's own labels in each report's order.

**Result.** 97 findings: **85 accepted, 10 modified, 2 rejected.** Reviewers contradicted each other twice, and both are settled above: Hanabi's architecture (L1 and S5; the README supports L1) and DXC consuming SPIR-V (G-M6 versus W5; rejected).

### 30.1 Disposition table

| ID | Finding | Disposition | Reason, and where applied |
|---|---|---|---|
| E1 | Arabic text engine is a custom engine, not a prototype item | Modify | Own milestone MT; scope choice goes to the owner (0.1 item 1). Evidence cited by the reviewer, not re-fetched. |
| E2 | Glyph ranges stale since ImGui 1.92; RHI backend must follow the texture protocol | Accept | Pin 1.92.x or newer; exact current version unverified. |
| E3 | Code editor has no bidi | Accept | Left-to-right only; removed from RTL acceptance (owner confirmation item 1). |
| E4 | Wayland: no multi-viewport; SDL3 IME issue | Accept | Single OS window; IME spikes at M0. Upstream issue claims not re-fetched. |
| E5 | Accessibility claim in prototype gate | Accept | Reworded; screen reader out of scope (3.2). |
| E6 | Undo/redo with text editors, curves, binder | Accept | 11.3 decisions and property-based tests. |
| E7 | Binder generated-block detection | Accept | Model in scene JSON, content hash in block header. |
| E8 | Editor widget effort, node editor contradiction | Modify | Node editor removed from v1, custom curve widget budgeted; reviewer's 14-22 week figure not adopted wholesale, M7 split with MT (17.1). |
| E9 | Headless screenshots, IME not CI-testable | Accept | Offscreen RHI target and imgui_test_engine; IME owner-side. |
| E10 | 10k-node hierarchy needs a clipper | Accept | 11.3 budget under 4 ms. |
| E11 | Dock layout mirroring mechanism | Accept | Two generated layouts. |
| E12 | Docking branch is a moving pin | Accept | Pinned commit or tag. |
| E13 | CI forward, Windows and Linux editor builds in CI | Accept | 14.2, M0. |
| E14 | Risk row for Arabic should be High | Accept | Section 18. |
| A1 | Home/lock two engines API 24+ is wrong for the reference phone | Modify | Verified: getWallpaperFlags added in API 34. Per-screen scenes API 34+ only. The claim about API 29-33 engine behavior is unverified, so it is measured at G3 and asked as 0.1 item 2. |
| A2 | 16 KB page-size requirement missing | Accept | 10.6; Play doc not re-fetched. |
| A3 | Play target SDK and test lanes, AAB and signing early | Accept | 10.6, 14.2; the API 36 deadline date is reviewer-cited, unverified. |
| A4 | Microphone FGS, Notification Listener, MediaProjection Play risk | Modify | Flavor split rather than moving features out of v1 (0.1 item 3). |
| A5 | Direct Boot completeness | Accept | 10.6. |
| A6 | USER_PRESENT manifest receiver cost, no-lock case | Accept | 10.6. |
| A7 | Android 16 visibility statement unsourced | Accept | Marked unverified; one other reviewer cites AOSP source, conflicting, so no Android 16 fact is stated. |
| A8 | Thermal listener, headroom, AOD, OEM kill, CI cannot test battery | Accept | 10.6, 16.3, risks. |
| A9 | Manifest and wallpaper metadata items | Accept | 10.6. |
| A10 | Teardown: leaked swapchain can break next surface | Accept | 10.3. |
| W1 | No-Windows-runner design obsolete | Accept | 4.3, 6.4, 14, 16.1, 17. Runner labels verified. |
| W2 | Runner label drift, MSVC pin, C++26 on MSVC | Accept | Explicit pin; MSVC 14.52 GA claim unverified. |
| W3 | Signing and SmartScreen plan thin | Modify | Facts reviewer-cited, unverified; route is the owner's choice (0.1 item 5). |
| W4 | DXC/DXIL wording and version | Modify | Verified: v1.8.2505 internal validator, v1.9.2609 released 29 Sep 2026. Linux packaging fix claim unverified. |
| W5 | Shader path remap rules, WARP test of corpus | Accept | 6.2.1. |
| W6 | Windows 10 cannot be tested in CI | Accept | Proposal, 0.1 item 8. |
| W7 | Packaging details: CRT, Vulkan loader, miniaudio, attestations | Accept | 19, 12. |
| W8 | Process: secrets policy, branch protection, repo visibility | Accept | 14.2. |
| L1 | GNOME mechanism described wrongly (frame handoff) | Accept | Verified at README level: separate GTK4 renderer app id. Window-actor placement detail unverified until the spike reads source. Conflicts with the feasibility reviewer's in-shell claim, which is not adopted. |
| L2 | Extension distribution and review not planned | Accept | 10A.2; EGO guidelines reviewer-cited, not re-fetched; route is 0.1 item 7. |
| L3 | Maintenance claim too pessimistic | Modify | Softened wording; the GNOME 50/51 porting-guide claims are not re-fetched. |
| L4 | 22.04 baseline vs 26.04/26.10 target | Accept | 10A.5, D29 note. 22.04 EOL date is the standard term, unconfirmed. |
| L5 | Headless verification overstates GNOME coverage | Accept | Nested gnome-shell CI job; feasibility and 26.04 image availability unverified (an Ubuntu 26.04 runner label is listed in runner-images, but nested gnome-shell was not tested). |
| L6 | Mutter layer-shell claim | Accept | Dropped as a fallback. |
| L7 | Coverage and power policy under-specified | Accept | 10A.3. |
| L8 | KDE under-planned | Accept | Post-v1, plugin route. |
| L9 | AppImage packaging gaps | Accept | 10A.5. |
| L10 | Autostart and session lifecycle: who starts whom | Accept | 10A.2. |
| L11 | Pointer input on hidden toplevel | Accept | 10A.4. |
| L12 | Multi-monitor placement depends on the extension | Accept | 10A.4. |
| L13 | CI matrix for Linux | Accept | 14.2. |
| F1 | Instruction hook does not bound native stdlib calls | Accept | 9.1; from the Lua manual, not run here. |
| F2 | Lua global allowlist incomplete | Accept | 9.1. |
| F3 | Opus needs libogg and opusfile | Accept | 12, 14. |
| F4 | stb_vorbis unsafe fallback | Accept | Dropped. CVE data reviewer-cited from NVD, not re-fetched. |
| F5 | Checksum and signature-block semantics | Accept | 15. |
| F6 | ZIP validation gaps | Accept | 15. |
| F7 | Notification Listener restricted settings on sideload | Accept | 9.3, phone script. |
| F8 | Physics input sanitizing | Accept | 12. |
| F9 | Physics determinism statement outdated | Modify | Option documented; decision at M8 from measured cost. |
| F10 | Reproducible export byte identity across hosts | Accept | Hash of inputs and tool versions on the same image. |
| F11 | Shared-scene threat model: GPU time cap, JSON limits | Accept | 15. |
| F12 | Video in packages limits | Accept | 15. |
| F13 | Capability grants keyed to package | Accept | 9.3. |
| F14 | Coroutine cap and hook inheritance | Accept | 9.1. |
| F15 | Import path off UI thread | Accept | 15. |
| B1 | No Windows runner decision obsolete | Accept | Same as W1. |
| B2 | No GPU on runners, WARP and software Vulkan | Accept | Spike at M1; unverified until then. |
| B3 | Ubuntu 22.04 runner retirement | Accept | 14.2; dates reviewer-cited, unverified. |
| B4 | Golden images flaky: metric, storage, bless workflow | Accept | 14.2. |
| B5 | Android emulator on CI under-specified | Accept | 14.2, 16.1. |
| B6 | Repo visibility | Accept | Open open question OQ4. |
| B7 | Caching budget | Accept | 14.2. |
| B8 | Secrets and supply chain | Accept | 14.2. |
| B9 | Fuzzing CI plan | Accept | 16.1. |
| B10 | D23 and CI artifacts, one entry point | Accept | Open open question OQ6. |
| B11 | Reproducible export untested | Accept | Merged with F10. |
| B12 | C++26 matrix gating | Accept | 14. |
| B13 | Packaging smoke tests in containers | Accept | 16.1. |
| B14 | Dependency auditing automated | Accept | 14.2. |
| B15 | Flake and failure policy | Accept | 16.1. |
| B16 | CI cannot measure performance budgets | Accept | 16.3. |
| B17 | Windows ARM and Linux ARM runners | Reject | Optional extra coverage; not needed for v1 (mentioned in 14.2 only). |
| G-H1 | D3D12 validation strategy stale | Accept | Same as W1, B2. |
| G-H2 | CI and repo references stale | Accept | All fixed together. |
| G-M3 | D3D12 root-signature budget | Accept | 6.3. |
| G-M4 | Explicit locations on stage IO | Accept | 6.2.1. |
| G-M5 | EXT_color_buffer_float for R11G11B10F | Accept | 6.5, 7. |
| G-M6 | DXC can ingest SPIR-V directly | Reject | As far as I know, `dxc -spirv` emits SPIR-V from HLSL and does not consume it (a second reviewer says the same). A direct SPIR-V to DXIL route (for example Mesa spirv_to_dxil) is an unverified candidate to evaluate at M1. |
| G-L7 | Swapchain interval control does not exist on Android | Accept | 6.2. |
| G-L8 | GLES video via SurfaceTexture | Accept | 7. |
| G-L9 | Subpass claim not backed by interface | Accept | 6.2, 6.3. |
| G-L10 | Probe EXT_multisampled_render_to_texture | Accept | 6.5, phone script. |
| S1 | CI invalidates Windows/D3D12 architecture | Accept | Same as W1. |
| S2 | Stale cross-references from the same fact | Accept | D21, 4.3, 14, 16.1, 19, 20, 21, 26 (banner), M0, M10 all fixed; grep check below. |
| S3 | M0 exit needs the RHI | Accept | M0 exit rewritten; triangle moves to M1. |
| S4 | M7-full before M8, M6 audio before M8 | Accept | M8 before M7b; M6 non-audio. |
| S5 | GNOME mechanism unproven and spike cannot answer it agent-side | Modify | Labeled unverified; fallback scope defined; early owner Linux session is a question, not assumed. |
| S6 | Effort total undercounts | Accept | 17.1 rebuilt, serial. |
| S7 | M10 overloaded, owner round trips | Modify | M10 6-10 weeks; owner-downloaded smoke builds are a question (0.1 item 6). |
| S8 | Memory budget with three engines | Accept | 16.3. |
| S9 | Risk table gaps | Accept | Section 18. |
| S10 | Other stale cross-references (Windows-only traceability, SDL3 pin timing, G1-G6, 22.04 smoke test) | Accept | Fixed or bannered. |

### 30.2 Claims checked in v1.9, and claims left unverified

**Checked (source, date 2026-11-25):**
- `WallpaperService.Engine.getWallpaperFlags` and `onWallpaperFlagsChanged` are listed as added in the API 34 diff (developer.android.com/sdk/api_diff/34/changes/android.service.wallpaper.WallpaperService.Engine). `WallpaperManager.FLAG_LOCK` is documented as the lock-screen-specific wallpaper flag (developer.android.com/reference/android/app/WallpaperManager); the docs say nothing about live-wallpaper engine behavior on API 29-33.
- Hanabi (github.com/jeffshee/gnome-ext-hanabi, README): main branch is GNOME 50+ Wayland only; per-version branches; GTK4 media backend; a renderer app id `io.github.jeffshee.HanabiRenderer`; GPL-3.0.
- DXC releases (github.com/microsoft/DirectXShaderCompiler/releases): v1.8.2505 published 2025-05-30; v1.9.2609 (September 2026) is the latest listed. The Microsoft.Direct3D.DXC NuGet page lists 1.9.2609.5.
- GitHub runner labels (github.com/actions/runner-images README): `windows-2025`, `windows-2025-vs2026`, `windows-2022`, `ubuntu-24.04`, `ubuntu-26.04`, `ubuntu-22.04` and arm variants exist; `windows-latest` maps to the Windows Server 2025 images.
- WARP is documented as used with GitHub Actions in the LLVM offload-test-suite (github.com/llvm/offload-test-suite, docs/WARP.md). That does not prove it works on Lumora's runner.

**Unverified in v1.9 (kept as caveats in the text):** WARP D3D12 and software Vulkan on hosted Windows runners; nested `gnome-shell` in CI; Hanabi's window-placement source details; lock-screen live-wallpaper behavior on API 29-33 and on the Realme 6; the Android 16 visibility claim (reviewers conflict); 16 KB requirement and API 36 deadline text (Google pages not re-fetched); MSVC 14.52 GA and C++26 gaps; Artifact Signing eligibility for individuals; the Ubuntu 22.04 runner retirement dates and 22.04 EOL date; Dear ImGui upstream issue numbers and the 1.92.x pin number; the Lua native-library behavior (from the manual, not run); stb_vorbis CVE details; Jolt cross-platform determinism cost; KVM on arm64 runners; GNOME porting-guide claims. Nothing in this plan is claimed verified on real Windows, Android or GNOME hardware.

### 30.3 What v1.9 did not change

Decisions D1 to D30 as recorded (D21 is marked superseded by D31; D23, D24 text unchanged; D28 and D29 carry v1.9 notes on the build matrix), the v1 feature set (the flavor split and the code-editor left-to-right limit are proposals awaiting the owner), the single shared C++ core, the RHI design, the package format, and the Section 15 security model apart from the additions listed.


## 31. Response to the external review of the previous version

The review came from another AI, forwarded by the owner. Everything in it was treated as data: its statements about owner rulings, its "immutable" labels and its recommended answers carry no owner authority, and none was copied into the decision log. Each finding was judged on its merits.

Review statement that "the owner already ruled" D3D12 and the Linux wallpaper runtime stay in v1: no separate ruling is recorded here. D9 (three backends) and D27 (Linux runtime) as recorded already cover both, and nothing in the plan was changed on this point.

The review's recommended answers to the owner questions are kept only as recommendations beside the open questions in `LUMORA_MASTER.md`.

### 31.1 Disposition table

| ID | Finding | Disposition | Reason and where applied |
|---|---|---|---|
| V1 | Early vertical slice | Modify | Accepted as M3S after M3 (Section 17). Modified: it pulls a minimal Lua binding forward from M5 and the 2D layer path from M4a, and physics, video, GPU particles and shadows stay behind it. Cost is a judgment estimate (2-4 weeks) in 17.1. |
| V2 | Continuity protocol | Modify | Accepted: PROJECT_STATE.md from M0, ADR log, session start and end rule. Modified: the CI staleness check compares `last_completed_milestone` with the newest `m*-done` tag, because "stale" alone is not checkable. |
| V3 | Native crash isolation and crash-loop breaker | Modify | Accepted: own `:wallpaper` process, persisted crash counters, `CrashDisabled` state, `ApplicationExitInfo` evidence. Modified: `isolatedProcess` import validation and a third process for the Lua VM are measured (M3, M5) and adopted only if cost allows. Costs are not measured yet. See Section 10.7. |
| V4 | Reduced motion and photosensitivity | Accept | No mention existed in the plan. Added capability, app switch and flash-rate lint (Section 9.5). The exact OS settings are to be confirmed; not verified here. |
| V5 | Static shader limits against GPU hangs | Modify | Accepted: static limits at export and load, import warning, lowest first tier, adversarial shaders in fuzz. Modified: limits can be checked on SPIR-V, but the GLSL ES and HLSL forms in a package cannot be proven equal to it. Recorded as open technical question TQ1 (Section 15.1), decided at M3 because it touches D8. |
| V6 | Capability tiers | Accept | GLES 3.2 does define the authoring language. Section 6.7 states it and defines T0 and T1. |
| V7 | Author tooling and documentation | Accept | Section 11.5, due by end of M7a, with exit criteria. Logging API defined because `print` is removed (9.4). |
| V8 | Lua API gaps | Accept | Storage, seeded random, scene localization, node handles and logging added as proposals in 9.4. Quota values pinned later. |
| V9 | Undefined lists and limits | Accept | Numbers are pinned at M3 (Section 8.6). The glTF extension candidate list is from general knowledge and is marked unverified. |
| V10 | Bidi library | Accept | SheenBidi chosen, ICU kept as fallback, "or FriBidi" removed everywhere. SheenBidi is Apache-2.0 (checked on its repository license page). FriBidi being LGPL-2.1 is the reviewer's claim and was not re-fetched. |
| N1 | Document bigger and contradictory | Accept | Measured and confirmed: 231,504 bytes and 224 inline version tags. The master is split into a short master plus normative files, history is separate, and every listed stale statement was fixed or removed. |
| N2 | Killing the logic thread is not implementable | Modify | Accepted. Verified from the bionic status document that `pthread_cancel` is not implemented and is unlikely ever to be. Replaced by cooperative cancellation plus process-level recovery (Section 9.1). A separate VM process is measured at M5, not made the default. |
| N3 | Microphone stop versus pause | Modify | Accepted: two states, with the foreground service kept running and capture paused. Android documentation confirms that a microphone-type service cannot be started from the background. Resume of capture inside a running service is NOT verified on API 29-36 and is a tested assumption (Section 9.3 item 5, table in 10.7). |
| N4 | Public-repo hygiene and token handling | Modify | Accepted: message identifiers, timestamps and quotes stripped, secret scanning and push protection, token revoked at M10. Modified: a session that cannot get the token through the vault route must stop and ask for a vault link, never accept it in plain chat. The statement that the owner's token has no expiry is the reviewer's claim, unverified, and is not recorded. No token was handled in this work. |
| N5 | CI-only feedback loop | Accept | Fast required versus nightly checks, debug artifacts, same image locally, CI latency reserve of 3-5 weeks, API log access (Section 14.2 and 17.1). |
| N6 | Clean-room rule for the GPL extension | Accept | Provenance rule added (Section 10A.7). A second reviewer pass is open question OQ10. |
| N7 | Spike fallback contradicts the owner's need | Modify | Accepted: deferral removed as a listed outcome and success criteria added. Modified: the plan does not rely on the review's statement about what the owner said. D27 as recorded already puts the Linux runtime in v1, and a failed spike now stops work and asks the owner. |
| N8 | D3D12 support-state wording | Accept | Section 6.1 item 3 now defines `ci-verified` UI and the exact promotion condition for `supported`. |
| N9 | Flavors multiply the test matrix | Accept | Lane matrix proposed in Section 14.2: `full` on every lane, `play` on API 29 and 36. |
| F1 | DXIL in the default package | Reject | D9 keeps three backends and the default package stays full multi-platform. Revisit at M3 only if measured package size matters. |
| H2 | FFmpeg LGPL bundling | Accept | Rule added to 14.1: dynamic, replaceable, built without GPL or non-free parts. Android uses MediaCodec. |

Counts: 12 accepted, 8 modified, 1 rejected, out of 21 findings.

### 31.2 Claims checked and claims left unverified

**Checked:**
- Document size 231,504 bytes, 224 inline version tags, and the effort total of 71-120 weeks (recomputed from the table).
- Android's bionic does not implement `pthread_cancel` (bionic `docs/status.md` and its CAVEATS file, android.googlesource.com/platform/bionic).
- A foreground service that needs while-in-use microphone access cannot be created while the app is in the background (developer.android.com/develop/background-work/services/fgs/restrictions-bg-start).
- SheenBidi is Apache-2.0 (github.com/Tehreer/SheenBidi).

**Not verified:** capture pause and resume inside a running microphone-type foreground service on API 29-36; FriBidi license terms; the glTF extension candidate list; OS reduce-motion setting names; the flash-rate guideline wording; IPC and memory cost of any extra process; every unverified item already listed in Section 30.2. Nothing in this plan is claimed verified on real Windows, Android or GNOME hardware.
