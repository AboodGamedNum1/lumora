"""Fuzz corpus tool: report, dedup and minimize for corpus folders such as tools/fuzz/corpus/<target>/.

Python 3 standard library only (3.8 or newer).

Command line:
  corpus_tool.py report   --corpus DIR [--json]
  corpus_tool.py dedup    --corpus DIR [--apply] [--json]
  corpus_tool.py minimize --corpus DIR [--max-files N] [--max-bytes N] [--apply] [--json]

Exit codes: 0 success, 1 corpus problem (missing folder, unreadable file, symlink, a file above the size cap, a
failed delete), 2 usage error. Without --apply nothing on disk changes. The rules are written in docs/fuzz-tools.md.

The corpus folder is untrusted input. Only its direct entries are read. The tool never follows a symlink, never opens
anything that is not a regular file, and with --apply it only deletes regular files directly inside the folder that
the report named, after checking again that their content is unchanged.
"""
import argparse
import hashlib
import json
import os
import re
import stat
import sys

MAX_FILE_BYTES = 1024 * 1024  # a file above 1 MiB is a corpus problem
SCHEMA_VERSION = 1
_NUMBER = re.compile(r"^[0-9]{1,18}$")


# ------------------------------------------------------------------ names

def name_key(name):
    """Sort key: the raw bytes of the file name. Identical on every platform and locale."""
    return os.fsencode(name)


def display_name(name):
    """A printable, injective, single-line form of a file name for reports.

    Backslash becomes two backslashes. Bytes that are not valid UTF-8 and every character that is not printable
    (control characters, line separators, format characters) become backslash escapes, so a hostile name can never
    add lines or hide text in a report.
    """
    out = []
    for ch in name:
        code = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif 0xDC80 <= code <= 0xDCFF:  # a byte that was not valid UTF-8 (surrogateescape)
            out.append("\\x%02x" % (code - 0xDC00))
        elif code < 0x80 and not ch.isprintable():
            out.append("\\x%02x" % code)
        elif not ch.isprintable():
            out.append("\\u%04x" % code if code <= 0xFFFF else "\\U%08x" % code)
        else:
            out.append(ch)
    return "".join(out)


# ------------------------------------------------------------------ scanning

class Entry(object):
    __slots__ = ("name", "size", "sha256")

    def __init__(self, name, size, sha256):
        self.name = name
        self.size = size
        self.sha256 = sha256


def _hash_file(path):
    """Returns (size, sha256 hex) of a regular file read without following links, or raises OSError / ValueError."""
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0) | getattr(os, "O_NONBLOCK", 0)
    fd = os.open(path, flags)
    try:
        st = os.fstat(fd)
        if not stat.S_ISREG(st.st_mode):
            raise ValueError("not a regular file")
        if st.st_size > MAX_FILE_BYTES:
            raise ValueError("file above 1 MiB")
        digest = hashlib.sha256()
        total = 0
        while True:
            chunk = os.read(fd, 65536)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_FILE_BYTES:
                raise ValueError("file above 1 MiB")
            digest.update(chunk)
        return total, digest.hexdigest()
    finally:
        os.close(fd)


def scan_corpus(corpus):
    """Reads the direct entries of a corpus folder.

    Returns (entries, problems). entries is a list of Entry sorted by name bytes. problems is a list of
    (name or None, reason) sorted by name bytes, with fixed reason texts. Any problem means the corpus is not usable.
    """
    entries, problems = [], []
    try:
        st = os.lstat(corpus)
    except OSError:
        return [], [(None, "corpus folder is missing or unreadable")]
    if stat.S_ISLNK(st.st_mode):
        return [], [(None, "corpus folder is a symlink")]
    if not stat.S_ISDIR(st.st_mode):
        return [], [(None, "corpus path is not a folder")]
    try:
        names = os.listdir(corpus)
    except OSError:
        return [], [(None, "corpus folder is unreadable")]
    for name in sorted(names, key=name_key):
        path = os.path.join(corpus, name)
        try:
            mode = os.lstat(path).st_mode
        except OSError:
            problems.append((name, "unreadable entry"))
            continue
        if stat.S_ISLNK(mode):
            problems.append((name, "symlink"))
        elif not stat.S_ISREG(mode):
            problems.append((name, "not a regular file"))
        else:
            try:
                size, digest = _hash_file(path)
            except ValueError as exc:
                problems.append((name, str(exc)))
            except OSError:
                problems.append((name, "unreadable file"))
            else:
                entries.append(Entry(name, size, digest))
    return entries, problems


# ------------------------------------------------------------------ planning

def group_duplicates(entries):
    """Groups of identical content, each as a list sorted by name; only groups with 2 or more files, ordered by the
    name of the file each group keeps."""
    by_hash = {}
    for e in entries:
        by_hash.setdefault(e.sha256, []).append(e)
    groups = []
    for members in by_hash.values():
        if len(members) > 1:
            members.sort(key=lambda e: name_key(e.name))
            groups.append(members)
    groups.sort(key=lambda g: name_key(g[0].name))
    return groups


def plan_dedup(entries):
    """Returns (kept, removed). removed is a list of (Entry, reason, duplicate_of) sorted by name; kept is a list of
    Entry sorted by name. The file with the smallest name in each group of identical content is kept."""
    removed = []
    gone = set()
    for group in group_duplicates(entries):
        for e in group[1:]:
            removed.append((e, "duplicate", group[0].name))
            gone.add(e.name)
    kept = [e for e in entries if e.name not in gone]
    removed.sort(key=lambda r: name_key(r[0].name))
    return kept, removed


def plan_minimize(entries, max_files, max_bytes):
    """Dedup first, then keep the smallest unique files (size, then name) while both caps hold.

    The walk stops at the first file that would break a cap. Files are in ascending size order, so every later file
    would break the cap too. Returns (kept, removed, stopped_by) with stopped_by None, "max-files" or "max-bytes".
    """
    unique, removed = plan_dedup(entries)
    ordered = sorted(unique, key=lambda e: (e.size, name_key(e.name)))
    kept, total, stopped_by = [], 0, None
    for index, e in enumerate(ordered):
        if max_files is not None and len(kept) + 1 > max_files:
            stopped_by = "max-files"
        elif max_bytes is not None and total + e.size > max_bytes:
            stopped_by = "max-bytes"
        if stopped_by is not None:
            removed.extend((r, "over-cap", None) for r in ordered[index:])
            break
        kept.append(e)
        total += e.size
    kept.sort(key=lambda e: name_key(e.name))
    removed.sort(key=lambda r: name_key(r[0].name))
    return kept, removed, stopped_by


# ------------------------------------------------------------------ applying

def apply_removals(corpus, removed):
    """Deletes the reported files. Each file is checked again first: it must still be a regular file with the size
    and SHA-256 that were reported. Returns (deleted_names, failures) with failures as (name, reason)."""
    deleted, failures = [], []
    for entry, _reason, _of in removed:
        name = entry.name
        if name in (".", "..") or os.sep in name or (os.altsep and os.altsep in name) or "\x00" in name:
            failures.append((name, "unsafe name"))
            continue
        path = os.path.join(corpus, name)
        try:
            size, digest = _hash_file(path)
        except (OSError, ValueError):
            failures.append((name, "changed or unreadable since the report"))
            continue
        if size != entry.size or digest != entry.sha256:
            failures.append((name, "changed since the report"))
            continue
        try:
            os.unlink(path)
        except OSError:
            failures.append((name, "delete failed"))
            continue
        deleted.append(name)
    return deleted, failures


# ------------------------------------------------------------------ reports

def _summary(entries):
    sizes = [e.size for e in entries]
    unique = len(set(e.sha256 for e in entries))
    return {
        "files": len(entries),
        "bytes": sum(sizes),
        "smallest": min(sizes) if sizes else None,
        "largest": max(sizes) if sizes else None,
        "unique": unique,
    }


def _dump_json(obj):
    return json.dumps(obj, sort_keys=True, indent=2, ensure_ascii=True) + "\n"


def _problem_json(command, problems):
    return {
        "schema": SCHEMA_VERSION,
        "command": command,
        "ok": False,
        "problems": [{"name": None if n is None else display_name(n), "reason": r} for n, r in problems],
    }


def _kb(n):
    return "%d byte%s" % (n, "" if n == 1 else "s")


def build_report(command, entries, kept, removed, stopped_by, caps, applied, deleted, failures):
    """The result as a plain dict. Names are in display form. Used for JSON and as the source of the text form."""
    summary = _summary(entries)
    doc = {
        "schema": SCHEMA_VERSION,
        "command": command,
        "ok": not failures,
        "applied": applied,
        "summary": summary,
        "duplicate_groups": [
            {
                "keep": display_name(g[0].name),
                "remove": [display_name(e.name) for e in g[1:]],
                "sha256": g[0].sha256,
                "size": g[0].size,
            }
            for g in group_duplicates(entries)
        ],
    }
    if command == "report":
        doc["files"] = [{"name": display_name(e.name), "size": e.size, "sha256": e.sha256} for e in entries]
        return doc
    doc["kept"] = [{"name": display_name(e.name), "size": e.size} for e in kept]
    doc["removed"] = [
        {
            "name": display_name(e.name),
            "size": e.size,
            "reason": reason,
            "duplicate_of": None if of is None else display_name(of),
        }
        for e, reason, of in removed
    ]
    doc["totals"] = {
        "kept_files": len(kept),
        "kept_bytes": sum(e.size for e in kept),
        "removed_files": len(removed),
        "removed_bytes": sum(e.size for e, _r, _o in removed),
    }
    if command == "minimize":
        doc["caps"] = {"max_files": caps[0], "max_bytes": caps[1], "stopped_by": stopped_by}
    if applied:
        doc["deleted"] = [display_name(n) for n in deleted]
        doc["failed"] = [{"name": display_name(n), "reason": r} for n, r in failures]
    return doc


def render_text(doc):
    lines = []
    command = doc["command"]
    s = doc["summary"]
    if command == "report":
        lines.append("Fuzz corpus report")
    else:
        lines.append("Fuzz corpus %s (%s)" % (command, "applied" if doc["applied"] else "dry run, nothing changed"))
    lines.append("files: %d" % s["files"])
    lines.append("total bytes: %d" % s["bytes"])
    lines.append("smallest file: %s" % ("none" if s["smallest"] is None else _kb(s["smallest"])))
    lines.append("largest file: %s" % ("none" if s["largest"] is None else _kb(s["largest"])))
    lines.append("unique contents: %d" % s["unique"])
    groups = doc["duplicate_groups"]
    removable = sum(len(g["remove"]) for g in groups)
    removable_bytes = sum(len(g["remove"]) * g["size"] for g in groups)
    lines.append("duplicate groups: %d" % len(groups))
    lines.append("duplicate files that dedup removes: %d (%s)" % (removable, _kb(removable_bytes)))
    if command == "minimize":
        caps = doc["caps"]
        lines.append("max files: %s" % ("none" if caps["max_files"] is None else caps["max_files"]))
        lines.append("max bytes: %s" % ("none" if caps["max_bytes"] is None else caps["max_bytes"]))
        lines.append("stopped by: %s" % (caps["stopped_by"] or "no cap reached"))
    if command == "report":
        if groups:
            lines.append("")
            lines.append("duplicate groups:")
            for g in groups:
                lines.append("  keep: %s (%s, sha256 %s)" % (g["keep"], _kb(g["size"]), g["sha256"][:12]))
                for name in g["remove"]:
                    lines.append("    duplicate: %s" % name)
    else:
        t = doc["totals"]
        lines.append("kept: %d files, %s" % (t["kept_files"], _kb(t["kept_bytes"])))
        lines.append("removed: %d files, %s" % (t["removed_files"], _kb(t["removed_bytes"])))
        if doc["removed"]:
            lines.append("")
            lines.append("remove:")
            for r in doc["removed"]:
                why = "duplicate of %s" % r["duplicate_of"] if r["reason"] == "duplicate" else "over the caps"
                lines.append("  %s (%s, %s)" % (r["name"], _kb(r["size"]), why))
        if doc["applied"]:
            lines.append("")
            lines.append("deleted: %d files" % len(doc["deleted"]))
            for f in doc["failed"]:
                lines.append("  failed: %s (%s)" % (f["name"], f["reason"]))
    return "\n".join(lines) + "\n"


# ------------------------------------------------------------------ command line

class _Parser(argparse.ArgumentParser):
    def error(self, message):  # usage errors are exit code 2 and print nothing but the message and usage
        self.print_usage(sys.stderr)
        sys.stderr.write("corpus_tool: error: %s\n" % message)
        raise SystemExit(2)


def _number(text):
    if not _NUMBER.match(text):
        raise argparse.ArgumentTypeError("must be a whole number of 0 or more")
    return int(text)


def _build_parser():
    parser = _Parser(prog="corpus_tool.py", description="Report on, dedup and minimize a fuzz corpus folder.")
    sub = parser.add_subparsers(dest="command")
    for name, help_text in (("report", "show sizes and duplicates"), ("dedup", "remove files with identical content"),
                            ("minimize", "dedup, then keep the smallest files within the caps")):
        p = sub.add_parser(name, help=help_text)
        p.add_argument("--corpus", required=True, metavar="DIR")
        p.add_argument("--json", action="store_true", help="print JSON instead of text")
        if name != "report":
            p.add_argument("--apply", action="store_true", help="delete the files the report names (default: dry run)")
        if name == "minimize":
            p.add_argument("--max-files", type=_number, default=None, metavar="N")
            p.add_argument("--max-bytes", type=_number, default=None, metavar="N")
    return parser


def main(argv):
    parser = _build_parser()
    try:
        args = parser.parse_args(list(argv))
    except SystemExit as exc:
        return exc.code if isinstance(exc.code, int) else 2
    if not args.command:
        parser.print_usage(sys.stderr)
        sys.stderr.write("corpus_tool: error: a command is required (report, dedup or minimize)\n")
        return 2

    entries, problems = scan_corpus(args.corpus)
    if problems:
        if args.json:
            sys.stdout.write(_dump_json(_problem_json(args.command, problems)))
        else:
            for name, reason in problems:
                sys.stderr.write("corpus_tool: %s: %s\n" % ("corpus" if name is None else display_name(name), reason))
        return 1

    kept, removed, stopped_by, caps = [], [], None, (None, None)
    if args.command == "dedup":
        kept, removed = plan_dedup(entries)
    elif args.command == "minimize":
        caps = (args.max_files, args.max_bytes)
        kept, removed, stopped_by = plan_minimize(entries, args.max_files, args.max_bytes)

    applied = bool(getattr(args, "apply", False))
    deleted, failures = [], []
    if applied:
        deleted, failures = apply_removals(args.corpus, removed)
    doc = build_report(args.command, entries, kept, removed, stopped_by, caps, applied, deleted, failures)
    sys.stdout.write(_dump_json(doc) if args.json else render_text(doc))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
