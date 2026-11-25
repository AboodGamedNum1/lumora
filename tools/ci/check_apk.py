#!/usr/bin/env python3
"""Inspect a release APK: native ABIs must be exactly {arm64-v8a} (D25) and every
LOAD segment of every .so must be aligned to at least 16 KB (Section 10.6).
Usage: check_apk.py path/to/app-release-unsigned.apk"""
import struct, sys, zipfile, io

def load_aligns(data):
    if data[:4] != b"\x7fELF" or data[4] != 2:
        raise SystemExit("not a 64-bit ELF")
    phoff, = struct.unpack_from("<Q", data, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
    out = []
    for i in range(phnum):
        off = phoff + i * phentsize
        p_type, = struct.unpack_from("<I", data, off)
        if p_type == 1:
            out.append(struct.unpack_from("<Q", data, off + 48)[0])
    return out

def main(path):
    z = zipfile.ZipFile(path)
    abis, bad = set(), []
    for n in z.namelist():
        if n.startswith("lib/") and n.endswith(".so"):
            abis.add(n.split("/")[1])
            for a in load_aligns(z.read(n)):
                if a < 16384:
                    bad.append((n, a))
    print("native ABIs:", sorted(abis))
    ok = True
    if abis != {"arm64-v8a"}:
        print("FAIL: ABI set must be exactly {arm64-v8a}"); ok = False
    if bad:
        print("FAIL: libraries with LOAD alignment below 16384:", bad); ok = False
    if ok:
        print("OK: ABI set and 16 KB alignment")
    sys.exit(0 if ok else 1)

main(sys.argv[1])
