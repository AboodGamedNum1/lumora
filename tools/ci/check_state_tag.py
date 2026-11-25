#!/usr/bin/env python3
"""Fail when last_completed_milestone in docs/PROJECT_STATE.md differs from the newest m*-done tag.
Needs full tag history (actions/checkout with fetch-depth 0)."""
import re, subprocess, sys
state = open("docs/PROJECT_STATE.md", encoding="utf-8").read()
m = re.search(r"^last_completed_milestone:\s*(\S+)", state, re.M)
declared = m.group(1) if m else "none"
tags = subprocess.check_output(["git", "tag", "--list", "m*-done", "--sort=-creatordate"]).decode().split()
newest = tags[0][:-5].upper() if tags else "none"
if declared.upper() != newest:
    print(f"FAIL: PROJECT_STATE says {declared}, newest m*-done tag says {newest}"); sys.exit(1)
print("state tag check ok:", declared)
