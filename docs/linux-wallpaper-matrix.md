# Linux wallpaper method matrix (GNOME spike, source review)

Status: source review only. Everything here comes from reading public source and pages. **Nothing is verified on a real GNOME desktop, and no nested-shell run has been done yet.** Label: unverified on real hardware. Open questions OQ6 and OQ7 stay open.

## Goal

Find a way to show a Lumora renderer as the desktop background on GNOME (Wayland): below all windows, absent from Alt-Tab and the dock, input-through, one surface per monitor, and stable across extension enable and disable, sleep and wake, and lock.

## Reference studied: Hanabi (GNOME Shell extension)

- Source: https://github.com/jeffshee/gnome-ext-hanabi, files `src/windowManager.ts`, `src/waylandSubprocess.ts`, `src/wallpaper.ts`, `src/extension.ts`. Reviewed at the main branch around merge commit 7b680b6 (pull request 238, lock screen support).
- License: GPL-3.0. **Lumora is Apache-2.0 compatible only. The source was read to understand the method. No code is copied, and none may be.**
- Supported shells per its README: GNOME 50 and later on the main branch (TypeScript, Wayland only), GNOME 45 to 50 on a maintenance branch (X11 and Wayland).

### How it works (as read from the source)

1. **Renderer is a separate process.** The extension starts the renderer as a Wayland client subprocess through Mutter's `Meta.WaylandClient.new_subprocess`. This gives the shell a handle that answers `owns_window(window)`, so the extension can tell which toplevels are its renderer windows without matching on names alone.
2. **One renderer window per monitor.** The extension looks for renderer windows (matched by an application id in the window title and by ownership), requires at least as many windows as monitors, rejects duplicates on the same monitor, then picks the window whose monitor index equals the background actor's monitor.
3. **The window is placed inside the shell's background actor.** Instead of leaving the toplevel on screen, the extension takes the renderer window's compositor actor and adds it as a child of the per-monitor background actor (`Meta.BackgroundActor`). The desktop background is then the live window content. The same approach is used for the lock screen in newer commits.
4. **Window hygiene by signals.** A managed-window object listens for `raised` (calls `lower()` when it must stay at the bottom), `notify::above` (calls `unmake_above()`), `notify::minimized` and `shown` (keeps the window minimized when asked), and `position-changed` (moves it back). It also forces a minimize again after wake from suspend, because Mutter can leave the window visible and clickable.
5. **Control channel through the window title.** The renderer encodes state as JSON in its title with an application-id prefix. The extension parses it on `notify::title`.
6. **Window actor access.** Renderer windows are hidden from the public window actor list, so the extension injects an unfiltered accessor by overriding shell internals (`gnomeShellOverride.ts`). This is the most version-fragile part.
7. **Pause logic** exists in a separate module (`autoPause.ts`), outside the scope of this review.

## Method matrix

| Method | Idea | What the source review says | Risk for Lumora | Status |
|---|---|---|---|---|
| A. Shell extension hosts a renderer subprocess and embeds its window actor in the background actor | Hanabi's method | Works on GNOME 45 and later per the project's own support table. Needs overriding shell internals to find the actors | Breaks on shell releases (GNOME releases twice a year), extension review and install onboarding, GPL code cannot be reused, so Lumora writes its own | Unverified, source only |
| B. Normal window with a "wallpaper" mode | Borderless full-screen window kept at the bottom by the user's compositor rules | No special shell hook needed | Not a real background: shows in Alt-Tab, takes input, tiles | Fallback defined in docs/milestones.md |
| C. Layer-shell background surface (wlr-layer-shell) | A client asks the compositor for a layer surface below windows | Mutter source not reviewed yet, so support on GNOME is unknown | Unknown until Mutter is checked for the protocol | Not reviewed, to check headless |
| D. Set the static wallpaper through GSettings | Write a rendered image to the background setting | No live content | Only a static fallback | Not pursued |

## What a Lumora implementation would need (from the review, not tested)

- A renderer program that opens one toplevel per monitor with a stable application id, plus a small extension that finds those windows by process ownership, embeds or lowers them, and pauses on lock and sleep.
- An install and enable flow for the extension, version checks against the installed shell, and a clear message when the shell is too new.
- Wayland-only focus is acceptable per the plan. X11 is not a target.
- Fallback B stays defined: if the extension path fails or is not acceptable, the Linux runtime ships as a normal-window mode with the limitation written down. If the spike fails, work stops at the M0-M1 boundary and the owner decides.

## Still to do for the spike

1. Run a nested GNOME Shell in CI (headless) with a test extension of our own that embeds a trivial client window, and record what Mutter reports. Label: nested-shell evidence, not real-hardware evidence.
2. Check whether the Mutter version in the CI image exposes a layer-shell style protocol (method C), headless.
3. Record the GNOME Shell and Mutter versions used. Do not claim the result for Ubuntu 26.04 or 26.10 until a matching image is tested.
4. Real-session validation stays with the owner's own GNOME (gate G7, open question OQ6 and OQ7).
