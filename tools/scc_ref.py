#!/usr/bin/env python3
"""Independent reference for core/scc: prints the data tables of tests/scc/scc_tests.cpp as C++ source.

The C++ method is iterative Tarjan with a renumbering pass. This reference uses neither:
- Small graphs (flat table, digest groups, battery, mid graphs) use the transitive closure: reach[v] is a bit mask of the nodes reachable from
  v (Warshall, bit operations); u and v share a component exactly when each is in the other's mask.
- Max-size graphs use Kosaraju (finishing order of a depth-first search on the graph, then a sweep over the reversed graph), written as
  iterative loops over Python lists. Mid graphs are solved by both and compared.
Components are numbered by their smallest member node (the first node in node order starts the next number).
Output is deterministic.
"""

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


def fold_result(h, count, comp):
    h = fnv_word(h, count)
    for c in comp:
        h = fnv_word(h, c)
    return h


def emit_array(name, vals, per=16):
    print("const u32 %s[%d] = {" % (name, max(1, len(vals))))
    if not vals:
        vals = [0]
    for i in range(0, len(vals), per):
        print("    " + ", ".join("%du" % v for v in vals[i:i + per]) + ",")
    print("};")


def closure_scc(n, edges):
    reach = [1 << v for v in range(n)]
    for u, v in edges:
        reach[u] |= 1 << v
    for k in range(n):
        rk = reach[k]
        bit = 1 << k
        for i in range(n):
            if reach[i] & bit:
                reach[i] |= rk
    comp = [-1] * n
    count = 0
    for v in range(n):
        if comp[v] >= 0:
            continue
        for w in range(v, n):
            if comp[w] < 0 and (reach[v] >> w) & 1 and (reach[w] >> v) & 1:
                comp[w] = count
        count += 1
    return count, comp


def kosaraju_scc(n, edges):
    fwd = [[] for _ in range(n)]
    rev = [[] for _ in range(n)]
    for u, v in edges:
        fwd[u].append(v)
        rev[v].append(u)
    seen = [False] * n
    order = []
    for s in range(n):
        if seen[s]:
            continue
        seen[s] = True
        st = [(s, 0)]
        while st:
            u, i = st.pop()
            if i < len(fwd[u]):
                st.append((u, i + 1))
                w = fwd[u][i]
                if not seen[w]:
                    seen[w] = True
                    st.append((w, 0))
            else:
                order.append(u)
    label = [-1] * n
    k = 0
    for s in reversed(order):
        if label[s] >= 0:
            continue
        label[s] = k
        st = [s]
        while st:
            u = st.pop()
            for w in rev[u]:
                if label[w] < 0:
                    label[w] = k
                    st.append(w)
        k += 1
    # canonical numbering: by first appearance in node order
    mp = {}
    comp = []
    for v in range(n):
        if label[v] not in mp:
            mp[label[v]] = len(mp)
        comp.append(mp[label[v]])
    return len(mp), comp


class Lcg:
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return self.s >> 33

    def rnd(self, k):
        return self.next() % k


def pairs_with_loops(n):
    return [(u, v) for u in range(n) for v in range(n)]


def pairs_no_loops(n):
    return [(u, v) for u in range(n) for v in range(n) if u != v]


def subset_digest(n, pairs):
    h = FNV_OFF
    cases = 0
    k = len(pairs)
    for mask in range(1 << k):
        edges = [pairs[i] for i in range(k) if (mask >> i) & 1]
        count, comp = closure_scc(n, edges)
        h = fold_result(h, count, comp)
        cases += 1
    return h, cases


def sequence_digest(n, maxlen):
    pairs = pairs_with_loops(n)
    h = FNV_OFF
    cases = 0
    seqs = [[]]
    for length in range(maxlen + 1):
        for seq in seqs:
            count, comp = closure_scc(n, [pairs[i] for i in seq])
            h = fold_result(h, count, comp)
            cases += 1
        if length < maxlen:
            seqs = [s + [i] for s in seqs for i in range(len(pairs))]
    return h, cases


SAMPLE_STRIDE = 331


def strided_digest(n):
    """Looped graphs on n nodes: every SAMPLE_STRIDE-th edge set of the 2^(n*n) edge sets, mask order, pair order (u, v) with bit u * n + v."""
    pairs = pairs_with_loops(n)
    k = len(pairs)
    h = FNV_OFF
    cases = 0
    for mask in range(0, 1 << k, SAMPLE_STRIDE):
        edges = [pairs[i] for i in range(k) if (mask >> i) & 1]
        count, comp = closure_scc(n, edges)
        h = fold_result(h, count, comp)
        cases += 1
    return h, cases


def battery_digest(cases):
    g = Lcg(4242)
    h = FNV_OFF
    for _ in range(cases):
        n = 1 + g.rnd(14)
        m = g.rnd(41)
        edges = [(g.rnd(n), g.rnd(n)) for _ in range(m)]
        count, comp = closure_scc(n, edges)
        h = fold_result(h, count, comp)
    return h


def shuffled_perm(n, g):
    p = list(range(n))
    for i in range(n - 1, 0, -1):
        j = g.rnd(i + 1)
        p[i], p[j] = p[j], p[i]
    return p


def big_graph(pid):
    """Mirror of big_graph in the test: 65536 nodes, at most 262144 edges."""
    g = Lcg(3000 + pid)
    N, M = N_BIG, M_BIG
    e = []
    if pid == 0:  # one huge cycle in node order, every edge four times
        for _ in range(4):
            for i in range(N):
                e.append((i, (i + 1) % N))
    elif pid == 1:  # one huge cycle in a shuffled order plus random chords
        p = shuffled_perm(N, g)
        for i in range(N):
            e.append((p[i], p[(i + 1) % N]))
        for _ in range(M - N):
            a = g.rnd(N)
            b = g.rnd(N)
            e.append((p[a], p[b]))
    elif pid == 2:  # long path, forward
        for i in range(N - 1):
            e.append((i, i + 1))
    elif pid == 3:  # long path, backward
        for i in range(N - 1):
            e.append((i + 1, i))
    elif pid == 4:  # long path closed by one back edge
        for i in range(N - 1):
            e.append((i, i + 1))
        e.append((N - 1, 0))
    elif pid == 5:  # dense core: all 512 * 512 pairs, loops included
        for u in range(512):
            for v in range(512):
                e.append((u, v))
    elif pid == 6:  # 21845 triangles chained forward, node 65535 alone
        for c in range(21845):
            a = 3 * c
            e.append((a, a + 1))
            e.append((a + 1, a + 2))
            e.append((a + 2, a))
            if c + 1 < 21845:
                e.append((a, a + 3))
    elif pid == 7:  # random forward edges only
        for _ in range(M):
            a = g.rnd(N - 1)
            b = a + 1 + g.rnd(N - 1 - a)
            e.append((a, b))
    elif pid == 8:  # no edges
        pass
    elif pid == 9:  # self-loops only
        for i in range(N):
            e.append((i, i))
    elif pid == 10:  # two shuffled cycles of 32768 nodes, one bridge, random edges inside each half
        p = shuffled_perm(N, g)
        h = N // 2
        for i in range(h):
            e.append((p[i], p[(i + 1) % h]))
        for i in range(h):
            e.append((p[h + i], p[h + (i + 1) % h]))
        e.append((p[0], p[h]))
        for _ in range(M - N - 1):
            side = g.rnd(2)
            a = g.rnd(h)
            b = g.rnd(h)
            e.append((p[side * h + a], p[side * h + b]))
    elif pid == 11:  # uniform random edges
        for _ in range(M):
            e.append((g.rnd(N), g.rnd(N)))
    elif pid == 13:  # layered DAG: 256 layers of 256 nodes, random edges between consecutive layers
        for _ in range(M):
            l = g.rnd(255)
            e.append((l * 256 + g.rnd(256), (l + 1) * 256 + g.rnd(256)))
    elif pid == 12:  # four dense cores of 256 interleaved nodes (node mod 4), all pairs inside each
        for c in range(4):
            nodes = [c + 4 * k for k in range(256)]
            for u in nodes:
                for v in nodes:
                    e.append((u, v))
    return N, e


BIG_COUNTS = {0: 1, 2: N_BIG, 3: N_BIG, 4: 1, 5: N_BIG - 511, 6: 21846, 7: N_BIG, 8: N_BIG, 9: N_BIG, 10: 2, 12: N_BIG - 1020, 13: N_BIG}


def main():
    import random
    out_table = []
    for n in (1, 2, 3):
        pairs = pairs_with_loops(n)
        for mask in range(1 << len(pairs)):
            edges = [pairs[i] for i in range(len(pairs)) if (mask >> i) & 1]
            count, comp = closure_scc(n, edges)
            word = count
            for i, c in enumerate(comp):
                word |= c << (4 + 2 * i)
            out_table.append(word)
    gd = []
    gcases = []
    for h, c in (subset_digest(4, pairs_with_loops(4)), subset_digest(5, pairs_no_loops(5)), sequence_digest(3, 4), sequence_digest(2, 6), strided_digest(5)):
        gd += [h & 0xFFFFFFFF, h >> 32]
        gcases.append(c)
    battery_cases = 20000
    bd = battery_digest(battery_cases)
    rng = random.Random(20260604)
    mid = []
    nmid = 100
    for _ in range(nmid):
        n = rng.randint(1, 40)
        m = rng.randint(0, 150)
        edges = [(rng.randrange(n), rng.randrange(n)) for _ in range(m)]
        c1, comp1 = closure_scc(n, edges)
        c2, comp2 = kosaraju_scc(n, edges)
        assert (c1, comp1) == (c2, comp2)
        mid += [n, m] + [u for u, _ in edges] + [v for _, v in edges] + [c1] + comp1
    golden = FNV_OFF
    for v in gd:
        golden = fnv_word(golden, v)
    for v in mid:
        golden = fnv_word(golden, v)
    big = []
    for pid in range(14):
        N, edges = big_graph(pid)
        assert len(edges) <= M_BIG
        count, comp = kosaraju_scc(N, edges)
        if pid in BIG_COUNTS:
            assert count == BIG_COUNTS[pid], (pid, count)
        h = fold_result(FNV_OFF, count, comp)
        row = [pid, len(edges), count, h & 0xFFFFFFFF, h >> 32]
        big += row
        for v in row[1:]:
            golden = fnv_word(golden, v)
    print("// Generated by tools/scc_ref.py. Do not edit by hand.")
    print("const u32 kTableCases = %d;" % len(out_table))
    emit_array("kTable", out_table)
    print("const u32 kMidCases = %d;" % nmid)
    emit_array("kMid", mid)
    emit_array("kGroupDigest", gd)
    emit_array("kGroupCases", gcases)
    print("const u32 kBatteryCases = %d;" % battery_cases)
    print("const u64 kBatteryDigest = %dull;" % bd)
    emit_array("kBig", big, per=5)
    print("const u64 kGolden = %dull;" % golden)


if __name__ == "__main__":
    main()
