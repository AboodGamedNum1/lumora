"""Tests for corpus_tool.py. Every test builds its own temporary corpus folder; no real corpus is touched.
Run: python3 -m unittest discover -s tools/fuzz-tools/tests
"""
import contextlib
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOL_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, TOOL_DIR)

import corpus_tool  # noqa: E402

TOOL = os.path.join(TOOL_DIR, "corpus_tool.py")


def run(*argv):
    """Runs main() in-process. Returns (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = corpus_tool.main(list(argv))
    return code, out.getvalue(), err.getvalue()


class Fixture(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="corpus_tool_test_")
        self.addCleanup(shutil.rmtree, self.root, True)
        self.corpus = os.path.join(self.root, "corpus")
        os.mkdir(self.corpus)
        self.outside = os.path.join(self.root, "outside.txt")
        with open(self.outside, "wb") as f:
            f.write(b"outside")

    def outside_bytes(self):
        with open(self.outside, "rb") as f:
            return f.read()

    def put(self, name, data=b"", folder=None):
        path = os.path.join(folder or self.corpus, name)
        with open(path, "wb") as f:
            f.write(data)
        return path

    def names(self, folder=None):
        return sorted(os.listdir(folder or self.corpus), key=corpus_tool.name_key)

    def snapshot(self, folder=None):
        folder = folder or self.corpus
        snap = {}
        for name in os.listdir(folder):
            st = os.lstat(os.path.join(folder, name))
            with open(os.path.join(folder, name), "rb") as f:
                snap[name] = (f.read(), st.st_mtime_ns, st.st_mode)
        return snap

    def js(self, *argv):
        code, out, err = run(*argv, "--json")
        return code, json.loads(out), err


class ReportTests(Fixture):
    def test_report_counts(self):
        self.put("a", b"x")
        self.put("b", b"x")
        self.put("c", b"yy")
        self.put("empty", b"")
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertEqual(doc["summary"], {"files": 4, "bytes": 4, "smallest": 0, "largest": 2, "unique": 3})
        self.assertEqual(len(doc["duplicate_groups"]), 1)
        self.assertEqual(doc["duplicate_groups"][0]["keep"], "a")
        self.assertEqual(doc["duplicate_groups"][0]["remove"], ["b"])
        self.assertEqual(doc["duplicate_groups"][0]["sha256"], hashlib.sha256(b"x").hexdigest())
        self.assertEqual([f["name"] for f in doc["files"]], ["a", "b", "c", "empty"])

    def test_json_schema_is_stable(self):
        self.put("a", b"x")
        self.put("b", b"x")
        _, rep, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(sorted(rep), ["applied", "command", "duplicate_groups", "files", "ok", "schema", "summary"])
        self.assertEqual(rep["schema"], 1)
        self.assertEqual(sorted(rep["summary"]), ["bytes", "files", "largest", "smallest", "unique"])
        self.assertEqual(sorted(rep["files"][0]), ["name", "sha256", "size"])
        _, ded, _ = self.js("dedup", "--corpus", self.corpus)
        self.assertEqual(sorted(ded), ["applied", "command", "duplicate_groups", "kept", "ok", "removed", "schema", "summary", "totals"])
        self.assertEqual(sorted(ded["removed"][0]), ["duplicate_of", "name", "reason", "size"])
        self.assertEqual(sorted(ded["kept"][0]), ["name", "size"])
        self.assertEqual(sorted(ded["totals"]), ["kept_bytes", "kept_files", "removed_bytes", "removed_files"])
        self.assertEqual(sorted(ded["duplicate_groups"][0]), ["keep", "remove", "sha256", "size"])
        _, mini, _ = self.js("minimize", "--corpus", self.corpus, "--max-files", "1")
        self.assertEqual(sorted(mini["caps"]), ["max_bytes", "max_files", "stopped_by"])
        self.assertIn("caps", mini)
        _, app, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        self.assertEqual(sorted(app), ["applied", "command", "deleted", "duplicate_groups", "failed", "kept", "ok", "removed", "schema", "summary", "totals"])

    def test_json_is_sorted_and_ends_with_a_newline(self):
        self.put("a", b"x")
        _, out, _ = run("report", "--corpus", self.corpus, "--json")
        self.assertTrue(out.endswith("\n"))
        self.assertEqual(out, json.dumps(json.loads(out), sort_keys=True, indent=2) + "\n")

    def test_empty_corpus(self):
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertEqual(doc["summary"], {"files": 0, "bytes": 0, "smallest": None, "largest": None, "unique": 0})
        code, out, _ = run("report", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertIn("smallest file: none", out)
        for command in ("dedup", "minimize"):
            self.assertEqual(run(command, "--corpus", self.corpus, "--apply")[0], 0)

    def test_text_report_is_stable(self):
        self.put("a", b"x")
        self.put("b", b"x")
        _, one, _ = run("report", "--corpus", self.corpus)
        _, two, _ = run("report", "--corpus", self.corpus)
        self.assertEqual(one, two)
        self.assertEqual(one.splitlines()[0], "Fuzz corpus report")
        self.assertIn("keep: a (1 byte, sha256 ", one)
        self.assertIn("    duplicate: b", one)
        self.assertNotIn(self.corpus, one)  # no host paths in the output

    def test_output_does_not_depend_on_creation_order(self):
        other = os.path.join(self.root, "other")
        os.mkdir(other)
        names = ["m", "a", "z", "B", "10", "9", "a b"]
        for n in names:
            self.put(n, n.encode()[:1] * 3)
        for n in reversed(names):
            self.put(n, n.encode()[:1] * 3, folder=other)
        for command in (["report"], ["dedup"], ["minimize", "--max-files", "3"]):
            a = run(*(command + ["--corpus", self.corpus, "--json"]))
            b = run(*(command + ["--corpus", other, "--json"]))
            self.assertEqual(a, b)


class DedupTests(Fixture):
    def test_keeps_the_smallest_filename_by_bytes(self):
        for n in ("b", "a", "C", "10", "9", "a1"):
            self.put(n, b"same")
        self.put("other", b"different")
        code, doc, _ = self.js("dedup", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertEqual(doc["duplicate_groups"][0]["keep"], "10")  # "1" sorts before "9", "C" and "a"
        removed = [r["name"] for r in doc["removed"]]
        self.assertEqual(removed, ["9", "C", "a", "a1", "b"])
        self.assertTrue(all(r["reason"] == "duplicate" and r["duplicate_of"] == "10" for r in doc["removed"]))
        self.assertEqual([k["name"] for k in doc["kept"]], ["10", "other"])

    def test_same_size_different_content_is_not_a_duplicate(self):
        self.put("a", b"abc")
        self.put("b", b"abd")
        _, doc, _ = self.js("dedup", "--corpus", self.corpus)
        self.assertEqual(doc["removed"], [])
        self.assertEqual(doc["duplicate_groups"], [])

    def test_dry_run_changes_nothing(self):
        for n in ("a", "b", "c"):
            self.put(n, b"same")
        before = self.snapshot()
        for command in (["dedup"], ["minimize", "--max-files", "1"], ["report"]):
            code, _, _ = run(*(command + ["--corpus", self.corpus]))
            self.assertEqual(code, 0)
            self.assertEqual(self.snapshot(), before)
            self.assertEqual(self.names(), ["a", "b", "c"])

    def test_apply_deletes_only_the_reported_files(self):
        for n in ("a", "b", "c"):
            self.put(n, b"same")
        self.put("keep", b"unique")
        code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        self.assertEqual(code, 0)
        self.assertTrue(doc["applied"])
        self.assertEqual(doc["deleted"], ["b", "c"])
        self.assertEqual(doc["failed"], [])
        self.assertEqual(self.names(), ["a", "keep"])
        code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")  # nothing left to do
        self.assertEqual((code, doc["deleted"], doc["removed"]), (0, [], []))
        with open(self.outside, "rb") as f:
            self.assertEqual(f.read(), b"outside")

    def test_empty_files_are_files_and_duplicates_of_each_other(self):
        self.put("e2", b"")
        self.put("e1", b"")
        self.put("e3", b"")
        code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        self.assertEqual(code, 0)
        self.assertEqual(doc["deleted"], ["e2", "e3"])
        self.assertEqual(self.names(), ["e1"])

    def test_apply_skips_a_file_that_changed_after_the_plan(self):
        self.put("a", b"same")
        self.put("b", b"same")
        entries, problems = corpus_tool.scan_corpus(self.corpus)
        self.assertEqual(problems, [])
        kept, removed = corpus_tool.plan_dedup(entries)
        self.put("b", b"changed since the report")
        deleted, failures = corpus_tool.apply_removals(self.corpus, removed)
        self.assertEqual(deleted, [])
        self.assertEqual([f[0] for f in failures], ["b"])
        self.assertEqual(self.names(), ["a", "b"])
        # A file that vanished is a failure too, and nothing else is touched.
        os.unlink(os.path.join(self.corpus, "b"))
        deleted, failures = corpus_tool.apply_removals(self.corpus, removed)
        self.assertEqual((deleted, [f[0] for f in failures]), ([], ["b"]))
        self.assertEqual(self.names(), ["a"])

    def test_apply_refuses_a_file_replaced_by_a_symlink(self):
        self.put("a", b"same")
        self.put("b", b"same")
        entries, _ = corpus_tool.scan_corpus(self.corpus)
        _, removed = corpus_tool.plan_dedup(entries)
        os.unlink(os.path.join(self.corpus, "b"))
        try:
            os.symlink(self.outside, os.path.join(self.corpus, "b"))
        except (OSError, NotImplementedError, AttributeError):
            self.skipTest("symlinks not available")
        deleted, failures = corpus_tool.apply_removals(self.corpus, removed)
        self.assertEqual(deleted, [])
        self.assertEqual(len(failures), 1)
        self.assertTrue(os.path.exists(self.outside))

    def test_apply_failure_exits_1(self):
        self.put("a", b"same")
        self.put("b", b"same")
        real = corpus_tool.os.unlink

        def boom(path):
            raise OSError("no")

        corpus_tool.os.unlink = boom
        try:
            code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        finally:
            corpus_tool.os.unlink = real
        self.assertEqual(code, 1)
        self.assertFalse(doc["ok"])
        self.assertEqual(doc["failed"], [{"name": "b", "reason": "delete failed"}])
        self.assertEqual(self.names(), ["a", "b"])


class MinimizeTests(Fixture):
    def setUp(self):
        Fixture.setUp(self)
        # sizes: e=0, a=1, b=2, c=3, d=4 (total 10), plus a duplicate of "a" named "a_copy"
        self.put("e", b"")
        self.put("a", b"1")
        self.put("b", b"22")
        self.put("c", b"333")
        self.put("d", b"4444")
        self.put("a_copy", b"1")

    def kept(self, *args):
        code, doc, _ = self.js("minimize", "--corpus", self.corpus, *args)
        self.assertEqual(code, 0)
        return [k["name"] for k in doc["kept"]], doc

    def test_no_caps_is_dedup(self):
        kept, doc = self.kept()
        self.assertEqual(kept, ["a", "b", "c", "d", "e"])
        self.assertEqual([r["name"] for r in doc["removed"]], ["a_copy"])
        self.assertIsNone(doc["caps"]["stopped_by"])

    def test_max_files_boundaries(self):
        self.assertEqual(self.kept("--max-files", "5")[0], ["a", "b", "c", "d", "e"])  # exactly the unique count
        kept, doc = self.kept("--max-files", "4")
        self.assertEqual(kept, ["a", "b", "c", "e"])  # smallest first: e, a, b, c
        self.assertEqual(doc["caps"]["stopped_by"], "max-files")
        self.assertEqual(self.kept("--max-files", "1")[0], ["e"])
        self.assertEqual(self.kept("--max-files", "0")[0], [])
        self.assertEqual(self.kept("--max-files", "100")[0], ["a", "b", "c", "d", "e"])
        removed = {r["name"]: r["reason"] for r in self.kept("--max-files", "4")[1]["removed"]}
        self.assertEqual(removed, {"a_copy": "duplicate", "d": "over-cap"})

    def test_max_bytes_boundaries(self):
        self.assertEqual(self.kept("--max-bytes", "10")[0], ["a", "b", "c", "d", "e"])  # exactly the total
        kept, doc = self.kept("--max-bytes", "9")
        self.assertEqual(kept, ["a", "b", "c", "e"])
        self.assertEqual(doc["caps"]["stopped_by"], "max-bytes")
        self.assertEqual(self.kept("--max-bytes", "6")[0], ["a", "b", "c", "e"])  # 0 + 1 + 2 + 3
        self.assertEqual(self.kept("--max-bytes", "5")[0], ["a", "b", "e"])
        self.assertEqual(self.kept("--max-bytes", "0")[0], ["e"])  # the empty file costs nothing
        self.assertEqual(doc["totals"]["kept_bytes"], 6)

    def test_both_caps_the_tighter_one_wins(self):
        kept, doc = self.kept("--max-files", "4", "--max-bytes", "3")
        self.assertEqual(kept, ["a", "b", "e"])
        self.assertEqual(doc["caps"]["stopped_by"], "max-bytes")
        kept, doc = self.kept("--max-files", "2", "--max-bytes", "100")
        self.assertEqual(kept, ["a", "e"])
        self.assertEqual(doc["caps"]["stopped_by"], "max-files")

    def test_equal_sizes_are_ordered_by_name(self):
        for n, data in (("z", b"zz"), ("m", b"mm"), ("q", b"qq")):
            self.put(n, data)
        kept, _ = self.kept("--max-files", "6")  # e, a, then the three 2-byte files and b(2): b, m, q, z by name
        self.assertEqual(kept, ["a", "b", "e", "m", "q", "z"])
        kept, _ = self.kept("--max-files", "4")
        self.assertEqual(kept, ["a", "b", "e", "m"])

    def test_dedup_happens_before_the_caps(self):
        # Without dedup first, the copy would take the second slot.
        kept, doc = self.kept("--max-files", "2")
        self.assertEqual(kept, ["a", "e"])
        self.assertEqual({r["name"]: r["reason"] for r in doc["removed"]}["a_copy"], "duplicate")

    def test_apply_deletes_exactly_the_reported_files(self):
        _, doc, _ = self.js("minimize", "--corpus", self.corpus, "--max-files", "3")
        planned = [r["name"] for r in doc["removed"]]
        before = self.names()
        code, doc2, _ = self.js("minimize", "--corpus", self.corpus, "--max-files", "3", "--apply")
        self.assertEqual(code, 0)
        self.assertEqual(doc2["deleted"], planned)
        self.assertEqual(self.names(), sorted(set(before) - set(planned), key=corpus_tool.name_key))
        self.assertEqual(self.names(), ["a", "b", "e"])

    def test_text_output(self):
        code, out, _ = run("minimize", "--corpus", self.corpus, "--max-files", "3", "--max-bytes", "100")
        self.assertEqual(code, 0)
        self.assertIn("Fuzz corpus minimize (dry run, nothing changed)", out)
        self.assertIn("max files: 3", out)
        self.assertIn("max bytes: 100", out)
        self.assertIn("stopped by: max-files", out)
        self.assertIn("a_copy (1 byte, duplicate of a)", out)
        self.assertIn("d (4 bytes, over the caps)", out)


class HostileInputTests(Fixture):
    def test_hostile_names_are_escaped_and_handled(self):
        names = ["line\nbreak", "tab\there", "back\\slash", "-rf", "--apply", " lead", "trail ", "caf\u00e9", ".hidden",
                 "...", "a..b", "\u2028sep", "x" * 200, "q'uote\"s", "$(touch pwned)", "`id`", "a;b", "a b"]
        for n in names:
            try:
                self.put(n, n.encode("utf-8"))
            except (OSError, UnicodeError):
                names.remove(n)
        self.put("dup1", b"d")
        self.put("dup2", b"d")
        code, out, _ = run("minimize", "--corpus", self.corpus, "--max-files", "0")
        self.assertEqual(code, 0)
        self.assertNotIn("\u2028", out)
        self.assertNotIn("\t", out)
        self.assertIn("line\\x0abreak", out)
        self.assertIn("back\\\\slash", out)
        self.assertIn("\\u2028sep", out)
        # A name cannot add a line to the report: line count is fixed by the structure.
        for line in out.splitlines():
            self.assertTrue(line == "" or line[0] != "\x0a")
        code, doc, _ = self.js("dedup", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertEqual([r["name"] for r in doc["removed"]], ["dup2"])
        # JSON text is plain ASCII.
        _, raw, _ = run("report", "--corpus", self.corpus, "--json")
        raw.encode("ascii")
        # Apply never leaves the folder: only dup2 goes.
        code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        self.assertEqual((code, doc["deleted"]), (0, ["dup2"]))
        self.assertFalse(os.path.exists(os.path.join(os.getcwd(), "pwned")))
        self.assertEqual(len(self.names()), len(names) + 1)

    def test_non_utf8_name(self):
        if sys.platform == "win32":
            self.skipTest("file names are text on Windows")
        path = os.path.join(os.fsencode(self.corpus), b"bad\xff\xfename")
        try:
            with open(path, "wb") as f:
                f.write(b"x")
        except OSError:
            self.skipTest("file system does not allow non UTF-8 names")
        self.put("good", b"x")
        code, doc, _ = self.js("dedup", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertEqual(doc["duplicate_groups"][0]["keep"], "bad\\xff\\xfename")  # 'b' sorts before 'g'
        self.assertEqual([r["name"] for r in doc["removed"]], ["good"])
        code, out, _ = run("report", "--corpus", self.corpus, "--json")
        out.encode("ascii")

    def test_display_name_is_injective_and_single_line(self):
        samples = ["a\\x0a", "a\n", "a\\\\", "a\\", "\u00e9", "\x7f", "\x00", "\u202e", "\U0001f600", "\ud800"]
        shown = [corpus_tool.display_name(s) for s in samples]
        self.assertEqual(len(set(shown)), len(samples))
        for s in shown:
            self.assertNotIn("\n", s)
            self.assertNotIn("\r", s)

    def test_symlink_is_a_corpus_problem(self):
        self.put("a", b"same")
        self.put("b", b"same")
        link = os.path.join(self.corpus, "link")
        try:
            os.symlink(self.outside, link)
        except (OSError, NotImplementedError, AttributeError):
            self.skipTest("symlinks not available")
        before = self.snapshot()
        for command in ("report", "dedup", "minimize"):
            code, out, err = run(command, "--corpus", self.corpus, "--apply") if command != "report" else run(command, "--corpus", self.corpus)
            self.assertEqual(code, 1)
            self.assertEqual(out, "")
            self.assertIn("link: symlink", err)
        self.assertEqual(self.names(), ["a", "b", "link"])
        code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        self.assertEqual(code, 1)
        self.assertFalse(doc["ok"])
        self.assertEqual(doc["problems"], [{"name": "link", "reason": "symlink"}])
        self.assertEqual(set(doc), {"schema", "command", "ok", "problems"})
        self.assertEqual(self.outside_bytes(), b"outside")
        self.assertEqual(self.snapshot(), before)

    def test_broken_symlink_and_directory_symlink(self):
        try:
            os.symlink(os.path.join(self.root, "missing"), os.path.join(self.corpus, "broken"))
            os.symlink(self.root, os.path.join(self.corpus, "dirlink"))
        except (OSError, NotImplementedError, AttributeError):
            self.skipTest("symlinks not available")
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 1)
        self.assertEqual([p["name"] for p in doc["problems"]], ["broken", "dirlink"])

    def test_symlinked_corpus_folder_is_rejected(self):
        self.put("a", b"x")
        link = os.path.join(self.root, "corpus_link")
        try:
            os.symlink(self.corpus, link)
        except (OSError, NotImplementedError, AttributeError):
            self.skipTest("symlinks not available")
        self.assertEqual(run("dedup", "--corpus", link, "--apply")[0], 1)

    def test_subfolder_is_a_corpus_problem(self):
        os.mkdir(os.path.join(self.corpus, "sub"))
        self.put("a", b"x", folder=os.path.join(self.corpus, "sub"))
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 1)
        self.assertEqual(doc["problems"], [{"name": "sub", "reason": "not a regular file"}])
        self.assertEqual(run("dedup", "--corpus", self.corpus, "--apply")[0], 1)
        self.assertEqual(self.names(), ["sub"])

    @unittest.skipIf(not hasattr(os, "mkfifo"), "no fifo support")
    def test_fifo_is_rejected_without_opening_it(self):
        os.mkfifo(os.path.join(self.corpus, "pipe"))
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 1)
        self.assertEqual(doc["problems"], [{"name": "pipe", "reason": "not a regular file"}])

    def test_size_cap_boundary(self):
        self.put("exact", b"\0" * (1024 * 1024))
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 0)
        self.assertEqual(doc["summary"]["largest"], 1024 * 1024)
        self.put("over", b"\0" * (1024 * 1024 + 1))
        self.put("dup1", b"d")
        self.put("dup2", b"d")
        code, doc, _ = self.js("dedup", "--corpus", self.corpus, "--apply")
        self.assertEqual(code, 1)
        self.assertEqual(doc["problems"], [{"name": "over", "reason": "file above 1 MiB"}])
        self.assertEqual(self.names(), ["dup1", "dup2", "exact", "over"])  # nothing deleted

    def test_missing_and_non_folder_corpus(self):
        for path in (os.path.join(self.root, "nope"), self.outside):
            for command in ("report", "dedup", "minimize"):
                code, out, err = run(command, "--corpus", path)
                self.assertEqual(code, 1)
                self.assertEqual(out, "")
                self.assertIn("corpus_tool: corpus:", err)
        code, doc, _ = self.js("report", "--corpus", os.path.join(self.root, "nope"))
        self.assertEqual(code, 1)
        self.assertEqual(doc["problems"], [{"name": None, "reason": "corpus folder is missing or unreadable"}])

    def test_unreadable_file_is_a_corpus_problem(self):
        if sys.platform == "win32" or (hasattr(os, "geteuid") and os.geteuid() == 0):
            self.skipTest("permissions are not enforced here")
        path = self.put("secret", b"x")
        os.chmod(path, 0)
        self.addCleanup(os.chmod, path, 0o600)
        code, doc, _ = self.js("report", "--corpus", self.corpus)
        self.assertEqual(code, 1)
        self.assertEqual(doc["problems"], [{"name": "secret", "reason": "unreadable file"}])

    def test_only_files_inside_the_corpus_are_ever_touched(self):
        sibling = os.path.join(self.root, "sibling")
        os.mkdir(sibling)
        self.put("a", b"same", folder=sibling)
        self.put("b", b"same", folder=sibling)
        for n in ("x", "y"):
            self.put(n, b"same")
        before = self.snapshot(sibling), self.snapshot(self.root + "/sibling")
        run("dedup", "--corpus", self.corpus, "--apply")
        run("minimize", "--corpus", self.corpus, "--max-files", "0", "--apply")
        self.assertEqual((self.snapshot(sibling), self.snapshot(self.root + "/sibling")), before)
        self.assertEqual(self.outside_bytes(), b"outside")
        self.assertEqual(sorted(os.listdir(self.root)), ["corpus", "outside.txt", "sibling"])
        self.assertEqual(self.names(), [])


class UsageTests(Fixture):
    def test_usage_errors_exit_2(self):
        c = self.corpus
        bad = [
            [],
            ["bogus"],
            ["report"],
            ["dedup"],
            ["minimize"],
            ["report", "--corpus"],
            ["report", "--corpus", c, "--apply"],
            ["report", "--corpus", c, "--max-files", "1"],
            ["dedup", "--corpus", c, "--max-files", "1"],
            ["dedup", "--corpus", c, "--max-bytes", "1"],
            ["minimize", "--corpus", c, "--max-files", "-1"],
            ["minimize", "--corpus", c, "--max-files", "abc"],
            ["minimize", "--corpus", c, "--max-files", ""],
            ["minimize", "--corpus", c, "--max-files", "1.5"],
            ["minimize", "--corpus", c, "--max-files", "+5"],
            ["minimize", "--corpus", c, "--max-bytes", " 5"],
            ["minimize", "--corpus", c, "--max-bytes", "1_0"],
            ["minimize", "--corpus", c, "--max-bytes", "99999999999999999999"],
            ["minimize", "--corpus", c, "--max-files"],
            ["report", "--corpus", c, "--unknown"],
            ["--json"],
        ]
        for argv in bad:
            before = self.snapshot()
            code, out, err = run(*argv)
            self.assertEqual(code, 2, argv)
            self.assertEqual(out, "", argv)
            self.assertIn("corpus_tool", err)
            self.assertEqual(self.snapshot(), before)

    def test_usage_error_before_any_scan(self):
        self.put("a", b"same")
        self.put("b", b"same")
        self.assertEqual(run("dedup", "--corpus", self.corpus, "--apply", "--max-files", "1")[0], 2)
        self.assertEqual(self.names(), ["a", "b"])

    def test_help_exits_0(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = corpus_tool.main(["--help"])
        self.assertEqual(code, 0)
        self.assertIn("minimize", out.getvalue())


class CommandLineTests(Fixture):
    def call(self, *argv):
        p = subprocess.run([sys.executable, TOOL] + list(argv), stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        return p.returncode, p.stdout, p.stderr

    def test_exit_codes_as_a_process(self):
        self.put("a", b"x")
        self.put("b", b"x")
        self.assertEqual(self.call("report", "--corpus", self.corpus)[0], 0)
        self.assertEqual(self.call("dedup", "--corpus", self.corpus, "--json")[0], 0)
        self.assertEqual(self.call("report", "--corpus", os.path.join(self.root, "nope"))[0], 1)
        self.assertEqual(self.call()[0], 2)
        self.assertEqual(self.call("dedup", "--corpus", self.corpus, "--apply")[0], 0)
        self.assertEqual(self.names(), ["a"])

    def test_process_output_matches_in_process_output(self):
        self.put("a", b"x")
        self.put("b", b"x")
        code, out, _ = self.call("minimize", "--corpus", self.corpus, "--max-files", "1", "--json")
        self.assertEqual(code, 0)
        self.assertEqual(out, run("minimize", "--corpus", self.corpus, "--max-files", "1", "--json")[1])


if __name__ == "__main__":
    unittest.main()
