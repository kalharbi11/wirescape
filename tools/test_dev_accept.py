# SPDX-License-Identifier: GPL-3.0-only
"""Focused CLI acceptance contract tests (discoverable via test_dev*.py)."""
import unittest
from foundation.acceptance import classify_line
from dev import make_parser


class DevAcceptanceTests(unittest.TestCase):
    def test_line_kinds(self):
        samples = {
            "python -m unittest": "run",
            "ci: guard": "ci:",
            "pi-run: ctest --test-dir build/native": "pi-run:",
            "pi: build + tests (descriptive)": "pi:",
            "user: listen": "user:",
            "future-lane: unsupported": "unknown",
        }
        for command, expected in samples.items():
            with self.subTest(command=command):
                self.assertEqual(classify_line(command), expected)

    def test_strict_cli_switch(self):
        self.assertTrue(make_parser().parse_args(["accept", "--file", "task.md", "--strict"]).strict)
        self.assertFalse(make_parser().parse_args(["accept", "--file", "task.md"]).strict)


if __name__ == "__main__":
    unittest.main()
