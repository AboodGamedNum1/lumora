#!/usr/bin/env python3
"""Minimal secret scan over tracked files: fails on well-known credential shapes.
This complements GitHub secret scanning and push protection, it does not replace them."""
import re, subprocess, sys
PATTERNS = {
    "github token": r"\b(ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{30,}\b|\bgithub_pat_[A-Za-z0-9_]{40,}\b",
    "private key": r"-----BEGIN (RSA |EC |OPENSSH |DSA |PGP )?PRIVATE KEY-----",
    "aws access key": r"\bAKIA[0-9A-Z]{16}\b",
    "slack token": r"\bxox[abprs]-[A-Za-z0-9-]{10,}\b",
}
files = subprocess.check_output(["git", "ls-files", "-z"]).decode().split("\0")
bad = []
for f in files:
    if not f or f.endswith((".jar", ".png")):
        continue
    try:
        text = open(f, encoding="utf-8", errors="ignore").read()
    except OSError:
        continue
    for name, pat in PATTERNS.items():
        if re.search(pat, text):
            bad.append((f, name))
for f, n in bad:
    print(f"FAIL: {f}: looks like a {n}")
print("secret scan:", "FAILED" if bad else "ok")
sys.exit(1 if bad else 0)
