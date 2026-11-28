#!/usr/bin/env python3
"""Independent reference for core/calkinwilf (Stern's diatomic sequence fusc, the Calkin-Wilf tree, and the index of a fraction).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/calkinwilf.md.
Usage:
  python3 tools/calkinwilf_ref.py chunk K     digest of (n, s(n), s(n + 1)) for n in [K * 2^20, (K + 1) * 2^20) (K = 0..3; K = 0 starts at 1; K = 3 also holds 2^22)
  python3 tools/calkinwilf_ref.py pairs K     digest of (p, q, code, index) for p in a half of 0..1024 (K = 0: p <= 512, K = 1: p >= 513) and every q in 0..1024
  python3 tools/calkinwilf_ref.py nodes K     digest of (n, s(n), s(n + 1)) for the 50000 sampled nodes of shard K = 0..3
  python3 tools/calkinwilf_ref.py fracs K     digest of (p, q, code, index) for the 50000 sampled pairs of shard K = 0..1
  python3 tools/calkinwilf_ref.py deep        digest and counts of the 20000 sampled depth 40..63 nodes that fit a u32 plus their children that fit a u32
  python3 tools/calkinwilf_ref.py noncoprime  digest of 1000 deliberately non-coprime u32 pairs
  python3 tools/calkinwilf_ref.py edges       the digest of the edge nodes 2^k - 1, 2^k, 2^k + 1 and 2^64 - 3 .. 2^64 - 1
  python3 tools/calkinwilf_ref.py check       the identities below (fusc maxima, header examples, the tree against the inverse), prints "OK"
  python3 tools/calkinwilf_ref.py emit        print the golden values and tables as C++ source for tests/calkinwilf/calkinwilf_tests.cpp
Methods (all independent of the implementation, which uses the binary loop for fusc and the quotient walk for the index):
  fusc: the recurrence s(2k) = s(k), s(2k + 1) = s(k) + s(k + 1), by memoised recursion; an array built by the recurrence for n <= 2^22.
  index: an actual walk up the tree with Python integers (no overflow, no 64-bit limit): a fraction p / q with p > q has the parent (p - q) / q, one subtraction at a time
     for p, q <= 1024; for the large pairs the same walk by runs (quotients) with the run lengths summed; the index is 2^S plus the path bits and is TooLarge when it is at least 2^64.
  depth tables: fractions built from the root by applying a run-length shape downward (a right step is p += q, a left step is q += p), so the index is the path itself.
Digest: FNV-1a 64-bit over u64 words.
"""
import sys
from math import gcd

sys.setrecursionlimit(100000)
M64 = (1 << 64) - 1
FNV0 = 0xcbf29ce484222325
TOP = 1 << 22
U32 = (1 << 32) - 1
OK, BAD, NOTC, BIG = 0, 2, 3, 4


def step(h, w):
    return ((h ^ w) * 0x100000001b3) & M64


_memo = {0: 0, 1: 1}


def s(n):
    if n in _memo:
        return _memo[n]
    v = s(n // 2) if n % 2 == 0 else s(n // 2) + s(n // 2 + 1)
    _memo[n] = v
    return v


_ARR = None


def arr():
    global _ARR
    if _ARR is None:
        a = [0] * (TOP + 2)
        a[1] = 1
        for n in range(2, TOP + 2):
            a[n] = a[n >> 1] if n % 2 == 0 else a[n >> 1] + a[(n >> 1) + 1]
        _ARR = a
    return _ARR


def walk_single(p, q):
    """index and steps by one subtraction at a time (p, q <= about 2^10 here)."""
    bits = []
    while (p, q) != (1, 1):
        if p > q:
            p -= q
            bits.append(1)
        else:
            q -= p
            bits.append(0)
    n = 1
    for b in reversed(bits):
        n = n * 2 + b
    return n, len(bits)


def walk_runs(p, q):
    """steps and, when the steps are at most 63, the index, by runs of equal steps."""
    runs = []  # leaf-first (bit, length)
    while (p, q) != (1, 1):
        if p > q:
            t = p - 1 if q == 1 else p // q
            runs.append((1, t))
            p -= t * q
        else:
            t = q - 1 if p == 1 else q // p
            runs.append((0, t))
            q -= t * p
    total = sum(t for _, t in runs)
    if total > 63:
        return None, total
    n = 0
    sh = 0
    for b, t in runs:
        if b:
            n |= ((1 << t) - 1) << sh
        sh += t
    return (1 << total) | n, total


def code_index(p, q, single=False):
    """(code, index) as the header defines them."""
    if p == 0 or q == 0:
        return BAD, 0
    if gcd(p, q) != 1:
        return NOTC, 0
    if single:
        n, st = walk_single(p, q)
        return (OK, n) if n < (1 << 64) else (BIG, 0)
    n, st = walk_runs(p, q)
    return (BIG, 0) if n is None else (OK, n)


def chunk_digest(k):
    a = arr()
    lo = max(k * (1 << 20), 1)
    hi = (k + 1) * (1 << 20) + (1 if k == 3 else 0)
    h = FNV0
    for n in range(lo, hi):
        h = step(step(step(h, n), a[n]), a[n + 1])
    return h


def pairs_digest(k):
    h = FNV0
    ps = range(0, 513) if k == 0 else range(513, 1025)
    for p in ps:
        for q in range(0, 1025):
            c, i = code_index(p, q, single=True)
            for w in (p, q, c, i):
                h = step(h, w)
    return h


def splitmix_at(pos):
    """the stream value number pos (0-based) of splitmix64 with state 0x9E3779B97F4A7C15."""
    st = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = st
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def nodes_digest(k):
    h = FNV0
    for i in range(k * 50000, (k + 1) * 50000):
        n = TOP + splitmix_at(i) % ((1 << 64) - 1 - TOP)
        for w in (n, s(n), s(n + 1)):
            h = step(h, w)
    return h


def fracs_pair(i):
    x = 1 + splitmix_at(2 * i + (1 << 40)) % U32
    y = 1 + splitmix_at(2 * i + 1 + (1 << 40)) % U32
    return x, y


def fracs_digest(k):
    h = FNV0
    for i in range(k * 50000, (k + 1) * 50000):
        p, q = fracs_pair(i)
        c, ix = code_index(p, q)
        for w in (p, q, c, ix):
            h = step(h, w)
    return h


def deep_items():
    """the 20000 sampled nodes of depth 40..63 whose fraction fits a u32, and their depth + 1 children that fit a u32.
    Returns (rows, nodes, children): rows are (p, q, code, index) in the order node, left child, right child."""
    rows = []
    nodes = 0
    kids = 0
    pos = 0
    while nodes < 20000:
        d = 40 + splitmix_at(3 * pos + (1 << 41)) % 24
        n = (1 << d) + splitmix_at(3 * pos + 1 + (1 << 41)) % (1 << d)
        pos += 1
        p, q = s(n), s(n + 1)
        if p > U32 or q > U32:
            continue
        nodes += 1
        rows.append((p, q) + code_index(p, q))
        for cp, cq in ((p, p + q), (p + q, q)):
            if cp <= U32 and cq <= U32:
                kids += 1
                rows.append((cp, cq) + code_index(cp, cq))
    return rows, nodes, kids, pos


def deep_digest():
    rows, nodes, kids, pos = deep_items()
    h = FNV0
    for r in rows:
        for w in r:
            h = step(h, w)
    return h, nodes, kids, pos


def noncoprime_pairs():
    out = []
    for i in range(1000):
        g = 2 + splitmix_at(5 * i + (1 << 42)) % 65535
        a = 1 + splitmix_at(5 * i + 1 + (1 << 42)) % (U32 // g)
        b = 1 + splitmix_at(5 * i + 2 + (1 << 42)) % (U32 // g)
        out.append((g * a, g * b))
    return out


def noncoprime_digest():
    h = FNV0
    for p, q in noncoprime_pairs():
        c, ix = code_index(p, q)
        assert c == NOTC
        for w in (p, q, c, ix):
            h = step(h, w)
    return h


def edge_nodes():
    v = set()
    for k in range(1, 64):
        v.update(((1 << k) - 1, 1 << k, (1 << k) + 1))
    v.update((M64 - 2, M64 - 1, M64))
    return sorted(v)


def edges_digest():
    h = FNV0
    for n in edge_nodes():
        ok = n <= M64 - 1
        for w in (n, s(n) if ok else 0, s(n + 1) if ok else 0, OK if ok else BIG):
            h = step(h, w)
    return h


def from_shape(runs, first_right):
    """fraction after applying runs of steps downward from 1/1; returns (p, q, path index)."""
    p = q = 1
    idx = 1
    right = first_right
    for t in runs:
        for _ in range(t):
            if right:
                p += q
            else:
                q += p
            idx = idx * 2 + (1 if right else 0)
        right = not right
    return p, q, idx


def shape_table():
    """depth-boundary fractions (depth 62, 63, 64), several continued-fraction shapes, both orientations, only those with p and q below 2^32."""
    import random
    rng = random.Random(0xC0FFEE)
    rows = []
    seen = set()

    def add(runs, fr, d):
        p, q, idx = from_shape(runs, fr)
        if p > U32 or q > U32 or (p, q) in seen:
            return
        seen.add((p, q))
        rows.append((p, q, d, idx if d <= 63 else 0))
    for d in (62, 63, 64):
        for fr in (True, False):
            add([d], fr, d)
            for t1 in range(1, d):
                add([t1, d - t1], fr, d)
            for m in range(1, 9):
                runs = [m] * (d // m) + ([d % m] if d % m else [])
                add(runs, fr, d)
            for t1 in range(1, d - 1, 3):
                for t2 in range(1, d - t1, 5):
                    add([t1, t2, d - t1 - t2], fr, d)
            tries = 0
            got = 0
            while got < 60 and tries < 5000:
                tries += 1
                left = d
                runs = []
                while left > 0:
                    t = min(left, rng.choice((1, 1, 1, 2, 2, 3, 4, 6, 9)))
                    runs.append(t)
                    left -= t
                before = len(rows)
                add(runs, fr, d)
                got += len(rows) - before
    return rows


def fib_table():
    f = [0, 1]
    while len(f) < 60:
        f.append(f[-1] + f[-2])
    rows = []
    i = 1
    while f[i + 1] <= U32:
        c1, i1 = code_index(f[i + 1], f[i])
        c2, i2 = code_index(f[i], f[i + 1])
        rows.append((i, f[i + 1], f[i], i1, i2))
        assert c1 == OK and c2 == OK
        i += 1
    return rows


def checks():
    f = [0, 1]
    while len(f) < 70:
        f.append(f[-1] + f[-2])
    assert f[65] == 17167680177565
    for k in range(1, 65):
        n = ((1 << (k + 1)) + (-1) ** k) // 3
        assert n < (1 << k) and s(n) == f[k + 1], k
    a = arr()
    best = {}
    for n in range(1, TOP):
        k = n.bit_length()
        if a[n] > best.get(k, 0):
            best[k] = a[n]
    for k in range(1, 23):
        assert best[k] == f[k + 1], k
    for n, v in ((0, 0), (1, 1), (100, 7), (1000, 11), (10 ** 6, 191), (2 ** 32, 1), (2 ** 63, 1), (M64 - 2, 125), (M64 - 1, 63), (M64, 64)):
        assert s(n) == v, n
    for n, p, q in ((100, 7, 19), (1000, 11, 39), (2 ** 32, 1, 33), (2 ** 63, 1, 64), (M64 - 2, 125, 63), (M64 - 1, 63, 64)):
        assert (s(n), s(n + 1)) == (p, q), n
    ex = [((1, 1), 1), ((1, 2), 2), ((2, 1), 3), ((3, 2), 5), ((7, 5), 25), ((5, 7), 22), ((22, 7), 519), ((355, 113), 67107847), ((113, 355), 33555448), ((46368, 28657), 5592405), ((64, 1), M64), ((1, 64), 1 << 63), ((63, 64), M64 - 1), ((125, 63), M64 - 2), ((2971215073, 1836311903), 58640620148053), ((1836311903, 2971215073), 46912496118442)]
    for (p, q), n in ex:
        assert code_index(p, q) == (OK, n), (p, q)
    for p, q in ((65, 1), (1, 65), (4294967295, 4294967294)):
        assert code_index(p, q)[0] == BIG
    assert code_index(2, 4)[0] == NOTC
    # the tree against the inverse for every n up to 2^16, and the single and run walks agree on every p, q <= 300
    for n in range(1, 1 << 16):
        p, q = a[n], a[n + 1]
        assert code_index(p, q) == (OK, n)
    for p in range(1, 301):
        for q in range(1, 301):
            if gcd(p, q) == 1:
                n1, st1 = walk_single(p, q)
                n2, st2 = walk_runs(p, q)
                assert st1 == st2 and ((n2 is None and st1 > 63) or n2 == n1)
    for p, q, d, ix in shape_table():
        c, i2 = code_index(p, q)
        assert (c == OK) == (d <= 63) and (c != OK or i2 == ix)
    # the u64 index bound: a node at depth S is in [2^S, 2^(S + 1) - 1]
    for n in (1, 2, 3, 4, 7, 8, 1 << 62, (1 << 63) - 1, 1 << 63, M64):
        S = n.bit_length() - 1
        assert (1 << S) <= n < (1 << (S + 1))
    assert len(fib_table()) == 46


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    arg = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    hx = lambda v: "0x%016xull" % v
    if cmd == "chunk":
        print(hx(chunk_digest(arg)))
    elif cmd == "pairs":
        print(hx(pairs_digest(arg)))
    elif cmd == "nodes":
        print(hx(nodes_digest(arg)))
    elif cmd == "fracs":
        print(hx(fracs_digest(arg)))
    elif cmd == "deep":
        print(*[hx(deep_digest()[0])] + list(deep_digest()[1:]))
    elif cmd == "noncoprime":
        print(hx(noncoprime_digest()))
    elif cmd == "edges":
        print(len(edge_nodes()), hx(edges_digest()))
    elif cmd == "check":
        checks()
        print("OK")
    elif cmd == "emit":
        print("constexpr std::uint64_t kChunkDigest[4] = {%s};" % ", ".join(hx(chunk_digest(k)) for k in range(4)))
        print("constexpr std::uint64_t kPairsDigest[2] = {%s};" % ", ".join(hx(pairs_digest(k)) for k in range(2)))
        print("constexpr std::uint64_t kNodesDigest[4] = {%s};" % ", ".join(hx(nodes_digest(k)) for k in range(4)))
        print("constexpr std::uint64_t kFracsDigest[2] = {%s};" % ", ".join(hx(fracs_digest(k)) for k in range(2)))
        dd = deep_digest()
        print("constexpr std::uint64_t kDeepDigest = %s;" % hx(dd[0]))
        print("constexpr std::uint32_t kDeepNodes = %du;" % dd[1])
        print("constexpr std::uint32_t kDeepKids = %du;" % dd[2])
        print("constexpr std::uint64_t kDeepTries = %dull;" % dd[3])
        print("constexpr std::uint64_t kNonCoprimeDigest = %s;" % hx(noncoprime_digest()))
        print("constexpr std::uint64_t kEdgesDigest = %s;" % hx(edges_digest()))
        print("constexpr std::size_t kEdgesCount = %d;" % len(edge_nodes()))
        rows = shape_table()
        print("struct Shape { std::uint32_t p; std::uint32_t q; std::uint32_t depth; std::uint64_t index; };")
        print("constexpr Shape kShapes[%d] = {" % len(rows))
        for p, q, d, ix in rows:
            print("    {%du, %du, %du, %dull}," % (p, q, d, ix))
        print("};")
        fr = fib_table()
        print("struct FibRow { std::uint32_t i; std::uint32_t big; std::uint32_t small; std::uint64_t up; std::uint64_t down; };")
        print("constexpr FibRow kFib[%d] = {" % len(fr))
        for i, a, b, i1, i2 in fr:
            print("    {%du, %du, %du, %dull, %dull}," % (i, a, b, i1, i2))
        print("};")
    else:
        sys.exit("unknown command")


if __name__ == "__main__":
    main()
