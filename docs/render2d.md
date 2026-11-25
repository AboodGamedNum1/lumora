# CPU reference 2D renderer (core/render2d)

Pure CPU, deterministic renderer for 2D parallax scenes. It is software evidence only. Conventions are
normative in `core/render2d/cpu_renderer.h`; this page records the choices the header leaves open.

## Behaviour

- `decode_ppm`: P3 and P6, maxval 255 only. Header numbers are at most 9 digits. Dimensions must be
  1..max_dim (default 8192). P6 needs exactly one whitespace byte after maxval and exactly `3*w*h` bytes
  after it. P3 allows comments and any whitespace, and only whitespace after the last sample. The pixel
  buffer is allocated only after the input is known to be long enough, so a huge header with a tiny body
  cannot force a large allocation.
- `load_textures`: decodes every asset of kind `texture`; other kinds are ignored. A missing or bad file
  returns false with the asset id in the error and leaves the map empty.
- `render_scene`: view sides must be 1..8192. The node tree is walked depth-first, pre-order, with an
  explicit stack. A pixel centre is mapped back into a layer's quad with the inverse transform. Pixels
  outside the quad are skipped; inside it, coordinates beyond the outer texel centres clamp to the edge
  texel, so the quad edge is a hard cut, not a fade. Layers with a singular or non-finite transform or a
  missing texture are skipped.
- Parallax: translation by `offset * (1 - depth) * kParallaxRange`, added after the node transform.
  Positive `offset_y` moves the layer up (scene y is up).
- Blending: straight-alpha source-over in the 8-bit buffer, rounded to nearest after every layer.
- `encode_ppm`: P6, alpha dropped; an invalid image encodes to an empty vector.

## Tests

`tests/render2d/render2d_tests.cpp` has hand-worked expected pixels for identity, 2x scale, 90 degree
rotation, translation and clipping, parent/child composition, parallax at depths 0, 0.4 and 1 (both
axes), alpha overlap in both orders, an empty scene, the aurora sample at 128x128, repeat-render byte
equality, and hostile PPM input (truncation at every offset, every bit flip, bad maxval, huge numbers,
trailing data, size caps).
