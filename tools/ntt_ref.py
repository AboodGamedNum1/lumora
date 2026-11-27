#!/usr/bin/env python3
"""Reference for core/ntt. Prints data.inc (digests and the case table) for tests/ntt/ntt_tests.cpp.

Three independent methods, none of them a transform:
  1. schoolbook: the defining double sum with Python integers (used for every case with na * nb up to 4 million, and for the exhaustive group);
  2. Kronecker substitution: each input is packed into one Python big integer with 80 bits per coefficient (the largest true coefficient is below
     2^18 * (p - 1)^2 < 2^78, so no field overflows), the two big integers are multiplied by Python's own multiplication, and the fields are read back
     modulo p. This is the reference for the large cases;
  3. polynomial evaluation: for the largest cases, out(x) = a(x) * b(x) modulo p at several fixed points x (Horner), a check of the Kronecker result.
The closed form for the all (p - 1) inputs (every product is 1 modulo p, so out[k] is the number of terms) is a fourth check for those cases.
Every method is cross-checked against the others while the data is generated; a disagreement aborts. Run `python3 tools/ntt_ref.py check` to
check without printing.

Digest of a result: FNV-1a over the length (4 bytes, little endian) then every entry (4 bytes, little endian). The inputs are generated from
(na, nb, mode, seed) by the same generator as in the test file: a 64-bit LCG (s = s * 6364136223846793005 + 1442695040888963407, value = s >> 33),
a first, then b (b is a copy of a when the flag SQ is set).
  mode 0: value % p;  mode 1: p - 1 - value % 4;  mode 2: value % 3;  mode 3: (value % 5 == 0) ? (next value) % p : 0;  mode 4: p - 1;  mode 5: 0.
"""
import sys

P = 998244353
FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
MAXLEN = 1 << 18
SQ = 1
ALIAS = 2


def fold(h, v, nbytes):
    for _ in range(nbytes):
        h = ((h ^ (v & 0xFF)) * FNV_PRIME) & M64
        v >>= 8
    return h


def digest_out(h, out):
    h = fold(h, len(out), 4)
    for v in out:
        h = fold(h, v, 4)
    return h


class Lcg:
    def __init__(self, s):
        self.s = s

    def rnd(self, k):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return (self.s >> 33) % k


def entry(g, mode):
    if mode == 0:
        return g.rnd(P)
    if mode == 1:
        return P - 1 - g.rnd(4)
    if mode == 2:
        return g.rnd(3)
    if mode == 3:
        return g.rnd(P) if g.rnd(5) == 0 else 0
    if mode == 4:
        return P - 1
    return 0


def gen(na, nb, mode, seed, flags):
    g = Lcg(seed)
    a = [entry(g, mode) for _ in range(na)]
    b = list(a) if flags & SQ else [entry(g, mode) for _ in range(nb)]
    return a, b


def schoolbook(a, b):
    if not a or not b:
        return []
    out = [0] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        if x:
            for j, y in enumerate(b):
                out[i + j] += x * y
    return [v % P for v in out]


def kron(a, b):
    if not a or not b:
        return []
    pa = int.from_bytes(b''.join(x.to_bytes(10, 'little') for x in a), 'little')
    pb = int.from_bytes(b''.join(x.to_bytes(10, 'little') for x in b), 'little')
    r = len(a) + len(b) - 1
    by = (pa * pb).to_bytes(10 * r + 10, 'little')
    return [int.from_bytes(by[10 * i:10 * i + 10], 'little') % P for i in range(r)]


def horner(c, x):
    v = 0
    for t in reversed(c):
        v = (v * x + t) % P
    return v


def reference(a, b):
    """Result by Kronecker, cross-checked."""
    out = kron(a, b)
    if len(a) * len(b) <= 4000000:
        assert out == schoolbook(a, b), "kron vs schoolbook"
    else:
        for x in (2, 3, 123456789, P - 1, 77777777):
            assert horner(out, x) == horner(a, x) * horner(b, x) % P, "evaluation"
    return out


def cases():
    """The case table: (na, nb, mode, seed, flags)."""
    t = []
    seed = 7000
    idx = 0
    seen = set()
    for k in range(1, 20):
        for r in ((1 << k) - 1, 1 << k, (1 << k) + 1):
            if r < 1 or r > 2 * MAXLEN - 1:
                continue
            for nb in (1, 2, (r + 1) // 2, (r + 2) // 2, r):
                na = r + 1 - nb
                if na < 1 or nb < 1 or na > MAXLEN or nb > MAXLEN or (na, nb) in seen:
                    continue
                seen.add((na, nb))
                t.append((na, nb, idx % 4, seed, ALIAS if r <= 5000 else 0))
                seed += 1
                idx += 1
    # r = 1, 2, 3 and a few odd shapes
    for na, nb in ((1, 1), (1, 2), (2, 1), (2, 2), (3, 5), (5, 3), (7, 7), (17, 100), (100, 17), (1000, 1500), (1500, 1000)):
        if (na, nb) not in seen:
            seen.add((na, nb))
            t.append((na, nb, idx % 4, seed, ALIAS))
            seed += 1
            idx += 1
    # all (p - 1), all zero
    for n in (1, 2, 3, 17, 1000, MAXLEN):
        t.append((n, n, 4, seed, 0))
        seed += 1
    t.append((64, 100, 5, seed, 0))
    seed += 1
    # largest extents
    for mode in range(4):
        t.append((MAXLEN, MAXLEN, mode, seed, 0))
        seed += 1
    t.append((MAXLEN, MAXLEN, 0, seed, SQ))
    seed += 1
    t.append((MAXLEN, MAXLEN, 3, seed, SQ))
    seed += 1
    for na, nb in ((MAXLEN, 1), (1, MAXLEN), (MAXLEN, 2), (2, MAXLEN), (MAXLEN, MAXLEN // 2 + 3), (MAXLEN - 1, MAXLEN), (MAXLEN, 40), (131073, 131073)):
        t.append((na, nb, 0, seed, 0))
        seed += 1
    # small square cases for the a == b alias
    for n in (1, 2, 3, 5, 8, 16, 33, 100, 1000):
        t.append((n, n, n % 4, seed, SQ | ALIAS))
        seed += 1
    return t


def exhaustive():
    h = FNV_OFF
    for na in range(0, 41):
        for nb in range(0, 41):
            a, b = gen(na, nb, 0, 100000 + na * 41 + nb, 0)
            out = schoolbook(a, b)
            assert out == kron(a, b)
            h = digest_out(h, out)
    return h


def main():
    check = len(sys.argv) > 1 and sys.argv[1] == 'check'
    rows = []
    for (na, nb, mode, seed, flags) in cases():
        a, b = gen(na, nb, mode, seed, flags)
        out = reference(a, b)
        if mode == 4:
            expect = [min(k + 1, na, nb, na + nb - 1 - k) % P for k in range(na + nb - 1)]
            assert out == expect, "closed form"
        if mode == 5:
            assert not any(out)
        rows.append((na, nb, mode, seed, flags, digest_out(FNV_OFF, out)))
    ex = exhaustive()
    if check:
        print("ok", len(rows))
        return
    print("// Generated by tools/ntt_ref.py. Do not edit.")
    print("struct Case {")
    print("  u32 na;")
    print("  u32 nb;")
    print("  u32 mode;")
    print("  u64 seed;")
    print("  u32 flags;")
    print("  u64 digest;")
    print("};")
    print("const u64 kExhaustiveDigest = 0x%016Xull;" % ex)
    print("const Case kCases[] = {")
    for r in rows:
        print("    {%du, %du, %du, %dull, %du, 0x%016Xull}," % r)
    print("};")


if __name__ == '__main__':
    main()
