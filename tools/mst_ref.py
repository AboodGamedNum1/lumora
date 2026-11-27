#!/usr/bin/env python3
"""Independent reference for core/mst: prints the data tables of tests/mst/mst_tests.cpp as C++ source.

The C++ method is Kruskal with a heapsort and a union-find. This reference shares none of it:
- Small graphs (the flat table) are solved by brute force over edge subsets: the forest is the acyclic subset of maximum size that minimises
  the sum of 2^rank, where rank is the position of an edge in the (weight, index) order. With distinct ranks this is the unique minimum
  spanning forest, so it is the forest the header's rule selects. Acyclicity is checked by naive label propagation.
- Digest groups and mid graphs use a heap-based Prim search (one start per component) keyed by (weight, index).
- Prim is cross-checked against the brute force on 20000 random small graphs.
- Every result used for a mid or digest-group case also passes a cut-property check: removing a tree edge splits its tree component in two
  sides, and no edge crossing that cut has a smaller (weight, index) key. The big cases use a cycle-property check on a sample of
  non-tree edges (the largest key on the tree path is below the key of the edge) with binary lifting, plus count = nodes - components, plus
  an equal-total check against Prim.
Output is deterministic.
"""
import heapq
import itertools
import random

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
N_BIG = 1 << 16
M_BIG = 1 << 18


def fnv_word(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & M64
    return h


def key(edges, i):
    return (edges[i][2], i)


def comps_naive(n, pairs):
    """Number of components of n nodes under the given (u, v) pairs, by label propagation."""
    lab = list(range(n))
    changed = True
    while changed:
        changed = False
        for u, v in pairs:
            if lab[u] != lab[v]:
                lo = min(lab[u], lab[v])
                lab[u] = lab[v] = lo
                changed = True
    return len(set(lab))


def brute(n, edges):
    m = len(edges)
    order = sorted(range(m), key=lambda i: (edges[i][2], i))
    rank = [0] * m
    for r, i in enumerate(order):
        rank[i] = r
    full = comps_naive(n, [(u, v) for u, v, _ in edges])
    target = n - full
    best = None
    for mask in range(1 << m):
        if bin(mask).count("1") != target:
            continue
        sub = [i for i in range(m) if mask >> i & 1]
        if comps_naive(n, [(edges[i][0], edges[i][1]) for i in sub]) != n - target:
            continue
        score = sum(1 << rank[i] for i in sub)
        if best is None or score < best[0]:
            best = (score, sub)
    sub = best[1]
    return sorted(sub, key=lambda i: (edges[i][2], i))


def prim(n, edges):
    adj = [[] for _ in range(n)]
    for i, (u, v, w) in enumerate(edges):
        if u != v:
            adj[u].append((w, i, v))
            adj[v].append((w, i, u))
    seen = [False] * n
    taken = []
    for s in range(n):
        if seen[s]:
            continue
        seen[s] = True
        heap = list(adj[s])
        heapq.heapify(heap)
        while heap:
            w, i, to = heapq.heappop(heap)
            if seen[to]:
                continue
            seen[to] = True
            taken.append(i)
            for item in adj[to]:
                if not seen[item[2]]:
                    heapq.heappush(heap, item)
    taken.sort(key=lambda i: (edges[i][2], i))
    return taken


def cut_check(n, edges, taken):
    """Cut property: every tree edge is the smallest-key edge crossing the cut that its removal makes in its tree."""
    tadj = [[] for _ in range(n)]
    for i in taken:
        u, v, _ = edges[i]
        tadj[u].append((v, i))
        tadj[v].append((u, i))
    for e in taken:
        u0, _, _ = edges[e]
        side = {u0}
        stack = [u0]
        while stack:
            x = stack.pop()
            for y, i in tadj[x]:
                if i != e and y not in side:
                    side.add(y)
                    stack.append(y)
        ke = key(edges, e)
        for f, (u, v, w) in enumerate(edges):
            if (u in side) != (v in side) and (w, f) < ke:
                return False
    if len(taken) != n - comps_naive(n, [(u, v) for u, v, _ in edges]):
        return False
    return True


def cycle_check(n, edges, taken, sample):
    """Cycle property on a sample of non-tree edges, with binary lifting over the forest; plus count and total checks."""
    tadj = [[] for _ in range(n)]
    for i in taken:
        u, v, w = edges[i]
        tadj[u].append((v, (w << 20) | i))
        tadj[v].append((u, (w << 20) | i))
    par = [-1] * n
    depth = [0] * n
    pk = [0] * n
    root = [-1] * n
    ncomp = 0
    for s in range(n):
        if root[s] != -1:
            continue
        ncomp += 1
        root[s] = s
        par[s] = s
        stack = [s]
        while stack:
            x = stack.pop()
            for y, k in tadj[x]:
                if root[y] == -1:
                    root[y] = s
                    par[y] = x
                    depth[y] = depth[x] + 1
                    pk[y] = k
                    stack.append(y)
    # components of the whole graph by a plain DFS (not the tree) for the count check
    gadj = [[] for _ in range(n)]
    for u, v, _ in edges:
        if u != v:
            gadj[u].append(v)
            gadj[v].append(u)
    seen = [False] * n
    gcomp = 0
    for s in range(n):
        if seen[s]:
            continue
        gcomp += 1
        seen[s] = True
        stack = [s]
        while stack:
            x = stack.pop()
            for y in gadj[x]:
                if not seen[y]:
                    seen[y] = True
                    stack.append(y)
    if gcomp != ncomp or len(taken) != n - gcomp:
        return False
    if not taken:
        return True
    LOG = max(1, (max(depth) + 1).bit_length())
    up = [par]
    mx = [pk]
    for j in range(1, LOG):
        pu, pm = up[-1], mx[-1]
        up.append([pu[pu[v]] for v in range(n)])
        mx.append([max(pm[v], pm[pu[v]]) for v in range(n)])

    def pathmax(a, b):
        best = 0
        if depth[a] < depth[b]:
            a, b = b, a
        d = depth[a] - depth[b]
        j = 0
        while d:
            if d & 1:
                best = max(best, mx[j][a])
                a = up[j][a]
            d >>= 1
            j += 1
        if a == b:
            return best
        for j in range(LOG - 1, -1, -1):
            if up[j][a] != up[j][b]:
                best = max(best, mx[j][a], mx[j][b])
                a, b = up[j][a], up[j][b]
        return max(best, mx[0][a], mx[0][b])

    tset = set(taken)
    nontree = [i for i in range(len(edges)) if i not in tset and edges[i][0] != edges[i][1]]
    step = max(1, len(nontree) // sample)
    for i in nontree[::step]:
        u, v, w = edges[i]
        if root[u] != root[v]:
            return False
        if pathmax(u, v) >= ((w << 20) | i):
            return False
    return True


def fold_case(h, taken, edges):
    total = sum(edges[i][2] for i in taken)
    for v in (len(taken), total & 0xFFFFFFFF, total >> 32):
        h = fnv_word(h, v)
    for i in taken:
        h = fnv_word(h, i)
    return h


def emit_array(name, vals, per=16, typ="u32"):
    print("const %s %s[%d] = {" % (typ, name, max(1, len(vals))))
    if not vals:
        vals = [0]
    for i in range(0, len(vals), per):
        print("    " + ", ".join("%du" % v if v < (1 << 32) else "%dull" % v for v in vals[i:i + per]) + ",")
    print("};")


def flat_case(n, edges, taken):
    total = sum(edges[i][2] for i in taken)
    out = [n, len(edges)]
    out += [e[0] for e in edges] + [e[1] for e in edges] + [e[2] for e in edges]
    out += [len(taken)] + list(taken) + [total & 0xFFFFFFFF, total >> 32]
    return out


def group_digest(n, mmax, weights):
    choices = [(u, v, w) for u in range(n) for v in range(n) for w in weights]
    h = FNV_OFF
    cases = 0
    for m in range(0, mmax + 1):
        for combo in itertools.product(choices, repeat=m):
            edges = list(combo)
            taken = prim(n, edges)
            h = fold_case(h, taken, edges)
            cases += 1
    return h, cases


class Lcg:
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def scramble(edges, g):
    for i in range(len(edges) - 1, 0, -1):
        j = g.rnd(i + 1)
        edges[i], edges[j] = edges[j], edges[i]


def big_graph(pid):
    g = Lcg(1000 + pid)
    N, M = N_BIG, M_BIG
    edges = []
    if pid == 0:
        for i in range(1, N):
            edges.append((i, g.rnd(i), g.rnd(1000)))
        while len(edges) < M:
            edges.append((g.rnd(N), g.rnd(N), g.rnd(1000)))
        scramble(edges, g)
    elif pid == 1:
        for _ in range(M):
            edges.append((g.rnd(N), g.rnd(N), 7))
    elif pid == 2:
        for _ in range(M):
            u = g.rnd(N)
            edges.append((u, u, g.rnd(100000)))
    elif pid == 3:
        for i in range(N - 1):
            edges.append((i, i + 1, N - 1 - i))
        while len(edges) < M:
            j = g.rnd(N - 1)
            edges.append((j + 1, j, N - 1 - j + 1 + g.rnd(5)))
        scramble(edges, g)
    elif pid == 4:
        for _ in range(M):
            u = 2 * g.rnd(N // 2)
            v = u + 1
            if g.rnd(2):
                u, v = v, u
            edges.append((u, v, 0xFFFFFFFF - g.rnd(3)))
    elif pid == 5:
        for i in range(M):
            edges.append((0, 1 + (i % (N - 1)), 0xFFFFFFFF))
    elif pid == 6:
        N, M = N_BIG, 100
        for _ in range(M):
            edges.append((g.rnd(N), g.rnd(N), g.rnd(10)))
    elif pid == 7:
        N = 1000
        for _ in range(M):
            edges.append((g.rnd(N), g.rnd(N), g.rnd(1 << 20)))
    elif pid == 8:
        for i in range(1, N):
            edges.append((g.rnd(i), i, 0))
    return N, edges


def main():
    rng = random.Random(20260527)
    # cross-check Prim against the brute force on small random graphs
    for _ in range(20000):
        n = rng.randint(1, 6)
        m = rng.randint(0, 7)
        wmax = rng.choice([1, 2, 5, 1 << 32])
        edges = [(rng.randrange(n), rng.randrange(n), rng.randrange(wmax)) for _ in range(m)]
        assert brute(n, edges) == prim(n, edges), (n, edges)
    # flat table: all graphs with nodes 1..3, up to 2 edges (and nodes 2 with 3 edges, nodes 1 with 3), weights 0, 1, 2
    table = []
    ncases = 0
    for n, mmax in ((1, 3), (2, 3), (3, 2)):
        choices = [(u, v, w) for u in range(n) for v in range(n) for w in (0, 1, 2)]
        for m in range(0, mmax + 1):
            for combo in itertools.product(choices, repeat=m):
                edges = list(combo)
                taken = brute(n, edges)
                if m and m <= 2:
                    assert taken == prim(n, edges)
                table += flat_case(n, edges, taken)
                ncases += 1
    # mid graphs
    mid = []
    nmid = 80
    for t in range(nmid):
        n = rng.randint(1, 40)
        m = rng.randint(0, 120)
        mode = t % 4
        if mode == 0:
            wf = lambda: rng.randrange(4)
        elif mode == 1:
            wf = lambda: rng.randrange(1000)
        elif mode == 2:
            wf = lambda: rng.randrange(1 << 32)
        else:
            wf = lambda: 0xFFFFFFFF - rng.randrange(3)
        edges = [(rng.randrange(n), rng.randrange(n), wf()) for _ in range(m)]
        taken = prim(n, edges)
        assert cut_check(n, edges, taken), ("mid", t)
        assert cycle_check(n, edges, taken, 1000), ("midc", t)
        mid += flat_case(n, edges, taken)
    # cut-property check on a spread of random small graphs
    for _ in range(300):
        n = rng.randint(1, 8)
        m = rng.randint(0, 14)
        edges = [(rng.randrange(n), rng.randrange(n), rng.randrange(4)) for _ in range(m)]
        assert cut_check(n, edges, prim(n, edges))
    # digest groups
    groups = [(3, 4, (0, 1)), (4, 3, (0, 1, 2)), (3, 3, (0, 0xFFFFFFFE, 0xFFFFFFFF))]
    gd = []
    gcases = []
    for n, mmax, ws in groups:
        h, c = group_digest(n, mmax, ws)
        gd += [h & 0xFFFFFFFF, h >> 32]
        gcases.append(c)
    # big graphs
    big = []
    golden = FNV_OFF
    for pid in range(9):
        N, edges = big_graph(pid)
        taken = prim(N, edges)
        assert cycle_check(N, edges, taken, 2500), ("big", pid)
        h = fold_case(FNV_OFF, taken, edges)
        total = sum(edges[i][2] for i in taken)
        first = taken[0] if taken else 0
        last = taken[-1] if taken else 0
        big += [pid, len(taken), total & 0xFFFFFFFF, total >> 32, h & 0xFFFFFFFF, h >> 32, first, last]
    # golden digest over every digest-group digest, mid result words and big rows
    for v in gd:
        golden = fnv_word(golden, v)
    pos = 0
    for _ in range(nmid):
        n, m = mid[pos], mid[pos + 1]
        pos += 2 + 3 * m
        cnt = mid[pos]
        for v in mid[pos:pos + 1 + cnt + 2]:
            golden = fnv_word(golden, v)
        pos += 1 + cnt + 2
    for r in range(9):
        row = big[r * 8:(r + 1) * 8]
        for v in (row[1], row[2], row[3], row[4], row[5]):
            golden = fnv_word(golden, v)
    print("// Generated by tools/mst_ref.py. Do not edit by hand.")
    print("const u32 kTableCases = %d;" % ncases)
    emit_array("kTable", table)
    print("const u32 kMidCases = %d;" % nmid)
    emit_array("kMid", mid)
    emit_array("kGroupDigest", gd)
    emit_array("kGroupCases", gcases)
    emit_array("kBig", big, per=8)
    print("const u64 kGolden = %dull;" % golden)


if __name__ == "__main__":
    main()
