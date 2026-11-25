# Dependency manifest (M0)

Every direct dependency is pinned here with license, upstream and policy. A dependency is added only when the milestone that needs it starts. License rule: Apache-2.0 compatible only, no GPL or AGPL (see `docs/build-and-deps.md`, Section 14.1).

| Dependency | Pin | License | Upstream | How fetched | Used by | Platforms tested |
|---|---|---|---|---|---|---|
| Catch2 | v3.9.1 | BSL-1.0 | github.com/catchorg/Catch2 | CMake FetchContent, shallow | tests | Linux, Windows (CI) |
| SDL3 | release-3.4.20 | zlib | github.com/libsdl-org/SDL | CMake FetchContent, static | editor window layer (default per plan; GLFW is the fallback) | Linux, Windows (CI) |
| Dear ImGui | v1.92.9b-docking | MIT | github.com/ocornut/imgui | CMake FetchContent | editor UI (no RHI backend yet) | Linux, Windows (CI) |
| SPIRV-Cross | vulkan-sdk-1.4.357.0 (pinned) | Apache-2.0 | github.com/KhronosGroup/SPIRV-Cross | CMake FetchContent (cmake/Dependencies.cmake) | SPIR-V to GLSL ES for the GLES 3.2 backend | built by the Linux CI job (LUMORA_ENABLE_SPIRV_CROSS), GLSL backend only; the converter is tested on GLES (llvmpipe) |
| DirectX Shader Compiler (DXC) | v1.9.2609 (pinned, not yet fetched; CI currently uses the dxc.exe in the Windows SDK 10.0.26100.0, observed as 1.9.2602.17) | LLVM Release (Apache-2.0 with LLVM exceptions) | github.com/microsoft/DirectXShaderCompiler | release package or NuGet Microsoft.Direct3D.DXC, when the SDK dxc is not enough | HLSL to DXIL test shaders and later the shader pipeline | SDK dxc 1.9.2602.17: Windows (CI) |
| Lua | 5.4.9 (pinned), sha256 2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6, the value lua.org publishes at www.lua.org/ftp/ | MIT | www.lua.org (PUC-Rio) | CMake FetchContent from the release tarball with URL_HASH, built from source as a static library (compiled as C++, only the core and the base, math, string and table libraries) | sandboxed script host for the M3S slice (M5 work pulled forward minimally) | Linux, Windows (CI) |
| Mesa for Windows (lavapipe ICD), CI spike only | 26.3.1, msvc package, sha256 64E84CF13D67DD354640BF443ED0445E6A83AFD06A5729F0750B3744D633B3F1 | MIT (Mesa) | github.com/pal1000/mesa-dist-win | downloaded in a Windows CI probe step, not shipped | software Vulkan on the Windows runner (spike) | Windows (CI, Vulkan tests pass on it) |

Update policy: bump a pin in its own commit, run the full CI, and update this table in the same commit. Security fixes are applied within a week of upstream release.

Toolchain pins (CMake, Ninja, MSVC toolset, Windows SDK, NDK, Gradle, AGP, JDK) are recorded here from the first green CI logs; see the section below once filled in.

## Toolchain versions observed in CI

Read from the CI run for commit 8b14281 (https://github.com/Captain3BoOd/lumora/actions/runs/45568499611). These are runner-provided versions, recorded as observed. Pinning to exact versions on drift is still to do.

| Tool | Linux (ubuntu-24.04) | Windows (windows-2025) | Android job |
|---|---|---|---|
| CMake | 3.31.6 | 4.5.0 | 3.22.1 (SDK package, pinned in Gradle) |
| Compiler | GCC 13.3.0 | MSVC 14.52.37025 (Visual Studio 18 2026) | NDK 27.2.12479018 Clang (pinned) |
| Windows SDK | n/a | 10.0.26100.0 | n/a |
| Ninja | 1.13.2 | not used (Visual Studio generator) | n/a |
| Python | 3.12.3 | runner default | runner default |
| Gradle / AGP / Kotlin / JDK | n/a | n/a | 8.11.1 / 8.7.3 / 2.0.21 / Temurin 17.0.21 |

Shader and Vulkan test tools on the Linux job, installed from the Ubuntu 24.04 archive and recorded as observed (not yet hard-pinned): glslang-tools 15.1.0 (glslangValidator compiles the test shaders to SPIR-V), Vulkan validation layers 1.3.275, Vulkan headers and loader 1.3.275, Mesa 25.2.8 (lavapipe). Hard pins (a source build or container image) and SPIRV-Cross and DXC pins are recorded in the table above from the upstream tag and release pages; they are not consumed by CI yet (the Windows job uses the SDK dxc and logs its version).
