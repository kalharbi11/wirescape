# SPDX-License-Identifier: GPL-3.0-only
"""Device-off test: execute the real main-verifier shell with a fake GitHub CLI."""
import os
from pathlib import Path
import re
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
        with tempfile.TemporaryDirectory() as tmp:
            gh = Path(tmp) / 'gh'
            gh.write_text("""#!/usr/bin/env python3
import os, sys
from pathlib import Path
a = sys.argv[1:]
if a[0] == 'api':
    print('fixed-main-sha')
elif a[:2] == ['run', 'list']:
    name = a[a.index('--workflow') + 1]
    print(int(name in os.environ['STUB_EXISTING'].split(',')))
elif a[:2] == ['workflow', 'run']:
    with Path(os.environ['STUB_LOG']).open('a') as out:
        out.write(a[2] + '\\n')
else:
    sys.exit(1)
""")
            gh.chmod(0o755)
            log = Path(tmp) / 'dispatches'
            env = {**os.environ, 'PATH': tmp + os.pathsep + os.environ['PATH'],
                   'GH_REPO': 'example/repo', 'STUB_LOG': str(log)}
            for existing, wanted in (
                ('ci.yml,web.yml,web-demo.yml', []),
                ('ci.yml,web-demo.yml', ['web.yml']),
                ('', ['ci.yml', 'web.yml', 'web-demo.yml']),
            ):
                log.unlink(missing_ok=True)
                env['STUB_EXISTING'] = existing
                result = subprocess.run(['bash', '-e', '-c', script], env=env,
                                        capture_output=True, text=True, timeout=15)
                self.assertEqual(result.returncode, 0, result.stderr)
                actual = log.read_text().splitlines() if log.exists() else []
                self.assertEqual(actual, wanted)


if __name__ == '__main__':
    unittest.main()
