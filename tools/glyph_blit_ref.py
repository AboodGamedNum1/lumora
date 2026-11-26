#!/usr/bin/env python3
"""Independent reference for core/glyphblit (scope AE). Exact Python integers; memory is a bytearray, addresses are offsets.

It is written from the rules in glyph_blit.h and shares no code with the implementation. Running it prints the constants that
tests/glyphblit/glyph_blit_tests.cpp pastes: blit cases (separate buffers and one shared buffer, with the status and the digest of the
final bytes) and image_digest cases. Usage: python3 tools/glyph_blit_ref.py > data.inc
"""
M64 = (1 << 64) - 1
MAXDIM = 4096
MAXSTRIDE = 65536
OK, NULLARG, BADSIZE, BADSTRIDE, OOB, OVERLAP = 0, 1, 2, 3, 4, 5


def fnv_bytes(bs, h=0xcbf29ce484222325):
    for b in bs:
        h ^= b
        h = (h * 0x100000001b3) & M64
    return h


def digest(pix, w, h, stride):
    if w == 0 or h == 0 or w > MAXDIM or h > MAXDIM or stride < w or stride > MAXSTRIDE:
        return fnv_bytes(b'')
    bs = list(w.to_bytes(4, 'little')) + list(h.to_bytes(4, 'little'))
    for r in range(h):
        bs += list(pix[r * stride:r * stride + w])
    return fnv_bytes(bs)


class Lcg:
    def __init__(s, seed): s.s = seed & M64
    def next(s):
        s.s = (s.s * 6364136223846793005 + 1442695040888963407) & M64
        return s.s >> 32
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


def fill(seed, n):
    s = seed & M64
    out = bytearray()
    for _ in range(n):
        s = (s * 6364136223846793005 + 1442695040888963407) & M64
        out.append(s >> 56)
    return out


def blit(src, src_addr, dst, dst_addr, w, h, ss, dw, dh, ds, x, y, sbase=0, dbase=0):
    """src and dst are bytearrays (dst is modified in place on success; they may be the same object). Returns the status."""
    if w == 0 or h == 0 or dw == 0 or dh == 0 or max(w, h, dw, dh) > MAXDIM: return BADSIZE
    if ss < w or ds < dw or ss > MAXSTRIDE or ds > MAXSTRIDE: return BADSTRIDE
    if x + w > dw or y + h > dh: return OOB
    wp = min(w + 1, dw - x)
    hp = min(h + 1, dh - y)
    sb, se = src_addr, src_addr + (h - 1) * ss + w
    db, de = dst_addr + y * ds + x, dst_addr + (y + hp - 1) * ds + x + wp
    if sb < de and db < se: return OVERLAP
    block = [bytes(src[sbase + r * ss:sbase + r * ss + w]) for r in range(h)]   # read before any write
    for r in range(h):
        o = dbase + (y + r) * ds + x
        dst[o:o + w] = block[r]
        if x + w < dw: dst[o + w] = 0
    if y + h < dh:
        o = dbase + (y + h) * ds + x
        dst[o:o + wp] = bytes(wp)
    return OK


def case_a(i, w, h, ss, dw, dh, ds, x, y, srclen=None, dstlen=None):
    sl = (h - 1) * ss + w if srclen is None else srclen
    dl = (dh - 1) * ds + dw if dstlen is None else dstlen
    src, dst = fill(2 * i + 1, sl), fill(2 * i + 2, dl)
    st = blit(src, 1 << 40, dst, 1 << 50, w, h, ss, dw, dh, ds, x, y)
    return (0, w, h, ss, dw, dh, ds, x, y, sl, dl, 0, 0, 0, st, fnv_bytes(dst), 2 * i + 1)


def case_b(i, w, h, ss, dw, dh, ds, x, y, src_off, dst_off):
    sl = (h - 1) * ss + w
    dl = (dh - 1) * ds + dw
    ml = max(src_off + sl, dst_off + dl)
    mem = fill(2 * i + 1, ml)
    st = blit(mem, src_off, mem, dst_off, w, h, ss, dw, dh, ds, x, y, src_off, dst_off)
    return (1, w, h, ss, dw, dh, ds, x, y, sl, dl, src_off, dst_off, ml, st, fnv_bytes(mem), 2 * i + 1)


def cases():
    out = []
    r = Lcg(0xE1)
    n = 0
    def add(f, *a):
        nonlocal n
        out.append(f(n, *a)); n += 1
    # hand shaped valid and edge cases, separate buffers
    add(case_a, 3, 2, 3, 8, 6, 8, 2, 1)       # interior: right cell and row below cleared
    add(case_a, 3, 2, 5, 8, 6, 9, 5, 4)       # right edge touches atlas, bottom row cleared
    add(case_a, 3, 2, 3, 8, 6, 8, 5, 0)       # x + w == dw: no right cell, row below has wp = w
    add(case_a, 3, 2, 3, 8, 6, 8, 0, 4)       # y + h == dh: no row below
    add(case_a, 3, 2, 3, 8, 6, 8, 5, 4)       # both edges: nothing cleared
    add(case_a, 1, 1, 1, 1, 1, 1, 0, 0)
    add(case_a, 1, 1, 1, 2, 2, 2, 0, 0)
    add(case_a, 1, 1, 1, 2, 2, 2, 1, 1)
    add(case_a, 4, 4, 4, 5, 5, 5, 0, 0)       # corner cell cleared
    add(case_a, 4, 4, 7, 5, 5, 9, 1, 1)
    add(case_a, 4096, 1, 4096, 4096, 1, 4096, 0, 0)
    add(case_a, 1, 4096, 1, 1, 4096, 1, 0, 0)
    add(case_a, 4096, 2, 4096, 4096, 3, 4096, 0, 0)
    add(case_a, 2, 4096, 2, 4096, 4096, 4096, 4094, 0)
    add(case_a, 5, 5, 5, 5, 5, 5, 0, 0)
    add(case_a, 3, 3, 65536, 3, 3, 65536, 0, 0)  # strides at the limit
    # errors
    add(case_a, 0, 3, 3, 8, 8, 8, 0, 0, 4, 64)
    add(case_a, 3, 0, 3, 8, 8, 8, 0, 0, 4, 64)
    add(case_a, 3, 3, 3, 0, 8, 8, 0, 0, 9, 4)
    add(case_a, 3, 3, 3, 8, 0, 8, 0, 0, 9, 4)
    add(case_a, 4097, 3, 4097, 4096, 8, 4096, 0, 0, 4, 4)
    add(case_a, 3, 4097, 3, 4096, 4096, 4096, 0, 0, 4, 4)
    add(case_a, 3, 3, 3, 4097, 8, 4097, 0, 0, 4, 4)
    add(case_a, 3, 3, 3, 8, 4097, 8, 0, 0, 4, 4)
    add(case_a, 3, 3, 2, 8, 8, 8, 0, 0, 4, 64)     # src_stride < w
    add(case_a, 3, 3, 3, 8, 8, 7, 0, 0, 9, 64)     # dst_stride < dst_w
    add(case_a, 3, 3, 65537, 8, 8, 8, 0, 0, 4, 64)
    add(case_a, 3, 3, 3, 8, 8, 65537, 0, 0, 9, 64)
    add(case_a, 3, 3, 3, 8, 8, 8, 6, 0, 9, 64)     # x + w > dw
    add(case_a, 3, 3, 3, 8, 8, 8, 0, 6, 9, 64)
    add(case_a, 3, 3, 3, 8, 8, 8, 0xFFFFFFFF, 0, 9, 64)
    add(case_a, 3, 3, 3, 8, 8, 8, 0, 0xFFFFFFFF, 9, 64)
    add(case_a, 3, 3, 3, 8, 8, 8, 0xFFFFFFFE, 0xFFFFFFFE, 9, 64)
    add(case_a, 0, 0, 2, 0, 0, 1, 0, 0, 4, 4)       # BadSize before BadStride
    add(case_a, 4097, 3, 1, 8, 8, 8, 0xFFFFFFFF, 0, 4, 64)  # BadSize before BadStride and OutOfBounds
    add(case_a, 3, 3, 2, 8, 8, 8, 0xFFFFFFFF, 0, 4, 64)     # BadStride before OutOfBounds
    # random separate-buffer corpus
    for _ in range(160):
        w, h = r.rng(1, 12), r.rng(1, 12)
        ss = w + r.rng(0, 4)
        dw, dh = w + r.rng(0, 5), h + r.rng(0, 5)
        ds = dw + r.rng(0, 4)
        x, y = r.rng(0, dw - w), r.rng(0, dh - h)
        add(case_a, w, h, ss, dw, dh, ds, x, y)
    for _ in range(12):
        w, h = r.rng(1, 300), r.rng(1, 40)
        ss = w + r.rng(0, 30)
        dw, dh = min(4096, w + r.rng(0, 700)), h + r.rng(0, 60)
        ds = dw + r.rng(0, 20)
        add(case_a, w, h, ss, dw, dh, ds, r.rng(0, dw - w), r.rng(0, dh - h))
    # shared buffer: touch, overlap and disjoint placements
    for k in range(200):
        w, h = r.rng(1, 8), r.rng(1, 8)
        ss = w + r.rng(0, 3)
        dw, dh = w + r.rng(0, 4), h + r.rng(0, 4)
        ds = dw + r.rng(0, 3)
        x, y = r.rng(0, dw - w), r.rng(0, dh - h)
        wp, hp = min(w + 1, dw - x), min(h + 1, dh - y)
        sl = (h - 1) * ss + w
        d_off = r.rng(sl + 4, sl + 40)
        db, de = d_off + y * ds + x, d_off + (y + hp - 1) * ds + x + wp
        mode = k % 7
        if mode == 0: s_off = db - sl                    # src ends exactly where the dst span starts
        elif mode == 1: s_off = de                       # src starts exactly where the dst span ends
        elif mode == 2: s_off = db - sl + 1              # one byte of overlap at the front
        elif mode == 3: s_off = de - 1                   # one byte of overlap at the back
        elif mode == 4: s_off = r.rng(0, de + 4)         # anywhere
        elif mode == 5: s_off = de + r.rng(0, 3)         # after the span, maybe inside the dst buffer
        else: s_off = max(0, db - sl - r.rng(0, 3))      # before the span
        out.append(case_b(2 * n + 1000 + k, w, h, ss, dw, dh, ds, x, y, s_off, d_off))
    return out


def emit_cases(cs):
    o = ['struct BlitCase {u32 mode, w, h, ss, dw, dh, ds, x, y, srclen, dstlen, src_off, dst_off, memlen, status; u64 digest, seed;};',
         'const BlitCase kBlitCases[] = {']
    for c in cs:
        o.append('  {%s, %dull, %dull},' % (', '.join('%du' % v for v in c[:15]), c[15], c[16]))
    o.append('};')
    return o


def digest_cases():
    r = Lcg(0xE2)
    out = []
    for i in range(60):
        w, h = r.rng(1, 20), r.rng(1, 20)
        st = w + r.rng(0, 6)
        n = (h - 1) * st + w
        out.append((i, w, h, st, n, digest(fill(7000 + i, n), w, h, st)))
    out.append((100, 4096, 3, 4096, 3 * 4096, digest(fill(7100, 3 * 4096), 4096, 3, 4096)))
    out.append((101, 3, 4096, 3, 4096 * 3, digest(fill(7101, 4096 * 3), 3, 4096, 3)))
    out.append((102, 1, 1, 65536, 1, digest(fill(7102, 1), 1, 1, 65536)))
    for j, (w, h, st) in enumerate(((0, 4, 4), (4, 0, 4), (4097, 2, 4097), (2, 4097, 2), (4, 4, 3), (4, 4, 65537))):
        out.append((200 + j, w, h, st, 4, digest(fill(7200 + j, 4), w, h, st)))
    o = ['struct DigestCase {u32 seed, w, h, stride, len; u64 digest;};', 'const DigestCase kDigestCases[] = {']
    for i, w, h, st, n, d in out:
        seed = 7000 + i if i < 100 else 7000 + i
        o.append('  {%du, %du, %du, %du, %du, %dull},' % (seed, w, h, st, n, d))
    o.append('};')
    return o


def main():
    cs = cases()
    o = emit_cases(cs)
    o += digest_cases()
    o.append('const u64 kBasis = %dull;' % fnv_bytes(b''))
    o.append('const u64 kSmallDigest = %dull;  // 2 x 2 pixels 1 2 / 3 4 with stride 3 (stride padding byte 99)' % digest(bytearray([1, 2, 99, 3, 4]), 2, 2, 3))
    cnt = {}
    for c in cs: cnt[c[14]] = cnt.get(c[14], 0) + 1
    o.append('// status counts: %s' % ', '.join('%d:%d' % kv for kv in sorted(cnt.items())))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
