#!/usr/bin/env python3
"""Independent reference for core/boxfilter (scope AG). Exact Python integers; memory is a bytearray, addresses are offsets.

Written from the rules in box_filter.h; it shares no code with the implementation. Running it prints the constants that
tests/boxfilter/box_filter_tests.cpp pastes: downscale cases on separate buffers and on one shared buffer (status, output size and the
digest of every final byte). Usage: python3 tools/box_filter_ref.py > data.inc
"""
M64 = (1 << 64) - 1
MAXDIM = 4096
MAXSTRIDE = 65536
MAXFACTOR = 16
OK, NULLARG, BADSIZE, BADSTRIDE, BADFACTOR, OVERLAP = 0, 1, 2, 3, 4, 5


def fnv_bytes(bs):
    h = 0xcbf29ce484222325
    for b in bs:
        h ^= b
        h = (h * 0x100000001b3) & M64
    return h


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


def downscale(src, src_addr, dst, dst_addr, w, h, ss, f, ds, sbase=0, dbase=0):
    """Returns (status, out_w, out_h). dst is modified in place on success (src and dst may be the same bytearray)."""
    if w == 0 or h == 0 or w > MAXDIM or h > MAXDIM: return BADSIZE, 0, 0
    if ss < w or ss > MAXSTRIDE: return BADSTRIDE, 0, 0
    if f == 0 or f > MAXFACTOR or f > w or f > h: return BADFACTOR, 0, 0
    ow, oh = w // f, h // f
    if ds < ow or ds > MAXSTRIDE: return BADSTRIDE, 0, 0
    sb, se = src_addr, src_addr + (oh * f - 1) * ss + ow * f
    db, de = dst_addr, dst_addr + (oh - 1) * ds + ow
    if sb < de and db < se: return OVERLAP, 0, 0
    n = f * f
    px = []
    for oy in range(oh):
        for ox in range(ow):
            total = 0
            for dy in range(f):
                for dx in range(f):
                    total += src[sbase + (oy * f + dy) * ss + ox * f + dx]
            px.append((oy, ox, (total + n // 2) // n))
    for oy, ox, v in px: dst[dbase + oy * ds + ox] = v
    return OK, ow, oh


def spans(w, h, ss, f, ds):
    ow, oh = w // f, h // f
    return (oh * f - 1) * ss + ow * f, (oh - 1) * ds + ow


def case_a(i, w, h, ss, f, ds, srclen=None, dstlen=None):
    ow, oh = w // f if f else 0, h // f if f else 0
    sl = (h - 1) * ss + w if srclen is None else srclen
    dl = ((oh - 1) * ds + ow if oh else 1) if dstlen is None else dstlen
    src, dst = fill(2 * i + 1, sl), fill(2 * i + 2, dl)
    st, a, b = downscale(src, 1 << 40, dst, 1 << 50, w, h, ss, f, ds)
    return (0, w, h, ss, f, ds, sl, dl, 0, 0, 0, st, a, b, fnv_bytes(dst), 2 * i + 1)


def case_b(i, w, h, ss, f, ds, src_off, dst_off):
    ow, oh = w // f, h // f
    sl = (h - 1) * ss + w
    dl = (oh - 1) * ds + ow
    ml = max(src_off + sl, dst_off + dl)
    mem = fill(2 * i + 1, ml)
    st, a, b = downscale(mem, src_off, mem, dst_off, w, h, ss, f, ds, src_off, dst_off)
    return (1, w, h, ss, f, ds, sl, dl, src_off, dst_off, ml, st, a, b, fnv_bytes(mem), 2 * i + 1)


def cases():
    out = []
    n = [0]
    def add(f, *a, **k):
        out.append(f(n[0], *a, **k)); n[0] += 1
    # hand shaped valid cases
    add(case_a, 1, 1, 1, 1, 1)
    add(case_a, 4, 4, 4, 1, 4)          # f = 1 copies
    add(case_a, 4, 3, 7, 1, 6)          # f = 1 with strides
    add(case_a, 4, 4, 4, 2, 2)
    add(case_a, 4, 4, 6, 2, 5)
    add(case_a, 5, 7, 5, 2, 2)          # a column and a row are ignored
    add(case_a, 5, 7, 9, 2, 3)
    add(case_a, 3, 3, 3, 3, 1)
    add(case_a, 7, 7, 7, 3, 2)
    add(case_a, 16, 16, 16, 16, 1)
    add(case_a, 31, 17, 40, 16, 4)
    add(case_a, 4096, 16, 4096, 16, 256)
    add(case_a, 4095, 17, 4095, 16, 255)
    add(case_a, 4096, 4, 4096, 4, 1024)
    add(case_a, 2, 4096, 2, 2, 1)
    add(case_a, 17, 33, 17, 16, 1)
    add(case_a, 8, 8, 65536, 4, 65536)  # strides at the limit
    add(case_a, 6, 6, 6, 5, 1)
    # errors
    add(case_a, 0, 4, 4, 2, 2, 4, 4)
    add(case_a, 4, 0, 4, 2, 2, 4, 4)
    add(case_a, 4097, 4, 4097, 2, 2048, 4, 4)
    add(case_a, 4, 4097, 4, 2, 2, 4, 4)
    add(case_a, 4, 4, 3, 2, 2, 4, 4)            # src_stride < w
    add(case_a, 4, 4, 65537, 2, 2, 4, 4)
    add(case_a, 4, 4, 4, 0, 2, 4, 4)            # factor 0
    add(case_a, 4, 4, 4, 17, 2, 4, 4)
    add(case_a, 4, 4, 4, 0xFFFFFFFF, 2, 4, 4)
    add(case_a, 3, 8, 3, 4, 2, 4, 4)            # f > w
    add(case_a, 8, 3, 8, 4, 2, 4, 4)            # f > h
    add(case_a, 8, 8, 8, 4, 1, 4, 4)            # dst_stride < out_w (2)
    add(case_a, 8, 8, 8, 2, 3, 4, 4)            # dst_stride 3 < 4
    add(case_a, 8, 8, 8, 2, 65537, 4, 4)
    add(case_a, 4097, 4, 1, 0, 0, 4, 4)         # BadSize before BadStride and BadFactor
    add(case_a, 4, 4, 3, 0, 0, 4, 4)            # BadStride before BadFactor
    add(case_a, 4, 4, 4, 5, 0, 4, 4)            # BadFactor before dst stride
    add(case_a, 16, 16, 16, 16, 0, 4, 4)        # dst_stride 0 < out_w 1
    # random separate corpus
    r = Lcg(0xA1)
    for _ in range(150):
        f = r.rng(1, 5)
        w, h = r.rng(f, 14), r.rng(f, 14)
        ss = w + r.rng(0, 4)
        ds = w // f + r.rng(0, 4)
        add(case_a, w, h, ss, f, ds)
    for _ in range(14):
        f = r.rng(1, 16)
        w, h = r.rng(f, 150), r.rng(f, 60)
        ss = w + r.rng(0, 30)
        ds = w // f + r.rng(0, 10)
        add(case_a, w, h, ss, f, ds)
    # random invalid arguments (small buffers: the function must not read or write them)
    for _ in range(40):
        w, h = r.rng(0, 6), r.rng(0, 6)
        f = r.rng(0, 18)
        ss = r.rng(0, 8)
        ds = r.rng(0, 4)
        ow = w // f if f else 0
        if w > 0 and h > 0 and ss >= w and 1 <= f <= min(16, w, h) and ds >= ow:
            continue
        add(case_a, w, h, ss, f, ds, 8, 8)
    # shared buffer: touching, overlapping, disjoint
    for k in range(220):
        f = r.rng(1, 3)
        w, h = r.rng(f, 9), r.rng(f, 9)
        ss = w + r.rng(0, 3)
        ds = w // f + r.rng(0, 3)
        ssp, dsp = spans(w, h, ss, f, ds)
        mode = k % 8
        if mode == 0: s_off, d_off = 0, ssp + r.rng(0, 3) if r.rng(0, 1) else ssp      # dst starts at/after the src span end
        elif mode == 1: d_off = 0; s_off = dsp                                         # src starts at the dst span end
        elif mode == 2: s_off = 0; d_off = ssp - 1                                     # one byte of overlap
        elif mode == 3: d_off = 0; s_off = dsp - 1
        elif mode == 4: s_off, d_off = r.rng(0, 30), r.rng(0, 30)
        elif mode == 5: s_off = r.rng(0, 5); d_off = s_off + (h - 1) * ss + w         # dst after the whole source buffer
        elif mode == 6: s_off = 0; d_off = r.rng(ssp, (h - 1) * ss + w + 2)            # dst in the unused source tail
        else: d_off = r.rng(0, 5); s_off = d_off + r.rng(0, dsp + 2)
        out.append(case_b(n[0] + 5000, w, h, ss, f, ds, s_off, d_off)); n[0] += 1
    return out


def main():
    cs = cases()
    o = ['struct Case {u32 mode, w, h, ss, f, ds, srclen, dstlen, src_off, dst_off, memlen, status, ow, oh; u64 digest, seed;};',
         'const Case kCases[] = {']
    for c in cs:
        o.append('  {%s, %dull, %dull},' % (', '.join('%du' % v for v in c[:14]), c[14], c[15]))
    o.append('};')
    # constant blocks and a hand digest
    src = bytearray([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160])
    dst = bytearray(4)
    downscale(src, 1 << 40, dst, 1 << 50, 4, 4, 4, 2, 2)
    o.append('// 4 x 4 ramp 10..160 by 2 gives %s' % list(dst))
    cnt = {}
    for c in cs: cnt[c[11]] = cnt.get(c[11], 0) + 1
    o.append('// status counts: %s' % ', '.join('%d:%d' % kv for kv in sorted(cnt.items())))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
