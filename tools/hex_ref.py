#!/usr/bin/env python3
"""Independent reference for core/hex (scope AL). Exact Python integers.

Written from the rules in hex.h; it shares no code with the implementation. Encode splits every byte into its high and low nibble and
looks each one up in a digit string; decode checks every character against the digit table, then joins nibble pairs. The standard
binascii module is only a second check inside this script: every encoding and every accepted decode is compared with it. The script
prints the constants pasted into tests/hex/hex_tests.cpp: seeded encode cases (the C++ test mirrors the input generator below) and decode
strings embedded as arrays, each with a digest of the status, the length and the bytes for several destination capacities.
Usage: python3 tools/hex_ref.py
"""
import binascii

M64 = (1 << 64) - 1
KMAX_IN = 1 << 27
KMAX_ENC = 1 << 28
OK, NULL, TOOLARGE, SMALL, MALFORMED = 0, 1, 2, 3, 4
SENT = 0xDEADBEEF
LOW = '0123456789abcdef'
UP = '0123456789ABCDEF'


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
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


def gen(mode, n, seed):
    r = Lcg(seed)
    if mode == 0: return [r.rng(0, 255) for _ in range(n)]
    if mode == 1: return [0] * n
    if mode == 2: return [255] * n
    return [i & 255 for i in range(n)]


def encode(data, upper):
    digits = UP if upper else LOW
    out = ''
    for b in data:
        out += digits[b >> 4] + digits[b & 15]
    ref = binascii.hexlify(bytes(data)).decode()
    assert out == (ref.upper() if upper else ref)
    return out


def nib(c):
    if 48 <= c <= 57: return c - 48
    if 97 <= c <= 102: return c - 87
    if 65 <= c <= 70: return c - 55
    return -1


def decode(chars):
    n = len(chars)
    if n > KMAX_ENC: return TOOLARGE, None
    if n % 2: return MALFORMED, None
    for c in chars:
        if nib(c) < 0: return MALFORMED, None
    out = [(nib(chars[i]) << 4) | nib(chars[i + 1]) for i in range(0, n, 2)]
    assert bytes(out) == binascii.unhexlify(bytes(chars))
    return OK, out


def enc_digest(data, upper):
    s = encode(data, upper)
    need = len(s)
    caps = (0, 1, need - 1 if need > 0 else 0, need, need + 3)
    ws = []
    for cap in caps:
        if need > cap: ws += [SMALL, SENT]
        else: ws += [OK, need] + [ord(c) for c in s]
    return fnv_words(ws), need


def dec_digest(chars):
    st, out = decode(chars)
    caps = [0, 1, 2, 3, 4, 300]
    if st == OK: caps += [len(out) - 1 if len(out) > 0 else 0, len(out), len(out) + 3]
    ws = []
    for cap in caps:
        if st != OK: ws += [st, SENT]
        elif len(out) > cap: ws += [SMALL, SENT]
        else: ws += [OK, len(out)] + out
    return fnv_words(ws), (len(out) if st == OK else 0xFFFFFFFF)


def main():
    o = []
    cases = [(0, n, 100 + n, u) for n in range(0, 13) for u in (0, 1)] + [(1, 5, 1, 0), (2, 7, 2, 1), (3, 256, 3, 0), (3, 256, 3, 1),
             (0, 255, 4, 0), (0, 257, 5, 1), (1, 3000, 6, 0), (2, 3001, 7, 1), (3, 4097, 8, 0), (0, 10000, 9, 1), (0, 65536, 10, 0)]
    rows = []
    for mode, n, seed, up in cases:
        dg, need = enc_digest(gen(mode, n, seed), bool(up))
        rows.append('{%d, %d, %d, %dull, %du, %s}' % (mode, n, seed, dg, need, 'true' if up else 'false'))
    o.append('const EncCase kEncCases[] = {%s};' % ', '.join(rows))
    strs = []
    for n in range(0, 10):
        strs.append(encode(gen(0, n, 200 + n), False).encode())
        strs.append(encode(gen(0, n, 300 + n), True).encode())
    strs.append(b'aB')
    strs.append(b'Ab9F')
    strs.append(b'00fFfF00')
    strs.append(encode(gen(3, 300, 1), False).encode())
    strs.append(encode(gen(0, 1000, 2), True).encode())
    bad = [b'0', b'abc', b'0x00', b'0X1f', b'00 ', b' 00', b'0 0', b'\n00', b'00\n', b'0g', b'g0', b'G0', b'0G', b'0z', b'0/', b'0:', b'0@', b'0`',
           b'0F0f0', b'gg', b'0-', b'0_', b'00:', b'ab:cd', b'ab cd', b'ab\x00d', b'a\x00', b'\x00a', b'\xc8a', b'a\xc8', b'\xff\xff', b'a\x80',
           b'=0', b'0=', b'0000000g', b'g0000000', b'000g0000', b'00000000h', b'0123456789abcdefG', b'0123456789abcdefg0']
    strs += bad
    r = Lcg(4242)
    odd = [ord(c) for c in '0123456789abcdefABCDEF'] + [103, 71, 58, 47, 64, 96, 32, 10, 0, 61, 200, 255, 128]
    for k in range(60):
        base = list(encode(gen(0, r.rng(1, 12), 400 + k), k % 2 == 1).encode())
        pos = r.rng(0, len(base) - 1)
        base[pos] = odd[r.rng(0, len(odd) - 1)]
        strs.append(bytes(base))
    rows = []
    for i, s in enumerate(strs):
        arr = ', '.join(str(b - 256 if b > 127 else b) for b in s) if s else '0'
        o.append('const char kDec%d[] = {%s};' % (i, arr))
        dg, ln = dec_digest(list(s))
        rows.append('{kDec%d, %du, %du, %dull}' % (i, len(s), ln, dg))
    o.append('const DecCase kDecCases[] = {%s};' % ', '.join(rows))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
