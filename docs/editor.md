# Lumora Editor (Section 11)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 11. Lumora Editor (Windows and Linux)

### 11.1 Shape

Single-window Dear ImGui application for **Windows and Linux**, docked panels, C++26, rendering through the RHI (Vulkan default on both platforms; Linux has no other backend; D3D12 is Windows-only and selectable only per the support-state gate in Section 6.1 item 3 until G5 passes). The preview viewport **is** the runtime renderer, forced to Android-equivalent settings (GLES-tier limits simulation, selectable device profiles; "Realme 6" is a built-in profile with its resolution, refresh, and GPU tier).

### 11.2 Feature list (D15, "powerful, with everything")

- **Hierarchy panel:** scene graph tree, reparenting, multi-select, search.
- **Inspector:** transform, components, material slots, script attachment; all property edits undoable.
- **Viewport:** translate/rotate/scale gizmos, camera fly-through, grid/snap, selection outlines, playback controls (play/pause/step the scene simulation exactly as the phone will run it).
- **Asset browser:** import glTF/images/audio/video by drag-drop; shows validation results immediately.
- **Material editor:** PBR parameters, texture slots, blend modes; live.
- **Shader editor:** GLSL text editor with syntax highlight, error squiggles from glslang, per-backend compile report. (A visual node-based shader graph is a post-v1 candidate; code-first per the confirmed plan.)
- **Animation timeline:** keyframe editor for node properties (position/rotation/scale/color/custom floats), curves with easing; plays in viewport; glTF skeletal clips playable and blendable.
- **Particle editor:** emitter shape, curves over lifetime, forces, collision with scene physics; live preview.
- **Physics panel:** collider editing (box/sphere/capsule/mesh), body types, constraints; debug draw in viewport.
- **Audio panel:** attach sounds to events, audition them.
- **Interaction binder:** UI for wiring inputs → script events ("when screen unlocks → play animation X") without writing code for common cases; the generated Lua is visible and editable. **Ownership rule:** generated Lua lives in marked generated blocks; editing a binding in the binder regenerates its block. If the author hand-edits a generated block, that wiring turns read-only in the binder with a visible notice (never a silent overwrite).
- **Settings designer:** the per-scene settings screen (`settings.json`) authored visually: add sliders/colors/toggles, bind them to scene values.
- **Localization-aware:** editor UI in Arabic/English with RTL layout; scene text preview with Arabic shaping.
- **Project system:** a project folder (unpacked working form) + one-click **Export → `.lumora`** (validates, compresses textures to KTX2/ASTC, compiles shaders to all three targets, packs, checksums, stamps version).
- **Platform preview:** the device-profile selector gains a "Linux desktop" profile (pointer-only input, desktop resolutions and refresh, no phone sensors, mocked audio monitor). The pre-export validation report warns when a scene uses inputs missing on a chosen target (for example gyroscope on Linux) and shows per-platform capability coverage.
- **Linux editor specifics:** the window layer works on both X11 and Wayland; file dialogs, drag-and-drop, clipboard, IME/text input, HiDPI/fractional scaling and Arabic text input are verified on each supported session; the Linux editor and the Windows editor share one codebase and one project format (Section 11.4 is platform-neutral). The window layer is **SDL3 by default with GLFW as the documented fallback, pinned at M0** (not at the Linux spike): the plan's riskiest editor requirements - Arabic IME input, HiDPI/fractional scaling, Wayland - are exactly where GLFW is historically weak (no proper IME composition API) and SDL3 covers, so the spike validates the choice instead of discovering it.
- **Send to phone:** ADB push if the phone is connected; otherwise produces the file + a QR/share flow for manual transfer.
- **Safety features:** undo/redo everywhere, autosave with rolling backups, crash recovery, pre-export validation report (missing assets, oversized textures, scripts that call unavailable APIs, permission declarations).

### 11.3 Editor architecture decisions

- **Undo/redo** is a central command/transaction system from the first editor milestone, not retrofitted: every mutation (inspector edits, gizmos, reparent/delete, asset replacement, material/shader edits, timeline keys, settings designer changes, multi-select ops) goes through a reversible command with drag gestures grouped into one transaction; bounded history with dirty-state tracking.
- **Autosave and recovery:** interval + on-idle autosave to a journal beside the project, atomic save-via-rename, rolling backups with retention limits, and a crash-recovery flow that offers the autosaved state without touching the last known-good save.
- **Code editor component:** the GLSL/Lua editor is built on a dedicated text-editor component (an ImGuiColorTextEdit-class component, evaluated and pinned at M7) with syntax highlighting, line numbers, diagnostics with source navigation, find/replace, multi-file tabs, compile debounce, and per-backend error reports. Dear ImGui itself provides no code editor; this is budgeted as its own work item. The original BalazsJako/ImGuiColorTextEdit repository is effectively unmaintained; the evaluation baseline is the maintained fork (goossens/ImGuiColorTextEdit). It has no RTL/bidi support, so mixed-direction editing inside the code editor is an explicit RTL prototype checklist item (Section 13). The code editor is **left-to-right only** in v1 (monospace; Arabic appears only inside comments and strings, rendered per run with no bidi caret). It is removed from the RTL acceptance item (16.4 item 7) and the limitation is documented. Its text widget keeps a private undo stack while focused and commits one global "text edit" command on blur or save, so Ctrl+Z does not fight the global stack.
- **Preview parity, stated honestly:** the device profile simulates resolution/render-scale, refresh cap, feature tier, texture formats, memory budget, shader variant, and mocked sensors/permissions. It cannot simulate a real Mali driver, OEM OS behavior, thermal behavior, or hardware video decoding; the editor labels simulated constraints as such, and on-device validation remains the final gate (Section 16).
- **Asset import pipeline:** importers validate and report diagnostics on drop; every imported asset gets a stable UUID and metadata (color space, normal-map convention, skinning limits, supported glTF extensions with warnings for unsupported ones); reimport detection, background processing with cancellation, and hard rejection of malformed or oversized files.
- **Undo/redo boundaries:** the command system defines transaction boundaries explicitly (gesture drags coalesce into one transaction; an import is one transaction), detects external file modification of watched project files (conflicts are surfaced with keep-mine/reload/merge-where-safe choices, never silently overwritten), marks irreversible operations as breaking the undo chain with a warning, and represents deleted/moved assets referenced by history entries as tombstones.
- **Internal extension architecture:** no public plugin system, but component registration, custom inspectors, importers, and runtime components go through versioned internal interfaces with their own serialization and migration rules, so post-v1 features plug in without core rewrites.
- **Editor UX prototype gate:** Dear ImGui is a UI foundation, not a ready-made editor. At the start of M7a (after the text engine milestone MT, Section 13), before feature build-out, a prototype proves: docking and workspace layout, hierarchy, inspector, asset browser, the code editor component, RTL mirrored layout, text editing, keyboard navigation and focus order, keyboard-only operation and focus visibility of the core panels (screen-reader support is out of scope: Dear ImGui has no OS accessibility tree, and the upstream request is open), and large-project responsiveness (a 10k-node synthetic scene). Screenshot-verified through an offscreen RHI render target and the Dear ImGui test engine (`imgui_test_engine`) for scripted UI tests in CI; findings are fixed before M7 continues.

- **Editor decisions from the editor review.**
 - **Single OS window, no floating viewports** on every platform. Docking works inside the window; multi-viewport is not supported by the SDL3 backend on Wayland (reviewer-cited upstream issue, not re-fetched), and SDL3 plus Wayland text input has a reported open issue on a recent Dear ImGui release. M0 spikes type Arabic through IME on Wayland (GNOME/ibus), X11 and Windows; SDL3 and Dear ImGui versions are pinned and re-tested on every bump.
 - **Dear ImGui is pinned** to a specific docking-branch commit or tag (version 1.92.x or newer; the exact current number is unverified), rebased on a set cadence. FreeType is the font backend; with 1.92 glyph ranges are unnecessary and fonts load dynamically, so the RHI gets its own Dear ImGui backend written to the 1.92 texture protocol (`ImGuiBackendFlags_RendererHasTextures`).
 - **Undo/redo details.** One rule for text editors (above). Command granularity for curves is point, tangent and whole-curve. Property-based tests apply random command sequences, undo all, and assert the scene hash equals the original.
 - **Binder round-trip.** The binder model is stored in scene JSON as the source of truth; the Lua block is generated from it and carries a content hash in its header. A hash mismatch marks the block hand-edited, read-only in the binder, with "revert to generated" and "detach to script" actions. A golden-file test covers it.
 - **Layouts and mirroring.** Two named dock layouts (LTR, RTL) generated with the DockBuilder API plus a "reset layout" command; gizmos, timeline and viewport stay left-to-right and are not mirrored.
 - **Hierarchy at scale.** `ImGuiListClipper` over a flattened visible-node list; budget: UI CPU under 4 ms at the 10k-node synthetic scene.
 - **Widgets by name.** ImGuizmo and ImPlot are pinned at M0; ImPlot is not a timeline or curve editor, so a custom timeline/curve widget is budgeted (2-3 weeks, inside M7b). The material node editor is **not in v1** (a node shader graph is post-v1, 11.2), which removes the v1 contradiction.

### 11.4 Editor project and asset layout

A Lumora project is a folder with a fixed structure, specified in `docs/project-format.md` (M3, alongside the package format):

```
project.lumoraproj # project descriptor: format version, engine version, scene list
scenes/*.scene.json # source-of-truth scenes (the package's scene.json is derived)
assets/ # source assets: models, images, audio, video, scripts, shaders
assets/<id>.meta # per-asset import metadata: stable UUID, importer settings, color space
cache/ # derived: imported/processed assets, shader artifacts - always regenerable
backups/ # rolling autosave backups
```

Rules: UUIDs are generated once at import and never change; all references are UUID-based with relative paths inside the project only; moving a project folder is safe (no absolute paths); a missing or changed source asset is flagged in the asset browser with last-known-good available from cache and reimport offered; `cache/` can be deleted and rebuilt; only source files and the descriptor are authoritative.


### 11.5 Authoring experience (work item due by the end of M7a)

D15 asks that anyone can build any scene. The editor and runtime contracts alone do not deliver that. This work item has exit criteria and is due no later than M7a:

- A user manual and a "first scene in 15 minutes" tutorial, in Arabic and English.
- Starter templates and sample assets.
- LuaLS type stubs for `lumora.*`, generated with the API reference from `docs/lua-api.md` (Section 9.4).
- An in-editor script console and error panel: runtime errors with file and line, log output, and an event log.
- Live reload of scripts and settings in the editor preview.

*Exit: a person who has not seen the project builds and runs a parallax scene from the tutorial alone, in both languages, and the script console shows a deliberately introduced Lua error with file and line.*
