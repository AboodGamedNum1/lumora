#!/usr/bin/env python3
"""Independent reference for core/jacobi (Python standard library only; no code shared with the library).

kronecker: by factoring n (trial division for small n, Miller-Rabin and Pollard rho for large n) and the Legendre symbol by the Euler criterion pow(a, (p-1)/2, p);
sqrt_mod_prime: primality by Miller-Rabin, the square root by a brute-force residue table for p below 8192 and by Cipolla's algorithm above, the least root min(x, p - x).
Digests are FNV-1a over 64-bit words (fold(h, w) = (h ^ w) * 0x100000001b3 mod 2^64), the same fold as the test.

Commands: check, kron1, kron2, komp, kron3 K (K = 0..3), sq1a, sq1b, sq2, sq3 K (K = 0..3), lists, emit_part N.
"""
import sys
from math import gcd

M64 = (1 << 64) - 1
FNV0 = 0xCBF29CE484222325
OK, NULLARG, BADMOD, NOTPRIME, NOROOT = 0, 1, 2, 3, 4
I64MIN = -(1 << 63)
I64MAX = (1 << 63) - 1


def fold(h, w):
    return ((h ^ (w & M64)) * 0x100000001B3) & M64


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


SMALLP = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]


def is_prime(n):
    if n < 2:
        return False
    for p in SMALLP:
        if n % p == 0:
            return n == p
    d = n - 1
    s = 0
    while d % 2 == 0:
        d //= 2
        s += 1
    for a in (2, 3, 5, 7, 11, 13, 17):
        x = pow(a, d, n)
        if x == 1 or x == n - 1:
            continue
        for _ in range(s - 1):
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
        x = y = 2
        d = 1
        while d == 1:
            x = (x * x + c) % n
            y = (y * y + c) % n
            y = (y * y + c) % n
            d = gcd(abs(x - y), n)
        if d != n:
            return d
        c += 1


def factor(n):
    out = []
    for p in SMALLP:
        while n % p == 0:
            out.append(p)
            n //= p
    stack = [n] if n > 1 else []
    while stack:
        m = stack.pop()
        if m == 1:
            continue
        if is_prime(m):
            out.append(m)
            continue
        d = rho(m)
        stack += [d, m // d]
    return sorted(out)


def leg(a, p):
    a %= p
    if a == 0:
        return 0
    return 1 if pow(a, (p - 1) // 2, p) == 1 else -1


def kron(a, n):
    if n == 0:
        return 1 if a in (1, -1) else 0
    r = 1
    for p in factor(n):
        if p == 2:
            if a % 2 == 0:
                return 0
            r *= 1 if a % 8 in (1, 7) else -1
        else:
            r *= leg(a, p)
            if r == 0:
                return 0
    return r


def code_k(v):
    return v + 1


def kron_fast_table(nmax):
    """per n: list of prime factors; per odd prime: residue table of the Legendre symbol (for the exhaustive sweep)"""
    tabs = {}
    facs = {}
    for n in range(2, nmax + 1):
        f = factor(n)
        facs[n] = f
        for p in f:
            if p > 2 and p not in tabs:
                t = [-1] * p
                t[0] = 0
                for x in range(1, p):
                    t[x * x % p] = 1
                tabs[p] = t
    return facs, tabs


def d_kron1():
    amax, nmax = 1200, 1200
    facs, tabs = kron_fast_table(nmax)
    h = FNV0
    cnt = 0
    for a in range(-amax, amax + 1):
        for n in range(0, nmax + 1):
            if n == 0:
                v = 1 if a in (1, -1) else 0
            elif n == 1:
                v = 1
            else:
                v = 1
                for p in facs[n]:
                    if p == 2:
                        if a % 2 == 0:
                            v = 0
                            break
                        v *= 1 if a % 8 in (1, 7) else -1
                    else:
                        v *= tabs[p][a % p]
                        if v == 0:
                            break
            h = fold(fold(fold(h, a), n), v + 1)
            cnt += 1
    return h, cnt


K2_A = ([I64MIN, I64MIN + 1, -(1 << 62), -(1 << 32) - 1, -(1 << 32), -(1 << 31) - 1, -(1 << 31)] + list(range(-65537, 0, 4099)) + [-1, 0, 1] + list(range(2, 65538, 4099)) +
        [(1 << 31) - 1, 1 << 31, (1 << 32) - 1, 1 << 32, 1 << 40, 1 << 62, I64MAX - 1, I64MAX, 3 * (1 << 30), -3 * (1 << 30), 4294967291, -4294967291, 999999999999, -999999999999])
K2_N = [0, 1, 2, 3, 4, 5, 8, 9, 15, 16, 255, 256, 65535, 65536, 65537, (1 << 31) - 1, 1 << 31, (1 << 31) + 1, (1 << 32) - 2, (1 << 32) - 1, 4294967291, 65521 * 65521,
        3 * (1 << 30), 2 * 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23, 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23 * 29 // 3, 1 << 30, (1 << 30) + 1, 3 * 2 ** 20, 2 ** 20 * 4095 + 1, 4294967281, 4294967279]


def d_kron2():
    h = FNV0
    cnt = 0
    for n in K2_N:
        for a in K2_A:
            h = fold(fold(fold(h, a), n), kron(a, n) + 1)
            cnt += 1
    return h, cnt


def comp_lists(limit=10 ** 7):
    """base-2 Fermat pseudoprimes and Carmichael numbers below limit, by a smallest-prime-factor sieve and Korselt's criterion"""
    from array import array
    spf = array("I", [0]) * (limit + 1)
    for i in range(2, limit + 1):
        if spf[i] == 0:
            for j in range(i, limit + 1, i):
                if spf[j] == 0:
                    spf[j] = i
    psp = []
    carm = []
    for n in range(3, limit + 1, 2):
        if spf[n] == n:
            continue
        if pow(2, n - 1, n) == 1:
            psp.append(n)
        # Korselt: squarefree, at least 3 factors, p - 1 | n - 1 for every factor
        m = n
        ok = True
        k = 0
        while m > 1:
            p = spf[m]
            m //= p
            if m % p == 0:
                ok = False
                break
            if (n - 1) % (p - 1) != 0:
                ok = False
                break
            k += 1
        if ok and k >= 3:
            carm.append(n)
    return psp, carm


EXTRA_COMP = [3215031751, 65521 * 65521, 65521 * 65519, 65537 * 65519, 65521 * 65497, 4294967295, 4294967293 - 2, 2 * 65521, 3 * 1431655765, 641 * 6700417 % (1 << 32) if False else 641 * 6700417]


def comp_all():
    psp, carm = comp_lists()
    big_carm = [n for n in (1 << 32) and []]
    s = sorted(set(psp[:400] + psp[-100:] + carm + [x for x in EXTRA_COMP if x < (1 << 32) and not is_prime(x)]))
    return psp, carm, s


KOMP_A = [-1, 0, 1, 2, 3, -2, 5, 7, 11, 12345, -12345, 1 << 40, -(1 << 40), I64MIN, I64MAX]


def d_komp():
    psp, carm, s = comp_all()
    h = FNV0
    cnt = 0
    for n in s:
        for a in KOMP_A:
            h = fold(fold(fold(h, a), n), kron(a, n) + 1)
            cnt += 1
    return h, cnt


def kcase(p):
    w0 = splitmix_at(p)
    w1 = splitmix_at(p + 1)
    bits = 1 + (w0 % 63)
    mag = w1 >> (64 - bits)
    a = -mag if (w0 >> 8) & 1 else mag
    if (w0 >> 20) % 64 == 0:
        a = I64MIN
    nb = 1 + ((w0 >> 12) % 32)
    n = w1 & ((1 << nb) - 1)
    t = (w0 >> 24) & 3
    if t == 0:
        n |= 1
    elif t == 1:
        n &= ~1
    elif t == 2:
        n = ((n | 1) << ((w0 >> 30) % 8)) & 0xFFFFFFFF
    return a, n


def d_kron3(s):
    h = FNV0
    for i in range(50000):
        a, n = kcase((1 << 40) + s * 100000 + 2 * i)
        h = fold(fold(fold(h, a), n), kron(a, n) + 1)
    return h, 50000


# ---- square roots ----
def p_next_prime(c):
    while not is_prime(c):
        c += 2
    return c


def r_sqrt(a, p):
    if not is_prime(p):
        return NOTPRIME, 0
    r = a % p
    if p == 2:
        return OK, r
    if r == 0:
        return OK, 0
    if pow(r, (p - 1) // 2, p) != 1:
        return NOROOT, 0
    if p < 8192:
        for x in range(p):
            if x * x % p == r:
                return OK, x
    if p % 4 == 3:
        x = pow(r, (p + 1) // 4, p)
        return OK, min(x, p - x)
    t = 0
    while True:
        t += 1
        w = (t * t - r) % p
        if w == 0:
            return OK, min(t, p - t)
        if pow(w, (p - 1) // 2, p) == p - 1:
            break
    def mul(x, y):
        return ((x[0] * y[0] + x[1] * y[1] * w) % p, (x[0] * y[1] + x[1] * y[0]) % p)
    res = (1, 0)
    b = (t, 1)
    e = (p + 1) // 2
    while e:
        if e & 1:
            res = mul(res, b)
        b = mul(b, b)
        e >>= 1
    assert res[1] == 0 and res[0] * res[0] % p == r
    return OK, min(res[0], p - res[0])


class Brute:
    """brute-force table of the least root of every residue modulo p (p < 8192)"""
    def __init__(self):
        self.cache = {}

    def get(self, p):
        t = self.cache.get(p)
        if t is None:
            t = [-1] * p
            for x in range(p - 1, -1, -1):
                t[x * x % p] = x
            if len(self.cache) > 4:
                self.cache.clear()
            self.cache[p] = t
        return t


def d_sq1a():
    h = FNV0
    cnt = 0
    br = Brute()
    for p in range(2000):
        if not is_prime(p):
            for a in (0, 1, -1, 2, p, p + 1, 12345, I64MIN):
                h = fold(fold(fold(h, a), p), NOTPRIME)
                cnt += 1
            continue
        t = br.get(p)
        for a in range(-p, 2 * p):
            x = t[a % p]
            if x < 0:
                code, v = NOROOT, 0
            else:
                code, v = OK, x
            h = fold(fold(fold(fold(h, a), p), code), v)
            cnt += 1
    return h, cnt


def d_sq1b():
    h = FNV0
    cnt = 0
    br = Brute()
    for p in range(2000, 8000):
        if not is_prime(p):
            continue
        t = br.get(p)
        for a in range(p):
            x = t[a]
            if x < 0:
                code, v = NOROOT, 0
            else:
                code, v = OK, x
            h = fold(fold(fold(fold(h, a), p), code), v)
            cnt += 1
    return h, cnt


def primes_just_below(n, k):
    out = []
    c = n - 1
    while len(out) < k:
        if is_prime(c):
            out.append(c)
        c -= 1
    return out


def primes_kpow2():
    """primes k 2^s + 1, s >= 20, below 2^32"""
    out = []
    for s in range(20, 32):
        for k in range(1, (1 << (32 - s))):
            p = k * (1 << s) + 1
            if p < (1 << 32) and is_prime(p):
                out.append(p)
    return sorted(set(out))


def prime_lists():
    return {
        "Kpow": primes_kpow2(),
        "Top32": primes_just_below(1 << 32, 40),
        "Top31": primes_just_below(1 << 31, 20),
        "Top30": primes_just_below(1 << 30, 20),
        "Top16": primes_just_below(1 << 16, 20),
    }


def sq2_a_list(p):
    out = [0, 1, -1, 2, p - 1, p, p + 1, I64MIN, I64MAX]
    out += [x * x for x in range(1, 41)]
    out += [-(x * x) for x in range(1, 9)]
    for i in range(300):
        out.append(splitmix_at((1 << 42) + p * 7 + i) & ((1 << 40) - 1))
    return out


def d_sq2():
    L = prime_lists()
    allp = sorted(set(sum(L.values(), [])))
    h = FNV0
    cnt = 0
    for p in allp:
        for a in sq2_a_list(p):
            code, v = r_sqrt(a, p)
            h = fold(fold(fold(fold(h, a), p), code), v)
            cnt += 1
    return h, cnt


def scase(pos):
    w0 = splitmix_at(pos)
    w1 = splitmix_at(pos + 1)
    bits = 2 + (w0 % 31)
    cand = (w1 >> (64 - bits)) | (1 << (bits - 1)) | 1
    p = cand
    while p < (1 << 32) and not is_prime(p):
        p += 2
    if p >= (1 << 32):
        p = 4294967291
    w2 = splitmix_at(pos + 2)
    mb = 1 + ((w0 >> 8) % 64)
    mag = w2 >> (64 - mb)
    if mag > I64MAX:
        mag = I64MAX
    a = -mag if (w0 >> 16) & 1 else mag
    t = (w0 >> 24) & 7
    if t == 0:
        a = I64MIN
    elif t == 1:
        # force a square residue: a = x^2 mod 2^40 reduced, so that roots exist often
        x = w2 & 0xFFFFFFFF
        a = x * x % p
    return a, p


def d_sq3(s):
    h = FNV0
    for i in range(50000):
        a, p = scase((1 << 43) + s * 150000 + 3 * i)
        code, v = r_sqrt(a, p)
        h = fold(fold(fold(fold(h, a), p), code), v)
    return h, 50000


def c_check():
    assert kron(2 ** 63 - 1, 4294967295) == -1
    # reciprocity and multiplicativity sanity on small values
    for n in range(1, 60, 2):
        for a in range(-40, 40):
            for b in range(-5, 5):
                assert kron(a * b, n) == kron(a, n) * kron(b, n)
    for p in (3, 5, 7, 13, 17, 65537, 998244353, 1000000007, 4294967291):
        for a in (1, 2, 3, 5, 7, -1):
            c, x = r_sqrt(a, p)
            if c == OK:
                assert x * x % p == a % p and x <= p - x
            else:
                assert leg(a, p) == -1
    print("check ok")


def emit_cpp_array(name, vals, typ="u32"):
    body = ", ".join("%du" % v for v in vals)
    return "constexpr %s %s[%d] = {%s};" % (typ, name, len(vals), body)


def put(out, name, hc):
    h, c = hc
    out.append("constexpr u64 k%sDigest = 0x%016xull;" % (name, h))
    out.append("constexpr u64 k%sCount = %du;" % (name, c))


def emit_part(n):
    out = []
    if n == 0:
        put(out, "Kron1", d_kron1())
        put(out, "Kron2", d_kron2())
    elif n == 1:
        put(out, "Komp", d_komp())
        psp, carm, s = comp_all()
        out.append(emit_cpp_array("kComposites", s))
        out.append("constexpr u32 kNumComposites = %d;" % len(s))
        L = prime_lists()
        allp = sorted(set(sum(L.values(), [])))
        out.append(emit_cpp_array("kBigPrimes", allp))
        out.append("constexpr u32 kNumBigPrimes = %d;" % len(allp))
    elif n == 2:
        for s in range(4):
            put(out, "KronS%d" % s, d_kron3(s))
    elif n == 3:
        put(out, "Sq1a", d_sq1a())
        put(out, "Sq1b", d_sq1b())
        put(out, "Sq2", d_sq2())
    elif n == 4:
        for s in range(4):
            put(out, "SqS%d" % s, d_sq3(s))
    print("\n".join(out))


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == "check":
        c_check()
    elif a[0] == "kron1":
        print(d_kron1())
    elif a[0] == "kron2":
        print(d_kron2())
    elif a[0] == "komp":
        print(d_komp())
    elif a[0] == "kron3":
        print(d_kron3(int(a[1])))
    elif a[0] == "sq1a":
        print(d_sq1a())
    elif a[0] == "sq1b":
        print(d_sq1b())
    elif a[0] == "sq2":
        print(d_sq2())
    elif a[0] == "sq3":
        print(d_sq3(int(a[1])))
    elif a[0] == "lists":
        psp, carm, s = comp_all()
        print(len(psp), len(carm), len(s))
        L = prime_lists()
        print({k: len(v) for k, v in L.items()})
    elif a[0] == "emit_part":
        emit_part(int(a[1]))


main()
