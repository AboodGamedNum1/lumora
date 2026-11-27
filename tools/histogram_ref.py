#!/usr/bin/env python3
"""Independent reference for core/histogram (scope AI). Exact Python integers.

Written from the rules in histogram.h; it shares no code with the implementation. Images are generated here (the same seeded generator is
mirrored in the C++ test), counted with a plain dictionary, and the percentile of a pixel image is also found by sorting the pixels and
reading the value at rank target - 1, which is a different method from the cumulative scan used for hand-built histograms; the script
asserts that the two agree. It prints the constants pasted into tests/histogram/histogram_tests.cpp: seeded image cases and seeded
hand-built histogram cases, each with a digest of the bins, total, mean, percentiles and stretch tables.
Usage: python3 tools/histogram_ref.py
"""
M64 = (1 << 64) - 1
PMS = (0, 1, 10, 100, 250, 333, 500, 501, 750, 900, 990, 999, 1000)
PAIRS = ((0, 1000), (10, 990), (250, 750), (0, 1), (999, 1000), (500, 501), (0, 500), (500, 1000), (5, 5), (700, 300))


def fnv_words(ws):
    h = 0xcbf29ce484222325
    for v in ws:
        v &= 0xFFFFFFFF
        for k in range(4):
            h ^= (v >> (8 * k)) & 0xFF
            h = (h * 0x100000001b3) & M64
    return h


class Lcg:
    def __init__(s, seed): s.s = seed & M64
    def next(s):
        s.s = (s.s * 6364136223846793005 + 1442695040888963407) & M64
        return s.s >> 32
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


def pixel(mode, seed, r, x, y):
    if mode == 0: return r.rng(0, 255)
    if mode == 1: return seed & 255
    if mode == 2: return 17 if r.rng(0, 3) else 200
    if mode == 3: return (x + y) & 255
    if mode == 4: return r.rng(100, 103)
    if mode == 5: return 255 if r.rng(0, 9) else 0
    return r.rng(0, 255) & r.rng(0, 255)


def image(w, h, stride, mode, seed):
    """Returns (buffer, pixels): the exact-size buffer with garbage between rows, and the w*h pixels."""
    r = Lcg(seed)
    buf = [0] * ((h - 1) * stride + w)
    px = []
    for y in range(h):
        for x in range(w):
            v = pixel(mode, seed, r, x, y)
            buf[y * stride + x] = v
            px.append(v)
        if y + 1 < h:
            for x in range(w, stride): buf[y * stride + x] = r.rng(0, 255)
    return buf, px


def total(bins): return sum(bins)


def pct_scan(bins, pm):
    t = total(bins)
    if t == 0 or pm > 1000: return None
    if pm == 0:
        return min(v for v in range(256) if bins[v] != 0)
    target = -((-t * pm) // 1000)
    run = 0
    for v in range(256):
        run += bins[v]
        if run >= target: return v


def pct_sorted(px, pm):
    t = len(px)
    srt = sorted(px)
    if pm == 0: return srt[0]
    target = -((-t * pm) // 1000)
    return srt[target - 1]


def mean_q8(bins):
    t = total(bins)
    if t == 0: return None
    return (sum(v * bins[v] for v in range(256)) * 256 + t // 2) // t


def lut(bins, lo_pm, hi_pm):
    t = total(bins)
    if t == 0 or lo_pm >= hi_pm or hi_pm > 1000: return None
    lo, hi = pct_scan(bins, lo_pm), pct_scan(bins, hi_pm)
    out = []
    for v in range(256):
        if lo == hi: out.append(v)
        elif v <= lo: out.append(0)
        elif v >= hi: out.append(255)
        else: out.append(((v - lo) * 255 + (hi - lo) // 2) // (hi - lo))
    return out


def digest(bins):
    t = total(bins)
    ws = list(bins) + [t & 0xFFFFFFFF, t >> 32]
    m = mean_q8(bins)
    ws += [0] if m is None else [1, m]
    for pm in PMS:
        p = pct_scan(bins, pm)
        ws += [0] if p is None else [1, p]
    for lo, hi in PAIRS:
        l = lut(bins, lo, hi)
        ws += [0] if l is None else [1] + l
    return fnv_words(ws), len(ws)


def main():
    o = []
    cases = ((1, 1, 1, 0, 1), (7, 5, 7, 0, 2), (7, 5, 11, 0, 3), (16, 16, 16, 3, 4), (33, 9, 40, 1, 5), (64, 64, 64, 2, 6), (50, 20, 77, 4, 7),
             (100, 31, 128, 5, 8), (255, 3, 300, 6, 9), (256, 256, 256, 0, 10), (257, 17, 260, 0, 11), (1024, 1, 1024, 3, 12),
             (1, 300, 1, 0, 13), (1, 300, 9, 6, 14), (4096, 2, 65536, 0, 15), (4096, 4, 4096, 2, 16), (300, 300, 513, 6, 17), (128, 128, 128, 4, 18))
    rows = []
    for w, h, st, mode, seed in cases:
        buf, px = image(w, h, st, mode, seed)
        bins = [0] * 256
        for v in px: bins[v] += 1
        assert total(bins) == w * h
        for pm in PMS: assert pct_scan(bins, pm) == pct_sorted(px, pm)
        d, n = digest(bins)
        rows.append('{%d, %d, %d, %d, %d, %dull, %du}' % (w, h, st, mode, seed, d, n))
    o.append('const ImgCase kImgCases[] = {%s};' % ', '.join(rows))
    # hand-built histograms: (seed, max bin value, share of empty bins per 10)
    rows = []
    for seed, mx, empty in ((101, 5, 0), (102, 5, 8), (103, 1000, 5), (104, 4294967295, 0), (105, 4294967295, 9), (106, 1, 9), (107, 65535, 3), (108, 4294967295, 6),
                            (109, 3, 10), (110, 4294967295, 2), (111, 100, 7), (112, 2147483648, 4)):
        r = Lcg(seed)
        bins = [0 if r.rng(0, 9) < empty else r.rng(0, mx) for _ in range(256)]
        d, n = digest(bins)
        rows.append('{%d, %du, %d, %dull, %du}' % (seed, mx, empty, d, n))
    o.append('const HistCase kHistCases[] = {%s};' % ', '.join(rows))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
