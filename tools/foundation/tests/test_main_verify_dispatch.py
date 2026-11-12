# SPDX-License-Identifier: GPL-3.0-only
"""Device-off test: execute the real main-verifier shell with a fake GitHub CLI."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[3]


class MainVerifyTests(unittest.TestCase):
    def test_existing_runs_are_not_redispatched(self):
        workflow = (ROOT / '.github/workflows/main_verify.yml').read_text()
        match = re.search(r'(?m)^        run: \|\n((?:^          .*\n)+)', workflow)
        self.assertIsNotNone(match)
        script = textwrap.dedent(match.group(1))
        bash = str(Path(os.environ['ProgramFiles']) / 'Git/bin/bash.exe') if os.name == 'nt' else 'bash'
        with tempfile.TemporaryDirectory() as tmp:
            gh = Path(tmp) / 'gh'
            gh.write_text("""#!/bin/sh
case "$1 $2" in
    api*) if [ "$STUB_EXISTING" = advanced ] && [ -f "$STUB_LOG" ]; then echo advanced-main; else
        touch "$STUB_LOG"; echo fixed-main-sha; fi ;;
    'run list')
        while [ "$1" != --workflow ]; do shift; done
        case ",$STUB_EXISTING," in *,"$2",*) echo 1 ;; *) echo 0 ;; esac ;;
    'workflow run') printf '%s\\n' "$3" >> "$STUB_LOG" ;;
    *) exit 1 ;;
esac
""")
            gh.chmod(0o755)
            log = Path(tmp) / 'dispatches'
            env = {**os.environ, 'STUB_LOG': log.as_posix(), 'GH_REPO': 'example/repo'}
            prefix = 'export PATH=' + shlex.quote(Path(tmp).as_posix()) + ':"$PATH"\n'
            probe = 'case "$(command -v gh)" in */' + Path(tmp).name + '/gh) ;; *) exit 90 ;; esac\n'
            for existing, wanted in (
                ('ci.yml,web.yml,web-demo.yml', []),
                ('ci.yml,web-demo.yml', ['web.yml']),
                ('', ['ci.yml', 'web.yml', 'web-demo.yml']),
                ('advanced', []),
            ):
                log.unlink(missing_ok=True)
                env['STUB_EXISTING'] = existing
                result = subprocess.run([bash, '-e', '-c', prefix + probe + script], env=env,
                                        capture_output=True, text=True, timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                actual = log.read_text().splitlines() if log.exists() else []
                self.assertEqual(actual, wanted)


if __name__ == '__main__':
    unittest.main()
