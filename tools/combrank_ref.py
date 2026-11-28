#!/usr/bin/env python3
"""Independent reference for core/combrank (Python standard library only; bigint math.comb, no code shared with the library).

The rank of a k-subset with elements c_1 < ... < c_k is sum C(c_j, j) (colex); unrank searches the largest c with C(c, j) <= rest, digit by digit;
next is unrank(rank + 1). Digests are FNV-1a over 64-bit words (fold(h, w) = (h ^ w) * 0x100000001b3 mod 2^64), the same fold as the test.

Commands: check (identities and the header examples), count, exh LO HI, edge, samp K (K = 0..3), masks K (K = 0..3), wrap, emit (all constants as C++).
"""
import sys
from itertools import combinations
from math import comb

M64 = (1 << 64) - 1
FNV0 = 0xCBF29CE484222325
OK, NULLARG, BADARG, BADMASK, TOOBIG, NONEXT = 0, 1, 2, 3, 4, 5


def fold(h, w):
    return ((h ^ (w & M64)) * 0x100000001B3) & M64


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def pop(m):
    return bin(m).count("1")


def low(n):
    return (1 << n) - 1


def r_count(n, k):
    if n > 64 or k > n:
        return BADARG, 0
    return OK, comb(n, k)


def r_rank(mask, n, k):
    if n > 64 or k > n:
        return BADARG, 0
    if (mask >> n) != 0 or pop(mask) != k:
        return BADMASK, 0
    s = 0
    j = 0
    for c in range(n):
        if (mask >> c) & 1:
            j += 1
            s += comb(c, j)
    return OK, s


def r_unrank(n, k, rank):
    if n > 64 or k > n:
        return BADARG, 0
    if rank >= comb(n, k):
        return TOOBIG, 0
    m = 0
    for j in range(k, 0, -1):
        c = j - 1
        while comb(c + 1, j) <= rank:
            c += 1
        rank -= comb(c, j)
        m |= 1 << c
    return OK, m


def r_next(mask, n, k):
    if n > 64 or k > n:
        return BADARG, 0
    if (mask >> n) != 0 or pop(mask) != k:
        return BADMASK, 0
    r = r_rank(mask, n, k)[1]
    if k == 0 or r + 1 == comb(n, k):
        return NONEXT, 0
    return OK, r_unrank(n, k, r + 1)[1]


def d_count():
    h = FNV0
    for n in range(65):
        for k in range(65):
            c, v = r_count(n, k)
            h = fold(fold(h, c), v)
    for n, k in EXTRA_COUNT:
        c, v = r_count(n, k)
        h = fold(fold(h, c), v)
    return h, 65 * 65 + len(EXTRA_COUNT)


EXTRA_COUNT = [(65, 0), (65, 65), (70, 3), (255, 1), (4294967295, 0), (64, 65), (0, 1), (4294967295, 4294967295)]


def d_exh(lo, hi):
    h = FNV0
    cnt = 0
    for n in range(lo, hi + 1):
        for mask in range(1 << n):
            k = pop(mask)
            s = 0
            j = 0
            mm = mask
            c = 0
            while mm:
                if mm & 1:
                    j += 1
                    s += comb(c, j)
                mm >>= 1
                c += 1
            h = fold(fold(fold(h, n), mask), s)
            cnt += 1
    return h, cnt


RANKS = lambda c: [0, 1, 2, c - 2, c - 1, c, c + 1, M64]


def d_edge():
    h = FNV0
    cnt = 0
    for n in range(65):
        for k in range(n + 1):
            c = comb(n, k)
            first = low(k)
            last = low(k) << (n - k)
            for m in (first, last):
                code, r = r_rank(m, n, k)
                h = fold(fold(h, code), r)
                code, nx = r_next(m, n, k)
                h = fold(fold(h, code), nx)
            for r in RANKS(c):
                if r < 0 or r > M64:
                    continue
                code, m = r_unrank(n, k, r)
                h = fold(fold(h, code), m)
                if code == OK:
                    h = fold(h, r_rank(m, n, k)[1])
                cnt += 1
            for start in (first, r_unrank(n, k, max(0, c - 65))[1]):
                m = start
                for _ in range(130):
                    code, nx = r_next(m, n, k)
                    h = fold(fold(h, code), nx)
                    if code != OK:
                        break
                    m = nx
    return h, cnt


def samp_case(p):
    w0 = splitmix_at(p)
    w1 = splitmix_at(p + 1)
    n = w0 % 65
    k = (w0 >> 8) % (n + 1)
    t = (w0 >> 16) & 3
    c = comb(n, k)
    rank = w1 if t == 0 else w1 % c
    return n, k, rank


def d_samp(s):
    h = FNV0
    for i in range(50000):
        n, k, rank = samp_case((1 << 40) + s * 100000 + 2 * i)
        code, m = r_unrank(n, k, rank)
        h = fold(fold(fold(fold(fold(h, n), k), rank), code), m)
        if code == OK:
            h = fold(h, r_rank(m, n, k)[1])
            code2, nx = r_next(m, n, k)
            h = fold(fold(h, code2), nx)
    return h, 50000


def mask_case(p):
    w0 = splitmix_at(p)
    w1 = splitmix_at(p + 1)
    n = 1 + w0 % 64
    k = (w0 >> 8) % (n + 1)
    m = w1 & low(n)
    while pop(m) > k:
        m &= m - 1
    while pop(m) < k:
        z = ~m & low(n)
        m |= z & -z
    return n, k, m


def d_masks(s):
    h = FNV0
    for i in range(50000):
        n, k, m = mask_case((1 << 41) + s * 100000 + 2 * i)
        code, r = r_rank(m, n, k)
        h = fold(fold(fold(fold(h, n), m), code), r)
        code, nx = r_next(m, n, k)
        h = fold(fold(h, code), nx)
    return h, 50000


WRAP_KS = (1, 2, 3, 61, 62, 63, 64)


def wrap_masks(k):
    if k <= 3:
        ms = [sum(1 << c for c in cs) for cs in combinations(range(64), k)]
    else:
        ms = [M64 ^ sum(1 << c for c in cs) for cs in combinations(range(64), 64 - k)]
    ms.sort()
    return ms


def d_wrap():
    h = FNV0
    cnt = 0
    for k in WRAP_KS:
        for m in wrap_masks(k):
            h = fold(h, m)
            cnt += 1
    return h, cnt


def c_check():
    for n in range(0, 65):
        for k in range(n + 1):
            assert comb(n, k) == (comb(n - 1, k - 1) if n and k else 0) + (comb(n - 1, k) if n and k <= n - 1 else 0) or (n == 0 and k == 0)
            assert comb(n, k) <= comb(64, 32) < 1 << 61
    for n in range(0, 15):
        for k in range(n + 1):
            seen = []
            for cs in combinations(range(n), k):
                seen.append(sum(1 << c for c in cs))
            seen.sort()
            assert len(seen) == comb(n, k)
            for i, m in enumerate(seen):
                assert r_rank(m, n, k) == (OK, i)
                assert r_unrank(n, k, i) == (OK, m)
                nx = r_next(m, n, k)
                assert nx == ((OK, seen[i + 1]) if i + 1 < len(seen) else (NONEXT, 0))
    assert r_count(64, 32) == (OK, 1832624140942590534)
    assert r_rank(0xFFFFFFFF00000000, 64, 32) == (OK, comb(64, 32) - 1)
    assert r_unrank(10, 4, 100) == (OK, 353)
    assert r_unrank(64, 10, 100000000000) == (OK, 0x2211000002221101)
    assert r_unrank(64, 2, 2015) == (OK, 0xC000000000000000)
    assert r_next(0xFFFFFFFF, 64, 32) == (OK, 0x17FFFFFFF)
    assert r_next(0x7FFFFFFFFFFFFFFF, 64, 63) == (OK, 0xBFFFFFFFFFFFFFFF)
    assert r_next(1 << 63, 64, 1) == (NONEXT, 0)
    assert r_rank(0b101101, 6, 4) == (OK, 7)
    two = 0x2211000002221101
    assert r_rank(two, 64, 10) == (OK, 100000000000)
    print("check ok")


def emit():
    out = []
    def put(name, v, cnt=None):
        out.append("constexpr u64 k%sDigest = 0x%016xull;" % (name, v))
        if cnt is not None:
            out.append("constexpr u64 k%sCount = %du;" % (name, cnt))
    h, c = d_count(); put("Count", h, c)
    for nm, lo, hi in (("ExhA", 0, 17), ("ExhB", 18, 18), ("ExhC", 19, 19), ("ExhD", 20, 20)):
        h, c = d_exh(lo, hi); put(nm, h, c)
    h, c = d_edge(); put("Edge", h, c)
    for s in range(4):
        h, c = d_samp(s); put("Samp%d" % s, h, c)
    for s in range(4):
        h, c = d_masks(s); put("Masks%d" % s, h, c)
    h, c = d_wrap(); put("Wrap", h, c)
    print("\n".join(out))


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == "check":
        c_check()
    elif a[0] == "count":
        print(d_count())
    elif a[0] == "exh":
        print(d_exh(int(a[1]), int(a[2])))
    elif a[0] == "edge":
        print(d_edge())
    elif a[0] == "samp":
        print(d_samp(int(a[1])))
    elif a[0] == "masks":
        print(d_masks(int(a[1])))
    elif a[0] == "wrap":
        print(d_wrap())
    elif a[0] == "emit":
        emit()


main()
