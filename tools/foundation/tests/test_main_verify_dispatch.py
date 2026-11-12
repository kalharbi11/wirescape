# SPDX-License-Identifier: GPL-3.0-only
"""Device-off: run the real main-verifier shell with fake GitHub and Git CLIs."""
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
    def test_dispatch_matches_main_sha_and_workflow_paths(self):
        workflow = (ROOT / '.github/workflows/main_verify.yml').read_text()
        match = re.search(r'(?m)^        run: \|\n((?:^          .*\n)+)', workflow)
        self.assertIsNotNone(match)
        script = textwrap.dedent(match.group(1))
        bash = str(Path(os.environ['ProgramFiles']) / 'Git/bin/bash.exe') if os.name == 'nt' else 'bash'
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            posix = (subprocess.check_output([bash, '-c', 'cygpath -u "$1"', '_', tmp], text=True).strip()
                     if os.name == 'nt' else folder.as_posix())
            (folder / 'gh').write_text("""#!/bin/sh
printf x >> "$STUB_HITS"
case "$1 $2" in
  api*) if [ "$STUB_EXISTING" = advanced ] && [ -f "$STUB_LOG" ]; then echo advanced-main; else
      touch "$STUB_LOG"; echo fixed-main-sha; fi ;;
  'run list')
      while [ "$1" != --workflow ]; do shift; done
      case ",$STUB_EXISTING," in
        *,"$2=cancelled-twice",*) echo cancelled,cancelled ;;
        *,"$2=cancelled",*) echo cancelled ;;
        *,"$2=failed",*) echo failure ;;
        *,"$2",*) echo success ;;
        *) echo "" ;;
      esac ;;
  'workflow run') printf '%s\n' "$3" >> "$STUB_LOG" ;;
  *) exit 1 ;;
esac
""", newline='\n')
            (folder / 'git').write_text("""#!/bin/sh
case "$1" in
  rev-parse) echo fixed-main-sha ;;
  diff) printf '%s\n' "$STUB_FILES" ;;
  *) exit 1 ;;
esac
""", newline='\n')
            for stub in ('gh', 'git'):
                (folder / stub).chmod(0o755)
            log, hits = folder / 'dispatches', folder / 'invocations'
            env = {**os.environ, 'STUB_LOG': posix + '/dispatches',
                   'STUB_HITS': posix + '/invocations', 'GH_REPO': 'example/repo'}
            prefix = 'export PATH=' + shlex.quote(posix) + ':"$PATH"\n'
            cases = (
                ('ci.yml,web.yml,web-demo.yml', 'apps/studio/src/a.ts', []),
                ('ci.yml,web-demo.yml', 'apps/studio/src/a.ts', ['web.yml']),
                ('', 'apps/studio/src/a.ts', ['ci.yml', 'web.yml', 'web-demo.yml']),
                ('', 'native/sidecar/session.cpp', ['ci.yml']),
                ('ci.yml=cancelled', 'native/sidecar/session.cpp', ['ci.yml']),
                ('ci.yml=cancelled-twice', 'native/sidecar/session.cpp', []),
                ('ci.yml=failed', 'native/sidecar/session.cpp', []),
                ('advanced', 'native/sidecar/session.cpp', []),
            )
            for existing, files, wanted in cases:
                with self.subTest(existing=existing, files=files):
                    log.unlink(missing_ok=True)
                    hits.unlink(missing_ok=True)
                    env.update(STUB_EXISTING=existing, STUB_FILES=files)
                    result = subprocess.run([bash, '-e', '-c', prefix + script], cwd=ROOT,
                                            env=env, capture_output=True, text=True, timeout=20)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertTrue(hits.exists(), 'fake gh was not invoked')
                    actual = log.read_text().splitlines() if log.exists() else []
                    self.assertEqual(actual, wanted)


if __name__ == '__main__':
    unittest.main()
