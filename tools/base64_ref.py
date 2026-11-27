#!/usr/bin/env python3
"""Independent reference for core/base64 (scope AK). Exact Python integers.

Written from the rules in base64.h; it shares no code with the implementation. Both directions work on strings of bits: encode turns the
bytes into a bit string, cuts it into 6-bit groups (the last one padded on the right with zeros) and maps them through the alphabet;
decode checks the characters group by group and position by position, turns them back into bits, drops the padding bits and requires
them to be zero. The standard base64 module is only a second check inside this script: every encoding is compared with it, and so is
every accepted decode (the module cannot judge the trailing bits, so it is not the oracle for rejection). The script prints the constants
pasted into tests/base64/base64_tests.cpp: seeded encode cases (the C++ test mirrors the input generator below) and decode strings
embedded as arrays, each with a digest of the status, the length and the bytes for several destination capacities.
Usage: python3 tools/base64_ref.py
"""
import base64 as stdlib_b64

M64 = (1 << 64) - 1
KMAX_IN = 3 << 22
KMAX_ENC = 4 << 22
OK, NULL, TOOLARGE, SMALL, MALFORMED, NONCANON = 0, 1, 2, 3, 4, 5
SENT = 0xDEADBEEF
ALPHA = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'


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


def encode(data):
    bits = ''.join(format(b, '08b') for b in data)
    out = ''
    for i in range(0, len(bits), 6):
        chunk = bits[i:i + 6].ljust(6, '0')
        out += ALPHA[int(chunk, 2)]
    out += '=' * (-len(out) % 4)
    assert out.encode() == stdlib_b64.b64encode(bytes(data))
    return out


def decode(chars):
    """chars: list of ints 0..255. Returns (status, bytes or None)."""
    n = len(chars)
    if n % 4: return MALFORMED, None
    if n > KMAX_ENC: return TOOLARGE, None
    groups = n // 4
    pad = 0
    for g in range(groups):
        last = g == groups - 1
        for k in range(4):
            c = chars[4 * g + k]
            if c < 128 and chr(c) in ALPHA: continue
            if c != ord('='): return MALFORMED, None
            if not last: return MALFORMED, None
            if k < 2: return MALFORMED, None
            if k == 2 and chars[4 * g + 3] != ord('='): return MALFORMED, None
        if last:
            pad = (chars[n - 1] == 61) + (chars[n - 2] == 61)
    bits = ''.join(format(ALPHA.index(chr(c)), '06b') for c in chars if c != 61)
    extra = len(bits) % 8
    if extra and int(bits[len(bits) - extra:], 2) != 0: return NONCANON, None
    bits = bits[:len(bits) - extra]
    out = [int(bits[i:i + 8], 2) for i in range(0, len(bits), 8)]
    assert len(out) == 3 * groups - pad
    assert bytes(out) == stdlib_b64.b64decode(bytes(chars), validate=True)
    return OK, out


def enc_digest(data):
    s = encode(data)
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
    cases = [(0, n, 100 + n) for n in range(0, 13)] + [(1, 5, 1), (2, 7, 2), (3, 256, 3), (0, 255, 4), (0, 257, 5), (1, 3000, 6), (2, 3001, 7),
                                                       (3, 4097, 8), (0, 10000, 9), (0, 65536, 10), (3, 70001, 11)]
    rows = []
    for mode, n, seed in cases:
        dg, need = enc_digest(gen(mode, n, seed))
        rows.append('{%d, %d, %d, %dull, %du}' % (mode, n, seed, dg, need))
    o.append('const EncCase kEncCases[] = {%s};' % ', '.join(rows))
    strs = []
    for n in range(0, 10): strs.append(encode(gen(0, n, 200 + n)).encode())
    strs.append(encode(gen(3, 300, 1)).encode())
    strs.append(encode(gen(0, 1000, 2)).encode())
    bad = [b'A', b'AB', b'ABC', b'ABCDE', b'QUJD', b'QQ==', b'QUI=', b'QR==', b'QUJ=', b'QUK=', b'QR=A', b'Q===', b'====', b'=QUJ', b'QU=J', b'QQ=A',
           b'QQ= ', b'QQ==QQ==', b'QB==', b'QC==', b'QE==', b'QI==', b'QUE=', b'QUI=', b'QUM=', b'QUJD=QQ=', b'QUJDQQ==', b'QUJDQUI=', b'QUJD\n', b'QUJ\nQQ==', b'QUJD QQ==', b'QU-D', b'QU_D', b'QU\x00D', b'QU\xc8D',
           b'QUJDQUJD', b'+/+/', b'/+/+', b'+/==', b'+w==', b'+x==', b'/+8=', b'/+9=', b'AAAA', b'AAA=', b'AA==', b'AB==', b'AAB=', b'AAC=', b'A===',
           b'AAA\xff', b'QUJDQU\xffD', b'QQ==\x00\x00\x00\x00']
    strs += bad
    r = Lcg(4242)
    odd = [ord(c) for c in ALPHA] + [61, 10, 32, 45, 95, 0, 200, 255]
    for k in range(60):
        base = list(encode(gen(0, r.rng(1, 12), 300 + k)).encode())
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
