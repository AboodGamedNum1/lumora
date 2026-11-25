# Dependency manifest (M0)

Every direct dependency is pinned here with license, upstream and policy. A dependency is added only when the milestone that needs it starts. License rule: Apache-2.0 compatible only, no GPL or AGPL (see `docs/build-and-deps.md`, Section 14.1).

| Dependency | Pin | License | Upstream | How fetched | Used by | Platforms tested |
|---|---|---|---|---|---|---|
| Catch2 | v3.9.1 | BSL-1.0 | github.com/catchorg/Catch2 | CMake FetchContent, shallow | tests | Linux, Windows (CI) |
| SDL3 | release-3.4.20 | zlib | github.com/libsdl-org/SDL | CMake FetchContent, static | editor window layer (default per plan; GLFW is the fallback) | Linux, Windows (CI) |
| Dear ImGui | v1.92.9b-docking | MIT | github.com/ocornut/imgui | CMake FetchContent | editor UI (no RHI backend yet) | Linux, Windows (CI) |

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

DXC and glslang pins are not made yet (they arrive with the shader pipeline, M1).
