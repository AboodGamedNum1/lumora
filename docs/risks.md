# Risks and mitigations (Section 18)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 18. Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| Realme 6 Vulkan driver bugs (Mali + MediaTek drivers are historically flaky) | Medium | GLES backend is the automatic fallback (D9); driver blocklist mechanism; golden tests run on-device by the owner's script. |
| 90 fps at 1080×2400 too heavy for Mali-G76 on complex scenes | Medium | Render-scale setting, quality tiers, fps cap; the editor's Realme 6 profile warns authors at design time. |
| C++26 toolchain gaps | Medium | Section 14 feature-subset rule; CI matrix with MSVC, GCC, Clang and NDK Clang and an executable feature-test header; compiler versions pinned by container or setup action. |
| Battery drain complaints | Medium | Hard visibility rules, battery-saver behavior, measured drain in the phone test script before release. |
| Scope size vs. one developer | High (it is a big project) | Strict milestone order; editor MVP before editor full; feature demos only become samples after passing gates. Progress reported per milestone (D23). |
| Untested-on-hardware surprises | Medium | Hardware issues surface at M10 because the owner decided against mid-project hardware gates (D23). Mitigations: CI-verified Windows and Linux builds from M0, software and WARP evidence clearly labeled, emulator lanes for API 29/34/36, honest labels, the G1-G7 gate set (parallelized, with a fix-retest reserve and diagnostics from M9), the GLES fallback, and the open option of owner-downloaded CI smoke builds (open question OQ6). The residual schedule risk is accepted by the owner. |
| Diagnostics over-collect | Low | Diagnostics stay local; export is an explicit user action; no analytics, no accounts, nothing transmitted (Section 10.2, 16). |
| Arabic RTL in ImGui is weak upstream | **High** | Dear ImGui has no shaping or bidi; the plan builds the Lumora text engine as its own milestone MT (Section 13), keeps the code editor left-to-right, and offers a reduced scope (open question OQ1). Screenshot-verified; IME and Wayland typing checked on owner hardware. |
| Linux wallpaper fragmentation (X11 vs Wayland, GNOME has no background-layer protocol, per-desktop behavior) | High | The deciding spike runs at M0-M1, before the runtime is built (17); GNOME-on-Wayland is the primary case and the design is a Shell extension companion with the native renderer (10A.2), labeled experimental; per-desktop compatibility matrix; no claim beyond tested sessions. **"Honest unsupported-desktop message" is no longer an acceptable GNOME outcome** - GNOME is the owner's own machine, so failing there fails D27 on his hardware; the extension-placed renderer toplevel is the mitigation, with the fallback scope of Section 17 if the spike fails. |
| GNOME Shell extension breaks on a new GNOME release | Medium-High (recurring) | Extensions declare supported shell versions and typically need a small update per GNOME release; per-version support matrix and maintenance plan in `docs/dependency-manifest.md`; the extension is deliberately tiny so the per-release port is small. |
| Linux scope growth (distros, desktops, packaging) | Medium-High | The distribution is fixed to Ubuntu (D29) and the desktop to GNOME (D29); packaging is fixed (D28). Anything else is best-effort and unlabeled as supported. |
| Malicious/buggy friend scenes | Low-Medium | Section 15 security model. |
| GNOME extension operational reality | Medium | Install and update route (10A.2), GNOME disabling extensions after a shell crash, orphan renderers, no wallpaper on the lock screen; first-run onboarding and the reaper in the control app. |
| Realme 6 firmware may not show a live wallpaper on the lock screen | Medium | Home-only is accepted as D4 met with a documented OEM limitation if the owner agrees (open question OQ2); G3 records actual behavior. |
| OEM power-saving kills the wallpaper process (ColorOS "super power saving") | Medium | Documented in the app; the phone script tests it; watchdog and restart path. |
| Play policy rejection for microphone, Notification Listener or MediaProjection | Medium-High | `play` / `full` flavors (9.3), declarations and demo material at M10. |
| Windows SmartScreen and signing | Medium | Route chosen by the owner (open question OQ5); documented warning for unsigned builds; never sign public builds with a self-signed certificate. |
| CI cost and flakiness | Medium | Repo visibility decision, concurrency cancel, nightly heavy jobs, cache budget, flake policy (14.2). |
| Owner availability for M10 gates | Medium | M10 budgeted at 6-10 weeks with the stated assumption; gates parallelized. |
| Single-executor continuity over 12+ months | Medium | `docs/PROJECT_STATE.md`, an ADR log, a start-and-end-of-session rule and a CI staleness check (Section 14.2); milestone checkpoints. |

---
| Native crash or GPU hang freezes the display or loops (new) | Medium | Own wallpaper process, crash-loop breaker, isolated import validation (Section 10.7), shader static limits (Section 15.1). |
| CI-only feedback is slow and Windows cannot be reproduced locally (new) | Medium | Fast required checks versus nightly, debug artifacts on every failure, a CI latency reserve in the estimate (Sections 14.2 and 17.1). |
| Public repository exposes private details or a leaked credential (new) | Medium | Review history stripped of message identifiers and quotes, secret scanning and push protection, vault-only credential route, token revoked at M10 (Section 14.2). |
