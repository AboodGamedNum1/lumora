# Fuzz corpus tool

`tools/fuzz-tools/corpus_tool.py` reports on, deduplicates and shrinks a fuzz corpus folder. It uses only the Python
standard library (Python 3.8 or newer) and does not touch the build, the workflows or any other tool. Run it with
`python3 tools/fuzz-tools/corpus_tool.py <command> --corpus DIR`.

## Commands

| Command | Purpose |
| --- | --- |
| `report --corpus DIR [--json]` | Count files, bytes and duplicate groups. Never changes anything. |
| `dedup --corpus DIR [--apply] [--json]` | List files whose bytes repeat another file. With `--apply`, delete them. |
| `minimize --corpus DIR [--max-files N] [--max-bytes N] [--apply] [--json]` | Dedup, then keep the smallest files that fit the caps. With `--apply`, delete the rest. |

`dedup` and `minimize` are dry runs unless `--apply` is given. A dry run reads the folder and prints a plan; it
changes no name, content or modification time.

## Rules

- **Duplicates.** Two files are duplicates when their SHA-256 hashes match, which means their bytes are equal. File
  names, sizes of other files and modification times play no part. The file with the smallest name is kept.
- **Name order.** Names compare as raw bytes (the operating system bytes of the name), so `10` comes before `9` and
  `C` before `a`. The order does not depend on locale or on the order files were created.
- **Empty files.** An empty file is a normal file. All empty files are duplicates of the smallest-named one.
- **Minimize order.** After dedup, unique files are taken in order of size (smallest first), then name. The walk
  stops at the first file that would break a cap. That file and every later one is removed with reason `over-cap`.
  `--max-files N` allows at most N kept files. `--max-bytes N` allows at most N kept bytes in total. A cap of `0` is
  allowed (`--max-bytes 0` still keeps empty files). With no caps, `minimize` equals `dedup`.
- **Caps.** A cap must be written as plain decimal digits (up to 18). Signs, spaces, underscores and decimals are
  usage errors.
- **Rejected entries.** The corpus folder must be a real folder, not a symlink. Inside it, every entry must be a
  regular file of at most 1 MiB (1048576 bytes; exactly 1 MiB is allowed). A symlink, subfolder, fifo, unreadable
  file or larger file is a corpus problem: the tool reports it and exits 1 before it plans or deletes anything.
  The tool never follows a symlink.
- **Writing.** The tool only deletes files directly inside the corpus folder. It creates nothing, renames nothing
  and never writes outside the folder.
- **Apply.** `--apply` deletes only files named in the plan it just printed. Before each delete it reads the file
  again and checks that its size and SHA-256 still match the plan. A file that changed, vanished or became a symlink
  is not deleted; it is listed under `failed`, the other deletes continue, and the exit code is 1.
- **Names in output.** Reports are one line per file. Backslash, bytes that are not valid UTF-8 and unprintable
  characters are shown as escapes (`\\`, `\xNN`, `\uNNNN`), so a hostile name cannot add lines or change the text.
  The corpus path itself is not printed.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success. Includes a dry run and an apply that deleted everything it planned. |
| 1 | Corpus problem (missing folder, symlink, subfolder, file above 1 MiB, unreadable file) or an apply that failed to delete a planned file. |
| 2 | Usage error (unknown command, missing `--corpus`, bad cap, option the command does not take). |

In text mode, problems go to stderr. With `--json` a corpus problem prints
`{"command": ..., "ok": false, "problems": [{"name": ..., "reason": ...}], "schema": 1}` on stdout. Usage errors
always go to stderr.

## JSON output

`--json` prints one JSON object with sorted keys, two-space indent and ASCII only, so equal corpora give identical
bytes. `schema` is `1`; a change to a key bumps it.

- `report`: `schema`, `command`, `ok`, `applied`, `summary`, `duplicate_groups`, `files`.
- `dedup` and `minimize`: `schema`, `command`, `ok`, `applied`, `summary`, `duplicate_groups`, `kept`, `removed`,
  `totals`. `minimize` also has `caps` (`max_files`, `max_bytes`, `stopped_by`). After `--apply` there are also
  `deleted` and `failed`.
- `summary`: `files`, `bytes`, `smallest`, `largest`, `unique` (`smallest` and `largest` are null for an empty corpus).
- `duplicate_groups[]`: `sha256`, `size`, `keep`, `remove`.
- `removed[]`: `name`, `size`, `reason` (`duplicate` or `over-cap`), `duplicate_of`.

## Examples

These ran on a throwaway folder of five files (`seed-a` and `seed-b` hold the same two bytes). They are local runs,
not CI runs.

```text
$ report --corpus corpus
Fuzz corpus report
files: 5
total bytes: 9
smallest file: 0 bytes
largest file: 4 bytes
unique contents: 4
duplicate groups: 1
duplicate files that dedup removes: 1 (2 bytes)

duplicate groups:
  keep: seed-a (2 bytes, sha256 961b6dd3ede3)
    duplicate: seed-b
exit 0
```

```text
$ dedup --corpus corpus
Fuzz corpus dedup (dry run, nothing changed)
files: 5
total bytes: 9
smallest file: 0 bytes
largest file: 4 bytes
unique contents: 4
duplicate groups: 1
duplicate files that dedup removes: 1 (2 bytes)
kept: 4 files, 7 bytes
removed: 1 files, 2 bytes

remove:
  seed-b (2 bytes, duplicate of seed-a)
exit 0
```

```text
$ minimize --corpus corpus --max-files 3
Fuzz corpus minimize (dry run, nothing changed)
files: 5
total bytes: 9
smallest file: 0 bytes
largest file: 4 bytes
unique contents: 4
duplicate groups: 1
duplicate files that dedup removes: 1 (2 bytes)
max files: 3
max bytes: none
stopped by: max-files
kept: 3 files, 3 bytes
removed: 2 files, 6 bytes

remove:
  crash-1 (4 bytes, over the caps)
  seed-b (2 bytes, duplicate of seed-a)
exit 0
```

```text
$ dedup --corpus corpus --json
{
  "applied": false,
  "command": "dedup",
  "duplicate_groups": [
    {
      "keep": "seed-a",
      "remove": [
        "seed-b"
      ],
      "sha256": "961b6dd3ede3cb8ecbaacbd68de040cd78eb2ed5889130cceb4c49268ea4d506",
      "size": 2
    }
  ],
  "kept": [
    {
      "name": "crash-1",
      "size": 4
    },
    {
      "name": "empty-1",
      "size": 0
    },
    {
      "name": "seed-a",
      "size": 2
    },
    {
      "name": "tiny",
      "size": 1
    }
  ],
  "ok": true,
  "removed": [
    {
      "duplicate_of": "seed-a",
      "name": "seed-b",
      "reason": "duplicate",
      "size": 2
    }
  ],
  "schema": 1,
  "summary": {
    "bytes": 9,
    "files": 5,
    "largest": 4,
    "smallest": 0,
    "unique": 4
  },
  "totals": {
    "kept_bytes": 7,
    "kept_files": 4,
    "removed_bytes": 2,
    "removed_files": 1
  }
}
exit 0
```

## Evidence limits

- The tool judges bytes only. It does not run a fuzzer, so it cannot tell whether two different files cover the same
  code, or whether a removed file mattered for coverage. `minimize` is a size and count cap, not coverage-guided
  minimization.
- A dry run is a plan for the folder as it was when read. If other programs change the folder before `--apply`, the
  re-check skips changed files, but new files are not considered.
- SHA-256 equality is treated as byte equality. The tool does not compare bytes a second time.
- Only local runs back the examples in this page. It has not been checked on Windows or other platforms by the
  author of this page; the tests skip symlink and fifo cases where the system lacks them.
- Never run `--apply` on a committed corpus under `tools/fuzz/corpus/` as a casual cleanup. Review the dry run first.

## Maintainer notes

Run the tests with:

```text
python3 -m unittest discover -s tools/fuzz-tools/tests
```

Every test builds a temporary folder, so no real corpus is read. The tests cover duplicates, empty files, hostile
names, symlinks, the 1 MiB and cap boundaries, the JSON key sets, dry runs and apply. Remove `__pycache__` folders
before committing.
