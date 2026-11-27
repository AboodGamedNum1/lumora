#!/usr/bin/env python3
"""Independent reference for core/digitcount (how many integers in 0..n have digit sum s in base b, and the sum of the digit sums of 0..n).

Standard library only (no NumPy, no third-party package). Runtime: Python 3.8 or newer; the timing is stated in docs/digitcount.md.
Usage:
  python3 tools/digitcount_ref.py check   compare the two oracles on the small extents and on sampled large inputs, print "OK"
  python3 tools/digitcount_ref.py emit    print the golden digests as C++ source for tests/digitcount/digitcount_tests.cpp (runs the checks first)
Oracles of different shape:
  scan oracle (THE oracle, O(n)): the digit sum of every x in 0..n by repeated division, counted one by one; the total is the plain sum. It is infeasible at
     n = 2^40 (10^12 integers), so the digests of the large inputs do NOT come from it.
  generating-function oracle (O(digits * base) big-integer operations): a polynomial is packed into one Python integer with 64 bits per coefficient;
     G(x) = 1 + x + ... + x^(b-1); the count polynomial of n is x^(digit sum of n) + the sum over positions i of x^pre(i) * (1 + ... + x^(d_i - 1)) * G^i,
     where d_i is the digit of n at position i and pre(i) the sum of the digits above it; the total is the sum of s * coefficient. Compared with the scan on
     every n up to 1500 for every base 2..64, on a stride through n up to 2^16 and on the neighbours of the powers of every base up to 2^16.
For one n and one base the result words are: count(n, s) for s = 0 .. (b - 1) * k + 1, where k is the number of base-b digits of n (0 for n = 0), then the total.
Digest: FNV-1a 64-bit over 64-bit words (one xor and one multiply per word); a group digest folds its results in the stated order.
"""
import sys

MAXN = 1 << 40
M64 = 0xFFFFFFFFFFFFFFFF
BASES = list(range(2, 65))
SMALL_N = 200


def fnv(words, h=0xcbf29ce484222325):
    for w in words:
        h = ((h ^ w) * 0x100000001b3) & M64
    return h


def digits(n, b):
    d = []
    while n:
        d.append(n % b)
        n //= b
    return d  # least significant first


def digit_sum(x, b):
    s = 0
    while x:
        s += x % b
        x //= b
    return s


def words_len(n, b):
    return (b - 1) * len(digits(n, b)) + 2


def scan_result(n, b):
    k = len(digits(n, b))
    cnt = [0] * ((b - 1) * k + 2)
    total = 0
    for x in range(n + 1):
        ds = digit_sum(x, b)
        cnt[ds] += 1
        total += ds
    return cnt, total


def gf_result(n, b):
    K = 64
    X = 1 << K
    mask = X - 1
    d = digits(n, b)
    k = len(d)
    G = (X ** b - 1) // (X - 1)
    P = X ** digit_sum(n, b)
    pw = 1  # G^i
    pre_all = sum(d)
    pre = pre_all
    for i in range(k):
        pre -= d[i]
        P += (X ** pre) * ((X ** d[i] - 1) // (X - 1)) * pw
        pw *= G
    size = (b - 1) * k + 2
    cnt = [(P >> (K * t)) & mask for t in range(size)]
    assert P >> (K * size) == 0
    total = sum(t * c for t, c in enumerate(cnt))
    return cnt, total


def result_words(res):
    cnt, total = res
    return list(cnt) + [total]


def boundary_ns(b):
    out = set()
    p = 1
    while p <= MAXN:
        for n in (p - 1, p, p + 1):
            if 0 <= n <= MAXN:
                out.add(n)
        p *= b
    out.add(MAXN - 1)
    out.add(MAXN)
    return sorted(out)


def random_ns(b):
    x = (0x9E3779B97F4A7C15 + b) & M64
    out = []
    for _ in range(40):
        x ^= (x << 13) & M64
        x ^= x >> 7
        x ^= (x << 17) & M64
        out.append(x % (MAXN + 1))
    return out


def pad(cnt, size):
    return list(cnt) + [0] * (size - len(cnt))


def checks():
    # The scan oracle runs once over every x up to 2^16 for every base (running counts); the generating function is compared at every n up to 1500, on a stride
    # of 97 up to 2^16 and on the neighbours of every power of the base up to 2^16.
    for b in BASES:
        keep = set(range(0, 1501)) | set(range(1501, 1 << 16, 97))
        p = 1
        while p <= (1 << 16):
            keep.update(n for n in (p - 1, p, p + 1) if 0 <= n <= (1 << 16))
            p *= b
        cnt = {}
        total = 0
        for n in range((1 << 16) + 1):
            ds = digit_sum(n, b)
            cnt[ds] = cnt.get(ds, 0) + 1
            total += ds
            if n in keep:
                g, gt = gf_result(n, b)
                size = len(g)
                assert gt == total, (n, b, "total")
                assert g == [cnt.get(t, 0) for t in range(size)], (n, b)
                assert max(cnt) < size
    # cross identities on the big inputs
    for b in BASES:
        for n in boundary_ns(b) + random_ns(b):
            cnt, total = gf_result(n, b)
            assert sum(cnt) == n + 1, (n, b)
            assert total == sum(t * c for t, c in enumerate(cnt))
            if n > 0:
                c1, t1 = gf_result(n - 1, b)
                ds = digit_sum(n, b)
                assert t1 + ds == total
                for t in range(len(cnt)):
                    assert cnt[t] - (c1[t] if t < len(c1) else 0) == (1 if t == ds else 0), (n, b, t)
    return True


def emit():
    print("// Generated by tools/digitcount_ref.py emit")
    for name, fn in (("kSmallDigest", lambda b: [n for n in range(SMALL_N + 1)]), ("kBoundaryDigest", boundary_ns), ("kRandomDigest", random_ns)):
        print("constexpr std::uint64_t %s[63] = {  // per base 2..64" % name)
        row = []
        for b in BASES:
            h = 0xcbf29ce484222325
            for n in fn(b):
                res = scan_result(n, b) if name == "kSmallDigest" else gf_result(n, b)
                h = fnv(result_words(res), h)
            row.append("0x%016xull" % h)
        for i in range(0, 63, 3):
            print("    " + ", ".join(row[i:i + 3]) + ",")
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
