#!/usr/bin/env python3
"""Reference for core/euler. Prints data.inc (digests and tables) for tests/euler/euler_tests.cpp.

Small graphs: brute force over ALL orders of the edges. A trail is an order of the edge numbers in which the head of each edge is the tail of the next.
Orders are searched depth-first over edge numbers (parallel edges are different edges), once per choice of first edge, stopping at the first complete
trail for that first edge. The graph has a Circuit when some trail ends where it starts (the empty graph counts), a Path when some trail ends elsewhere,
None when no first edge gives a complete trail. For a Path the set of first tails over the trails found must hold exactly one node, and that node is the
required start; the search asserts it, and asserts that a graph never has both kinds. No degree rule and no Hierholzer step is used for these.
Max-size patterns: kinds known by construction and confirmed by degree counts and union-find (a rule reference, no trail search, so independent of the library's
Hierholzer method). `--selfcheck` compares the rule reference with the brute force on random graphs with up to 7 edges (0 mismatches expected).
The digest of a result folds one byte for the kind (0 None, 1 Path, 2 Circuit) and four bytes for the required start (0xFFFFFFFF unless Path).
Which trail is returned is not specified, so it is not part of any digest; the tests validate every returned trail edge by edge.
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


def fold_result(h, kind, start):
    h = fold(h, kind, 1)
    return fold(h, start, 4)


class Lcg:
    def __init__(self, s):
        self.s = s

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def brute(n, edges):
    m = len(edges)
    if m == 0:
        return (2, NONE)
    closed = False
    starts = set()
    used = [False] * m

    def dfs(node, cnt, first):
        # True at the first complete trail found; sets `last` to its final head
        if cnt == m:
            return node
        for i in range(m):
            if not used[i] and edges[i][0] == node:
                used[i] = True
                res = dfs(edges[i][1], cnt + 1, first)
                used[i] = False
                if res is not None:
                    return res
        return None

    seen_open = set()
    for i in range(m):
        used[i] = True
        res = dfs(edges[i][1], 1, edges[i][0])
        used[i] = False
        if res is not None:
            if res == edges[i][0]:
                closed = True
            else:
                seen_open.add(edges[i][0])
    assert not (closed and seen_open)
    if closed:
        return (2, NONE)
    if seen_open:
        assert len(seen_open) == 1
        return (1, next(iter(seen_open)))
    return (0, NONE)


def fast(n, edges):
    m = len(edges)
    if m == 0:
        return (2, NONE)
    out = [0] * n
    inn = [0] * n
    par = list(range(n))

    def find(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x

    for u, v in edges:
        out[u] += 1
        inn[v] += 1
        a, b = find(u), find(v)
        if a != b:
            par[a] = b
    roots = set(find(u) for u, v in edges)
    if len(roots) != 1:
        return (0, NONE)
    src = [v for v in range(n) if out[v] - inn[v] == 1]
    snk = [v for v in range(n) if inn[v] - out[v] == 1]
    other = [v for v in range(n) if abs(out[v] - inn[v]) > 1]
    if other:
        return (0, NONE)
    if not src and not snk:
        return (2, NONE)
    if len(src) == 1 and len(snk) == 1:
        return (1, src[0])
    return (0, NONE)


def walk(r, lo, cnt, m, closed):
    edges = []
    v0 = lo + r.rnd(cnt)
    cur = v0
    for i in range(m):
        nxt = lo + r.rnd(cnt)
        if closed and i == m - 1:
            nxt = v0
        edges.append((cur, nxt))
        cur = nxt
    return edges


def shuffle(r, e):
    for i in range(len(e) - 1, 0, -1):
        j = r.rnd(i + 1)
        e[i], e[j] = e[j], e[i]


def gen(r, n, m, mode):
    if mode == 0:
        return [(r.rnd(n), r.rnd(n)) for _ in range(m)]
    if mode == 1:
        e = walk(r, 0, n, m, False)
        shuffle(r, e)
        return e
    if mode == 2:
        e = walk(r, 0, n, m, True)
        shuffle(r, e)
        return e
    if mode == 3:
        e = walk(r, 0, n, m, r.rnd(2) == 1)
        shuffle(r, e)
        if m > 0:
            i = r.rnd(m)
            e[i] = (e[i][0], r.rnd(n))
        return e
    a = r.rnd(m + 1)
    h = n // 2
    e = walk(r, 0, h, a, True) + walk(r, h, n - h, m - a, True)
    shuffle(r, e)
    return e


def selfcheck():
    r = Lcg(1)
    bad = 0
    for k in range(30000):
        n = 1 + r.rnd(5)
        m = r.rnd(8)
        e = gen(r, n, m, r.rnd(4))
        if brute(n, e) != fast(n, e):
            bad += 1
    print("selfcheck mismatches:", bad, "of 30000")


def group_multiset(name, n, mmax, cache):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    K = [(a, b) for a in range(n) for b in range(n)]
    h = FNV_OFF
    cnt = 0
    for m in range(mmax + 1):
        for es in itertools.combinations_with_replacement(K, m):
            cnt += 1
            k, s = brute(n, es)
            h = fold_result(h, k, s)
    res = [cnt, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def group_ordered(name, n, mmax, cache):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    K = [(a, b) for a in range(n) for b in range(n)]
    h = FNV_OFF
    cnt = 0
    for m in range(mmax + 1):
        for es in itertools.product(K, repeat=m):
            cnt += 1
            k, s = brute(n, es)
            h = fold_result(h, k, s)
    res = [cnt, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def group_subsets4(name, cache):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    K = [(a, b) for a in range(4) for b in range(4) if a != b]
    h = FNV_OFF
    for mask in range(1 << 12):
        es = [K[i] for i in range(12) if (mask >> i) & 1]
        k, s = brute(4, es)
        h = fold_result(h, k, s)
    res = [1 << 12, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def big(pid):
    r = Lcg(9000 + pid)
    if pid == 0:
        return NN, []
    if pid == 1:
        e = [(i, i + 1) for i in range(NN - 1)]
        shuffle(r, e)
        return NN, e
    if pid == 2:
        e = [(i, (i + 1) % NN) for i in range(NN)] + [(x, x) for x in (r.rnd(NN) for _ in range(MM - NN))]
        shuffle(r, e)
        return NN, e
    if pid == 3:
        e = walk(r, 0, NN, MM, True)
        shuffle(r, e)
        return NN, e
    if pid == 4:
        return 1, [(0, 0)] * MM
    if pid == 5:
        return 2, [(0, 1)] * 131073 + [(1, 0)] * 131071
    if pid == 6:
        e = [(0, 1)] * 131072 + [(1, 0)] * 131072
        shuffle(r, e)
        return 2, e
    if pid == 7:
        e = [(0, 1)] * 131072 + [(1, 0)] * 131071 + [(0, 0)]
        shuffle(r, e)
        return 2, e
    if pid == 8:
        h = NN // 2
        e = [(i, (i + 1) % h) for i in range(h)] + [(h + i, h + (i + 1) % h) for i in range(h)]
        shuffle(r, e)
        return NN, e
    if pid == 9:
        e = [(i, i + 1) for i in range(1000)] + [(2000 + i, 2000 + (i + 1) % 50) for i in range(50)]
        shuffle(r, e)
        return NN, e
    if pid == 10:
        e = walk(r, 0, NN, MM, False)
        shuffle(r, e)
        return NN, e
    if pid == 11:
        return 512, [(a, b) for a in range(512) for b in range(512)]
    if pid == 12:
        e = [(0, i) for i in range(1, NN)] + [(i, 0) for i in range(1, NN)]
        shuffle(r, e)
        return NN, e
    if pid == 13:
        e = [(i, (i + 1) % 1000) for i in range(1000)] + [(0, 1)]
        shuffle(r, e)
        return NN, e
    e = [(r.rnd(NN), r.rnd(NN)) for _ in range(MM)]
    return NN, e


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
    gs = [("n1", group_multiset, (1, 6)), ("n2", group_multiset, (2, 6)), ("n3", group_multiset, (3, 5)), ("n4", group_multiset, (4, 4)),
          ("o2", group_ordered, (2, 6)), ("o3", group_ordered, (3, 4)), ("o4", group_ordered, (4, 3))]
    rows = [f(name, a[0], a[1], cache) for name, f, a in gs]
    rows.append(group_subsets4("s4", cache))
    P("// groups: instances, digest lo, digest hi")
    P("const u32 kGroups[%d] = {" % (3 * len(rows)))
    for g in rows:
        P("    %du, %du, %du," % (g[0], g[1], g[2]))
    P("};")
    r = Lcg(7171)
    h = FNV_OFF
    kc = [0, 0, 0]
    for _ in range(20000):
        n = 1 + r.rnd(6)
        m = r.rnd(8)
        mode = r.rnd(4)
        e = gen(r, n, m, mode)
        k, s = brute(n, e)
        kc[k] += 1
        h = fold_result(h, k, s)
    P("const u32 kBattery[5] = {%du, %du, %du, %du, %du};" % (h & 0xFFFFFFFF, h >> 32, kc[0], kc[1], kc[2]))
    r = Lcg(7272)
    h = FNV_OFF
    kc = [0, 0, 0]
    for _ in range(200):
        n = 2 + r.rnd(8)
        m = r.rnd(13)
        mode = r.rnd(5)
        e = gen(r, n, m, mode)
        k, s = brute(n, e)
        kc[k] += 1
        h = fold_result(h, k, s)
    P("const u32 kMid[5] = {%du, %du, %du, %du, %du};" % (h & 0xFFFFFFFF, h >> 32, kc[0], kc[1], kc[2]))
    P("// max-size patterns: pid, n, m, kind, start")
    P("const u32 kBig[%d] = {" % (5 * 15))
    for pid in range(15):
        n, e = big(pid)
        k, s = fast(n, e)
        P("    %du, %du, %du, %du, %du," % (pid, n, len(e), k, s))
        sys.stderr.write("pid %d n %d m %d kind %d start %d\n" % (pid, n, len(e), k, s if s != NONE else -1))
    P("};")
    print("\n".join(out))


if __name__ == "__main__":
    main()
