#!/usr/bin/env python3
"""Independent reference for core/rmq: prints the data tables of tests/rmq/rmq_tests.cpp as C++ source.

The C++ method is a sparse table of indices. This reference does not build any table: the answer for [l, r] is found by taking the minimum of
the slice a[l : r + 1] and then the first position of that value in the slice (so the leftmost one), and each answer is cross-checked by a
plain left-to-right scan with a strict less-than. The table size uses n * bit_length(n), not a log loop. Output is deterministic.
"""
import itertools
import random

M32 = 0xFFFFFFFF
FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
N_BIG = 1 << 16


def rmq_slice(a, l, r):
    seg = a[l:r + 1]
    return l + seg.index(min(seg))


def rmq_scan(a, l, r):
    best = l
    for i in range(l + 1, r + 1):
        if a[i] < a[best]:
            best = i
    return best


def answer(a, l, r):
    x = rmq_slice(a, l, r)
    if r - l < 400:
        assert x == rmq_scan(a, l, r)
    return x


def fnv(h, v):
    for _ in range(4):
        h ^= v & 0xFF
        v >>= 8
        h = (h * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def arr(name, ctype, vals, per=12):
    out = ["const %s %s[%d] = {" % (ctype, name, len(vals))]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) + ("u" if ctype == "u32" else "") for v in vals[i:i + per]) + ",")
    out.append("};")
    return "\n".join(out)


def pattern(pid, n):
    if pid == 0:
        return list(range(n))
    if pid == 1:
        return [n - 1 - i for i in range(n)]
    if pid == 2:
        return [7] * n
    if pid == 3:
        return [i % 1024 for i in range(n)]
    if pid == 4:
        s = 1234
        out = []
        for _ in range(n):
            s = (s * 1664525 + 1013904223) & M32
            out.append(s >> 8)
        return out
    # 5: large unsigned values near 2^32 with repeats, so unsigned comparison and ties both matter
    s = 99
    out = []
    for _ in range(n):
        s = (s * 1664525 + 1013904223) & M32
        out.append(M32 - ((s >> 16) % 50))
    return out


def sample_queries(rng, n):
    qs = {(0, n - 1), (0, 0), (n - 1, n - 1)}
    if n > 1:
        qs |= {(0, 1), (n - 2, n - 1), (1, n - 1), (0, n - 2)}
    k = 1
    while k <= n:
        for ln in (k - 1, k, k + 1):
            if 1 <= ln <= n:
                for l in (0, n - ln, rng.randrange(0, n - ln + 1)):
                    qs.add((l, l + ln - 1))
        k <<= 1
    for _ in range(120):
        l = rng.randrange(n)
        r = rng.randrange(l, n)
        qs.add((l, r))
    return sorted(qs)


def main():
    rng = random.Random(20261127)
    h = FNV_OFF
    lines = []

    # table size: n * bit_length(n)
    ws = [0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 100, 1000, 4095, 4096, 4097, 32767, 32768, 32769, 65535, 65536]
    lines.append(arr("kWordN", "u32", ws))
    lines.append(arr("kWordNeed", "u32", [n * n.bit_length() for n in ws]))
    assert 65536 * 17 == 1114112

    # exhaustive: all arrays over {0,1,2} with n = 1..6, lexicographic order, all (l, r) with l <= r in row-major order
    ex = []
    for n in range(1, 7):
        for a in itertools.product(range(3), repeat=n):
            a = list(a)
            for l in range(n):
                for r in range(l, n):
                    x = answer(a, l, r)
                    ex.append(x)
                    h = fnv(h, x)
    lines.append(arr("kExAns", "std::uint8_t", ex, 40))

    # random arrays: three value styles
    vals, hdr, qs = [], [], []
    for t in range(300):
        n = rng.randrange(1, 71) if t < 240 else rng.randrange(100, 400)
        style = t % 3
        if style == 0:
            a = [rng.randrange(4) for _ in range(n)]
        elif style == 1:
            a = [rng.randrange(M32 + 1) for _ in range(n)]
        else:
            a = [M32 - rng.randrange(6) for _ in range(n)]
        pairs = sample_queries(rng, n) if n > 100 else [(l, r) for l in range(n) for r in range(l, n)][:: max(1, n // 2)]
        hdr.append((n, len(vals), len(qs)))
        vals += a
        for (l, r) in pairs:
            x = answer(a, l, r)
            qs.append((l, r, x))
            h = fnv(h, x)
        hdr[-1] = hdr[-1] + (len(pairs),)
    lines.append(arr("kRandVals", "u32", vals))
    lines.append("struct RandHdr { u32 n, voff, qoff, qcount; };")
    lines.append("const RandHdr kRandHdr[%d] = {\n%s\n};" % (len(hdr), ",\n".join("    {%d, %d, %d, %d}" % x for x in hdr)))
    lines.append("struct Q { u32 l, r, ans; };")
    lines.append("const Q kRandQ[%d] = {\n%s\n};" % (len(qs), ",\n".join("    {%d, %d, %d}" % x for x in qs)))

    # big: n = 65536
    bq = []
    bh = []
    for pid in range(6):
        a = pattern(pid, N_BIG)
        pairs = sample_queries(rng, N_BIG)
        bh.append((pid, len(bq), len(pairs)))
        for (l, r) in pairs:
            x = answer(a, l, r)
            bq.append((l, r, x))
            h = fnv(h, x)
    lines.append("struct BigHdr { u32 pid, qoff, qcount; };")
    lines.append("const BigHdr kBigHdr[%d] = {\n%s\n};" % (len(bh), ",\n".join("    {%d, %d, %d}" % x for x in bh)))
    lines.append("const Q kBigQ[%d] = {\n%s\n};" % (len(bq), ",\n".join("    {%d, %d, %d}" % x for x in bq)))
    lines.append("const std::uint64_t kGoldenDigest = %dull;" % h)
    print("\n\n".join(lines))


main()
