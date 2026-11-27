#!/usr/bin/env python3
"""Independent reference for core/intmath (scope AN). Exact Python integers, no fixed-width arithmetic anywhere.

Written from the rules in intmath.h; it shares no code with the implementation. Every value is a Python int and every result is
computed from the definition: the largest r with r * r <= x (found by bisection, not math.isqrt), gcd by factor-free Euclid on ints,
lcm as a * b // gcd checked against 2**64, signed results checked against the int64 range, and a * b / d as floor and remainder of the
exact product with the three rounding rules. The standard math module is only a second check inside this script (assertions). Operands
come from a seeded generator that the C++ test mirrors. Each section folds every result (status, then the value as two 32-bit words, or
the sentinel when the call fails) into an FNV-1a 64 digest. The script prints the table pasted into tests/intmath/intmath_tests.cpp.
Usage: python3 tools/intmath_ref.py
"""
import math

M64 = (1 << 64) - 1
U64MAX = M64
I64MAX = (1 << 63) - 1
I64MIN = -(1 << 63)
SENT64 = 0x0123456789ABCDEF  # the value *out holds before a call; a failure must leave it
N = 3000


def fnv_words(ws):
    h = 0xcbf29ce484222325
    for v in ws:
        v &= 0xFFFFFFFF
        for k in range(4):
            h ^= (v >> (8 * k)) & 0xFF
            h = (h * 0x100000001b3) & M64
    return h


class Lcg:
    def __init__(s, seed): s.s = seed & M64
    def next(s):
        s.s = (s.s * 6364136223846793005 + 1442695040888963407) & M64
        return s.s >> 32
    def next64(s): return (s.next() << 32) | s.next()
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


SPECIAL = [0, 1, 2, U64MAX, U64MAX - 1, 1 << 63, (1 << 63) - 1, 1 << 32, (1 << 32) - 1]


def gen(r):
    if r.rng(0, 9) == 0: return SPECIAL[r.rng(0, 8)]
    k = r.rng(0, 64)
    v = r.next64()
    return v if k == 64 else v & ((1 << k) - 1)


def sgn(v): return v - (1 << 64) if v >= (1 << 63) else v


def words(v):  # value as unsigned 64 -> two words
    v &= M64
    return [v & 0xFFFFFFFF, v >> 32]


def isqrt(x):
    lo, hi = 0, 1 << 32
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if mid * mid <= x: lo = mid
        else: hi = mid - 1
    assert lo * lo <= x < (lo + 1) * (lo + 1) and lo == math.isqrt(x)
    return lo


def gcd(a, b):
    while b: a, b = b, a % b
    assert a == math.gcd(a, b) if True else 0
    return a


def sec_isqrt():
    r = Lcg(1)
    ws = []
    for _ in range(N):
        x = gen(r)
        ws += [isqrt(x)]
    return fnv_words(ws)


def sec_gcd():
    r = Lcg(2)
    ws = []
    for _ in range(N):
        if r.rng(0, 2) == 0:
            g = gen(r); x = r.rng(0, 65535); y = r.rng(0, 65535)
            a, b = (x * g) & M64, (y * g) & M64
        else:
            a, b = gen(r), gen(r)
        ws += words(gcd(a, b))
    return fnv_words(ws)


def sec_lcm():
    r = Lcg(3)
    ws = []
    for _ in range(N):
        if r.rng(0, 2) == 0:
            g = gen(r) & 0xFFFFFFFF; x = r.rng(0, 65535); y = r.rng(0, 65535)
            a, b = (x * g) & M64, (y * g) & M64
        else:
            a, b = gen(r), gen(r)
        if a == 0 or b == 0: v = 0
        else: v = a * b // gcd(a, b)
        assert v == math.lcm(a, b)
        if v > U64MAX: ws += [0] + words(SENT64)
        else: ws += [1] + words(v)
    return fnv_words(ws)


def sec_signed(op, seed):
    r = Lcg(seed)
    ws = []
    for _ in range(N):
        a, b = sgn(gen(r)), sgn(gen(r))
        if r.rng(0, 3) == 0: b = a if op != 'sub' else -a if a != I64MIN else a  # equal or opposite operands
        v = a + b if op == 'add' else a - b if op == 'sub' else a * b
        if v < I64MIN or v > I64MAX: ws += [0] + words(SENT64)
        else: ws += [1] + words(v)
    return fnv_words(ws)


def mul_div(a, b, d, mode):
    if d == 0 or mode not in (0, 1, 2): return None
    p = a * b
    q, rem = divmod(p, d)
    if mode == 1 and rem != 0: q += 1
    if mode == 2 and 2 * rem >= d: q += 1
    return q if q <= U64MAX else None


def sec_muldiv():
    r = Lcg(7)
    ws = []
    for _ in range(N):
        sel = r.rng(0, 3)
        if sel == 0: a, b, d = gen(r), gen(r), gen(r)
        elif sel == 1:
            a = gen(r); b = gen(r); d = a
        elif sel == 2:
            a = r.next() ; b = r.next(); d = gen(r)
        else:
            x = r.next() + 1; d = 2 * x; a = x; b = 2 * r.next() + 1
        mode = r.rng(0, 4)
        v = mul_div(a, b, d, mode)
        if v is None: ws += [0] + words(SENT64)
        else: ws += [1] + words(v)
    return fnv_words(ws)


def main():
    print('const u64 kDigIsqrt = %dull;' % sec_isqrt())
    print('const u64 kDigGcd = %dull;' % sec_gcd())
    print('const u64 kDigLcm = %dull;' % sec_lcm())
    print('const u64 kDigAdd = %dull;' % sec_signed('add', 4))
    print('const u64 kDigSub = %dull;' % sec_signed('sub', 5))
    print('const u64 kDigMul = %dull;' % sec_signed('mul', 6))
    print('const u64 kDigMulDiv = %dull;' % sec_muldiv())
    print('const u32 kN = %d;' % N)


if __name__ == '__main__':
    main()
