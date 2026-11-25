import io
import os
import struct
import sys
import tempfile
import time
import unittest
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import golden_report as gr  # noqa: E402


def write_ppm(directory, filename, w, h, raster):
    with open(os.path.join(directory, filename), "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h) + raster)


def slurp(path):
    with open(path, "rb") as f:
        return f.read()


def solid(w, h, rgb):
    return bytes(rgb) * (w * h)


def decode_png(data):
    """Tiny PNG decoder for the files this tool writes: checks signature and CRCs, returns (w, h, rows)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat, ihdr, ended = 8, b"", None, False
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + n]
        crc = struct.unpack(">I", data[pos + 8 + n:pos + 12 + n])[0]
        assert crc == zlib.crc32(kind + body) & 0xFFFFFFFF, "bad CRC"
        if kind == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            ended = True
        pos += 12 + n
    assert ended and ihdr is not None
    w, h, depth, ctype, comp, filt, inter = ihdr
    assert (depth, ctype, comp, filt, inter) == (8, 2, 0, 0, 0)
    raw = zlib.decompress(idat)
    assert len(raw) == h * (1 + w * 3)
    rows = []
    for y in range(h):
        line = raw[y * (1 + w * 3):(y + 1) * (1 + w * 3)]
        assert line[0] == 0
        rows.append(line[1:])
    return w, h, rows


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.actual = os.path.join(self.tmp.name, "actual")
        self.base = os.path.join(self.tmp.name, "base")
        self.out = os.path.join(self.tmp.name, "out")
        os.mkdir(self.actual)
        os.mkdir(self.base)

    def scene(self, name, a, b, w=4, h=4):
        write_ppm(self.actual, name + ".actual.ppm", w, h, a)
        if b is not None:
            write_ppm(self.base, name + ".ppm", w, h, b)

    def run_tool(self, *extra):
        out, err = io.StringIO(), io.StringIO()
        args = gr.parse_args(["--actual-dir", self.actual, "--baseline-dir", self.base] + list(extra))
        code = gr.run(args, out, err)
        return code, out.getvalue(), err.getvalue()


class ReportTests(Base):
    def test_all_pass(self):
        self.scene("a", solid(4, 4, (1, 2, 3)), solid(4, 4, (1, 2, 3)))
        self.scene("b", solid(4, 4, (10, 10, 10)), solid(4, 4, (11, 10, 9)))  # within tolerance 2
        code, text, _ = self.run_tool()
        self.assertEqual(code, 0)
        self.assertTrue(text.startswith("Golden report: 2 pass, 0 fail, 0 size-mismatch, 0 baseline-missing, "
                                        "0 unreadable (tolerance 2)\n"))
        self.assertIn("No failures", text)
        self.assertNotIn("| Name", text)
        self.assertIn("diagnostic only", text)

    def test_known_fail_delta_and_bbox(self):
        b = bytearray(solid(4, 4, (50, 50, 50)))
        a = bytearray(b)
        a[(1 * 4 + 2) * 3] += 9   # pixel (2,1)
        a[(2 * 4 + 3) * 3 + 1] += 5  # pixel (3,2)
        self.scene("s", bytes(a), bytes(b))
        code, text, _ = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("1 fail", text)
        self.assertIn("| s | fail | 4x4 | 2 | 9 | (2,1)-(3,2) |", text.replace("\\", ""))

    def test_size_mismatch(self):
        write_ppm(self.actual, "m.actual.ppm", 4, 4, solid(4, 4, (0, 0, 0)))
        write_ppm(self.base, "m.ppm", 2, 2, solid(2, 2, (0, 0, 0)))
        code, text, _ = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("size-mismatch", text)
        self.assertIn("actual 4x4, baseline 2x2", text)

    def test_missing_baseline(self):
        self.scene("n", solid(4, 4, (0, 0, 0)), None)
        code, text, _ = self.run_tool("--images-dir", self.out)
        self.assertEqual(code, 1)
        self.assertIn("baseline-missing", text)
        w, h, _rows = decode_png(slurp(os.path.join(self.out, "n.actual.png")))
        self.assertEqual((w, h), (512, 512))

    def test_bad_and_oversized_files(self):
        with open(os.path.join(self.actual, "bad.actual.ppm"), "wb") as f:
            f.write(b"not a ppm at all")
        write_ppm(self.base, "bad.ppm", 2, 2, solid(2, 2, (0, 0, 0)))
        with open(os.path.join(self.actual, "huge.actual.ppm"), "wb") as f:
            f.write(b"P6\n9000 9000\n255\n")
        write_ppm(self.base, "huge.ppm", 2, 2, solid(2, 2, (0, 0, 0)))
        code, text, _ = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("2 unreadable", text)

    def test_oversized_side_is_unreadable(self):
        old = gr.MAX_SIDE
        gr.MAX_SIDE = 3
        self.addCleanup(setattr, gr, "MAX_SIDE", old)
        self.scene("big", solid(4, 4, (0, 0, 0)), solid(4, 4, (0, 0, 0)))
        code, text, _ = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("1 unreadable", text)

    def test_name_rejection(self):
        self.scene("ok", solid(4, 4, (0, 0, 0)), solid(4, 4, (0, 0, 0)))
        for bad in (".hidden", "caf\u00e9", "a\\b"):
            write_ppm(self.actual, bad + ".actual.ppm", 4, 4, solid(4, 4, (0, 0, 0)))
        code, text, _ = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("Rejected names, not processed: 3", text)
        self.assertIn("1 pass", text)
        self.assertFalse(gr.safe_name("../x"))
        self.assertFalse(gr.safe_name("a/b"))
        self.assertTrue(gr.safe_name("scene_01"))

    def test_deterministic(self):
        for n in ("z", "a", "m"):
            self.scene(n, solid(4, 4, (0, 0, 0)), solid(4, 4, (20, 0, 0)))
        r1 = self.run_tool("--images-dir", self.out)
        files1 = dict((f, slurp(os.path.join(self.out, f))) for f in sorted(os.listdir(self.out)))
        r2 = self.run_tool("--images-dir", self.out)
        files2 = dict((f, slurp(os.path.join(self.out, f))) for f in sorted(os.listdir(self.out)))
        self.assertEqual(r1, r2)
        self.assertEqual(files1, files2)
        self.assertLess(r1[1].index("| a |"), r1[1].index("| m |"))

    def test_max_failures_limits_images_not_table(self):
        for n in ("a", "b", "c"):
            self.scene(n, solid(4, 4, (0, 0, 0)), solid(4, 4, (30, 0, 0)))
        code, text, _ = self.run_tool("--images-dir", self.out, "--max-failures", "2")
        self.assertEqual(sorted(os.listdir(self.out)), ["a.compare.png", "b.compare.png"])
        for n in "abc":
            self.assertIn("| %s | fail" % n, text)
        self.assertIn("no image because of the limit", text)

    def test_compare_png_panels(self):
        a = bytearray(solid(4, 4, (100, 100, 100)))
        b = bytearray(a)
        a[0] += 1                 # delta 1 at pixel (0,0): within tolerance, visible in diff panel
        a[3] += 50            # pixel (1,0): beyond tolerance, red
        self.scene("p", bytes(a), bytes(b))
        self.run_tool("--images-dir", self.out)
        w, h, rows = decode_png(slurp(os.path.join(self.out, "p.compare.png")))
        self.assertEqual((w, h), (512 * 3 + 8, 512))
        scale, diff_x = 128, 512 * 2 + 8
        self.assertEqual(rows[0][0:3], bytes(b[0:3]))               # baseline panel
        self.assertEqual(rows[0][(512 + 4) * 3:(512 + 4) * 3 + 3], bytes(a[0:3]))  # actual panel
        d0 = rows[0][diff_x * 3:diff_x * 3 + 3]
        self.assertGreater(d0[0], 0)
        self.assertEqual(d0[1:], b"\x00\x00")
        self.assertEqual(rows[0][(diff_x + scale) * 3:(diff_x + scale) * 3 + 3], b"\xff\x00\x00")

    def test_large_image_is_sampled(self):
        a = bytearray(solid(1030, 4, (0, 0, 0)))
        b = bytes(solid(1030, 4, (0, 0, 0)))
        a[5 * 3] = 200
        self.scene("w", bytes(a), b, 1030, 4)
        self.run_tool("--images-dir", self.out)
        w, h, _ = decode_png(slurp(os.path.join(self.out, "w.compare.png")))
        self.assertEqual(h, 2)
        self.assertEqual(w, 344 * 3 + 8)

    def test_markdown_escaping(self):
        self.scene("a|b_c", solid(4, 4, (0, 0, 0)), solid(4, 4, (90, 0, 0)))
        code, text, _ = self.run_tool()
        self.assertIn("a\\|b\\_c", text)
        self.assertNotIn("| a|b", text)

    def test_verbose_lists_pass(self):
        self.scene("ok", solid(4, 4, (0, 0, 0)), solid(4, 4, (0, 0, 0)))
        _, quiet, _ = self.run_tool()
        _, loud, _ = self.run_tool("--verbose")
        self.assertNotIn("| ok", quiet)
        self.assertIn("| ok | pass", loud.replace("\\", ""))

    def test_existing_diff_files_ignored(self):
        self.scene("d", solid(4, 4, (0, 0, 0)), solid(4, 4, (0, 0, 0)))
        write_ppm(self.actual, "d.diff.ppm", 4, 4, solid(4, 4, (255, 255, 255)))
        code, text, _ = self.run_tool()
        self.assertEqual(code, 0)

    def test_empty_folder(self):
        code, text, _ = self.run_tool()
        self.assertEqual(code, 0)
        self.assertIn("nothing was compared", text)

    def test_report_file(self):
        self.scene("a", solid(4, 4, (0, 0, 0)), solid(4, 4, (0, 0, 0)))
        path = os.path.join(self.tmp.name, "r.md")
        code, text, _ = self.run_tool("--report-md", path)
        self.assertEqual(text, "")
        self.assertTrue(slurp(path).decode().startswith("Golden report: 1 pass"))


class ExitCodeTests(Base):
    def test_usage_errors(self):
        err = io.StringIO()
        old = sys.stderr
        sys.stderr = err
        self.addCleanup(setattr, sys, "stderr", old)
        for argv in (["--actual-dir", self.actual], ["--actual-dir", self.actual, "--baseline-dir", self.base,
                                                     "--tolerance", "300"],
                     ["--actual-dir", self.actual, "--baseline-dir", self.base, "--max-failures", "-1"]):
            with self.assertRaises(SystemExit) as cm:
                gr.main(argv)
            self.assertEqual(cm.exception.code, 2)

    def test_missing_directory(self):
        self.assertEqual(gr.main(["--actual-dir", os.path.join(self.tmp.name, "nope"), "--baseline-dir", self.base]), 2)
        self.assertEqual(gr.main(["--actual-dir", self.actual, "--baseline-dir", os.path.join(self.tmp.name, "nope")]), 2)

    def test_exit_codes_via_main(self):
        self.scene("a", solid(4, 4, (0, 0, 0)), solid(4, 4, (0, 0, 0)))
        old = sys.stdout
        sys.stdout = io.StringIO()
        self.addCleanup(setattr, sys, "stdout", old)
        self.assertEqual(gr.main(["--actual-dir", self.actual, "--baseline-dir", self.base]), 0)
        self.scene("b", solid(4, 4, (0, 0, 0)), solid(4, 4, (99, 0, 0)))
        self.assertEqual(gr.main(["--actual-dir", self.actual, "--baseline-dir", self.base]), 1)


class SpeedTests(Base):
    def test_identical_1024_pair_is_fast(self):
        raster = bytes(range(256)) * (1024 * 1024 * 3 // 256)
        self.scene("big", raster, raster, 1024, 1024)
        start = time.time()
        code, text, _ = self.run_tool()
        self.assertEqual(code, 0)
        self.assertLess(time.time() - start, 5.0)


if __name__ == "__main__":
    unittest.main()
