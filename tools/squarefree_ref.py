#!/usr/bin/env python3
"""Independent reference for core/squarefree (mu(n) for every u32 n >= 1 and Q(n), the number of squarefree k with 1 <= k <= n, for every u32 n).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/squarefree.md.
Usage:
  python3 tools/squarefree_ref.py brute      mu by its own factorisation against the definition (no square above 1 divides n, by a scan of d^2 | n) for every n <= 2^16 and against
                                              the divisor sum (sum of mu(d) over d | n is [n = 1]) for every n <= 4000; the recurrence route with a table cut at 64 and the
                                              mu-sum against Q by the definition (flag every k with a square factor, count the rest) for n < 5000 and at 2^j, 10^6
  python3 tools/squarefree_ref.py chunk K    digest of (n, mu(n)) for the n of chunk K (K = 0..7: n in [K * 2^17, (K + 1) * 2^17), from 1; chunk 7 also holds 2^20)
  python3 tools/squarefree_ref.py large      the large lists: mu from the factorisation; Q by the recurrence route AND by the mu-sum, which must agree
  python3 tools/squarefree_ref.py check      all of the above, prints "OK"
  python3 tools/squarefree_ref.py emit       print the golden values as C++ source for tests/squarefree/squarefree_tests.cpp
Methods (independent of the implementation's source):
  mu: own trial division over a prime list; an exponent of 2 or more gives 0, otherwise (-1)^(number of distinct primes); mu(1) = 1.
  Q by the definition: for n <= 2^22, mark every multiple of d^2 for d >= 2 and count the unmarked k.
  Q by the recurrence (uses no mu at all): every k >= 1 is uniquely m^2 s with s squarefree, so the sum over m >= 1 of Q(floor(n / m^2)) equals n, hence
     Q(n) = n - (the sum over m = 2..floor(sqrt(n)) of Q(floor(n / m^2))), solved with memoisation over a table of Q (by the definition) up to 2^22.
  Q by the mu-sum: the sum over d = 1..floor(sqrt(n)) of mu(d) * floor(n / d^2), with mu from the factorisation (a table up to 65535). Same formula as the source, other code.
Digest: FNV-1a 64-bit over 64-bit words (n, mu(n) + 1 for each n of a chunk in increasing order).
"""
import sys
from array import array
from itertools import accumulate
from math import isqrt

M64 = 0xFFFFFFFFFFFFFFFF
MAXN = 0xFFFFFFFF
CHUNK = 1 << 17
FNV0 = 0xcbf29ce484222325
TAB = 1 << 22


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


def mu(n):
    f = factor(n)
    if any(e >= 2 for e in f.values()):
        return 0
    return -1 if len(f) % 2 else 1


_mu_small = None


def mu_small():
    global _mu_small
    if _mu_small is None:
        _mu_small = [0] + [mu(d) for d in range(1, 65536)]
    return _mu_small


_tab = None


def q_table():
    """Q(k) for k = 0..TAB by the definition."""
    global _tab
    if _tab is None:
        flags = bytearray([1]) * (TAB + 1)
        flags[0] = 0
        for d in range(2, isqrt(TAB) + 1):
            flags[d * d::d * d] = bytearray(len(flags[d * d::d * d]))
        _tab = array('I', accumulate(flags))
    return _tab


_memo = {}


def q_rec(n):
    t = q_table()
    if n <= TAB:
        return t[n]
    if n in _memo:
        return _memo[n]
    r = n - sum(q_rec(n // (m * m)) for m in range(2, isqrt(n) + 1))
    _memo[n] = r
    return r


def q_musum(n):
    m = mu_small()
    return sum(m[d] * (n // (d * d)) for d in range(1, isqrt(n) + 1))


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


def mu_list():
    L = set()
    for j in range(33):
        for d in (-1, 0, 1):
            v = (1 << j) + d
            if 1 <= v <= MAXN:
                L.add(v)
    L.add(MAXN); L.add(MAXN - 1); L.add(2147483648); L.add(2147483647)
    top = []
    p = MAXN
    while len(top) < 30:
        if is_prime(p):
            top.append(p)
        p -= 1
    L.update(top)
    L.update(2 * p for p in top[:10] if 2 * p <= MAXN)
    pr = 1
    for p in PR:
        pr *= p
        if pr > MAXN:
            break
        L.add(pr)
        L.add(pr * 2 if pr * 2 <= MAXN else pr)
        L.add(pr + 1 if pr + 1 <= MAXN else pr)
    L.add(223092870)
    near = [p for p in range(65000, 70000) if is_prime(p)]
    for i in range(len(near)):
        for j in range(i, len(near)):
            if near[i] * near[j] <= MAXN and (i + j) % 7 == 0:
                L.add(near[i] * near[j])
    for p in [q for q in PR if q > 65000]:
        L.add(p * p)
        L.add(p * p - 1)
    for p in PR[:60]:
        L.add(p * p)
        L.add(p * p * 3 if p * p * 3 <= MAXN else p * p)
    rnd = rng(0x1234567812345678)
    for _ in range(60):
        m = rnd(1 << 12) + 1
        lim = MAXN // m
        while True:
            p = 65536 + rnd(lim - 65536) if lim > 65537 else 65537
            if is_prime(p):
                break
        L.add(p * m if p * m <= MAXN else p)
    for _ in range(40):
        p = PR[rnd(len(PR))]
        q = PR[rnd(len(PR))]
        if p * p * q <= MAXN:
            L.add(p * p * q)
    for _ in range(600):
        L.add(rnd(MAXN) + 1)
    L.discard(0)
    return sorted(L)


def q_list():
    L = set()
    for k in range(10):
        L.add(10 ** k)
    for j in range(1, 33):
        for d in (-1, 0, 1):
            v = (1 << j) + d
            if 0 <= v <= MAXN:
                L.add(v)
    L.add(MAXN); L.add(MAXN - 1); L.add(2147483648); L.add(2147483647)
    for d in (65535, 65521, 46341, 32768, 4096, 1000, 65536 - 2, 30030, 2310):
        for e in (-1, 0, 1):
            if d * d + e <= MAXN:
                L.add(d * d + e)
    ps = [p for p in PR if p <= 65521]
    for p in ps[-40:]:
        for e in (-1, 0, 1):
            L.add(p * p + e)
    for p in ps[:30]:
        L.add(p * p)
    pr = 1
    for p in PR:
        pr *= p
        if pr * pr > MAXN:
            break
        for e in (-1, 0, 1):
            L.add(pr * pr + e)
    rnd = rng(0x0FEDCBA987654321)
    for _ in range(150):
        L.add(rnd(MAXN) + 1)
    for _ in range(60):
        L.add(rnd(1 << 24) + 1)
    return sorted(L)


def digest_chunk(k):
    lo = max(1, k * CHUNK)
    hi = k * CHUNK + CHUNK + (1 if k == 7 else 0)
    h = FNV0
    for n in range(lo, hi):
        h = fnv_step(h, n)
        h = fnv_step(h, mu(n) + 1)
    return h


def brute():
    N = 1 << 16
    for n in range(1, N + 1):
        sqf = all(n % (d * d) != 0 for d in range(2, isqrt(n) + 1))
        assert (mu(n) != 0) == sqf, ("mu support", n)
    t = q_table()
    # mu itself against the identity: the sum over d | n of mu(d) is 1 for n = 1 and 0 otherwise
    for n in range(1, 4001):
        assert sum(mu(d) for d in range(1, n + 1) if n % d == 0) == (1 if n == 1 else 0), ("mu sum", n)
    # recurrence route (small table) against the definition at n <= 2^22: the recurrence with the table cut at 2^11
    def rec_small(n, cut, memo):
        if n <= cut:
            return t[n]
        if n not in memo:
            memo[n] = n - sum(rec_small(n // (m * m), cut, memo) for m in range(2, isqrt(n) + 1))
        return memo[n]
    memo = {}
    for n in list(range(0, 5000)) + [1 << j for j in range(12, 23)] + [10 ** 6, 4194304 - 1, 4194304]:
        assert rec_small(n, 64, memo) == t[n], ("rec vs def", n)
        assert q_musum(n) == t[n] if n <= 4194304 else True, ("musum vs def", n)
    print("brute OK")


def large():
    ML = mu_list()
    QL = q_list()
    for n in QL:
        a, b = q_rec(n), q_musum(n)
        assert a == b, ("Q routes disagree", n, a, b)
    assert q_rec(10 ** 6) == 607926 and q_rec(10 ** 9) == 607927124
    assert q_rec(2 ** 31) == 1305513583 and q_rec(MAXN) == 2611027094
    print("large OK mu entries", len(ML), "Q entries", len(QL), "Q(10^9) =", q_rec(10 ** 9), "Q(2^31) =", q_rec(2 ** 31), "Q(2^32-1) =", q_rec(MAXN))


def emit():
    print("// Generated by tools/squarefree_ref.py emit.")
    print("constexpr std::uint64_t kMuDigest[8] = {" + ", ".join("0x%016xull" % digest_chunk(k) for k in range(8)) + "};")
    ML = mu_list()
    print("struct LargeMu { std::uint32_t n; std::int32_t mu; };")
    print("constexpr LargeMu kLargeMu[%d] = {" % len(ML))
    for n in ML:
        print("    {%du, %d}," % (n, mu(n)))
    print("};")
    QL = q_list()
    print("struct LargeQ { std::uint32_t n; std::uint64_t q; };")
    print("constexpr LargeQ kLargeQ[%d] = {" % len(QL))
    for n in QL:
        print("    {%du, %dull}," % (n, q_rec(n)))
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
