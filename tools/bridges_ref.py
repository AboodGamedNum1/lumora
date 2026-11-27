#!/usr/bin/env python3
"""Independent reference for core/bridges: prints the data tables of tests/bridges/bridges_tests.cpp as C++ source.

The C++ method is a depth-first search with low-points. This reference uses none of it:
- Small graphs (flat table, digest groups, battery, mid graphs) use brute-force removal: an edge is a bridge when the graph without that edge
  has more connected components; a node is an articulation point when the graph without the node has more connected components among the
  remaining nodes than the whole graph had. Components are counted by flood fill on bit masks.
- Large graphs use two other methods. Bridges: a BFS spanning forest, then every non-tree edge covers the tree path between its ends (path
  covering with a union-find over tree nodes); the tree edges left uncovered are the bridges. Articulation points: Schmidt's chain
  decomposition of the simple graph (loops dropped, parallel edges merged) over a DFS tree. Both are checked against brute-force removal on
  random small and mid graphs when the script runs with --selfcheck.
Output is deterministic.
"""
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
N_BIG = 65536
M_BIG = 262144


def fnv_word(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & M64
    return h


P4 = pow(FNV_PRIME, 4, 1 << 64)


def fold_result(h, br, cut):
    """FNV-1a over the little-endian bytes of the words: bridge count, cut count, the bridge flags, the cut flags."""
    h = fnv_word(h, sum(br))
    h = fnv_word(h, sum(cut))
    for b in br:  # flags are 0 or 1: the three zero bytes after the first are three more multiplications by the prime
        h = ((h ^ b) * P4) & M64
    for c in cut:
        h = ((h ^ c) * P4) & M64
    return h


def emit_array(name, vals, per=16):
    print("const u32 %s[%d] = {" % (name, max(1, len(vals))))
    if not vals:
        vals = [0]
    for i in range(0, len(vals), per):
        print("    " + ", ".join("%du" % v for v in vals[i:i + per]) + ",")
    print("};")


def comps(n, adj, alive):
    seen = 0
    c = 0
    for s in range(n):
        if (alive >> s) & 1 and not (seen >> s) & 1:
            c += 1
            fr = 1 << s
            seen |= fr
            while fr:
                nxt = 0
                f = fr
                while f:
                    b = f & -f
                    f ^= b
                    nxt |= adj[b.bit_length() - 1]
                nxt &= alive & ~seen
                seen |= nxt
                fr = nxt
    return c


def brute(n, edges):
    full = (1 << n) - 1

    def adjof(skip):
        adj = [0] * n
        for i, (u, v) in enumerate(edges):
            if i != skip and u != v:
                adj[u] |= 1 << v
                adj[v] |= 1 << u
        return adj

    adj0 = adjof(-1)
    base = comps(n, adj0, full)
    br = []
    distinct = len(set((min(u, v), max(u, v)) for u, v in edges)) == len(edges)
    for i, (u, v) in enumerate(edges):
        if u == v:
            br.append(0)
        elif distinct:  # no parallel twin: removing the edge clears the two adjacency bits
            a = list(adj0)
            a[u] &= ~(1 << v)
            a[v] &= ~(1 << u)
            br.append(1 if comps(n, a, full) > base else 0)
        else:
            br.append(1 if comps(n, adjof(i), full) > base else 0)
    cut = [1 if comps(n, adj0, full & ~(1 << v)) > base else 0 for v in range(n)]
    return br, cut


def bridges_cover(n, edges):
    adj = [[] for _ in range(n)]
    for i, (u, v) in enumerate(edges):
        if u != v:
            adj[u].append((v, i))
            adj[v].append((u, i))
    parent = [-1] * n
    pedge = [-1] * n
    depth = [0] * n
    seen = [False] * n
    is_tree = [False] * len(edges)
    for s in range(n):
        if seen[s]:
            continue
        seen[s] = True
        q = [s]
        qi = 0
        while qi < len(q):
            u = q[qi]
            qi += 1
            for w, i in adj[u]:
                if not seen[w]:
                    seen[w] = True
                    parent[w] = u
                    pedge[w] = i
                    depth[w] = depth[u] + 1
                    is_tree[i] = True
                    q.append(w)
    up = list(range(n))

    def find(x):
        r = x
        while up[r] != r:
            r = up[r]
        while up[x] != r:
            up[x], x = r, up[x]
        return r

    covered = [False] * n
    for i, (u, v) in enumerate(edges):
        if u == v or is_tree[i]:
            continue
        a, b = find(u), find(v)
        while a != b:
            if depth[a] < depth[b]:
                a, b = b, a
            covered[a] = True
            up[a] = parent[a]
            a = find(a)
    br = [0] * len(edges)
    for v in range(n):
        if parent[v] != -1 and not covered[v]:
            br[pedge[v]] = 1
    return br


def cuts_chain(n, edges):
    nb = [set() for _ in range(n)]
    for u, v in edges:
        if u != v:
            nb[u].add(v)
            nb[v].add(u)
    adj = [sorted(s) for s in nb]
    pre = [-1] * n
    parent = [-1] * n
    order = []
    kids = [0] * n
    root_of = [-1] * n
    for s in range(n):
        if pre[s] != -1:
            continue
        pre[s] = len(order)
        order.append(s)
        root_of[s] = s
        st = [(s, 0)]
        while st:
            u, i = st.pop()
            if i < len(adj[u]):
                st.append((u, i + 1))
                w = adj[u][i]
                if pre[w] == -1:
                    pre[w] = len(order)
                    order.append(w)
                    parent[w] = u
                    root_of[w] = s
                    kids[u] += 1
                    st.append((w, 0))
    cut = [0] * n
    visited = [False] * n
    in_chain = [False] * n  # in_chain[x]: the tree edge (x, parent[x]) lies on some chain
    first_chain_done = set()
    for v in order:
        for w in adj[v]:
            if pre[w] > pre[v] and parent[w] != v:  # back edge from the ancestor v down to w
                r = root_of[v]
                first = r not in first_chain_done
                first_chain_done.add(r)
                visited[v] = True
                cur = w
                while not visited[cur]:
                    visited[cur] = True
                    in_chain[cur] = True
                    cur = parent[cur]
                if cur == v and not first and v != r:
                    cut[v] = 1
    for x in range(n):
        if parent[x] != -1 and not in_chain[x]:  # a tree edge on no chain is a bridge of the simple graph: its ends with degree >= 2 are cuts
            for y in (x, parent[x]):
                if len(adj[y]) >= 2:
                    cut[y] = 1
    for s in range(n):
        if parent[s] == -1 and kids[s] >= 2:
            cut[s] = 1
    return cut


def large(n, edges):
    return bridges_cover(n, edges), cuts_chain(n, edges)


class Lcg:
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def shuffled_perm(n, g):
    p = list(range(n))
    for i in range(n - 1, 0, -1):
        j = g.rnd(i + 1)
        p[i], p[j] = p[j], p[i]
    return p


def pairs_up(n, loops):
    return [(u, v) for u in range(n) for v in range(u, n) if loops or u != v]


def subset_digest(n, loops):
    pairs = pairs_up(n, loops)
    k = len(pairs)
    h = FNV_OFF
    for mask in range(1 << k):
        edges = [pairs[i] for i in range(k) if (mask >> i) & 1]
        br, cut = brute(n, edges)
        h = fold_result(h, br, cut)
    return h, 1 << k


def sequence_digest(n, maxlen):
    pairs = [(u, v) for u in range(n) for v in range(n)]
    h = FNV_OFF
    cases = 0
    seqs = [[]]
    for length in range(maxlen + 1):
        for seq in seqs:
            br, cut = brute(n, [pairs[i] for i in seq])
            h = fold_result(h, br, cut)
            cases += 1
        if length < maxlen:
            seqs = [s + [i] for s in seqs for i in range(len(pairs))]
    return h, cases


def multigraph4_digest():
    n = 4
    pairs = [(u, v) for u in range(n) for v in range(u + 1, n)]  # 6 non-loop pairs, 0..2 copies each
    h = FNV_OFF
    cases = 0
    for code in range(3 ** 6):
        copies = []
        c = code
        for _ in pairs:
            copies.append(c % 3)
            c //= 3
        for lmask in range(16):
            edges = []
            for (u, v), k in zip(pairs, copies):
                edges += [(u, v)] * k
            edges += [(i, i) for i in range(4) if (lmask >> i) & 1]
            br, cut = brute(n, edges)
            h = fold_result(h, br, cut)
            cases += 1
    return h, cases


def battery_graph(g):
    n = 1 + g.rnd(14)
    m = g.rnd(41)
    edges = []
    for _ in range(m):
        u = g.rnd(n)
        v = g.rnd(n)
        edges.append((u, v))
    return n, edges


BATTERY_CASES = 20000


def battery_digest():
    g = Lcg(5151)
    h = FNV_OFF
    for _ in range(BATTERY_CASES):
        n, edges = battery_graph(g)
        br, cut = brute(n, edges)
        h = fold_result(h, br, cut)
    return h


def big_graph(pid):
    """Mirror of big_graph in the test. Returns (nodes, edge list)."""
    g = Lcg(5000 + pid)
    N = N_BIG
    e = []
    if pid == 0:  # path of 65535 edges
        n = N
        e = [(i, i + 1) for i in range(N - 1)]
    elif pid == 1:  # one cycle of 65536 nodes
        n = N
        e = [(i, (i + 1) % N) for i in range(N)]
    elif pid == 2:  # star, centre 0
        n = N
        e = [(0, i) for i in range(1, N)]
    elif pid == 3:  # 8192 cycles of 8 nodes, consecutive cycles joined by a bridge
        n = N
        for c in range(8192):
            for j in range(8):
                e.append((8 * c + j, 8 * c + (j + 1) % 8))
            if c + 1 < 8192:
                e.append((8 * c + 7, 8 * c + 8))
    elif pid == 4:  # doubled path
        n = N
        for i in range(N - 1):
            e += [(i, i + 1), (i + 1, i)]
    elif pid == 5:  # path with every edge four times
        n = N
        for i in range(N - 1):
            e += [(i, i + 1), (i + 1, i), (i, i + 1), (i + 1, i)]
    elif pid == 6:  # random tree
        n = N
        for v in range(1, N):
            e.append((g.rnd(v), v))
    elif pid == 7:  # no edges
        n = N
    elif pid == 8:  # one node, 262144 loops
        n = 1
        e = [(0, 0)] * M_BIG
    elif pid == 9:  # one loop on every node
        n = N
        e = [(i, i) for i in range(N)]
    elif pid == 10:  # uniform random edges
        n = N
        for _ in range(M_BIG):
            u = g.rnd(N)
            v = g.rnd(N)
            e.append((u, v))
    elif pid == 11:  # 1024 blocks of 64 nodes (cycle plus 192 random chords), consecutive blocks share one node
        n = 1024 * 63 + 1
        for c in range(1024):
            base = 63 * c
            for j in range(64):
                e.append((base + j, base + (j + 1) % 64))
            for _ in range(192):
                e.append((base + g.rnd(64), base + g.rnd(64)))
    elif pid == 12:  # pid 3 with shuffled labels, shuffled edge order and flipped ends
        n = N
        p = shuffled_perm(N, g)
        base = []
        for c in range(8192):
            for j in range(8):
                base.append((8 * c + j, 8 * c + (j + 1) % 8))
            if c + 1 < 8192:
                base.append((8 * c + 7, 8 * c + 8))
        idx = shuffled_perm(len(base), g)
        for i in idx:
            u, v = base[i]
            if g.rnd(2):
                u, v = v, u
            e.append((p[u], p[v]))
    elif pid == 13:  # 256 x 256 grid
        n = N
        for r in range(256):
            for c in range(256):
                if c + 1 < 256:
                    e.append((256 * r + c, 256 * r + c + 1))
                if r + 1 < 256:
                    e.append((256 * r + c, 256 * (r + 1) + c))
    return n, e


def closed_form(pid, n, e):
    if pid == 0:
        return 65535, 65534
    if pid == 1:
        return 0, 0
    if pid == 2:
        return 65535, 1
    if pid in (3, 12):
        return 8191, 16382
    if pid in (4, 5):
        return 0, 65534
    if pid == 6:
        deg = [0] * n
        for u, v in e:
            deg[u] += 1
            deg[v] += 1
        return n - 1, sum(1 for d in deg if d >= 2)
    if pid in (7, 8, 9, 13):
        return 0, 0
    if pid == 11:
        return None, 1023
    return None, None


def selfcheck():
    g = Lcg(31337)
    bad = 0
    for it in range(30000):
        n = 1 + g.rnd(12)
        m = g.rnd(30)
        edges = [(g.rnd(n), g.rnd(n)) for _ in range(m)]
        if brute(n, edges) != large(n, edges):
            bad += 1
            if bad < 4:
                print("MISMATCH", n, edges, file=sys.stderr)
    print("selfcheck mismatches:", bad, file=sys.stderr)
    return bad == 0


def main():
    import random
    if "--selfcheck" in sys.argv:
        sys.exit(0 if selfcheck() else 1)
    table = []
    for n in (1, 2, 3):
        pairs = pairs_up(n, True)
        for mask in range(1 << len(pairs)):
            edges = [pairs[i] for i in range(len(pairs)) if (mask >> i) & 1]
            br, cut = brute(n, edges)
            word = 0
            for i, b in enumerate(br):
                word |= b << i
            for v, c in enumerate(cut):
                word |= c << (8 + v)
            table.append(word)
    gd = []
    gcases = []
    groups = [lambda: subset_digest(4, True), lambda: subset_digest(5, True), lambda: subset_digest(6, True),
              lambda: subset_digest(7, False), lambda: sequence_digest(3, 5), lambda: sequence_digest(2, 6), multigraph4_digest,
              lambda: (battery_digest(), BATTERY_CASES)]
    results = []
    for gi, fn in enumerate(groups):
        cache = None
        for a in sys.argv:
            if a.startswith("--cache="):
                cache = a[8:]
        if cache:
            try:
                h, c = [int(x) for x in open("%s/g%d.txt" % (cache, gi)).read().split()]
            except OSError:
                h, c = fn()
                open("%s/g%d.txt" % (cache, gi), "w").write("%d %d\n" % (h, c))
        else:
            h, c = fn()
        results.append((h, c))
    for h, c in results[:7]:
        gd += [h & 0xFFFFFFFF, h >> 32]
        gcases.append(c)
    bd = results[7][0]
    rng = random.Random(20260611)
    mid = []
    nmid = 100
    for _ in range(nmid):
        n = rng.randint(1, 40)
        m = rng.randint(0, 150)
        edges = [(rng.randrange(n), rng.randrange(n)) for _ in range(m)]
        b1, c1 = brute(n, edges)
        assert (b1, c1) == large(n, edges)
        mid += [n, m] + [u for u, _ in edges] + [v for _, v in edges] + [sum(b1), sum(c1)] + b1 + c1
    golden = FNV_OFF
    for v in gd:
        golden = fnv_word(golden, v)
    for v in mid:
        golden = fnv_word(golden, v)
    big = []
    for pid in range(14):
        n, edges = big_graph(pid)
        assert len(edges) <= M_BIG and n <= N_BIG
        br, cut = large(n, edges)
        nb, nc = closed_form(pid, n, edges)
        if nb is not None:
            assert sum(br) == nb, (pid, sum(br), nb)
        if nc is not None:
            assert sum(cut) == nc, (pid, sum(cut), nc)
        h = fold_result(FNV_OFF, br, cut)
        row = [pid, n, len(edges), sum(br), sum(cut), h & 0xFFFFFFFF, h >> 32]
        big += row
        for v in row[1:]:
            golden = fnv_word(golden, v)
    print("// Generated by tools/bridges_ref.py. Do not edit by hand.")
    print("const u32 kTableCases = %d;" % len(table))
    emit_array("kTable", table)
    print("const u32 kMidCases = %d;" % nmid)
    emit_array("kMid", mid)
    emit_array("kGroupDigest", gd)
    emit_array("kGroupCases", gcases)
    print("const u32 kBatteryCases = %d;" % BATTERY_CASES)
    print("const u64 kBatteryDigest = %dull;" % bd)
    emit_array("kBig", big, per=7)
    print("const u64 kGolden = %dull;" % golden)


if __name__ == "__main__":
    main()
