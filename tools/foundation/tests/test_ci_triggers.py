# SPDX-License-Identifier: GPL-3.0-only
"""Text-only CI workflow trigger and formatting-delivery contract."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


class CITriggers(unittest.TestCase):
    def test_no_ready_event_rerun(self):
        for name in ("ci.yml", "portability.yml"):
            with self.subTest(name=name):
                workflow = source(".github/workflows/" + name)
                events = re.search(r"(?m)^    types: \[([^]]+)\]$", workflow)
                self.assertIsNotNone(events)
                self.assertEqual(events.group(1), "opened, synchronize, reopened")

    def test_drafts_receive_windows_check(self):
        workflow = source(".github/workflows/ci.yml")
        matrix = next(line for line in workflow.splitlines() if "include: ${{ fromJSON" in line)
        self.assertIn("github.event_name == 'pull_request'", matrix)
        self.assertIn('"windows-x64"', matrix)
        self.assertNotIn("draft", matrix)
        self.assertNotIn("ci/windows", matrix)

    def test_cloud_clang_format_is_pinned(self):
        setup = source("tools/ci/codex_cloud_setup.sh")
        self.assertIn("clang-format==18.", setup)
        self.assertIn("clang-format --version", setup)

    def test_static_provides_applyable_patch(self):
        workflow = source(".github/workflows/static.yml")
        self.assertIn("FORMAT_PATCH_BEGIN", workflow)
        self.assertIn('Path("format.patch").write_text', workflow)
        self.assertIn("actions/upload-artifact@v4", workflow)
        self.assertIn("name: format-patch", workflow)
