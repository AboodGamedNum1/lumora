# Texture atlas packer

`core/image/atlas.h` and `core/image/atlas.cpp` pack many RGBA8 images into one RGBA8 image and describe where each
one went. `tools/atlas` is a command line tool, `lumora-atlas`, that does this for PNG files. Everything is pure CPU
integer code: no GPU, no file system in the library, no dependencies. The interface is in `atlas.h`.

## Rules

- Inputs are `AtlasInput` values: a name and a valid `Rgba8` image. Results keep input order, so `entries[i]`
  belongs to `inputs[i]`.
- Names are unique (case sensitive), 1 to 128 bytes, valid UTF-8, with no control characters (bytes below 0x20 and
  0x7F). Valid UTF-8 is required so the JSON index is always valid text.
- Limits: `max_size` 1 to 16384 (width and height cap), `padding` 0 to 16, `max_inputs` at least 1, and
  `power_of_two`. With `power_of_two` the atlas width and height are powers of two and the usable cap is the largest
  power of two not above `max_size` (for example 64 when `max_size` is 100).
- Padding: every sprite is surrounded by a ring of `padding` pixels that repeats its border pixels. The corner
  squares repeat the corner pixel. The ring belongs to the sprite's cell and never overlaps another cell. Entry
  rectangles (`x`, `y`, `w`, `h`) are the sprite itself and exclude the padding, so `x >= padding` and
  `y >= padding`. Atlas pixels that no cell covers are transparent black (all four bytes 0).
- Sprites are not rotated or trimmed.
- All size arithmetic is 64-bit and is checked against the limits before any buffer is allocated. Each input is
  compared with the cap before any multiplication, and the running area is compared with the cap squared as it grows.
- On any failure `image` is empty (width and height 0), `entries` is empty, `message` is a short fixed text, and
  `atlas_index_json` returns an empty string.

## Status table

| Status | When |
| --- | --- |
| `Ok` | Packed. `image` and `entries` are set. |
| `BadLimits` | `max_size` outside 1 to 16384, `padding` above 16, or `max_inputs` 0. Checked first. |
| `NoInputs` | The input list is empty. |
| `TooManyInputs` | More inputs than `max_inputs`. |
| `BadInput` | An empty, too long, control-character or invalid UTF-8 name, a duplicate name, or an image that is not `valid()`. |
| `DoesNotFit` | The sprites cannot be placed inside the cap (see the bound below), or the atlas image could not be allocated. |

Checks run in this order: limits, empty list, input count, every name and image, duplicates, then fit. A bad input is
therefore reported even when another input is also too large.

## Algorithm

Skyline bottom-left, all in integers:

1. Each sprite becomes a cell of `(w + 2*padding) x (h + 2*padding)`.
2. Cells are placed in this order: height descending, width descending, input index ascending. The last key makes the
   order total, so no sort ever depends on a pointer, a hash or an unstable sort.
3. The packer keeps a skyline: a list of segments `(x, y, width)` from left to right, with equal-height neighbours
   merged. For one cell it tries every segment start `x` from left to right. The cell's `y` there is the highest
   skyline point under its width. It must fit under the height cap. The lowest `y` wins, and on equal `y` the smallest
   `x` wins (the first one found, because `x` grows).
4. The skyline is raised over the placed cell and the next cell is placed.

The result is byte-identical on every host and compiler, because nothing depends on floating point, pointer values,
hash order or the platform's sort stability.

### Smallest-atlas search

The packer does not just use `max_size`. It packs into strips of several candidate widths, each with the height cap,
and keeps the smallest result:

- Candidate widths without `power_of_two`: the widest cell, `max_size`, and `ceil(s * f)` for the factors
  1, 21/20, 11/10, 6/5, 27/20, 3/2, 7/4, 2, 3 and 4, where `s` is the integer ceiling of the square root of the total
  cell area. Each is clamped between the widest cell and `max_size`. The list is sorted and de-duplicated, so there are at
  most 12.
- Candidate widths with `power_of_two`: every power of two from the one at or above the widest cell up to the cap.
- For each width the final atlas is the used extent: the right edge of the rightmost cell by the top edge of the
  highest cell, each rounded up to a power of two when `power_of_two` is set.
- The smallest area wins. On equal area the smaller longest side wins, and on equal again the narrower strip (it
  comes first). This is a total order, so the choice is deterministic.

### Bound for `DoesNotFit`

`DoesNotFit` is returned only when one of these holds:

- some cell is wider or taller than the cap;
- the total cell area is larger than the cap squared (no packing can exist);
- no candidate width yields a packing at or under the height cap, or the atlas image cannot be allocated.

The last case is a heuristic limit. A different algorithm might find a layout the skyline misses, so `DoesNotFit` there
means "this packer found none", not "none exists". One guarantee is exact: if every cell fits the cap's width and the
cell heights add up to at most the cap, the result is always `Ok`, because stacking the cells in one column is always
one of the positions the skyline tries. The result never exceeds the cap in either direction.

### Shuffled inputs

Placement depends on the cell sizes and the input index only. Any permutation of the inputs gives the same set of
rectangles and the same atlas size. Equal-size sprites take the slots in input order, so swapping two equal-size inputs
swaps their positions (and names), and the atlas pixels change only where the sprites' contents differ.

### Complexity

With `n` inputs, `K <= 12` candidate widths and `S` skyline segments (`S <= n + 1`): placing one cell scans up to
`S` start positions and up to `S` segments for each, so a trial is `O(n * S^2)` worst case and `O(n * S)` typically,
and the search is `K` trials. Sorting is `O(n log n)`. Rendering is linear in the output size. Peak memory is the output
image (`width * height * 4` bytes, up to 1 GiB at 16384 by 16384) plus the cell list and one skyline. Nothing is
allocated for the output until a packing has been found.

## JSON index

`atlas_index_json` returns compact JSON with a fixed key order and no whitespace:

```
{"width":W,"height":H,"entries":[{"name":"...","x":X,"y":Y,"w":W,"h":H}]}
```

Entries are in input order. Names escape `"` and `\`. Control characters cannot occur in names. All other bytes are
copied (they are valid UTF-8).

## Command line

```
lumora-atlas --out atlas.png --index atlas.json [--max-size N] [--padding N] [--pot] input1.png input2.png ...
```

- A sprite name is the base file name without its last extension (`art/tree.v2.png` becomes `tree.v2`). Two inputs
  with the same base name are an error.
- Defaults are the `AtlasLimits` defaults (`--max-size 4096`, `--padding 2`, no `--pot`).
- Inputs are decoded with the default PNG limits. Bounds: at most 4096 files, 32 MiB per file, 256 MiB of files and 64 Mi
  decoded pixels in total.
- Exit codes: 0 success, 1 packer or input error (unreadable or oversize file, bad PNG, any packer status including
  out-of-range numbers, an output that cannot be written), 2 usage error (missing or unknown option, a value that is not
  a number, no inputs, the same path for `--out` and `--index`).
- Output files are written only after everything succeeded: both are written to `<path>.tmp` first and then renamed.
  A failure before the renames leaves existing files untouched. The two renames are not one atomic step, so a failure of
  the second rename (very rare) can leave the new atlas next to an old index.

## Evidence limits

Tests (`tests/atlas`) run on the CPU only and use fixtures made in the tests. They cover pixel-exact sprites and
padding, non-overlap over 240 fixed-seed random sets, determinism, every status with several triggers, boundary
sweeps, the JSON text parsed back with the repository parser, a 1000-sprite performance case, and the command line
logic in-process. CI runs on Linux and Windows hosted runners and Android emulators. There is no GPU test and no claim
about GPU texture limits, filtering or mipmaps: the padding ring is plain edge extrusion, and how much padding a given
renderer needs is not tested here.

## Maintainer notes

- Run the tests with `ctest -R atlas`.
- Do not change the sort order or the tie-breaks without updating this page and the exact-string tests: they define the
  output bytes.
- Memory for a 16384 by 16384 atlas is 1 GiB. Callers that must stay small should lower `max_size`.
