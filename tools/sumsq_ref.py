#!/usr/bin/env python3
"""Independent reference for core/sumsq (r2(n) and r4(n), the numbers of integer solutions of x^2+y^2=n and x^2+y^2+z^2+w^2=n, every u32 n).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/sumsq.md.
Usage:
  python3 tools/sumsq_ref.py brute        brute force against the reference: r2 by the histogram of (x, y) for every n <= 2^16, r4 by the direct 4-variable loop for n <= 60
                                          and by the convolution of the r2 histogram for every n <= 3000
  python3 tools/sumsq_ref.py chunk K      digest of (n, r2, r4) for the n of chunk K (K = 0..7: n in [K * 2^17, (K + 1) * 2^17); chunk 7 also holds 2^20)
  python3 tools/sumsq_ref.py large        the large list: the reference value of every entry; r2 also by the x-loop (isqrt) for every entry; prints the maxima
  python3 tools/sumsq_ref.py check        all of the above, prints "OK"
  python3 tools/sumsq_ref.py emit         print the golden values as C++ source for tests/sumsq/sumsq_tests.cpp
Reference route (independent of the implementation's source, which multiplies per prime power): factor n by trial division (own code over a prime list), enumerate ALL
  divisors, then r2(n) = 4 (d1(n) - d3(n)) (the divisors of n that are 1 and 3 modulo 4) and r4(n) = 8 * the sum of the divisors of n not divisible by 4. r2(0) = r4(0) = 1.
  These are the classical two-square and Jacobi four-square theorems; they are checked here and in the tests, not proved.
Digest: FNV-1a 64-bit over 64-bit words (n, r2(n), r4(n) for each n of a chunk in increasing order).
"""
import sys
from math import isqrt

M64 = 0xFFFFFFFFFFFFFFFF
MAXN = 0xFFFFFFFF
CHUNK = 1 << 17
FNV0 = 0xcbf29ce484222325


def fnv_step(h, w):
    return ((h ^ w) * 0x100000001b3) & M64


def primes_upto(n):
    s = bytearray([1]) * (n + 1)
    s[0:2] = b"\x00\x00"
    for i in range(2, isqrt(n) + 1):
        if s[i]:
            s[i * i::i] = bytearray(len(s[i * i::i]))
    return [i for i in range(n + 1) if s[i]]


PR = primes_upto(65536)


def factor(n):
    f = {}
    for q in PR:
        if q * q > n:
            break
        if n % q == 0:
            e = 0
            while n % q == 0:
                n //= q
                e += 1
            f[q] = e
    if n > 1:
        f[n] = f.get(n, 0) + 1
    return f


def is_prime(n):
    return n >= 2 and factor(n) == {n: 1}


def divisors(n):
    ds = [1]
    for p, e in factor(n).items():
        ds = [d * p ** i for d in ds for i in range(e + 1)]
    return ds


def ref(n):
    if n == 0:
        return 1, 1
    ds = divisors(n)
    r2 = 4 * (sum(1 for d in ds if d % 4 == 1) - sum(1 for d in ds if d % 4 == 3))
    r4 = 8 * sum(d for d in ds if d % 4 != 0)
    return r2, r4


def xloop_r2(n):
    c = 0
    for x in range(-isqrt(n), isqrt(n) + 1):
        y2 = n - x * x
        y = isqrt(y2)
        if y * y == y2:
            c += 1 if y == 0 else 2
    return c


def xorshift(s):
    s ^= (s << 13) & M64
    s ^= s >> 7
    s ^= (s << 17) & M64
    return s


def large_list():
    L = set()
    for j in range(33):
        for d in (-1, 0, 1):
            v = (1 << j) + d
            if 0 <= v <= MAXN:
                L.add(v)
    L.add(MAXN - 1)
    for v in (3491888400, 4227022800, 4132042525, 4219365150, 4294967295, 4294967294):
        L.add(v)
    for d in range(-4, 5):
        L.add((1 << 31) + d)
        L.add((1 << 30) + d)
    top = []
    p = MAXN
    while len(top) < 120:
        if is_prime(p):
            top.append(p)
        p -= 1
    c1 = [p for p in top if p % 4 == 1][:30]
    c3 = [p for p in top if p % 4 == 3][:30]
    L.update(top[:30]); L.update(c1); L.update(c3)
    p = (1 << 31) - 1
    k = 0
    while k < 30:
        if is_prime(p):
            L.add(2 * p)
            k += 1
        p -= 1
    sm = [p for p in PR if p <= 65535]
    for p in sm[-30:]:
        L.add(p * p)
    for p in sm[:50]:
        L.add(p * p)
    for p in sm:
        if p ** 3 > MAXN:
            break
        L.add(p ** 3)
        if p ** 5 <= MAXN:
            L.add(p ** 5)
    for b in (2, 3, 5, 7, 11, 13, 17, 29):
        v = 1
        while v <= MAXN:
            L.add(v)
            v *= b
    pr = 1
    for p in PR:
        pr *= p
        if pr > MAXN:
            break
        L.add(pr)
    pr = 1
    for p in [q for q in PR if q % 4 == 1]:
        pr *= p
        if pr > MAXN:
            break
        L.add(pr)
        L.add(pr * pr if pr * pr <= MAXN else pr)
    pr = 1
    for p in [q for q in PR if q % 4 == 3]:
        pr *= p
        if pr > MAXN:
            break
        L.add(pr)
        L.add(pr * pr if pr * pr <= MAXN else pr)
        if 4 * pr <= MAXN:
            L.add(4 * pr)
    s = 0x123456789ABCDEF1
    def rnd(m):
        nonlocal s
        s = xorshift(s)
        return s % m
    for _ in range(40):
        a, b = rnd(46341), rnd(46341)
        if a * a + b * b <= MAXN:
            L.add(a * a + b * b)
    for _ in range(40):
        m = 2 * rnd(1 << 26) + 1
        for a in range(5):
            L.add((1 << a) * m)
    for _ in range(40):
        L.add(8 * rnd(1 << 29) + 3)
        L.add(8 * rnd(1 << 29) + 7)
    for _ in range(60):
        m = rnd(1 << 12) + 1
        lim = MAXN // m
        while True:
            p = 65536 + rnd(lim - 65536) if lim > 65537 else 65537
            if is_prime(p):
                break
        L.add(p * m if p * m <= MAXN else p)
    for _ in range(600):
        L.add(rnd(MAXN) + 1)
    L.discard(0)
    return sorted(L)


def digest_chunk(k):
    lo = k * CHUNK
    hi = lo + CHUNK + (1 if k == 7 else 0)
    h = FNV0
    for n in range(lo, hi):
        a, b = ref(n)
        h = fnv_step(h, n); h = fnv_step(h, a); h = fnv_step(h, b)
    return h


def brute():
    N = 1 << 16
    h = [0] * (N + 1)
    r = isqrt(N)
    for x in range(-r, r + 1):
        for y in range(-r, r + 1):
            v = x * x + y * y
            if v <= N:
                h[v] += 1
    for n in range(N + 1):
        assert ref(n)[0] == h[n], ("r2", n)
    for n in range(61):
        c = 0
        for x in range(-8, 9):
            for y in range(-8, 9):
                for z in range(-8, 9):
                    for w in range(-8, 9):
                        if x * x + y * y + z * z + w * w == n:
                            c += 1
        assert ref(n)[1] == c, ("r4 direct", n)
    for n in range(3001):
        assert ref(n)[1] == sum(h[k] * h[n - k] for k in range(n + 1)), ("r4 conv", n)
    print("brute OK")


def large():
    L = large_list()
    mx2 = mx4 = (0, 0)
    for i, n in enumerate(L):
        a, b = ref(n)
        if True:
            assert xloop_r2(n) == a, ("xloop", n)
        mx2 = max(mx2, (a, n)); mx4 = max(mx4, (b, n))
    print("large OK", len(L), "r2 max", mx2, "r4 max", mx4)
    return L


def emit():
    print("// Generated by tools/sumsq_ref.py emit.")
    print("constexpr std::uint32_t kChunkSize = 131072u;")
    print("constexpr std::uint64_t kChunkDigest[8] = {" + ", ".join("0x%016xull" % digest_chunk(k) for k in range(8)) + "};")
    L = large_list()
    print("struct Large { std::uint32_t n; std::uint64_t r2; std::uint64_t r4; };")
    print("constexpr Large kLarge[%d] = {" % len(L))
    for n in L:
        a, b = ref(n)
        print("    {%du, %dull, %dull}," % (n, a, b))
    print("};")


if __name__ == "__main__":
    c = sys.argv[1] if len(sys.argv) > 1 else "check"
    if c == "brute": brute()
    elif c == "chunk": print("chunk %s digest 0x%016x" % (sys.argv[2], digest_chunk(int(sys.argv[2]))))
    elif c == "large": large()
    elif c == "emit": emit()
    elif c == "check":
        brute(); large()
        for k in range(8): print("chunk", k, "0x%016x" % digest_chunk(k))
        print("OK")
    else: sys.exit("usage")
