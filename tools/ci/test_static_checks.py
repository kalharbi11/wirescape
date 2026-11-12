# SPDX-License-Identifier: GPL-3.0-only
import os
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.ci import static_checks

ANALYSIS_UNITS = static_checks.analysis_units

DIFF = "+++ b/native/test.cpp\n@@ -1 +2,3 @@\n+x\n@@ -8 +10,0 @@\n-x\n+++ /dev/null\n@@ -1 +0,0 @@\n-x\n"


class StaticChecksTests(unittest.TestCase):
    def setUp(self):
        def units(root, files):
            return [{"file": str(root / f), "directory": str(root),
                     "command": "c++ -c " + f, "build": str(root / "build/static")} for f in files]
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
                self.assertEqual(ANALYSIS_UNITS(root, {"native/nested.h": [], "native/catalog.inc": []},
                                                builds=("build/static",)), [{**e, "build": str(build),
                                                                           "cppcheck_model": "native"}
                                                                          for e in entries])
                with self.assertRaisesRegex(ValueError, "No compiled owner"):
                    ANALYSIS_UNITS(root, {"native/orphan.h": []}, builds=("build/static",))

    def test_multiple_builds_require_valid_databases_and_union_ownership(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            for name, source in (("static", "native/test.cpp"), ("static-web", "native/web/test.cpp")):
                build = root / "build" / name
                build.mkdir(parents=True)
                entry = {"directory": str(build), "file": str(root / source),
                         "command": "c++ -o test.o -c " + str(root / source)}
                (build / "compile_commands.json").write_text(json.dumps([entry]))
            with patch.object(static_checks.subprocess, "check_output", return_value="test.o: #deps 0 (VALID)\n"):
                units = ANALYSIS_UNITS(root, {"native/web/test.cpp": []})
                self.assertEqual([u["build"] for u in units], [str(root / "build/static-web")])
                for stale in (0, 1):
                    deps = ["test.o: #deps 0 (VALID)\n"] * 2
                    deps[stale] = "test.o: #deps 0 (STALE)\n"
                    with patch.object(static_checks.subprocess, "check_output", side_effect=deps):
                        with self.assertRaisesRegex(ValueError, "Stale compiler dependencies"):
                            ANALYSIS_UNITS(root, {"native/web/test.cpp": []})
                (root / "build/static-web/compile_commands.json").unlink()
                with self.assertRaises(FileNotFoundError):
                    ANALYSIS_UNITS(root, {"native/test.cpp": []})


    def test_transitive_product_headers_win_even_in_mixed_qt_and_document_tus(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            build = root / "build/static"
            build.mkdir(parents=True)
            cases = [
                ("native/engine2/core.cpp", "core.o", ["native/engine2/common.h"], "native"),
                ("native/document/reader.cpp", "reader.o",
                 ["native/modules2/macro.inc", "external/QtCore/QObject"], "native"),
                ("native/web/writer.cpp", "writer.o", ["native/sidecar/session.h"], "native"),
                ("native/ui/qt_only.cpp", "qt.o", ["external/QtCore/QObject"], "qt"),
                ("native/ui/mixed.cpp", "mixed.o",
                 ["external/QtCore/QObject", "native/engine2/common.h"], "native"),
                ("native/misc/unknown.cpp", "unknown.o", ["native/misc/local.h"], "native"),
            ]
            entries, blocks = [], []
            for source, obj, deps, _ in cases:
                entries.append({"directory": str(build), "file": str(root / source),
                                "command": f"c++ -DKEEP_COMPILE_FLAGS -o {obj} -c {root / source}"})
                blocks.append(f"{obj}: #deps {len(deps)}, deps mtime 1 (VALID)")
                blocks.extend("    " + str(root / dep) for dep in deps)
            (build / "compile_commands.json").write_text(json.dumps(entries))
            with patch.object(static_checks.subprocess, "check_output", return_value="\n".join(blocks) + "\n"):
                wanted = {source: [(1, 1)] for source, _, _, _ in cases}
                selected = ANALYSIS_UNITS(root, wanted, builds=("build/static",))
                self.assertEqual([(Path(x["file"]).relative_to(root).as_posix(), x["cppcheck_model"])
                                  for x in selected],
                                 [(source, model) for source, _, _, model in cases])
                # A changed product header reaches both native and Qt-facing includers
                # through the compiler-recorded dependency graph, not guessed by path.
                selected = ANALYSIS_UNITS(root, {"native/engine2/common.h": []},
                                          builds=("build/static",))
                self.assertEqual({Path(x["file"]).name for x in selected},
                                 {"core.cpp", "mixed.cpp"})
                self.assertTrue(all(x["cppcheck_model"] == "native" for x in selected))
                selected = ANALYSIS_UNITS(root, {"native/modules2/macro.inc": []},
                                          builds=("build/static",))
                self.assertEqual([Path(x["file"]).name for x in selected], ["reader.cpp"])
                self.assertEqual(selected[0]["cppcheck_model"], "native")

    def test_cppcheck_native_and_qt_projects_preserve_commands_and_gate_options(self):
        entries = [
            {"file": "/repo/native/document/reader.cpp", "directory": "/repo",
             "command": "c++ -DKEEP_FLAG -c reader.cpp", "build": "/repo/build/static",
             "cppcheck_model": "native"},
            {"file": "/repo/native/ui/qt_only.cpp", "directory": "/repo",
             "command": "c++ -DKEEP_FLAG -c qt_only.cpp", "build": "/repo/build/static",
             "cppcheck_model": "qt"},
        ]
        diff = "+++ b/native/document/reader.cpp\n@@ -1 +1 @@\n+int x;\n" + (
            "+++ b/native/ui/qt_only.cpp\n@@ -1 +1 @@\n+int y;\n")
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(static_checks, "analysis_units", return_value=entries), \
                patch.object(static_checks.subprocess, "run",
                             return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            self.assertEqual(static_checks.run_checks(Path(tmp), diff, "unused"), 0)
            cpp = [x.args[0] for x in run.call_args_list if x.args[0][0] == "cppcheck"]
            self.assertEqual(len(cpp), 2)
            self.assertNotIn("--library=qt", cpp[0])
            self.assertIn("--library=qt", cpp[1])
            self.assertTrue(all("--inline-suppr" in c and "--error-exitcode=42" in c for c in cpp))
            for command, owner in zip(cpp, entries):
                project = next(a.partition("=")[2] for a in command if a.startswith("--project="))
                content = json.loads(Path(project).read_text())
                self.assertEqual(content, [{k: v for k, v in owner.items()
                                            if k not in ("build", "cppcheck_model")}])
                self.assertIn("-DKEEP_FLAG", content[0]["command"])

    def test_tidy_selects_each_units_build(self):
        entries = [{"file": "native/web/test.cpp", "directory": "/repo", "command": "c++ -c test.cpp",
                    "build": "/repo/build/static-web"}]
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(static_checks, "analysis_units", return_value=entries), \
                patch.object(static_checks.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            self.assertEqual(static_checks.run_checks(Path(tmp), DIFF, "unused"), 0)
            self.assertEqual(run.call_args_list[0].args[0][1:3], ["-p", entries[0]["build"]])
            project = json.loads((Path(tmp) / "build/static/cppcheck_changed.json").read_text())
            self.assertNotIn("build", project[0])

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
        self.assertNotIn("--library=qt", commands[0])
        # Honour vendor annotations such as nlohmann/json_fwd.hpp's "cppcheck-suppress syntaxError".
        self.assertIn("--inline-suppr", commands[0])
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


    def baseline_fixture(self, root):
        source = root / "native/modules2/macro_oscillator/CMakeLists.txt"
        source.parent.mkdir(parents=True)
        source.write_text("pinned source config")
        data = {"schema": 1, "source_sha256": {
            source.relative_to(root).as_posix(): hashlib.sha256(source.read_bytes()).hexdigest()},
            "diagnostics": [{"path": "eurorack/plaits/test.h", "line": 10, "column": 2,
                             "id": "uninitMemberVar", "message": "Existing vendor finding"}]}
        (source.parent / "cppcheck-baseline.json").write_text(json.dumps(data))
        return source

    def test_only_exact_pinned_vendor_findings_are_baselined(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            self.baseline_fixture(root)
            finding = "plaits/test.h:10:2: warning: Existing vendor finding [uninitMemberVar]"
            for build in ("static", "static-web"):
                output = str(root / ("build/" + build + "/_deps/macro_eurorack-src")) + "/" + finding
                filtered, code, count = static_checks.pinned_cppcheck_baseline(root, output, 42)
                self.assertEqual((filtered, code, count), ("", 0, 1))
                # Crashes/non-findings exits are never converted into success.
                self.assertEqual(static_checks.pinned_cppcheck_baseline(root, output, 1)[1], 1)
                for changed in (output.replace(":10:", ":11:"),
                                output.replace("Existing vendor", "New vendor"),
                                output.replace("uninitMemberVar", "otherFinding"),
                                output.replace("warning:", "error:"),
                                output.replace("build/" + build + "/_deps/macro_eurorack-src", "native")):
                    self.assertEqual(static_checks.pinned_cppcheck_baseline(root, changed, 42),
                                     (changed, 42, 0))

    def test_source_pin_change_invalidates_vendor_baseline(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            source = self.baseline_fixture(root)
            source.write_text("different dependency config")
            with self.assertRaisesRegex(ValueError, "invalidated"):
                static_checks.pinned_cppcheck_baseline(root, "", 0)

    def test_mixed_vendor_and_first_party_findings_still_fail(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            self.baseline_fixture(root)
            vendor = str(root / "build/static/_deps/macro_eurorack-src/plaits/test.h")
            known = vendor + ":10:2: warning: Existing vendor finding [uninitMemberVar]"
            own = str(root / "native/test.h") + ":10:2: warning: Real defect [uninitMemberVar]"
            filtered, code, count = static_checks.pinned_cppcheck_baseline(root, known + "\n" + own, 42)
            self.assertEqual((filtered, code, count), (own, 42, 1))


if __name__ == "__main__":
    unittest.main()
