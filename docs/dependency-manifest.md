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

Filled in from CI job logs as they become available (see `docs/PROJECT_STATE.md`).
