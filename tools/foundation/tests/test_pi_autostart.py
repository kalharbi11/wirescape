# SPDX-License-Identifier: GPL-3.0-only
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / 'tools/pi/install-autostart.sh'

# The installer writes an XDG autostart entry for the Pi/Linux desktop; it has no Windows
# counterpart (Windows uses the Start-menu shortcut from P1F). Linux CI and the Pi run it.
@unittest.skipIf(os.name == 'nt', 'Pi/Linux XDG autostart installer; covered on Linux and the Pi')
class PiAutostartTest(unittest.TestCase):
    def test_install_is_idempotent_and_remove_keeps_shared_launcher(self):
        with tempfile.TemporaryDirectory(prefix='wirescape home ') as temp:
            home = Path(temp)
            launcher = home / '.local/bin/wirescape'
            launcher.parent.mkdir(parents=True)
            launcher.write_text('#!/bin/sh\nexec true\n')
            launcher.chmod(0o755)
            env = {**os.environ, 'HOME': temp}
            def run(*args):
                return subprocess.run(['sh', str(SCRIPT), *args], env=env,
                    check=True, text=True, capture_output=True)
            destination = home / '.config/autostart/wirescape.desktop'
            run()
            self.assertTrue(destination.is_file())
            content = destination.read_text()
            self.assertIn(f'Exec="{launcher}"', content)
            first_stat = destination.stat().st_mtime_ns
            run()
            self.assertEqual(destination.read_text(), content)
            self.assertEqual(destination.stat().st_mtime_ns, first_stat)
            run('--remove')
            self.assertFalse(destination.exists())
            self.assertTrue(launcher.exists())

if __name__ == '__main__':
    unittest.main()
