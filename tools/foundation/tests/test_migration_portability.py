# SPDX-License-Identifier: GPL-3.0-only
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import dev

class MigrationPortabilityTests(unittest.TestCase):
    def test_qt_environment_priority_and_exact_fallback(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            paths = [root/'environment', root/'prefix', root/'.tools/qt/6.10.3/msvc2022_64']
            for path in paths:
                config = path/'lib/cmake/Qt6/Qt6Config.cmake'
                config.parent.mkdir(parents=True)
                config.touch()
            with patch.object(dev, 'ROOT', root), patch.dict(os.environ, {'WIRESCAPE_QT_DIR':str(paths[0]), 'CMAKE_PREFIX_PATH':str(paths[1])}):
                self.assertEqual(dev._qt_prefix(None), paths[0])
                self.assertIsNone(dev._qt_prefix(str(root/'missing')))
                os.environ['WIRESCAPE_QT_DIR']=''
                self.assertEqual(dev._qt_prefix(None), paths[1])
                os.environ['CMAKE_PREFIX_PATH']=''
                self.assertEqual(dev._qt_prefix(None), paths[2])

if __name__ == '__main__':
    unittest.main()
