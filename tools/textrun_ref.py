#!/usr/bin/env python3
"""Independent reference for core/textrun (scope AB). Exact integers only.

It has its own minimal font builder, its own UTF-8 decoder (written from the rules in textrun.h, not from any C++ file), its own
format 4 cmap lookup, hmtx advance lookup, cumulative pen rounding and FNV. Running it prints the constants that
tests/textrun/textrun_tests.cpp pastes: font digests and sizes, case rows and corpus digests.
Usage: python3 tools/textrun_ref.py > data.inc
"""
import struct

M64 = (1 << 64) - 1
OK, NULLBUF, BADSIZE, BADGLYPH, BADCMAP, OUTRANGE, TOOSMALL, OVERLAP = 'ok', 'null', 'badsize', 'badglyph', 'badcmap', 'outrange', 'toosmall', 'overlap'
MAXSIZE = 1 << 22
MAXRUN = 4096
LIM = 1 << 30


def u16(v): return struct.pack('>H', v & 0xFFFF)
def u32(v): return struct.pack('>I', v & 0xFFFFFFFF)
def pad4(b): return b + b'\0' * ((-len(b)) % 4)


def fnv_bytes(bs):
    h = 0xcbf29ce484222325
    for b in bs:
        h ^= b
        h = (h * 0x100000001b3) & M64
    return h


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


# ---- font builder: all glyphs empty, only cmap (format 4, platform 3 encoding 1) and hmtx matter ----
def cmap4(segs):
    """segs: (start, end, delta, ids or None); ids is the glyph id array slice for this segment, None for idRangeOffset 0.
    A segment with ids == 'far' gets an idRangeOffset that points outside the subtable."""
    n = len(segs)
    arr = []
    ro = []
    for i, (st, en, dl, ids) in enumerate(segs):
        if ids is None:
            ro.append(0)
        elif ids == 'far':
            ro.append(2 * (n - i) + 2 * 1000)
        else:
            ro.append(2 * (n - i) + 2 * len(arr))
            arr.extend(ids)
    body = b''.join(u16(s[1]) for s in segs) + u16(0) + b''.join(u16(s[0]) for s in segs)
    body += b''.join(u16(s[2]) for s in segs) + b''.join(u16(r) for r in ro) + b''.join(u16(v) for v in arr)
    sub = u16(4) + u16(14 + len(body)) + u16(0) + u16(2 * n) + u16(0) + u16(0) + u16(0) + body
    return u16(0) + u16(1) + u16(3) + u16(1) + u32(12) + sub


def font(upem, ng, nhm, adv, segs):
    head = bytearray(54)
    head[12:16] = u32(0x5F0F3CF5)
    head[18:20] = u16(upem)
    head[50:52] = u16(0)
    maxp = u32(0x00010000) + u16(ng)
    loca = b'\0' * (2 * (ng + 1))
    glyf = b'\0' * 4
    hhea = bytearray(36)
    hhea[34:36] = u16(nhm)
    hmtx = b''.join(u16(adv[i]) + u16(0) for i in range(nhm)) + b'\0' * (2 * (ng - nhm))
    tabs = [(b'cmap', cmap4(segs)), (b'glyf', glyf), (b'head', bytes(head)), (b'hhea', bytes(hhea)), (b'hmtx', hmtx),
            (b'loca', loca), (b'maxp', maxp)]
    out = u32(0x00010000) + u16(len(tabs)) + b'\0' * 6
    off = 12 + 16 * len(tabs)
    body = b''
    for tag, data in tabs:
        out += tag + u32(0) + u32(off + len(body)) + u32(len(data))
        body += pad4(data)
    return out + body


# ---- the four test fonts ----
def seg(a, b, g): return (a, b, (g - a) & 0xFFFF, None)

SEGS_A = [seg(0x20, 0x20, 2), seg(0x41, 0x4F, 1), seg(0xE9, 0xE9, 9), seg(0x20AC, 0x20AC, 12), (0x4E2D, 0x4E2D, 0, [11]),
          seg(0xFFFD, 0xFFFD, 7), (0xFFFF, 0xFFFF, 1, None)]
ADV_A = [500, 600, 250, 0, 333]
FA = font(1000, 16, 5, ADV_A, SEGS_A)
SEGS_B = [seg(0x41, 0x43, 1), (0x44, 0x44, 0, 'far'), seg(0x45, 0x45, 5), (0xFFFF, 0xFFFF, 1, None)]
ADV_B = [100, 2048, 1, 1000, 777, 65535, 12345, 1]
FB = font(2048, 8, 8, ADV_B, SEGS_B)
SEGS_C = [seg(0x61, 0x64, 0), (0x7A, 0x7A, 0, 'far'), (0xFFFF, 0xFFFF, 1, None)]
ADV_C = [16, 65535, 0, 8]
FC = font(16, 4, 4, ADV_C, SEGS_C)
SEGS_D = [(0, 0xFFFE, 0, None), (0xFFFF, 0xFFFF, 1, None)]
FD = font(1024, 65535, 1, [7], SEGS_D)
FONTS = [FA, FB, FC, FD]
ADVS = [ADV_A, ADV_B, ADV_C, [7]]
UPEM = [1000, 2048, 16, 1024]
NG = [16, 8, 4, 65535]
NHM = [5, 8, 4, 1]


def lookup(fi, cp):
    """Own format 4 lookup over the segment list. Returns glyph id, or None for an out-of-subtable read (BadCmap)."""
    if cp > 0xFFFF: return 0
    segs = {0: SEGS_A, 1: SEGS_B, 2: SEGS_C, 3: SEGS_D}[fi]
    for i, (st, en, dl, ids) in enumerate(segs):
        if en >= cp:
            if st > cp: return 0
            if ids is None: g = (cp + dl) & 0xFFFF
            elif ids == 'far': return None
            else:
                v = ids[cp - st]
                g = 0 if v == 0 else (v + dl) & 0xFFFF
            return g if g < NG[fi] else 0
    return 0


def adv(fi, g): return ADVS[fi][g] if g < NHM[fi] else ADVS[fi][NHM[fi] - 1]


# ---- UTF-8 decoder written from the rules in the header ----
def decode(bs):
    out = []
    i = 0
    n = len(bs)
    def cont(j, lo=0x80, hi=0xBF): return j < n and lo <= bs[j] <= hi
    while i < n:
        b = bs[i]
        if b <= 0x7F: out.append(b); i += 1; continue
        ln = 0
        if 0xC2 <= b <= 0xDF:
            if cont(i + 1): ln = 2; cp = ((b & 0x1F) << 6) | (bs[i + 1] & 0x3F)
        elif b == 0xE0:
            if cont(i + 1, 0xA0, 0xBF) and cont(i + 2): ln = 3
        elif 0xE1 <= b <= 0xEC or 0xEE <= b <= 0xEF:
            if cont(i + 1) and cont(i + 2): ln = 3
        elif b == 0xED:
            if cont(i + 1, 0x80, 0x9F) and cont(i + 2): ln = 3
        elif b == 0xF0:
            if cont(i + 1, 0x90, 0xBF) and cont(i + 2) and cont(i + 3): ln = 4
        elif 0xF1 <= b <= 0xF3:
            if cont(i + 1) and cont(i + 2) and cont(i + 3): ln = 4
        elif b == 0xF4:
            if cont(i + 1, 0x80, 0x8F) and cont(i + 2) and cont(i + 3): ln = 4
        if ln == 0: out.append(0xFFFD); i += 1; continue
        v = bs[i] & ((0x7F >> ln))
        for k in range(1, ln): v = (v << 6) | (bs[i + k] & 0x3F)
        out.append(v); i += ln
    return out


def rnd(u, size, upem): return (2 * u * size + upem) // (2 * upem)


def shape(fi, bs, size, cap=None):
    """Returns (status, count, width, digest of placed). cap defaults to enough."""
    cps = decode(bs)
    if size < 1 or size > MAXSIZE: return (BADSIZE, 0, 0, 0)
    if len(cps) > MAXRUN or (cap is not None and cap < len(cps)): return (TOOSMALL, 0, 0, 0)
    u = 0
    placed = []
    for cp in cps:
        g = lookup(fi, cp)
        if g is None: return (BADCMAP, 0, 0, 0)
        a = adv(fi, g)
        x = rnd(u, size, UPEM[fi])
        if x > LIM: return (OUTRANGE, 0, 0, 0)
        placed.append((g, x))
        u += a
    w = rnd(u, size, UPEM[fi])
    if w > LIM: return (OUTRANGE, 0, 0, 0)
    return (OK, len(cps), w, fnv_words([v for p in placed for v in p]))


STN = {OK: 'Status::Ok', NULLBUF: 'Status::NullBuffer', BADSIZE: 'Status::BadSize', BADCMAP: 'Status::BadCmap',
       OUTRANGE: 'Status::OutOfRange', TOOSMALL: 'Status::BufferTooSmall', OVERLAP: 'Status::Overlap', BADGLYPH: 'Status::BadGlyph'}


def cstr(bs): return '"' + ''.join('\\x%02x' % b for b in bs) + '"'


def row(fi, bs, size):
    st, c, w, d = shape(fi, bs, size)
    return '  {%d, %d, %d, %s, %s, %d, %d, %dull},' % (fi, size, len(bs), cstr(bs) if bs else '""', STN[st], c, w, d)


def main():
    o = []
    o.append('const u64 kFontDigest[4] = {%s};' % ', '.join('%dull' % fnv_bytes(f) for f in FONTS))
    o.append('const u32 kFontSize[4] = {%s};' % ', '.join(str(len(f)) for f in FONTS))
    # hand written cases
    S = [1, 256, 4096, 256 * 16, 256 * 100, 12345, MAXSIZE]
    cases = []
    A = [b'', b'A', b'ABC', b'A A', b'ABCDEFGHIJKLMNO', b'BAD', b'\xc3\xa9', b'\xe2\x82\xac', b'\xe4\xb8\xad', b'\xef\xbf\xbd',
         b'A\xffB', b'\xc0\x80', b'\xe2\x82', b'A\xe2\x82', b'\xf0\x9f\x98\x80', b'\xed\xa0\x80', b'\xf4\x90\x80\x80', b'\x80\x80',
         b'\xe0\x80\x80', b'\xf0\x80\x80\x80', b'\xc2', b'\xc2\x41', b'zz', b'\x00', b'A\x00B', b'\xe2\x28\xa1', b'\xf0\x9f\x98']
    for t in A:
        for s in S: cases.append(row(0, t, s))
    for t in [b'', b'ABC', b'ADC', b'EEE', b'CBA', b'AAAAD', b'DAA', b'\xff', b'E', b'BBB' * 5]:
        for s in [1, 2048, 5000, MAXSIZE]: cases.append(row(1, t, s))
    for s in [1, 7, 4096, MAXSIZE]:
        for t in [b'a', b'aa', b'ab', b'ba', b'bb', b'abc', b'cccc', b'dddd', b'abd']: cases.append(row(2, t, s))
    for t in [b'z', b'bz', b'bbz', b'zb', b'bzb', b'azb']:
        for s in [1, MAXSIZE]: cases.append(row(2, t, s))
    for n in (255, 256, 257, 258, 300):
        cases.append(row(2, b'a' * n, MAXSIZE))
        cases.append(row(2, b'a' * n, MAXSIZE - 1))
    # pens and widths that land exactly on 2^30 - 1, 2^30 and 2^30 + 1 with non power of two sizes (font 2: upem 16, 'a' 16, 'd' 8)
    for target in (LIM - 1, LIM, LIM + 1):
        found = 0
        for n in range(2000, 4096):
            for k in (0, 1):
                u = 16 * n + 8 * k
                for sz in range(max(1, (16 * target) // u - 2), min(MAXSIZE, (16 * target) // u + 2) + 1):
                    if rnd(u, sz, 16) == target and found < 2:
                        cases.append(row(2, b'a' * n + b'd' * k + b'c', sz)); found += 1
        assert found >= 1, target
    # tie rounding, upem 1000: advances 500 600 250 0 333 for glyphs 1,2,3,... letters A B C D E
    for t in [b'AA', b'AAAA', b'ACCC', b'CCCC', b'EEEEEE', b'DDDDA', b'AEEEEEEEEE']:
        for s in [1, 3, 7, 999, 1000, 1001]: cases.append(row(0, t, s))
    o.append('const Row kRows[] = {')
    o.extend(cases)
    o.append('};')
    # random corpora on font A and D
    rng = Lcg(0xA1)
    alpha = [0x41, 0x4F, 0x20, 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xE4, 0xB8, 0xAD, 0xEF, 0xBF, 0xBD, 0xFF, 0x80, 0xF0, 0x9F, 0x98, 0xED, 0xA0, 0xC0, 0x00, 0x7F, 0xF4, 0x8F, 0xBF, 0x90, 0xC2, 0xDF, 0xE0, 0x41]
    stat = {}
    ws = []
    for k in range(3000):
        ln = rng.rng(0, 40)
        bs = bytes(alpha[rng.rng(0, len(alpha) - 1)] if rng.rng(0, 9) < 8 else rng.rng(0, 255) for _ in range(ln))
        size = [1, 100, 256 * 16, 12345, MAXSIZE][rng.rng(0, 4)]
        st, c, w, d = shape(0, bs, size)
        stat[st] = stat.get(st, 0) + 1
        ws += [len(bs), c, w & 0xFFFFFFFF, d & 0xFFFFFFFF, d >> 32]
    o.append('const u64 kCorpusDigest = %dull;  // seed 0xA1, 3000 strings on font 0' % fnv_words(ws))
    o.append('const u32 kCorpusOk = %d;' % stat.get(OK, 0))
    # exhaustive decoder sweep on font D (glyph id = code point below 0xFFFF)
    ws = []
    def fold(bs):
        cps = decode(bs)
        g = [lookup(3, c) for c in cps]
        ws.append(len(bs)); ws.append(len(cps)); ws.extend(g)
    for b0 in range(256): fold(bytes([b0]))
    for b0 in range(256):
        for b1 in range(256): fold(bytes([b0, b1]))
    for b0 in range(0xC0, 0xFF):
        for b1 in range(256):
            for b2 in range(256): fold(bytes([b0, b1, b2]))
    special = [0x00, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC2, 0xF4]
    for b0 in range(0xF0, 0xF8):
        for b1 in range(256):
            for b2 in special:
                for b3 in special: fold(bytes([b0, b1, b2, b3]))
    o.append('const u64 kSweepDigest = %dull;' % fnv_words(ws))
    o.append('const u32 kSweepWords = %d;' % len(ws))
    # digest_placed golden
    pl = [(1, 0), (2, 256), (0xFFFFFFFF, 0x7FFFFFFF), (7, -5 & 0xFFFFFFFF)]
    o.append('const u64 kPlacedDigest = %dull;  // glyph/x pairs 1,0 2,256 4294967295,2147483647 7,-5' % fnv_words([v for p in pl for v in p]))
    o.append('const u64 kEmptyDigest = %dull;' % fnv_words([]))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
