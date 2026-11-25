# Scripting in the M3S slice

A minimal pull-forward of M5 (Section 9 of the plan): one Lua callback, no engine API beyond read-only settings.
The full engine API, events, per-function budgets and the escape suite stay M5 work.

## Contract

A scene script defines one function:

- `on_parallax(x, y)` receives the parallax input in [-1, 1] and returns two finite numbers. The host clamps them to
  [-1, 1] and uses them as the view offset.

The global `settings` is a read-only view of the resolved setting values (bool, number, enum id string, color as a
read-only table with `r`, `g`, `b`, `a`). The global `lumora.api_version` is 1.

## Sandbox (core/script)

- Lua 5.4.9 is built from source as a static library, as C++, so that Lua errors are C++ exceptions. Only the core
  and the base, math, string and table libraries are compiled. io, os, package, debug, coroutine and utf8 are not in
  the build.
- The script environment is a fresh table filled from an allowlist: assert, error, ipairs, next, pairs, select,
  tonumber, tostring, type, rawequal, rawget, rawlen, getmetatable, pcall, setmetatable, the safe parts of math
  (no random), string (byte, char, len, lower, upper, sub, reverse) and table (insert, remove, concat, unpack).
  The real globals are wiped and the string metatable points at the safe string table and is hidden.
- Binary chunks are refused (text mode only). Sources over 64 KB are refused.
- Every call, and the chunk itself, has an instruction budget (200000 VM instructions by default) enforced by a count
  hook. `pcall` is replaced so it cannot swallow a budget overrun.
- The state has a memory cap (4 MB by default) through a custom allocator. Allocation failure is a script error.
- `setmetatable` refuses `__gc` and `__close`. Lua switches debug hooks off while those handlers run, so the
  instruction budget could not stop a runaway handler.
- Three failed calls in a row suspend the script. A successful call resets the count. Rendering continues without
  the script.

## Limits of this slice

- Software evidence only: the tests run on the CI hosts (Linux with sanitizers, Windows). Nothing here is device
  evidence.
- Native standard-library calls are not bounded by the instruction hook. The exposed ones (concat, unpack, reverse,
  upper, lower) are linear in sizes that the memory cap bounds, and the risky ones (rep, find, match, gsub, format,
  sort, dump) are not exposed. M5 replaces this with the full length-capped wrappers.
- The watchdog and process-level recovery of Section 9.1 are not part of this slice.
