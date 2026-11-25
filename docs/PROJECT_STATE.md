# PROJECT_STATE

> Current state of the project. Every session starts by reading this file and the ADR log (`docs/adr/`), and ends by updating it.

last_completed_milestone: M1
last_updated: 2026-11-25

## Current milestone
M2, Runtime MVP (M0 and M1 are done). Exit criteria: see `docs/milestones.md`; status table in `PLAN.md`.

## Done
- M1 exit criteria met, CI-verified in https://github.com/Captain3BoOd/lumora/actions/runs/45639527288 (commit 0c78fcc): Vulkan on lavapipe (Linux and Windows), GLES on llvmpipe and on the API 29 emulator, D3D12 on WARP, golden images per renderer, CodeQL. Carry-over is in PLAN.md section 6.
- M1 step 4: golden-image harness with tolerance and a lavapipe triangle baseline, https://github.com/Captain3BoOd/lumora/actions/runs/45590514447.
- M1 step 3: Vulkan command recording, render pass, pipeline, pixel-checked triangle on lavapipe, validation layer asserted active, https://github.com/Captain3BoOd/lumora/actions/runs/45587423739.
- M1 step 2: Vulkan backend first slice (device, validation, buffers, textures) CI-verified on lavapipe, https://github.com/Captain3BoOd/lumora/actions/runs/45573393813.
- M1 step 1: RHI interface, Null backend and tests; WARP D3D12 spike worked (WARP software evidence), https://github.com/Captain3BoOd/lumora/actions/runs/45570530990.
- M0 exit criteria met, CI-verified in https://github.com/Captain3BoOd/lumora/actions/runs/45568499611 (commit 8b14281): Linux build, tests, ASan and UBSan green; Windows build and editor smoke test green (no GPU); Android release APK builds with ABI set exactly arm64-v8a and 16 KB alignment checked; API 29 x86_64 emulator boots and runs one instrumented test (KVM works on ubuntu-24.04 hosted runners).
- Secret scan, Dependabot, shared test entry point, docs check in CI.

## In flight
- M2 started.
- M1: D3D12 backend first slice on WARP with DXIL test shaders and a triangle golden image (WARP software evidence, CI-verified).
- M1: GLES backend first slice on Mesa llvmpipe with a triangle golden image (CI-verified software evidence); CodeQL workflow green.
- M0 carry-over (not exit criteria): WARP and software-Vulkan spike, GNOME spike start, DXC and glslang pins, CodeQL, feature-test header check, version drift failure, branch protection (owner repository setting).

## Known failures
- Maven Central occasionally rate-limits hosted runners (HTTP 429); Gradle jobs cache and retry.
- ASan job runs with leak detection off (third-party llvmpipe allocations).

## Next five tasks
1. M2: Android WallpaperService hosting the core, engine in its own process.
2. M2: frame pacing, visibility and pause rules, fps overlay.
3. M2: crash-loop breaker, manifest metadata, Direct Boot rule, flavors.
4. M2: emulator lanes for API 29, 34 and 36.
5. M1 carry-over: SPIRV-Cross path for GLES, nested-shell GNOME spike run, Vulkan feature gaps.

## Where CI logs and artifacts live
https://github.com/Captain3BoOd/lumora/actions

## Evidence labels in use
CI-verified, WARP software evidence, owner-hardware-verified. Never claim hardware evidence that has not been run on the owner's device.

## Decisions pending with the owner
None block M0.
