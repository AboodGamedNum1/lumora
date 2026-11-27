#!/usr/bin/env python3
"""Independent reference for core/suffix: prints the data tables of tests/suffix/suffix_tests.cpp as C++ source.

The C++ method is prefix doubling with counting sorts plus Kasai. This reference never uses either on the small and mid inputs: the suffix
array is a naive sort of the suffixes by Python bytes comparison (unsigned bytes, a proper prefix sorts first), and every LCP value is found by
comparing the two suffixes byte by byte. For the 65536-byte inputs a naive sort is too heavy, so the suffix array comes from prefix doubling
with tuple keys and Python's sort, the LCP from Kasai written separately, and both are cross-checked: the array is a permutation, a sample of
adjacent suffix pairs is compared directly by slices, and a sample of LCP values is found by binary search on slice equality. The output is
deterministic.
"""
import itertools
import random

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
N_BIG = 1 << 16
ALPHA3 = [0x00, 0x80, 0xFF]
ALPHA2 = [0x00, 0xFF]


def fnv_word(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & M64
    return h


def naive_sa(t):
    return sorted(range(len(t)), key=lambda i: t[i:])


def naive_lcp(t, sa):
    out = [0] * len(t)
    for r in range(1, len(t)):
        a, b = sa[r - 1], sa[r]
        h = 0
        while a + h < len(t) and b + h < len(t) and t[a + h] == t[b + h]:
            h += 1
        out[r] = h
    return out


def doubling_sa(t):
    n = len(t)
    rk = list(t)
    sa = list(range(n))
    k = 1
    while True:
        key = [(rk[i], rk[i + k] if i + k < n else -1) for i in range(n)]
        sa.sort(key=lambda i: key[i])
        new = [0] * n
        for r in range(1, n):
            new[sa[r]] = new[sa[r - 1]] + (1 if key[sa[r]] != key[sa[r - 1]] else 0)
        rk = new
        if rk[sa[-1]] == n - 1:
            return sa
        k *= 2


def kasai(t, sa):
    n = len(t)
    rank = [0] * n
    for i, s in enumerate(sa):
        rank[s] = i
    out = [0] * n
    h = 0
    for i in range(n):
        if rank[i] > 0:
            j = sa[rank[i] - 1]
            while i + h < n and j + h < n and t[i + h] == t[j + h]:
                h += 1
            out[rank[i]] = h
            if h > 0:
                h -= 1
        else:
            h = 0
    return out


def lcp_binary(t, a, b):
    lo, hi = 0, min(len(t) - a, len(t) - b)
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if t[a:a + mid] == t[b:b + mid]:
            lo = mid
        else:
            hi = mid - 1
    return lo


def arr(name, ctype, vals, per=24):
    out = ["const %s %s[%d] = {" % (ctype, name, len(vals))]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return "\n".join(out)


def pattern(pid, n):
    if pid == 0:
        return bytes(n)
    if pid == 1:
        return b"\xff" * n
    if pid == 2:
        return (b"ab" * n)[:n]
    if pid == 3:
        return (b"\x00\x80\xff" * n)[:n]
    if pid == 4:
        return (b"abcdefg" * n)[:n]
    if pid == 5:  # Fibonacci word
        a, b = b"a", b"ab"
        while len(b) < n:
            a, b = b, b + a
        return b[:n]
    if pid == 6:  # Thue-Morse
        return bytes(0x61 + bin(i).count("1") % 2 for i in range(n))
    if pid == 7:  # a^(n-1) b
        return b"a" * (n - 1) + b"b"
    s = [pid * 7 + 1]

    def lcg():
        s[0] = (s[0] * 1664525 + 1013904223) & 0xFFFFFFFF
        return s[0]

    if pid == 8:  # pseudo-random binary
        return bytes(0xFF if (lcg() >> 16) & 1 else 0x00 for _ in range(n))
    if pid == 9:  # pseudo-random bytes
        return bytes(lcg() >> 24 for _ in range(n))
    blk = bytes(lcg() >> 24 for _ in range(1000))  # a repeated pseudo-random block
    return (blk * n)[:n]


def mid_text(rng, t):
    n = rng.randrange(8, 301)
    style = t % 6
    if style == 0:
        return bytes(rng.choice((0x00, 0xFF)) for _ in range(n))
    if style == 1:
        return bytes(rng.choice(ALPHA3) for _ in range(n))
    if style == 2:
        return bytes(rng.randrange(256) for _ in range(n))
    if style == 3:
        p = rng.randrange(1, 9)
        blk = bytes(rng.choice((0x61, 0x62, 0x63)) for _ in range(p))
        b = bytearray((blk * n)[:n])
        if t % 12 == 3:
            b[rng.randrange(n)] ^= 1
        return bytes(b)
    if style == 4:
        return bytes([rng.choice((0x00, 0x80))]) * n
    return b"a" * (n - 1) + bytes([rng.choice((0x00, 0x62, 0xFF))])


def main():
    rng = random.Random(20261127)
    lines = []
    fold = FNV_OFF

    def f(vals):
        nonlocal fold
        for v in vals:
            fold = fnv_word(fold, v)

    for name, alpha, maxn in (("T3", ALPHA3, 7), ("T2", ALPHA2, 10)):
        sas, lcps = [], []
        for n in range(1, maxn + 1):
            for code in itertools.product(range(len(alpha)), repeat=n):
                t = bytes(alpha[c] for c in code)
                sa = naive_sa(t)
                lc = naive_lcp(t, sa)
                sas += sa
                lcps += lc
                f(sa)
                f(lc)
        lines.append(arr("k%sSa" % name, "std::uint8_t", sas, 40))
        lines.append(arr("k%sLcp" % name, "std::uint8_t", lcps, 40))
    lines.append(arr("kAlpha3", "std::uint8_t", ALPHA3))
    lines.append(arr("kAlpha2", "std::uint8_t", ALPHA2))

    text, hdr, msa, mlcp = [], [], [], []
    for t in range(60):
        b = mid_text(rng, t)
        sa = naive_sa(b)
        lc = naive_lcp(b, sa)
        hdr.append((len(b), len(text)))
        text += list(b)
        msa += sa
        mlcp += lc
        f(sa)
        f(lc)
    lines.append(arr("kMidText", "std::uint8_t", text, 32))
    lines.append("struct MidHdr { std::uint32_t n, off; };")
    lines.append("const MidHdr kMidHdr[%d] = {\n%s\n};" % (len(hdr), ",\n".join("    {%d, %d}" % x for x in hdr)))
    lines.append(arr("kMidSa", "std::uint16_t", msa))
    lines.append(arr("kMidLcp", "std::uint16_t", mlcp))

    big = []
    for pid in range(11):
        t = pattern(pid, N_BIG)
        sa = doubling_sa(t)
        assert sorted(sa) == list(range(N_BIG))
        lc = kasai(t, sa)
        srng = random.Random(pid)
        for _ in range(2500):
            r = srng.randrange(1, N_BIG)
            assert t[sa[r - 1]:] < t[sa[r]:]
            assert lc[r] == lcp_binary(t, sa[r - 1], sa[r])
        ds = FNV_OFF
        for v in sa:
            ds = fnv_word(ds, v)
        dl = FNV_OFF
        for v in lc:
            dl = fnv_word(dl, v)
        big.append((pid, ds, dl, sum(lc), sa[0], sa[-1]))
        for v in (ds & 0xFFFFFFFF, ds >> 32, dl & 0xFFFFFFFF, dl >> 32):
            fold = fnv_word(fold, v)
    lines.append("struct BigRow { std::uint32_t pid; std::uint64_t sa_digest, lcp_digest, lcp_sum; std::uint32_t sa_first, sa_last; };")
    lines.append("const BigRow kBig[%d] = {\n%s\n};" % (len(big), ",\n".join("    {%d, %dull, %dull, %dull, %d, %d}" % x for x in big)))
    lines.append("const std::uint64_t kGolden = %dull;" % fold)
    print("\n\n".join(lines))


main()
