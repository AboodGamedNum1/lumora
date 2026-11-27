# Hex

`core/hex/hex.h` and `hex.cpp` (`lumora::hex`) are strict hexadecimal text for small binary blobs (digests, ids): `encode` and `decode` are one-shot functions over caller buffers.

Evidence label: unit tests only (local runs; CI is reported separately in the pull request).

Integer only: no floating point, no libm, no 128-bit types, no allocation, no recursion, no mutable statics. The source includes only the header, which includes `<cstdint>`.

## Rule

- Alphabet: `0-9` = 0..9, `a-f` and `A-F` = 10..15. Each byte is two characters, high nibble first. No prefix, no separators, no whitespace.
- `encode`: writes `2 * n` characters, lowercase when `upper` is false, uppercase when true (digits `0-9` are the same either way). No terminator. Empty input gives empty output.
- `decode`: both cases are accepted and may be mixed (`aB` is valid). Any other character is `Malformed`. Empty input gives `Ok` with length 0.

## Check order

- `encode`: `out_len` null, `src` null with `n > 0`, `dst` null with `dst_cap > 0`: `NullArgument`. Then `n > kMaxInput`: `TooLarge`. Then `2 * n > dst_cap`: `BufferTooSmall`.
- `decode`: the same three null checks, then `n > kMaxEncoded`: `TooLarge` (nothing read), then odd `n`: `Malformed` (nothing read), then the scan of every character from first to last (nothing written), then `n / 2 > dst_cap`: `BufferTooSmall`. An odd `n` above `kMaxEncoded` is `TooLarge`; an odd `n` at or below it is `Malformed`.
- On any failure `*out_len` and `dst` are untouched.

## Decisions

Confirmed by the maintainer (relayed through the coordinating agent) for the numbered readings: the check orders above exactly as the header states them; the scan covers every character and wins over `BufferTooSmall`; `dst_cap == 0` with a null `dst` and `n == 0` with a null `src` are allowed; mixed case, no prefix, no whitespace and no `=`; characters compared as unsigned bytes, so a negative `char` is outside the alphabet. `src` and `dst` must not overlap; this is not checked and not tested.

## Bounds

| Item | Bound |
| --- | --- |
| raw size | 0 to 134217728 bytes (`kMaxInput`) |
| encoded size | 0 to 268435456 characters (`kMaxEncoded`), even |
| aliasing of `src` and `dst` | not supported, not tested |

## Tests

`tools/hex_ref.py` is an independent reference in Python. It splits each byte into nibbles and uses digit tables, and has its own FNV-1a 64. The standard `binascii` module is used only as a second check inside the script (an assertion). It prints the generated tables pasted into the tests: 37 encode cases (lowercase and uppercase) and 125 decode strings.

`tests/hex/hex_tests.cpp` has 9 test cases and 342328 assertions in the last local run: reference digests for the encode cases (five buffer sizes each, plus a round trip) and the decode strings (nine buffer sizes each); known vectors and mixed case; null arguments, check order and the no-write rule with sentinel buffers; all 256 byte values in both cases; every byte value at every position of a decode string and the value of every digit in the high and low nibble; the size limits with a small dummy buffer; and a structural oracle on random buffers (round trip, random case flips, one bad character, odd length). Buffers are exact size, so ASan sees any overrun.

Local run: g++ with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined`, no warnings, all passed. No other compiler was run.

## Not run

By the maintainer's decision, no buffer of 128 MiB or 256 MiB is allocated. Two cases are therefore omitted:

- A successful encode or decode at the full limit (`n = kMaxInput`, or 268435456 characters).
- A decode of exactly 268435456 characters, which needs a full scan, including the zero-capacity case.

The limits are tested only where the function returns before reading: `encode` at `n = 134217728` with a small capacity gives `BufferTooSmall`, and 134217729 gives `TooLarge`; `decode` with an odd length of 268435455 gives `Malformed`, and 268435457, 268435458 and 4294967295 give `TooLarge`. The dummy source stays untouched in each.

## Mutation check

95 hand-written source mutants of `hex.cpp` (each range bound and value of the three alphabet blocks, character masking, every null and size check, the order of checks, the limits, the odd test, the scan range and result, the digit tables and the nibble order, the loop bounds, the decode arithmetic and the output length) were built against the unchanged tests under ASan and UBSan. 90 were killed and 5 survive.

Survivors:

- Signed char: reading the character as `unsigned` instead of `unsigned char`. A negative `char` becomes a value of at least 0xFFFFFF80, which matches no alphabet range, exactly like the byte value of at least 128. Equivalent whether `char` is signed or unsigned.
- Encode size in 32 bits: after the `n > kMaxInput` check, `2 * n` is at most 268435456, which fits in 32 bits.
- Decode limit `>=` instead of `>`: differs only at `n = 268435456`, the case that is not run by the maintainer's decision (it needs a 256 MiB source for the scan). Not equivalent. It is not killed here, and the condition is stated: only a 256 MiB test would show it.
- Decode size `(n + 1) / 2`: odd `n` is already rejected before this line, so `n` is even and the result is the same.
- `(hi << 4) + lo` instead of `|`: `hi` and `lo` are at most 15, so the low 4 bits of `hi << 4` are zero and addition equals or.
