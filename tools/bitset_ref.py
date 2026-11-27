#!/usr/bin/env python3
"""Independent reference for core/bitset (scope AM). Exact Python integers.

Written from the rules in bitset.h; it shares no code with the implementation. The model is a plain list of bits (one entry per bit,
padding bits included up to 64 * words_for(nbits)), never words and masks: set and fill touch single list entries below nbits, popcount
counts entries, find_next scans entry by entry. A scenario is (nbits, seed, mode, nops): the initial words come from the seeded generator
below (the C++ test mirrors it), then nops scripted operations (valid and invalid indices) run against the model, and the digest folds
every result and the final words, padding included. The script prints the table pasted into tests/bitset/bitset_tests.cpp.
Usage: python3 tools/bitset_ref.py
"""
M64 = (1 << 64) - 1
KMAX = 1 << 30
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
    def next64(s): return (s.next() << 32) | s.next()
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


def words_for(n):
    return 0 if n > KMAX else (n + 63) // 64


def init_words(nbits, seed, mode):
    r = Lcg(seed)
    out = []
    for _ in range(words_for(nbits)):
        if mode == 0: out.append(r.next64())
        elif mode == 1: out.append(0)
        elif mode == 2: out.append(M64)
        else: out.append(r.next64() & r.next64() & r.next64())
    return out


def scenario(nbits, seed, mode, nops):
    words = init_words(nbits, seed, mode)
    bits = [(w >> k) & 1 for w in words for k in range(64)]
    r = Lcg(seed + 1)
    ws = []
    for idx in range(nops):
        kind = r.rng(0, 19)
        if kind <= 6:
            i = r.rng(0, nbits + 2); v = r.rng(0, 1)
            ok = i < nbits
            if ok: bits[i] = v
            ws += [0, int(ok), SENT]
        elif kind <= 9:
            i = r.rng(0, nbits + 2)
            ok = i < nbits
            pre = idx & 1
            ws += [1, int(ok), bits[i] if ok else pre]
        elif kind == 10:
            v = r.rng(0, 1)
            for j in range(nbits): bits[j] = v
            ws += [2, 1, SENT]
        elif kind <= 12:
            ws += [3, 1, sum(bits[:nbits])]
        else:
            f = r.rng(0, nbits + 2); v = r.rng(0, 1)
            if f > nbits:
                ws += [4, 0, SENT]
            else:
                res = nbits
                for j in range(f, nbits):
                    if bits[j] == v:
                        res = j
                        break
                ws += [4, 1, res]
    final = [sum(bits[64 * w + k] << k for k in range(64)) for w in range(words_for(nbits))]
    for w in final: ws += [w & 0xFFFFFFFF, w >> 32]
    return fnv_words(ws)


def main():
    sizes = [0, 1, 2, 7, 31, 32, 33, 63, 64, 65, 100, 127, 128, 129, 191, 192, 193, 255, 256, 257, 1000, 4095, 4096, 4097, 20000]
    rows = []
    seed = 1000
    for n in sizes:
        for mode in range(4):
            seed += 1
            rows.append('{%d, %d, %d, 300, %dull}' % (n, seed, mode, scenario(n, seed, mode, 300)))
    print('const ScCase kScCases[] = {%s};' % ', '.join(rows))


if __name__ == '__main__':
    main()
