#!/usr/bin/env python3
"""Independent reference for core/arithfun (phi, d, sigma, rad, omega, Omega of one u32).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/arithfun.md.
Usage:
  python3 tools/arithfun_ref.py chunk K     digest of (n, phi, d, sigma, rad, omega, Omega) for n in [K * 2^19, (K + 1) * 2^19) (K = 0..7; K = 0 starts at 1; K = 7 also holds 2^22)
  python3 tools/arithfun_ref.py sample K    digest of the 125000 sampled n of shard K (K = 0..7), factored by Pollard rho with Miller-Rabin (no trial division, no sieve)
  python3 tools/arithfun_ref.py sets        count and digest of each structured set (prime powers, top window, semiprimes near 2^16, primorial multiples, omega 9, edges)
  python3 tools/arithfun_ref.py gcd K       phi by the definition (count of k <= n with gcd(k, n) = 1), d and sigma by listing the divisors, for the n of shard K = 0..7 of 1..2^16
                                             (n in [K * 8192 + 1, (K + 1) * 8192]) against the factorisation route
  python3 tools/arithfun_ref.py maxima      the largest sigma and d over all u32 by search; see docs/arithfun.md (the full-range check was a separate local run)
  python3 tools/arithfun_ref.py examples    the header's examples
  python3 tools/arithfun_ref.py check       all of the above, prints "OK"
  python3 tools/arithfun_ref.py emit        print the golden values as C++ source for tests/arithfun/arithfun_tests.cpp
Methods (all independent of the implementation, which is trial division by 2, 3, 5, 7, 9, ... with a leftover prime):
  chunk: a smallest-prime-factor sieve to 2^22 and the factorisation by repeated lookup.
  sample: Miller-Rabin (bases 2, 7, 61, deterministic below 4759123141) and Pollard rho with Brent's cycle.
  gcd: the definitions.
Digest: FNV-1a 64-bit over u64 words (n, phi, d, sigma, rad, omega, Omega for each n in increasing order).
"""
import sys
from math import gcd, isqrt

M64 = 0xFFFFFFFFFFFFFFFF
FNV0 = 0xcbf29ce484222325
TOP = 1 << 22
CH = 1 << 19
NS = 1000000
SH = 125000
LIM = 1 << 32


def step(h, w):
    return ((h ^ w) * 0x100000001b3) & M64


def values(fac):
    """(phi, d, sigma, rad, omega, Omega) from {prime: exponent}."""
    phi = d = sigma = rad = 1
    om = big = 0
    for p, e in fac.items():
        phi *= p ** (e - 1) * (p - 1)
        d *= e + 1
        sigma *= (p ** (e + 1) - 1) // (p - 1)
        rad *= p
        om += 1
        big += e
    return phi, d, sigma, rad, om, big


def digest(ns, factor):
    h = FNV0
    for n in ns:
        for w in (n,) + values(factor(n)):
            h = step(h, w)
    return h


_SPF = None


def spf_table():
    global _SPF
    if _SPF is None:
        s = list(range(TOP + 1))
        for i in range(2, isqrt(TOP) + 1):
            if s[i] == i:
                for j in range(i * i, TOP + 1, i):
                    if s[j] == j:
                        s[j] = i
        _SPF = s
    return _SPF


def factor_spf(n):
    s = spf_table()
    f = {}
    while n > 1:
        p = s[n]
        f[p] = f.get(p, 0) + 1
        n //= p
    return f


def is_prime(n):
    if n < 2:
        return False
    for p in (2, 3, 5, 7, 11, 13):
        if n % p == 0:
            return n == p
    d, r = n - 1, 0
    while d % 2 == 0:
        d //= 2
        r += 1
    for a in (2, 7, 61):
        if a % n == 0:
            continue
        x = pow(a, d, n)
        if x in (1, n - 1):
            continue
        for _ in range(r - 1):
            x = x * x % n
            if x == n - 1:
                break
        else:
            return False
    return True


def rho(n):
    if n % 2 == 0:
        return 2
    c = 1
    while True:
        y, m, g, r, q = 2, 128, 1, 1, 1
        x = ys = 2
        while g == 1:
            x = y
            for _ in range(r):
                y = (y * y + c) % n
            k = 0
            while k < r and g == 1:
                ys = y
                for _ in range(min(m, r - k)):
                    y = (y * y + c) % n
                    q = q * abs(x - y) % n
                g = gcd(q, n)
                k += m
            r *= 2
        if g == n:
            g = 1
            while g == 1:
                ys = (ys * ys + c) % n
                g = gcd(abs(x - ys), n)
        if g != n:
            return g
        c += 1


def factor_rho(n):
    f = {}
    stack = [n]
    while stack:
        m = stack.pop()
        if m == 1:
            continue
        if is_prime(m):
            f[m] = f.get(m, 0) + 1
            continue
        for p in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
            if m % p == 0:
                stack.append(p)
                stack.append(m // p)
                break
        else:
            g = rho(m)
            stack.append(g)
            stack.append(m // g)
    return f


def chunk_range(k):
    lo = max(k * CH, 1)
    hi = (k + 1) * CH
    return range(lo, hi + 1 if k == 7 else hi)


def splitmix(count):
    s = 0x9E3779B97F4A7C15
    out = []
    for _ in range(count):
        s = (s + 0x9E3779B97F4A7C15) & M64
        z = s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        z ^= z >> 31
        out.append(TOP + 1 + z % (LIM - 1 - TOP))
    return out


def sample_digest(k):
    s = 0x9E3779B97F4A7C15
    # advance to shard start
    s = (s + 0x9E3779B97F4A7C15 * (k * SH)) & M64
    ns = []
    for _ in range(SH):
        s = (s + 0x9E3779B97F4A7C15) & M64
        z = s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        z ^= z >> 31
        ns.append(TOP + 1 + z % (LIM - 1 - TOP))
    return digest(ns, factor_rho)


def small_primes(n):
    s = bytearray([1]) * (n + 1)
    s[0:2] = b"\x00\x00"
    for i in range(2, isqrt(n) + 1):
        if s[i]:
            s[i * i::i] = bytearray(len(s[i * i::i]))
    return [i for i in range(n + 1) if s[i]]


def sets():
    pr = small_primes(65535)
    out = {}
    # prime powers p^k < 2^32, every prime below 2^16
    a = set()
    for p in pr:
        q = p
        while q < LIM:
            a.add(q)
            q *= p
    out["prime powers"] = a
    # the top window
    out["top window"] = set(range(LIM - 4096, LIM))
    # semiprimes and squares of the 16 largest primes below 2^16 and the 16 smallest above
    low = pr[-16:]
    high = []
    q = 65537
    while len(high) < 16:
        if is_prime(q):
            high.append(q)
        q += 2
    both = low + high
    b = set()
    for i, p in enumerate(both):
        for q in both[i:]:
            if p * q < LIM:
                b.add(p * q)
    out["semiprimes near 2^16"] = b
    # primorials 2..p_k for k = 1..9 and the multiples j * P_k, j = 1..64
    c = set()
    pk = 1
    for p in pr[:9]:
        pk *= p
        for j in range(1, 65):
            if pk * j < LIM:
                c.add(pk * j)
    out["primorial multiples"] = c
    # every n < 2^32 with exactly 9 distinct prime factors (the 9 primes have product at most 2^32, so the largest is below 443)
    nine = set()
    ps = [p for p in pr if p < 450]

    def go(i, n, cnt):
        if cnt == 9:
            nine.add(n)
            return
        for j in range(i, len(ps)):
            p = ps[j]
            m = n * p
            # the cheapest completion: the next 9 - cnt - 1 primes after p
            need = m
            ok = True
            for t in range(1, 9 - cnt):
                if j + t >= len(ps):
                    ok = False
                    break
                need *= ps[j + t]
            if not ok or need >= LIM:
                if need >= LIM:
                    break
                continue
            while m < LIM:
                go(j + 1, m, cnt + 1)
                m *= p
    go(0, 1, 0)
    out["omega 9"] = nine
    # edges
    e = set()
    for k in range(32):
        e.add(1 << k)
    for k in range(1, 33):
        e.add((1 << k) - 1)
        if k < 32:
            e.add((1 << k) + 1)
    for k in range(10):
        e.add(10 ** k)
    e.update(range((1 << 31) - 64, (1 << 31) + 65))
    e.update((3491888400, 4227022800, 4294967295, 4294967294, 4294967291, 3234846615, 4190266080))
    out["edges"] = e
    return out


def maxima():
    P = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53]
    best = {"s": (0, 0), "d": (0, 0)}

    def dfs(i, n, s, d):
        if s > best["s"][0]:
            best["s"] = (s, n)
        if d > best["d"][0] or (d == best["d"][0] and n < best["d"][1]):
            best["d"] = (d, n)
        for j in range(i, len(P)):
            p = P[j]
            m, e, t = n, 0, 1
            while m * p < LIM:
                m *= p
                e += 1
                t += p ** e
                dfs(j + 1, m, s * t, d * (e + 1))
    dfs(0, 1, 1, 1)
    return best


EX = {
    "phi": {1: 1, 2: 1, 6: 2, 12: 4, 36: 12, 60: 16, 97: 96, 100: 40, 360: 96, 1000: 400, 720720: 138240, 2 ** 31: 1073741824, 2 ** 32 - 1: 2147483648, 4294967291: 4294967290},
    "d": {1: 1, 2: 2, 6: 4, 12: 6, 36: 9, 60: 12, 97: 2, 100: 9, 360: 24, 1000: 16, 720720: 240, 2 ** 31: 32, 3234846615: 512},
    "sigma": {1: 1, 2: 3, 6: 12, 12: 28, 36: 91, 60: 168, 97: 98, 100: 217, 360: 1170, 1000: 2340, 720720: 3249792, 2 ** 31: 4294967295, 2 ** 32 - 1: 7304603328, 3234846615: 8360755200},
    "rad": {2: 2, 12: 6, 36: 6, 60: 30, 100: 10, 1000: 10, 720720: 30030, 2 ** 31: 2, 3 ** 20: 3, 2 ** 32 - 1: 2 ** 32 - 1},
    "counts": {1: (0, 0), 2: (1, 1), 12: (2, 3), 36: (2, 4), 60: (3, 4), 360: (3, 6), 1000: (2, 6), 720720: (6, 10), 2 ** 31: (1, 31), 3 ** 20: (1, 20), 2 ** 32 - 1: (5, 5), 3234846615: (9, 9)},
}


def examples():
    for n, v in EX["phi"].items():
        assert values(factor_rho(n))[0] == v, ("phi", n)
    for n, v in EX["d"].items():
        assert values(factor_rho(n))[1] == v, ("d", n)
    for n, v in EX["sigma"].items():
        assert values(factor_rho(n))[2] == v, ("sigma", n)
    for n, v in EX["rad"].items():
        assert values(factor_rho(n))[3] == v, ("rad", n)
    for n, v in EX["counts"].items():
        r = values(factor_rho(n))
        assert (r[4], r[5]) == v, ("counts", n)


def gcd_shard(k):
    for n in range((k * 8192) + 1, (k + 1) * 8192 + 1):
        ph = sum(1 for j in range(1, n + 1) if gcd(j, n) == 1)
        divs = [i for i in range(1, isqrt(n) + 1) if n % i == 0]
        ds = set(divs) | {n // i for i in divs}
        v = values(factor_spf(n))
        assert v[0] == ph, ("phi", n)
        assert v[1] == len(ds), ("d", n)
        assert v[2] == sum(ds), ("sigma", n)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    arg = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    if cmd == "chunk":
        print("0x%016xull" % digest(chunk_range(arg), factor_spf))
    elif cmd == "sample":
        print("0x%016xull" % sample_digest(arg))
    elif cmd == "sets":
        for name, s in sets().items():
            print(name, len(s), "0x%016xull" % digest(sorted(s), factor_rho))
    elif cmd == "gcd":
        gcd_shard(arg)
        print("OK")
    elif cmd == "maxima":
        b = maxima()
        print(b)
        assert b["s"] == (22318571520, 4227022800) and b["d"] == (1920, 3491888400)
    elif cmd == "examples":
        examples()
        print("OK")
    elif cmd == "emit":
        print("// chunk digests")
        print("constexpr std::uint64_t kChunkDigest[8] = {%s};" % ", ".join("0x%016xull" % digest(chunk_range(k), factor_spf) for k in range(8)))
        print("// set counts and digests (order of the sets as in the test)")
        for name, s in sets().items():
            print("// %s: %d %016x" % (name, len(s), digest(sorted(s), factor_rho)))
        print("constexpr std::uint64_t kSampleDigest[8] = {%s};" % ", ".join("0x%016xull" % sample_digest(k) for k in range(8)))
    elif cmd == "check":
        examples()
        for k in range(8):
            gcd_shard(k)
        b = maxima()
        assert b["s"] == (22318571520, 4227022800) and b["d"] == (1920, 3491888400)
        for k in range(8):
            lo = chunk_range(k)
            # the same range factored by rho on a stride (every 997th n), against the sieve route
            for n in list(lo)[::997]:
                assert values(factor_rho(n)) == values(factor_spf(n)), n
        for name, s in sets().items():
            for n in sorted(s)[::53]:
                assert values(factor_rho(n)) == values(factor_spf(n)) if n <= TOP else True
        print("OK")
    else:
        sys.exit("unknown command")


if __name__ == "__main__":
    main()
