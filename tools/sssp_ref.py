#!/usr/bin/env python3
"""Reference for core/sssp. Prints data.inc (tables and digests) for tests/sssp/sssp_tests.cpp.

Small graphs: brute force over ALL simple paths from the source (every sequence of distinct nodes joined by edges, one entry per edge sequence).
With weights of at least 1 a shortest walk is a simple path, so the shortest length, the number of shortest edge sequences and the smallest
predecessor come straight from that list, with no use of any shortest-path algorithm.
Large graphs: Dijkstra with heapq, then counts and predecessors by one pass over the nodes in increasing distance order over the tight edges
(dist[u] + w == dist[v]). `python3 tools/sssp_ref.py --selfcheck` compares the two methods and a Bellman-Ford relaxation on 30000 random graphs.
"""
import heapq
import itertools
import os
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
INF = M64
NONE = 0xFFFFFFFF
NN = 65536
MM = 262144


def fold(h, v, nbytes):
    for _ in range(nbytes):
        h = ((h ^ (v & 0xFF)) * FNV_PRIME) & M64
        v >>= 8
    return h


def fold_result(h, dist, count, parent):
    """FNV-1a over little-endian bytes: per node, dist (8 bytes), count (8 bytes), parent (4 bytes)."""
    for d, c, p in zip(dist, count, parent):
        h = fold(h, d, 8)
        h = fold(h, c, 8)
        h = fold(h, p, 4)
    return h


class Lcg:
    def __init__(self, s):
        self.s = s

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k

    def w32(self):
        a = self.next()
        b = self.next()
        return (((a << 1) | (b & 1)) % 4294967295) + 1


def brute(n, edges, s):
    out = [[] for _ in range(n)]
    for u, v, w in edges:
        out[u].append((v, w))
    best = [INF] * n
    cnt = [0] * n
    par = [NONE] * n
    seen = [False] * n
    seen[s] = True
    best[s] = 0
    cnt[s] = 1

    def rec(u, length):
        for v, w in out[u]:
            if seen[v]:
                continue
            l = length + w
            if l < best[v]:
                best[v], cnt[v], par[v] = l, 1, u
            elif l == best[v]:
                cnt[v] += 1
                if u < par[v]:
                    par[v] = u
            seen[v] = True
            rec(v, l)
            seen[v] = False

    rec(s, 0)
    # a path reaches v only by a simple path whose last step is the entry above; every simple path to v was counted once
    return best, [c & M64 for c in cnt], par


def large(n, edges, s):
    out = [[] for _ in range(n)]
    inn = [[] for _ in range(n)]
    for u, v, w in edges:
        out[u].append((v, w))
        inn[v].append((u, w))
    dist = [INF] * n
    dist[s] = 0
    pq = [(0, s)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u]:
            continue
        for v, w in out[u]:
            if d + w < dist[v]:
                dist[v] = d + w
                heapq.heappush(pq, (d + w, v))
    order = sorted((v for v in range(n) if dist[v] != INF), key=lambda v: dist[v])
    count = [0] * n
    parent = [NONE] * n
    count[s] = 1
    for v in order:
        if v == s:
            continue
        c = 0
        p = NONE
        for u, w in inn[v]:
            if dist[u] != INF and dist[u] + w == dist[v]:
                c += count[u]
                if u < p:
                    p = u
        count[v] = c & M64
        parent[v] = p
    return dist, count, parent


def bellman(n, edges, s):
    dist = [INF] * n
    dist[s] = 0
    for _ in range(n):
        ch = False
        for u, v, w in edges:
            if dist[u] != INF and dist[u] + w < dist[v]:
                dist[v] = dist[u] + w
                ch = True
        if not ch:
            break
    return dist


def selfcheck():
    r = Lcg(31337)
    bad = 0
    for k in range(30000):
        n = 1 + r.rnd(8)
        m = r.rnd(16)
        wm = 1 + r.rnd(5)
        edges = [(r.rnd(n), r.rnd(n), 1 + r.rnd(wm)) for _ in range(m)]
        s = r.rnd(n)
        a = brute(n, edges, s)
        b = large(n, edges, s)
        if list(a[0]) != b[0] or a[1] != b[1] or a[2] != b[2] or bellman(n, edges, s) != b[0]:
            bad += 1
            if bad < 4:
                print("MISMATCH", n, edges, s, a, b)
    print("selfcheck mismatches:", bad)


def kinds(n):
    return [(u, v, w) for u in range(n) for v in range(n) for w in (1, 2, 3)]


def group(name, n, mmax, ordered, cache):
    fn = os.path.join(cache, name) if cache else None
    if fn and os.path.exists(fn):
        return [int(x) for x in open(fn).read().split()]
    K = kinds(n)
    h = FNV_OFF
    graphs = 0
    for m in range(mmax + 1):
        it = itertools.product(K, repeat=m) if ordered else itertools.combinations_with_replacement(K, m)
        for edges in it:
            graphs += 1
            for s in range(n):
                d, c, p = brute(n, edges, s)
                h = fold_result(h, d, c, p)
    res = [graphs, h & 0xFFFFFFFF, h >> 32]
    if fn:
        open(fn, "w").write(" ".join(map(str, res)))
    return res


def gen_random(r, n, m, wmax):
    return [(r.rnd(n), r.rnd(n), 1 + r.rnd(wmax)) for _ in range(m)]


def big(pid):
    r = Lcg(9000 + pid)
    n, edges, s = NN, [], 0
    if pid == 0:
        edges = [(i, i + 1, 4294967295) for i in range(NN - 1)]
    elif pid == 1:
        edges = [(i + 1, i, 4294967295) for i in range(NN - 1)]
        s = NN - 1
    elif pid == 2:
        for a in range(256):
            for b in range(256):
                if b + 1 < 256:
                    edges.append((256 * a + b, 256 * a + b + 1, 1))
                if a + 1 < 256:
                    edges.append((256 * a + b, 256 * (a + 1) + b, 1))
    elif pid == 3:
        for a in range(256):
            for b in range(256):
                if b + 1 < 256:
                    edges.append((256 * a + b, 256 * a + b + 1, 1 + r.rnd(3)))
                    edges.append((256 * a + b + 1, 256 * a + b, 1 + r.rnd(3)))
                if a + 1 < 256:
                    edges.append((256 * a + b, 256 * (a + 1) + b, 1 + r.rnd(3)))
                    edges.append((256 * (a + 1) + b, 256 * a + b, 1 + r.rnd(3)))
        s = 256 * 128 + 128
    elif pid == 4:
        for i in range(1, NN):
            edges.append((0, i, 1 + (i % 7)))
            edges.append((i, 0, 1))
    elif pid == 5:
        for i in range(1, NN):
            edges.append((i, 0, 1 + (i % 5)))
        s = 5
    elif pid == 6:
        for k in range(NN // 2 - 1):
            for a in (0, 1):
                for b in (0, 1):
                    edges.append((2 * k + a, 2 * (k + 1) + b, 1))
    elif pid == 7:
        n = 3 * 21845
        for k in range(21844):
            for a in range(3):
                for b in range(3):
                    edges.append((3 * k + a, 3 * (k + 1) + b, 1))
    elif pid == 8:
        n = 16384
        for b in range(1024):
            for x in range(16):
                for y in range(16):
                    if x != y:
                        edges.append((16 * b + x, 16 * b + y, 1 + r.rnd(3)))
    elif pid == 9:
        for i in range(NN - 1):
            for w in (1, 1, 2, 1):
                edges.append((i, i + 1, w))
    elif pid == 10:
        s = 7
    elif pid == 11:
        edges = gen_random(r, NN, MM, 100)
    elif pid == 12:
        edges = [(r.rnd(NN), r.rnd(NN), 0) for _ in range(MM)]
        edges = [(u, v, r.w32()) for u, v, _ in edges]
    elif pid == 13:
        edges = gen_random(r, NN, 100000, 2)
    elif pid == 14:
        n = 512
        edges = [(i, j, 1 + r.rnd(2)) for i in range(512) for j in range(512)]
    return n, edges, s


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
    # flat table: every multiset of edges on 1 and 2 nodes with at most 3 edges, every source, full results
    rows = []
    for n, mmax in ((1, 3), (2, 3)):
        K = kinds(n)
        for m in range(mmax + 1):
            for edges in itertools.combinations_with_replacement(K, m):
                for s in range(n):
                    d, c, p = brute(n, edges, s)
                    rows += [n, m, s]
                    for e in edges:
                        rows += list(e)
                    for v in range(n):
                        rows += [d[v] & 0xFFFFFFFF, d[v] >> 32, c[v] & 0xFFFFFFFF, c[v] >> 32, p[v]]
    P("const u32 kFlat[%d] = {" % len(rows))
    for i in range(0, len(rows), 20):
        P("    " + ", ".join("%du" % v for v in rows[i:i + 20]) + ",")
    P("};")
    P("const u32 kFlatLen = %d;" % len(rows))
    groups = [("A1", 1, 6, False), ("B", 2, 8, False), ("C", 3, 5, False), ("D", 2, 5, True), ("E", 3, 3, True)]
    P("// groups: graphs, digest lo, digest hi")
    P("const u32 kGroups[%d] = {" % (3 * len(groups)))
    for name, n, mm, od in groups:
        g = group(name, n, mm, od, cache)
        P("    %du, %du, %du,  // %s n=%d m<=%d %s" % (g[0], g[1], g[2], name, n, mm, "ordered" if od else "multisets"))
    P("};")
    # random battery and mid graphs
    r = Lcg(8181)
    h = FNV_OFF
    for k in range(20000):
        n = 1 + r.rnd(10)
        m = r.rnd(41)
        wm = 1 + r.rnd(6)
        edges = gen_random(r, n, m, wm)
        s = r.rnd(n)
        d, c, p = large(n, edges, s)
        h = fold_result(h, d, c, p)
    P("const u32 kBattery[2] = {%du, %du};" % (h & 0xFFFFFFFF, h >> 32))
    r = Lcg(8282)
    h = FNV_OFF
    for k in range(100):
        n = 1 + r.rnd(9)
        m = r.rnd(21)
        wm = 1 + r.rnd(9)
        edges = gen_random(r, n, m, wm)
        s = r.rnd(n)
        a = brute(n, edges, s)
        b = large(n, edges, s)
        assert list(a[0]) == b[0] and a[1] == b[1] and a[2] == b[2]
        h = fold_result(h, b[0], b[1], b[2])
    P("const u32 kMid[2] = {%du, %du};" % (h & 0xFFFFFFFF, h >> 32))
    P("// max-size patterns: pid, n, m, source, reachable nodes, digest lo, digest hi")
    P("const u32 kBig[%d] = {" % (7 * 15))
    for pid in range(15):
        n, edges, s = big(pid)
        d, c, p = large(n, edges, s)
        reach = sum(1 for x in d if x != INF)
        h = fold_result(FNV_OFF, d, c, p)
        P("    %du, %du, %du, %du, %du, %du, %du," % (pid, n, len(edges), s, reach, h & 0xFFFFFFFF, h >> 32))
        sys.stderr.write("pid %d n %d m %d reach %d maxdist %d\n" % (pid, n, len(edges), reach, max(x for x in d if x != INF)))
    P("};")
    print("\n".join(out))


if __name__ == "__main__":
    main()
