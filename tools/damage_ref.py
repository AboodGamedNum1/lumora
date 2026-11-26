#!/usr/bin/env python3
"""Independent reference for core/damage (scope AH). Exact Python integers, rectangles as tuples (x, y, w, h).

Written from the rules in damage.h and rect.h; it shares no code with the implementation. A program is a list of ops on one damage set:
'A' add(rect, bounds), 'C' clear, 'H' write rects[idx] directly (hand-built sets), 'N' write count directly. After every op the model
records a snapshot (size, total area, bounding box, every get). Running it prints the constants pasted into tests/damage/damage_tests.cpp:
scenario programs with the digest of all recorded words and the final state, and seeded random corpora. Usage: python3 tools/damage_ref.py
"""
M64 = (1 << 64) - 1
IMAX = (1 << 31) - 1
IMIN = -(1 << 31)
KMAX = 8


def fnv_words(ws):
    h = 0xcbf29ce484222325
    for v in ws:
        v &= 0xFFFFFFFF
        for k in range(4):
            h ^= (v >> (8 * k)) & 0xFF
            h = (h * 0x100000001b3) & M64
    return h


def valid(r):
    x, y, w, h = r
    return w >= 0 and h >= 0 and x + w <= IMAX and y + h <= IMAX


def empty(r):
    return (not valid(r)) or r[2] == 0 or r[3] == 0


def inter(a, b):
    if empty(a) or empty(b): return (0, 0, 0, 0)
    l, t = max(a[0], b[0]), max(a[1], b[1])
    rt, bt = min(a[0] + a[2], b[0] + b[2]), min(a[1] + a[3], b[1] + b[3])
    if rt <= l or bt <= t: return (0, 0, 0, 0)
    return (l, t, rt - l, bt - t)


def ovl(a, b): return not empty(inter(a, b))


def union(a, b):
    """Returns the rectangle or None when it does not fit."""
    if empty(a) and empty(b): return (0, 0, 0, 0)
    if empty(a): return b
    if empty(b): return a
    l, t = min(a[0], b[0]), min(a[1], b[1])
    rt, bt = max(a[0] + a[2], b[0] + b[2]), max(a[1] + a[3], b[1] + b[3])
    if rt - l > IMAX or bt - t > IMAX: return None
    return (l, t, rt - l, bt - t)


def area(r): return 0 if empty(r) else r[2] * r[3]


class Set:
    def __init__(s):
        s.rects = [(0, 0, 0, 0)] * KMAX
        s.count = 0
    def corrupt(s): return s.count > KMAX
    def add(s, r, bounds):
        if s.corrupt(): return False
        cur = inter(r, bounds)
        if empty(cur): return True
        used = list(s.rects[:s.count])
        while True:
            hit = None
            for i, u in enumerate(used):
                if ovl(cur, u):
                    hit = i
                    break
            if hit is None: break
            u = union(cur, used[hit])
            if u is None: return False
            cur = u
            del used[hit]
        if len(used) < KMAX:
            used.append(cur)
        else:
            box = (0, 0, 0, 0)
            for u in used + [cur]:
                box = union(box, u)
                if box is None: return False
            used = [box]
        for i, u in enumerate(used): s.rects[i] = u
        s.count = len(used)
        return True
    def size(s): return 0 if s.corrupt() else s.count
    def total(s): return 0 if s.corrupt() else sum(area(u) for u in s.rects[:s.count]) & M64
    def bbox(s):
        if s.corrupt(): return (0, 0, 0, 0)
        box = (0, 0, 0, 0)
        for u in s.rects[:s.count]:
            box = union(box, u)
            if box is None: return (0, 0, 0, 0)
        return box
    def get(s, i):
        if s.corrupt() or i >= s.count: return None
        return s.rects[i]
    def state_words(s):
        ws = [s.count]
        if not s.corrupt():
            for u in s.rects[:s.count]: ws += list(u)
        return ws


def snapshot(s):
    t = s.total()
    ws = [s.size(), t & 0xFFFFFFFF, t >> 32] + list(s.bbox())
    for i in range(s.size() + 1):
        g = s.get(i)
        ws += [1] + list(g) if g is not None else [0]
    return ws


def run(prog):
    s = Set()
    ws = []
    for op in prog:
        k = op[0]
        if k == 'A': ws.append(1 if s.add(tuple(op[1:5]), tuple(op[5:9])) else 0)
        elif k == 'C': s.count = 0
        elif k == 'H': s.rects[op[1]] = tuple(op[2:6])
        elif k == 'N': s.count = op[1]
        ws += snapshot(s)
    return ws, s


def u32(v): return v & 0xFFFFFFFF


class Lcg:
    def __init__(s, seed): s.s = seed & M64
    def next(s):
        s.s = (s.s * 6364136223846793005 + 1442695040888963407) & M64
        return s.s >> 32
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


def opline(op):
    v = list(op[1:]) + [0] * 9
    return "  {'%s', {%s}}," % (op[0], ', '.join('%dll' % x for x in v[:9]))


def scenario(name, prog):
    ws, s = run(prog)
    o = ['const Op kProg_%s[] = {' % name] + [opline(p) for p in prog] + ['};']
    o.append('const Expect kExp_%s = {%dull, %dull, %d, %dull, %d};' % (name, fnv_words(ws), fnv_words(s.state_words()), s.count, s.total(), len(ws)))
    return o


W = (0, 0, 100, 100)


def A(x, y, w, h, b=W): return ('A', x, y, w, h) + tuple(b)


def main():
    S = []
    S.append(('basic', [A(0, 0, 10, 10), A(20, 0, 10, 10), A(0, 20, 5, 5), ('C',), A(1, 1, 2, 2)]))
    S.append(('clip', [A(-5, -5, 10, 10), A(95, 95, 10, 10), A(200, 200, 5, 5), A(0, 0, 0, 5), A(0, 0, 5, 0), A(10, 10, -3, 4),
                       A(10, 10, 4, 4, (0, 0, 0, 0)), A(10, 10, 4, 4, (20, 20, 5, 5)), A(10, 10, 4, 4, (0, 0, -5, 5))]))
    S.append(('touch_no_merge', [A(0, 0, 10, 10), A(10, 0, 10, 10), A(0, 10, 10, 10), A(10, 10, 10, 10), A(20, 0, 5, 5)]))
    S.append(('merge_one', [A(0, 0, 10, 10), A(30, 0, 10, 10), A(5, 5, 10, 10)]))
    S.append(('merge_chain', [A(0, 0, 10, 10), A(12, 0, 10, 10), A(24, 0, 10, 10), A(5, 0, 25, 4)]))
    S.append(('merge_restart', [A(0, 0, 10, 10), A(40, 0, 10, 10), A(0, 40, 10, 10), A(8, 8, 36, 4), A(0, 0, 3, 50)]))
    S.append(('reorder_covered', [A(0, 0, 10, 10), A(20, 0, 10, 10), A(40, 0, 10, 10), A(22, 2, 3, 3), A(0, 0, 10, 10)]))
    S.append(('order_kept', [A(0, 0, 4, 4), A(10, 0, 4, 4), A(20, 0, 4, 4), A(30, 0, 4, 4), A(11, 1, 6, 6)]))
    S.append(('fill_eight', [A(i * 10, 0, 5, 5) for i in range(8)] + [A(0, 50, 5, 5)]))
    S.append(('fill_exact', [A(i * 10, 0, 5, 5) for i in range(8)]))
    S.append(('full_merge_not_collapse', [A(i * 10, 0, 5, 5) for i in range(8)] + [A(2, 2, 10, 2)]))
    S.append(('full_merge_three', [A(i * 10, 0, 5, 5) for i in range(8)] + [A(2, 2, 20, 2)]))
    S.append(('full_collapse_after', [A(i * 10, 0, 5, 5) for i in range(8)] + [A(60, 60, 5, 5), A(1, 1, 3, 3), ('C',), A(2, 2, 2, 2)]))
    S.append(('full_empty_add', [A(i * 10, 0, 5, 5) for i in range(8)] + [A(500, 500, 5, 5), A(0, 0, 0, 0)]))
    S.append(('full_covered', [A(i * 10, 0, 5, 5) for i in range(8)] + [A(0, 0, 5, 5)]))
    S.append(('clip_to_bounds_merge', [A(0, 0, 10, 10), A(-20, -20, 30, 30, (0, 0, 50, 50))]))
    S.append(('diff_bounds', [A(0, 0, 10, 10, (0, 0, 20, 20)), A(50, 50, 10, 10, (40, 40, 30, 30)), A(5, 5, 100, 100, (0, 0, 8, 8))]))
    # rectangles outside the current bounds can make a union exceed int32 (bounds b1 reach up to -1, b2 start at 0)
    b1 = (IMIN, IMIN, IMAX, IMAX)
    b2 = (0, 0, IMAX, IMAX)
    wide = (-2000000000, 0, IMAX, 100)
    S.append(('union_fail_merge', [A(0, 0, IMAX, 10, b2), A(5, 50, 4, 4, b2), A(-2000000000, 0, IMAX, 10, wide), A(-2000000000, 0, 10, 10, wide),
                                   A(-2000000000, 52, IMAX, 10, wide)]))
    S.append(('union_fit_exact', [A(0, 0, 147483647, 10, b2), A(-2000000000, 0, IMAX, 10, wide)]))
    S.append(('union_fail_collapse', [A(-2000000000, 0, 10, 10, b1)] + [A(i * 20, 0, 5, 5, b2) for i in range(7)] + [A(1900000000, 0, 10, 10, b2),
                                      A(0, 100, 5, 5, b2)]))
    S.append(('extreme_edges', [A(IMAX - 5, IMAX - 5, 5, 5, b2), A(IMIN, IMIN, 5, 5, b1), A(IMAX - 5, IMAX - 5, 6, 6, b2), A(0, 0, IMAX, IMAX, b2),
                                A(IMIN, IMIN, IMAX, IMAX, b1)]))
    S.append(('invalid_add', [A(IMAX, 0, 5, 5, b2), A(0, 0, -1, 5, b2), A(0, IMAX, 5, 1, b2), A(1, 1, 3, 3, (IMAX, 0, 5, 5))]))
    # hand-built sets
    S.append(('hb_corrupt', [A(0, 0, 5, 5), ('N', 9), A(10, 10, 5, 5), ('N', 0xFFFFFFFF), A(10, 10, 5, 5), ('C',), A(10, 10, 5, 5)]))
    S.append(('hb_stale_beyond_count', [('H', 5, 0, 0, 50, 50), ('H', 7, 3, 3, 9, 9), ('N', 2), ('H', 0, 0, 0, 4, 4), ('H', 1, 10, 0, 4, 4), A(2, 2, 4, 4)]))
    S.append(('hb_empty_invalid_stored', [('H', 0, 0, 0, 0, 5), ('H', 1, 0, 0, -3, 4), ('H', 2, IMAX, 0, 5, 5), ('H', 3, 10, 10, 5, 5), ('N', 4), A(0, 0, 50, 50)]))
    S.append(('hb_overlapping_stored', [('H', 0, 0, 0, 10, 10), ('H', 1, 5, 5, 10, 10), ('H', 2, 40, 40, 4, 4), ('N', 3), A(6, 6, 2, 2), A(0, 0, 1, 1)]))
    S.append(('hb_area_wrap', [('H', i, 0, 0, IMAX, IMAX) for i in range(8)] + [('N', 8)]))
    S.append(('hb_area_big_disjoint', [('H', 0, IMIN, IMIN, IMAX, IMAX), ('H', 1, 0, 0, IMAX - 1, IMAX - 1), ('N', 2)]))
    S.append(('hb_full_overlap_collapse', [('H', i, i, i, 20, 20) for i in range(8)] + [('N', 8), A(500, 500, 1, 1, (0, 0, 1000, 1000)), A(50, 50, 1, 1)]))
    o = []
    for name, prog in S: o += scenario(name, prog)
    o.append('const ProgRef kProgs[] = {%s};' % ', '.join('{kProg_%s, sizeof(kProg_%s) / sizeof(kProg_%s[0]), kExp_%s}' % ((n,) * 4) for n, _ in S))
    # random corpora: (seed, ops, world, max side, extreme share per mille)
    for seed, nops, world, side, ext in ((0xAA1, 3000, 60, 12, 0), (0xAA2, 3000, 200, 40, 0), (0xAA3, 2000, 30, 30, 0), (0xAA4, 2500, 1000, 100, 150)):
        r = Lcg(seed)
        s = Set()
        ws = []
        for k in range(nops):
            x = r.rng(0, 999)
            if x < 30:
                s.count = 0
            else:
                if r.rng(0, 999) < ext:
                    rx, ry = r.rng(IMIN, IMAX), r.rng(IMIN, IMAX)
                    rw, rh = r.rng(0, IMAX), r.rng(0, IMAX)
                    b = (IMIN, IMIN, IMAX, IMAX) if r.rng(0, 1) else (r.rng(IMIN, 0), r.rng(IMIN, 0), r.rng(0, IMAX), r.rng(0, IMAX))
                else:
                    rx, ry = r.rng(-5, world), r.rng(-5, world)
                    rw, rh = r.rng(0, side), r.rng(0, side)
                    if r.rng(0, 9) == 0: b = (r.rng(0, world // 2), r.rng(0, world // 2), r.rng(0, world), r.rng(0, world))
                    else: b = (0, 0, world, world)
                ws.append(1 if s.add((rx, ry, rw, rh), b) else 0)
            if k % 10 == 9: ws += snapshot(s)
        o.append('const CorpusRef kCorpus_%x = {%dull, %dull, %du, %d, %d, %du, %d, %d};' % (seed, fnv_words(ws), fnv_words(s.state_words()), len(ws), seed, nops, world, side, ext))
    ws, s = run([A(0, 0, 10, 10), A(20, 0, 10, 10)])
    o.append('const u64 kSmallState = %dull;  // two rectangles (0,0,10,10) and (20,0,10,10)' % fnv_words(s.state_words()))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
