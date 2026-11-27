#!/usr/bin/env python3
"""Independent reference for core/sortu (scope AO). Exact Python integers.

Written from the rules in sortu.h; it shares no code with the implementation. The stable sort is an LSD method on Python lists of
(key, value) pairs in base 256 with explicit buckets (bucket lists appended in order), a different shape from the C++ count table and
prefix offsets. Python's own sorted() with a key is only a second check inside this script (assertions on keys and on the stable
order of values). Scenarios are (n, seed, mode); keys come from a seeded generator mirrored by the C++ test, values are the original
index times an odd constant (so they are distinct and identify the element). The digest folds, per scenario, the sortedness of the
input, the sorted keys, the permuted values and the sortedness of the result. The script prints the table pasted into
tests/sortu/sortu_tests.cpp.
Usage: python3 tools/sortu_ref.py
"""
M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
VMUL = 2654435761  # odd, so index -> value is injective modulo 2^32


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


def gen_keys(n, seed, mode):
    r = Lcg(seed)
    pool = [r.next() for _ in range(5)]
    out = []
    for i in range(n):
        if mode == 0: k = r.next()
        elif mode == 1: k = pool[r.rng(0, 4)]
        elif mode == 2: k = (i * 3 + r.rng(0, 2)) & M32
        elif mode == 3: k = ((n - 1 - i) * 3 + r.rng(0, 2)) & M32
        elif mode == 4: k = pool[0]
        elif mode == 5: k = 0xABCDEF00 | r.rng(0, 255)
        else: k = (r.rng(0, 255) << 24) | 0x00123456
        out.append(k)
    return out


def lsd_sort(pairs):
    for shift in (0, 8, 16, 24):
        buckets = [[] for _ in range(256)]
        for p in pairs: buckets[(p[0] >> shift) & 255].append(p)
        pairs = [p for b in buckets for p in b]
    return pairs


def scenario(n, seed, mode):
    keys = gen_keys(n, seed, mode)
    pairs = [(k, (i * VMUL) & M32) for i, k in enumerate(keys)]
    res = lsd_sort(pairs)
    assert [p[0] for p in res] == sorted(keys)
    assert res == sorted(pairs, key=lambda p: p[0])  # sorted() is stable: equal keys keep their order
    ws = [int(all(keys[i] <= keys[i + 1] for i in range(n - 1)))]
    ws += [p[0] for p in res]
    ws += [p[1] for p in res]
    ws += [1]
    return fnv_words(ws)


def main():
    sizes = [0, 1, 2, 3, 4, 5, 16, 255, 256, 257, 1000, 4096, 5000]
    rows = []
    seed = 500
    for n in sizes:
        for mode in range(7):
            seed += 1
            rows.append('{%d, %d, %d, %dull}' % (n, seed, mode, scenario(n, seed, mode)))
    print('const ScCase kScCases[] = {%s};' % ', '.join(rows))


if __name__ == '__main__':
    main()
