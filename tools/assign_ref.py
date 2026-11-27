#!/usr/bin/env python3
"""Reference for core/assign. Prints data.inc (digests and tables) for tests/assign/assign_tests.cpp.

Small matrices (side up to 6): brute force over ALL permutations of the columns (itertools.permutations), the least sum is the answer.
Mid matrices (side 7 to 12): exact dynamic programming over subsets of used columns (a different method from the library's Hungarian method).
Max-size patterns: matrices built so that the optimum is known by construction. Write cost[i][j] = r[i] + c[j] + d[i][j] with d >= 0. Any assignment
costs at least sum(r) + sum(c), and a permutation along which d is 0 reaches it, so that sum is the minimum. Patterns with a constant matrix or a
zero diagonal below a triangle of zeros are the same argument with r, c or d chosen directly.
The digest of a result folds the total as 8 bytes (little endian bytes of the u64) into FNV-1a. Which assignment is returned is not specified, so it
is not part of any digest; the tests validate every returned assignment (a permutation whose cost equals the total).
"""
import itertools
import os
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
MAXV = 0xFFFFFFFF


def fold(h, v, nbytes):
    for _ in range(nbytes):
        h = ((h ^ (v & 0xFF)) * FNV_PRIME) & M64
        v >>= 8
    return h


class Lcg:
    def __init__(self, s):
        self.s = s

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def brute(n, a):
    if n == 0:
        return 0
    best = None
    for perm in itertools.permutations(range(n)):
        s = 0
        for i in range(n):
            s += a[i * n + perm[i]]
        if best is None or s < best:
            best = s
    return best


def dp(n, a):
    INF = 1 << 80
    f = [INF] * (1 << n)
    f[0] = 0
    for mask in range(1 << n):
        if f[mask] == INF:
            continue
        i = bin(mask).count("1")
        if i == n:
            continue
        base = i * n
        for j in range(n):
            if not (mask >> j) & 1:
                nm = mask | (1 << j)
                v = f[mask] + a[base + j]
                if v < f[nm]:
                    f[nm] = v
    return f[(1 << n) - 1]


def rnd32(r):
    hi = r.rnd(65536)
    lo = r.rnd(65536)
    return (hi << 16) | lo


def entry(r, mode):
    if mode == 0:
        return r.rnd(4)
    if mode == 1:
        return r.rnd(100)
    if mode == 2:
        return rnd32(r)
    return MAXV - r.rnd(8)


def group(name, n, vals, cache, use_brute=True):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    h = FNV_OFF
    cnt = 0
    for a in itertools.product(vals, repeat=n * n):
        cnt += 1
        h = fold(h, brute(n, a), 8)
    res = [cnt, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def shuffle(r, e):
    for i in range(len(e) - 1, 0, -1):
        j = r.rnd(i + 1)
        e[i], e[j] = e[j], e[i]


def planted(r, n, rmax, cmax, dmax):
    """cost = r[i] + c[j] + d[i][j], d zero along a random permutation. Returns (matrix, optimum)."""
    perm = list(range(n))
    shuffle(r, perm)
    rr = [r.rnd(rmax) for _ in range(n)]
    cc = [r.rnd(cmax) for _ in range(n)]
    a = []
    for i in range(n):
        for j in range(n):
            d = 0 if perm[i] == j else r.rnd(dmax + 1)
            a.append(rr[i] + cc[j] + d)
    assert max(a) <= MAXV
    return a, sum(rr) + sum(cc)


def big(pid):
    r = Lcg(9100 + pid)
    N = 512
    if pid == 0:
        return N, [0] * (N * N), 0
    if pid == 1:
        return N, [MAXV] * (N * N), N * MAXV
    if pid == 2:
        return N, [1234567] * (N * N), N * 1234567
    if pid == 3:
        return N, [0 if i == j else MAXV for i in range(N) for j in range(N)], 0
    if pid == 4:
        perm = list(range(N))
        shuffle(r, perm)
        a = [0 if perm[i] == j else rnd32(r) for i in range(N) for j in range(N)]
        return N, a, 0
    if pid == 5:
        a, t = planted(r, N, 1 << 30, 1 << 30, (1 << 31) - 1)
        return N, a, t
    if pid == 6:
        perm = list(range(N))
        shuffle(r, perm)
        rr = [(1 << 31) - 2000 + r.rnd(1000) for _ in range(N)]
        cc = [(1 << 31) - 2000 + r.rnd(1000) for _ in range(N)]
        a = []
        for i in range(N):
            for j in range(N):
                d = 0 if perm[i] == j else r.rnd(1000)
                a.append(rr[i] + cc[j] + d)
        assert max(a) <= MAXV
        return N, a, sum(rr) + sum(cc)
    if pid == 7:
        a, t = planted(r, N, 4, 4, 1)
        return N, a, t
    if pid == 8:
        rr = [rnd32(r) for _ in range(N)]
        return N, [rr[i] for i in range(N) for j in range(N)], sum(rr)
    if pid == 9:
        cc = [rnd32(r) for _ in range(N)]
        return N, [cc[j] for i in range(N) for j in range(N)], sum(cc)
    if pid == 10:
        return N, [MAXV if j > i else 0 for i in range(N) for j in range(N)], 0
    n = 300
    a, t = planted(r, n, 1 << 30, 1 << 30, (1 << 31) - 1)
    return n, a, t


def main():
    cache = None
    for x in sys.argv[1:]:
        if x.startswith("--cache="):
            cache = x[8:]
            if not os.path.isdir(cache):
                os.makedirs(cache)
    big_vals = [0, 1, MAXV - 1, MAXV]
    out = []
    P = out.append
    rows = [
        group("a1", 1, [0, 1, 2, MAXV - 1, MAXV], cache),
        group("a2", 2, list(range(6)), cache),
        group("a3", 3, [0, 1, 2, 3], cache),
        group("a4", 4, [0, 1], cache),
        group("b2", 2, big_vals, cache),
        group("b3", 3, big_vals, cache),
    ]
    P("// groups: instances, digest lo, digest hi")
    P("const u32 kGroups[%d] = {" % (3 * len(rows)))
    for g in rows:
        P("    %du, %du, %du," % (g[0], g[1], g[2]))
    P("};")
    r = Lcg(8181)
    h = FNV_OFF
    for _ in range(8000):
        n = 1 + r.rnd(6)
        mode = r.rnd(4)
        a = [entry(r, mode) for _ in range(n * n)]
        h = fold(h, brute(n, a), 8)
    P("const u32 kBattery[2] = {%du, %du};" % (h & 0xFFFFFFFF, h >> 32))
    r = Lcg(8282)
    h = FNV_OFF
    for _ in range(300):
        n = 7 + r.rnd(6)
        mode = r.rnd(4)
        a = [entry(r, mode) for _ in range(n * n)]
        h = fold(h, dp(n, a), 8)
    P("const u32 kMid[2] = {%du, %du};" % (h & 0xFFFFFFFF, h >> 32))
    P("// max-size patterns: pid, n, total lo, total hi")
    P("const u32 kBig[%d] = {" % (4 * 12))
    for pid in range(12):
        n, a, t = big(pid)
        P("    %du, %du, %du, %du," % (pid, n, t & 0xFFFFFFFF, t >> 32))
        sys.stderr.write("pid %d n %d total %d\n" % (pid, n, t))
    P("};")
    print("\n".join(out))


if __name__ == "__main__":
    main()
