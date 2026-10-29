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
        self.assertEqual([item["status"] for item in report["commands"]], ["external"] * len(commands))

    def test_pi_run_is_external_and_preserves_runner_command(self):
        command = 'pi-run: build/native/probe --benchmark # runner only'
        with patch.object(acceptance, "run_logged", side_effect=AssertionError("Pi command executed locally")):
            code, report = self.run_task([command])
        self.assertEqual(code, 0)
        self.assertEqual(report["commands"], [{"command": command, "status": "external"}])

    def test_size_overflow_and_binary_fail(self):
        with patch.object(subprocess, "check_output", side_effect=[b"abc123\n", b"12\t3\tfile.cpp\0"]):
            code, report = self.run_task(['python -c "pass"'], "<= 10 changed lines, <= 1 files")
        self.assertEqual(code, 1)
        self.assertEqual(report["size"]["changed_lines"], 15)
        with patch.object(subprocess, "check_output", side_effect=[b"abc123\n", b"-\t-\tfile.bin\0"]):
            self.assertEqual(acceptance.size_check(self.root, "<= 10 changed lines, <= 1 files")["status"], "failed")

    def test_stacked_base_counts_own_diff_and_reports_resolved_revision(self):
        def git(*args):
            return subprocess.check_output(["git", *args], cwd=self.root, stderr=subprocess.PIPE).decode().strip()
        git("init", "-b", "main")
        git("config", "user.email", "test@example.invalid")
        git("config", "user.name", "Test")
        for name in ("main", "parent", "child"):
            (self.root / (name + ".txt")).write_text(name + "\n")
            git("add", name + ".txt")
            git("commit", "-m", name)
            if name == "main":
                git("update-ref", "refs/remotes/origin/main", "HEAD")
            elif name == "parent":
                git("branch", "parent")
        code, default = self.run_task(['python -c "pass"'], "<= 1 changed lines, <= 1 files")
        self.assertEqual(code, 1)
        self.assertEqual(default["size"]["files"], 2)
        code, own = self.run_task(['python -c "pass"'], "<= 1 changed lines, <= 1 files", base="parent")
        self.assertEqual(code, 0)
        self.assertEqual(own["size"]["files"], 1)
        self.assertEqual(own["size"]["base"], "parent")
        self.assertEqual(own["size"]["base_commit"], git("rev-parse", "parent"))
        for invalid in ("missing-ref", "--help"):
            code, report = self.run_task(['python -c "pass"'], "<= 1 changed lines, <= 1 files", base=invalid)
            self.assertEqual(code, 1)
            self.assertEqual(report["commands"], [])

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

    def test_pr_selection_optional_without_closes(self):
        issue, task_path, message = acceptance.select_pr_task(self.root, "Fix tests")
        self.assertIsNone(issue)
        self.assertIsNone(task_path)
        self.assertIn("not selected", message)

    def test_pr_selection_only_allows_two_explicit_legacy_exceptions(self):
        for issue in (45, 64):
            number, path, message = acceptance.select_pr_task(self.root, f"Closes #{issue}")
            self.assertEqual(number, issue)
            self.assertIsNone(path)
            self.assertIn("legacy exception — not verified", message)
            self.assertIn("not an acceptance pass", message)
        for issue in (44, 46, 63, 65, 999):
            with self.subTest(issue=issue), self.assertRaisesRegex(ValueError, "not verified.*found 0"):
                acceptance.select_pr_task(self.root, f"Closes #{issue}")

    def test_pr_selection_missing_or_ambiguous_task_is_not_verified(self):
        directory = self.root / "docs/tasks"
        directory.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "not verified.*found 0"):
            acceptance.select_pr_task(self.root, "Closes #114")
        first = directory / "one.md"
        first.write_text(task(["python -c 'pass'"]).replace("#85", "#114"), encoding="utf-8")
        number, path, message = acceptance.select_pr_task(self.root, "Closes #114")
        self.assertEqual((number, path, message), (114, first, ""))
        (directory / "two.md").write_text(task(["python -c 'pass'"]).replace("#85", "#114"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "not verified.*found 2"):
            acceptance.select_pr_task(self.root, "Closes #114")

    def test_pr_selection_missing_and_malformed_acceptance_block_fail(self):
        directory = self.root / "docs/tasks"
        directory.mkdir(parents=True)
        source = directory / "one.md"
        for contents in (
            "# Missing acceptance (#114)\n",
            "# Invalid block (#114)\n```acceptance\ntier: T0\nsize: n/a\nmust_pass:\n```\n",
            "# Invalid tier (#114)\n" + task(["python -c 'pass'"]).replace("#85", "#999").replace("tier: T0", "tier: something"),
        ):
            with self.subTest(contents=contents):
                source.write_text(contents, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "not verified"):
                    acceptance.select_pr_task(self.root, "Closes #114")

    def test_timeout_preserves_failure(self):
        code, report = self.run_task(['python -c "import time; time.sleep(10)"'], timeout=.1)
        self.assertEqual(code, 1)
        self.assertTrue(report["commands"][0]["timed_out"])

    def test_unsupported_command_is_failed(self):
        code, report = self.run_task(["echo unsupported"])
        self.assertEqual(code, 1)
        self.assertIn("unsupported", report["commands"][0]["error"])

    def test_plain_python_executes_without_bash_and_uses_active_interpreter(self):
        with patch.object(acceptance.shutil, "which", side_effect=AssertionError("Bash must not be queried")):
            code, report = self.run_task(['python3 -c "print(456)" # inline task comment'])
        self.assertEqual(code, 0)
        self.assertEqual(Path(report["commands"][0]["log"]).read_text().strip(), "456")
        self.assertEqual(acceptance.command_argv('python -c "pass"'), [sys.executable, "-c", "pass"])

    def test_quoted_regex_code_and_inline_comments_remain_native_arguments(self):
        self.assertEqual(acceptance.command_argv('ctest -R "^(first|second)$" --output-on-failure # && ignored'),
                         ["ctest", "-R", "^(first|second)$", "--output-on-failure"])
        self.assertEqual(acceptance.command_argv('python -c "print(1); print(2)" # note'),
                         [sys.executable, "-c", "print(1); print(2)"])

    def test_named_static_and_hook_tools_run_natively(self):
        for command in ('actionlint .github/workflows/static.yml', 'pre-commit run --all-files'):
            self.assertEqual(acceptance.command_argv(command), command.split())

    def test_real_shell_operators_and_explicit_scripts_use_bash(self):
        with patch.object(acceptance.shutil, "which", return_value="working-bash"):
            for command in ('python -c "pass" && ctest', 'ctest | python filter.py', 'ctest > report.txt'):
                self.assertEqual(acceptance.command_argv(command), ["working-bash", "-c", command])
            self.assertEqual(acceptance.command_argv('bash "tools/test script.sh" # note'),
                             ["working-bash", "tools/test script.sh"])

    def test_missing_bash_is_actionable_and_does_not_skip_native_command(self):
        with patch.object(acceptance.shutil, "which", return_value=None):
            code, report = self.run_task(['bash tools/test.sh', 'python -c "pass"'])
        self.assertEqual(code, 1)
        self.assertIn("Bash is required", report["commands"][0]["error"])
        self.assertEqual(report["commands"][1]["status"], "passed")


if __name__ == "__main__":
    unittest.main()
