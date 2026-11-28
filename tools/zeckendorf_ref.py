#!/usr/bin/env python3
"""Independent reference for core/zeckendorf (Fibonacci numbers and Zeckendorf representations).

Standard library only. Python integers are unbounded, so there is no 64-bit overflow to reason about: the value of a list is its exact bigint sum and TooLarge is
"the sum is above 2^64 - 1". The implementation uses a pair walk in u64; this reference uses a bigint Fibonacci list, bisect and subtraction.
Usage:
  python3 tools/zeckendorf_ref.py chunk K     digest of (n, count, indices) for n in [K * 2^20, (K + 1) * 2^20) (K = 0..3; K = 3 also holds 2^22)
  python3 tools/zeckendorf_ref.py samp K      digest of the same words for the 50000 sampled u64 values of shard K = 0..3
  python3 tools/zeckendorf_ref.py rlist K     digest of (len, indices, code, value) for the 50000 random valid index lists of shard K = 0..1
  python3 tools/zeckendorf_ref.py struct      digest of the structured n values (F(k) - 1, F(k), F(k) + 1, 2^k - 1, 2^k, 2^k + 1, 2^64 - 1, 2^64 - 2)
  python3 tools/zeckendorf_ref.py arith       digest of every arithmetic index list
  python3 tools/zeckendorf_ref.py alpha       digest of every list of length 0..4 over the 16 boundary indices
  python3 tools/zeckendorf_ref.py pairs       digest of every list (p, q) with p, q in 0..255
  python3 tools/zeckendorf_ref.py triples     digest of every list (a, b, c) with a, b, c in 0..12
  python3 tools/zeckendorf_ref.py planted     digest of the planted-error lists
  python3 tools/zeckendorf_ref.py around      digest of every single-term change of the lists of 2^64 - 1 and 2^64
  python3 tools/zeckendorf_ref.py fibk        digest of F(k) for k = 0..93
  python3 tools/zeckendorf_ref.py check       the identities and the header examples; also the enumeration cross-check; prints "OK"
  python3 tools/zeckendorf_ref.py emit        print the golden values as C++ source for tests/zeckendorf/zeckendorf_tests.cpp
Digest: FNV-1a 64-bit over u64 words. Codes: Ok 0, NullArgument 1, BadArgument 2, BufferTooSmall 3, TooLarge 4.
"""
import itertools
import sys
from bisect import bisect_right

M64 = (1 << 64) - 1
FNV0 = 0xcbf29ce484222325
OK, NULL, BAD, BUF, BIG = 0, 1, 2, 3, 4

FIB = [0, 1]
while len(FIB) < 100:
    FIB.append(FIB[-1] + FIB[-2])


def step(h, w):
    return ((h ^ w) * 0x100000001b3) & M64


def zeck(n):
    """indices of n, largest first, by bisect and subtraction on the bigint list (n >= 0)."""
    out = []
    while n > 0:
        k = bisect_right(FIB, n, 2) - 1
        out.append(k)
        n -= FIB[k]
    return out


def verdict(lst):
    """(code, value) of the index list under the header: the whole list is checked first, then the sum."""
    prev = None
    for v in lst:
        if v < 2 or v > 93:
            return BAD, 0
        if prev is not None and v + 2 > prev:
            return BAD, 0
        prev = v
    total = sum(FIB[v] for v in lst)
    if total > M64:
        return BIG, 0
    return OK, total


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def dig_n(h, n):
    z = zeck(n)
    h = step(step(h, n), len(z))
    for k in z:
        h = step(h, k)
    return h


def dig_list(h, lst):
    code, val = verdict(lst)
    h = step(h, len(lst))
    for v in lst:
        h = step(h, v)
    return step(step(h, code), val)


def chunk_digest(k):
    lo = k << 20
    hi = (k + 1) << 20
    if k == 3:
        hi += 1
    h = FNV0
    for n in range(lo, hi):
        h = dig_n(h, n)
    return h


def samp_digest(k):
    h = FNV0
    for i in range(k * 50000, (k + 1) * 50000):
        h = dig_n(h, splitmix_at(i))
    return h


def rlist(i):
    base = 1 << 43
    r0 = splitmix_at(4 * i + base)
    r1 = splitmix_at(4 * i + 1 + base)
    r2 = splitmix_at(4 * i + 2 + base)
    cur = 93 - r0 % 6
    length = 1 + r1 % 46
    out = []
    for j in range(length):
        if cur < 2:
            break
        out.append(cur)
        cur -= 2 + ((r2 >> j) & 1)
    return out


def rlist_digest(k):
    h = FNV0
    for i in range(k * 50000, (k + 1) * 50000):
        h = dig_list(h, rlist(i))
    return h


def struct_values():
    v = []
    for k in range(2, 94):
        v += [FIB[k] - 1, FIB[k], FIB[k] + 1]
    for k in range(0, 64):
        v += [(1 << k) - 1, 1 << k, (1 << k) + 1]
    v += [M64, M64 - 1]
    return v


def struct_digest():
    h = FNV0
    for n in struct_values():
        h = dig_n(h, n)
    return h


def arith_lists():
    out = []
    for d in range(2, 7):
        for a in range(2, 94):
            cur = a
            lst = []
            while cur >= 2:
                lst.append(cur)
                out.append(list(lst))
                cur -= d
    return out


def digest_lists(lists):
    h = FNV0
    for l in lists:
        h = dig_list(h, l)
    return h


ALPHA = [0, 1, 2, 3, 4, 5, 6, 7, 90, 91, 92, 93, 94, 95, 96, 255]


def alpha_lists():
    out = []
    for n in range(0, 5):
        out += [list(t) for t in itertools.product(ALPHA, repeat=n)]
    return out


def pairs_lists():
    return [[p, q] for p in range(256) for q in range(256)]


def triples_lists():
    return [[a, b, c] for a in range(13) for b in range(13) for c in range(13)]


B1 = zeck(M64)
B2 = list(range(92, 1, -2))
B3 = list(range(93, 2, -2))
B4 = [93, 91, 88, 86, 82, 76, 74, 70, 66, 63, 59, 56, 47, 44, 40, 37, 33, 31, 29, 25, 19, 17, 13, 11, 6]


def planted_lists():
    out = []
    for base in (B1, B2, B3, B4):
        n = len(base)
        for i in range(n):
            if i > 0:
                out.append(base[:i] + [base[i - 1]] + base[i + 1:])
                out.append(base[:i] + [base[i - 1] - 1] + base[i + 1:])
            for v in (0, 1, 94, 95, 255):
                out.append(base[:i] + [v] + base[i + 1:])
            out.append(base[:i + 1] + [base[i]] + base[i + 1:])
            out.append(base[:i + 1] + [base[i] - 1] + base[i + 1:])
            out.append(base[:i] + base[i + 1:])
            if i + 1 < n:
                sw = list(base)
                sw[i], sw[i + 1] = sw[i + 1], sw[i]
                out.append(sw)
        out.append(base + [94])
        out.append(base + [1])
        out.append(base + [0])
    return out


def around_lists():
    out = []
    for base in (B1, B4):
        n = len(base)
        for i in range(n):
            for v in range(96):
                out.append(base[:i] + [v] + base[i + 1:])
            out.append(base[:i] + base[i + 1:])
        for i in range(n + 1):
            for v in range(96):
                out.append(base[:i] + [v] + base[i:])
    return out


def fibk_digest():
    h = FNV0
    for k in range(94):
        h = step(step(h, k), FIB[k])
    return h


def checks():
    assert FIB[93] <= M64 < FIB[94]
    assert FIB[93] == 12200160415121876738 and FIB[94] == 19740274219868223167
    assert zeck(M64) == [93, 91, 88, 86, 82, 76, 74, 70, 66, 63, 59, 56, 47, 44, 40, 37, 33, 31, 29, 25, 19, 17, 13, 11, 5, 3]
    assert zeck(1 << 63) == [92, 88, 86, 84, 75, 71, 67, 63, 61, 54, 51, 49, 42, 40, 38, 34, 31, 27, 25, 22, 20, 17, 12, 8]
    assert zeck(FIB[93] - 1) == list(range(92, 1, -2)) and len(B2) == 46
    assert verdict(B1) == (OK, M64) and verdict(B4) == (BIG, 0) and sum(FIB[v] for v in B4) == 1 << 64
    assert verdict([93, 91]) == (OK, 16860207025497407047)
    assert verdict([93, 91, 89]) == (BIG, 0) and sum(FIB[v] for v in (93, 91, 89)) == 18640186441502121236
    assert verdict([3, 3])[0] == BAD and verdict([4, 3])[0] == BAD and verdict([1])[0] == BAD and verdict([94])[0] == BAD
    assert verdict([]) == (OK, 0) and verdict(B3) == (BIG, 0) and sum(FIB[v] for v in B3) == FIB[94] - 1 > M64
    # no valid list has 47 terms: 47 indices at least 2 apart from 2 would end at 2 + 2 * 46 = 94 or more
    assert 2 + 2 * 46 == 94
    # cross-check by enumeration: the valid lists with every index at most K, in increasing order of value, are exactly n = 0, 1, 2, ...
    def gen(K):
        if K < 2:
            yield []
            return
        yield from gen(K - 1)
        for l in gen(K - 2):
            yield [K] + l
    for n, l in enumerate(gen(24)):
        assert sum(FIB[v] for v in l) == n and zeck(n) == l
    assert n + 1 == FIB[25]
    # a mixed sample: random valid lists give back their own list when the sum fits
    for i in range(3000):
        l = rlist(i)
        c, v = verdict(l)
        if c == OK:
            assert zeck(v) == l
        else:
            assert c == BIG and sum(FIB[x] for x in l) > M64


def hx(v):
    return "0x%016xull" % v


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    arg = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    if cmd == "chunk":
        print(hx(chunk_digest(arg)))
    elif cmd == "samp":
        print(hx(samp_digest(arg)))
    elif cmd == "rlist":
        print(hx(rlist_digest(arg)))
    elif cmd == "struct":
        print(len(struct_values()), hx(struct_digest()))
    elif cmd == "arith":
        print(len(arith_lists()), hx(digest_lists(arith_lists())))
    elif cmd == "alpha":
        print(len(alpha_lists()), hx(digest_lists(alpha_lists())))
    elif cmd == "pairs":
        print(hx(digest_lists(pairs_lists())))
    elif cmd == "triples":
        print(hx(digest_lists(triples_lists())))
    elif cmd == "planted":
        print(len(planted_lists()), hx(digest_lists(planted_lists())))
    elif cmd == "around":
        print(len(around_lists()), hx(digest_lists(around_lists())))
    elif cmd == "fibk":
        print(hx(fibk_digest()))
    elif cmd == "check":
        checks()
        print("OK")
    elif cmd == "emit":
        def arr(name, vals):
            print("constexpr std::uint64_t %s[%d] = {%s};" % (name, len(vals), ", ".join(hx(v) for v in vals)))
        arr("kChunkDigest", [chunk_digest(k) for k in range(4)])
        arr("kSampDigest", [samp_digest(k) for k in range(4)])
        arr("kRlistDigest", [rlist_digest(k) for k in range(2)])
        sv = struct_values()
        print("constexpr std::size_t kStructCount = %d;" % len(sv))
        print("constexpr std::uint64_t kStructDigest = %s;" % hx(struct_digest()))
        al = arith_lists()
        print("constexpr std::size_t kArithCount = %d;" % len(al))
        print("constexpr std::uint64_t kArithDigest = %s;" % hx(digest_lists(al)))
        al = alpha_lists()
        print("constexpr std::size_t kAlphaCount = %d;" % len(al))
        print("constexpr std::uint64_t kAlphaDigest = %s;" % hx(digest_lists(al)))
        print("constexpr std::uint64_t kPairsDigest = %s;" % hx(digest_lists(pairs_lists())))
        print("constexpr std::uint64_t kTriplesDigest = %s;" % hx(digest_lists(triples_lists())))
        al = planted_lists()
        print("constexpr std::size_t kPlantedCount = %d;" % len(al))
        print("constexpr std::uint64_t kPlantedDigest = %s;" % hx(digest_lists(al)))
        al = around_lists()
        print("constexpr std::size_t kAroundCount = %d;" % len(al))
        print("constexpr std::uint64_t kAroundDigest = %s;" % hx(digest_lists(al)))
        print("constexpr std::uint64_t kFibkDigest = %s;" % hx(fibk_digest()))
        tl = [sum(1 for i in range(k * 50000, (k + 1) * 50000) if verdict(rlist(i))[0] == BIG) for k in range(2)]
        print("constexpr std::uint32_t kRlistTooLarge[2] = {%du, %du};" % (tl[0], tl[1]))
    else:
        sys.exit("unknown command")


if __name__ == "__main__":
    main()
