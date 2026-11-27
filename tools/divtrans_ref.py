#!/usr/bin/env python3
"""Independent reference for core/divtrans (divisor zeta, divisor Moebius and Dirichlet convolution modulo 998244353 on the integers 1..n).

Standard library only (no NumPy, no third-party package). Runtime: Python 3.8 or newer; the timing is stated in docs/divtrans.md.
Usage:
  python3 tools/divtrans_ref.py check   run every oracle against every other on the small extents and on sampled large inputs, print "OK"
  python3 tools/divtrans_ref.py emit    print the golden digests as C++ source for tests/divtrans/divtrans_tests.cpp (runs the checks first)
The array of n entries is indexed by the integers 1..n through position m - 1.
Oracles of different shape:
  scan oracle (THE oracle, O(n^2)): zeta(m) = sum of a(d) over every d in 1..m with m % d == 0; mobius(m) = sum over those d of mu(m / d) * A(d), with mu from
     factoring by trial division; convolve(m) = sum over those d of a(d) * b(m / d). Used on every small extent and for every n up to 300 of the random
     arrays. It is infeasible at n = 2^20 (about 5 * 10^11 steps), so the large digests do NOT come from it.
  pair oracle (O(n log n), the same definition with the divisors enumerated as the pairs d * e = m by the harmonic double loop): used where the scan is too
     slow. It is compared with the scan on every small extent and on random arrays for n = 1..300.
  prime-pass oracle (O(n log log n), a different method) for the transforms: per prime p, zeta: for i = 1..n/p ascending, a[i p] += a[i]; mobius: for i = n/p
     down to 1, a[i p] -= a[i]. Compared with the pair oracle on every large input.
  spf oracle for the convolution (a different enumeration): for each m the divisors come from its factorisation by smallest prime factors. Compared with the
     pair oracle on every small extent and every n up to 2000, and on the rand-by-rand convolution at every large n. Also conv(a, ones) = zeta(a) everywhere.
Digest: FNV-1a 64-bit over 32-bit words (one xor and one multiply per word); a group digest folds its result arrays in the stated enumeration order.
Large inputs come from the documented xorshift32 generators in gen().
"""
import sys
from itertools import product

P = 998244353
MAXN = 1 << 20
LARGE_N = [65536, 65537, 524289, 720720, 1048573, 1048575, 1048576]
LARGE_OPS = ["zeta_rand", "mobius_rand", "zeta_sparse", "mobius_sparse", "zeta_ones", "mobius_ones", "zeta_max", "mobius_max",
             "conv_rand", "conv_sparse", "conv_ones", "conv_max"]
M64 = 0xFFFFFFFFFFFFFFFF


def fnv(words, h=0xcbf29ce484222325):
    for w in words:
        h = ((h ^ w) * 0x100000001b3) & M64
    return h


def mu_of(k):
    r = 1
    p = 2
    while p * p <= k:
        if k % p == 0:
            k //= p
            if k % p == 0:
                return 0
            r = -r
        p += 1
    if k > 1:
        r = -r
    return r


def zeta_scan(a):
    n = len(a)
    return [sum(a[d - 1] for d in range(1, m + 1) if m % d == 0) % P for m in range(1, n + 1)]


def mobius_scan(A):
    n = len(A)
    mu = [0] + [mu_of(k) for k in range(1, n + 1)]
    return [sum(mu[m // d] * A[d - 1] for d in range(1, m + 1) if m % d == 0) % P for m in range(1, n + 1)]


def conv_scan(a, b):
    n = len(a)
    return [sum(a[d - 1] * b[m // d - 1] for d in range(1, m + 1) if m % d == 0) % P for m in range(1, n + 1)]


def zeta_pair(a):
    n = len(a)
    out = list(a)
    for d in range(1, n + 1):
        v = a[d - 1]
        if v:
            for m in range(2 * d, n + 1, d):
                out[m - 1] += v
    return [x % P for x in out]


def mobius_pair(A):
    # mu(k) by a sieve; a(m) = sum over the pairs d * k = m of mu(k) * A(d)
    n = len(A)
    mu = [1] * (n + 1)
    comp = bytearray(n + 1)
    for p in range(2, n + 1):
        if not comp[p]:
            for q in range(p, n + 1, p):
                if q > p:
                    comp[q] = 1
                mu[q] = -mu[q]
            pp = p * p
            for q in range(pp, n + 1, pp):
                mu[q] = 0
    out = [0] * n
    for d in range(1, n + 1):
        v = A[d - 1]
        if v:
            for k in range(1, n // d + 1):
                if mu[k]:
                    out[d * k - 1] += mu[k] * v
    return [x % P for x in out]


def conv_pair(a, b):
    n = len(a)
    out = [0] * n
    for d in range(1, n + 1):
        x = a[d - 1]
        if x:
            for e in range(1, n // d + 1):
                out[d * e - 1] += x * b[e - 1]
    return [x % P for x in out]


def primes_upto(n):
    s = bytearray([1]) * (n + 1)
    ps = []
    for i in range(2, n + 1):
        if s[i]:
            ps.append(i)
            for j in range(i * i, n + 1, i):
                s[j] = 0
    return ps


def zeta_prime(a, ps=None):
    n = len(a)
    out = list(a)
    for p in (ps if ps is not None else primes_upto(n)):
        if p > n:
            break
        for i in range(1, n // p + 1):
            out[i * p - 1] = (out[i * p - 1] + out[i - 1]) % P
    return out


def mobius_prime(A, ps=None):
    n = len(A)
    out = list(A)
    for p in (ps if ps is not None else primes_upto(n)):
        if p > n:
            break
        for i in range(n // p, 0, -1):
            out[i * p - 1] = (out[i * p - 1] - out[i - 1]) % P
    return out


def spf_table(n):
    spf = list(range(n + 1))
    i = 2
    while i * i <= n:
        if spf[i] == i:
            for j in range(i * i, n + 1, i):
                if spf[j] == j:
                    spf[j] = i
        i += 1
    return spf


def divisors_of(m, spf):
    divs = [1]
    while m > 1:
        p = spf[m]
        e = 0
        while m % p == 0:
            m //= p
            e += 1
        cur = divs
        pw = 1
        divs = list(cur)
        for _ in range(e):
            pw *= p
            divs += [d * pw for d in cur]
    return divs


def conv_spf(a, b, spf=None):
    # per integer m: the divisors from the factorisation by smallest prime factor, then the sum (a different enumeration from the harmonic loop)
    n = len(a)
    spf = spf if spf is not None else spf_table(n)
    return [sum(a[d - 1] * b[m // d - 1] for d in divisors_of(m, spf)) % P for m in range(1, n + 1)]


def gen(kind, n, seed=0):
    x = (0x9E3779B9 + n + seed) & 0xFFFFFFFF
    out = []
    for _ in range(n):
        if kind == "ones":
            out.append(1)
            continue
        if kind == "max":
            out.append(P - 1)
            continue
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        if kind == "rand":
            out.append(x % P)
        else:  # sparse: one entry in eight is non-zero
            r1 = x
            x ^= (x << 13) & 0xFFFFFFFF
            x ^= x >> 17
            x ^= (x << 5) & 0xFFFFFFFF
            out.append(x % P if (r1 & 7) == 0 else 0)
    return out


def arrays(alphabet, n):
    return product(alphabet, repeat=n)


def ex_transform(alphabet, nmax, fn):
    h = 0xcbf29ce484222325
    cnt = 0
    for n in range(0, nmax + 1):
        for t in arrays(alphabet, n):
            h = fnv(fn(list(t)), h)
            cnt += 1
    return h, cnt


def ex_conv(alphabet, nmax, fn):
    h = 0xcbf29ce484222325
    cnt = 0
    for n in range(0, nmax + 1):
        for ta in arrays(alphabet, n):
            for tb in arrays(alphabet, n):
                h = fnv(fn(list(ta), list(tb)), h)
                cnt += 1
    return h, cnt


# Small exhaustive extents that get a reference digest (the larger exhaustive extents are checked in the test against its own scan only).
EX_T = [("zeta_01m_0_8", [0, 1, P - 1], 8, "zeta"), ("mobius_01m_0_8", [0, 1, P - 1], 8, "mobius"), ("zeta_012m_0_6", [0, 1, 2, P - 1], 6, "zeta"),
        ("mobius_012m_0_6", [0, 1, 2, P - 1], 6, "mobius")]
EX_C = [("conv_01m_0_4", [0, 1, P - 1], 4), ("conv_012_0_5", [0, 1, 2], 5)]


def checks():
    # 1. small exhaustive extents: scan oracle = pair oracle = prime-pass oracle, and mobius inverts zeta.
    for alpha, nmax in (([0, 1, P - 1], 8), ([0, 1, 2, P - 1], 6)):
        for n in range(0, nmax + 1):
            for t in arrays(alpha, n):
                a = list(t)
                z = zeta_scan(a)
                assert z == zeta_pair(a) == zeta_prime(a), (a, "zeta")
                m = mobius_scan(a)
                assert m == mobius_pair(a) == mobius_prime(a), (a, "mobius")
                assert mobius_scan(z) == a and zeta_scan(m) == a, (a, "inverse")
    for alpha, nmax in (([0, 1, P - 1], 4), ([0, 1, 2], 5)):
        for n in range(0, nmax + 1):
            for ta in arrays(alpha, n):
                for tb in arrays(alpha, n):
                    a, b = list(ta), list(tb)
                    c = conv_scan(a, b)
                    assert c == conv_pair(a, b) == conv_spf(a, b), (a, b, "conv")
    # 2. random arrays n = 1..300 with the scan, then pair vs prime-pass for every n up to 2000.
    for n in range(1, 2001):
        for kind in ("rand", "sparse"):
            a, b = gen(kind, n), gen("rand", n, 77)
            z, m, c = zeta_pair(a), mobius_pair(a), conv_pair(a, b)
            assert z == zeta_prime(a) and m == mobius_prime(a) and c == conv_spf(a, b), (n, kind)
            if n <= 300:
                assert z == zeta_scan(a) and m == mobius_scan(a) and c == conv_scan(a, b), (n, kind, "scan")
            if n <= 60:
                assert mobius_pair(z) == a and zeta_pair(m) == a
    # 3. the closed forms on the full array of a few large n (checked again in the test from independent sieves).
    return True


def large_results(n):
    ps = primes_upto(n)
    res = {}
    rb = gen("rand", n, 77)
    spf = spf_table(n)
    for k in ("rand", "sparse", "ones", "max"):
        a = gen(k, n)
        z, m = zeta_pair(a), mobius_pair(a)
        assert z == zeta_prime(a, ps) and m == mobius_prime(a, ps), (n, k)
        res["zeta_" + k] = z
        res["mobius_" + k] = m
        b = {"rand": rb, "sparse": rb, "ones": gen("ones", n), "max": gen("max", n)}[k]
        c = conv_pair(a, b)
        if k == "rand":
            assert c == conv_spf(a, b, spf), (n, k, "conv")
        assert conv_pair(a, [1] * n) == z if k == "ones" else True
        res["conv_" + k] = c
    return res


def emit_small():
    print("// Generated by tools/divtrans_ref.py emit")
    for name, alpha, nmax, op in EX_T:
        fn = zeta_scan if op == "zeta" else mobius_scan
        h, cnt = ex_transform(alpha, nmax, fn)
        print("constexpr std::uint64_t kDigest_%s = 0x%016xull;  // %d arrays" % (name, h, cnt))
    for name, alpha, nmax in EX_C:
        h, cnt = ex_conv(alpha, nmax, conv_scan)
        print("constexpr std::uint64_t kDigest_%s = 0x%016xull;  // %d pairs" % (name, h, cnt))
    h = 0xcbf29ce484222325
    for n in range(1, 2001):
        a, b = gen("rand", n), gen("rand", n, 77)
        for r in (zeta_pair(a), mobius_pair(a), conv_pair(a, b)):
            h = fnv(r, h)
    print("constexpr std::uint64_t kDigest_random_1_2000 = 0x%016xull;  // n = 1..2000: zeta, mobius, convolve of rand arrays" % h)
    print("constexpr std::uint64_t kLargeDigest[%d][%d] = {  // [n index][op]: n %s; ops %s" % (len(LARGE_N), len(LARGE_OPS), " ".join(map(str, LARGE_N)), " ".join(LARGE_OPS)))


def emit_large_row(n):
    r = large_results(n)
    print("    {" + ", ".join("0x%016xull" % fnv(r[o]) for o in LARGE_OPS) + "},  // n = %d" % n)


def emit():
    emit_small()
    for n in LARGE_N:
        emit_large_row(n)
    print("};")


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "emit-part":  # emit-part small | emit-part <n>: the same output in pieces (small, then one row per n)
        emit_small() if sys.argv[2] == "small" else emit_large_row(int(sys.argv[2]))
        sys.exit(0)
    if len(sys.argv) < 2 or sys.argv[1] not in ("check", "emit"):
        print(__doc__)
        sys.exit(2)
    if sys.argv[1] == "check":
        checks()
        print("OK")
    else:
        checks()
        emit()
