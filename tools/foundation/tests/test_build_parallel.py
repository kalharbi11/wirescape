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
                     patch.object(dev, "_base_record", return_value={"source": {}}), \
                     patch.object(dev, "_cmake_and_generator", return_value=("cmake", "Ninja", "x64")), \
                     patch.object(dev, "_qt_prefix", return_value=None), \
                     patch.object(dev, "run_logged", side_effect=[{"exit_code": 0}, {"exit_code": 7}]) as run, \
                     patch.object(dev, "_save_record"), patch.object(dev, "_print_record"):
                    args = argparse.Namespace(config="Release", qt_prefix=None, headless=True, json=False)
                    self.assertEqual(dev.command_build(args), 7)
                command = run.call_args_list[1].args[0]
                expected = ["--parallel", limit] if limit else ["--parallel"]
                self.assertEqual(command[command.index("--parallel"):], expected)

