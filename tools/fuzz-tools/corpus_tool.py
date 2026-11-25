"""Fuzz corpus tool (interface fixed by the owner; the contributor implements it).

Python 3 standard library only. Works on corpus folders such as tools/fuzz/corpus/<target>/.

Command line:
  corpus_tool.py report   --corpus DIR [--json]
  corpus_tool.py dedup    --corpus DIR [--apply] [--json]
  corpus_tool.py minimize --corpus DIR [--max-files N] [--max-bytes N] [--apply] [--json]

Exit codes: 0 success, 1 corpus problem (missing folder, unreadable file, symlink, a file above the size cap),
2 usage error. Without --apply nothing on disk changes. The rules are written in docs/fuzz-tools.md.
"""
import sys


def main(argv):
    sys.stderr.write("corpus_tool: not implemented\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
