# SPDX-License-Identifier: GPL-3.0-only
import os
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import static_checks

ANALYSIS_UNITS = static_checks.analysis_units

DIFF = "+++ b/native/test.cpp\n@@ -1 +2,3 @@\n+x\n@@ -8 +10,0 @@\n-x\n+++ /dev/null\n@@ -1 +0,0 @@\n-x\n"


class StaticChecksTests(unittest.TestCase):
    def setUp(self):
        def units(root, files):
            return [{"file": str(root / f), "directory": str(root),
                     "command": "c++ -c " + f} for f in files]
        mock = patch.object(static_checks, "analysis_units", side_effect=units)
        mock.start()
        self.addCleanup(mock.stop)

    def test_compiler_dependencies_map_nested_header_and_xmacro(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            build = root / "build/static"
            build.mkdir(parents=True)
            entries = [{"directory": str(build), "file": str(root / "native/test.cpp"),
                        "command": "c++ -o test.o -c " + str(root / "native/test.cpp")}]
            (build / "compile_commands.json").write_text(json.dumps(entries))
            deps = "test.o: #deps 2, deps mtime 1 (VALID)\n    " + str(root / "native/nested.h")
            deps += "\n    " + str(root / "native/catalog.inc") + "\n"
            with patch.object(static_checks.subprocess, "check_output", return_value=deps):
                self.assertEqual(ANALYSIS_UNITS(root, {"native/nested.h": [], "native/catalog.inc": []}), entries)
                with self.assertRaisesRegex(ValueError, "No compiled owner"):
                    ANALYSIS_UNITS(root, {"native/orphan.h": []})

    def test_missing_database_fails_closed_but_runs_other_tools(self):
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(static_checks, "analysis_units", side_effect=FileNotFoundError("database")), \
                patch.object(static_checks.subprocess, "run", return_value=subprocess.CompletedProcess([], 1, "", "")) as run:
            self.assertEqual(static_checks.run_checks(Path(tmp), DIFF, "unused"), 1)
            self.assertIn("database", run.call_args_list[0].args[0][-1])
            self.assertEqual(run.call_args_list[-1].args[0], ["actionlint"])

    def test_ranges_ignore_deletions_and_non_cpp(self):
        diff = DIFF + "+++ b/tools/script.py\n@@ -1 +1 @@\n+x\n"
        self.assertEqual(static_checks.changed_ranges(diff), {"native/test.cpp": [(2, 4)]})

    def test_all_tools_run_and_summary_survives_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            summary = Path(tmp) / "summary.md"
            def execute(command, **kwargs):
                if command[0] == "cppcheck":
                    return subprocess.CompletedProcess(command, 1, "", "native/test.cpp:2:1: warning: finding\n")
                return subprocess.CompletedProcess(command, 0, "", "")
            with patch.dict(os.environ, {"GITHUB_STEP_SUMMARY": str(summary)}), \
                    patch.object(static_checks.subprocess, "run", side_effect=execute) as run:
                self.assertEqual(static_checks.run_checks(Path(tmp), DIFF + "+++ b/tools/script.py\n", "tidy.py"), 1)
                self.assertEqual(run.call_count, 5)
                commands = [call.args[0] for call in run.call_args_list]
                self.assertIn("--lines=2:4", commands[2])
                self.assertIn("--line-filter=", " ".join(commands[0]))
                self.assertIn("| cppcheck changed files | 1 | failed |", summary.read_text())
                self.assertIn("ruff changed files", summary.read_text())

    def test_cpp_headers_and_c_sources_use_their_own_language_modes(self):
        diff = "+++ b/native/rng.hpp\n@@ -0,0 +1 @@\n+namespace rng {}\n+++ b/native/driver.c\n@@ -0,0 +1 @@\n+int value;\n"
        with patch.object(static_checks.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            self.assertEqual(static_checks.run_checks(Path("."), diff, "tidy.py"), 0)
        commands = [c.args[0] for c in run.call_args_list if c.args[0][0] == "cppcheck"]
        self.assertEqual(len(commands), 1)
        self.assertIn("--library=qt", commands[0])
        project = next(arg.split("=", 1)[1] for arg in commands[0] if arg.startswith("--project="))
        entries = json.loads(Path(project).read_text())
        self.assertEqual({Path(e["file"]).name for e in entries}, {"rng.hpp", "driver.c"})

    def test_ruff_checks_only_changed_surviving_tools_files(self):
        diff = '+++ b/tools/changed.py\n+++ /dev/null\n+++ b/docs/example.py\n+++ "b/tools/space name.py"\n+++ b/tools/changed.py\n'
        with patch.object(static_checks.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            self.assertEqual(static_checks.run_checks(Path("."), diff, "tidy.py"), 0)
            self.assertEqual(run.call_args_list[-1].args[0], ["ruff", "check", "--output-format", "concise", "--", "./tools/changed.py", "./tools/space name.py"])

    def test_missing_tool_does_not_skip_other_checks(self):
        with patch.object(static_checks.subprocess, "run", side_effect=FileNotFoundError("tool missing")) as run:
            self.assertEqual(static_checks.run_checks(Path("."), "", "tidy.py"), 1)
            self.assertEqual(run.call_count, 1)

    def test_deletion_only_still_cppchecks_without_formatting_whole_file(self):
        diff = "+++ b/native/test.cpp\n@@ -3,2 +3,0 @@\n-x\n-y\n"
        with patch.object(static_checks.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            self.assertEqual(static_checks.run_checks(Path("."), diff, "tidy.py"), 0)
            self.assertEqual([c.args[0][0] for c in run.call_args_list], ["cppcheck", "actionlint"])


if __name__ == "__main__":
    unittest.main()
