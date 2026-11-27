#!/usr/bin/env python3
"""Independent reference for core/perm: prints the data tables of tests/perm/perm_tests.cpp as C++ source.

Expected values are not computed with the C++ algorithm. Next arrangements come from the sorted set of all distinct arrangements
(itertools.permutations), ranks of small n come from the index in the sorted list of all permutations, and the larger ranks come from
a counting argument with exact Python integers, cross-checked against the inverse (unrank by repeated division) and against the
enumeration for n up to 8.
"""
import bisect
import itertools
import math
import random

M32 = 0xFFFFFFFF
FNV = 0xCBF29CE484222325
PRIME = 0x100000001B3


def fnv(h, v):
    for k in range(4):
        h ^= (v >> (8 * k)) & 255
        h = (h * PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def next_by_enumeration(a):
    """(has_next, result) from the sorted list of distinct arrangements."""
    allp = sorted(set(itertools.permutations(a)))
    i = bisect.bisect_left(allp, tuple(a))
    if i + 1 < len(allp):
        return True, list(allp[i + 1])
    return False, list(allp[0])


def rank_by_counting(p):
    """Number of permutations of 0..n-1 that are lexicographically smaller (exact integers)."""
    n = len(p)
    total = 0
    left = list(range(n))
    for i, v in enumerate(p):
        smaller = left.index(v)
        total += smaller * math.factorial(n - 1 - i)
        left.remove(v)
    return total


def unrank_by_division(n, r):
    left = list(range(n))
    out = []
    for i in range(n):
        f = math.factorial(n - 1 - i)
        q, r = divmod(r, f)
        out.append(left.pop(q))
    return out


def main():
    rng = random.Random(20260527)
    out = []
    # --- next rows: n, a[n], has, out[n]
    rows = []
    alpha = [0, 2, 0x80000000, M32]
    for n in range(0, 7):
        for a in itertools.product(alpha, repeat=n):
            h, r = next_by_enumeration(list(a))
            rows.append([n] + list(a) + [1 if h else 0] + r)
    for _ in range(300):
        n = 7
        a = [rng.choice([1, 5, M32 - 1, 7]) for _ in range(n)]
        h, r = next_by_enumeration(a)
        rows.append([n] + a + [1 if h else 0] + r)
    flat = [x for r in rows for x in r]
    out.append("static const unsigned kNextRows = %d;" % len(rows))
    out.append("static const std::uint32_t kNext[] = {%s};" % ",".join(str(x) for x in flat))
    # --- rank rows: n, perm[n], rank_lo, rank_hi ; unrank uses the same table
    prow = []
    for n in range(0, 21):
        cases = [list(range(n)), list(range(n - 1, -1, -1))]
        for _ in range(40):
            p = list(range(n))
            rng.shuffle(p)
            cases.append(p)
        if n >= 2:
            for r in (1, math.factorial(n) - 2, math.factorial(n) // 2):
                cases.append(unrank_by_division(n, r))
        for p in cases:
            r = rank_by_counting(p)
            assert unrank_by_division(n, r) == p
            if n <= 8 and rng.random() < 0.02:
                allp = list(itertools.permutations(range(n)))
                assert allp[r] == tuple(p)
            prow.append([n] + p + [r & M32, r >> 32])
    flat = [x for r in prow for x in r]
    out.append("static const unsigned kRankRows = %d;" % len(prow))
    out.append("static const std::uint32_t kRank[] = {%s};" % ",".join(str(x) for x in flat))
    out.append("static const std::uint64_t kFact[21] = {%s};" % ",".join("%dull" % math.factorial(n) for n in range(21)))
    # --- digests: every permutation of 0..n-1 in lexicographic order, n = 1..8 (from itertools, sorted by construction)
    for n in range(1, 9):
        h = FNV
        cnt = 0
        for p in itertools.permutations(range(n)):  # itertools yields lexicographic order for sorted input
            for v in p:
                h = fnv(h, v)
            cnt += 1
        out.append("static const std::uint64_t kPermDigest%d = %dull;" % (n, h))
        assert cnt == math.factorial(n)
    # --- multiset walks: distinct arrangements in order, digest and count
    for name, ms in (("A", [0, 0, 1, 1, 1, 2]), ("B", [0, 0, 1, 1, 2, 2, 3]), ("C", [5, M32, M32, 5, 0x80000000])):
        allp = sorted(set(itertools.permutations(ms)))
        h = FNV
        for p in allp:
            for v in p:
                h = fnv(h, v)
        out.append("static const std::uint32_t kMs%s[] = {%s};" % (name, ",".join(str(v) for v in sorted(ms))))
        out.append("static const unsigned kMs%sN = %d;" % (name, len(ms)))
        out.append("static const unsigned kMs%sCount = %d;" % (name, len(allp)))
        out.append("static const std::uint64_t kMs%sDigest = %dull;" % (name, h))
    print("\n".join(out))


if __name__ == "__main__":
    main()
