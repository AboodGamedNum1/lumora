# Lumora

**Lumora is a live wallpaper engine.** You build a fully custom 2D or 3D scene (your own models, shaders, scripts, animations, audio and physics) in a desktop editor, export it as one `.lumora` package, and run it as a live wallpaper on an Android phone and on the Linux desktop.

> **Status: early development (milestone M0, foundations and CI).** Nothing here is a usable product yet. See [PLAN.md](PLAN.md) for exactly what is done and what is missing. Lumora is a working name and may change.

## The idea

Most live wallpaper apps ship a fixed set of effects. Lumora works like a small game engine instead: the scene is yours, the behavior is scripted, and after you build it, the engine runs it as the wallpaper. Scenes can react to touch, motion sensors, screen on/off, time, and other inputs the operating system exposes. The scene author decides which inputs to use and how.

Two programs share one C++ core, so what you see in the editor preview is what the wallpaper renders:

| Program | Platform | Role |
|---|---|---|
| **Lumora Editor** | Windows and Linux (Dear ImGui) | Author scenes, preview, export `.lumora` packages |
| **Lumora Runtime** | Android app, and a Linux desktop wallpaper runtime | Load a package and render it as the wallpaper |

Planned technology: C++ (language level 23 now, 26 where every pinned compiler supports it), Dear ImGui for the editor, glTF 2.0 for 3D assets, Lua 5.4 (sandboxed) for scripting, GLSL as the single shader language, and a new in-house render hardware interface (RHI) with Vulkan, OpenGL ES and Direct3D 12 backends. Android targets 64-bit ARM only (`arm64-v8a`), minimum API 29.

## What works today

Only the build and CI foundation. See the status table in [PLAN.md](PLAN.md). Nothing is verified on real Windows, Android or GNOME hardware. Where CI proves something, the plan says so and labels the evidence (CI-verified, WARP software evidence, owner-hardware-verified). CI results are never presented as hardware results.

## Build

Requirements: CMake 3.22 or newer, a C++23 compiler (GCC, Clang or MSVC), Python 3, Git and network access (dependencies are fetched by CMake at pinned versions).

Linux (Ubuntu) also needs the SDL3 build dependencies for the editor, for example:

```
sudo apt-get install build-essential cmake ninja-build python3 libx11-dev libxext-dev libxtst-dev libxfixes-dev libxrender-dev libxinerama-dev \
  libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev libwayland-dev \
  libegl-dev libgl-dev libdrm-dev libgbm-dev libasound2-dev \
  libvulkan-dev mesa-vulkan-drivers vulkan-validationlayers glslang-tools
```

The single entry point shared by local runs and CI configures, builds and tests everything:

```
python3 tools/ci/test.py
```

Options: `--sanitize` (ASan and UBSan on GCC/Clang), `--config Debug`, `--build-dir DIR`. To build only the core and tests without the editor, configure with `-DLUMORA_BUILD_EDITOR=OFF`.

On Windows use a "x64 Native Tools" prompt for Visual Studio 2022 or newer and run the same Python command.

## Run

The editor is currently an empty window (a Dear ImGui frame loop without a GPU backend):

```
build/editor/lumora_editor            # Linux
build\editor\Release\lumora_editor.exe  # Windows
build/editor/lumora_editor --smoke    # headless smoke run, exits by itself
```

There is no runnable Android wallpaper or Linux desktop wallpaper yet.

## Repository layout

```
core/            platform-agnostic engine core (grows per milestone)
editor/          desktop editor (Dear ImGui)
tests/           unit tests (Catch2)
android/         Android app and wallpaper glue (from M0 skeleton onward)
tools/           check_docs.py, ci/test.py (shared test entry point)
cmake/           dependency pins and build helpers
docs/            normative design documents, PROJECT_STATE.md, ADRs
LUMORA_MASTER.md the master plan; PLAN.md is the live status view
```

The full target layout is in [docs/architecture.md](docs/architecture.md).

## Documentation

- [PLAN.md](PLAN.md): project plan, milestone status, what is done and what is missing (kept current).
- [LUMORA_MASTER.md](LUMORA_MASTER.md): the master specification and decision log.
- [docs/](docs/): one normative document per topic. Run `python3 tools/check_docs.py` before committing document changes.

## Contributing and CI

GitHub Actions runs on every push and pull request: documentation check, Linux build and tests (including ASan and UBSan), Windows build and the editor smoke test, and an Android build. Runners and third-party actions are pinned, the default token is read-only, and no secret is ever committed. Small, focused commits are preferred. Read [docs/ci.md](docs/ci.md) and [docs/coding-standards.md](docs/coding-standards.md) first.

## License

Apache License 2.0, see [LICENSE](LICENSE). Third-party components keep their own licenses, recorded in [docs/dependency-manifest.md](docs/dependency-manifest.md).
