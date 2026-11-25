"""Placeholder tests for corpus_tool.py. The contributor replaces this file.
Run: python3 -m unittest discover -s tools/fuzz-tools/tests
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import corpus_tool  # noqa: E402


class PlaceholderTest(unittest.TestCase):
    def test_usage_error_until_implemented(self):
        self.assertEqual(corpus_tool.main([]), 2)


if __name__ == "__main__":
    unittest.main()
