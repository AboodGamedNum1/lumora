# CPU reference renderer golden images

Golden images for the CPU reference renderer in `core/render2d`. The conventions are in
`core/render2d/cpu_renderer.h` and `docs/render2d.md`. This page covers the baselines only. The GPU renderers
have their own baselines (see `docs/golden-images.md`) and are never compared with these.

## Layout

- Test: `tests/render2d/golden_cpu_tests.cpp`, Catch2 tags `[render2d][golden]`, part of `lumora_render2d_tests`.
- Baselines: `tests/golden/cpu-reference/<scene>.ppm`, binary P6, each at most 64 KB, about 300 KB in total.
- Every scene is built in code from tiny in-memory textures and rendered by `render_scene`. The helper is
  `tests/golden.h`, used unchanged.
- Comparison: per channel, tolerance 2 for every scene. A pixel fails when any channel differs by more than 2.

## Scenes

Textures: `grid` is 4x4, red rises from 30 to 210 to the right, green rises from 30 to 210 downwards, blue is 128,
so the top-left is dark blue and the bottom-right is pale yellow. `quad` is 2x2: red top-left, green top-right,
blue bottom-left, white bottom-right. Other textures are solid colours. A scene fills the whole view unless the
line says the layer is smaller. Texels blend smoothly (bilinear), and a layer edge is a hard cut.

| Scene | View | Expected appearance |
| --- | --- | --- |
| `identity_grid` | 64x64 | The grid fills the view with smooth colour ramps. |
| `scale_down_half` | 64x64 | The grid at half size in the centre on a dark blue-grey clear colour. |
| `scale_up_2x` | 64x64 | Only the centre quarter of the grid, magnified to fill the view, softer ramps. |
| `scale_nonuniform` | 64x64 | The grid at 75% width and 40% height, centred, on dark blue-grey. |
| `rotate_45` | 64x64 | A diamond. The dark blue corner points left and the pale yellow corner points right. |
| `rotate_90_wide` | 96x48 | The `quad` rotated 90 degrees counter-clockwise in a wide view, so it is stretched wider. Red bottom-left, green top-left, blue bottom-right, white top-right. |
| `translate_clip` | 64x64 | The grid moved right and down so its right and bottom sides are cut by the view. Dark red clear colour shows on the left and top. |
| `layer_order` | 64x64 | Red, green and blue squares stepping from top-left to bottom-right, each one drawn over the one before. Pale grey clear colour. |
| `preorder_nested` | 64x64 | A red square top-left, a green child square at its lower right, and a small blue square that sits over both. This shows pre-order: parent, child, then the later sibling. |
| `parent_child_transform` | 64x64 | A small grid rotated 20 degrees counter-clockwise with a tiny `quad` child at its right side. The child is scaled and rotated with its parent. |
| `alpha_ramp` | 64x64 | A yellow strip over the grid, fully transparent on its left and fully opaque on its right. |
| `alpha_stack` | 64x64 | Three half-transparent squares (red lower left, blue lower right, green top) on white. Overlaps show the mixed colours, which depend on draw order. |
| `parallax_x_pos1` | 96x64 | Blue background (depth 1), green bar (depth 0.5), red bar (depth 0), `offset_x` = +1. The red bar moves right by about 5 px, the green bar by about 2 px, the background does not move. |
| `parallax_x_neg1` | 96x64 | The same scene with `offset_x` = -1. The bars move left by the same amounts. |
| `parallax_y_half` | 96x64 | The same scene with `offset_y` = +0.5. The bars move up by about 2 px (red) and 1 px (green). |
| `parallax_zero` | 96x64 | The same scene with no offset. Bars sit at their node positions. |
| `view_tall` | 48x128 | The `quad` stretched over a tall, narrow view. |
| `view_1x1` | 1x1 | One grey-blue pixel, the average of the four centre texels (120, 120, 128). |
| `view_large` | 128x160 | The grid scaled to 0.9, rotated 15 degrees, moved right and down, in a non-square view, with clipping on the right and bottom. |

Scenes are stretched in non-square views because the viewport always spans [-1, 1] on both axes.

## Tolerance policy

- The tolerance is 2 per channel. No scene needs more.
- A scene that needs more than 2 must say why in one line in this page. Nobody raises a tolerance silently to make a
  difference pass.
- If another platform produces a difference above 2, report the scene, the platform and the delta. The project
  owners decide whether it is a renderer problem, a tolerance reason, or a separate baseline.

## Blessing a baseline

1. Run `LUMORA_BLESS_GOLDEN=1 ctest -R lumora_render2d_tests` (or run the test binary with the same variable set).
   This writes the actual images over `tests/golden/cpu-reference/`.
2. Look at every changed image. Compare it with the expected appearance above, not with what the renderer happened
   to produce. A wrong image is a renderer bug, not a baseline. Do not bless it.
3. Review the diff of the `.ppm` files and this page together.

`tools/golden/ppm_diff.py A.ppm B.ppm [--tolerance N]` compares two files and prints the sizes, the number of bad
pixels, the largest delta and the bounding box of the bad pixels. `python3 -m unittest discover -s tools/golden`
runs its tests.

## Reading a failure

On a failure or a missing baseline, the test writes `<scene>.actual.ppm`, and for a pixel mismatch also
`<scene>.diff.ppm`, to `golden-out/cpu-reference/` inside the build directory. In the diff image, a red pixel is a
pixel beyond the tolerance. To see how far off it is, run `ppm_diff.py` on the baseline and the actual image. A
bounding box that hugs one object points to a change in that layer. A whole-image difference points to a change in
sampling or blending.

## Other tests in the same file

- Every scene renders twice with byte-identical output, including the encoded PPM.
- A missing baseline fails and writes the actual image.
- A one-channel change of tolerance + 1 fails, a change of exactly the tolerance passes. These tests use temporary
  folders and never touch the real baselines. They are skipped when `LUMORA_BLESS_GOLDEN=1` is set.

## Evidence limits

These baselines are CPU software rendering evidence. They check pixels from the CPU reference renderer on the
platforms where CI runs the tests. They say nothing about GPU, device or phone behaviour.
