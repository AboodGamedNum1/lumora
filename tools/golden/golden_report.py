#!/usr/bin/env python3
"""Turn a golden output folder into a short Markdown report and side-by-side PNG images.

Usage:
  python3 tools/golden/golden_report.py --actual-dir DIR --baseline-dir DIR [--tolerance N]
      [--report-md FILE] [--images-dir DIR] [--max-failures N] [--verbose]

Every <name>.actual.ppm in the actual folder (not recursive) is compared with <baseline-dir>/<name>.ppm. Existing
.diff.ppm files are ignored: the comparison is recomputed with ppm_diff.py. The report is diagnostic only. It does
not change CI pass or fail and it is not evidence that a renderer is correct.

Exit codes: 0 all scenes pass, 1 at least one scene did not pass, 2 usage error or unreadable folder.
The folders hold untrusted data (pull request artifacts): names are checked, sizes are capped, nothing from them is
imported or executed, and the tool uses no network and no subprocess. Only the Python standard library is used.
"""
import argparse
import os
import stat
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the folder of this script, never an input folder
import ppm_diff  # noqa: E402

MAX_SIDE = 8192
MAX_PIXEL_BYTES = 64 * 1024 * 1024
MAX_FILE_BYTES = MAX_PIXEL_BYTES + 1024  # raster plus a generous header allowance
MAX_FILES = 500
MAX_PANEL_SIDE = 512
SUFFIX = ".actual.ppm"

PASS = "pass"
FAIL = "fail"
SIZE_MISMATCH = "size-mismatch"
BASELINE_MISSING = "baseline-missing"
UNREADABLE = "unreadable"
SEVERITY = {UNREADABLE: 0, SIZE_MISMATCH: 1, BASELINE_MISSING: 2, FAIL: 3, PASS: 4}
STATUSES = [PASS, FAIL, SIZE_MISMATCH, BASELINE_MISSING, UNREADABLE]


class Row(object):
    def __init__(self, name, status):
        self.name = name
        self.status = status
        self.size = None  # (w, h) of the actual image
        self.baseline_size = None
        self.bad = None
        self.max_delta = None
        self.bbox = None
        self.actual = None  # (w, h, raster) kept only for non-pass rows, for the images
        self.baseline = None


def safe_name(name):
    """True when a file name stem may be used: ASCII, printable, no separator, not starting with a dot."""
    if not name or name.startswith("."):
        return False
    for ch in name:
        o = ord(ch)
        if o < 0x21 and ch != " " or o > 0x7E:
            return False
        if ch in "/\\":
            return False
    return True


def load_ppm(path):
    """Returns (w, h, raster) or None for anything unreadable, oversized or not a regular file."""
    try:
        st = os.lstat(path)
        if not stat.S_ISREG(st.st_mode) or st.st_size > MAX_FILE_BYTES:
            return None
        w, h, raster = ppm_diff.read_ppm(path)
    except (ppm_diff.PpmError, OSError, MemoryError, ValueError, OverflowError):
        return None
    if w > MAX_SIDE or h > MAX_SIDE or len(raster) > MAX_PIXEL_BYTES:
        return None
    return w, h, raster


def compare_scene(name, actual_dir, baseline_dir, tolerance):
    actual = load_ppm(os.path.join(actual_dir, name + SUFFIX))
    if actual is None:
        return Row(name, UNREADABLE)
    row_path = os.path.join(baseline_dir, name + ".ppm")
    try:
        os.lstat(row_path)
        exists = True
    except OSError:
        exists = False
    row = Row(name, BASELINE_MISSING)
    row.size = (actual[0], actual[1])
    if not exists:
        row.actual = actual
        return row
    baseline = load_ppm(row_path)
    if baseline is None:
        row = Row(name, UNREADABLE)
        return row
    row.baseline_size = (baseline[0], baseline[1])
    if row.size != row.baseline_size:
        row.status = SIZE_MISMATCH
        return row
    if actual[2] == baseline[2]:  # byte equality shortcut, no pixel loop
        row.status = PASS
        row.bad = 0
        row.max_delta = 0
        return row
    d = ppm_diff.diff(actual, baseline, tolerance)
    row.bad = d["bad"]
    row.max_delta = d["max_delta"]
    row.bbox = d["bbox"]
    row.status = PASS if d["bad"] == 0 else FAIL
    if row.status == FAIL:
        row.actual = actual
        row.baseline = baseline
    return row


def md_escape(text):
    out = []
    for ch in text:
        if ch in "\\`*_{}[]()#+-.!|<>~&:":
            out.append("\\")
        out.append(ch)
    return "".join(out)


def sort_rows(rows):
    return sorted(rows, key=lambda r: (SEVERITY[r.status], r.name))


def size_text(row):
    if row.size is None:
        return "-"
    if row.status == SIZE_MISMATCH:
        return "actual %dx%d, baseline %dx%d" % (row.size + row.baseline_size)
    return "%dx%d" % row.size


def bbox_text(row):
    if row.bbox is None:
        return "-"
    return "(%d,%d)-(%d,%d)" % row.bbox


def build_markdown(rows, tolerance, verbose, rejected, not_processed, images_note):
    counts = dict((s, 0) for s in STATUSES)
    for r in rows:
        counts[r.status] += 1
    lines = ["Golden report: %d pass, %d fail, %d size-mismatch, %d baseline-missing, %d unreadable (tolerance %d)"
             % (counts[PASS], counts[FAIL], counts[SIZE_MISMATCH], counts[BASELINE_MISSING], counts[UNREADABLE],
                tolerance), ""]
    if rejected:
        lines += ["Rejected names, not processed: %d" % rejected, ""]
    if not_processed:
        lines += ["Not processed because of the %d file limit: %d" % (MAX_FILES, not_processed), ""]
    failed = [r for r in rows if r.status != PASS]
    if not rows and not rejected:
        lines += ["No actual files found, nothing was compared.", ""]
    elif not failed and not rejected and not not_processed:
        lines += ["No failures: every compared scene is within the tolerance.", ""]
    shown = sort_rows(rows) if verbose else sort_rows(failed)
    if shown:
        lines += ["| Name | Status | Size | Bad pixels | Max delta | Bounding box |", "| --- | --- | --- | --- | --- | --- |"]
        for r in shown:
            lines.append("| %s | %s | %s | %s | %s | %s |" % (
                md_escape(r.name), r.status, size_text(r), "-" if r.bad is None else r.bad,
                "-" if r.max_delta is None else r.max_delta, bbox_text(r)))
        lines.append("")
    if not verbose and counts[PASS]:
        lines += ["Passing scenes are only counted. Use --verbose to list them.", ""]
    if images_note:
        lines += [images_note, ""]
    lines.append("This report is diagnostic only. It is not evidence that a renderer is correct.")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- PNG

def png_chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def encode_png(width, height, rgb_rows):
    """8-bit RGB, no interlace, filter 0 on every row. rgb_rows is a list of bytes, one per row."""
    raw = b"".join(b"\x00" + row for row in rgb_rows)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))


def panel_geometry(w, h):
    """Returns (scale, stride, out_w, out_h): enlarge by an integer factor, or sample every stride-th pixel."""
    longest = max(w, h)
    if longest <= MAX_PANEL_SIDE:
        scale = MAX_PANEL_SIDE // longest
        return scale, 1, w * scale, h * scale
    stride = -(-longest // MAX_PANEL_SIDE)
    return 1, stride, -(-w // stride), -(-h // stride)


def image_panel(img, geometry):
    """List of RGB rows (bytes) for one image, enlarged or sampled."""
    w, h, raster = img
    scale, stride, out_w, out_h = geometry
    rows = []
    for oy in range(out_h):
        sy = oy // scale if scale > 1 else oy * stride
        base = sy * w * 3
        if scale > 1:
            src = raster[base:base + w * 3]
            rows.append(b"".join(src[x * 3:x * 3 + 3] * scale for x in range(w)))
        elif stride == 1:
            rows.append(raster[base:base + w * 3])
        else:
            rows.append(b"".join(raster[base + x * 3 * stride:base + x * 3 * stride + 3] for x in range(out_w)))
    return rows


def diff_panel(actual, baseline, tolerance, geometry, bbox):
    """Amplified absolute differences; pixels beyond the tolerance are pure red. At stride > 1 the bounding box
    of the bad pixels is drawn as a red outline, because sampling can skip them."""
    w, h, ra = actual
    rb = baseline[2]
    scale, stride, out_w, out_h = geometry
    gain = max(1, 255 // (tolerance + 1))
    rows = []
    for oy in range(out_h):
        sy = oy // scale if scale > 1 else oy * stride
        ba = sy * w * 3
        ca, cb = ra[ba:ba + w * 3], rb[ba:ba + w * 3]
        if ca == cb:
            rows.append(b"\x00" * (out_w * 3))
            continue
        out = bytearray()
        for ox in range(out_w):
            sx = ox // scale if scale > 1 else ox * stride
            i = sx * 3
            d0, d1, d2 = abs(ca[i] - cb[i]), abs(ca[i + 1] - cb[i + 1]), abs(ca[i + 2] - cb[i + 2])
            if max(d0, d1, d2) > tolerance:
                out += b"\xff\x00\x00"
            else:
                out += bytes((min(255, d0 * gain), min(255, d1 * gain), min(255, d2 * gain)))
        rows.append(bytes(out))
    if stride > 1 and bbox is not None:
        x0, y0, x1, y1 = [v // stride for v in bbox]
        x1, y1 = min(x1, out_w - 1), min(y1, out_h - 1)
        for y in range(y0, y1 + 1):
            row = bytearray(rows[y])
            for x in range(x0, x1 + 1):
                if y in (y0, y1) or x in (x0, x1):
                    row[x * 3:x * 3 + 3] = b"\xff\x00\x00"
            rows[y] = bytes(row)
    return rows


GAP = 4
GAP_COLOUR = b"\x40\x40\x40"


def join_panels(panels, out_h):
    rows = []
    gap = GAP_COLOUR * GAP
    for y in range(out_h):
        rows.append(gap.join(p[y] for p in panels))
    return rows


def write_images(rows, images_dir, tolerance, limit):
    """Writes images for the first `limit` rows that can have one (fail, baseline-missing), in table order.
    Returns how many files were written."""
    written = 0
    for row in [r for r in rows if r.status in (FAIL, BASELINE_MISSING)][:limit]:
        if row.status == FAIL:
            geometry = panel_geometry(row.size[0], row.size[1])
            panels = [image_panel(row.baseline, geometry), image_panel(row.actual, geometry),
                      diff_panel(row.actual, row.baseline, tolerance, geometry, row.bbox)]
            out_w = geometry[2] * 3 + GAP * 2
            png = encode_png(out_w, geometry[3], join_panels(panels, geometry[3]))
            target = os.path.join(images_dir, row.name + ".compare.png")
        elif row.status == BASELINE_MISSING:
            geometry = panel_geometry(row.size[0], row.size[1])
            panel = image_panel(row.actual, geometry)
            png = encode_png(geometry[2], geometry[3], panel)
            target = os.path.join(images_dir, row.name + ".actual.png")
        else:
            continue
        with open(target, "wb") as f:
            f.write(png)
        written += 1
    return written


# ---------------------------------------------------------------- main

def run(args, out=sys.stdout, err=sys.stderr):
    if not os.path.isdir(args.actual_dir):
        err.write("error: cannot read the actual folder\n")
        return 2
    if not os.path.isdir(args.baseline_dir):
        err.write("error: cannot read the baseline folder\n")
        return 2
    try:
        listing = sorted(os.listdir(args.actual_dir))
    except OSError:
        err.write("error: cannot list the actual folder\n")
        return 2
    names = []
    rejected = 0
    for entry in listing:
        if not entry.endswith(SUFFIX):
            continue
        stem = entry[:-len(SUFFIX)]
        if safe_name(stem):
            names.append(stem)
        else:
            rejected += 1
    not_processed = max(0, len(names) - MAX_FILES)
    names = names[:MAX_FILES]
    rows = [compare_scene(n, args.actual_dir, args.baseline_dir, args.tolerance) for n in names]
    ordered_failed = [r for r in sort_rows(rows) if r.status != PASS]

    images_note = ""
    if args.images_dir:
        try:
            os.makedirs(args.images_dir, exist_ok=True)
            written = write_images(ordered_failed, args.images_dir, args.tolerance, args.max_failures)
        except OSError:
            err.write("error: cannot write the images folder\n")
            return 2
        eligible = len([r for r in ordered_failed if r.status in (FAIL, BASELINE_MISSING)])
        images_note = "Images: wrote %d file(s), limit %d (--max-failures)." % (written, args.max_failures)
        if eligible > written:
            images_note += " %d fail or baseline-missing scene(s) have no image because of the limit." % (eligible - written)
    text = build_markdown(rows, args.tolerance, args.verbose, rejected, not_processed, images_note)
    if args.report_md:
        try:
            with open(args.report_md, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
        except OSError:
            err.write("error: cannot write the report file\n")
            return 2
    else:
        out.write(text)
    return 0 if not ordered_failed and not rejected and not not_processed else 1


def parse_args(argv):
    p = argparse.ArgumentParser(description="Markdown report and PNG images for a golden output folder.")
    p.add_argument("--actual-dir", required=True)
    p.add_argument("--baseline-dir", required=True)
    p.add_argument("--tolerance", type=int, default=2)
    p.add_argument("--report-md")
    p.add_argument("--images-dir")
    p.add_argument("--max-failures", type=int, default=20)
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args(argv)
    if not 0 <= args.tolerance <= 255:
        p.error("--tolerance must be between 0 and 255")
    if args.max_failures < 0:
        p.error("--max-failures must be 0 or more")
    return args


def main(argv=None):
    return run(parse_args(sys.argv[1:] if argv is None else argv))


if __name__ == "__main__":
    sys.exit(main())
