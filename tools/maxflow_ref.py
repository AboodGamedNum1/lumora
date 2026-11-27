#!/usr/bin/env python3
"""Independent reference for core/maxflow: prints the data tables of tests/maxflow/maxflow_tests.cpp as C++ source.

The C++ method is Dinic (BFS levels, blocking flow by depth-first search). This reference uses none of it:
- Small graphs (the flat table) are solved by brute force over node subsets: the maximum flow value equals the minimum capacity of
  a cut (S, V - S) with src in S and sink outside S, and the source side of the minimum cut closest to src is the intersection of all
  minimum-capacity sets S.
- Digest groups and mid graphs use Edmonds-Karp (shortest augmenting paths by BFS on a residual graph kept in Python lists). The
  residual-reachable set from src after the last augmentation is computed separately, and it is checked against the brute-force
  intersection on 20000 random small graphs.
- Max-size graphs use FIFO push-relabel with global relabeling and a final flow-conservation check. Every result is also checked by the
  certificate: the capacity of the cut around the returned side set equals the flow value, no residual arc leaves the side set, and src
  is inside while the sink is outside.
Output is deterministic.
"""
import itertools
import random
from collections import deque

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
MAXC = 0xFFFFFFFF
N_BIG = 4096
M_BIG = 65536


def fnv_word(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & M64
    return h


def brute(n, edges, s, t):
    best = None
    sets = []
    others = [v for v in range(n) if v != s and v != t]
    for mask in range(1 << len(others)):
        S = {s}
        for i, v in enumerate(others):
            if mask >> i & 1:
                S.add(v)
        c = sum(cap for u, v, cap in edges if u in S and v not in S)
        if best is None or c < best:
            best = c
            sets = [S]
        elif c == best:
            sets.append(S)
    inter = set.intersection(*sets)
    return best, [1 if v in inter else 0 for v in range(n)]


class Residual:
    def __init__(self, n, edges):
        self.n = n
        self.adj = [[] for _ in range(n)]
        self.to = []
        self.cap = []
        for u, v, c in edges:
            if u == v:
                continue
            self.adj[u].append(len(self.to))
            self.to.append(v)
            self.cap.append(c)
            self.adj[v].append(len(self.to))
            self.to.append(u)
            self.cap.append(0)

    def reach(self, s):
        seen = [0] * self.n
        seen[s] = 1
        q = deque([s])
        while q:
            u = q.popleft()
            for a in self.adj[u]:
                if self.cap[a] > 0 and not seen[self.to[a]]:
                    seen[self.to[a]] = 1
                    q.append(self.to[a])
        return seen


def edmonds_karp(n, edges, s, t):
    r = Residual(n, edges)
    flow = 0
    while True:
        prev = [-1] * n
        prev[s] = -2
        q = deque([s])
        while q and prev[t] == -1:
            u = q.popleft()
            for a in r.adj[u]:
                v = r.to[a]
                if r.cap[a] > 0 and prev[v] == -1:
                    prev[v] = a
                    q.append(v)
        if prev[t] == -1:
            break
        b = None
        v = t
        while v != s:
            a = prev[v]
            b = r.cap[a] if b is None else min(b, r.cap[a])
            v = r.to[a ^ 1]
        v = t
        while v != s:
            a = prev[v]
            r.cap[a] -= b
            r.cap[a ^ 1] += b
            v = r.to[a ^ 1]
        flow += b
    return flow, r.reach(s)


def push_relabel(n, edges, s, t):
    r = Residual(n, edges)
    adj, to, cap = r.adj, r.to, r.cap
    height = [0] * n
    excess = [0] * n
    cur = [0] * n

    def global_relabel():
        # exact distances to the sink in the residual graph; nodes that cannot reach it get n + distance to the source
        d = [-1] * n
        d[t] = 0
        q = deque([t])
        while q:
            v = q.popleft()
            for a in adj[v]:
                u = to[a]
                if d[u] == -1 and cap[a ^ 1] > 0 and u != s:
                    d[u] = d[v] + 1
                    q.append(u)
        d[s] = n
        q = deque([s])
        seen = [False] * n
        seen[s] = True
        while q:
            v = q.popleft()
            for a in adj[v]:
                u = to[a]
                if not seen[u] and d[u] == -1 and cap[a ^ 1] > 0:
                    seen[u] = True
                    d[u] = d[v] + 1
                    q.append(u)
        for v in range(n):
            if d[v] == -1:
                d[v] = 2 * n
            height[v] = d[v]
            cur[v] = 0

    height[s] = n
    for a in adj[s]:
        c = cap[a]
        if c > 0:
            v = to[a]
            cap[a] = 0
            cap[a ^ 1] += c
            excess[v] += c
    global_relabel()
    fifo = deque(v for v in range(n) if v != s and v != t and excess[v] > 0)
    inq = [False] * n
    for v in fifo:
        inq[v] = True
    work = 0
    while fifo:
        u = fifo.popleft()
        inq[u] = False
        while excess[u] > 0:
            if cur[u] == len(adj[u]):
                best = None
                for a in adj[u]:
                    if cap[a] > 0 and (best is None or height[to[a]] < best):
                        best = height[to[a]]
                height[u] = best + 1
                cur[u] = 0
                work += len(adj[u])
                if work > 6 * n + 4 * len(to) // 2:
                    work = 0
                    global_relabel()
                continue
            a = adj[u][cur[u]]
            v = to[a]
            if cap[a] > 0 and height[u] == height[v] + 1:
                d = min(excess[u], cap[a])
                cap[a] -= d
                cap[a ^ 1] += d
                excess[u] -= d
                excess[v] += d
                if v != s and v != t and not inq[v]:
                    inq[v] = True
                    fifo.append(v)
            else:
                cur[u] += 1
    return excess[t], r.reach(s), r


def check_certificate(n, edges, s, t, flow, side, resid=None):
    if side[s] != 1 or side[t] != 0:
        return False
    cut = sum(c for u, v, c in edges if side[u] and not side[v])
    if cut != flow:
        return False
    if resid is None:
        return True
    # no residual arc leaves the side set, and flow conservation holds at every node but s and t
    for u in range(n):
        for a in resid.adj[u]:
            if side[u] and not side[resid.to[a]] and resid.cap[a] > 0:
                return False
    net = [0] * n
    k = 0
    for u, v, c in edges:
        if u == v:
            continue
        f = resid.cap[k + 1]
        net[u] -= f
        net[v] += f
        k += 2
    for v in range(n):
        if v not in (s, t) and net[v] != 0:
            return False
    return net[t] == flow and net[s] == -flow


def fold_result(h, flow, side):
    h = fnv_word(h, flow & 0xFFFFFFFF)
    h = fnv_word(h, flow >> 32)
    for b in side:
        h = fnv_word(h, b)
    return h


def emit_array(name, vals, per=16):
    print("const u32 %s[%d] = {" % (name, max(1, len(vals))))
    if not vals:
        vals = [0]
    for i in range(0, len(vals), per):
        print("    " + ", ".join("%du" % v for v in vals[i:i + per]) + ",")
    print("};")


def group_digest(n, mmax, caps, pairs):
    choices = [(u, v, c) for u in range(n) for v in range(n) for c in caps]
    h = FNV_OFF
    cases = 0
    for m in range(0, mmax + 1):
        for combo in itertools.product(choices, repeat=m):
            edges = list(combo)
            for s, t in pairs:
                flow, side = edmonds_karp(n, edges, s, t)
                h = fold_result(h, flow, side)
                cases += 1
    return h, cases


def all_pairs(n):
    return [(s, t) for s in range(n) for t in range(n) if s != t]


class Lcg:
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def full_cap(g):
    hi = g.next()
    lo = g.rnd(2)
    return ((hi << 1) | lo) & 0xFFFFFFFF


def scramble(edges, g):
    for i in range(len(edges) - 1, 0, -1):
        j = g.rnd(i + 1)
        edges[i], edges[j] = edges[j], edges[i]


def big_graph(pid):
    g = Lcg(2000 + pid)
    N, M = N_BIG, M_BIG
    e = []
    s, t = 0, N - 1
    if pid == 0:
        for _ in range(M):
            l = g.rnd(63)
            u = l * 64 + g.rnd(64)
            v = (l + 1) * 64 + g.rnd(64)
            e.append((u, v, 1 + g.rnd(1000)))
    elif pid == 1:
        for _ in range(M // 2):
            e.append((0, 1, MAXC))
        for _ in range(M // 2):
            e.append((1, N - 1, MAXC))
    elif pid == 2:
        for i in range(N - 1):
            e.append((i, i + 1, 5 + g.rnd(50)))
        while len(e) < M:
            j = g.rnd(N - 1)
            e.append((j, j + 1, g.rnd(6)))
        scramble(e, g)
    elif pid == 3:
        for i in range(2047):
            e.append((0, 1 + i, 1 + g.rnd(9)))
        for i in range(2047):
            e.append((2048 + i, N - 1, 1 + g.rnd(9)))
        while len(e) < M:
            u = 1 + g.rnd(2047)
            v = 2048 + g.rnd(2047)
            e.append((u, v, 1 + g.rnd(3)))
    elif pid == 4:
        for _ in range(M):
            u = g.rnd(N)
            v = g.rnd(N)
            z = g.rnd(10)
            c = 0 if z < 9 else full_cap(g)
            e.append((u, v, c))
    elif pid == 5:
        s, t = 5, 4000
        for _ in range(M):
            u = g.rnd(N)
            e.append((u, u, 1 + g.rnd(100)))
    elif pid == 6:
        for _ in range(M):
            u = g.rnd(2048)
            v = g.rnd(2048)
            e.append((u, v, 1 + g.rnd(100)))
    elif pid == 7:
        t = 63
        for _ in range(M):
            u = g.rnd(64)
            v = g.rnd(64)
            z = g.rnd(4)
            c = MAXC if z == 0 else full_cap(g)
            e.append((u, v, c))
    elif pid == 9:
        for _ in range(M):
            e.append((0, N - 1, MAXC))
    else:
        for i in range(64):
            for j in range(63):
                a = i * 64 + j
                e.append((a, a + 1, 1 + g.rnd(10)))
                e.append((a + 1, a, 1 + g.rnd(10)))
        for i in range(63):
            for j in range(64):
                a = i * 64 + j
                e.append((a, a + 64, 1 + g.rnd(10)))
                e.append((a + 64, a, 1 + g.rnd(10)))
        for i in range(1, 64):
            e.append((0, i * 64, 1000))
        for i in range(63):
            e.append((i * 64 + 63, N - 1, 1000))
        while len(e) < M:
            i = g.rnd(63)
            j = g.rnd(63)
            a = i * 64 + j
            d = 1 if g.rnd(2) == 0 else 64
            e.append((a, a + d, 1 + g.rnd(10)))
    assert len(e) == M, (pid, len(e))
    return N, e, s, t


def battery_graph(g, it):
    """Random small graph number `it` of the cancellation battery (mirrored in the test)."""
    n = 2 + g.rnd(11)
    m = g.rnd(41)
    mode = it % 4
    edges = []
    for _ in range(m):
        u = g.rnd(n)
        v = g.rnd(n)
        if mode == 0:
            c = g.rnd(5)
        elif mode == 1:
            c = g.rnd(1000000)
        elif mode == 2:
            c = full_cap(g)
        else:
            c = (0, 1, MAXC)[g.rnd(3)]
        edges.append((u, v, c))
    s = g.rnd(n)
    t = g.rnd(n - 1)
    if t >= s:
        t += 1
    return n, edges, s, t


BATTERY_CASES = 20000


def battery_digest():
    g = Lcg(777)
    h = FNV_OFF
    for it in range(BATTERY_CASES):
        n, edges, s, t = battery_graph(g, it)
        flow, side = edmonds_karp(n, edges, s, t)
        h = fold_result(h, flow, side)
    return h


def main():
    rng = random.Random(20260528)
    for _ in range(20000):
        n = rng.randint(2, 6)
        m = rng.randint(0, 8)
        cm = rng.choice([1, 2, 5, MAXC])
        edges = [(rng.randrange(n), rng.randrange(n), rng.randint(0, cm)) for _ in range(m)]
        s, t = rng.sample(range(n), 2)
        bf, bs = brute(n, edges, s, t)
        ef, es = edmonds_karp(n, edges, s, t)
        assert bf == ef and bs == es, (n, edges, s, t)
    # flat table, packed: header n | m << 4 | src << 8 | sink << 12; each edge u | v << 4 | cap << 8; result flow | side mask << 8
    table = []
    ncases = 0
    for n, mmax in ((2, 3), (3, 2)):
        choices = [(u, v, c) for u in range(n) for v in range(n) for c in (0, 1, 2)]
        for m in range(0, mmax + 1):
            for combo in itertools.product(choices, repeat=m):
                edges = list(combo)
                for s, t in all_pairs(n):
                    flow, side = brute(n, edges, s, t)
                    ef, es = edmonds_karp(n, edges, s, t)
                    assert (flow, side) == (ef, es)
                    assert flow < 256
                    table.append(n | (m << 4) | (s << 8) | (t << 12))
                    for u, v, c in edges:
                        table.append(u | (v << 4) | (c << 8))
                    table.append(flow | (sum(b << i for i, b in enumerate(side)) << 8))
                    ncases += 1
    # mid graphs: n, m, eu[], ev[], cap[], src, sink, flow lo, flow hi, side[]
    mid = []
    nmid = 80
    for tcase in range(nmid):
        n = rng.randint(2, 24)
        m = rng.randint(0, 120)
        mode = tcase % 4
        if mode == 0:
            cf = lambda: rng.randrange(4)
        elif mode == 1:
            cf = lambda: rng.randrange(1000)
        elif mode == 2:
            cf = lambda: rng.randrange(1 << 32)
        else:
            cf = lambda: rng.choice((0, MAXC, MAXC, 1))
        edges = [(rng.randrange(n), rng.randrange(n), cf()) for _ in range(m)]
        s, t = rng.sample(range(n), 2)
        flow, side = edmonds_karp(n, edges, s, t)
        assert check_certificate(n, edges, s, t, flow, side)
        mid += [n, m] + [e[0] for e in edges] + [e[1] for e in edges] + [e[2] for e in edges]
        mid += [s, t, flow & 0xFFFFFFFF, flow >> 32] + side
    # digest groups
    groups = [(3, 3, (0, 1, 2)), (4, 2, (0, 1, MAXC)), (3, 3, (0, MAXC)), (4, 3, (1,))]
    gd = []
    gcases = []
    for n, mmax, caps in groups:
        h, c = group_digest(n, mmax, caps, all_pairs(n))
        gd += [h & 0xFFFFFFFF, h >> 32]
        gcases.append(c)
    # big graphs
    big = []
    golden = FNV_OFF
    for v in gd:
        golden = fnv_word(golden, v)
    pos = 0
    for tcase in range(nmid):
        n, m = mid[pos], mid[pos + 1]
        pos += 2 + 3 * m
        for v in mid[pos:pos + 4 + n]:
            golden = fnv_word(golden, v)
        pos += 4 + n
    for pid in range(10):
        N, edges, s, t = big_graph(pid)
        flow, side, resid = push_relabel(N, edges, s, t)
        assert check_certificate(N, edges, s, t, flow, side, resid), ("big", pid)
        h = fold_result(FNV_OFF, flow, side)
        row = [pid, flow & 0xFFFFFFFF, flow >> 32, h & 0xFFFFFFFF, h >> 32, sum(side)]
        big += row
        for v in row[1:]:
            golden = fnv_word(golden, v)
    print("// Generated by tools/maxflow_ref.py. Do not edit by hand.")
    print("const u32 kTableCases = %d;" % ncases)
    emit_array("kTable", table)
    print("const u32 kMidCases = %d;" % nmid)
    emit_array("kMid", mid)
    bh = battery_digest()
    print("const u32 kBatteryCases = %d;" % BATTERY_CASES)
    print("const u64 kBatteryDigest = %dull;" % bh)
    emit_array("kGroupDigest", gd)
    emit_array("kGroupCases", gcases)
    emit_array("kBig", big, per=6)
    print("const u64 kGolden = %dull;" % golden)


if __name__ == "__main__":
    main()
