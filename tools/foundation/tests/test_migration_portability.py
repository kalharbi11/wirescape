# SPDX-License-Identifier: GPL-3.0-only
"""Node/npm layout selection without running a shell or loading a runtime."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev


class MigrationPortabilityTests(unittest.TestCase):
    def test_resolved_npm_cli_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            cli = root/'npm/npm-cli.js'
            cli.parent.mkdir()
            cli.touch()
            node = str(root/'node')
            with patch.object(dev.shutil, 'which', side_effect=lambda name: node if name == 'node' else str(root/'npm/../npm/npm-cli.js')):
                self.assertEqual(dev._node_npm(), (node, str(cli.resolve())))

    def test_windows_adjacent_npm_cli_with_spaces_and_shims(self):
        for location in ('node', 'npm'):
            with self.subTest(location=location), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                node, npm = root/'Node With Spaces/node.exe', root/'npm shims/npm.cmd'
                for file in (node, npm):
                    file.parent.mkdir(parents=True)
                    file.touch()
                cli = (node if location == 'node' else npm).parent/'node_modules/npm/bin/npm-cli.js'
                cli.parent.mkdir(parents=True)
                cli.touch()
                with patch.object(dev.shutil, 'which', side_effect=lambda name: str(node if name == 'node' else npm)):
                    self.assertEqual(dev._node_npm(), (str(node), str(cli)))

    def test_missing_cli_is_not_replaced_by_an_unrelated_wrapper(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            wrapper = root/'npm.cmd'
            wrapper.touch()
            with patch.object(dev.shutil, 'which', side_effect=lambda name: str(root/'node.exe' if name == 'node' else wrapper)):
                self.assertEqual(dev._node_npm(), (str(root/'node.exe'), None))


if __name__ == '__main__':
    unittest.main()
