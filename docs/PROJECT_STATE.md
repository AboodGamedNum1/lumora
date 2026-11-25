# PROJECT_STATE

> Current state of the project. Every session starts by reading this file and the ADR log (`docs/adr/`), and ends by updating it.

last_completed_milestone: M0
last_updated: 2026-11-25

## Current milestone
M0, Foundations and CI. Exit criteria: see `docs/milestones.md`; status table in `PLAN.md`.

## Done
- M0 exit criteria met, CI-verified in https://github.com/Captain3BoOd/lumora/actions/runs/45568499611 (commit 8b14281): Linux build, tests, ASan and UBSan green; Windows build and editor smoke test green (no GPU); Android release APK builds with ABI set exactly arm64-v8a and 16 KB alignment checked; API 29 x86_64 emulator boots and runs one instrumented test (KVM works on ubuntu-24.04 hosted runners).
- Secret scan, Dependabot, shared test entry point, docs check in CI.

## In flight
- M0 carry-over (not exit criteria): WARP and software-Vulkan spike, GNOME spike start, DXC and glslang pins, CodeQL, feature-test header check, version drift failure, branch protection (owner repository setting).

## Known failures
- none recorded yet

## Next five tasks
1. Close M0 carry-over items (WARP spike, GNOME spike source review, CodeQL).
2. M1: RHI interface and Vulkan backend skeleton.
3. M1: shader toolchain pins (glslang, SPIRV-Cross, DXC).
4. M1: GLES 3.2 backend and golden-image harness.
5. M1: D3D12 build and DXIL on Windows CI.

## Where CI logs and artifacts live
https://github.com/Captain3BoOd/lumora/actions

## Evidence labels in use
CI-verified, WARP software evidence, owner-hardware-verified. Never claim hardware evidence that has not been run on the owner's device.

## Decisions pending with the owner
None block M0.
