#!/usr/bin/env python3
"""Reference for core/twosat. Prints data.inc (digests and tables) for tests/twosat/twosat_tests.cpp.

Small and mid instances: brute force over ALL assignments. For each literal the set of assignments that make it true is one Python integer with
2^n bits; a clause allows the union of its two literals' sets and the instance allows the intersection over its clauses. The instance is satisfiable
exactly when that set is not empty. The conflict variable (the smallest v with v and not v in one strongly connected component) comes from a
transitive closure of the implication graph by Floyd-Warshall on bit rows: v and not v are in one component exactly when each reaches the other.
Large instances: iterative Kosaraju (a different method from the Tarjan search of the library). `--selfcheck` compares the methods on 30000 random
instances (0 mismatches, and satisfiable exactly when no variable has a conflict).
The digest of a result folds one byte for the verdict and four bytes for the conflict variable (0xFFFFFFFF when satisfiable). Which assignment is
returned is not specified, so it is not part of any digest; the tests validate every returned assignment clause by clause.
"""
import itertools
import os
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
NONE = 0xFFFFFFFF
NN = 65536
MM = 262144


def fold(h, v, nbytes):
    for _ in range(nbytes):
        h = ((h ^ (v & 0xFF)) * FNV_PRIME) & M64
        v >>= 8
    return h


def fold_result(h, sat, conflict):
    h = fold(h, sat, 1)
    return fold(h, conflict, 4)


class Lcg:
    def __init__(self, s):
        self.s = s

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def brute(n, clauses):
    """Returns (sat, conflict)."""
    full = (1 << (1 << n)) - 1
    lit = []
    for v in range(n):
        t = 0
        for a in range(1 << n):
            if (a >> v) & 1:
                t |= 1 << a
        lit.append(t)
        lit.append(full ^ t)
    allowed = full
    for a, b in clauses:
        allowed &= lit[a] | lit[b]
        if allowed == 0:
            break
    sat = 1 if allowed else 0
    if sat:
        return 1, NONE
    N = 2 * n
    row = [1 << v for v in range(N)]
    for a, b in clauses:
        row[a ^ 1] |= 1 << b
        row[b ^ 1] |= 1 << a
    for k in range(N):
        for i in range(N):
            if (row[i] >> k) & 1:
                row[i] |= row[k]
    for v in range(n):
        if (row[2 * v] >> (2 * v + 1)) & 1 and (row[2 * v + 1] >> (2 * v)) & 1:
            return 0, v
    raise AssertionError("unsat without a conflict variable")


def kosaraju(n, clauses):
    N = 2 * n
    g = [[] for _ in range(N)]
    rg = [[] for _ in range(N)]
    for a, b in clauses:
        g[a ^ 1].append(b)
        rg[b].append(a ^ 1)
        g[b ^ 1].append(a)
        rg[a].append(b ^ 1)
    seen = [False] * N
    order = []
    for r in range(N):
        if seen[r]:
            continue
        seen[r] = True
        st = [(r, 0)]
        while st:
            u, i = st.pop()
            if i < len(g[u]):
                st.append((u, i + 1))
                w = g[u][i]
                if not seen[w]:
                    seen[w] = True
                    st.append((w, 0))
            else:
                order.append(u)
    comp = [-1] * N
    c = 0
    for r in reversed(order):
        if comp[r] != -1:
            continue
        comp[r] = c
        st = [r]
        while st:
            u = st.pop()
            for w in rg[u]:
                if comp[w] == -1:
                    comp[w] = c
                    st.append(w)
        c += 1
    for v in range(n):
        if comp[2 * v] == comp[2 * v + 1]:
            return 0, v
    return 1, NONE


def selfcheck():
    r = Lcg(31337)
    bad = 0
    for k in range(30000):
        n = 1 + r.rnd(8)
        m = r.rnd(14)
        cl = [(r.rnd(2 * n), r.rnd(2 * n)) for _ in range(m)]
        a = brute(n, cl)
        b = kosaraju(n, cl)
        if a != b:
            bad += 1
            if bad < 4:
                print("MISMATCH", n, cl, a, b)
    print("selfcheck mismatches:", bad)


def group_multiset(name, n, mmax, cache, unordered):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    L = 2 * n
    K = [(a, b) for a in range(L) for b in range(L) if (not unordered or a <= b)]
    h = FNV_OFF
    cnt = 0
    for m in range(mmax + 1):
        for cl in itertools.combinations_with_replacement(K, m):
            cnt += 1
            s, c = brute(n, cl)
            h = fold_result(h, s, c)
    res = [cnt, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def group_ordered(name, n, mmax, cache):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    L = 2 * n
    K = [(a, b) for a in range(L) for b in range(L)]
    h = FNV_OFF
    cnt = 0
    for m in range(mmax + 1):
        for cl in itertools.product(K, repeat=m):
            cnt += 1
            s, c = brute(n, cl)
            h = fold_result(h, s, c)
    res = [cnt, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def group_subsets3(name, cache):
    """Every set of clauses on 3 variables: the 21 unordered literal pairs a <= b (tautologies and unit clauses included), all 2^21 subsets."""
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    n = 3
    K = [(a, b) for a in range(6) for b in range(a, 6)]
    # per assignment, the set of clause kinds it satisfies (bit mask over the 21 kinds)
    sat_masks = []
    for a in range(8):
        mk = 0
        for i, (x, y) in enumerate(K):
            tx = ((a >> (x >> 1)) & 1) ^ (x & 1) ^ 1
            ty = ((a >> (y >> 1)) & 1) ^ (y & 1) ^ 1
            if tx or ty:
                mk |= 1 << i
        sat_masks.append(mk)
    h = FNV_OFF
    for sub in range(1 << 21):
        ok = False
        for mk in sat_masks:
            if sub & ~mk == 0:
                ok = True
                break
        if ok:
            h = fold_result(h, 1, NONE)
        else:
            cl = [K[i] for i in range(21) if (sub >> i) & 1]
            s, c = brute(n, cl)
            h = fold_result(h, s, c)
    res = [1 << 21, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def planted(r, n, m):
    hid = [r.rnd(2) for _ in range(n)]
    cl = []
    while len(cl) < m:
        a = r.rnd(2 * n)
        b = r.rnd(2 * n)
        ta = hid[a >> 1] ^ (a & 1) ^ 1
        tb = hid[b >> 1] ^ (b & 1) ^ 1
        if ta or tb:
            cl.append((a, b))
    return cl


def big(pid):
    r = Lcg(7000 + pid)
    n = NN
    cl = []
    if pid == 0:
        pass
    elif pid == 1:  # implication chain x_i -> x_{i+1}, units x_0 and not x_{n-1}: unsat, one component, conflict 0
        cl = [(2 * i + 1, 2 * (i + 1)) for i in range(n - 1)] + [(0, 0), (2 * (n - 1) + 1, 2 * (n - 1) + 1)]
    elif pid == 2:  # the same chain, only x_0 forced: satisfiable, everything true
        cl = [(2 * i + 1, 2 * (i + 1)) for i in range(n - 1)] + [(0, 0)]
    elif pid == 3:  # chain on shuffled names, satisfiable
        p = list(range(n))
        for i in range(n, 1, -1):
            j = r.rnd(i)
            p[i - 1], p[j] = p[j], p[i - 1]
        cl = [(2 * p[i] + 1, 2 * p[i + 1]) for i in range(n - 1)]
    elif pid == 4:  # satisfiable random plus the unsat gadget on the last variable: conflict 65535
        cl = planted(r, n, 100000) + [(2 * (n - 1), 2 * (n - 1)), (2 * (n - 1) + 1, 2 * (n - 1) + 1)]
    elif pid == 5:  # uniform random, 262144 clauses: unsat
        cl = [(r.rnd(2 * n), r.rnd(2 * n)) for _ in range(MM)]
    elif pid == 6:  # uniform random, 50000 clauses
        cl = [(r.rnd(2 * n), r.rnd(2 * n)) for _ in range(50000)]
    elif pid == 7:  # all tautologies
        cl = [(2 * i, 2 * i + 1) if i % 2 == 0 else (2 * i + 1, 2 * i) for i in range(n)]
    elif pid == 8:  # unit clauses: x_i for even i, not x_i for odd i
        cl = [(2 * i + (i & 1), 2 * i + (i & 1)) for i in range(n)]
    elif pid == 9:  # one clause repeated 262144 times
        cl = [(0, 2)] * MM
    elif pid == 10:  # equivalence ring x_i <-> x_{i+1} (both implications), satisfiable
        cl = []
        for i in range(n - 1):
            cl.append((2 * i + 1, 2 * (i + 1)))
            cl.append((2 * i, 2 * (i + 1) + 1))
    elif pid == 11:  # the same chain of equivalences closed with an odd twist: unsat, conflict 0
        for i in range(n - 1):
            cl.append((2 * i + 1, 2 * (i + 1)))
            cl.append((2 * i, 2 * (i + 1) + 1))
        cl.append((2 * (n - 1), 0))
        cl.append((2 * (n - 1) + 1, 1))
    elif pid == 12:  # satisfiable planted clauses elsewhere, an unsat equivalence ring on variables 40000 .. 40010: conflict 40000
        cl = planted(r, n, 60000)
        base = 40000
        for i in range(10):
            cl.append((2 * (base + i) + 1, 2 * (base + i + 1)))
            cl.append((2 * (base + i), 2 * (base + i + 1) + 1))
        cl.append((2 * (base + 10), 2 * base))
        cl.append((2 * (base + 10) + 1, 2 * base + 1))
    elif pid == 13:  # dense planted on 512 variables, 262144 clauses
        n = 512
        cl = planted(r, n, MM)
    elif pid == 14:  # planted, 65536 variables, 262144 clauses
        cl = planted(r, n, MM)
    return n, cl


def main():
    cache = None
    for a in sys.argv[1:]:
        if a.startswith("--cache="):
            cache = a[8:]
            if not os.path.isdir(cache):
                os.makedirs(cache)
    if "--selfcheck" in sys.argv:
        selfcheck()
        return
    out = []
    P = out.append
    gs = [
        ("n1", group_multiset, (1, 6, True)),
        ("n2", group_multiset, (2, 6, False)),
        ("n3", group_multiset, (3, 4, False)),
        ("n4", group_multiset, (4, 3, True)),
        ("o2", group_ordered, (2, 4)),
        ("o3", group_ordered, (3, 3)),
    ]
    P("// groups: instances, digest lo, digest hi")
    rows = []
    for name, f, args in gs:
        g = f(name, *args[:2], cache, *args[2:]) if f is group_multiset else f(name, args[0], args[1], cache)
        rows.append(g)
    g = group_subsets3("s3", cache)
    rows.append(g)
    P("const u32 kGroups[%d] = {" % (3 * len(rows)))
    for g in rows:
        P("    %du, %du, %du," % (g[0], g[1], g[2]))
    P("};")
    # battery and mid
    r = Lcg(6161)
    h = FNV_OFF
    for k in range(20000):
        n = 1 + r.rnd(9)
        m = r.rnd(25)
        cl = [(r.rnd(2 * n), r.rnd(2 * n)) for _ in range(m)]
        s, c = brute(n, cl)
        h = fold_result(h, s, c)
    P("const u32 kBattery[2] = {%du, %du};" % (h & 0xFFFFFFFF, h >> 32))
    r = Lcg(6262)
    h = FNV_OFF
    nsat = 0
    for k in range(200):
        n = 1 + r.rnd(16)
        m = r.rnd(2 * n + 6)
        cl = [(r.rnd(2 * n), r.rnd(2 * n)) for _ in range(m)]
        s, c = brute(n, cl)
        assert (s, c) == kosaraju(n, cl)
        nsat += s
        h = fold_result(h, s, c)
    P("const u32 kMid[3] = {%du, %du, %du};" % (h & 0xFFFFFFFF, h >> 32, nsat))
    P("// max-size patterns: pid, n, m, sat, conflict")
    P("const u32 kBig[%d] = {" % (5 * 15))
    for pid in range(15):
        n, cl = big(pid)
        s, c = kosaraju(n, cl)
        P("    %du, %du, %du, %du, %du," % (pid, n, len(cl), s, c))
        sys.stderr.write("pid %d n %d m %d sat %d conflict %d\n" % (pid, n, len(cl), s, c if c != NONE else -1))
    P("};")
    print("\n".join(out))


if __name__ == "__main__":
    main()
