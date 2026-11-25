#!/usr/bin/env python3
"""Compare two binary PPM (P6, maxval 255) images the way tests/golden.h does.

Usage: python3 tools/golden/ppm_diff.py A.ppm B.ppm [--tolerance N]

A pixel is bad when any channel differs by more than the tolerance (default 2). Prints the sizes, the number of
bad pixels, the largest per-channel delta over the whole image, and the bounding box of the bad pixels.
Exit code 0 when the images match within the tolerance, 1 when they differ, 2 on an unreadable file.
Only the Python standard library is used.
"""
import argparse
import sys


class PpmError(Exception):
    pass


def read_ppm(path):
    """Returns (width, height, bytes of w*h*3 RGB values). Raises PpmError on anything that is not a clean P6."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        raise PpmError("%s: %s" % (path, e.strerror))
    pos = 0
    fields = []
    while len(fields) < 4:
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if pos < len(data) and data[pos] == 0x23:  # '#': comment to end of line
            while pos < len(data) and data[pos] != 0x0A:
                pos += 1
            continue
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        if start == pos:
            raise PpmError("%s: truncated header" % path)
        fields.append(data[start:pos])
    pos += 1  # exactly one whitespace byte after maxval
    if fields[0] != b"P6":
        raise PpmError("%s: not a P6 file" % path)
    try:
        w, h, maxval = int(fields[1]), int(fields[2]), int(fields[3])
    except ValueError:
        raise PpmError("%s: bad header numbers" % path)
    if w <= 0 or h <= 0 or maxval != 255:
        raise PpmError("%s: need positive size and maxval 255" % path)
    raster = data[pos:]
    if len(raster) != w * h * 3:
        raise PpmError("%s: raster has %d bytes, expected %d" % (path, len(raster), w * h * 3))
    return w, h, raster


def diff(a, b, tolerance):
    """Returns a dict with bad, max_delta and bbox (x0, y0, x1, y1 inclusive, or None)."""
    w, h, ra = a
    _, _, rb = b
    bad = 0
    max_delta = 0
    x0 = y0 = x1 = y1 = None
    for y in range(h):
        row = y * w * 3
        for x in range(w):
            i = row + x * 3
            worst = max(abs(ra[i] - rb[i]), abs(ra[i + 1] - rb[i + 1]), abs(ra[i + 2] - rb[i + 2]))
            if worst > max_delta:
                max_delta = worst
            if worst > tolerance:
                bad += 1
                if x0 is None:
                    x0 = x1 = x
                    y0 = y1 = y
                else:
                    x0, x1 = min(x0, x), max(x1, x)
                    y0, y1 = min(y0, y), max(y1, y)
    return {"bad": bad, "max_delta": max_delta, "bbox": None if x0 is None else (x0, y0, x1, y1)}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("a")
    p.add_argument("b")
    p.add_argument("--tolerance", type=int, default=2, help="per-channel tolerance (default 2)")
    args = p.parse_args(argv)
    if args.tolerance < 0:
        p.error("tolerance must be 0 or more")
    try:
        ia = read_ppm(args.a)
        ib = read_ppm(args.b)
    except PpmError as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
    print("size A: %dx%d" % (ia[0], ia[1]))
    print("size B: %dx%d" % (ib[0], ib[1]))
    if ia[:2] != ib[:2]:
        print("size mismatch")
        return 1
    r = diff(ia, ib, args.tolerance)
    print("tolerance: %d" % args.tolerance)
    print("bad pixels: %d" % r["bad"])
    print("max delta: %d" % r["max_delta"])
    if r["bbox"] is None:
        print("bounding box: none")
        print("match")
        return 0
    print("bounding box: x %d..%d, y %d..%d" % (r["bbox"][0], r["bbox"][2], r["bbox"][1], r["bbox"][3]))
    print("differ")
    return 1


if __name__ == "__main__":
    sys.exit(main())
