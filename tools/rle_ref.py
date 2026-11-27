#!/usr/bin/env python3
"""Independent reference for core/rle (scope AJ). Exact Python integers.

Written from the rules in rle.h; it shares no code with the implementation. The encoder cuts maximal runs with itertools.groupby and
splits each run into chunks of 256, which is a different method from the greedy scan in the C++ code. The decoder follows the check
order of the header literally (the first problem found in pair order wins), and for every stream it accepts it asserts that re-encoding
the output gives the input back, which is the definition of canonical. It prints the constants pasted into tests/rle/rle_tests.cpp:
seeded encode cases (the C++ test mirrors the input generator below) and decode streams embedded as byte arrays, each with a digest of
the status, the length and the bytes for several destination capacities. Usage: python3 tools/rle_ref.py
"""
import itertools

M64 = (1 << 64) - 1
KMAX_IN = 1 << 24
KMAX_ENC = 1 << 25
OK, NULL, TOOLARGE, SMALL, MALFORMED, NONCANON = 0, 1, 2, 3, 4, 5
SENT = 0xDEADBEEF


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


RUNLENS = (255, 256, 257, 512, 511, 513)


def gen(mode, n, seed):
    r = Lcg(seed)
    out = []
    if mode == 0:
        while len(out) < n:
            v = r.rng(0, 255)
            out += [v] * r.rng(1, 40)
    elif mode == 1:
        out = [r.rng(0, 255) for _ in range(n)]
    elif mode == 2:
        out = [seed & 255] * n
    elif mode == 3:
        out = [(7, 9)[i & 1] for i in range(n)]
    elif mode == 4:
        while len(out) < n:
            v = r.rng(0, 1)
            out += [v] * r.rng(1, 600)
    else:
        while len(out) < n:
            v = r.rng(0, 2)
            out += [v] * RUNLENS[r.rng(0, 5)]
    return out[:n]


def encode(data):
    out = []
    for v, grp in itertools.groupby(data):
        left = len(list(grp))
        while left > 0:
            c = min(left, 256)
            out += [c - 1, v]
            left -= c
    return out


def decode(enc):
    """Returns (status, raw bytes or None, raw total or None)."""
    n = len(enc)
    if n % 2: return MALFORMED, None, None
    if n > KMAX_ENC: return TOOLARGE, None, None
    raw = 0
    for i in range(0, n, 2):
        if i > 0 and enc[i + 1] == enc[i - 1] and enc[i - 2] != 255: return NONCANON, None, None
        raw += enc[i] + 1
        if raw > KMAX_IN: return TOOLARGE, None, None
    out = []
    for i in range(0, n, 2): out += [enc[i + 1]] * (enc[i] + 1)
    assert encode(out) == list(enc), 'accepted stream is not canonical'
    return OK, out, raw


def enc_digest(data):
    need = len(encode(data))
    caps = (0, 1, need - 1 if need > 0 else 0, need, need + 3)
    ws = []
    enc = encode(data)
    for cap in caps:
        if need > cap: ws += [SMALL, SENT]
        else: ws += [OK, need] + enc
    return fnv_words(ws), need


def dec_digest(enc):
    st, out, raw = decode(enc)
    caps = [0, 1, 255, 256, 300, 5000]
    if st == OK: caps += [raw - 1 if raw > 0 else 0, raw, raw + 3]
    ws = []
    for cap in caps:
        if st != OK: ws += [st, SENT]
        elif raw > cap: ws += [SMALL, SENT]
        else: ws += [OK, raw] + out
    return fnv_words(ws), (raw if st == OK else 0xFFFFFFFF)


def main():
    o = []
    cases = ((0, 0, 1, 0), (1, 1, 2, 1), (1, 300, 3, 0), (0, 700, 4, 0), (1, 2000, 5, 1), (2, 1, 6, 2), (2, 255, 7, 2), (2, 256, 8, 2), (2, 257, 9, 2),
             (2, 512, 10, 2), (2, 513, 11, 2), (2, 70000, 12, 2), (3, 100, 13, 3), (4, 5000, 14, 4), (5, 4000, 15, 5), (0, 9000, 16, 0),
             (4, 30000, 17, 4), (5, 20000, 18, 5), (1, 65537, 19, 1))
    rows = []
    for _, n, seed, mode in cases:
        d = gen(mode, n, seed)
        dg, need = enc_digest(d)
        rows.append('{%d, %d, %d, %dull, %du}' % (mode, n, seed, dg, need))
    o.append('const EncCase kEncCases[] = {%s};' % ', '.join(rows))
    streams = [[], [0, 5], [255, 7], [255, 7, 255, 7, 3, 7], [0, 7, 0, 7], [3, 7, 0, 8, 0, 8], [10, 1, 20, 2, 30, 1], [0, 1, 0], [0, 7, 0, 7, 0],
               [254, 7, 0, 7], [255, 7, 0, 7, 0, 7], [255, 7, 255, 7, 255, 7], [0, 7, 1, 8, 0, 7], [5, 9]]
    for mode, n, seed in ((0, 400, 31), (1, 60, 32), (4, 3000, 33), (5, 2500, 34), (3, 40, 35)):
        streams.append(encode(gen(mode, n, seed)))
    e = encode(gen(0, 400, 31))
    e2 = list(e)
    e2[3] = e2[1]  # make pair 1 repeat pair 0's value
    streams.append(e2)
    r = Lcg(77)
    for k in range(10):
        streams.append([x for _ in range(r.rng(1, 30)) for x in (r.rng(0, 300) & 255 if r.rng(0, 3) else 255, r.rng(0, 2))])
    rows = []
    for i, s in enumerate(streams):
        o.append('const std::uint8_t kDec%d[] = {%s};' % (i, ', '.join(str(x) for x in s) if s else '0'))
        dg, raw = dec_digest(s)
        rows.append('{kDec%d, %du, %du, %dull}' % (i, len(s), raw, dg))
    o.append('const DecCase kDecCases[] = {%s};' % ', '.join(rows))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
