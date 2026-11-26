#!/usr/bin/env python3
"""Independent reference for core/glyphcache (scope AC). Exact integers only.

It models the cache as 128 slots of (key, value, last_used) or None, with its own tick, counters and FNV, written from the rules in
glyph_cache.h. Running it prints the constants that tests/glyphcache/glyph_cache_tests.cpp pastes: scenario programs with their
results and digests, and the random corpus digests.
Usage: python3 tools/glyph_cache_ref.py > data.inc
"""
M64 = (1 << 64) - 1
CAP = 128
MAXT = M64
INS, UPD, EVI, NULLC, BADK = 0, 1, 2, 3, 4


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


def sat(v): return v + 1 if v < MAXT else v


class Model:
    def __init__(s):
        s.clear()
    def clear(s):
        s.slots = [None] * CAP          # (key tuple, value, last_used) or None
        s.tick = 0
        s.count = 0
        s.hits = s.misses = s.evictions = 0
    @staticmethod
    def valid(k): return k[3] <= 3 and k[2] >= 1
    def where(s, k):
        for i, e in enumerate(s.slots):
            if e is not None and e[0] == k: return i
        return None
    def find(s, k):
        if not s.valid(k): return (False, 0)
        i = s.where(k)
        if i is None:
            s.misses = sat(s.misses)
            return (False, 0)
        s.tick = sat(s.tick)
        e = s.slots[i]
        s.slots[i] = (e[0], e[1], s.tick)
        s.hits = sat(s.hits)
        return (True, e[1])
    def insert(s, k, v):
        if not s.valid(k): return BADK
        i = s.where(k)
        if i is not None:
            s.tick = sat(s.tick)
            s.slots[i] = (k, v, s.tick)
            return UPD
        target = None
        if s.count < CAP:
            for j in range(CAP):
                if s.slots[j] is None: target = j; break
        if target is not None:
            res = INS
            s.count += 1
        else:
            res = EVI
            used = [(e[2], j) for j, e in enumerate(s.slots) if e is not None]
            target = min(used)[1] if used else 0     # smallest last_used, then lowest index
            s.evictions = sat(s.evictions)
        s.tick = sat(s.tick)
        s.slots[target] = (k, v, s.tick)
        return res
    def digest(s):
        ws = []
        for e in s.slots:
            if e is None: continue
            k, v, lu = e
            ws += [k[0], k[1] | (k[2] << 16), k[3], v, lu & 0xFFFFFFFF, lu >> 32]
        for c in (s.hits, s.misses, s.evictions): ws += [c & 0xFFFFFFFF, c >> 32]
        return fnv_words(ws)
    def set_entry(s, i, k, v, lu, used):
        assert 0 <= lu <= M64 and 0 <= v <= 0xFFFFFFFF
        s.slots[i] = (k, v, lu) if used else None


def K(f, g, sz, sb): return (f, g, sz, sb)


def run(prog):
    """prog: list of ops. Returns (results words, model). Ops:
    ('I', f,g,sz,sb, value) insert; ('F', f,g,sz,sb) find; ('C',) clear; ('T', v) set tick; ('N', v) set count;
    ('E', idx, f,g,sz,sb, value, last_used, used) set a slot directly; ('H', hits, misses, evictions) set counters."""
    m = Model()
    res = []
    for op in prog:
        t = op[0]
        if t == 'I': res.append(m.insert(K(*op[1:5]), op[5]))
        elif t == 'F':
            ok, v = m.find(K(*op[1:5]))
            res += [1 if ok else 0, v]
        elif t == 'C': m.clear()
        elif t == 'T': m.tick = op[1]
        elif t == 'N': m.count = op[1]
        elif t == 'E': m.set_entry(op[1], K(*op[2:6]), op[6], op[7], op[8])
        elif t == 'H': m.hits, m.misses, m.evictions = op[1:4]
    return res, m


def cop(op):
    t = op[0]
    return '{\'%s\', %s}' % (t, ', '.join(str(x) + ('ull' if i == 'big' else '') for i, x in [(0, v) for v in op[1:]]) if len(op) > 1 else '0')


def row(op):
    t = op[0]
    a = list(op[1:]) + [0] * 9
    return "  {'%s', {%s}}," % (t, ', '.join('%dull' % x for x in a[:8]))


def scenario(name, prog):
    res, m = run(prog)
    o = ['const Op kProg_%s[] = {' % name]
    o += [row(p) for p in prog]
    o.append('};')
    o.append('const Expect kExp_%s = {%dull, %dull, %d, %dull, %dull, %dull, %dull, %d};' % (
        name, fnv_words(res), m.digest(), m.count, m.tick, m.hits, m.misses, m.evictions, len(res)))
    return o


def main():
    o = []
    S = []
    # basic LRU: fill 128, find some, evict
    p = [('I', 1, i, 10, i % 4, 1000 + i) for i in range(128)]
    p += [('F', 1, 0, 10, 0), ('F', 1, 5, 10, 1), ('F', 9, 9, 9, 1)]
    p += [('I', 2, 7, 12, 3, 7)]                       # evicts glyph 1 (slot 1)
    p += [('I', 2, 8, 12, 3, 8), ('I', 2, 9, 12, 3, 9)]  # slot 2, slot 3 ... but slot 0 and 5 were refreshed
    p += [('I', 1, 0, 10, 0, 5555)]                    # update
    p += [('F', 1, 1, 10, 1), ('F', 1, 0, 10, 0)]
    S.append(('lru', p))
    # invalid keys change nothing
    p = [('I', 1, 1, 10, 0, 1), ('I', 1, 1, 0, 0, 2), ('I', 1, 1, 10, 4, 3), ('F', 1, 1, 0, 0), ('F', 1, 1, 10, 4), ('F', 1, 1, 10, 0),
         ('F', 5, 5, 5, 3), ('I', 0, 0, 1, 3, 9), ('I', 0xFFFFFFFF, 0xFFFF, 0xFFFF, 3, 0xFFFFFFFF), ('F', 0xFFFFFFFF, 0xFFFF, 0xFFFF, 3)]
    S.append(('invalid', p))
    # key equality needs all four fields
    p = [('I', 1, 2, 3, 0, 10), ('F', 2, 2, 3, 0), ('F', 1, 3, 3, 0), ('F', 1, 2, 4, 0), ('F', 1, 2, 3, 1), ('F', 1, 2, 3, 0)]
    S.append(('fields', p))
    # clear
    p = [('I', 1, i, 10, 0, i) for i in range(130)] + [('F', 1, 3, 10, 0), ('C',), ('F', 1, 129, 10, 0), ('I', 1, 1, 1, 1, 1)]
    S.append(('clear', p))
    # tick saturation: tick near the maximum, ties resolved by the lowest index
    p = [('T', MAXT - 3)] + [('I', 1, i, 10, 0, i) for i in range(6)] + [('F', 1, 0, 10, 0), ('F', 1, 5, 10, 0)]
    p += [('I', 1, 100 + i, 10, 0, 100 + i) for i in range(4)]
    p += [('N', 128)] + [('I', 2, i, 10, 0, 77) for i in range(3)]
    S.append(('tick_sat', p))
    # fill to 128 with saturated ticks, then evict among equal last_used
    p = [('T', MAXT - 1)] + [('I', 1, i, 10, 0, i) for i in range(130)] + [('F', 1, 129, 10, 0), ('F', 1, 3, 10, 0)]
    S.append(('tick_full', p))
    # counter saturation
    p = [('H', MAXT, MAXT - 1, MAXT), ('F', 1, 1, 1, 0), ('F', 1, 1, 1, 0), ('I', 1, 1, 1, 0, 1), ('F', 1, 1, 1, 0), ('F', 1, 1, 1, 0)]
    p += [('H', 5, MAXT, MAXT - 1)] + [('I', 1, i, 1, 0, i) for i in range(2, 131)] + [('F', 7, 7, 7, 1), ('F', 7, 7, 7, 1)]
    S.append(('counters', p))
    # inconsistent states
    full = [('E', i, 1, i, 10, 0, i, 1000 - i if i % 3 else 500, 1) for i in range(128)]
    S.append(('inc_count_low', full + [('N', 100), ('I', 3, 3, 3, 3, 3), ('I', 3, 4, 3, 3, 4), ('N', 0), ('I', 3, 5, 3, 3, 5)]))
    part = [('E', i, 1, i, 10, 0, i, 10 + i, 1) for i in range(0, 128, 2)]
    S.append(('inc_count_high', part + [('N', 128), ('I', 3, 3, 3, 3, 3), ('N', 500), ('I', 3, 4, 3, 3, 4), ('F', 3, 3, 3, 3), ('F', 1, 2, 10, 0)]))
    S.append(('inc_none_used', [('N', 128), ('I', 3, 3, 3, 3, 3), ('I', 3, 4, 3, 3, 4), ('N', 5), ('I', 3, 5, 3, 3, 5), ('I', 3, 6, 3, 3, 6)]))
    S.append(('inc_count_low_free', [('E', 5, 1, 1, 1, 0, 1, 1, 1), ('N', 0), ('I', 1, 2, 1, 0, 2), ('I', 1, 3, 1, 0, 3), ('N', 127), ('I', 1, 4, 1, 0, 4), ('I', 1, 5, 1, 0, 5)]))
    S.append(('inc_unused_flag', [('E', 0, 1, 1, 1, 0, 9, 3, 0), ('E', 1, 1, 1, 1, 0, 8, 4, 1), ('N', 1), ('F', 1, 1, 1, 0), ('I', 1, 1, 1, 0, 7), ('I', 1, 2, 1, 0, 6)]))
    S.append(('inc_ties', [('E', 0, 1, 1, 1, 0, 1, 7, 1), ('E', 1, 1, 2, 1, 0, 1, 3, 1), ('E', 2, 1, 3, 1, 0, 1, 3, 1), ('E', 3, 1, 4, 1, 0, 1, 3, 1), ('N', 4),
                         ('N', 128), ('I', 2, 2, 2, 2, 2), ('I', 2, 3, 2, 2, 3)]))
    s0 = [('E', i, 1, i, 10, 0, i, 20 + (i * 7) % 50, 1) for i in range(1, 128)]
    S.append(('inc_slot0_unused', s0 + [('N', 128), ('I', 3, 3, 3, 3, 3), ('I', 3, 4, 3, 3, 4), ('N', 127), ('I', 3, 5, 3, 3, 5)]))
    for name, prog in S: o += scenario(name, prog)
    o.append('const ProgRef kProgs[] = {%s};' % ', '.join('{kProg_%s, sizeof(kProg_%s) / sizeof(kProg_%s[0]), kExp_%s}' % ((n,) * 4) for n, _ in S))
    # random corpora: ops from an LCG over a small key universe, checkpoints every 64 ops
    for seed, universe, nops in ((0xC1, 200, 6000), (0xC2, 130, 6000), (0xC3, 40, 3000)):
        r = Lcg(seed)
        m = Model()
        ws = []
        for k in range(nops):
            x = r.rng(0, 999)
            u = r.rng(0, universe - 1)
            key = K(u % 3, u // 3, 1 + (u % 5), u % 4)
            if x < 550:
                ws.append(m.insert(key, r.rng(0, 1000000)))
            elif x < 920:
                ok, v = m.find(key)
                ws += [1 if ok else 0, v]
            elif x < 960:
                ws.append(m.insert(K(key[0], key[1], r.rng(0, 1), r.rng(3, 5)), 1))
            elif x < 999:
                ok, v = m.find(K(key[0], key[1], key[2], 4))
                ws += [1 if ok else 0, v]
            else:
                m.clear()
            if k % 64 == 63:
                d = m.digest()
                ws += [d & 0xFFFFFFFF, d >> 32, m.count]
        o.append('const Expect kCorpus_%x = {%dull, %dull, %d, %dull, %dull, %dull, %dull, %d};' % (
            seed, fnv_words(ws), m.digest(), m.count, m.tick, m.hits, m.misses, m.evictions, len(ws)))
    # digest of a fixed small cache (hand check in the tests)
    m = Model()
    m.insert(K(1, 2, 3, 1), 10)
    m.insert(K(4, 5, 6, 3), 20)
    m.find(K(1, 2, 3, 1))
    m.find(K(9, 9, 9, 0))
    o.append('const u64 kSmallDigest = %dull;' % m.digest())
    o.append('const u64 kBasisDigest = %dull;  // a null cache hashes nothing' % fnv_words([]))
    o.append('const u64 kEmptyDigest = %dull;' % Model().digest())
    print('\n'.join(o))


if __name__ == '__main__':
    main()
