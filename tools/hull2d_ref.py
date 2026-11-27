#!/usr/bin/env python3
"""Independent reference for core/hull2d: prints the data tables of tests/hull2d/hull2d_tests.cpp as C++ source.

The C++ method is Andrew's monotone chain on a heapsorted index array. This reference uses neither: small and mid inputs go through a Jarvis
march (gift wrapping) with exact Python integers; the 65536-point inputs go through a Graham scan (angle sort around the lowest point with
a cmp_to_key comparison). Both are cross-checked against each other where the Jarvis march is affordable, and the parabola and perimeter
inputs are also checked against closed forms. Duplicates are handled by grouping equal coordinates under their smallest index first.
Output is deterministic.
"""
import functools
import itertools
import random

L = (1 << 30) - 1
FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
N_BIG = 1 << 16


def fnv_word(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & M64
    return h


def cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def distinct(pts):
    first = {}
    for i, p in enumerate(pts):
        if p not in first:
            first[p] = i
    return first  # coordinates -> smallest index


def jarvis(pts):
    """Vertex indices, ccw from the lexicographically smallest point; collinear edge points excluded."""
    if not pts:
        return []
    first = distinct(pts)
    uniq = sorted(first)
    start = uniq[0]
    if len(uniq) == 1:
        return [first[start]]
    hull = [start]
    cur = start
    while True:
        nxt = uniq[1] if cur == uniq[0] else uniq[0]
        for q in uniq:
            if q == cur:
                continue
            c = cross(cur, nxt, q)
            d_q = (q[0] - cur[0]) ** 2 + (q[1] - cur[1]) ** 2
            d_n = (nxt[0] - cur[0]) ** 2 + (nxt[1] - cur[1]) ** 2
            if c < 0 or (c == 0 and d_q > d_n):
                nxt = q
        if nxt == start:
            break
        hull.append(nxt)
        cur = nxt
    return [first[p] for p in hull]


def graham(pts):
    first = distinct(pts)
    uniq = list(first)
    if len(uniq) == 1:
        return [first[uniq[0]]]
    uniq.sort()
    lo, hi = uniq[0], uniq[-1]
    if all(cross(lo, hi, q) == 0 for q in uniq):
        return [first[lo], first[hi]]
    piv = min(uniq, key=lambda p: (p[1], p[0]))
    rest = [p for p in uniq if p != piv]

    def cmp(a, b):
        c = cross(piv, a, b)
        if c != 0:
            return -1 if c > 0 else 1
        da = (a[0] - piv[0]) ** 2 + (a[1] - piv[1]) ** 2
        db = (b[0] - piv[0]) ** 2 + (b[1] - piv[1]) ** 2
        return -1 if da < db else (1 if da > db else 0)

    rest.sort(key=functools.cmp_to_key(cmp))
    j = len(rest) - 1
    while j > 0 and cross(piv, rest[j], rest[j - 1]) == 0:
        j -= 1
    rest = rest[:j] + [rest[-1]]  # on the last ray only the farthest point can be a vertex
    st = [piv]
    for q in rest:
        while len(st) >= 2 and cross(st[-2], st[-1], q) <= 0:
            st.pop()
        st.append(q)
    k = st.index(lo)
    st = st[k:] + st[:k]
    return [first[p] for p in st]


def arr(name, ctype, vals, per=24):
    out = ["const %s %s[%d] = {" % (ctype, name, len(vals))]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return "\n".join(out)


def grid_cells(vals):
    # cell c = row * w + col, w = len(vals), with x from the column and y from the row
    w = len(vals)
    return [(vals[c % w], vals[c // w]) for c in range(w * w)]


def digest_group(vals, maxlen):
    # every ordered sequence of cells of length 1..maxlen, shorter lengths first, each length in itertools.product order
    cells = grid_cells(vals)
    h = FNV_OFF
    count = 0
    for ln in range(1, maxlen + 1):
        for seq in itertools.product(range(len(cells)), repeat=ln):
            res = jarvis([cells[c] for c in seq])
            h = fnv_word(h, len(res))
            for v in res:
                h = fnv_word(h, v)
            count += 1
    return h, count


def lcg_stream(seed):
    s = [seed]

    def nxt():
        s[0] = (s[0] * 1664525 + 1013904223) & 0xFFFFFFFF
        return s[0]

    return nxt


def big_pattern(pid):
    n = N_BIG
    if pid == 0:  # parabola plus one interior point
        pts = [(i - 32767, (i - 32767) ** 2) for i in range(n - 1)] + [(0, 2)]
    elif pid == 1:
        pts = [(5, -7)] * n
    elif pid == 2:  # diagonal, scrambled index order
        base = [((i - 32768) * 30000,) * 2 for i in range(n)]
        pts = [base[(i * 40503) % n] for i in range(n)]
    elif pid == 3:  # square perimeter, scrambled
        step = 131072
        base = []
        for t in range(16384):
            base.append((-L + t * step, -L))
        for t in range(16384):
            base.append((L, -L + t * step))
        for t in range(16384):
            base.append((L - t * step, L))
        for t in range(16384):
            base.append((-L, L - t * step))
        pts = [base[(i * 40503) % n] for i in range(n)]
    elif pid == 4:  # random over the full coordinate square
        g = lcg_stream(4)
        pts = []
        for _ in range(n):
            x = (g() >> 1) % 2147483647 - L
            y = (g() >> 1) % 2147483647 - L
            pts.append((x, y))
    elif pid == 5:  # dense small grid: many duplicates and collinear runs
        g = lcg_stream(5)
        pts = []
        for _ in range(n):
            x = (g() >> 16) % 200 - 100
            y = (g() >> 16) % 200 - 100
            pts.append((x, y))
    else:  # only the four limit corners, repeated
        cs = [(-L, -L), (L, -L), (L, L), (-L, L)]
        pts = [cs[i % 4] for i in range(n)]
    return pts


def main():
    lines = []
    fold = FNV_OFF

    # explicit table: 3x3 grid, every ordered sequence of length 0..4 (empty set first)
    cells = grid_cells([0, 1, 2])
    flat = []
    cases = 1
    flat += [0]  # n = 0: count 0
    for ln in range(1, 5):
        for seq in itertools.product(range(9), repeat=ln):
            pts = [cells[c] for c in seq]
            res = jarvis(pts)
            flat.append(len(res))
            flat += res
            cases += 1
    lines.append(arr("kGrid3Hull", "std::uint8_t", flat, 40))
    lines.append("const std::uint32_t kGrid3Cases = %d;" % cases)

    d1, c1 = digest_group([0, 1, 2], 5)
    d2, c2 = digest_group([-L, 0, L], 5)
    d3, c3 = digest_group([0, 1, 2, 3], 4)
    lines.append("const std::uint64_t kDigestGrid3 = %dull; const std::uint32_t kCountGrid3 = %d;" % (d1, c1))
    lines.append("const std::uint64_t kDigestGrid3Scaled = %dull; const std::uint32_t kCountGrid3Scaled = %d;" % (d2, c2))
    lines.append("const std::uint64_t kDigestGrid4 = %dull; const std::uint32_t kCountGrid4 = %d;" % (d3, c3))
    for d in (d1, d2, d3):
        for v in (d & 0xFFFFFFFF, d >> 32):
            fold = fnv_word(fold, v)

    # mid-size sets with full tables
    rng = random.Random(20261127)
    xs, ys, hdr, hres = [], [], [], []
    for t in range(80):
        n = rng.randrange(3, 61)
        style = t % 5
        if style == 0:
            pts = [(rng.randrange(-3, 4), rng.randrange(-3, 4)) for _ in range(n)]
        elif style == 1:
            pts = [(rng.randrange(-1000, 1001), rng.randrange(-1000, 1001)) for _ in range(n)]
        elif style == 2:
            pts = [(rng.choice((-L, 0, L, 5)), rng.choice((-L, 0, L, -5))) for _ in range(n)]
        elif style == 3:
            a, b = rng.randrange(-5, 6), rng.randrange(-5, 6)
            pts = [(a * k, b * k) for k in (rng.randrange(-20, 21) for _ in range(n))]
            if t % 10 == 3:
                pts[rng.randrange(n)] = (rng.randrange(-9, 10), rng.randrange(-9, 10))
        else:
            pts = [(rng.randrange(-L, L + 1), rng.randrange(-L, L + 1)) for _ in range(n)]
        res = jarvis(pts)
        assert res == graham(pts)
        hdr.append((n, len(xs), len(hres)))
        xs += [p[0] for p in pts]
        ys += [p[1] for p in pts]
        hres += [len(res)] + res
    lines.append(arr("kMidX", "std::int32_t", xs, 16))
    lines.append(arr("kMidY", "std::int32_t", ys, 16))
    lines.append("struct MidHdr { std::uint32_t n, off, hoff; };")
    lines.append("const MidHdr kMidHdr[%d] = {\n%s\n};" % (len(hdr), ",\n".join("    {%d, %d, %d}" % x for x in hdr)))
    lines.append(arr("kMidHull", "std::uint16_t", hres, 24))
    for v in hres:
        fold = fnv_word(fold, v)

    # big sets: digests of count + indices
    big = []
    for pid in range(7):
        pts = big_pattern(pid)
        res = graham(pts)
        if pid in (1, 2, 3, 4, 5, 6):
            assert res == jarvis(pts), pid
        if pid == 0:
            # closed form: the parabola points are all vertices except none; (0, 2) is interior. ccw from x = -32767.
            lower = list(range(0, 65535))  # x ascending along the lower chain
            upper = []  # the top edge joins the last point back to the first, nothing in between
            assert res == lower, "parabola closed form"
        if pid == 3:
            corners = [0, 0, 0, 0]
            pos = {p: i for i, p in enumerate(pts)}
            ccw = [(-L, -L), (L, -L), (L, L), (-L, L)]
            first = distinct(pts)
            assert res == [first[c] for c in ccw], "perimeter closed form"
        h = FNV_OFF
        h = fnv_word(h, len(res))
        for v in res:
            h = fnv_word(h, v)
        big.append((pid, len(res), h, res[0], res[-1]))
        for v in (len(res), h & 0xFFFFFFFF, h >> 32):
            fold = fnv_word(fold, v)
    lines.append("struct BigRow { std::uint32_t pid, count; std::uint64_t digest; std::uint32_t first, last; };")
    lines.append("const BigRow kBig[%d] = {\n%s\n};" % (len(big), ",\n".join("    {%d, %d, %dull, %d, %d}" % x for x in big)))
    lines.append("const std::uint64_t kGolden = %dull;" % fold)
    print("\n\n".join(lines))


main()
