#!/usr/bin/env python3
"""Independent reference for core/ordstat: prints the data tables of tests/ordstat/ordstat_tests.cpp as C++ source.

Expected values do not use the C++ method (randomized quickselect, bottom-up merge sort): the k-th smallest value is read from a fully sorted
list, and inversions are counted by bisect insertion into a sorted list, by closed formulas for the structured big inputs, and by brute force
over all pairs for small inputs. Each method is cross-checked against the others while generating.
"""
import bisect
import itertools
import random

M32 = 0xFFFFFFFF
N = 1 << 20


def inv_bisect(a):
    seen = []
    total = 0
    for x in a:
        total += len(seen) - bisect.bisect_right(seen, x)  # earlier elements strictly greater than x
        bisect.insort(seen, x)
    return total


def inv_brute(a):
    return sum(1 for i in range(len(a)) for j in range(i + 1, len(a)) if a[i] > a[j])


def pattern(pid, n):
    if pid == 0:
        return list(range(n))
    if pid == 1:
        return [n - 1 - i for i in range(n)]
    if pid == 2:
        return [7] * n
    if pid == 3:
        return [i % 1024 for i in range(n)]
    if pid == 4:
        return [i % 3 for i in range(n)]
    if pid == 5:
        h = n // 2
        return [i if i < h else n - 1 - i for i in range(n)]
    s = 1234
    out = []
    for _ in range(n):
        s = (s * 1664525 + 1013904223) & M32
        out.append(s >> 8)
    return out


def pattern_inversions(pid, n):
    if pid in (0, 2):
        return 0
    if pid == 1:
        return n * (n - 1) // 2
    if pid == 3:
        p, m = 1024, n // 1024
        return (m * (m - 1) // 2) * (p * (p - 1) // 2)
    if pid == 4:
        cnt = [0, 0, 0]
        tot = 0
        for x in range(n):
            v = x % 3
            tot += sum(cnt[w] for w in range(v + 1, 3))
            cnt[v] += 1
        return tot
    if pid == 5:
        h = n // 2
        return h * (h - 1)
    raise ValueError


def main():
    rng = random.Random(5150)
    out = []
    # formula checks at small sizes against brute force
    for pid in range(6):
        for n in (1024, 2048):
            assert pattern_inversions(pid, n) == inv_brute(pattern(pid, n)) if n == 1024 else True
    for pid in range(6):
        assert pattern_inversions(pid, 4096) == inv_bisect(pattern(pid, 4096))
    # select rows: n, a[n], k, value (n = 0 has no valid k)
    rows = []
    for n in range(1, 6):
        for a in itertools.product([0, 1, 2], repeat=n):
            s = sorted(a)
            for k in range(n):
                rows.append([n] + list(a) + [k, s[k]])
    for _ in range(800):
        n = rng.randint(1, 60)
        if rng.random() < 0.5:
            a = [rng.randint(0, 9) for _ in range(n)]
        else:
            a = [rng.choice([0, 5, 0x7FFFFFFF, 0x80000000, 0x80000001, M32 - 1, M32]) for _ in range(n)]
        s = sorted(a)
        for k in {0, n - 1, rng.randrange(n), rng.randrange(n)}:
            rows.append([n] + a + [k, s[k]])
    for _ in range(8):
        n = rng.randint(1000, 2000)
        a = [rng.randint(0, 40) if rng.random() < 0.5 else rng.getrandbits(32) for _ in range(n)]
        s = sorted(a)
        for k in (0, n // 2, n - 1, rng.randrange(n)):
            rows.append([n] + a + [k, s[k]])
    out.append("static const unsigned kSelRows = %d;" % len(rows))
    out.append("static const std::uint32_t kSel[] = {%s};" % ",".join(str(x) for r in rows for x in r))
    # inversion rows: n, a[n], lo, hi
    rows = []
    for n in range(0, 7):
        for a in itertools.product([0, 1, 2], repeat=n):
            c = inv_bisect(list(a))
            assert c == inv_brute(list(a))
            rows.append([n] + list(a) + [c & M32, c >> 32])
    for _ in range(800):
        n = rng.randint(0, 80)
        a = [rng.choice([0, 1, 2, 3, 0x80000000, M32]) for _ in range(n)]
        c = inv_bisect(a)
        assert c == inv_brute(a)
        rows.append([n] + a + [c & M32, c >> 32])
    for _ in range(8):
        n = rng.randint(500, 1500)
        a = [rng.getrandbits(32) if rng.random() < 0.5 else rng.randint(0, 50) for _ in range(n)]
        c = inv_bisect(a)
        rows.append([n] + a + [c & M32, c >> 32])
    out.append("static const unsigned kInvRows = %d;" % len(rows))
    out.append("static const std::uint32_t kInv[] = {%s};" % ",".join(str(x) for r in rows for x in r))
    # big patterns of N = 2^20 values: select expectations (pid, k, value) from the sorted list
    ks = (0, 1, N // 3, N // 2, N - 2, N - 1)
    big = []
    for pid in range(7):
        s = sorted(pattern(pid, N))
        for k in ks:
            big.extend([pid, k, s[k]])
    out.append("static const std::uint32_t kBigSel[] = {%s};" % ",".join(str(x) for x in big))
    out.append("static const unsigned kBigSelRows = %d;" % (len(big) // 3))
    # big inversion counts (patterns 0..5 at N, as u64)
    inv = []
    for pid in range(6):
        c = pattern_inversions(pid, N)
        inv.append("%dull" % c)
    out.append("static const std::uint64_t kBigInv[6] = {%s};" % ",".join(inv))
    # random pattern 6 at 50000 values: bisect insertion count
    a = pattern(6, 50000)
    out.append("static const std::uint64_t kRandInv50000 = %dull;" % inv_bisect(a))
    print("\n".join(out))


if __name__ == "__main__":
    main()
