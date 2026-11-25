# Requirements traceability (Section 20)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 20. Requirements traceability (self-review)

Every owner statement mapped to where the plan handles it:

- "Two programs: a scene-maker and a wallpaper-runner, scene converted to a format the runner understands" → §1, §5, §8.
- "Script it: GLSL or its own language; interaction defined per scene; supports 3D models" → §8, §9.
- "Vulkan, GLES, D3D12 under one RHI; new RHI OK" → §6.
- "Editor Windows-only for now, C++26 + Dear ImGui" (amended by D26: Windows and Linux) -> §11, D26.
- "Lua vs JS, your pick" → D5/D6.
- "Open format, single compressed file, shaders translated at export, GLSL base" → §8.
- "Min Android = my phone (Realme 6)" → D10/D11, §4.1, §10.2.
- "Interact with literally anything, author decides per scene" → §9.3.
- "60 fps, 90/120 if possible, user picks quality & speed" → D14, §10.1, §8 (`settings.json`).
- "Editor must be powerful and complete" → §11.2.
- "All assets incl. video; audio and physics needed" → §7, §12.
- "Library + per-scene settings + lock screen; cutscene-grade scenes" → §10.1, §11.2.
- "Open source, APK now, Play later" → §19.
- "Arabic + English with RTL" → §13.
- Repo and CI from the start (the earlier plan built locally first) -> D31, §14.2, §17 M0.
- "No files until done; message me when big parts complete" → §17 header, D23.
- "Nothing on my phone mid-project; phone testing at the very end" → D23, §17 M10 gates.
- (64-bit only, Android and Windows) → D25, §10.2, §16.1, §16.4.
- "Fingerprint/FaceID interaction" (early message) → §3.3 limitation + lock/unlock substitute.
- "Name may change later" → §1 header rule.
- "The editor runs on Windows and Linux, not Windows only; the live wallpaper itself also runs on Linux" → D26, D27, §4.4, §10A, §11, §16, §17 (ML, G7).
- "Anything you would build from scratch where an open-source cross-platform library exists, use it" → D30, §14.1.
- "Use our own RHI since it is the better option" → D9 reaffirmed.
- "Repo and CI available from the start, test freely, edit the plan freely, use more reviewers" -> D31, §14.2, §17.
