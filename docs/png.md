# PNG codec

`core/image/png.h` and `core/image/png.cpp` decode and encode PNG images for scene textures. The decoder runs on
untrusted input from `.lumora` packages. This page is the exact behaviour. The interface is in `png.h`.

## Supported features

- Colour types 0 (grey), 2 (RGB), 3 (palette), 4 (grey and alpha), 6 (RGBA).
- Bit depths 1, 2, 4, 8 and 16 as PNG allows per colour type. Output is always straight (non-premultiplied) RGBA8.
- 16-bit samples are reduced to 8 bits by taking the high byte, without rounding. Grey samples of 1, 2 and 4 bits
  scale to the full range (value times 255 divided by the maximum value).
- Interlace method 0 and Adam7 (method 1).
- `tRNS` for colour types 0, 2 and 3. Grey and RGB use a colour key compared with the full sample value (16 bits
  at depth 16). Palette alpha applies to the first entries. Entries without alpha are opaque.
- Gamma, colour profile, text, time and all other ancillary chunks are CRC-checked and then ignored.
- Animated PNG: the default image (the IDAT data) is decoded. `acTL`, `fcTL` and `fdAT` are CRC-checked and ignored.
- The encoder writes colour type 6, depth 8, no interlace, with `IHDR`, one `IDAT` and `IEND` only.

## Exact rules

Chunks:

- The 8-byte signature comes first. Every chunk is length, four ASCII letters, data, CRC-32. The CRC of every chunk
  is verified, critical or not, before the chunk is looked at. A chunk length above 2^31-1 is `BadChunk`.
- `IHDR` is the first chunk and appears once, with length 13. `IEND` is last, has length 0, and nothing may follow
  it. At least one `IDAT` is required.
- `IDAT` chunks form one contiguous run. Empty `IDAT` chunks are allowed. Their data is joined before inflating.
- `PLTE` appears at most once, before the first `IDAT` and before `tRNS`. It is rejected in colour types 0 and 4.
  In types 2 and 6 it is validated (length a non-zero multiple of 3, at most 256 entries) and ignored. In type 3 it
  is required and may not have more entries than the bit depth allows. Fewer entries than the depth allows are fine.
- `tRNS` appears at most once, before the first `IDAT`, never in colour types 4 and 6, and after `PLTE` in type 3.
  Its length is 2 for type 0, 6 for type 2, and at most the palette size for type 3.
- An unknown chunk whose first letter is upper case (critical) is `BadChunk`. An unknown ancillary chunk is ignored.

Stream:

- The IDAT data is a zlib stream: method 8, window of at most 32 KB, valid header check bits, no preset
  dictionary (`FDICT`). The raw DEFLATE stream comes from `core/package/deflate.h`. It must end exactly where the
  4-byte Adler-32 begins, and the Adler-32 of the inflated data must match.
- The inflated size must be exactly the size the header implies: for every row, one filter byte plus the packed row,
  and for Adam7 the seven passes in order (empty passes have no rows and no bytes). Filter types 0 to 4 only.
- Inflation is capped at that exact size, so a longer stream stops at the cap.

Limits (checked from the header values before anything is allocated):

- Width and height above `max_dim` (default 8192) or width times height above `max_pixels` (default 16 Mi pixels)
  is `TooLarge`. A size too large to address on the platform is also `TooLarge`.
- A zero size or a size above 2^31-1 is `BadHeader`.
- A huge size in a tiny file therefore fails from the header alone, or, with generous limits, when the short
  stream ends. No buffer of the claimed size is created.

## Status table

| Status | When |
| --- | --- |
| `Ok` | Decoded. The image is valid RGBA8. |
| `NotPng` | Empty input, or the first bytes are not the PNG signature. |
| `Truncated` | The data ends inside the signature, a chunk, or before `IEND`. Every proper prefix of a valid file gives this. |
| `BadChunk` | Chunk order or uniqueness is broken, `IEND` is not last or has data, bytes follow `IEND`, a type is not four letters, a length is above 2^31-1, an unknown critical chunk, `PLTE` in a grey image, `tRNS` with an alpha channel, `IHDR` length not 13. |
| `BadCrc` | A chunk CRC-32 does not match. |
| `BadHeader` | Zero or too large size field, bad colour type and depth combination, compression, filter or interlace method not 0 or 1. |
| `BadPalette` | Missing `PLTE` for type 3, bad `PLTE` length or too many entries, an index beyond the palette, a bad `tRNS` length. |
| `BadZlib` | Bad zlib header, `FDICT`, window above 32 KB, malformed DEFLATE data, the stream does not end at the Adler-32, Adler-32 mismatch, zlib stream too short. |
| `BadFilter` | A filter type above 4, or the inflated size is not the size the image needs (shorter, or longer than the cap). |
| `TooLarge` | Width, height or pixel count above the limits. |

On any status except `Ok` the image is empty (`width` and `height` are 0). The message is a short fixed text and
never contains input bytes.

## Encoder and memory

`encode_png(image, level)` returns an empty vector for an invalid image (zero size, wrong buffer size, a side above
2^31-1). Otherwise, for each row it tries all five filters and keeps the one with the smallest sum of absolute
differences (each byte difference read as signed). On a tie the lowest filter number wins. The rows are compressed
with `deflate_raw` at `level` clamped to 0 to 9, in a zlib stream with a 32 KB window header (the compression-level
hint follows the level), and written as a single `IDAT`. Output is deterministic: the same image and level give the
same bytes.

Peak memory of a decode is the inflated data plus the output image (plus the joined `IDAT` data, which is freed
before the output is allocated and is never larger than the file). For an 8-bit RGBA image that is about twice the
size of the pixels. At the default limits the output is at most 64 MB. The encoder holds the input, the filtered
rows and the compressed data.

## Evidence limits

The tests use fixtures built by a test-only PNG writer in `tests/image/png_fixtures.h`. They show that the decoder
agrees with that writer and with the rules above. They do not show compatibility with every PNG producer, and the
codec is not claimed to pass the whole PNG test suite. CI runs on Linux and Windows hosted runners and Android
emulators, on the CPU only. Coverage-guided fuzzing (`tools/fuzz/fuzz_png.cpp`) runs separately from CI.

## Maintainer notes

- Run the tests with `ctest` (`lumora_image_tests`, `fuzz_png_replay`).
- Do not relax the order of the checks: CRC first, then chunk rules, limits from the header, then inflate with the
  exact cap.
- A new fuzz seed goes in `tools/fuzz/corpus/png/` (ASCII name, at most 4 KB). The corpus is replayed on every lane.
