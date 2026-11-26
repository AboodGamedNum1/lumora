#!/usr/bin/env python3
"""Independent reference for core/shelfpack (scope AD). Exact integers only.

It models the packer with Python integers (no fixed width wrap except where the header says unsigned 64-bit), its own shelf choice and
its own FNV, written from the rules in shelf_pack.h. Running it prints the constants that tests/shelfpack/shelf_pack_tests.cpp pastes:
scenario programs with their results and digests, and seeded random corpora.
Usage: python3 tools/shelf_pack_ref.py > data.inc
"""
M64 = (1 << 64) - 1
MAXATLAS = 4096
MAXSHELVES = 64
PAD = 1
OK, NULLARG, NOTINIT, BADSIZE, NOSPACE = 0, 1, 2, 3, 4


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


class Model:
    def __init__(s):
        s.width = s.height = 0
        s.clear()
    def clear(s):
        s.shelf_count = 0
        s.next_y = 0
        s.used_area = 0
        s.shelves = [[0, 0, 0] for _ in range(MAXSHELVES)]   # y, height, used_x
    def init(s, w, h):
        if w == 0 or h == 0 or w > MAXATLAS or h > MAXATLAS: return False
        s.width, s.height = w, h
        s.clear()
        return True
    def reset(s):
        if s.width == 0 or s.height == 0: return False
        s.clear()
        return True
    def alloc(s, w, h):
        """Returns (status, x, y)."""
        if s.width == 0 or s.height == 0: return (NOTINIT, 0, 0)
        cw, ch = w + PAD, h + PAD                     # Python integers: no wrap
        if w == 0 or h == 0 or cw > s.width or ch > s.height: return (BADSIZE, 0, 0)
        best = None
        for i in range(min(s.shelf_count, MAXSHELVES)):
            y, sh, ux = s.shelves[i]
            if sh >= ch and ux + cw <= s.width:
                if best is None or sh < s.shelves[best][1]: best = i
        if best is not None:
            y, sh, ux = s.shelves[best]
            s.shelves[best][2] = ux + cw
            x, yy = ux, y
        else:
            if s.shelf_count >= MAXSHELVES or s.next_y + ch > s.height: return (NOSPACE, 0, 0)
            s.shelves[s.shelf_count] = [s.next_y, ch, cw]
            x, yy = 0, s.next_y
            s.next_y += ch
            s.shelf_count += 1
        s.used_area = min(M64, s.used_area + w * h)
        return (OK, x, yy)
    def digest(s):
        ws = [s.width, s.height, s.shelf_count, s.next_y, s.used_area & 0xFFFFFFFF, s.used_area >> 32]
        for i in range(min(s.shelf_count, MAXSHELVES)): ws += s.shelves[i]
        return fnv_words(ws)


def run(prog):
    """Ops: ('I', w, h) init; ('R',) reset; ('A', w, h) alloc; ('P', width, height, shelf_count, next_y, used_area) set fields;
    ('H', idx, y, height, used_x) set a shelf."""
    m = Model()
    res = []
    for op in prog:
        t = op[0]
        if t == 'I': res.append(1 if m.init(op[1], op[2]) else 0)
        elif t == 'R': res.append(1 if m.reset() else 0)
        elif t == 'A':
            st, x, y = m.alloc(op[1], op[2])
            res += [st, x, y]
        elif t == 'P': m.width, m.height, m.shelf_count, m.next_y, m.used_area = op[1:6]
        elif t == 'H': m.shelves[op[1]] = list(op[2:5])
    return res, m


def row(op):
    a = list(op[1:]) + [0] * 6
    return "  {'%s', {%s}}," % (op[0], ', '.join('%dull' % x for x in a[:5]))


def scenario(name, prog):
    res, m = run(prog)
    o = ['const Op kProg_%s[] = {' % name] + [row(p) for p in prog] + ['};']
    o.append('const Expect kExp_%s = {%dull, %dull, %d, %d, %d, %dull, %d};' % (
        name, fnv_words(res), m.digest(), m.shelf_count, m.next_y, m.width, m.used_area, len(res)))
    return o


def main():
    o = []
    S = []
    S.append(('basic', [('A', 4, 4), ('I', 64, 64), ('A', 4, 4), ('A', 4, 4), ('A', 10, 3), ('A', 4, 4), ('A', 0, 4), ('A', 4, 0),
                        ('A', 64, 4), ('A', 63, 4), ('A', 4, 64), ('A', 4, 63), ('A', 0xFFFFFFFF, 1), ('A', 1, 0xFFFFFFFF),
                        ('A', 0xFFFFFFFE, 1)]))
    S.append(('shelf_choice', [('I', 100, 100), ('A', 9, 9), ('A', 9, 19), ('A', 9, 4), ('A', 9, 4), ('A', 9, 9), ('A', 9, 14), ('A', 9, 19),
                               ('A', 70, 4), ('A', 20, 8)]))
    S.append(('row_fill', [('I', 33, 40)] + [('A', 5, 5)] * 7 + [('A', 5, 5), ('A', 3, 3), ('A', 31, 5), ('A', 32, 5)]))
    S.append(('vertical', [('I', 20, 31)] + [('A', 19, 4)] * 8 + [('R',), ('A', 19, 4)]))
    S.append(('shelf_limit', [('I', 4096, 4096)] + [('A', 1, 1 + i) for i in range(70)] + [('A', 1, 1), ('A', 200, 4000)]))
    S.append(('no_space', [('I', 16, 16), ('A', 15, 15), ('A', 1, 1), ('A', 15, 1)]))
    S.append(('init_errors', [('A', 1, 1), ('R',), ('I', 0, 5), ('I', 5, 0), ('I', 4097, 5), ('I', 5, 4097), ('I', 4096, 4096), ('A', 4095, 4095),
                              ('A', 4096, 1), ('I', 8, 8), ('A', 7, 7), ('A', 1, 1), ('I', 4097, 8), ('A', 1, 1), ('I', 6, 6), ('A', 1, 1)]))
    S.append(('reset', [('I', 50, 50), ('A', 10, 10), ('A', 20, 20), ('R',), ('A', 5, 5), ('R',), ('R',)]))
    S.append(('area', [('I', 4096, 4096), ('A', 4094, 4094), ('A', 1, 1), ('A', 2, 1)]))
    # hand-built fields
    S.append(('hb_count_high', [('I', 40, 40), ('P', 40, 40, 100, 0, 0), ('H', 0, 0, 10, 0), ('H', 63, 5, 6, 0), ('A', 3, 3), ('A', 30, 30)]))
    S.append(('hb_shelf_count_64', [('I', 40, 40), ('P', 40, 40, 64, 2, 0), ('H', 0, 0, 3, 38), ('H', 1, 3, 9, 0), ('A', 2, 2), ('A', 5, 5), ('A', 5, 20)]))
    S.append(('hb_tie_and_height', [('I', 40, 40), ('P', 40, 40, 3, 30, 7), ('H', 0, 0, 12, 0), ('H', 1, 12, 12, 0), ('H', 2, 24, 6, 35),
                                    ('A', 3, 3), ('A', 3, 3), ('A', 3, 9)]))
    S.append(('hb_big_fields', [('I', 40, 40), ('P', 40, 40, 1, 0xFFFFFFFF, M64 - 5), ('H', 0, 7, 10, 0xFFFFFFFF), ('A', 3, 3), ('A', 1, 1)]))
    S.append(('hb_big_next_y', [('I', 40, 40), ('P', 40, 40, 0, 0xFFFFFFFE, 0), ('A', 3, 3), ('A', 1, 1)]))
    S.append(('hb_uninit', [('P', 0, 5, 0, 0, 0), ('A', 1, 1), ('R',), ('P', 5, 0, 0, 0, 0), ('A', 1, 1), ('R',), ('P', 5000, 5000, 0, 0, 0), ('A', 100, 100), ('A', 4999, 4999)]))
    S.append(('hb_area_sat', [('I', 4096, 4096), ('P', 4096, 4096, 0, 0, M64), ('A', 2, 3), ('A', 1, 1), ('P', 4096, 4096, 0, 0, M64 - 6), ('A', 2, 3), ('A', 1, 1)]))
    S.append(('hb_area_edge', [('I', 4096, 4096), ('P', 4096, 4096, 0, 0, M64 - 7), ('A', 2, 3), ('A', 1, 1)]))
    S.append(('hb_zero_shelf', [('I', 30, 30), ('P', 30, 30, 2, 5, 0), ('H', 0, 0, 0, 0), ('H', 1, 0, 6, 29), ('A', 3, 3), ('A', 1, 1)]))
    for name, prog in S: o += scenario(name, prog)
    o.append('const ProgRef kProgs[] = {%s};' % ', '.join('{kProg_%s, sizeof(kProg_%s) / sizeof(kProg_%s[0]), kExp_%s}' % ((n,) * 4) for n, _ in S))
    # random corpora: (seed, atlas w, h, max rect size, ops)
    for seed, aw, ah, mx, nops in ((0xD1, 256, 256, 24, 3000), (0xD2, 1024, 512, 40, 3000), (0xD3, 64, 4096, 12, 3000), (0xD4, 4096, 4096, 300, 3000)):
        r = Lcg(seed)
        m = Model()
        m.init(aw, ah)
        ws = []
        for k in range(nops):
            x = r.rng(0, 999)
            if x < 940:
                st, px, py = m.alloc(r.rng(1, mx), r.rng(1, mx))
                ws += [st, px, py]
            elif x < 970:
                st, px, py = m.alloc(r.rng(0, 1) * r.rng(1, mx), r.rng(0, mx))
                ws += [st, px, py]
            elif x < 995:
                ws.append(1 if m.reset() else 0)
            else:
                ws.append(1 if m.init(r.rng(1, 5000), r.rng(1, 5000)) else 0)
            if k % 50 == 49:
                d = m.digest()
                ws += [d & 0xFFFFFFFF, d >> 32]
        o.append('const Expect kCorpus_%x = {%dull, %dull, %d, %d, %d, %dull, %d};' % (
            seed, fnv_words(ws), m.digest(), m.shelf_count, m.next_y, m.width, m.used_area, len(ws)))
    o.append('const u64 kBasisDigest = %dull;' % fnv_words([]))
    m = Model(); m.init(100, 50); m.alloc(9, 9); m.alloc(9, 4); m.alloc(20, 3)
    o.append('const u64 kSmallDigest = %dull;  // 100x50: allocs 9x9, 9x4, 20x3' % m.digest())
    print('\n'.join(o))


if __name__ == '__main__':
    main()
