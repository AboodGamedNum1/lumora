#!/usr/bin/env python3
"""Independent reference for core/seqdist: prints the data tables of tests/seqdist/seqdist_tests.cpp as C++ source.

Expected values are not computed with the C++ method (two rolling rows, patience tails). Here: full two-dimensional tables for LCS and
edit distance, a quadratic dynamic programme over end positions for the increasing subsequences, and, for short inputs, brute force over
all subsequences. Properties that tie the three together are asserted while generating.
"""
import itertools
import random

M32 = 0xFFFFFFFF
FNV = 0xCBF29CE484222325
PRIME = 0x100000001B3


def fnv(h, v):
    for k in range(4):
        h ^= (v >> (8 * k)) & 255
        h = (h * PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def lcs_table(a, b):
    t = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a) - 1, -1, -1):  # suffix form, the C++ code uses prefixes
        for j in range(len(b) - 1, -1, -1):
            if a[i] == b[j]:
                t[i][j] = 1 + t[i + 1][j + 1]
            else:
                t[i][j] = max(t[i + 1][j], t[i][j + 1])
    return t[0][0]


def edit_table(a, b):
    t = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a), -1, -1):
        for j in range(len(b), -1, -1):
            if i == len(a):
                t[i][j] = len(b) - j
            elif j == len(b):
                t[i][j] = len(a) - i
            else:
                t[i][j] = min(1 + t[i + 1][j], 1 + t[i][j + 1], t[i + 1][j + 1] + (0 if a[i] == b[j] else 1))
    return t[0][0]


def is_subseq(s, b):
    it = iter(b)
    return all(any(x == y for y in it) for x in s)


def lcs_brute(a, b):
    best = 0
    for mask in range(1 << len(a)):
        s = [a[i] for i in range(len(a)) if (mask >> i) & 1]
        if len(s) > best and is_subseq(s, b):
            best = len(s)
    return best


def lis_quadratic(a, strict):
    n = len(a)
    if n == 0:
        return 0
    best = [1] * n
    for i in range(n):
        for j in range(i):
            ok = a[j] < a[i] if strict else a[j] <= a[i]
            if ok and best[j] + 1 > best[i]:
                best[i] = best[j] + 1
    return max(best)


def lis_brute(a, strict):
    best = 0
    for mask in range(1 << len(a)):
        s = [a[i] for i in range(len(a)) if (mask >> i) & 1]
        if all((s[k] < s[k + 1]) if strict else (s[k] <= s[k + 1]) for k in range(len(s) - 1)):
            best = max(best, len(s))
    return best


def pair_row(a, b):
    l = lcs_table(a, b)
    e = edit_table(a, b)
    assert abs(len(a) - len(b)) <= e <= len(a) + len(b) - 2 * l
    assert l <= min(len(a), len(b))
    if len(a) <= 7 and len(b) <= 7:
        assert l == lcs_brute(a, b)
    return [len(a)] + list(a) + [len(b)] + list(b) + [l, e]


def lis_row(a):
    s = lis_quadratic(a, True)
    ns = lis_quadratic(a, False)
    assert s <= ns
    if len(a) <= 10:
        assert s == lis_brute(a, True) and ns == lis_brute(a, False)
    return [len(a)] + list(a) + [s, ns]


def main():
    rng = random.Random(77123)
    out = []
    rows = []
    seqs = [list(s) for n in range(4) for s in itertools.product([0, 1, 2], repeat=n)]
    for a in seqs:
        for b in seqs:
            rows.append(pair_row(a, b))
    alpha = [0, 1, 2, 3, 0x80000000, M32]
    for _ in range(1500):
        a = [rng.choice(alpha) for _ in range(rng.randint(0, 30))]
        b = [rng.choice(alpha) for _ in range(rng.randint(0, 30))]
        rows.append(pair_row(a, b))
    for _ in range(60):
        a = [rng.randint(0, 3) for _ in range(rng.randint(100, 250))]
        b = [rng.randint(0, 3) for _ in range(rng.randint(100, 250))]
        rows.append(pair_row(a, b))
    for na, nb in ((2000, 2000), (2000, 1500)):
        rows.append(pair_row([rng.randint(0, 4) for _ in range(na)], [rng.randint(0, 4) for _ in range(nb)]))
    out.append("static const unsigned kPairRows = %d;" % len(rows))
    out.append("static const std::uint32_t kPairs[] = {%s};" % ",".join(str(x) for r in rows for x in r))
    rows = []
    for n in range(7):
        for s in itertools.product([0, 1, 2], repeat=n):
            rows.append(lis_row(list(s)))
    for _ in range(1000):
        n = rng.randint(0, 60)
        if rng.random() < 0.5:
            a = [rng.randint(0, 9) for _ in range(n)]
        else:
            a = [rng.choice([0, 5, 0x7FFFFFFF, 0x80000000, 0x80000001, M32 - 1, M32]) for _ in range(n)]
        rows.append(lis_row(a))
    for _ in range(40):
        n = rng.randint(300, 900)
        rows.append(lis_row([rng.randint(0, 50) for _ in range(n)]))
    out.append("static const unsigned kLisRows = %d;" % len(rows))
    out.append("static const std::uint32_t kLis[] = {%s};" % ",".join(str(x) for r in rows for x in r))

    # digest run: LCG streams, five pairs (the C++ side regenerates the same streams)
    s = 314159

    def nxt():
        nonlocal s
        s = (s * 1664525 + 1013904223) & M32
        return s >> 8

    h = FNV
    for na, nb in ((600, 500), (450, 700), (800, 31), (1, 900), (300, 300)):
        a = [nxt() % 6 for _ in range(na)]
        b = [nxt() % 6 for _ in range(nb)]
        for v in (lcs_table(a, b), edit_table(a, b), lis_quadratic(a, True), lis_quadratic(a, False), lis_quadratic(b, True)):
            h = fnv(h, v)
    out.append("static const std::uint64_t kDigest = %dull;" % h)
    print("\n".join(out))


if __name__ == "__main__":
    main()
