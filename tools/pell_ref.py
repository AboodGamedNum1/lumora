#!/usr/bin/env python3
"""Independent reference for core/pell (smallest solution of x^2 - d y^2 = 1 modulo 998244353, and the continued fraction period of sqrt(d)).

Standard library only. Runtime: Python 3.8 or newer; the timings are stated in docs/pell.md.
Usage:
  python3 tools/pell_ref.py exact          exact big-integer part, d = 2..EXACT_MAX and the targeted list (prints a summary and "OK")
  python3 tools/pell_ref.py chunk K        modular oracle over the non-square d of chunk K (d in [K * 2^17, (K + 1) * 2^17), K = 0..7), prints its digest and maxima
  python3 tools/pell_ref.py verify K S    exact oracle against the modular one on slice S (0..3) of chunk K, every non-square d in it; prints "OK"
  python3 tools/pell_ref.py check          exact part plus every chunk (about 10 minutes), prints "OK"
  python3 tools/pell_ref.py emit           print the golden values as C++ source for tests/pell/pell_tests.cpp (runs everything; set PELL_CACHE=file to reuse chunk results)
Two oracles, labelled as such:
  EXACT (Python integers, no reduction): the convergents p_n / q_n of sqrt(d) are built exactly; the fundamental solution is the FIRST convergent with
     p^2 - d q^2 = 1 (the smallest solution is a convergent: a definition that does not use the period rule). The period is found separately, by the first
     return of the state (m, dd) of the expansion to its value after the first step. The residues are then taken. Run for every non-square d <= EXACT_MAX
     and for a targeted list of large d (longest period below 2^20: d = 1018879), and by `verify` on every d of the domain (the largest solution has
     about 1700 digits, so this is feasible in Python).
  MODULAR (all non-square d up to 2^20, chunked, compared with EXACT on every d): period by the first return of the state (m, dd), the solution by the 2 x 2 matrix product of the quotient
     matrices [[a, 1], [1, 0]] (not by the implementation's stopping rule, which is a == 2 * a0), and the norm identity x^2 - d y^2 = 1 (mod p) checked for
     every d. It is compared with the exact oracle wherever both run.
Digest: FNV-1a 64-bit over 32-bit words (d, x, y, period for every non-square d of a chunk, in increasing order).
"""
import os
import sys
import time
from math import isqrt

P = 998244353
M64 = 0xFFFFFFFFFFFFFFFF
MAXD = 1 << 20
CHUNK = 1 << 17
EXACT_MAX = 30000
SEED = 0x9E3779B97F4A7C15


def fnv_step(h, w):
    return ((h ^ w) * 0x100000001b3) & M64


FNV0 = 0xcbf29ce484222325


def is_square(d):
    r = isqrt(d)
    return r * r == d


def exact(d):
    """(x, y, period) with exact x, y. Independent of the a == 2 a0 rule."""
    a0 = isqrt(d)
    m, dd, a = 0, 1, a0
    pm2, pm1, qm2, qm1 = 0, 1, 1, 0
    # period: first return of (m, dd) after the first step
    m1 = a0
    d1 = d - a0 * a0
    mm, dn = m1, d1
    per = 0
    while True:
        q = (a0 + mm) // dn
        mm = q * dn - mm
        dn = (d - mm * mm) // dn
        per += 1
        if mm == m1 and dn == d1:
            break
    sol = None
    for n in range(0, 2 * per + 2):
        p = a * pm1 + pm2
        q = a * qm1 + qm2
        pm2, pm1, qm2, qm1 = pm1, p, qm1, q
        if p * p - d * q * q == 1:
            sol = (p, q, n)
            break
        m = a * dd - m
        dd = (d - m * m) // dd
        a = (a0 + m) // dd
    assert sol is not None
    x, y, n = sol
    assert n == (per - 1 if per % 2 == 0 else 2 * per - 1), (d, n, per)
    return x, y, per


def modular(d):
    """(x, y, period, max m, max dd, max a) mod P by quotient matrices and the first return of the state; odd period: the second period is run explicitly."""
    a0 = isqrt(d)
    m = a0
    dd = d - a0 * a0
    m1, d1 = m, dd
    qs = []
    mx_m, mx_dd, mx_a = m, dd, 0
    while True:
        a = (a0 + m) // dd
        qs.append(a)
        m = a * dd - m
        dd = (d - m * m) // dd
        if m > mx_m:
            mx_m = m
        if dd > mx_dd:
            mx_dd = dd
        if a > mx_a:
            mx_a = a
        if m == m1 and dd == d1:
            break
    per = len(qs)
    p1, p2, q1, q2 = a0 % P, 1, 1, 0
    seq = qs[:-1] if per % 2 == 0 else qs + qs[:-1]
    for a in seq:
        p1, p2 = (a * p1 + p2) % P, p1
        q1, q2 = (a * q1 + q2) % P, q1
    assert (p1 * p1 - d * q1 * q1) % P == 1, d  # the norm identity mod p
    return p1, q1, per, mx_m, mx_dd, mx_a


def targets():
    ds = {2, 3, 5, 6, 7, 61, 109, 181, 277, 661, 1000, 1019, 1018879, MAXD - 1, MAXD - 2, MAXD - 3}
    for n in (2, 3, 7, 31, 100, 255, 256, 511, 512, 999, 1000, 1022, 1023):
        for dlt in (-2, -1, 1, 2):
            d = n * n + dlt
            if 2 <= d < MAXD and not is_square(d):
                ds.add(d)
    for n in (1023, 1022, 1000, 512, 500, 300):
        ds.add(n * n - 1)
        ds.add(n * n + 1)
    x = SEED
    for _ in range(100):
        x ^= (x << 13) & M64
        x ^= x >> 7
        x ^= (x << 17) & M64
        d = 2 + x % (MAXD - 2)
        if not is_square(d):
            ds.add(d)
    return sorted(d for d in ds if 2 <= d <= MAXD and not is_square(d))


def cmd_exact(verbose=True):
    t0 = time.time()
    h = FNV0
    cnt = 0
    for d in range(2, EXACT_MAX + 1):
        if is_square(d):
            continue
        x, y, per = exact(d)
        assert x * x - d * y * y == 1
        mx, my, mp, _, _, _ = modular(d)
        assert (mx, my, mp) == (x % P, y % P, per), d
        for w in (d, x % P, y % P, per):
            h = fnv_step(h, w)
        cnt += 1
    rows = []
    bigdigits = 0
    for d in targets():
        x, y, per = exact(d)
        assert x * x - d * y * y == 1
        mx, my, mp, _, _, _ = modular(d)
        assert (mx, my, mp) == (x % P, y % P, per), d
        rows.append((d, x % P, y % P, per))
        bigdigits = max(bigdigits, len(str(x)))
    if verbose:
        print("exact: %d values of d up to %d, digest %016x; %d targeted d, longest solution %d digits; %.1f s" % (cnt, EXACT_MAX, h, len(rows), bigdigits, time.time() - t0))
    return h, rows


def chunk_range(k):
    lo = max(2, k * CHUNK)
    hi = min(MAXD - 1, (k + 1) * CHUNK - 1)
    return lo, hi


def cmd_chunk(k, verbose=True):
    t0 = time.time()
    lo, hi = chunk_range(k)
    h = FNV0
    mm = md = ma = mp = 0
    zero = 0
    cnt = 0
    for d in range(lo, hi + 1):
        if is_square(d):
            continue
        x, y, per, a, b, c = modular(d)
        if x == 0 or y == 0:
            zero += 1
        h = fnv_step(fnv_step(fnv_step(fnv_step(h, d), x), y), per)
        mm = max(mm, a)
        md = max(md, b)
        ma = max(ma, c)
        mp = max(mp, per)
        cnt += 1
    res = (h, cnt, mp, mm, md, ma, zero)
    if verbose:
        print("chunk %d [%d, %d]: %d values, digest %016x, max period %d, max m %d, max dd %d, max a %d, zero residues %d; %.1f s" % ((k, lo, hi, cnt, h, mp, mm, md, ma, zero, time.time() - t0)))
    return res


def cmd_verify(k, sl):
    t0 = time.time()
    lo, hi = chunk_range(k)
    step = CHUNK // 4
    a = max(lo, k * CHUNK + sl * step)
    b = min(hi, k * CHUNK + (sl + 1) * step - 1)
    cnt = 0
    for d in range(a, b + 1):
        if is_square(d):
            continue
        x, y, per, _, _, _ = modular(d)
        ex, ey, eper = exact(d)
        assert (ex % P, ey % P, eper) == (x, y, per), d
        cnt += 1
    print("verify chunk %d slice %d [%d, %d]: %d values, exact == modular; %.1f s" % (k, sl, a, b, cnt, time.time() - t0))


def all_chunks(verbose):
    cache = os.environ.get("PELL_CACHE")
    data = {}
    if cache and os.path.exists(cache):
        for line in open(cache):
            f = line.split()
            data[int(f[0])] = tuple(int(v) for v in f[1:])
    out = []
    for k in range(8):
        if k not in data:
            data[k] = cmd_chunk(k, verbose)
            if cache:
                with open(cache, "a") as fh:
                    fh.write("%d %s\n" % (k, " ".join(str(v) for v in data[k])))
        out.append(data[k])
    return out


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "exact":
        cmd_exact()
        print("OK")
    elif cmd == "chunk":
        cmd_chunk(int(sys.argv[2]))
    elif cmd == "verify":
        cmd_verify(int(sys.argv[2]), int(sys.argv[3]))
    elif cmd == "check":
        cmd_exact()
        for k in range(8):
            for sl in range(4):
                cmd_verify(k, sl)
        ch = all_chunks(True)
        print("max period %d, max m %d, max dd %d, max a %d" % (max(c[2] for c in ch), max(c[3] for c in ch), max(c[4] for c in ch), max(c[5] for c in ch)))
        print("OK")
    elif cmd == "emit":
        eh, rows = cmd_exact(False)
        ch = all_chunks(False)
        print("// generated by tools/pell_ref.py emit")
        print("constexpr std::uint32_t kExactMax = %du;" % EXACT_MAX)
        print("constexpr std::uint64_t kExactDigest = 0x%016xull;" % eh)
        print("constexpr std::uint32_t kChunkSize = %du;" % CHUNK)
        print("constexpr std::uint64_t kChunkDigest[8] = {%s};" % ", ".join("0x%016xull" % c[0] for c in ch))
        print("constexpr std::uint32_t kChunkCount[8] = {%s};" % ", ".join("%du" % c[1] for c in ch))
        print("constexpr std::uint32_t kMaxPeriod = %du;" % max(c[2] for c in ch))
        print("constexpr std::uint32_t kMaxM = %du;" % max(c[3] for c in ch))
        print("constexpr std::uint32_t kMaxDd = %du;" % max(c[4] for c in ch))
        print("constexpr std::uint32_t kMaxA = %du;" % max(c[5] for c in ch))
        print("constexpr std::uint32_t kZeroResidues = %du;" % sum(c[6] for c in ch))
        print("struct Target { std::uint32_t d, x, y, period; };")
        print("constexpr Target kTargets[%d] = {" % len(rows))
        print(",\n".join("    {%du, %du, %du, %du}" % r for r in rows))
        print("};")
    else:
        sys.exit("unknown command")


if __name__ == "__main__":
    main()
