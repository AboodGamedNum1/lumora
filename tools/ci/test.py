#!/usr/bin/env python3
"""Single test entry point shared by local runs and CI.

Usage: python3 tools/ci/test.py [--sanitize] [--build-dir DIR] [--config Release]
Steps: documentation check, CMake configure, build, ctest.
"""
import argparse, os, subprocess, sys, shutil

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    r = subprocess.run(cmd, cwd=ROOT, **kw)
    if r.returncode != 0:
        sys.exit(r.returncode)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sanitize", action="store_true")
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--config", default="Release")
    ap.add_argument("--skip-docs", action="store_true")
    a = ap.parse_args()
    if not a.skip_docs:
        run([sys.executable, "tools/check_docs.py"])
    cfg = ["cmake", "-S", ".", "-B", a.build_dir, "-DCMAKE_BUILD_TYPE=" + a.config]
    if shutil.which("ninja") and os.name != "nt":
        cfg += ["-G", "Ninja"]
    if a.sanitize:
        cfg += ["-DLUMORA_SANITIZE=ON"]
    run(cfg)
    run(["cmake", "--build", a.build_dir, "--config", a.config, "--parallel"])
    run(["ctest", "--test-dir", a.build_dir, "-C", a.config, "--output-on-failure"])
    print("ALL TESTS PASSED")

if __name__ == "__main__":
    main()
