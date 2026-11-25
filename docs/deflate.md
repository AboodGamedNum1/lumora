# DEFLATE module (core/package)

Dependency-free raw DEFLATE (RFC 1951, ZIP method 8) used by the .lumora
package format. The ZIP layer wires `inflate_raw`/`deflate_raw` into the
reader and writer; this module owns only the codec.

## Layout

- `deflate.h` — public interface (`inflate_raw`, `deflate_raw`,
  `InflateStatus`). Owned by the project; not edited by the codec work.
- `deflate.cpp` — hardened inflate (decoder).
- `deflate_compress.cpp` — deflate (compressor) for levels 0, 1 and 6.
- `deflate_common.h` — shared RFC 1951 constants (length/distance tables,
  fixed code lengths, bit helpers).

## Decoder hardening

`inflate_raw` runs on untrusted input:

- Every bit read is bounds-checked; a read past the input end is `Truncated`,
  never an out-of-bounds access.
- Output growth is checked against `max_output` before it happens; capacity
  never exceeds `max_output`, so decompression bombs stop at `OutputLimit`.
- Malformed streams return a status (`BadBlockType`, `BadStoredLen`,
  `BadHuffman`, `BadSymbol`, `BadDistance`), never a crash.
- Huffman decoding uses a single-level canonical table sized by the actual
  maximum code length. Over-subscribed and incomplete codes are rejected,
  with the RFC 1951/zlib exception that a lone symbol forms a degenerate
  1-bit code. An all-zero distance tree is legal until a distance symbol is
  actually used.

## Compressor determinism and levels

The same input and level always produce the same bytes on every platform:
block boundaries depend only on the token count, hash-chain walks are
bounded, and Huffman ties break on the symbol index.

- level 0: stored blocks (64 KiB chunks).
- level 1: a fixed-Huffman block with a greedy LZ77 matcher.
- level 6: dynamic-Huffman blocks (512K tokens per block) with a lazy
  matcher and a bounded hash-chain search; a block falls back to stored when
  that is smaller.
- Any other level clamps to the nearest of the three (0, 1, 6).

## Tests

`tests/deflate/deflate_tests.cpp` covers checked-in zlib-generated
interop fixtures (stored/fixed/dynamic, empty, one byte, 64 KiB boundaries,
258-byte matches, distance 32768), round trips and determinism at every
level, truncation at every byte offset, bit flips, every error status in
`deflate.h`, `max_output` bombs, and a performance sanity case. CI has no
independent decoder to compare against; the fixtures pin compatibility with
zlib output, and local verification additionally ran this compressor's
output through zlib's decoder.
