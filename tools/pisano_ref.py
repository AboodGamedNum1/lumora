#!/usr/bin/env python3
"""Independent reference for core/pisano (the Pisano period pi(m) for every u32 m >= 1).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/pisano.md.
Usage:
  python3 tools/pisano_ref.py walk        brute force: walk the sequence until (0, 1) returns, every m <= WALK_MAX and the special moduli; compare with the reference
  python3 tools/pisano_ref.py chunk K     digest of pi(m) for the m of chunk K (K = 0..7: m in [K * 2^17, (K + 1) * 2^17), from 1; chunk 7 also holds 2^20)
  python3 tools/pisano_ref.py large       the large-modulus list: reference value and certificate for every entry
  python3 tools/pisano_ref.py check       all of the above, prints "OK"
  python3 tools/pisano_ref.py emit        print the golden values as C++ source for tests/pisano/pisano_tests.cpp (set PISANO_CACHE=file to reuse prime-power results)
Methods (all independent of the implementation's source):
  walk (THE oracle where it runs): F(0), F(1), ... mod m until the pair (0, 1 mod m) returns. Feasible for m <= 4000 and a few special moduli.
  reference route: factor m by trial division (own code); for each prime power p^k ascend through ALL divisors d of N0 * p^(k-1) (N0 = p - 1, 2(p + 1) or 20 by the class of
     p modulo 5) and take the least d with Q^d = I modulo p^k by exact 2 x 2 integer matrix powers; pi(m) is the LCM. N0 * p^(k-1) is a multiple of the order (see
     docs/pisano.md), so the least divisor that gives the identity is the order. No stripping, no doubling.
  certificate (for every large entry, proof without any Pisano theory): n = pi(m) is the order of Q modulo m exactly when Q^n = I and Q^(n / q) != I for every prime q | n.
Digest: FNV-1a 64-bit over 64-bit words (m, pi(m) for each m of a chunk in increasing order).
"""
import json
import os
import sys

M64 = 0xFFFFFFFFFFFFFFFF
MAXM = 0xFFFFFFFF
WALK_MAX = 4000
CHUNK = 1 << 17
SEED = 0x9E3779B97F4A7C15
FNV0 = 0xcbf29ce484222325


def fnv_step(h, w):
    return ((h ^ w) * 0x100000001b3) & M64


def factor(n):
    f = {}
    q = 2
    while q * q <= n:
        while n % q == 0:
            f[q] = f.get(q, 0) + 1
            n //= q
        q += 1 if q == 2 else 2
    if n > 1:
        f[n] = f.get(n, 0) + 1
    return f


def is_prime(n):
    return n >= 2 and factor(n) == {n: 1}


def mat_mul(a, b, m):
    return ((a[0] * b[0] + a[1] * b[2]) % m, (a[0] * b[1] + a[1] * b[3]) % m, (a[2] * b[0] + a[3] * b[2]) % m, (a[2] * b[1] + a[3] * b[3]) % m)


def mat_pow(n, m):
    r = (1 % m, 0, 0, 1 % m)
    b = (1 % m, 1 % m, 1 % m, 0)
    while n:
        if n & 1:
            r = mat_mul(r, b, m)
        b = mat_mul(b, b, m)
        n >>= 1
    return r


def is_identity(n, m):
    return mat_pow(n, m) == (1 % m, 0, 0, 1 % m)


def divisors(f):
    ds = [1]
    for p, e in f.items():
        ds = [d * p ** i for d in ds for i in range(e + 1)]
    return sorted(ds)


_cache = {}


def pi_prime_power(p, k):
    key = (p, k)
    if key in _cache:
        return _cache[key]
    if p == 5:
        n0 = 20
    elif p % 5 in (1, 4):
        n0 = p - 1
    else:
        n0 = 2 * (p + 1)
    f = factor(n0)
    f[p] = f.get(p, 0) + (k - 1)
    if f[p] == 0:
        del f[p]
    pk = p ** k
    res = None
    for d in divisors(f):
        if is_identity(d, pk):
            res = d
            break
    assert res is not None
    _cache[key] = res
    return res


def lcm(a, b):
    from math import gcd
    return a // gcd(a, b) * b


def pi_ref(m):
    r = 1
    for p, k in factor(m).items():
        r = lcm(r, pi_prime_power(p, k))
    return r


def certificate(m, n):
    if not is_identity(n, m):
        return False
    for q in factor(n):
        if is_identity(n // q, m) and n // q >= 1 and m > 1:
            return False
    return True


def walk(m):
    a, b = 0, 1 % m
    n = 0
    while True:
        a, b = b, (a + b) % m
        n += 1
        if a == 0 and b == 1 % m:
            return n


def large_list():
    s = set()
    for j in range(1, 33):
        for d in (-1, 0, 1):
            v = (1 << j) + d
            if 1 <= v <= MAXM:
                s.add(v)
    s.update([MAXM - 1, 1 << 31, 10 ** 6, 10 ** 6 - 1, 10 ** 6 + 1, 999983, 10 ** 9, 10 ** 9 + 7, 1 << 20])
    # the 30 largest u32 primes and the 30 largest of each class mod 5
    cls = {0: [], 1: [], 2: [], 3: [], 4: []}
    allp = []
    v = MAXM
    while v > 2 and (len(allp) < 30 or any(len(cls[c]) < 30 for c in (1, 2, 3, 4))):
        if is_prime(v):
            if len(allp) < 30:
                allp.append(v)
            if len(cls[v % 5]) < 30 and v % 5 != 0:
                cls[v % 5].append(v)
        v -= 1
    s.update(allp)
    for c in (1, 2, 3, 4):
        s.update(cls[c])
    # prime powers
    bigp = [p for p in range(65521, 60000, -1) if is_prime(p)][:60]
    smallp = [p for p in range(2, 200) if is_prime(p)]
    for p in bigp + smallp:
        if p * p <= MAXM:
            s.add(p * p)
    for p in [p for p in range(1625, 1400, -1) if is_prime(p)][:20] + [p for p in range(2, 100) if is_prime(p)]:
        if p ** 3 <= MAXM:
            s.add(p ** 3)
    for p in (2, 3, 5, 7, 11, 13):
        k = 1
        while p ** k <= MAXM:
            s.add(p ** k)
            k += 1
    for k in range(1, 14):
        s.add(2 * 5 ** k)
    for k in range(0, 12):
        if 24 * 5 ** k <= MAXM:
            s.add(24 * 5 ** k)
    a, b = 1, 2
    while b <= MAXM:  # Fibonacci and Lucas numbers
        s.add(b)
        a, b = b, a + b
    a, b = 2, 1
    while b <= MAXM:
        s.add(b)
        a, b = b, a + b
    pr = 1
    for p in (2, 3, 5, 7, 11, 13, 17, 19, 23):
        pr *= p
        s.add(pr)
    s.update([65521 * 65519, 65521 * 65537, 65519 * 65537, 65537 * 65521 * 1, 65521 * 65521, 65519 * 65519, 46337 * 46337])
    x = SEED
    for _ in range(600):
        x ^= (x << 13) & M64
        x ^= x >> 7
        x ^= (x << 17) & M64
        s.add(1 + x % MAXM)
    return sorted(v for v in s if 1 <= v <= MAXM)


def special_walks():
    return [2 * 5 ** k for k in range(1, 9)] + [999983, 10 ** 6, 10 ** 6 + 3, 24 * 5 ** 4, 24 * 5 ** 5, 24 * 5 ** 6]


def load_cache():
    path = os.environ.get("PISANO_CACHE")
    if path and os.path.exists(path):
        for k, v in json.load(open(path)).items():
            p, e = k.split(",")
            _cache[(int(p), int(e))] = v
    return path


def save_cache(path):
    if path:
        json.dump({"%d,%d" % k: v for k, v in _cache.items()}, open(path, "w"))


def cmd_walk():
    for m in range(1, WALK_MAX + 1):
        assert walk(m) == pi_ref(m), m
    for m in special_walks():
        assert walk(m) == pi_ref(m), m
    print("walk: every m <= %d and %d special moduli agree with the reference" % (WALK_MAX, len(special_walks())))


def chunk_range(k):
    lo = max(1, k * CHUNK)
    hi = (k + 1) * CHUNK - 1 if k < 7 else (1 << 20)
    return lo, hi


def cmd_chunk(k):
    lo, hi = chunk_range(k)
    h = FNV0
    for m in range(lo, hi + 1):
        h = fnv_step(fnv_step(h, m), pi_ref(m))
    return h


def cmd_large():
    rows = []
    for m in large_list():
        n = pi_ref(m)
        assert certificate(m, n), m
        rows.append((m, n))
    return rows


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    path = load_cache()
    if cmd == "walk":
        cmd_walk()
    elif cmd == "chunk":
        k = int(sys.argv[2])
        print("chunk %d: digest %016x" % (k, cmd_chunk(k)))
    elif cmd == "large":
        rows = cmd_large()
        print("large: %d moduli, every one passes the certificate; max period %d" % (len(rows), max(r[1] for r in rows)))
    elif cmd == "check":
        cmd_walk()
        for k in range(8):
            print("chunk %d: digest %016x" % (k, cmd_chunk(k)))
        rows = cmd_large()
        print("large: %d moduli, certificate ok" % len(rows))
        print("OK")
    elif cmd == "emit":
        digs = [cmd_chunk(k) for k in range(8)]
        rows = cmd_large()
        print("// generated by tools/pisano_ref.py emit")
        print("constexpr std::uint32_t kChunkSize = %du;" % CHUNK)
        print("constexpr std::uint64_t kChunkDigest[8] = {%s};" % ", ".join("0x%016xull" % d for d in digs))
        print("struct Large { std::uint32_t m; std::uint64_t pi; };")
        print("constexpr Large kLarge[%d] = {" % len(rows))
        print(",\n".join("    {%du, %dull}" % r for r in rows))
        print("};")
    else:
        sys.exit("unknown command")
    save_cache(path)


if __name__ == "__main__":
    main()
