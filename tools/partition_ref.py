#!/usr/bin/env python3
"""Reference for core/partition (Python standard library only), and the data included by tests/partition/partition_tests.cpp.

p(v): partitions of v. q(v): partitions of v into distinct parts. p_k(v): partitions of v into at most k parts (= with every part at most k).
Usage:
  python3 tools/partition_ref.py          print the data (digests and values) on stdout
  python3 tools/partition_ref.py check    run all the cross-checks without printing the data

Methods (none is the recurrence the implementation uses for its own table; a disagreement aborts):
  1. part-by-part table in exact Python integers (no reduction): p_k from parts 1 .. k, q from a 0/1 table. Used for every small extent, and reduced mod p.
  2. the same table modulo p with strided prefix sums (itertools.accumulate on each residue class), with a snapshot after every part size up to a bound.
  3. Newton inversion of the Euler series E(x) = prod (1 - x^i) with Kronecker packed big-integer products, giving P = 1/E modulo x^65537 (mod p),
     then Q = P * E(x^2); each result is verified by the product identities P * E = 1 and Q * E = E(x^2) at full size. E itself is checked against the
     defining product for n up to 400 in exact integers.
  4. for k >= n / 2 - 1 (so v <= 2k + 2): p_k(v) = p(v) - sum over m = k + 1 .. v of p(v - m) (the partitions with a part above k, which leaves v - m < m).
Digest: FNV-1a over 64-bit words: h = (h ^ word) * 0x100000001B3 mod 2^64, from 0xCBF29CE484222325; a word is one table entry (mod 998244353); the tables of
a group are folded in the order stated next to each group below.
"""
import sys
from itertools import accumulate

P = 998244353
M64 = (1 << 64) - 1
OFF = 0xCBF29CE484222325
PRIME = 0x100000001B3
MAXN = 65536


def fold(h, w):
    return ((h ^ (w & M64)) * PRIME) & M64


def exact_p_table(n):
    t = [1] + [0] * n
    for i in range(1, n + 1):
        for v in range(i, n + 1):
            t[v] += t[v - i]
    return t


def exact_q_table(n):
    t = [1] + [0] * n
    for i in range(1, n + 1):
        for v in range(n, i - 1, -1):
            t[v] += t[v - i]
    return t


def exact_pk_tables(n, kmax):
    """rows[k][v] = p_k(v), exact, v = 0 .. n, k = 0 .. kmax."""
    t = [1] + [0] * n
    rows = [t[:]]
    for k in range(1, kmax + 1):
        if k <= n:
            for v in range(k, n + 1):
                t[v] += t[v - k]
        rows.append(t[:])
    return rows


def snapshots_mod(n, kset):
    """p_k(v) mod P for v = 0 .. n and each k in kset, by strided prefix sums, one pass over k = 1 .. max(kset)."""
    t = [1] + [0] * n
    out = {}
    kmax = max(kset)
    if 0 in kset:
        out[0] = t[:]
    for i in range(1, kmax + 1):
        if i <= n:
            for r in range(i):
                s = list(accumulate(t[r::i]))
                t[r::i] = [x % P for x in s]
        if i in kset:
            out[i] = t[:]
    return out


def pack(c):
    return int.from_bytes(b''.join(x.to_bytes(10, 'little') for x in c), 'little')


def mulmod(a, b, L):
    """First L coefficients of a * b modulo P (a, b lists of residues). 80 bit slots: a slot of the product is below L * P^2 < 2^80."""
    a = a[:L]
    b = b[:L]
    prod = pack(a) * pack(b)
    prod &= (1 << (80 * L)) - 1
    bs = prod.to_bytes(10 * L, 'little')
    return [int.from_bytes(bs[10 * i:10 * i + 10], 'little') % P for i in range(L)]


def euler_series(L, scale=1):
    """E(x^scale) = sum over j in Z of (-1)^j x^(scale j (3j - 1) / 2), as L coefficients mod P."""
    e = [0] * L
    e[0] = 1
    j = 1
    while True:
        g1 = scale * (j * (3 * j - 1) // 2)
        if g1 >= L:
            break
        g2 = scale * (j * (3 * j + 1) // 2)
        sg = P - 1 if j % 2 else 1
        e[g1] = sg
        if g2 < L:
            e[g2] = sg
        j += 1
    return e


def newton_inverse(f, L):
    g = [1]
    m = 1
    while m < L:
        m2 = min(2 * m, L)
        h = mulmod(f, g, m2)
        t = [(-x) % P for x in h]
        t[0] = (t[0] + 2) % P
        g = mulmod(g + [0] * (m2 - len(g)), t, m2)
        m = m2
    return g


def big_tables(n):
    """p mod P and q mod P for v = 0 .. n through Newton inversion and Kronecker products, verified by the product identities."""
    L = n + 1
    e = euler_series(L)
    p = newton_inverse(e, L)
    chk = mulmod(p, e, L)
    assert chk[0] == 1 and not any(chk[1:]), "P * E != 1"
    e2 = euler_series(L, 2)
    q = mulmod(p, e2, L)
    chk = mulmod(q, e, L)
    assert chk == e2, "Q * E != E(x^2)"
    return p, q


def fold_list(h, lst):
    for x in lst:
        h = fold(h, x)
    return h


# Extents ---------------------------------------------------------------------------------------------------------------------------------------
ALLQ_N = 300          # every n 0..300, partition_all and partition_distinct (digest of the tables in order of n)
ATK_N, ATK_K = 200, 210  # every n 0..200 and every k 0..210, at_most_k (tables in order n, then k)
GRID_N = [127, 128, 255, 256, 257, 1000, 4095, 4096, 4097, 10000]
GRID_K = [0, 1, 2, 3, 15, 16, 17, 63, 64, 65, 100, 255, 256, 257, 300, 1000, 4096, 65535, 65536]
LARGE_K = [0, 1, 2, 255, 256, 257, 65535, 65536]  # at n = 65536
IDX = [0, 1, 2, 3, 10, 100, 1000, 10000, 32768, 65535, 65536]
COLL = (7667, 2589)   # see group 5
GRID_DP_MAX = 1000    # k up to this by the part-by-part table; larger k only where n <= 2 k + 2 (identity 4)


def grid_pairs():
    pairs = []
    for n in GRID_N:
        ks = sorted(set(GRID_K + [n - 1, n, n + 1]))
        for k in ks:
            if k <= MAXN and (k <= GRID_DP_MAX or n <= 2 * k + 2):
                pairs.append((n, k))
    return pairs


def identity_table(ptab, n, k):
    """p_k(v) for v = 0 .. n when n <= 2 k + 2, from the unrestricted table ptab (mod P)."""
    assert n <= 2 * k + 2
    out = ptab[:n + 1]
    out = out[:]
    # sum over m = k + 1 .. v of p(v - m) = prefix sum of p over 0 .. v - k - 1
    pre = 0
    for v in range(k + 1, n + 1):
        pre = (pre + ptab[v - k - 1]) % P
        out[v] = (out[v] - pre) % P
    return out


def main():
    check = len(sys.argv) > 1 and sys.argv[1] == 'check'
    # Known values, verified here before the tests rely on them.
    pe = exact_p_table(1000)
    qe = exact_q_table(1000)
    assert pe[:11] == [1, 1, 2, 3, 5, 7, 11, 15, 22, 30, 42]
    assert qe[:11] == [1, 1, 1, 2, 2, 3, 4, 5, 6, 8, 10]
    assert pe[100] == 190569292 and qe[100] == 444793
    assert pe[1000] == 24061467864032622473692149727991
    # E against its defining product, exact, n <= 400
    prod = [1] + [0] * 400
    for i in range(1, 401):
        for v in range(400, i - 1, -1):
            prod[v] -= prod[v - i]
    e = euler_series(401)
    assert [x % P for x in prod] == e
    # Group 1: every n 0..300.
    h_all = OFF
    h_dis = OFF
    p300 = [x % P for x in exact_p_table(ALLQ_N)]
    q300 = [x % P for x in exact_q_table(ALLQ_N)]
    for n in range(ALLQ_N + 1):
        h_all = fold_list(h_all, p300[:n + 1])
        h_dis = fold_list(h_dis, q300[:n + 1])
    # Group 2: at most k, every n 0..200 and k 0..210.
    rows = exact_pk_tables(ATK_N, ATK_K)
    h_atk = OFF
    for n in range(ATK_N + 1):
        for k in range(ATK_K + 1):
            h_atk = fold_list(h_atk, [x % P for x in rows[k][:n + 1]])
    assert [x % P for x in rows[ATK_K][:ATK_N + 1]] == [x % P for x in exact_p_table(ATK_N)]  # k >= n: unrestricted
    # Big tables.
    pbig, qbig = big_tables(MAXN)
    assert pbig[:301] == p300 and qbig[:301] == q300
    assert pbig[:1001] == [x % P for x in pe] and qbig[:1001] == [x % P for x in qe]
    # Group 3: grid. k <= 1000 from the strided table (cross-checked with the exact table up to 300), larger k by the identity.
    dp = snapshots_mod(10000, set(k for n, k in grid_pairs() if k <= GRID_DP_MAX) | {0})
    for k in (0, 1, 2, 3, 15, 16, 17, 63, 64, 65, 100, 255, 256, 257, 300):
        if k in dp:
            assert dp[k][:301] == [x % P for x in exact_pk_tables(300, k)[k]] if k <= 300 else True
    h_grid = OFF
    pairs = grid_pairs()
    for n, k in pairs:
        if k <= GRID_DP_MAX:
            tab = dp[k][:n + 1]
            if n <= 2 * k + 2:
                assert tab == identity_table(pbig, n, k)  # both methods where both apply
        else:
            tab = identity_table(pbig, n, k)
        h_grid = fold_list(h_grid, tab)
    # Group 4: n = 65536.
    h_pall = fold_list(OFF, pbig)
    h_pdis = fold_list(OFF, qbig)
    big_dp = snapshots_mod(MAXN, {0, 1, 2, 255, 256, 257})
    large = []
    for k in LARGE_K:
        if k in big_dp:
            tab = big_dp[k]
        else:
            tab = identity_table(pbig, MAXN, k)
        large.append(fold_list(OFF, tab))
    assert big_dp[1] == [1] * (MAXN + 1)
    assert big_dp[2] == [(v // 2 + 1) % P for v in range(MAXN + 1)]
    assert identity_table(pbig, MAXN, MAXN) == pbig
    # Group 5: one collision case. With n = 7667 and k = 2589 the first correction term of the alternating sum has out[v] == tv (p(7667) equals the prefix sum
    # p(0) + ... + p(5077) modulo P), a coincidence found by search; the table comes from the part-by-part table (method 2), which does not use the identity.
    n_c, k_c = COLL
    pref = 0
    for w in range(n_c - k_c):
        pref = (pref + pbig[w]) % P
    assert pbig[n_c] == pref  # p(7667) == p(0) + ... + p(5077) mod P, with 5077 = n - k - 2
    coll_tab = snapshots_mod(n_c, {k_c})[k_c]
    h_coll = fold_list(OFF, coll_tab)
    if check:
        print("ok groups:", ALLQ_N, "x2", (ATK_N + 1) * (ATK_K + 1), len(pairs), len(LARGE_K))
        return
    print("// Generated by tools/partition_ref.py. Do not edit.")
    print("const u64 kAll300 = 0x%016Xull;" % h_all)
    print("const u64 kDistinct300 = 0x%016Xull;" % h_dis)
    print("const u64 kAtMost200 = 0x%016Xull;" % h_atk)
    print("const u64 kGrid = 0x%016Xull;" % h_grid)
    print("const u64 kGridCount = %dull;" % len(pairs))
    print("const u64 kAll65536 = 0x%016Xull;" % h_pall)
    print("const u64 kDistinct65536 = 0x%016Xull;" % h_pdis)
    print("const u64 kLargeK[%d] = {%s};" % (len(LARGE_K), ", ".join("0x%016Xull" % x for x in large)))
    print("const u32 kIdx[%d] = {%s};" % (len(IDX), ", ".join("%du" % i for i in IDX)))
    print("const u32 kIdxAll[%d] = {%s};" % (len(IDX), ", ".join("%du" % pbig[i] for i in IDX)))
    print("const u32 kIdxDistinct[%d] = {%s};" % (len(IDX), ", ".join("%du" % qbig[i] for i in IDX)))
    print("const u64 kCollision = 0x%016Xull;" % h_coll)
    print("const u32 kP1000 = %du;" % (pe[1000] % P))


if __name__ == '__main__':
    main()
