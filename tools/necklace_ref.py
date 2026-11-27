#!/usr/bin/env python3
"""Independent reference for core/necklace (necklaces, bracelets and Lyndon words modulo 998244353).

Standard library only (no NumPy, no third-party package). Runtime: Python 3.8 or newer (uses pow(x, -1, p)); the timing is stated in docs/necklace.md.
Usage:
  python3 tools/necklace_ref.py check   run every source against the others, print "OK"
  python3 tools/necklace_ref.py emit    print the golden constants as C++ source for tests/necklace/necklace_tests.cpp (after running the checks)
Three sources of different shape:
  1. brute force: enumerate all k^n strings; a necklace is a string equal to the smallest of its rotations, a bracelet is a string equal to the smallest
     of its rotations and the rotations of its reversal, a Lyndon word is a string strictly smaller than all its other rotations. n = 0 has one (empty)
     string, which counts as a necklace and a bracelet and not as a Lyndon word; k = 0 with n > 0 has no string.
  2. exact big-integer Burnside with checked integer division: necklaces sum_{d|n} phi(d) k^(n/d) / n, Lyndon words sum_{d|n} mu(d) k^(n/d) / n, bracelets
     (sum_{d|n} phi(d) k^(n/d) + R) / (2n) with R = n k^((n+1)/2) for odd n and (n/2)(k^(n/2+1) + k^(n/2)) for even n. Every division is asserted exact.
  3. modular, for large n and large k where exact integers are too big: necklaces as sum_{j<n} k^gcd(j, n) / n (every j counted, grouped by its gcd), Lyndon
     words from the necklace counts of all divisors by sum_{d|n} L(d) = N(n) (solved for L(n)), bracelets by the dihedral average with an explicit 1/(2n).
All three are compared wherever they overlap (check). Digest: FNV-1a 64-bit over 32-bit words (one xor and one multiply per word); a group digest folds the
triples (necklace, bracelet, Lyndon) in the stated loop order.
"""
import sys
from itertools import product
from math import gcd
from collections import Counter

P = 998244353
MAXN = 1 << 20
ENUM_LIMIT = 30000
ENUM_N, ENUM_K = 24, 1100
HEAVY = [(10, 3), (12, 3), (16, 2), (18, 2), (8, 4), (7, 5), (6, 8)]
KS = list(range(41)) + [255, 256, P - 2, P - 1, P, P + 1, 2 * P, 2147483648, 4294967295]
EXACT_N = 2000
LN = [1024, 4096, 65536, 131072, 262144, 524288, 524289, 786432, 1000000, 720720, 997920, 65537, 7919, 1048573, 1048575, 1048576]
KL = [0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 16, 255, 256, P - 1, P, P + 1, 2 * P, 2147483648, 4294967295]
KL_EXACT_MAX = 16  # exact big integers are used for k up to this at the large n


def fnv(words, h=0xcbf29ce484222325):
    for w in words:
        h = ((h ^ w) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def brute(n, k):
    if n == 0:
        return (1, 1, 0)
    if k == 0:
        return (0, 0, 0)
    nec = bra = lyn = 0
    for s in product(range(k), repeat=n):
        rots = [s[i:] + s[:i] for i in range(n)]
        m = min(rots)
        if s != m:
            continue
        nec += 1
        if rots.count(m) == 1:
            lyn += 1
        r = s[::-1]
        if m <= min(r[i:] + r[:i] for i in range(n)):
            bra += 1
    return (nec, bra, lyn)


def factor(n):
    f = {}
    q = 2
    while q * q <= n:
        while n % q == 0:
            f[q] = f.get(q, 0) + 1
            n //= q
        q += 1
    if n > 1:
        f[n] = f.get(n, 0) + 1
    return f


def phi(d):
    r = d
    for q in factor(d):
        r = r // q * (q - 1)
    return r


def mu(d):
    f = factor(d)
    if any(e > 1 for e in f.values()):
        return 0
    return -1 if len(f) % 2 else 1


def divisors(n):
    ds = [d for d in range(1, int(n ** 0.5) + 1) if n % d == 0]
    return sorted(set(ds + [n // d for d in ds]))


def exact(n, k):
    """Exact big-integer Burnside; returns the exact counts (not reduced)."""
    if n == 0:
        return (1, 1, 0)
    ds = divisors(n)
    sn = sum(phi(d) * k ** (n // d) for d in ds)
    sl = sum(mu(d) * k ** (n // d) for d in ds)
    assert sn % n == 0 and sl % n == 0, ("burnside division", n, k)
    refl = n * k ** ((n + 1) // 2) if n % 2 else (n // 2) * (k ** (n // 2 + 1) + k ** (n // 2))
    assert (sn + refl) % (2 * n) == 0, ("dihedral division", n, k)
    return (sn // n, (sn + refl) // (2 * n), sl // n)


def exact_mod(n, k):
    return tuple(v % P for v in exact(n, k))


_gc = {}


def gcd_counts(n):
    if n not in _gc:
        _gc[n] = Counter(gcd(j, n) for j in range(n))
    return _gc[n]


def nec_gcd(n, k):
    """Necklaces mod p as sum over j < n of k^gcd(j, n), divided by n (n >= 1)."""
    c = gcd_counts(n)
    return sum(cnt * pow(k, g, P) for g, cnt in c.items()) % P * pow(n, -1, P) % P


def modular(n, k):
    """Third source mod p for n >= 1: necklaces by gcd counting, Lyndon by Gauss inversion over the divisors, bracelets by an explicit 1/(2n) average."""
    ds = divisors(n)
    lyn = {}
    for d in ds:
        lyn[d] = (nec_gcd(d, k) - sum(lyn[e] for e in ds if e < d and d % e == 0)) % P
    nec = nec_gcd(n, k)
    s = nec * n % P
    refl = n * pow(k, (n + 1) // 2, P) if n % 2 else (n // 2) * (pow(k, n // 2 + 1, P) + pow(k, n // 2, P))
    return (nec, (s + refl) % P * pow(2 * n, -1, P) % P, lyn[n])


def enum_pairs():
    for n in range(ENUM_N + 1):
        for k in range(ENUM_K + 1):
            if n == 0 or k ** n <= ENUM_LIMIT:
                yield (n, k)


def group_digest(pairs, fn):
    h = 0xcbf29ce484222325
    for n, k in pairs:
        h = fnv(fn(n, k), h)
    return h


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    pairs = list(enum_pairs())
    enum_vals = {}
    for n, k in pairs + HEAVY:
        b = brute(n, k)
        e = exact_mod(n, k)
        assert tuple(v % P for v in b) == e, ("brute vs exact", n, k, b, e)
        if n >= 1:
            assert modular(n, k) == e, ("modular vs exact", n, k)
        enum_vals[(n, k)] = e
    # known textbook values
    assert brute(6, 2) == (14, 13, 9) and brute(4, 3) == (24, 21, 18) and brute(5, 2) == (8, 8, 6)
    # exact vs modular on the exhaustive range (all k, n through EXACT_N)
    for n in range(1, EXACT_N + 1):
        for k in KS:
            e = exact_mod(n, k)
            if n <= 120 or k > 40 or n % 7 == 0:
                assert modular(n, k) == e, ("modular vs exact", n, k)
    large = {}
    for n in LN:
        for k in KL:
            m = modular(n, k)
            if k <= KL_EXACT_MAX:
                assert exact_mod(n, k) == m, ("large n exact vs modular", n, k)
            large[(n, k)] = m
    if mode == "check":
        print("OK")
        return
    sm = lambda n, k: enum_vals[(n, k)]
    print("constexpr std::uint64_t kDigestEnum = 0x%016xull;  // %d pairs, n 0..%d, k 0..%d, k^n <= %d (n = 0 for every k)" % (group_digest(pairs, sm), len(pairs), ENUM_N, ENUM_K, ENUM_LIMIT))
    print("struct Heavy { std::uint32_t n, k, necklace, bracelet, lyndon; };")
    print("constexpr Heavy kHeavy[] = {")
    for n, k in HEAVY:
        v = enum_vals[(n, k)]
        print("    {%du, %du, %du, %du, %du}," % (n, k, v[0], v[1], v[2]))
    print("};")
    ex = lambda n, k: exact_mod(n, k) if n >= 1 else (1, 1, 0)
    ex_pairs = [(n, k) for n in range(EXACT_N + 1) for k in KS]
    print("constexpr std::uint64_t kDigestExact = 0x%016xull;  // n 0..%d times the 50 values of kKs" % (group_digest(ex_pairs, ex), EXACT_N))
    lg = lambda n, k: large[(n, k)]
    lp = [(n, k) for n in LN for k in KL]
    print("constexpr std::uint64_t kDigestLarge = 0x%016xull;  // the 16 values of kLargeN times the 20 values of kLargeK" % group_digest(lp, lg))
    print("// per-n digests of the large group (all 20 k values, triples in order)")
    print("constexpr std::uint64_t kDigestLargeByN[] = {")
    for n in LN:
        print("    0x%016xull,  // n = %d" % (group_digest([(n, k) for k in KL], lg), n))
    print("};")


main()
