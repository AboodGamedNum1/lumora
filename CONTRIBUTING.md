# Contributing to Lumora

Thanks for helping. This file explains how to propose changes and what a change must have to be merged.

Start with [README.md](README.md) (what Lumora is), [PLAN.md](PLAN.md) (what is done and what is missing) and [docs/coding-standards.md](docs/coding-standards.md).

## Workflow: fork and pull request

1. Fork the repository on GitHub. Contributors do not push to this repository.
2. Create a branch in your fork, one topic per branch.
3. Make small, focused commits with clear messages. One idea per commit.
4. Open a pull request against `main`. Describe what changed, why, and which item of [PLAN.md](PLAN.md) it belongs to.
5. CI must be green. A maintainer reviews the change against PLAN.md and the CI result before merge.
6. Before starting a larger item, check PLAN.md and open pull requests so two people do not build the same thing. Say in the PR or an issue which item you take.

Do not change repository settings, visibility, workflows' permissions or branch rules in a pull request.

## Build and test

Requirements: CMake 3.22 or newer, a C++23 compiler (GCC, Clang or MSVC), Python 3, Git, network access (CMake fetches pinned dependencies). The pins are in [docs/dependency-manifest.md](docs/dependency-manifest.md).

One entry point builds and tests everything, locally and in CI:

    python3 tools/ci/test.py

Options: `--sanitize` (ASan and UBSan on GCC or Clang), `--config Debug`, `--build-dir DIR`. To build only the core and tests, configure with `-DLUMORA_BUILD_EDITOR=OFF`.

Per platform:

- **Linux (Ubuntu 24.04):** install the packages listed in the README Build section. For the Vulkan tests also install `libvulkan-dev`, `mesa-vulkan-drivers`, `vulkan-validationlayers` and `glslang-tools` (CI uses lavapipe, a software Vulkan driver, so no GPU is needed).
- **Windows:** use a "x64 Native Tools" prompt for Visual Studio 2022 or newer, then run the same Python command. The D3D12 tests use the WARP software adapter and DXC from the Windows SDK.
- **Android:** the `android/` project builds with Gradle (see the Android job in `.github/workflows/ci.yml` for exact versions). No physical phone is required for CI.
- **Docs:** run `python3 tools/check_docs.py` before committing any document change.

You do not need to run Windows or Android locally. Pushing to your fork runs the same CI, which is the reference result.

## CI expectations

- Every pull request runs: docs check, Linux build and tests (with ASan and UBSan), Windows build, tests and editor smoke test, Android build, and an API 29 emulator test.
- A change is not ready until all jobs are green. Do not skip or weaken a test to get green. Fix the cause.
- Golden-image tests compare against a baseline under `tests/golden/<renderer>/`. A new baseline is taken from the CI artifact of a run, looked at by a person, and committed on purpose. Never overwrite a baseline just to make a diff pass. See [docs/golden-images.md](docs/golden-images.md).
- Dependencies are pinned. A pin bump is its own commit and updates the manifest in the same commit.

## Code style

Follow [docs/coding-standards.md](docs/coding-standards.md). In short: C++23, public headers under `core/<module>/include/lumora/...`, tests in `tests/` for the behavior you add, and match the style of the surrounding code. All repository text, comments and documents are in English.

## Evidence labels

The plan separates three kinds of evidence. Use these exact labels in docs, commit messages and PR text:

- **CI-verified:** a CI run proves it. Link the run.
- **WARP software evidence:** D3D12 on the WARP software adapter. It is never evidence for a real GPU.
- **owner-hardware-verified:** run on the owner's own device by the owner.

Nothing is claimed verified on real Windows, Android or GNOME hardware, or on a specific GPU, unless a real test shows it. A result from lavapipe, WARP, an emulator or a nested shell says so in its label.

## Secrets and personal details

- Never commit tokens, passwords, keys or personal data. CI runs a secret scan and will fail the change.
- Do not add personal details about anyone. Hardware gates stay as the plan states them.
- Do not paste credentials into issues, PRs, logs or commit messages.

## Notes for open items

These are starting points. The authority is [PLAN.md](PLAN.md) section 5 and [docs/milestones.md](docs/milestones.md). Check them before you begin, and update PLAN.md in the same PR.

### GLES 3.2 backend

- Goal: the same behavior as the Vulkan and D3D12 backends behind the RHI in `core/rhi/include/lumora/rhi/rhi.h`, at parity for the required set (device, buffers, textures, one render pass, a graphics pipeline, submit, readback).
- Create it as `core/rhi/gles/`, selected the way `create_device` selects Vulkan and D3D12, with its own `Backend` value and a backend state entry in `rhi.cpp` that reads CiVerified only when CI runs it.
- Shaders: the pipeline description takes SPIR-V today. Use SPIRV-Cross (pin: see the manifest, not yet fetched) to produce GLSL ES 3.20, or add a GLSL source field if that is simpler for the first slice. State the choice in the PR.
- CI needs a software GLES 3.2 context on Linux, for example Mesa llvmpipe through EGL on a headless surface, or ANGLE or SwiftShader. Record which one worked and its version in the manifest.
- Add a triangle golden baseline in `tests/golden/<renderer>/` following [docs/golden-images.md](docs/golden-images.md). Reuse the existing triangle test body so the three backends are compared the same way.
- Label: CI-verified software evidence. Not device evidence.

### CodeQL

- Add a workflow `.github/workflows/codeql.yml` that analyzes C and C++ (manual build using `tools/ci/test.py`) and, if cheap, Kotlin and Java for the Android app.
- Pin the action by commit SHA like the other workflows, keep `permissions` minimal (`security-events: write` only on that job, read-only elsewhere), and run on push to `main`, on pull requests and on a weekly schedule.
- Do not change repository settings. If code scanning needs an owner-only setting, say so in the PR and do not work around it.
- Update the CodeQL rows in PLAN.md when it runs green.

### GNOME spike source review

- Goal (see the GNOME spike item in docs/milestones.md): find out how a live wallpaper window can sit at the bottom of the stack on GNOME, stay out of Alt-Tab and the dock, be input-through, and work per monitor and across enable and disable.
- Deliverable: `docs/linux-wallpaper-matrix.md`. Read the source of existing GNOME live-wallpaper extensions and the Mutter and GNOME Shell code involved, and cite exact files, versions and commits.
- Label everything from source reading as unverified on real hardware. A nested-shell run in CI is a second, separate label. Do not claim the method works on a real desktop.
- Keep the fallback in view: a normal-window "wallpaper" mode with a documented limitation, as written in the milestones. Do not decide between options, record them. Open questions stay open until the owner decides.
