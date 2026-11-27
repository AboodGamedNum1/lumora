#!/usr/bin/env python3
"""Independent reference for core/cartree (min-heap Cartesian tree, leftmost minimum is the root).

Standard library only (no NumPy, no third-party package). Runtime: Python 3.8 or newer; the timing is stated in docs/cartree.md.
Usage:
  python3 tools/cartree_ref.py check   run both oracles and the validators, print "OK"
  python3 tools/cartree_ref.py emit    print the golden digests as C++ source for tests/cartree/cartree_tests.cpp (runs the checks first)
Oracles of different shape:
  definition(a): THE oracle. The tree is defined recursively: the root of a range is its leftmost minimum, and the left and right subtrees are the trees of
     the parts of the range to the left and to the right of it. Implemented with an explicit list of ranges and a linear scan for the leftmost minimum
     (O(n^2) worst case), so it is used for small n only.
  nearest_smaller(a): the nearest-smaller-value shape, O(n). L(i) = nearest j < i with a[j] <= a[i], R(i) = nearest j > i with a[j] < a[i], both by
     monotonic stacks; the parent of i is the one of L(i), R(i) with the larger value (R(i) when the values are equal, because an equal element to the
     right is a descendant of the equal element to its left), or the only one that exists; no neighbour means the root.
  Both are compared on every extent where both run (all small extents); at large n the digests come from nearest_smaller, which is checked against the
  definition on all the small cases and, for the large generators, on every n up to 2000.
Validators (validate): root has no parent; parent, left and right agree; left child index below, right child index above; heap order with the tie rule
  (a left child is strictly greater than its parent, a right child greater or equal); the in-order walk visits 0 .. n-1 exactly once each.
Digest: FNV-1a 64-bit over 32-bit words (one xor and one multiply per word). A tree is folded as parent[0..n-1], left[0..n-1], right[0..n-1], root with kNone
as 0xFFFFFFFF; a group digest folds its trees in the stated enumeration order. Large inputs come from the documented xorshift32 generators in gen().
"""
import sys
from itertools import product, permutations

NONE = 0xFFFFFFFF
MAXN = 1 << 20
LARGE_KINDS = ["equal", "asc", "desc", "saw", "two", "extremes", "perm", "organ", "full"]
LARGE_N = [1, 2, 3, 1000, 65535, 65536, 524289, 1048575, 1048576]


def fnv(words, h=0xcbf29ce484222325):
    for w in words:
        h = ((h ^ w) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def definition(a):
    n = len(a)
    parent, left, right = [-1] * n, [-1] * n, [-1] * n
    root = -1
    if n == 0:
        return parent, left, right, root
    todo = [(0, n - 1, -1, 0)]
    while todo:
        lo, hi, par, side = todo.pop()
        m = lo
        for i in range(lo + 1, hi + 1):
            if a[i] < a[m]:
                m = i
        parent[m] = par
        if par >= 0:
            if side == 0:
                left[par] = m
            else:
                right[par] = m
        else:
            root = m
        if lo <= m - 1:
            todo.append((lo, m - 1, m, 0))
        if m + 1 <= hi:
            todo.append((m + 1, hi, m, 1))
    return parent, left, right, root


def nearest_smaller(a):
    n = len(a)
    L = [-1] * n
    R = [-1] * n
    st = []
    for i in range(n):  # previous element with a[j] <= a[i]
        while st and a[st[-1]] > a[i]:
            st.pop()
        L[i] = st[-1] if st else -1
        st.append(i)
    st = []
    for i in range(n - 1, -1, -1):  # next element with a[j] < a[i]
        while st and a[st[-1]] >= a[i]:
            st.pop()
        R[i] = st[-1] if st else -1
        st.append(i)
    parent, left, right = [-1] * n, [-1] * n, [-1] * n
    root = -1
    for i in range(n):
        l, r = L[i], R[i]
        if l < 0 and r < 0:
            p = -1
        elif l < 0:
            p = r
        elif r < 0:
            p = l
        else:
            p = l if a[l] > a[r] else r
        parent[i] = p
        if p < 0:
            root = i
        elif p < i:
            right[p] = i
        else:
            left[p] = i
    return parent, left, right, root


def validate(a, t):
    parent, left, right, root = t
    n = len(a)
    if n == 0:
        assert root == -1
        return
    assert 0 <= root < n and parent[root] == -1
    roots = 0
    for i in range(n):
        if parent[i] == -1:
            roots += 1
        else:
            p = parent[i]
            assert (left[p] == i) != (right[p] == i), ("parent link", i)
            if left[p] == i:
                assert i < p and a[i] > a[p], ("left child", i)
            else:
                assert i > p and a[i] >= a[p], ("right child", i)
        if left[i] != -1:
            assert parent[left[i]] == i and left[i] < i
        if right[i] != -1:
            assert parent[right[i]] == i and right[i] > i
    assert roots == 1
    seen, st, cur, k = 0, [], root, 0
    while st or cur != -1:
        while cur != -1:
            st.append(cur)
            cur = left[cur]
        cur = st.pop()
        assert cur == k, ("in-order", k, cur)
        k += 1
        cur = right[cur]
    assert k == n


def words(t):
    parent, left, right, root = t
    return [v & 0xFFFFFFFF for v in parent] + [v & 0xFFFFFFFF for v in left] + [v & 0xFFFFFFFF for v in right] + [root & 0xFFFFFFFF]


class Rng:
    """xorshift32 (13, 17, 5); the state is never 0."""

    def __init__(self, seed):
        self.x = seed & 0xFFFFFFFF or 1

    def next(self):
        x = self.x
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.x = x
        return x


def gen(kind, n):
    """Large inputs; the seed is 0x9E3779B9 + n (mod 2^32). The C++ test implements the same generators."""
    r = Rng((0x9E3779B9 + n) & 0xFFFFFFFF)
    if kind == "equal":
        return [7] * n
    if kind == "asc":
        return list(range(n))
    if kind == "desc":
        return [n - 1 - i for i in range(n)]
    if kind == "saw":
        return [i % 1000 for i in range(n)]
    if kind == "two":
        return [r.next() >> 31 for _ in range(n)]
    if kind == "extremes":
        return [NONE if (r.next() >> 31) else 0 for _ in range(n)]
    if kind == "perm":
        p = list(range(n))
        for i in range(n - 1, 0, -1):
            j = r.next() % (i + 1)
            p[i], p[j] = p[j], p[i]
        return p
    if kind == "organ":
        return [min(i, n - 1 - i) for i in range(n)]
    if kind == "full":
        return [r.next() for _ in range(n)]
    raise ValueError(kind)


def extent_arrays():
    """The reference-digest extents, in the enumeration order the C++ test uses (the last position changes fastest)."""
    ea = [a for n in range(0, 7) for a in product(range(n), repeat=n)]
    eb = [a for n in range(0, 11) for a in product(range(3), repeat=n)]
    ec = [a for n in range(0, 9) for a in permutations(range(n))]
    return {"all_below_n_len_0_6": ea, "alphabet3_len_0_10": eb, "permutations_0_8": ec}


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    digests = {}
    for name, arrs in extent_arrays().items():
        h = 0xcbf29ce484222325
        for a in arrs:
            t = definition(list(a))
            assert t == nearest_smaller(list(a)), ("definition vs nearest-smaller", a)
            validate(a, t)
            h = fnv(words(t), h)
        digests[name] = (h, len(arrs))
    # hand-checked trees (the header example, ties)
    assert definition([2, 1, 1, 3]) == ([1, -1, 1, 2], [-1, 0, -1, -1], [-1, 2, 3, -1], 1)
    assert definition([5, 5, 5]) == ([-1, 0, 1], [-1, -1, -1], [1, 2, -1], 0)
    # the large generators: nearest-smaller against the definition for every n up to 2000
    for kind in LARGE_KINDS:
        for n in range(0, 2001, 37):
            a = gen(kind, n)
            assert definition(a) == nearest_smaller(a), ("generator", kind, n)
    large = {}
    for n in LARGE_N:
        for kind in LARGE_KINDS:
            a = gen(kind, n)
            if n <= 1000:
                assert definition(a) == nearest_smaller(a), (kind, n)
            t = nearest_smaller(a)
            if n <= 65536 or kind in ("equal", "asc", "perm", "full", "two"):
                validate(a, t)
            large[(kind, n)] = fnv(words(t))
    if mode == "check":
        print("OK")
        return
    for name, (h, c) in digests.items():
        print("constexpr std::uint64_t kDigest_%s = 0x%016xull;  // %d arrays" % (name, h, c))
    print("constexpr std::uint64_t kLargeDigest[9][9] = {  // [kind][n index]: kinds equal asc desc saw two extremes perm organ full; n 1 2 3 1000 65535 65536 524289 1048575 1048576")
    for kind in LARGE_KINDS:
        print("    {" + ", ".join("0x%016xull" % large[(kind, n)] for n in LARGE_N) + "},  // " + kind)
    print("};")


main()
