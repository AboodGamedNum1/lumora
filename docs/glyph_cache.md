# Glyph cache index

`core/glyphcache` maps a glyph raster key (font id, glyph id, size in pixels, quarter-pixel bin) to a 32-bit value chosen by the caller,
for example an atlas slot. It has a fixed capacity of 128 entries, a linear search, least-recently-used eviction and no pixels. The
rules are in the comments of `core/glyphcache/glyph_cache.h`; this page adds the bounds, the bookkeeping, the decisions and the tests.

Everything is integer arithmetic. There is no heap allocation, no recursion, no mutable static state, no hashing, no clock and no
128-bit type. Recency is an internal counter (`tick`). Evidence label: local unit tests only.

## Bounds

- Every call scans at most 128 entries once (find, the present-key check, one scan for a free slot or for the eviction candidate),
  so the work per call is bounded by the capacity.
- `tick` and the three counters saturate at `UINT64_MAX`; a saturated tick is stored as `last_used`. Entries then share a `last_used`
  value and the lowest index decides a tie.
- `subpixel_bin` is bits 6 and 7 of the value as a `uint32` (`(pen & 255) >> 6`), so negative pens floor toward minus infinity: -1 is
  bin 3, -64 is 3, -65 is 2, and `INT32_MIN` is 0. No negative value is shifted.

## Bookkeeping

- A key is valid when `subpixel <= 3` and `size_px >= 1`. Two keys are equal when all four fields are equal. Only entries with
  `used == true` take part in any search.
- `cache_find`: null cache, null value or an invalid key returns false and changes nothing, counters included. A hit sets `*value`,
  adds 1 to `tick`, sets that entry's `last_used` to the new tick and adds 1 to `hits`. A miss only adds 1 to `misses`; the tick and
  `*value` stay.
- `cache_insert`: a null cache gives `NullCache` before the key is looked at, an invalid key gives `BadKey`; both change nothing. If the
  key is present the value is replaced, the tick advances and the entry's `last_used` becomes the new tick (`Updated`); the count and
  all counters stay. Otherwise, when `count < 128` the entry goes to the lowest unused slot and `count` grows by 1 (`Inserted`).
  Otherwise the used entry with the smallest `last_used` (the lowest index on a tie) is replaced, `evictions` grows by 1 and `count`
  stays (`Evicted`). `Inserted` and `Evicted` also advance the tick, and the new entry gets `last_used` = tick. Insert never changes
  `hits` or `misses`.
- `cache_clear` sets every entry to its default state (zero key, value and `last_used`, `used` false) and the tick, the count and the
  three counters to 0.
- `cache_size` returns the stored `count`, not a recount and not clamped.
- `cache_digest`: FNV-1a 64 over 32-bit little-endian words, byte by byte. For each used entry in slot order: font id, glyph | size << 16,
  subpixel, value, `last_used` low and high; then hits, misses and evictions, each low then high. A null cache gives the offset basis.
  The count and the tick are not hashed.

## Hand-built caches

The used flags are the truth for find, the present-key check and the digest. Insert trusts `count` only to choose between a free slot and
an eviction: with `count < 128` but no unused slot it evicts, and with `count >= 128` it evicts even when unused slots exist (an unused
slot is never preferred then). Eviction candidates are used entries only; when there is no used entry at all, slot 0 is replaced. These
cases return `Evicted`, add 1 to `evictions` and leave `count` as it is. `Inserted` never takes `count` above 128, because it only
happens when `count < 128`.

## Tests

`tests/glyphcache/glyph_cache_tests.cpp` replays small programs on the cache and compares the result codes, the final digest, the count,
the tick and the three counters with constants from `tools/glyph_cache_ref.py`. The reference is a separate model: 128 slots of
(key, value, last used), its own tick and counters, its own FNV. The cases are:

- 14 scenario programs: LRU order with refreshes and an update, invalid keys, each key field in equality, clear, the tick near the
  maximum (ties by lowest index), a full cache with a saturated tick, saturating counters, and hand-built caches (count too low, count
  too high, no used entry, used flag off, equal `last_used` values, slot 0 unused);
- three seeded random corpora (seeds 0xC1, 0xC2, 0xC3) over key universes of 200, 130 and 40 keys with invalid keys and clears mixed in,
  with a digest checkpoint every 64 operations;
- `subpixel_bin` at every bin edge, both signs, the extremes, and a sweep from -1024 to 1024;
- null arguments, invalid keys and the valid extremes of a key, with nothing changed on an error;
- hand-computed bookkeeping (tick, `last_used`, counters, update, clear), eviction order on a full cache, and saturation.

A mutation run over the module (80 mutants of the validity and equality rules, saturation, the bin, clear, find, the insert order,
the free slot and eviction rules, `cache_size` and every part of the digest) was caught except one that cannot change a result: making
the `Inserted` branch add to `count` without the `count < 128` bound (the branch is only reached when `count < 128`).
