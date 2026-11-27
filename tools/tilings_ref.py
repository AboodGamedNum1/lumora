#!/usr/bin/env python3
"""Independent reference for core/tilings (domino tilings of an h by w rectangle modulo 998244353, h = 1..6, any u32 width).

Standard library only (no NumPy, no third-party package). Runtime: Python 3.8 or newer; the timing is stated in docs/tilings.md.
Usage:
  python3 tools/tilings_ref.py check   compare the methods on the small extents and on sampled huge widths, print "OK"
  python3 tools/tilings_ref.py emit    print the golden digests and recurrence tables as C++ source for tests/tilings/tilings_tests.cpp (runs the checks first)
Methods of different shape:
  brute (THE oracle where it runs): fill the first uncovered cell of the grid with a horizontal or a vertical domino and count the completed tilings, with
     exact integers. It is feasible for h = 1..6 and w = 0..8 (6 x 8 has 167089 tilings); beyond that it is too slow.
  column DP (exact): the number of ways through the columns one at a time, over the profiles, with exact integers (and the same reduced mod p); it is
     compared with the brute force on every (h, w) the brute force reaches and gives the sequences up to w = 3000.
  recurrence route (the oracle at huge w, which has no brute force and no column loop): Berlekamp-Massey on the sequence a(0..599) of each h gives the linear
     recurrence mod p, and x^w modulo its characteristic polynomial (Kitamasa) gives a(w) mod p for any w. The recurrence is checked against the column DP
     for every w up to 3000 and against an independent matrix power (Python, mod p) at every huge width in the digests.
Digest: FNV-1a 64-bit over 32-bit words (one xor and one multiply per word); a group digest folds its results in the stated order.
"""
import sys

P = 998244353
M64 = 0xFFFFFFFFFFFFFFFF
MAXW = 0xFFFFFFFF
HS = [1, 2, 3, 4, 5, 6]


def fnv(words, h=0xcbf29ce484222325):
    for w in words:
        h = ((h ^ w) * 0x100000001b3) & M64
    return h


def brute(h, w):
    full = (1 << (h * w)) - 1  # cell (r, c) is bit c * h + r
    memo_free = {}

    def count(mask):
        if mask == full:
            return 1
        # first free cell
        i = 0
        while (mask >> i) & 1:
            i += 1
        r, c = i % h, i // h
        total = 0
        if r + 1 < h and not (mask >> (i + 1)) & 1:
            total += count(mask | (3 << i))
        if c + 1 < w and not (mask >> (i + h)) & 1:
            total += count(mask | (1 << i) | (1 << (i + h)))
        return total

    return count(0) if w > 0 else 1


def transitions(h):
    size = 1 << h
    out = []
    for s in range(size):
        ts = []
        for t in range(size):
            if s & t:
                continue
            free = (~(s | t)) & (size - 1)
            ok, run = True, 0
            for r in range(h):
                if (free >> r) & 1:
                    run += 1
                else:
                    if run % 2:
                        ok = False
                    run = 0
            if run % 2:
                ok = False
            if ok:
                ts.append(t)
        out.append(ts)
    return out


def dp_seq(h, n, mod=None):
    tr = transitions(h)
    size = 1 << h
    v = [0] * size
    v[0] = 1
    seq = [1]
    for _ in range(n):
        nv = [0] * size
        for s in range(size):
            if v[s]:
                for t in tr[s]:
                    nv[t] += v[s]
        if mod:
            nv = [x % mod for x in nv]
        v = nv
        seq.append(v[0])
    return seq


def berlekamp_massey(a):
    c, b = [1], [1]
    L, m, bb = 0, 1, 1
    for n in range(len(a)):
        d = a[n]
        for i in range(1, L + 1):
            d = (d + c[i] * a[n - i]) % P
        if d == 0:
            m += 1
            continue
        t = c[:]
        coef = d * pow(bb, P - 2, P) % P
        if len(c) < len(b) + m:
            c += [0] * (len(b) + m - len(c))
        for i in range(len(b)):
            c[i + m] = (c[i + m] - coef * b[i]) % P
        if 2 * L <= n:
            L, b, bb, m = n + 1 - L, t, d, 1
        else:
            m += 1
    c = c[:L + 1] + [0] * max(0, L + 1 - len(c))
    # a(n) = sum_{i=1..L} rec[i-1] * a(n - i)
    return L, [(-c[i]) % P for i in range(1, L + 1)]


def poly_mulmod(x, y, rec):
    L = len(rec)
    if L == 0:
        return []
    prod = [0] * (2 * L - 1)
    for i, xi in enumerate(x):
        if xi:
            for j, yj in enumerate(y):
                prod[i + j] = (prod[i + j] + xi * yj) % P
    for k in range(2 * L - 2, L - 1, -1):
        c = prod[k]
        if c:
            for i in range(L):
                prod[k - 1 - i] = (prod[k - 1 - i] + c * rec[i]) % P
            prod[k] = 0
    return prod[:L]


def kitamasa(seq, rec, w):
    L = len(rec)
    if w < L:
        return seq[w]
    res = [1] + [0] * (L - 1)
    base = [0, 1] + [0] * (L - 2) if L > 1 else [rec[0]]
    e = w
    while e:
        if e & 1:
            res = poly_mulmod(res, base, rec)
        base = poly_mulmod(base, base, rec)
        e >>= 1
    return sum(res[i] * seq[i] for i in range(L)) % P


def matpow_value(h, w):
    tr = transitions(h)
    size = 1 << h
    M = [[0] * size for _ in range(size)]
    for s in range(size):
        for t in tr[s]:
            M[s][t] = 1

    def mul(A, B):
        return [[sum(A[i][k] * B[k][j] for k in range(size)) % P for j in range(size)] for i in range(size)]
    R = [[1 if i == j else 0 for j in range(size)] for i in range(size)]
    e = w
    while e:
        if e & 1:
            R = mul(R, M)
        M = mul(M, M)
        e >>= 1
    return R[0][0]


def huge_widths(h):
    ws = set()
    for j in range(32):
        for w in ((1 << j) - 1, 1 << j, (1 << j) + 1):
            if 0 <= w <= MAXW:
                ws.add(w)
    ws.add(MAXW - 1)
    ws.add(MAXW)
    x = (0x9E3779B97F4A7C15 + h) & M64
    for _ in range(40):
        x ^= (x << 13) & M64
        x ^= x >> 7
        x ^= (x << 17) & M64
        ws.add(x & MAXW)
    return sorted(ws)


NSEQ = 600
_cache = {}


def recurrence(h):
    if h not in _cache:
        seq = dp_seq(h, NSEQ - 1, P)
        L, rec = berlekamp_massey(seq)
        _cache[h] = (seq, L, rec)
    return _cache[h]


def checks():
    for h in HS:
        for w in range(0, 9):
            assert dp_seq(h, w)[w] == brute(h, w), (h, w)
    for h in HS:
        exact = dp_seq(h, 80)
        seq, L, rec = recurrence(h)
        assert L <= 64
        assert [x % P for x in exact] == seq[:81]
        # the recurrence reproduces the column DP up to w = 3000
        full = dp_seq(h, 3000, P)
        for n in range(L, 3001):
            assert full[n] == sum(rec[i] * full[n - 1 - i] for i in range(L)) % P, (h, n)
        for w in range(0, 3001, 7):
            assert kitamasa(seq, rec, w) == full[w], (h, w)
    # symmetry, known values
    for h in range(1, 7):
        for w in range(1, 7):
            assert brute(h, w) == brute(w, h)
    assert brute(2, 2) == 2 and brute(4, 4) == 36 and brute(6, 6) == 6728 and brute(4, 5) == 95
    assert [brute(3, 2 * k) for k in range(1, 5)] == [3, 11, 41, 153]
    # huge widths: the recurrence route against an independent matrix power
    for h in HS:
        seq, L, rec = recurrence(h)
        for w in huge_widths(h)[::5]:
            assert kitamasa(seq, rec, w) == matpow_value(h, w), (h, w)
    return True


def emit():
    assert len(set(len(huge_widths(h)) for h in HS)) == 1
    print("// Generated by tools/tilings_ref.py emit")
    print("constexpr std::uint64_t kSmallDigest[6] = {  // per h = 1..6: w = 0..300 from the exact column DP")
    row = []
    for h in HS:
        seq = dp_seq(h, 300)
        row.append("0x%016xull" % fnv([x % P for x in seq]))
    print("    " + ", ".join(row) + ",")
    print("};")
    print("constexpr std::uint64_t kHugeDigest[6] = {  // per h = 1..6: huge widths (2^j - 1, 2^j, 2^j + 1 for j = 0..31, UINT32_MAX - 1, UINT32_MAX, forty random) from the recurrence route")
    row = []
    for h in HS:
        seq, L, rec = recurrence(h)
        row.append("0x%016xull" % fnv([kitamasa(seq, rec, w) for w in huge_widths(h)]))
    print("    " + ", ".join(row) + ",")
    print("};")
    print("// linear recurrence of each h found by Berlekamp-Massey from a(0..599): a(n) = sum over i = 1..order of c[i - 1] * a(n - i) mod p")
    for h in HS:
        seq, L, rec = recurrence(h)
        print("constexpr std::uint32_t kRecOrder%d = %du;" % (h, L))
        print("constexpr std::uint32_t kRec%d[%d] = {%s};" % (h, max(L, 1), ", ".join("%du" % c for c in rec) if L else "0u"))
    print("constexpr std::uint32_t kHugeCount = %du;" % len(huge_widths(1)))
    print("constexpr std::uint32_t kHugeW[6][%d] = {" % len(huge_widths(1)))
    for h in HS:
        print("    {" + ", ".join("%du" % w for w in huge_widths(h)) + "},")
    print("};")


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in ("check", "emit"):
        print(__doc__)
        sys.exit(2)
    if sys.argv[1] == "check":
        checks()
        print("OK")
    else:
        checks()
        emit()
