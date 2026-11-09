# SPDX-License-Identifier: GPL-3.0-only
import argparse
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev  # noqa: E402


class BuildParallelTests(unittest.TestCase):
    def test_build_honors_environment_parallel_limit_and_preserves_failure(self):
        for limit in (None, "", "1", "2"):
            with self.subTest(limit=limit), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                (root / "native").mkdir()
                (root / "native/CMakeLists.txt").touch()
                env = {} if limit is None else {"CMAKE_BUILD_PARALLEL_LEVEL": limit}
                with patch.dict(os.environ, env, clear=True), \
                     patch.object(dev, "ROOT", root), \
                     patch.object(dev, "BUILD_DIR", root / "build"), \
                     patch.object(dev, "_base_record", return_value={"source": {}}), \
                     patch.object(dev, "_cmake_and_generator", return_value=("cmake", "Ninja", "x64")), \
                     patch.object(dev, "_qt_prefix", return_value=None) as qt, \
                     patch.object(dev, "run_logged", side_effect=[{"exit_code": 0}, {"exit_code": 7}]) as run, \
                     patch.object(dev, "_save_record"), patch.object(dev, "_print_record"):
                    args = argparse.Namespace(config="Release", qt_prefix=None, headless=True, json=False)
                    self.assertEqual(dev.command_build(args), 7)
                qt.assert_not_called()
                self.assertIn("-DWIRESCAPE_BUILD_UI=OFF", run.call_args_list[0].args[0])
                command = run.call_args_list[1].args[0]
                expected = ["--parallel", limit] if limit else ["--parallel"]
                self.assertEqual(command[command.index("--parallel"):], expected)

    def test_build_defaults_to_qt_free_and_qt_requires_explicit_opt_in(self):
        parser = dev.make_parser()
        self.assertTrue(parser.parse_args(["build"]).headless)
        self.assertTrue(parser.parse_args(["build", "--headless"]).headless)
        self.assertFalse(parser.parse_args(["build", "--with-qt"]).headless)

    def test_explicit_qt_build_discovers_sdk_and_sets_on(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "native").mkdir()
            (root / "native/CMakeLists.txt").touch()
            with patch.object(dev, "ROOT", root), \
                 patch.object(dev, "BUILD_DIR", root / "build"), \
                 patch.object(dev, "_base_record", return_value={"source": {}}), \
                 patch.object(dev, "_cmake_and_generator", return_value=("cmake", "Ninja", "x64")), \
                 patch.object(dev, "_qt_prefix", return_value=root / "qt") as qt, \
                 patch.object(dev, "run_logged", return_value={"exit_code": 7}) as run, \
                 patch.object(dev, "_save_record"), patch.object(dev, "_print_record"):
                self.assertEqual(dev.command_build(dev.make_parser().parse_args(["build", "--with-qt"])), 7)
            qt.assert_called_once_with(None)
            configure = run.call_args.args[0]
            self.assertIn("-DWIRESCAPE_BUILD_UI=ON", configure)
            self.assertIn(f"-DCMAKE_PREFIX_PATH={root / 'qt'}", configure)
