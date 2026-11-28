#!/usr/bin/env python3
"""Independent reference for core/divsum (D(n) = the sum of d(k) and S(n) = the sum of sigma(k) over k = 1..n).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/divsum.md.
Usage:
  python3 tools/divsum_ref.py brute       D and S by the definition (d(k) and sigma(k) from the divisors, running sums) against the quotient-block route for every n <= 2^16,
                                           and against the direct sums of floor(n / k) and k * floor(n / k) over all k for every n <= 2^13 and 40 larger n
  python3 tools/divsum_ref.py chunk K     digest of (n, D(n), S(n)) for the n of chunk K (K = 0..7: n in [K * 2^17, (K + 1) * 2^17); chunk 7 also holds 2^20)
  python3 tools/divsum_ref.py large       the large lists: D and S by the block route and, for every entry, the difference certificate (D(n) - D(n - 1) = d(n),
                                           S(n) - S(n - 1) = sigma(n), d and sigma from the factorisation) on a subset; the S(n) = 2^63 threshold
  python3 tools/divsum_ref.py check       all of the above, prints "OK"
  python3 tools/divsum_ref.py emit        print the golden values as C++ source for tests/divsum/divsum_tests.cpp (set DIVSUM_CACHE=file to reuse block-route results)
Methods (all independent of the implementation's source, which uses the hyperbola split at sqrt(n)):
  definition: d(k) and sigma(k) from the divisors of k (a divisor sieve: every multiple m of every d gets +1 and +d), running sums.
  block route: the quotient blocks. For k = 1.. let q = n // k and hi = n // q; every k' in [k, hi] has floor(n / k') = q, so D gets q * (hi - k + 1) and S gets
     q * (T(hi) - T(k - 1)) with T(m) = m (m + 1) / 2; then k = hi + 1. About 2 sqrt(n) blocks.
  difference certificate: D(n) - D(n - 1) = d(n) and S(n) - S(n - 1) = sigma(n), with d and sigma from the prime factorisation of n by trial division over a prime list.
Digest: FNV-1a 64-bit over 64-bit words (n, D(n), S(n) for each n of a chunk in increasing order).
"""
import json
import os
import sys
from math import isqrt

M64 = 0xFFFFFFFFFFFFFFFF
MAXD = 1 << 46
MAXS = 0xFFFFFFFF
CHUNK = 1 << 17
TOP = 1 << 20
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


_PR = None


def primes():
    global _PR
    if _PR is None:
        _PR = primes_upto(1 << 23)
    return _PR


def factor(n):
    f = {}
    for q in primes():
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


def d_of(n):
    r = 1
    for e in factor(n).values():
        r *= e + 1
    return r


def sigma_of(n):
    r = 1
    for p, e in factor(n).items():
        r *= (p ** (e + 1) - 1) // (p - 1)
    return r


def is_prime(n):
    if n < 2:
        return False
    for p in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        if n % p == 0:
            return n == p
    d = n - 1
    s = 0
    while d % 2 == 0:
        d //= 2
        s += 1
    for a in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        x = pow(a, d, n)
        if x in (1, n - 1):
            continue
        for _ in range(s - 1):
            x = x * x % n
            if x == n - 1:
                break
        else:
            return False
    return True


def tri(m):
    return m * (m + 1) // 2


def block_d(n):
    k = 1
    s = 0
    while k <= n:
        q = n // k
        hi = n // q
        s += q * (hi - k + 1)
        k = hi + 1
    return s


def block_s(n):
    k = 1
    s = 0
    while k <= n:
        q = n // k
        hi = n // q
        s += q * (tri(hi) - tri(k - 1))
        k = hi + 1
    return s


_sieve = None


def sieve():
    """d(k) and sigma(k) for k = 0..TOP by the divisor sieve, and the prefix sums D, S."""
    global _sieve
    if _sieve is None:
        dd = [0] * (TOP + 1)
        sg = [0] * (TOP + 1)
        for d in range(1, TOP + 1):
            for m in range(d, TOP + 1, d):
                dd[m] += 1
                sg[m] += d
        D = [0] * (TOP + 1)
        S = [0] * (TOP + 1)
        for k in range(1, TOP + 1):
            D[k] = D[k - 1] + dd[k]
            S[k] = S[k - 1] + sg[k]
        _sieve = (dd, sg, D, S)
    return _sieve


def xorshift(s):
    s ^= (s << 13) & M64
    s ^= s >> 7
    s ^= (s << 17) & M64
    return s


def rng(seed):
    s = [seed]
    def rnd(m):
        s[0] = xorshift(s[0])
        return s[0] % m
    return rnd


def largest_primes_below(x, k):
    out = []
    p = x
    while len(out) < k:
        if is_prime(p):
            out.append(p)
        p -= 1
    return out


def large_d_list():
    L = set()
    for k in range(14):
        L.add(10 ** k)
    for j in range(0, 47):
        for e in (-1, 0, 1):
            v = (1 << j) + e
            if 0 <= v <= MAXD:
                L.add(v)
    L.update((MAXD - 1, MAXD, MAXS, MAXS - 1, 1 << 31, (1 << 31) - 1))
    for r in (1 << 23, (1 << 23) - 1, 1 << 22, (1 << 22) - 1, 3000000, 1 << 20, 65536, 65535, 46341, 100000):
        for v in (r * r - 1, r * r, r * r + r - 1, r * r + r, r * r + r + 1):
            if v <= MAXD:
                L.add(v)
    L.update(largest_primes_below(MAXD, 6))
    L.update(largest_primes_below(MAXS, 6))
    L.update((3491888400, 4227022800, 4219365150, 4132042525))
    pr = 1
    for p in primes()[:20]:
        pr *= p
        if pr <= MAXD:
            L.add(pr)
    rnd = rng(0x7F4A7C159E3779B9)
    for _ in range(200):
        lo = 20 + rnd(27)
        L.add(rnd(1 << lo) + 1 if lo < 47 else 5)
    for _ in range(30):
        L.add(rnd(MAXS) + 1)
    L.discard(0)
    return sorted(v for v in L if v <= MAXD)


def s_threshold():
    """the largest n with S(n) <= 2^63 - 1 (S is increasing), found by bisection with the block route; the next n has S(n) >= 2^63"""
    lo, hi = 1, MAXS
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if block_s(mid) <= (1 << 63) - 1:
            lo = mid
        else:
            hi = mid - 1
    return lo


def large_s_list():
    L = set()
    for k in range(10):
        L.add(10 ** k)
    for j in range(0, 33):
        for e in (-1, 0, 1):
            v = (1 << j) + e
            if 0 <= v <= MAXS:
                L.add(v)
    L.update((MAXS, MAXS - 1, 1 << 31, (1 << 31) - 1))
    for r in (65535, 65534, 46341, 46340, 32768, 30000, 4096, 1000):
        for v in (r * r - 1, r * r, r * r + r - 1, r * r + r, r * r + r + 1):
            if v <= MAXS:
                L.add(v)
    L.update(largest_primes_below(MAXS, 8))
    L.update((3491888400, 4227022800, 4219365150))
    pr = 1
    for p in primes()[:12]:
        pr *= p
        if pr <= MAXS:
            L.add(pr)
    t = s_threshold()
    L.update((t - 1, t, t + 1))
    rnd = rng(0x0123456789ABCDEF)
    for _ in range(150):
        L.add(rnd(MAXS) + 1)
    for _ in range(50):
        L.add(rnd(1 << 24) + 1)
    L.discard(0)
    return sorted(L)


def digest_chunk(k):
    dd, sg, D, S = sieve()
    lo = k * CHUNK
    hi = lo + CHUNK + (1 if k == 7 else 0)
    h = FNV0
    for n in range(lo, hi):
        h = fnv_step(h, n)
        h = fnv_step(h, D[n])
        h = fnv_step(h, S[n])
    return h


def brute():
    N = 1 << 16
    dd = [0] * (N + 1)
    sg = [0] * (N + 1)
    for d in range(1, N + 1):
        for m in range(d, N + 1, d):
            dd[m] += 1
            sg[m] += d
    rd = rs = 0
    for n in range(0, N + 1):
        if n:
            rd += dd[n]
            rs += sg[n]
        assert block_d(n) == rd, ("D", n)
        assert block_s(n) == rs, ("S", n)
    for n in list(range(0, 8193)) + [10 ** 5 + 7 * i for i in range(10)] + [3 * 10 ** 6 + 11 * i for i in range(10)] + [(1 << 22) + i for i in range(10)] + [10 ** 7 + 3 * i for i in range(10)]:
        assert block_d(n) == sum(n // k for k in range(1, n + 1)), ("D direct", n)
        assert block_s(n) == sum(k * (n // k) for k in range(1, n + 1)), ("S direct", n)
    print("brute OK")


_cache = None


def cached(kind, n):
    global _cache
    path = os.environ.get("DIVSUM_CACHE")
    if _cache is None:
        _cache = json.load(open(path)) if path and os.path.exists(path) else {}
    key = "%s%d" % (kind, n)
    if key not in _cache:
        _cache[key] = block_d(n) if kind == "D" else block_s(n)
        if path:
            json.dump(_cache, open(path, "w"))
    return _cache[key]


def cert_d(n):
    return cached("D", n) - cached("D", n - 1) == d_of(n)


def cert_s(n):
    return cached("S", n) - cached("S", n - 1) == sigma_of(n)


def large():
    LD = large_d_list()
    LS = large_s_list()
    for i, n in enumerate(LD):
        cached("D", n)
        if i % 3 == 0:
            assert cert_d(n), ("D certificate", n)
    for i, n in enumerate(LS):
        cached("S", n)
        if i % 3 == 0:
            assert cert_s(n), ("S certificate", n)
    assert cached("D", MAXD) == 2254558383554734 and cached("D", MAXD - 1) == 2254558383554687
    assert cached("D", 10 ** 13) == 300880375389757 and cached("S", MAXS) == 15171838869443902239
    t = s_threshold()
    assert cached("S", t) <= (1 << 63) - 1 < cached("S", t + 1)
    print("large OK D entries", len(LD), "S entries", len(LS), "last n with S <= 2^63 - 1:", t, "S =", cached("S", t), "next S =", cached("S", t + 1))


def emit():
    print("// Generated by tools/divsum_ref.py emit.")
    print("constexpr std::uint64_t kChunkDigest[8] = {" + ", ".join("0x%016xull" % digest_chunk(k) for k in range(8)) + "};")
    LD = large_d_list()
    LS = large_s_list()
    print("struct LargeD { std::uint64_t n; std::uint64_t d; };")
    print("constexpr LargeD kLargeD[%d] = {" % len(LD))
    for n in LD:
        print("    {%dull, %dull}," % (n, cached("D", n)))
    print("};")
    print("struct LargeS { std::uint32_t n; std::uint64_t s; };")
    print("constexpr LargeS kLargeS[%d] = {" % len(LS))
    for n in LS:
        print("    {%du, %dull}," % (n, cached("S", n)))
    print("};")
    print("constexpr std::uint32_t kS63Last = %du;  // the largest n with S(n) <= 2^63 - 1" % s_threshold())


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
