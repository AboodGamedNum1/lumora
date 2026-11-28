#!/usr/bin/env python3
"""Independent reference for core/farey (Python standard library only; no code shared with the C++ library).

Neighbours in the Farey sequence F_n. Small n: the whole sorted sequence F_n is built by brute force (every reduced fraction with b <= n, sorted with exact Fraction
comparison) and the neighbours are read from the sorted list. Larger n: the exact definition by a scan, the least fraction above a / b with denominator at most n, which is
the minimum of (floor(a d / b) + 1) / d over every d in 1..n (and symmetrically the maximum of (ceil(a d / b) - 1) / d for the previous term). The full u32 range: the
window property (c / d follows a / b in F_n iff b c - a d = 1 and n - b < d <= n), with d found from pow(a, -1, b); every result is verified with exact integers, and
the `check` command cross-checks the scan against the window formula for n up to 40 (every term) and on sampled n up to 3000 and up to 20000, and the Stern-Brocot descent (sb_neighbour, batched mediant steps to the two parents of a / b, then the parent advanced by multiples of a / b) against the window formula for every term with n up to 40 and on 3000 samples over the whole u32 range. The Haros recurrence (F2) builds the whole sequence without any gcd.
Digests are FNV-1a over 64-bit words (fold(h, w) = (h ^ w) * 0x100000001b3 mod 2^64, start 0xcbf29ce484222325).
Commands: check, f1, f2 A|B|C, f3, f4 K (K = 0..3), f4b, emit_part N (N = 0..2).
"""
import sys
from fractions import Fraction
from math import gcd

M64 = (1 << 64) - 1
FNV0 = 0xCBF29CE484222325
OK, NULLARG, BADORD, BADFRAC, NONEIGH = 0, 1, 2, 3, 4
N32 = (1 << 32) - 1


def fold(h, w):
    return ((h ^ (w & M64)) * 0x100000001B3) & M64


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def valid(n, a, b):
    return b != 0 and b <= n and a <= b and gcd(a, b) == 1


def neigh_scan(n, a, b, nxt):
    """exact definition by a scan over every denominator d in 1..n"""
    best = None
    for d in range(1, n + 1):
        if nxt:
            c = (a * d) // b + 1
            if c > d:
                continue
            f = Fraction(c, d)
            if best is None or f < best:
                best = f
        else:
            c = -((-a * d) // b) - 1
            if c < 0:
                continue
            f = Fraction(c, d)
            if best is None or f > best:
                best = f
    return best.numerator, best.denominator


def sb_neighbour(n, a, b, nxt):
    """Stern-Brocot mediant descent to a / b (batched steps). The two parents L and R of a / b satisfy det 1 with it; the neighbour in F_n is the parent
    advanced by multiples of a / b: (c0 + k a) / (d0 + k b) with the largest k keeping the denominator at most n. Needs 0 < a / b < 1 or the matching end."""
    lc, ld, rc, rd = 0, 1, 1, 0
    while True:
        mc, md = lc + rc, ld + rd
        if (mc, md) == (a, b):
            break
        if a * md < mc * b:  # a / b below the mediant: move right bound left, batched
            num = b * rc - a * rd
            den = a * ld - b * lc
            j = (num - 1) // den
            rc, rd = rc + j * lc, rd + j * ld
            assert j >= 1
        else:  # above: move left bound right, batched (L_j = L + j R stays below a / b while j (b rc - a rd) < a ld - b lc)
            j = (a * ld - b * lc - 1) // (b * rc - a * rd)
            lc, ld = lc + j * rc, ld + j * rd
            assert j >= 1
    cc, dd = (rc, rd) if nxt else (lc, ld)
    k = (n - dd) // b
    return cc + k * a, dd + k * b


def neigh_window(n, a, b, nxt):
    """window property: det 1 and the largest denominator at most n"""
    inv = pow(a, -1, b) if b > 1 else 0
    d0 = (-inv) % b if nxt else inv % b
    d = d0 + ((n - d0) // b) * b
    c = (1 + a * d) // b if nxt else (a * d - 1) // b
    if nxt:
        assert b * c - a * d == 1
    else:
        assert a * d - b * c == 1
    assert n - b < d <= n and gcd(c, d) == 1 and 0 <= c <= d
    return c, d


def verdict(n, a, b, nxt):
    """(code, c, d)"""
    if n == 0:
        return BADORD, 0, 0
    if not valid(n, a, b):
        return BADFRAC, 0, 0
    if (nxt and a == b) or ((not nxt) and a == 0):
        return NONEIGH, 0, 0
    c, d = neigh_window(n, a, b, nxt)
    return OK, c, d


def sorted_seq(n):
    s = set()
    for b in range(1, n + 1):
        for a in range(0, b + 1):
            if gcd(a, b) == 1:
                s.add(Fraction(a, b))
    return sorted(s)


def fold_case(h, n, a, b):
    cn, c1, d1 = verdict(n, a, b, True)
    cp, c2, d2 = verdict(n, a, b, False)
    for w in (n, a, b, cn, c1, d1, cp, c2, d2):
        h = fold(h, w)
    return h


def d_f1():
    h = FNV0
    cnt = 0
    for n in range(1, 121):
        seq = sorted_seq(n)
        pos = {(f.numerator, f.denominator): i for i, f in enumerate(seq)}
        for a in range(0, n + 4):
            for b in range(0, n + 4):
                cn = cp = None
                if (a, b) in pos:
                    i = pos[(a, b)]
                    if i + 1 < len(seq):
                        nx = (OK, seq[i + 1].numerator, seq[i + 1].denominator)
                    else:
                        nx = (NONEIGH, 0, 0)
                    if i > 0:
                        pv = (OK, seq[i - 1].numerator, seq[i - 1].denominator)
                    else:
                        pv = (NONEIGH, 0, 0)
                else:
                    nx = pv = (BADFRAC, 0, 0)
                assert nx == verdict(n, a, b, True) and pv == verdict(n, a, b, False), (n, a, b)
                for w in (n, a, b) + nx + pv:
                    h = fold(h, w)
                cnt += 1
    return h, cnt


F2_A = list(range(1, 401))
F2_B = [500, 777, 1000, 1500]
F2_C = [2000, 3000]


def d_f2(ns):
    h = FNV0
    terms = 0
    for n in ns:
        h = fold(h, n)
        pa, pb, a, b = 0, 1, 1, n
        h = fold(fold(h, 0), 1)
        terms += 1
        while True:
            h = fold(fold(h, a), b)
            terms += 1
            if a == 1 and b == 1:
                break
            k = (n + pb) // b
            pa, pb, a, b = a, b, k * a - pa, k * b - pb
        # sanity: term count against the totient sum
    return h, terms


def phi_sum(n):
    phi = list(range(n + 1))
    for i in range(2, n + 1):
        if phi[i] == i:
            for j in range(i, n + 1, i):
                phi[j] -= phi[j] // i
    return 1 + sum(phi[1:n + 1])


def fib_pairs(n):
    out = []
    f0, f1 = 1, 1
    while f1 <= n:
        out.append((f0, f1))
        f0, f1 = f1, f0 + f1
    return out


E_N = [1, 2, 3, 4, 5, 6, 7, 8, 10, 100, 255, 256, 257, 1000, 65535, 65536, 65537, 1 << 20, (1 << 24) - 1, (1 << 31) - 1, 1 << 31, (1 << 31) + 1, 4294967291, 4294967293, 4294967294, 4294967295]


def edge_fracs(n):
    c = [(0, 1), (1, 1), (1, 2), (1, 3), (2, 3), (1, n), (1, n - 1), (n - 1, n), (n - 2, n - 1), (n - 3, n - 2)]
    m = (n - 1) // 2
    c += [(m, 2 * m + 1), (m + 1, 2 * m + 1)]
    c += fib_pairs(n)
    c += [(f1 - f0, f1) for f0, f1 in fib_pairs(n)]
    out = []
    for a, b in c:
        if a >= 0 and valid(n, a, b):
            out.append((a, b))
    return out


def invalid_fracs(n):
    x = (n - 1) // 2
    c = [(0, 0), (1, 0), (0, 2), (0, 3), (0, n), (0, n + 1), (1, n + 1), (n, n), (n, n + 1), (n + 1, n), (n + 1, n + 1), (n - 1, n), (n, n - 1), (2, 4), (2, 2), (4, 6),
         (2 * x, 2 * (x + 1)), (N32, N32), (N32 - 1, N32), (N32, N32 - 1), (N32 - 1, N32 - 2), (2, 2 * n), (n, 2 * n), (n + 2, n + 1), (1, 1), (0, 1)]
    return [(a, b) for a, b in c if 0 <= a <= N32 and 0 <= b <= N32]


def d_f3():
    h = FNV0
    cnt = 0
    for n in E_N:
        for a, b in edge_fracs(n) + invalid_fracs(n):
            h = fold_case(h, n, a, b)
            cnt += 1
    return h, cnt


def vcase(pos):
    w0, w1, w2, w3 = (splitmix_at(pos + i) for i in range(4))
    nb = 1 + w0 % 32
    if (w0 >> 10) & 1:
        nb = 32 - ((w0 >> 11) % 4)
    n = w1 & ((1 << nb) - 1)
    if n == 0:
        n = 1
    mode = (w0 >> 8) & 3
    if mode == 0:
        b = 1 + w2 % n
    elif mode == 1:
        b = n - (w2 % min(n, 1000))
    elif mode == 2:
        b = 1 + w2 % min(n, 1000)
    else:
        b = n
    a = w3 % (b + 1)
    while gcd(a, b) != 1:
        a = (a + 1) % (b + 1)
    return n, a, b


def d_f4(s):
    h = FNV0
    for i in range(50000):
        n, a, b = vcase((1 << 40) + s * 300000 + 4 * i)
        h = fold_case(h, n, a, b)
    return h, 50000


def icase(pos):
    w0, w1, w2 = (splitmix_at(pos + i) for i in range(3))
    nb = 1 + w0 % 32
    n = w1 & ((1 << nb) - 1)
    if n == 0:
        n = 1
    t = (w0 >> 8) % 8
    if t == 0:
        a, b = w2 % (n + 1), 0
    elif t == 1:
        a, b = w2 % (n + 1), min(n + 1 + w2 % 5, N32)
    elif t == 2:
        b = w2 % (n + 1)
        a = min(b + 1 + (w0 >> 16) % 5, N32)
    elif t == 3:
        m = max(2, n // 2)
        b = 2 * (1 + w2 % (m)) if 2 * m <= N32 else 2
        b = min(b, N32)
        a = 2 * (w0 >> 16) % (b + 1)
        a -= a % 2
    elif t == 4:
        a, b = N32, N32
    elif t == 5:
        a, b = 0, 2 + w2 % max(1, min(n, 1000))
    elif t == 6:
        b = 1 + w2 % n
        a = min(b + 1, N32)
    else:
        b = n
        a = w2 % (n + 1)
    return n, a, b


def d_f4b():
    h = FNV0
    for i in range(20000):
        n, a, b = icase((1 << 44) + 3 * i)
        h = fold_case(h, n, a, b)
    return h, 20000


def c_check():
    # brute sequence against the scan and the window formula
    for n in range(1, 41):
        seq = sorted_seq(n)
        for i, f in enumerate(seq):
            a, b = f.numerator, f.denominator
            if i + 1 < len(seq):
                assert neigh_scan(n, a, b, True) == (seq[i + 1].numerator, seq[i + 1].denominator)
                assert neigh_window(n, a, b, True) == (seq[i + 1].numerator, seq[i + 1].denominator)
            if i > 0:
                assert neigh_scan(n, a, b, False) == (seq[i - 1].numerator, seq[i - 1].denominator)
                assert neigh_window(n, a, b, False) == (seq[i - 1].numerator, seq[i - 1].denominator)
    for i in range(400):
        n, a, b = vcase((1 << 41) + 4 * i)
        n = 1 + n % 3000
        b = 1 + (b - 1) % n
        a = a % (b + 1)
        while gcd(a, b) != 1:
            a = (a + 1) % (b + 1)
        if a != b:
            assert neigh_scan(n, a, b, True) == neigh_window(n, a, b, True)
        if a != 0:
            assert neigh_scan(n, a, b, False) == neigh_window(n, a, b, False)
    # Stern-Brocot descent against the window formula over the whole u32 range, and the scan against both on sampled n up to 20000
    for i in range(3000):
        n, a, b = vcase((1 << 42) + 4 * i)
        if 0 < a < b:
            assert sb_neighbour(n, a, b, True) == neigh_window(n, a, b, True), (n, a, b)
            assert sb_neighbour(n, a, b, False) == neigh_window(n, a, b, False), (n, a, b)
    for n in range(1, 41):
        for b in range(1, n + 1):
            for a in range(1, b):
                if gcd(a, b) == 1:
                    assert sb_neighbour(n, a, b, True) == neigh_window(n, a, b, True)
                    assert sb_neighbour(n, a, b, False) == neigh_window(n, a, b, False)
    for i in range(300):
        w = splitmix_at((1 << 43) + i)
        n = 1 + w % 20000
        b = 1 + splitmix_at((1 << 43) + 1000 + i) % n
        a = splitmix_at((1 << 43) + 2000 + i) % (b + 1)
        while gcd(a, b) != 1:
            a = (a + 1) % (b + 1)
        if 0 < a < b:
            assert neigh_scan(n, a, b, True) == neigh_window(n, a, b, True) == sb_neighbour(n, a, b, True), (n, a, b)
            assert neigh_scan(n, a, b, False) == neigh_window(n, a, b, False) == sb_neighbour(n, a, b, False), (n, a, b)
        elif a == 0 or a == b:
            assert neigh_scan(n, a, b, a == 0) == neigh_window(n, a, b, a == 0)
    # header examples
    assert verdict(5, 1, 2, True)[1:] == (3, 5) and verdict(5, 1, 2, False)[1:] == (2, 5)
    assert verdict(100, 22, 67, True)[1:] == (23, 70) and verdict(100, 22, 67, False)[1:] == (21, 64)
    assert verdict(N32, 1234567, 7654321, True)[1:] == (691930712, 4289973553)
    assert verdict(N32, 1234567, 7654321, False)[1:] == (692018895, 4290520288)
    assert verdict(N32, 65536, 65537, True)[1:] == (4294836225, 4294901759)
    assert verdict(N32, 2147483647, N32, False)[1:] == (2147483646, 4294967293)
    for n in (1, 2, 5, 30, 100):
        assert d_f2([n])[1] == phi_sum(n) == len(sorted_seq(n))
    print("check ok")


def put(out, name, hc):
    h, c = hc
    out.append("constexpr u64 k%sDigest = 0x%016xull;" % (name, h))
    out.append("constexpr u64 k%sCount = %du;" % (name, c))


def emit_part(n):
    out = []
    if n == 0:
        put(out, "F1", d_f1())
        put(out, "F3", d_f3())
        put(out, "F4b", d_f4b())
    elif n == 1:
        for s in range(4):
            put(out, "F4s%d" % s, d_f4(s))
    elif n == 2:
        for nm, ns in (("F2A", F2_A), ("F2B", F2_B), ("F2C", F2_C)):
            h, t = d_f2(ns)
            assert t == sum(phi_sum(x) for x in ns)
            put(out, nm, (h, t))
    print("\n".join(out))


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == "check":
        c_check()
    elif a[0] == "f1":
        print(d_f1())
    elif a[0] == "f2":
        print(d_f2({"A": F2_A, "B": F2_B, "C": F2_C}[a[1]]))
    elif a[0] == "f3":
        print(d_f3())
    elif a[0] == "f4":
        print(d_f4(int(a[1])))
    elif a[0] == "f4b":
        print(d_f4b())
    elif a[0] == "emit_part":
        emit_part(int(a[1]))


main()
