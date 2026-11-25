"""Tests for ppm_diff.py. Run: python3 -m unittest discover -s tools/golden"""
import contextlib
import io
import os
import tempfile
import unittest

import ppm_diff


def ppm(w, h, rgb):
    return b"P6\n%d %d\n255\n" % (w, h) + bytes(rgb)


class PpmDiffTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)

    def write(self, name, data):
        p = os.path.join(self.dir.name, name)
        with open(p, "wb") as f:
            f.write(data)
        return p

    def run_main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = ppm_diff.main(list(args))
        return code, out.getvalue(), err.getvalue()

    def test_identical(self):
        a = self.write("a.ppm", ppm(2, 2, [10] * 12))
        code, out, _ = self.run_main(a, a)
        self.assertEqual(code, 0)
        self.assertIn("bad pixels: 0", out)
        self.assertIn("bounding box: none", out)

    def test_within_tolerance(self):
        a = self.write("a.ppm", ppm(2, 1, [10, 10, 10, 20, 20, 20]))
        b = self.write("b.ppm", ppm(2, 1, [12, 10, 10, 20, 20, 18]))
        code, out, _ = self.run_main(a, b)
        self.assertEqual(code, 0)
        self.assertIn("max delta: 2", out)

    def test_beyond_tolerance_reports_box(self):
        base = [0] * (4 * 3 * 3)
        other = list(base)
        other[(1 * 4 + 1) * 3] = 3   # pixel (1,1)
        other[(2 * 4 + 3) * 3 + 2] = 9  # pixel (3,2)
        a = self.write("a.ppm", ppm(4, 3, base))
        b = self.write("b.ppm", ppm(4, 3, other))
        code, out, _ = self.run_main(a, b)
        self.assertEqual(code, 1)
        self.assertIn("bad pixels: 2", out)
        self.assertIn("max delta: 9", out)
        self.assertIn("bounding box: x 1..3, y 1..2", out)

    def test_tolerance_option(self):
        a = self.write("a.ppm", ppm(1, 1, [0, 0, 0]))
        b = self.write("b.ppm", ppm(1, 1, [3, 0, 0]))
        self.assertEqual(self.run_main(a, b)[0], 1)
        self.assertEqual(self.run_main(a, b, "--tolerance", "3")[0], 0)

    def test_size_mismatch(self):
        a = self.write("a.ppm", ppm(1, 1, [0, 0, 0]))
        b = self.write("b.ppm", ppm(2, 1, [0] * 6))
        code, out, _ = self.run_main(a, b)
        self.assertEqual(code, 1)
        self.assertIn("size mismatch", out)

    def test_bad_files(self):
        good = self.write("g.ppm", ppm(1, 1, [0, 0, 0]))
        cases = {
            "missing": os.path.join(self.dir.name, "nope.ppm"),
            "p3": self.write("p3.ppm", b"P3\n1 1\n255\n0 0 0\n"),
            "short": self.write("short.ppm", b"P6\n2 2\n255\n\x00\x00\x00"),
            "long": self.write("long.ppm", ppm(1, 1, [0, 0, 0]) + b"x"),
            "maxval": self.write("mv.ppm", b"P6\n1 1\n65535\n" + bytes(6)),
            "empty": self.write("empty.ppm", b""),
        }
        for name, path in cases.items():
            with self.subTest(name):
                code, _, err = self.run_main(good, path)
                self.assertEqual(code, 2)
                self.assertIn("error:", err)

    def test_header_comment(self):
        a = self.write("a.ppm", b"P6\n# note\n1 1\n255\n\x01\x02\x03")
        b = self.write("b.ppm", ppm(1, 1, [1, 2, 3]))
        self.assertEqual(self.run_main(a, b)[0], 0)


if __name__ == "__main__":
    unittest.main()
