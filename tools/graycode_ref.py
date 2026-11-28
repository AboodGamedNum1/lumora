#!/usr/bin/env python3
"""Independent reference for core/graycode (Python standard library only; no code shared with the library).

Codes are u64 values, digit i in bits 4i..4i+3 (digit n - 1 is the slowest). Methods:
- the reflected code is built literally from the header's definition: seq_n = for d in 0..k-1: d followed by seq_(n-1) forward (d even) or backward (d odd). Every
  walk digest (k^n up to 2^22) is made from these explicit lists, so the expected values do not come from a formula;
- for the large sizes, unrank and rank are the same definition used as a recursion on the index (the block of the top digit is rank // k^(n-1); an odd top digit
  reverses the index of the lower digits), and the successor is unrank(rank + 1). `check` ties this recursion to the explicit lists for every (k, n) with k^n up
  to 2^16 and to a direct adjacency test (consecutive codes differ in one digit by exactly 1) on the explicit lists up to 2^20 (the walk digests go to 2^22).
The library uses neither the recursion nor lists: it uses the parity of the odd digits above each digit (an iterative rule), so the recursion here is a different
formulation of the same sequence, not a port. Digests are FNV-1a over 64-bit words (fold(h, w) = (h ^ w) * 0x100000001b3 mod 2^64, start 0xcbf29ce484222325).
Commands: check, emit_part N (N = 0..5).
"""
import sys

M64 = (1 << 64) - 1
FNV0 = 0xCBF29CE484222325
OK, NULLARG, BADARG, BADCODE, RTL, NONEXT, OVERFLOW = 0, 1, 2, 3, 4, 5, 6


def fold(h, w):
    return ((h ^ (w & M64)) * 0x100000001B3) & M64


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def construct(k, n):
    s = [0]
    for lvl in range(n):
        t = []
        for d in range(k):
            seg = s if d % 2 == 0 else s[::-1]
            sh = d << (4 * lvl)
            t.extend(sh | x for x in seg)
        s = t
    return s


def unrank_rec(k, n, r):
    if n == 0:
        return 0
    pw = k ** (n - 1)
    d, rem = divmod(r, pw)
    if d % 2 == 1:
        rem = pw - 1 - rem
    return (d << (4 * (n - 1))) | unrank_rec(k, n - 1, rem)


def rank_rec(k, n, code):
    if n == 0:
        return 0
    pw = k ** (n - 1)
    d = (code >> (4 * (n - 1))) & 15
    low = rank_rec(k, n - 1, code & ((1 << (4 * (n - 1))) - 1))
    return d * pw + (low if d % 2 == 0 else pw - 1 - low)


def valid(k, n, code):
    if n < 16 and code >> (4 * n):
        return False
    return all(((code >> (4 * i)) & 15) < k for i in range(n))


def unrank_py(k, n, r):
    if r >= k ** n:
        return RTL, 0
    return OK, unrank_rec(k, n, r)


def rank_py(k, n, code):
    if not valid(k, n, code):
        return BADCODE, 0
    return OK, rank_rec(k, n, code)


def next_py(k, n, code):
    if not valid(k, n, code):
        return BADCODE, 0
    r = rank_rec(k, n, code)
    if r + 1 == k ** n:
        return NONEXT, 0
    return OK, unrank_rec(k, n, r + 1)


def res(h, st, v):
    h = fold(h, st)
    if st == OK:
        h = fold(h, v)
    return h


def one_digit_by_one(a, b, n):
    diff = 0
    for i in range(n):
        x, y = (a >> (4 * i)) & 15, (b >> (4 * i)) & 15
        if x != y:
            if abs(x - y) != 1:
                return False
            diff += 1
    return diff == 1


# ---- G1: binary ----
def enc(x):
    return x ^ (x >> 1)


def dec(g):
    x = 0
    p = 0
    for b in range(63, -1, -1):
        p ^= (g >> b) & 1
        x |= p << b
    return x


def structured():
    out = [0, 0x5555555555555555, 0xAAAAAAAAAAAAAAAA, M64]
    for i in range(64):
        out += [(1 << i), ((1 << i) - 1) & M64, ((1 << i) + 1) & M64]
    for i in range(64):
        for j in range(i, 64):
            out.append((((1 << (j + 1)) - 1) ^ ((1 << i) - 1)) & M64)
    for i in range(64):
        for j in range(i + 1, 64):
            out.append((1 << i) | (1 << j))
    return out


def d_g1():
    h = FNV0
    cnt = 0
    for x in structured():
        h = fold(fold(fold(h, x), enc(x)), dec(x))
        cnt += 1
    for i in range(200000):
        x = splitmix_at((1 << 40) + i)
        h = fold(fold(fold(h, x), enc(x)), dec(x))
        cnt += 1
    return h, cnt


# ---- G2: counts ----
def d_g2():
    h = FNV0
    cnt = 0
    for k in range(2, 17):
        for n in range(1, 17):
            c = k ** n
            h = res(fold(fold(h, k), n), OK if c < (1 << 64) else OVERFLOW, c)
            cnt += 1
    return h, cnt


# ---- G4: whole walks ----
WALK_LIMIT = 1 << 22
CHECK_LIMIT = 1 << 20


def walk_pairs(k):
    return [n for n in range(1, 17) if k ** n <= WALK_LIMIT]


def d_g4(ks):
    h = FNV0
    cnt = 0
    for k in ks:
        for n in walk_pairs(k):
            s = construct(k, n)
            h = fold(fold(fold(h, k), n), len(s))
            for c in s:
                h = fold(h, c)
            cnt += len(s)
    return h, cnt


# ---- G5: edge and sampled ranks ----
def rank_list(k, n):
    c = k ** n
    pw = k ** (n - 1)
    cand = [0, 1, 2, k - 1, k, c - 2, c - 1, c, c + 1, M64]
    for j in range(1, k):
        cand += [j * pw - 1, j * pw, j * pw + 1]
    return [x for x in cand if 0 <= x <= M64]


def d_g5(ks):
    h = FNV0
    cnt = 0
    for k in ks:
        for n in range(1, 17):
            c = k ** n
            base = (1 << 44) + (k * 16 + n) * 1000000
            lst = rank_list(k, n) + [splitmix_at(base + i) % c for i in range(2000)]
            h = fold(fold(h, k), n)
            for r in lst:
                st, code = unrank_py(k, n, r)
                h = res(fold(h, r), st, code)
                if st == OK:
                    h = res(h, *rank_py(k, n, code))
                    h = res(h, *next_py(k, n, code))
                cnt += 1
    return h, cnt


# ---- G6: codes ----
def d_g6a(ks):
    h = FNV0
    cnt = 0
    for k in ks:
        for n in range(1, 5):
            for code in range(1 << 16):
                if valid(k, n, code):
                    h = res(res(fold(fold(fold(h, k), n), code), *rank_py(k, n, code)), *next_py(k, n, code))
                else:
                    h = fold(fold(fold(fold(h, k), n), code), BADCODE)
                cnt += 1
    return h, cnt


def code_list(k, n):
    base = (1 << 45) + (k * 16 + n) * 1000000
    w = unrank_rec(k, n, splitmix_at(base) % (k ** n))
    out = []
    for p in range(n):
        for v in range(k, 16):
            out.append((w & ~(15 << (4 * p))) | (v << (4 * p)))
    for b in range(4 * n, 64):
        out.append(w | (1 << b))
    for i in range(1, 501):
        out.append(splitmix_at(base + i))
    for i in range(501, 701):
        x = 0
        for p in range(n):
            x |= (splitmix_at(base + i * 16 + p) % 16) << (4 * p)
        out.append(x)
    return out


def d_g6b(ks):
    h = FNV0
    cnt = 0
    for k in ks:
        for n in range(1, 17):
            h = fold(fold(h, k), n)
            for code in code_list(k, n):
                h = res(res(fold(h, code), *rank_py(k, n, code)), *next_py(k, n, code))
                cnt += 1
    return h, cnt


PARTS = [
    [("G1", d_g1), ("G2", d_g2)],
    [("G4a", lambda: d_g4([2, 3, 4])), ("G4b", lambda: d_g4([5, 6, 7, 8, 9]))],
    [("G4c", lambda: d_g4([10, 11, 12, 13, 14, 15, 16]))],
    [("G5a", lambda: d_g5([2, 3, 4, 5, 6])), ("G5b", lambda: d_g5([7, 8, 9, 10]))],
    [("G5c", lambda: d_g5([11, 12, 13, 14, 15, 16])), ("G6b", lambda: d_g6b(range(2, 17)))],
    [("G6a", lambda: d_g6a(range(2, 17)))],
]


def emit(part):
    for name, f in PARTS[part]:
        h, cnt = f()
        print("constexpr u64 k%sDigest = 0x%016Xull;" % (name, h))
        print("constexpr u64 k%sCount = %dull;" % (name, cnt))


def check():
    # recursion against the explicit lists, adjacency on the explicit lists
    for k in range(2, 17):
        for n in range(1, 17):
            if k ** n > CHECK_LIMIT:
                break
            s = construct(k, n)
            assert len(s) == k ** n and len(set(s)) == len(s)
            assert s[0] == 0
            for i in range(len(s) - 1):
                assert one_digit_by_one(s[i], s[i + 1], n), (k, n, i)
            if k ** n <= 1 << 16:
                for r, c in enumerate(s):
                    assert unrank_rec(k, n, r) == c and rank_rec(k, n, c) == r
            else:
                for r in range(0, len(s), 997):
                    assert unrank_rec(k, n, r) == s[r] and rank_rec(k, n, s[r]) == r
    # header examples
    assert [unrank_rec(3, 2, r) for r in range(9)] == [0x00, 0x01, 0x02, 0x12, 0x11, 0x10, 0x20, 0x21, 0x22]
    assert unrank_rec(4, 2, 15) == 0x30 and unrank_rec(2, 16, 40000) == 0x1101001001100000
    assert unrank_rec(10, 3, 999) == 0x900 and unrank_rec(10, 16, 10**16 - 1) == 0x9000000000000000
    assert unrank_rec(10, 16, 12345678901234) == 0x17355371991735
    assert unrank_rec(3, 16, 3**16 - 1) == 0x2222222222222222
    assert unrank_rec(16, 16, M64) == 0xF000000000000000
    assert unrank_rec(16, 16, 0x123456789ABCDEF0) == 0x1D3B597795B3D1FF
    assert rank_rec(10, 16, 0x9000000000000000) == 10**16 - 1
    assert enc(0x123456789ABCDEF0) == 0x1B2E7D44D7E2B188 and dec(0x123456789ABCDEF0) == 0x1C279BAF132894A0
    assert dec(M64) == 0xAAAAAAAAAAAAAAAA and dec(1 << 63) == M64 and dec(3) == 2
    # sampled recursion round trips and adjacency at large sizes
    for k in range(2, 17):
        for n in range(1, 17):
            c = k ** n
            for i in range(200):
                r = splitmix_at(i * 77 + k * 16 + n) % c
                code = unrank_rec(k, n, r)
                assert valid(k, n, code) and rank_rec(k, n, code) == r
                if r + 1 < c:
                    assert one_digit_by_one(code, unrank_rec(k, n, r + 1), n)
    for k in (2, 3, 7, 16):
        for x in structured()[:500]:
            assert dec(enc(x)) == x and enc(dec(x)) == x
    print("check ok")


if __name__ == "__main__":
    if sys.argv[1] == "check":
        check()
    elif sys.argv[1] == "emit_part":
        emit(int(sys.argv[2]))
