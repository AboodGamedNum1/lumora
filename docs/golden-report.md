# Golden report tool

`tools/golden/golden_report.py` turns a folder of golden-scene outputs into a short Markdown report and, optionally,
side-by-side PNG images. It is a diagnostic aid for people reading a failed golden run. It is not wired into CI,
it does not change any pass or fail result, and a clean report is not evidence that a renderer is correct.

It needs Python 3.8 or newer and the standard library only. It reuses `read_ppm` and `diff` from `tools/golden/ppm_diff.py`.

## Usage

```
python3 tools/golden/golden_report.py --actual-dir DIR --baseline-dir DIR \
    [--tolerance N] [--report-md FILE] [--images-dir DIR] [--max-failures N] [--verbose]
```

| Option | Meaning |
| --- | --- |
| `--actual-dir` | Folder with `<name>.actual.ppm` files. Not searched recursively. |
| `--baseline-dir` | Folder with `<name>.ppm` baselines. |
| `--tolerance` | Largest per-channel difference that still counts as equal, 0 to 255. Default 2. |
| `--report-md` | Write the Markdown here. Without it the Markdown goes to standard output. |
| `--images-dir` | Write PNG images here. The folder is created if needed. |
| `--max-failures` | Write images for at most this many scenes. Default 20. The table always lists every non-pass scene. |
| `--verbose` | Also list passing scenes in the table. |

Exit codes: 0 all scenes pass, 1 at least one scene did not pass (or a name was rejected, or the file limit was hit), 2 when no actual file was found at all (a setup error, never a silent pass),
2 usage error, unreadable folder or a write failure.

## Inputs and outputs

Each `<name>.actual.ppm` is compared with `<baseline-dir>/<name>.ppm`. Existing `.diff` files are ignored. The
comparison is always recomputed.

Statuses:

- `pass`: every channel is within the tolerance.
- `fail`: same size, at least one pixel beyond the tolerance.
- `size-mismatch`: the two images have different sizes.
- `baseline-missing`: no baseline file with that name.
- `unreadable`: the actual or baseline file is not a valid or allowed PPM.

The Markdown starts with one summary line with the status counts and the tolerance. A table follows, sorted by
severity (unreadable, size-mismatch, baseline-missing, fail, pass) and then by name. Columns: name, status, size,
bad pixels, max delta, bounding box. Names are escaped for Markdown. No file content is copied into the report.

Images, for the first `--max-failures` scenes that can have one:

- `<name>.compare.png` for a `fail`: baseline, actual and difference panels. In the difference panel the absolute
  channel differences are amplified so a delta of 1 is visible, and pixels beyond the tolerance are pure red.
- `<name>.actual.png` for a `baseline-missing` scene: the actual image alone.

Small images are enlarged by a whole-number factor so the long side is at most 512 px. Larger images are sampled and
the difference panel gets a red outline of the bad-pixel bounding box.

## Example

```
python3 tools/golden/golden_report.py --actual-dir out/actual --baseline-dir tests/golden --images-dir out/img
```

```
Golden report: 0 pass, 1 fail, 0 size-mismatch, 0 baseline-missing, 0 unreadable (tolerance 2)

| Name | Status | Size | Bad pixels | Max delta | Bounding box |
| --- | --- | --- | --- | --- | --- |
| sphere\_b | fail | 16x16 | 2 | 30 | (5,6)-(9,7) |

Images: wrote 1 file(s), limit 20 (--max-failures).

This report is diagnostic only. It is not evidence that a renderer is correct.
```

## Limits and safety

The folders are treated as untrusted. The tool reads only the two folders, non-recursively, and writes only the
report file and the images folder. It uses no network and no subprocess.

- Names must be ASCII, with no path separator, no leading dot and no control character. Other names are not
  processed. They are only counted (`Rejected names`) and make the exit code 1.
- At most 500 actual files are processed (sorted by name). The rest are counted and make the exit code 1.
- A PPM can be at most 8192 pixels per side and 64 MB of pixel data. Anything else, and anything that is not a
  regular file, becomes an `unreadable` row.
- Identical files are detected by comparing bytes, so a clean 1024x1024 pair is fast.
- The output is deterministic: same inputs give the same Markdown and the same PNG bytes.

## Maintainer guidance

- Read the report as a pointer to where to look. Open the compare image and the scene itself before deciding
  whether a change is a bug or an intended change.
- Do not copy tolerance values from here into CI. The CI golden comparison stays the source of truth.
- The PNG writer is hand-encoded (8-bit RGB, filter 0, zlib). Keep it dependency free.
- Run the tests from the repository root:

```
python3 -m unittest discover -s tools/golden
```
