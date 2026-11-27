#!/usr/bin/env python3
"""Reference for core/floorsum: exact floor sums with Python big integers, and the data included by tests/floorsum/floorsum_tests.cpp.

S(n, m, a, b) = sum over i = 0 .. n-1 of floor((a * i + b) / m), floor toward minus infinity, exact (never rounded). Usage:
  python3 tools/floorsum_ref.py          print the data (digests and the table) on stdout
  python3 tools/floorsum_ref.py check    run all the cross-checks without printing the data

Independent methods (a disagreement aborts):
  1. loop: the defining sum, cumulative over n (cheap for small n).
  2. classic: the recurrence for f(n) = sum over i = 0 .. n of floor((a i + b) / c) (reduce a and b modulo c, then swap through m = floor((a n + b) / c)).
  3. acl: the iterative variant (quotient split, then swap of m and a) in big integers.
  4. count: lattice counting for a >= 0 and b >= 0: S = sum over k >= 1 of #{i < n : a i + b >= k m}, for a small number of levels.
  Signed values: method 2 after a reflection i -> n - 1 - i (so that the slope is not negative) and a shift of b by a multiple of m, which is not the
  split used by the implementation; methods 1 and 3 are applied to the signed inputs too.
Digest: FNV-1a over 64-bit words: h = (h ^ word) * 0x100000001B3 mod 2^64, starting at 0xCBF29CE484222325. A word is S mod 2^64 (two's complement for
negative S) or S mod 998244353.
"""
import sys

P = 998244353
M64 = (1 << 64) - 1
OFF = 0xCBF29CE484222325
PRIME = 0x100000001B3


def fold(h, w):
    return ((h ^ (w & M64)) * PRIME) & M64


def s_loop(n, m, a, b):
    return sum((a * i + b) // m for i in range(n))


def classic_f(a, b, c, n):
    """sum over i = 0 .. n of floor((a i + b) / c), a, b >= 0, c >= 1; 0 for n < 0."""
    if n < 0:
        return 0
    if a == 0:
        return (b // c) * (n + 1)
    if a >= c or b >= c:
        return n * (n + 1) // 2 * (a // c) + (n + 1) * (b // c) + classic_f(a % c, b % c, c, n)
    mm = (a * n + b) // c
    return n * mm - classic_f(c, c - b - 1, a, mm - 1)


def s_classic(n, m, a, b):
    """a, b >= 0."""
    return classic_f(a, b, m, n - 1) if n > 0 else 0


def s_acl(n, m, a, b):
    """Iterative variant; any integers a, b (Python floor division splits them)."""
    ans = 0
    while n > 0:
        qa, a = divmod(a, m)
        qb, b = divmod(b, m)
        ans += n * (n - 1) // 2 * qa + n * qb
        y = a * n + b
        if y < m:
            break
        n, b = divmod(y, m)
        m, a = a, m
    return ans


def s_count(n, m, a, b, levels=4000):
    """a >= 0, b >= 0. Returns None when there are more than `levels` levels."""
    if n == 0:
        return 0
    top = (a * (n - 1) + b) // m
    if top > levels:
        return None
    total = (b // m) * n
    for k in range(b // m + 1, top + 1):
        need = k * m - b  # a i >= need
        i0 = 0 if need <= 0 else (-(-need // a) if a else n)
        total += max(0, n - i0)
    return total


def s_signed_reflect(n, m, a, b):
    """Exact S for any integer a, b through a reflection and a shift, then the classic recurrence."""
    if n == 0:
        return 0
    if a < 0:
        a, b = -a, a * (n - 1) + b  # j = n - 1 - i
    k = b // m
    return classic_f(a, b - k * m, m, n - 1) + n * k


def s_exact(n, m, a, b, loop_cap=64):
    """Exact S with cross-checks; a, b any integers."""
    r = s_signed_reflect(n, m, a, b)
    assert r == s_acl(n, m, a, b), (n, m, a, b)
    if n <= loop_cap:
        assert r == s_loop(n, m, a, b), (n, m, a, b)
    if a >= 0 and b >= 0:
        assert r == s_classic(n, m, a, b)
        c = s_count(n, m, a, b)
        if c is not None:
            assert r == c, (n, m, a, b)
    return r


U15, U16, U31, U32 = 1 << 15, 1 << 16, 1 << 31, 1 << 32
U_LIST = [0, 1, 2, 3, 4, U15 - 1, U15, U15 + 1, U16 - 1, U16, U16 + 1, U31 - 1, U31, U31 + 1, U32 - 2, U32 - 1]
N_LIST = U_LIST
M_LIST = [v for v in U_LIST if v >= 1]
I_LIST = [0, 1, -1, 2, -2, 3, -3, U15 - 1, U15, U15 + 1, -(U15 - 1), -U15, -(U15 + 1), U16 - 1, U16, U16 + 1, -(U16 - 1), -U16, -(U16 + 1),
          U31 - 1, U31 - 2, -U31, -U31 + 1]


def grp_unsigned_small():
    """n 0..24, m 1..24, a 0..30, b 0..30. Cumulative direct loop over n, plus the recurrences."""
    h = OFF
    count = 0
    for m in range(1, 25):
        for a in range(31):
            for b in range(31):
                tot = 0
                for n in range(25):
                    if n:
                        tot += (a * (n - 1) + b) // m
                    if (n + a + b + m) % 5 == 0:
                        assert tot == s_classic(n, m, a, b) == s_acl(n, m, a, b)
                    h = fold(h, tot & M64)
                    h = fold(h, tot % P)
                    count += 1
    return h, count


def grp_signed_small():
    """n 0..16, m 1..16, a and b each -64..64."""
    h = OFF
    count = 0
    for m in range(1, 17):
        for a in range(-64, 65):
            for b in range(-64, 65):
                tot = 0
                for n in range(17):
                    if n:
                        tot += (a * (n - 1) + b) // m
                    if (n * 7 + a + b + m) % 11 == 0:
                        assert tot == s_signed_reflect(n, m, a, b) == s_acl(n, m, a, b)
                    h = fold(h, tot & M64)
                    count += 1
    return h, count


def grp_grid_unsigned():
    h = OFF
    count = 0
    for n in N_LIST:
        for m in M_LIST:
            for a in U_LIST:
                for b in U_LIST:
                    s = s_exact(n, m, a, b)
                    h = fold(h, s & M64)
                    h = fold(h, s % P)
                    count += 1
    return h, count


def grp_grid_signed():
    h = OFF
    count = 0
    for n in N_LIST:
        for m in M_LIST:
            for a in I_LIST:
                for b in I_LIST:
                    s = s_exact(n, m, a, b)
                    h = fold(h, s & M64)
                    count += 1
    return h, count


class Lcg:
    def __init__(self, s):
        self.s = s

    def rnd(self, k):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return (self.s >> 33) % k

    def bits(self, maxbits):
        """Random value of random bit length 0..maxbits."""
        nb = self.rnd(maxbits + 1)
        if nb == 0:
            return 0
        v = 0
        for _ in range(3):
            v = (v << 31) | self.rnd(1 << 31)
        return (v & ((1 << nb) - 1)) | (1 << (nb - 1))


def table():
    """Rows (kind, n, m, a, b): kind 0 unsigned, 1 signed. Sampled, not exhaustive."""
    rows = []
    g = Lcg(12345)
    for _ in range(300):
        rows.append((0, g.bits(32), max(1, g.bits(32)), g.bits(32), g.bits(32)))
    for _ in range(300):
        a = g.bits(31) * (1 if g.rnd(2) else -1)
        b = g.bits(31) * (1 if g.rnd(2) else -1)
        rows.append((1, g.bits(32), max(1, g.bits(32)), a, b))
    # Exact sum far above 2^64 (n = 2^32 - 1 and large slopes), unsigned and signed.
    for m in (1, 2, 3, 7, U16, U31, U32 - 1):
        for a in (U32 - 1, U32 - 2, U31, U31 + 1, 3):
            for b in (0, 1, U32 - 1):
                rows.append((0, U32 - 1, m, a, b))
    for m in (1, 2, 3, 7, U16, U31, U32 - 1):
        for a in (U31 - 1, -U31, -U31 + 1, -3):
            for b in (U31 - 1, -U31, 0):
                rows.append((1, U32 - 1, m, a, b))
    # Closed forms: a = 0, m = 1, a = m.
    for n in (1, 2, 1000, U32 - 1):
        for b in (0, 5, U32 - 1):
            rows.append((0, n, 1, 0, b))
            rows.append((0, n, 1, 7, b))
            rows.append((0, n, 977, 977, b))
    # Negative inputs with m = 1 and m = 2^32 - 1 (the cases of the INT32_MIN argument).
    for n in (1, 2, 3, 65536, U32 - 1):
        for m in (1, U32 - 1, U31, U31 + 1, 2):
            for a, b in ((-U31, -U31), (-U31, 0), (0, -U31), (-U31, U31 - 1), (U31 - 1, -U31), (-1, -1)):
                rows.append((1, n, m, a, b))
    return rows


def main():
    check = len(sys.argv) > 1 and sys.argv[1] == 'check'
    groups = [("kUnsignedSmall", grp_unsigned_small), ("kSignedSmall", grp_signed_small), ("kGridUnsigned", grp_grid_unsigned),
              ("kGridSigned", grp_grid_signed)]
    gd = []
    for name, f in groups:
        d, c = f()
        gd.append((name, d, c))
    rows = []
    for kind, n, m, a, b in table():
        s = s_exact(n, m, a, b, loop_cap=300)
        if kind == 0:
            assert a >= 0 and b >= 0
        rows.append((kind, n, m, a, b, s & M64, s % P if kind == 0 else 0, 1 if abs(s) >= (1 << 64) else 0))
    if check:
        print("ok", [(n, c) for n, d, c in gd], len(rows))
        return
    print("// Generated by tools/floorsum_ref.py. Do not edit.")
    print("struct Row {")
    print("  u32 kind;")
    print("  u32 n;")
    print("  u32 m;")
    print("  i64 a;")
    print("  i64 b;")
    print("  u64 wrap;")
    print("  u32 mod;")
    print("  u32 big;")
    print("};")
    for name, d, c in gd:
        print("const u64 %s = 0x%016Xull;" % (name, d))
        print("const u64 %sCount = %dull;" % (name, c))
    print("const Row kRows[] = {")
    for r in rows:
        print("    {%du, %du, %du, %dll, %dll, 0x%016Xull, %du, %du}," % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]))
    print("};")


if __name__ == '__main__':
    main()
