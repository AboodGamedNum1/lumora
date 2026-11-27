# Base64

`core/base64/base64.h` and `base64.cpp` (`lumora::base64`) are strict Base64: RFC 4648 standard alphabet, padding required, canonical trailing bits, no whitespace, no URL-safe alphabet. `encode` and `decode` are one-shot functions over caller buffers.

Evidence label: unit tests only (local runs; CI is reported separately in the pull request).

Integer only: no floating point, no libm, no 128-bit types, no allocation, no recursion, no mutable statics. The source includes only the header, which includes `<cstdint>`.

## Rule

- Alphabet: `A-Z` = 0..25, `a-z` = 26..51, `0-9` = 52..61, `+` = 62, `/` = 63, padding `=`.
- `encode`: 3 bytes give 4 characters. A final 2 bytes give 3 characters and `=`, a final byte gives 2 characters and `==`. The unused low bits are 0. The size is `4 * ceil(n / 3)`; empty input gives empty output. No terminator is written.
- `decode`: 4-character groups. `=` is allowed only at the end of the last group: `xx==` or `xxx=`. Anything else outside the alphabet is `Malformed`. Unused low bits must be 0 (low 4 bits of the second character with `==`, low 2 bits of the third with `=`), else `NonCanonical`. Empty input gives `Ok` with length 0.

## Check order

- `encode`: `out_len` null, `src` null with `n > 0`, `dst` null with `dst_cap > 0`: `NullArgument`. Then `n > kMaxInput`: `TooLarge`. Then encoded size above `dst_cap`: `BufferTooSmall`.
- `decode`: the same three null checks, then `n % 4 != 0`: `Malformed`, then `n > kMaxEncoded`: `TooLarge` (nothing read), then the scan of every group from first to last (first problem wins, `NonCanonical` is checked after the characters of the last group are accepted), then decoded size above `dst_cap`: `BufferTooSmall`.
- On any failure `*out_len` and `dst` are untouched. The scan writes nothing.

## Decisions

Applied as given by the maintainer (relayed through the coordinating agent, answering six numbered readings plus the limits):

- Characters are compared as unsigned bytes, so a negative `char` is outside the alphabet.
- The scan covers the whole input, so a bad character in an early group wins over a padding problem in the last group.
- Position 3 may be `=` only when position 4 is `=`; position 4 may be `=` alone.
- Limits: `kMaxInput` 12582912 and `kMaxEncoded` 16777216. A decode length of 16777217 is `Malformed` (not a multiple of 4) and 16777220 is `TooLarge`.
- `src` and `dst` must not overlap. This is not checked and not tested.

## Bounds

| Item | Bound |
| --- | --- |
| raw size | 0 to 12582912 bytes |
| encoded size | 0 to 16777216 characters, multiple of 4 |
| aliasing of `src` and `dst` | not supported, not tested |

## Tests

`tools/base64_ref.py` is an independent reference in Python. It encodes and decodes with explicit bit strings and a position-by-position checker, and has its own FNV-1a 64. The standard `base64` module is used only as a second check inside the script (an assertion), not as the oracle. It prints the generated tables pasted into the tests: 24 encode cases and 125 decode strings.

`tests/base64/base64_tests.cpp` has 10 test cases and 291843 assertions in the last local run: reference digests for the encode cases (five buffer sizes each, plus a round trip) and the decode strings (nine buffer sizes each); the RFC 4648 vectors; null arguments, check order with sentinel buffers and the no-write rule; the limits (12 MiB encode, 16 MiB decode, 16777217 and 16777220); padding forms, trailing bits and error priority; every byte value at every position; and a structural oracle on random buffers. Buffers are exact size, so ASan sees any overrun.

Local run: g++ with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined`, no warnings, all passed. No other compiler was run.

## Mutation check

123 hand-written source mutants of `base64.cpp` (alphabet and value mapping, URL-safe additions, character masking, every null and size check, the order of checks, the limits, the scan conditions, pad detection, both trailing-bit masks and positions, the decode arithmetic and the output length) were built against the unchanged tests under ASan and UBSan. 117 were killed and 6 survive. A first run left the second-character mask mutant (`& 7` instead of `& 15`) alive; the strings `QB==`, `QC==`, `QE==`, `QI==`, `QUE=` and `QUM=` were added and it is now killed.

Survivors, each equivalent under a stated condition:

- Signed char: reading the character as `unsigned` instead of `unsigned char`. A negative `char` becomes a value of at least 0xFFFFFF80, which matches no alphabet comparison and no equality, exactly like the byte value of at least 128. Equivalent when `char` is signed or unsigned (both are outside the alphabet).
- Encode size in 32 bits: after the `n > kMaxInput` check, `4 * ceil(n / 3)` is at most 16777216, which fits in 32 bits.
- Decode size in 32 bits: after the `n > kMaxEncoded` check, `3 * groups` is at most 12582912, which fits in 32 bits.
- Pad character value 1 instead of 0 for the third character: only used when the third character is `=`, in which case one byte is written from `v >> 16`, and the value changes only bits 6 to 11 of `v`.
- Pad character value 1 instead of 0 for the fourth character: it changes only bit 0 of `v`, and at most two bytes are written (`v >> 16` and `(v >> 8) & 255`).
- `*out_len` set from the computed size instead of the written count: on success they are equal by construction (`3 * groups - pad`).
