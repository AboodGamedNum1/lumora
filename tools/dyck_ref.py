#!/usr/bin/env python3
"""Independent reference for core/dyck (Python standard library only; no code shared with the C++ library).

Words are masks of 2n bits: bit 2n - 1 is the first character, bit 0 the last, '(' is 0 and ')' is 1; the rank order is the numeric order of the masks.
Methods (all different from the library's ballot-number formula): small n by brute force (every word generated as a string by recursion, sorted as strings with
'(' < ')', rank and neighbour read from the sorted list); every n up to 32 by a memoised dynamic program f(remaining, depth) with Python integers (rank and unrank
by the program, the successor as unrank(rank + 1)); the Catalan numbers by math.comb; the per-position bit counts of the whole set of words by a prefix times
suffix program. Digests are FNV-1a over 64-bit words (fold(h, w) = (h ^ w) * 0x100000001b3 mod 2^64, start 0xcbf29ce484222325).
Commands: check, emit_part N (N = 0..2).
"""
import sys
from functools import lru_cache
from math import comb

M64 = (1 << 64) - 1
FNV0 = 0xCBF29CE484222325
OK, NULLARG, BADARG, BADMASK, RTL, NONEXT = 0, 1, 2, 3, 4, 5


def fold(h, w):
    return ((h ^ (w & M64)) * 0x100000001B3) & M64


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


@lru_cache(maxsize=None)
def f(rem, depth):
    """completions of a prefix with rem characters left at the given depth"""
    if depth > rem:
        return 0
    if rem == 0:
        return 1 if depth == 0 else 0
    return f(rem - 1, depth + 1) + (f(rem - 1, depth - 1) if depth > 0 else 0)


def catalan(n):
    return f(2 * n, 0)


def valid(mask, n):
    if mask >> (2 * n):
        return False
    d = 0
    for i in range(2 * n - 1, -1, -1):
        d += -1 if (mask >> i) & 1 else 1
        if d < 0:
            return False
    return d == 0


def rank_dp(mask, n):
    r, d = 0, 0
    for i in range(2 * n - 1, -1, -1):
        if (mask >> i) & 1:
            r += f(i, d + 1)  # i characters remain after this one
            d -= 1
        else:
            d += 1
    return r


def unrank_dp(n, rank):
    m, d = 0, 0
    for i in range(2 * n - 1, -1, -1):
        c = f(i, d + 1)
        if rank < c:
            d += 1
        else:
            rank -= c
            m |= 1 << i
            d -= 1
    return m


def v_rank(mask, n):
    if not valid(mask, n):
        return BADMASK, 0
    return OK, rank_dp(mask, n)


def v_next(mask, n):
    if not valid(mask, n):
        return BADMASK, 0
    r = rank_dp(mask, n)
    if r + 1 >= catalan(n):
        return NONEXT, 0
    return OK, unrank_dp(n, r + 1)


def v_unrank(n, rank):
    if rank >= catalan(n):
        return RTL, 0
    return OK, unrank_dp(n, rank)


def fold_mask_tail(h, mask, n):
    cr, vr = v_rank(mask, n)
    cn, vn = v_next(mask, n)
    for w in (cr, vr, cn, vn):
        h = fold(h, w)
    return h


def mask_case(h, n, mask):
    return fold_mask_tail(fold(fold(h, n), mask), mask, n)


def rank_case(h, n, rank):
    cu, vu = v_unrank(n, rank)
    h = fold(fold(fold(fold(h, n), rank), cu), vu)
    if cu == OK:
        return fold_mask_tail(h, vu, n)
    for _ in range(4):
        h = fold(h, 0)
    return h


def low_mask(n):
    return M64 if n == 32 else (1 << (2 * n)) - 1


# ---- digests ----
def d_d3():
    h = FNV0
    for n in range(33):
        h = fold(fold(h, n), catalan(n))
    return h, 33


def d_d4():
    h = FNV0
    cnt = 0
    for n in range(10):
        for m in range(4 ** n):
            h = mask_case(h, n, m)
            cnt += 1
            if valid(m, n):
                for x in (m | (1 << (2 * n)), m | (1 << 63), m | (1 << (2 * n + 3)) if 2 * n + 3 < 64 else m | (1 << 63)):
                    h = mask_case(h, n, x)
                    cnt += 1
    return h, cnt


def rank_list(n):
    c = catalan(n)
    out = [0, 1, 2, (c - 2) & M64, (c - 1) & M64, c, c + 1, c + 2, (c // 2 - 1) & M64, c // 2, c // 2 + 1, M64, M64 - 1]
    for k in range(1, 64):
        out += [(1 << k) - 1, 1 << k]
    return out


def d_d6():
    h = FNV0
    cnt = 0
    for n in range(33):
        for r in rank_list(n):
            h = rank_case(h, n, r)
            cnt += 1
    return h, cnt


D7A, D7B, D7C = 12500, 1250, 3125


def d_d7a():
    h = FNV0
    for n in range(17, 33):
        c = catalan(n)
        base = (1 << 40) + n * 1000000
        for i in range(D7A):
            h = rank_case(h, n, splitmix_at(base + i) % c)
    return h, 16 * D7A


def edit_case(n, i, base):
    c = catalan(n)
    mask = unrank_dp(n, splitmix_at(base + 2 * i) % c)
    w2 = splitmix_at(base + 2 * i + 1)
    p1 = w2 % (2 * n)
    p2 = (w2 >> 8) % (2 * n)
    low = low_mask(n)
    k = i % 10
    if k == 0:
        return mask ^ (1 << p1)
    if k == 1:
        b1, b2 = (mask >> p1) & 1, (mask >> p2) & 1
        return mask ^ (((b1 ^ b2) << p1) | ((b1 ^ b2) << p2))
    if k == 2:
        return (mask | (1 << (2 * n + (w2 >> 16) % (64 - 2 * n)))) if n < 32 else mask ^ (1 << 63)
    if k == 3:
        return low
    if k == 4:
        return 0
    if k == 5:
        return 0x5555555555555555 & low
    if k == 6:
        return 0xAAAAAAAAAAAAAAAA & low
    if k == 7:
        return mask ^ low
    if k == 8:
        return mask ^ (1 << p1) ^ (1 << p2)
    return mask >> 1


def d_d7b():
    h = FNV0
    for n in range(17, 33):
        base = (1 << 41) + n * 1000000
        for i in range(D7B):
            h = mask_case(h, n, edit_case(n, i, base))
    return h, 16 * D7B


def d_d7c():
    h = FNV0
    for n in range(17, 33):
        base = (1 << 42) + n * 1000000
        for i in range(D7C):
            m = splitmix_at(base + i)
            if i % 5 != 0:
                m &= low_mask(n)
            h = mask_case(h, n, m)
    return h, 16 * D7C


def d_d8():
    h = FNV0
    cnt = 0
    for n in range(1, 33):
        c = catalan(n)
        rs = list(range(0, min(c, 21))) + list(range(max(c - 21, 0), c))
        for r in rs:
            h = mask_case(h, n, unrank_dp(n, r))
            cnt += 1
    return h, cnt


def bit_counts(n):
    """for each bit position p (0..2n-1): the number of words of semilength n with a 1 (')') there, by prefix times suffix"""
    out = []
    for p in range(2 * n):
        i = 2 * n - 1 - p  # characters before this one
        tot = 0
        for d in range(1, i + 2):
            # prefixes of length i ending at depth d (from 0, nonnegative): paths = f-like count by reflection-free DP
            tot += prefixes(i, d) * f(2 * n - i - 1, d - 1)
        out.append(tot)
    return out


@lru_cache(maxsize=None)
def prefixes(length, depth):
    if depth < 0 or depth > length:
        return 0
    if length == 0:
        return 1 if depth == 0 else 0
    return prefixes(length - 1, depth - 1) + prefixes(length - 1, depth + 1)


def d_d5(ns):
    h = FNV0
    cnt = 0
    for n in ns:
        c = catalan(n)
        bc = bit_counts(n)
        assert all(0 <= x <= c for x in bc)
        h = fold(fold(h, n), c)
        for p in range(2 * n):
            h = fold(h, bc[p])
        total = sum(bc[p] << p for p in range(2 * n))
        h = fold(h, total)
        cnt += c
    return h, cnt


D5 = {"A": list(range(0, 13)), "B": [13], "C": [14], "D": [15]}


def put(out, name, hc):
    out.append("constexpr u64 k%sDigest = 0x%016xull;" % (name, hc[0]))
    out.append("constexpr u64 k%sCount = %du;" % (name, hc[1]))


def emit_part(n):
    out = []
    if n == 0:
        put(out, "D3", d_d3())
        put(out, "D4", d_d4())
        put(out, "D6", d_d6())
        put(out, "D8", d_d8())
        for k in "ABCD":
            put(out, "D5" + k, d_d5(D5[k]))
    elif n == 1:
        put(out, "D7a", d_d7a())
    elif n == 2:
        put(out, "D7b", d_d7b())
        put(out, "D7c", d_d7c())
    print("\n".join(out))


def brute(n):
    words = []

    def gen(s, o, c):
        if o == n and c == n:
            words.append(s)
            return
        if o < n:
            gen(s + "(", o + 1, c)
        if c < o:
            gen(s + ")", o, c + 1)
    gen("", 0, 0)
    words.sort(key=lambda w: w.replace("(", "0").replace(")", "1"))
    return [int(w.replace("(", "0").replace(")", "1") or "0", 2) for w in words]


def c_check():
    for n in range(33):
        assert catalan(n) == comb(2 * n, n) // (n + 1)
    for n in range(0, 11):
        seq = brute(n)
        assert seq == sorted(seq) and len(seq) == catalan(n)
        for i, w in enumerate(seq):
            assert rank_dp(w, n) == i and unrank_dp(n, i) == w
            if i + 1 < len(seq):
                assert v_next(w, n) == (OK, seq[i + 1])
            else:
                assert v_next(w, n) == (NONEXT, 0)
        sset = set(seq)
        if n <= 7:
            for m in range(1 << (2 * n)):
                assert valid(m, n) == (m in sset)
    # header examples
    assert [unrank_dp(3, i) for i in range(5)] == [0b000111, 0b001011, 0b001101, 0b010011, 0b010101]
    assert rank_dp(0b10110011, 5) == 21 and rank_dp(0x555555555, 18) == 477638699
    assert rank_dp(0xFFFFFFFF, 32) == 0 and rank_dp(0x5555555555555555, 32) == catalan(32) - 1
    assert unrank_dp(32, 10 ** 16) == 0x08F6C4AB1D384BB7 and unrank_dp(20, 1000000) == 0xBF9F727 and unrank_dp(8, 100) == 0x7A7
    assert v_next(0xFFFFFFFF, 32) == (OK, 0x17FFFFFFF) and v_next(0x5555555555555553, 32) == (OK, 0x5555555555555555)
    assert v_next(0x5555555555555555, 32)[0] == NONEXT and v_next(0, 0)[0] == NONEXT and v_unrank(3, 5)[0] == RTL and v_unrank(0, 1)[0] == RTL
    assert catalan(32) == 55534064877048198 and catalan(31) == 14544636039226909 and catalan(20) == 6564120420
    # per-position counts against brute force
    for n in range(0, 11):
        seq = brute(n)
        assert bit_counts(n) == [sum((w >> p) & 1 for w in seq) for p in range(2 * n)]
    # sums of masks, sampled ranks round trip
    for n in range(1, 33):
        c = catalan(n)
        for k in range(30):
            r = splitmix_at(n * 100 + k) % c
            assert rank_dp(unrank_dp(n, r), n) == r
    print("check ok")


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == "check":
        c_check()
    elif a[0] == "emit_part":
        emit_part(int(a[1]))


main()
