# lumora-scenediff

`lumora-scenediff` compares two Lumora packages and lists what changed. Use it to review a change to a scene, or to
explain why a golden image differs (for example: the parallax depth of one layer moved). It runs on the CPU only.
It uses no Lua, no GPU and no network, and it reads nothing except the two inputs it is given.

Code: `tools/scenediff/` (static library `lumora_scenediff`, executable `lumora-scenediff`, tests in
`tools/scenediff/tests/`). The public API is documented in `tools/scenediff/scene_diff.h`.

## Usage

    lumora-scenediff A B [--format text|json] [--quiet]

`A` and `B` are each a `.lumora` archive or a scene directory. A directory is packed in memory with
`scene::pack_directory` (the same step as `lumora-packer pack`), an archive is read with `scene::read_zip_store`.
Both then go through `scene::load_package`, so the usual checksum, manifest and scene checks and size limits apply.
`A` is the old side and `B` the new side.

| Exit code | Meaning |
| --- | --- |
| 0 | the packages are identical for everything the tool compares |
| 1 | there are differences |
| 2 | an error: a package does not load, an input cannot be read, or the command line is wrong |

- `--format text` (default) and `--format json` choose the output. `--quiet` prints nothing to stdout for exit 0 and 1.
  Errors still go to stderr.
- A package that does not load is an error that names the side and the input as given, for example
  `error: B (new.lumora): invalid archive: ...`. The tool never prints a partial diff.
- Unknown options, a missing or extra argument, or a bad `--format` value print the usage line to stderr and exit 2.
- Output only contains the input paths exactly as given on the command line and paths inside the packages.

## Text output

One finding per line, `<mark> <category> <key>...`. Marks: `+` added, `-` removed, `~` changed. Lines are sorted by
category (file, manifest, node, asset), then key, then field. Control characters and backslashes inside names are
escaped, so one finding is always one line. Identical packages print `identical`.

Example, comparing the aurora sample with a modified copy:

```
~ file manifest.json sha256: d4fdede8e0f6 -> 52c741a1f92d
+ file notes.txt
~ file scene.json sha256: dde1af24de85 -> 603efb3fae19
~ manifest name: "Aurora (sample)" -> "Aurora (v2)"
~ node root/hills-layer position: [0, -0.2, 0] -> [0, -0.3, 0]
~ node root/sky-layer parallax_depth: 1 -> 0.9
+ node root/stars
```

Added assets print `kind=... path=...` after the id. File changes show the first 12 hex digits of the SHA-256
before and after.

## JSON output

Hand-written, no dependency, keys sorted, two-space indent, one finding per line. The top-level keys are `a`, `b`
(the inputs as given), `findings`, `identical` (boolean) and `summary`. A finding has `category`, `change`
(`added`, `removed`, `changed`), `key`, and when they apply `field`, `new`, `old`. Strings are escaped: quotes,
backslashes and control characters, and bytes that are not valid UTF-8 become U+FFFD.

```
{
  "a": "/tmp/sd/aurora",
  "b": "/tmp/sd/aurora_v2",
  "findings": [
    {"category": "file", "change": "changed", "field": "sha256", "key": "manifest.json", "new": "52c741a1f92d", "old": "d4fdede8e0f6"},
    {"category": "file", "change": "added", "key": "notes.txt"},
    {"category": "file", "change": "changed", "field": "sha256", "key": "scene.json", "new": "603efb3fae19", "old": "dde1af24de85"},
    {"category": "manifest", "change": "changed", "key": "name", "new": "Aurora (v2)", "old": "Aurora (sample)"},
    {"category": "node", "change": "changed", "field": "position", "key": "root/hills-layer", "new": "[0, -0.3, 0]", "old": "[0, -0.2, 0]"},
    {"category": "node", "change": "changed", "field": "parallax_depth", "key": "root/sky-layer", "new": "0.9", "old": "1"},
    {"category": "node", "change": "added", "key": "root/stars"}
  ],
  "identical": false,
  "summary": {"added": 2, "changed": 5, "removed": 0}
}
```

## What is compared

- **Files.** Matched by path, compared by SHA-256, reported sorted by path. `checksums.txt` is never reported (it
  is derived from the other files). `settings.json` is an ordinary file here: it is not parsed, so only a changed
  digest shows up.
- **Manifest.** Every field of the loaded manifest: `format_version`, `scene_schema_version`, `lua_api_version`,
  `min_engine_version`, `name`, `author`, `scene_path`, `settings_path`, `script_path`, `backends`. Each difference is one
  `~ manifest <field>: old -> new` line. `backends` is compared as a set and printed sorted.
- **Scene nodes.** Matched by node id. A node's path is its ids from the root joined with `/`, for example
  `root/hills-layer`. Reported fields: `name`, `position`, `scale`, `rotation_z_degrees`, `layer_asset`,
  `parallax_depth`, and `children_order`. Numbers are compared exactly as the loader read them and printed in the
  shortest form that reads back to the same value.
  - A node that exists only on one side is `+` or `-`. The tool lists only that node. Its children are matched on
    their own.
  - A node whose id exists on both sides under different parents is reported as removed (old path) plus added (new
    path). There is no separate "moved" finding.
  - `children_order` is reported when the order of the children that exist under the same parent on both sides
    differs. Order is draw order. Added and removed children do not count.
- **Assets.** Matched by id. Added and removed assets, and changes to `kind` or `path`.

## Limits

- Not compared: unknown JSON fields that the loader drops, anything inside non-JSON files other than the digest,
  and the file bytes themselves (only digests are compared).
- A change that only reformats `scene.json` shows as a changed file with no node findings.
- Memory: a package is held in memory after loading, as in the packer. An archive file above 512 MiB is refused
  before reading. The loader limits apply (entry count, total size, compression ratio). The diff itself allocates
  in proportion to the input: one digest per file, one map entry and one path string per node (a path is at most
  depth times the id length, and depth is capped by the loader), and one entry per finding.
- Exit code 0 means identical for the fields above. It does not mean the two files are byte-identical.

## Evidence

The tests run on CI hosted runners (Linux with ASan/UBSan, Windows). They say nothing about devices or GPUs.
