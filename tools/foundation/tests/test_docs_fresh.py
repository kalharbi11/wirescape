# SPDX-License-Identifier: GPL-3.0-only
import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import docs_fresh


FIXTURE = Path(__file__).parent / "fixtures/now_state.json"


class DocsFreshTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        self.now = docs_fresh.datetime.fromisoformat(self.data["now"].replace("Z", "+00:00"))

    def check(self):
        return docs_fresh.check(
            self.data["plan"], lambda n: self.data["issues"][n],
            lambda s: self.data["commits"][s], self.now)

    def test_offline_fixture_passes(self):
        self.assertEqual(self.check(), [])
        result = subprocess.run(
            [sys.executable, str(docs_fresh.ROOT / "tools/docs_fresh.py"),
             "--offline", str(FIXTURE)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_closed_pr_under_open_prs(self):
        self.data["issues"]["12"]["state"] = "closed"
        self.assertIn("#12: closed PR", "\n".join(self.check()))

    def test_closed_issue_under_next(self):
        self.data["issues"]["13"]["state"] = "closed"
        self.assertIn("#13: closed issue", "\n".join(self.check()))

    def test_old_main_sha(self):
        self.data["commits"]["abcdef0123456789"]["commit"]["committer"]["date"] = "2026-10-20T00:00:00Z"
        self.assertIn("older than 3 days", "\n".join(self.check()))

    def test_more_than_thirty_lines(self):
        self.data["plan"] = self.data["plan"].replace(
            "\n## Vision", "\n" + "\n".join("extra" for _ in range(31)) + "\n## Vision")
        self.assertIn("maximum 30", "\n".join(self.check()))

    def test_api_errors_fail_closed(self):
        del self.data["issues"]["12"]
        self.assertIn("cannot check GitHub state", "\n".join(self.check()))


if __name__ == "__main__":
    unittest.main()
