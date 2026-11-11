# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from foundation import acceptance_preflight as pre


def task(*commands, extra=""):
    body = "".join("  - " + c + "\n" for c in commands)
    return ("# T (#1)\n```acceptance\ntier: T0\nsize: <= 10 changed lines, <= 1 files\n"
            "must_pass:\n" + body + "stops: []\n```\n" + extra)


class PreflightTests(unittest.TestCase):
    def test_valid_nested_chromium_command(self):
        cmd = "bash -c 'cd apps/studio && npx playwright test --project=\"chromium\" -g \"a b\"'"
        self.assertEqual(pre.check_text(task(cmd)), [])

    def test_mixed_external_lines_are_not_checked_as_shell(self):
        text = task("python -m unittest x", "ci: guard green", "pi-run: it's fine", "pi: a 'b")
        self.assertEqual(pre.check_text(text), [])

    def test_unmatched_quote_names_command_index(self):
        problems = pre.check_text(task("python -m unittest x", "python -c 'print(1)"))
        self.assertEqual(len(problems), 1)
        self.assertTrue(problems[0].startswith("command 2:"))

    def test_duplicate_block_reported(self):
        text = task("python -m unittest x")
        problems = pre.check_text(text + text)
        self.assertIn("expected exactly one acceptance block, found 2", problems)

    def test_missing_block_reported(self):
        self.assertEqual(pre.check_text("# none\n"), ["expected exactly one acceptance block, found 0"])


if __name__ == "__main__":
    unittest.main()
