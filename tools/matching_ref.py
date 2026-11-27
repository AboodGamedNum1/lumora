#!/usr/bin/env python3
"""Reference for core/matching. Prints data.inc (tables and digests) for tests/matching/matching_tests.cpp.

Small graphs: brute force over ALL matchings (every set of edges sharing no vertex), kept as (left mask, right mask) states; the size is the largest
state, and a vertex is status 2 when it is in every maximum state, 0 when it has no edge, else 1.
Large graphs: size by augmenting paths in phases (Kuhn with a visited stamp per phase), statuses by alternating-path reachability from the free
vertices (Dulmage-Mendelsohn). `python3 tools/matching_ref.py --selfcheck` compares the two methods on 30000 random graphs.
"""
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
SIDE = 65536
M_BIG = 262144
P4 = pow(FNV_PRIME, 4, 1 << 64)


def fnv_word(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & M64
    return h


def fold_result(h, size, cl, cr):
    """FNV-1a over little-endian bytes of the words: size, then the left statuses, then the right statuses."""
    h = fnv_word(h, size)
    for c in cl:  # statuses are below 256: the three zero bytes are three more multiplications
        h = ((h ^ c) * P4) & M64
    for c in cr:
        h = ((h ^ c) * P4) & M64
    return h


def emit_array(name, vals, per=16):
    print("const u32 %s[%d] = {" % (name, max(1, len(vals))))
    if not vals:
        vals = [0]
    for i in range(0, len(vals), per):
        print("    " + ", ".join("%du" % v for v in vals[i:i + per]) + ",")
    print("};")


def brute(l, r, edges):
    nb = [0] * l
    for u, v in edges:
        nb[u] |= 1 << v
    states = {(0, 0)}
    for u in range(l):
        new = set(states)
        for lm, rm in states:
            free = nb[u] & ~rm
            while free:
                b = free & -free
                free ^= b
                new.add((lm | (1 << u), rm | b))
        states = new
    best = max(bin(lm).count("1") for lm, rm in states)
    top = [(lm, rm) for lm, rm in states if bin(lm).count("1") == best]
    andl = (1 << l) - 1
    andr = (1 << r) - 1
    for lm, rm in top:
        andl &= lm
        andr &= rm
    degl = [1 if nb[u] else 0 for u in range(l)]
    degr = [0] * r
    for u, v in edges:
        degr[v] = 1
    cl = [0 if not degl[u] else (2 if (andl >> u) & 1 else 1) for u in range(l)]
    cr = [0 if not degr[w] else (2 if (andr >> w) & 1 else 1) for w in range(r)]
    return best, cl, cr


def large(l, r, edges):
    adj = [[] for _ in range(l)]
    radj = [[] for _ in range(r)]
    for u, v in edges:
        adj[u].append(v)
        radj[v].append(u)
    ml = [-1] * l
    mr = [-1] * r
    for u in range(l):  # greedy start, in edge-list order of each node
        for w in adj[u]:
            if mr[w] < 0:
                ml[u] = w
                mr[w] = u
                break
    via = [-1] * r
    while True:  # phases: one visited stamp per phase, a search from every free left node
        seen = [False] * r
        any_aug = False
        for s in range(l):
            if ml[s] >= 0:
                continue
            stack = [s]
            pos = {s: 0}
            end = -1
            while stack and end < 0:
                u = stack[-1]
                i = pos[u]
                if i < len(adj[u]):
                    pos[u] = i + 1
                    w = adj[u][i]
                    if seen[w]:
                        continue
                    seen[w] = True
                    via[w] = u
                    if mr[w] < 0:
                        end = w
                    else:
                        y = mr[w]
                        pos[y] = 0
                        stack.append(y)
                else:
                    stack.pop()
            if end >= 0:
                w = end
                while w >= 0:
                    u = via[w]
                    prev = ml[u]
                    ml[u] = w
                    mr[w] = u
                    w = prev
                any_aug = True
        if not any_aug:
            break
    size = sum(1 for x in ml if x >= 0)
    sl = [0] * l
    sr = [0] * r
    q = [u for u in range(l) if ml[u] < 0]
    for u in q:
        sl[u] = 1
    for u in q:
        for w in adj[u]:
            y = mr[w]
            if y >= 0 and not sl[y]:
                sl[y] = 1
                q.append(y)
    q = [w for w in range(r) if mr[w] < 0]
    for w in q:
        sr[w] = 1
    for w in q:
        for u in radj[w]:
            x = ml[u]
            if x >= 0 and not sr[x]:
                sr[x] = 1
                q.append(x)
    cl = [0 if not adj[u] else (1 if sl[u] else 2) for u in range(l)]
    cr = [0 if not radj[w] else (1 if sr[w] else 2) for w in range(r)]
    return size, cl, cr


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


def pairs_all(l, r):
    return [(u, v) for u in range(l) for v in range(r)]


def subset_digest(sizes):
    """Every simple bipartite graph (edge subset of the l x r pairs, pairs in row order, bit i = pair i) for each (l, r) in sizes, in order."""
    h = FNV_OFF
    cases = 0
    for l, r in sizes:
        pairs = pairs_all(l, r)
        k = len(pairs)
        for mask in range(1 << k):
            size, cl, cr = brute(l, r, [pairs[i] for i in range(k) if (mask >> i) & 1])
            h = fold_result(h, size, cl, cr)
            cases += 1
    return h, cases


def sequence_digest(l, r, maxlen):
    """Every ordered edge list of length 0..maxlen over the l * r pairs (lexicographic per length): parallels and any order."""
    pairs = pairs_all(l, r)
    h = FNV_OFF
    cases = 0
    seqs = [[]]
    for length in range(maxlen + 1):
        for seq in seqs:
            size, cl, cr = brute(l, r, [pairs[i] for i in seq])
            h = fold_result(h, size, cl, cr)
            cases += 1
        if length < maxlen:
            seqs = [t + [i] for t in seqs for i in range(len(pairs))]
    return h, cases


def lists_digest(spec):
    h = FNV_OFF
    cases = 0
    for l, r, maxlen in spec:
        pairs = pairs_all(l, r)
        seqs = [[]]
        for length in range(maxlen + 1):
            for seq in seqs:
                size, cl, cr = brute(l, r, [pairs[i] for i in seq])
                h = fold_result(h, size, cl, cr)
                cases += 1
            if length < maxlen:
                seqs = [t + [i] for t in seqs for i in range(len(pairs))]
    return h, cases


BATTERY_CASES = 20000


def battery_graph(g):
    l = 1 + g.rnd(10)
    r = 1 + g.rnd(10)
    m = g.rnd(41)
    e = []
    for _ in range(m):
        u = g.rnd(l)
        v = g.rnd(r)
        e.append((u, v))
    return l, r, e


def battery_digest():
    g = Lcg(7171)
    h = FNV_OFF
    for _ in range(BATTERY_CASES):
        l, r, e = battery_graph(g)
        size, cl, cr = brute(l, r, e)
        h = fold_result(h, size, cl, cr)
    return h, BATTERY_CASES


def big_graph(pid):
    """Mirror of big_graph in the test. Returns (left, right, edge list)."""
    g = Lcg(6000 + pid)
    N = SIDE
    e = []
    L = R = N
    if pid == 0:  # perfect matching, identity
        e = [(i, i) for i in range(N)]
    elif pid == 1:  # shuffled perfect matching plus random edges up to 262144
        p = shuffled_perm(N, g)
        e = [(i, p[i]) for i in range(N)]
        while len(e) < M_BIG:
            u = g.rnd(N)
            v = g.rnd(N)
            e.append((u, v))
    elif pid == 2:  # left star: left 0 to every right node
        e = [(0, i) for i in range(N)]
    elif pid == 3:  # right star: every left node to right 0
        e = [(i, 0) for i in range(N)]
    elif pid == 4:  # 1024 complete blocks of 16 + 16
        L = R = 16384
        for b in range(1024):
            for a in range(16):
                for c in range(16):
                    e.append((16 * b + a, 16 * b + c))
    elif pid == 5:  # 8192 complete blocks of 8 left + 4 right
        L = N
        R = 32768
        for b in range(8192):
            for a in range(8):
                for c in range(4):
                    e.append((8 * b + a, 4 * b + c))
    elif pid == 6:  # zig-zag path: left i - right i, left i+1 - right i
        for i in range(N):
            e.append((i, i))
            if i + 1 < N:
                e.append((i + 1, i))
    elif pid == 7:  # one augmenting path of length 2N - 1 after a greedy start
        for i in range(N - 1):
            e.append((i, i + 1))
        for i in range(N):
            e.append((i, i))
    elif pid == 8:  # no edges
        pass
    elif pid == 9:  # no left nodes
        L = 0
    elif pid == 10:  # one right node
        R = 1
        e = [(i, 0) for i in range(N)]
    elif pid == 11:  # perfect matching, every edge four times
        for i in range(N):
            e += [(i, i)] * 4
    elif pid == 12:  # uniform random edges
        for _ in range(M_BIG):
            u = g.rnd(N)
            v = g.rnd(N)
            e.append((u, v))
    elif pid == 13:  # sparse unbalanced random
        R = 40000
        for _ in range(100000):
            u = g.rnd(N)
            v = g.rnd(R)
            e.append((u, v))
    elif pid == 14:  # few left nodes, many right nodes
        L = 512
        for _ in range(M_BIG):
            u = g.rnd(L)
            v = g.rnd(N)
            e.append((u, v))
    return L, R, e


def closed_form(pid):
    """(size, number of status-2 left nodes, number of status-2 right nodes) where known, else None."""
    table = {
        0: (65536, 65536, 65536),  # unique perfect matching
        2: (1, 1, 0),  # left star: the one left node is always matched
        3: (1, 0, 1),  # right star: the one right node is always matched
        4: (16384, 16384, 16384),  # complete 16 + 16 blocks: perfect, every node always matched
        5: (32768, 0, 32768),  # complete 8 + 4 blocks: every right node always matched, 4 of 8 left nodes per block
        6: (65536, 65536, 65536),  # zig-zag path with a unique perfect matching
        7: (65536, 65536, 65536),  # same, one long augmenting path
        8: (0, 0, 0),
        9: (0, 0, 0),
        10: (1, 0, 1),
        11: (65536, 65536, 65536),
    }
    return table.get(pid)


def selfcheck():
    g = Lcg(31337)
    bad = 0
    for it in range(30000):
        l = g.rnd(8)
        r = g.rnd(8)
        m = g.rnd(20) if l and r else 0
        edges = [(g.rnd(l), g.rnd(r)) for _ in range(m)]
        if brute(l, r, edges) != large(l, r, edges):
            bad += 1
            if bad < 4:
                print("MISMATCH", l, r, edges, file=sys.stderr)
    print("selfcheck mismatches:", bad, file=sys.stderr)
    return bad == 0


def main():
    import random
    if "--selfcheck" in sys.argv:
        sys.exit(0 if selfcheck() else 1)
    cache = None
    for a in sys.argv:
        if a.startswith("--cache="):
            cache = a[8:]
    table = []
    for l in range(1, 4):
        for r in range(1, 4):
            pairs = pairs_all(l, r)
            for mask in range(1 << len(pairs)):
                size, cl, cr = brute(l, r, [pairs[i] for i in range(len(pairs)) if (mask >> i) & 1])
                word = size
                for v, c in enumerate(cl):
                    word |= c << (4 + 2 * v)
                for w, c in enumerate(cr):
                    word |= c << (10 + 2 * w)
                table.append(word)
    groups = [
        lambda: subset_digest([(l, r) for l in range(5) for r in range(5)]),
        lambda: subset_digest([(5, 4), (4, 5)]),
        lambda: subset_digest([(6, 3), (3, 6)]),
        lambda: lists_digest([(2, 2, 6), (2, 3, 5), (3, 2, 5), (3, 3, 4)]),
        battery_digest,
    ]
    results = []
    for gi, fn in enumerate(groups):
        if cache:
            try:
                h, c = [int(x) for x in open("%s/g%d.txt" % (cache, gi)).read().split()]
            except OSError:
                h, c = fn()
                open("%s/g%d.txt" % (cache, gi), "w").write("%d %d\n" % (h, c))
        else:
            h, c = fn()
        results.append((h, c))
    gd = []
    gcases = []
    for h, c in results[:4]:
        gd += [h & 0xFFFFFFFF, h >> 32]
        gcases.append(c)
    bd = results[4][0]
    rng = random.Random(20260612)
    mid = []
    nmid = 100
    for _ in range(nmid):
        l = rng.randint(0, 9)
        r = rng.randint(0, 9)
        m = rng.randint(0, 30) if l and r else 0
        edges = [(rng.randrange(l), rng.randrange(r)) for _ in range(m)]
        b1 = brute(l, r, edges)
        assert b1 == large(l, r, edges)
        mid += [l, r, m] + [u for u, _ in edges] + [v for _, v in edges] + [b1[0]] + b1[1] + b1[2]
    golden = FNV_OFF
    for v in gd + mid:
        golden = fnv_word(golden, v)
    big = []
    for pid in range(15):
        L, R, edges = big_graph(pid)
        assert len(edges) <= M_BIG and L <= SIDE and R <= SIDE
        size, cl, cr = large(L, R, edges)
        cf = closed_form(pid)
        if cf is not None:
            assert (size, cl.count(2), cr.count(2)) == cf, (pid, size, cl.count(2), cr.count(2), cf)
        h = fold_result(FNV_OFF, size, cl, cr)
        row = [pid, L, R, len(edges), size, h & 0xFFFFFFFF, h >> 32]
        big += row
        for v in row[1:]:
            golden = fnv_word(golden, v)
    print("// Generated by tools/matching_ref.py. Do not edit by hand.")
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
