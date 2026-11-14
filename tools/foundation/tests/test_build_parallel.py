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
                     patch.object(dev, "_node_npm") as desktop, \
                     patch.object(dev, "run_logged", side_effect=[{"exit_code": 0}, {"exit_code": 7}]) as run, \
                     patch.object(dev, "_save_record"), patch.object(dev, "_print_record"):
                    args = argparse.Namespace(config="Release", headless=True, json=False)
                    self.assertEqual(dev.command_build(args), 7)
                desktop.assert_not_called()
                self.assertFalse(any('Qt' in arg or 'WIRESCAPE_BUILD_UI' in arg for arg in run.call_args_list[0].args[0]))
                command = run.call_args_list[1].args[0]
                expected = ["--parallel", limit] if limit else ["--parallel"]
                self.assertEqual(command[command.index("--parallel"):], expected)

    def test_build_defaults_to_electron_and_headless_keeps_native_only(self):
        parser = dev.make_parser()
        self.assertFalse(parser.parse_args(["build"]).headless)
        self.assertTrue(parser.parse_args(["build", "--headless"]).headless)

    def test_desktop_build_uses_node_npm_and_requires_actual_artifacts(self):
        for complete in (True, False):
            with self.subTest(complete=complete), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                (root/'native').mkdir()
                (root/'native/CMakeLists.txt').touch()
                build = root/'build'
                def run(command, cwd, log):
                    if command[0] == 'node' and complete:
                        files = ['apps/desktop/build/main.js', 'apps/desktop/build/preload.js', 'apps/studio/dist/index.html',
                                 'apps/desktop/node_modules/electron/dist/electron', 'apps/desktop/node_modules/electron/path.txt']
                        for relative in files:
                            file = root/relative
                            file.parent.mkdir(parents=True, exist_ok=True)
                            file.write_text('electron' if file.name == 'path.txt' else 'compiled')
                        engine = build/'sidecar'/('wirescape-engine.exe' if os.name == 'nt' else 'wirescape-engine')
                        engine.parent.mkdir(parents=True)
                        engine.write_bytes(b'engine')
                    return {'exit_code': 0, 'log': str(log)}
                with patch.object(dev, 'ROOT', root), patch.object(dev, 'BUILD_DIR', build), \
                     patch.object(dev, 'REPORT_DIR', root/'reports'), patch.object(dev, '_print_record'), \
                     patch.object(dev, '_cmake_and_generator', return_value=('cmake', 'Ninja', 'x64')), \
                     patch.object(dev, '_node_npm', return_value=('node', 'npm-cli.js')) as npm, \
                     patch.object(dev, 'run_logged', side_effect=run) as runner:
                    self.assertEqual(dev.command_build(dev.make_parser().parse_args(['build'])), 0 if complete else 1)
                    npm.assert_called_once()
                    self.assertEqual(runner.call_args_list[2].args[0], ['node', 'npm-cli.js', 'run', 'build'])
                    self.assertEqual(runner.call_args_list[2].args[1], root/'apps/desktop')
                    self.assertIs(dev._build_metadata()['desktop_verified'], complete)


if __name__ == '__main__':
    unittest.main()
