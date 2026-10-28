# SPDX-License-Identifier: GPL-3.0-only
import argparse
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from foundation import acceptance


def task(commands, size="<= 450 changed lines, <= 6 files"):
    return ("# Fixture (#85)\n```acceptance\ntier: T0\nbudget: test\nsize: " + size
            + "\nmust_pass:\n" + "".join("  - " + command + "\n" for command in commands)
            + "stops: []\n```\n")


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.file = self.root / "task.md"

    def run_task(self, commands, size="n/a", **overrides):
        self.file.write_text(task(commands, size), encoding="utf-8")
        args = argparse.Namespace(issue=85, file=str(self.file), timeout=5, json=True)
        for key, value in overrides.items():
            setattr(args, key, value)
        output = io.StringIO()
        with patch.object(acceptance, "git_revision", return_value="head"), redirect_stdout(output):
            code = acceptance.run(self.root, args)
        report = json.loads(output.getvalue())
        self.assertEqual(json.loads((self.root / "build_reports/accept-85.json").read_text()), report)
        self.assertTrue((self.root / "build_reports/accept-85.md").is_file())
        return code, report

    def test_success_json_is_only_summary_and_logs_are_separate(self):
        code, report = self.run_task(['python -c "print(123)"'])
        self.assertEqual(code, 0)
        self.assertEqual(report["commands"][0]["status"], "passed")
        self.assertEqual(Path(report["commands"][0]["log"]).read_text().strip(), "123")

    def test_failure_does_not_skip_other_independent_commands(self):
        code, report = self.run_task(['python -c "raise SystemExit(7)"', 'python -c "pass"'])
        self.assertEqual(code, 1)
        self.assertEqual([item["status"] for item in report["commands"]], ["failed", "passed"])
        self.assertEqual(report["commands"][0]["exit_code"], 7)

    def test_external_commands_are_listed_and_never_executed(self):
        commands = [prefix + " must not execute" for prefix in acceptance.EXTERNAL]
        code, report = self.run_task(commands)
        self.assertEqual(code, 0)
        self.assertEqual([item["status"] for item in report["commands"]], ["external"] * 4)

    def test_size_overflow_and_binary_fail(self):
        with patch.object(subprocess, "check_output", return_value=b"12\t3\tfile.cpp\0"):
            code, report = self.run_task(['python -c "pass"'], "<= 10 changed lines, <= 1 files")
        self.assertEqual(code, 1)
        self.assertEqual(report["size"]["changed_lines"], 15)
        with patch.object(subprocess, "check_output", return_value=b"-\t-\tfile.bin\0"):
            self.assertEqual(acceptance.size_check(self.root, "<= 10 changed lines, <= 1 files")["status"], "failed")

    def test_last_block_selected_and_missing_block_rejected(self):
        self.assertEqual(acceptance.parse(task(["python first"]) + task(["python last"]))["must_pass"], ["python last"])
        for text in ("no block", task([]), task(["python pass"], "unknown")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                acceptance.parse(text)

    def test_title_discovery_and_ambiguous_match(self):
        directory = self.root / "docs/tasks"
        directory.mkdir(parents=True)
        first = directory / "first.md"
        first.write_text(task(["python pass"]))
        self.assertEqual(acceptance.find_task(self.root, 85), first)
        (directory / "second.md").write_text(task(["python pass"]))
        with self.assertRaisesRegex(ValueError, "found 2"):
            acceptance.find_task(self.root, 85)

    def test_timeout_preserves_failure(self):
        code, report = self.run_task(['python -c "import time; time.sleep(10)"'], timeout=.1)
        self.assertEqual(code, 1)
        self.assertTrue(report["commands"][0]["timed_out"])

    def test_unsupported_command_is_failed(self):
        code, report = self.run_task(["echo unsupported"])
        self.assertEqual(code, 1)
        self.assertIn("unsupported", report["commands"][0]["error"])


if __name__ == "__main__":
    unittest.main()
