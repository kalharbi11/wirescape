# SPDX-License-Identifier: GPL-3.0-only
"""Owner update script: ref pinning, fail-fast order, and device-free execution."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SHA = "1234567890abcdef1234567890abcdef12345678"


@unittest.skipIf(os.name == "nt", "Pi update script requires POSIX sh")
class UpdateAndBuildTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix="wirescape update space ")
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        (self.root / "tools/pi").mkdir(parents=True)
        shutil.copyfile(ROOT / "tools/pi/update-and-build.sh",
                        self.root / "tools/pi/update-and-build.sh")
        (self.root / "build/native").mkdir(parents=True)
        executable = self.root / "build/native/wirescape-engine"
        executable.write_text("#!/bin/sh\nexit 0\n")
        executable.chmod(0o755)
        for app in ("studio", "desktop"):
            (self.root / "apps" / app).mkdir(parents=True)

        self.bin = self.root / "fakebin"
        self.bin.mkdir()
        self.log = self.root / "commands.log"
        self.log.write_text("")
        self.add_fake("git", r"""#!/bin/sh
printf 'git %s\n' "$*" >> "$WS_LOG"
case " $* " in
  *" status --porcelain "*) [ "$WS_DIRTY" != 1 ] || echo ' M owner.txt' ;;
  *" rev-parse --verify "*) printf '%s\n' "$WS_SHA" ;;
  *" rev-parse HEAD "*) printf '%s\n' "$WS_SHA" ;;
esac
""")
        self.add_fake("python3", r"""#!/bin/sh
if [ "$1" = "-" ]; then shift; exec "$WS_REAL_PYTHON" - "$@"; fi
printf 'python3 %s\n' "$*" >> "$WS_LOG"
[ "$WS_FAIL_BUILD" != 1 ]
""")
        self.add_fake("npm", r"""#!/bin/sh
printf 'npm %s : %s\n' "$PWD" "$*" >> "$WS_LOG"
case "$*" in
  'run build')
    if [ "$WS_FAIL_NPM" = 1 ]; then exit 1; fi
    case "$PWD" in
      */apps/studio) mkdir -p dist; touch dist/index.html ;;
      */apps/desktop) mkdir -p build; touch build/main.js ;;
    esac ;;
esac
""")
        self.env = {
            **os.environ,
            "PATH": str(self.bin) + os.pathsep + os.environ.get("PATH", ""),
            "WS_LOG": str(self.log),
            "WS_SHA": SHA,
            "WS_REAL_PYTHON": sys.executable,
        }

    def add_fake(self, name, body):
        path = self.bin / name
        path.write_text(body)
        path.chmod(0o755)

    def run_update(self, *args, **overrides):
        return subprocess.run(
            ["sh", str(self.root / "tools/pi/update-and-build.sh"), *args],
            env={**self.env, **overrides}, capture_output=True, text=True,
            timeout=15,
        )

    def test_exact_revision_release_order_and_safe_owner_launcher(self):
        done = self.run_update("origin/main")
        self.assertEqual(done.returncode, 0, done.stderr)
        calls = self.log.read_text().splitlines()
        fetch = next(i for i, line in enumerate(calls) if "fetch --prune origin" in line)
        checkout = next(i for i, line in enumerate(calls) if "switch --detach" in line)
        build = next(i for i, line in enumerate(calls) if "dev.py build" in line)
        npm = next(i for i, line in enumerate(calls) if line.startswith("npm "))
        self.assertLess(fetch, checkout)
        self.assertLess(checkout, build)
        self.assertLess(build, npm)
        self.assertIn("tools/dev.py build --headless --config Release", calls[build])
        self.assertEqual(sum("npm ci" in line for line in calls), 2)
        self.assertEqual(sum("run build" in line for line in calls), 2)
        self.assertIn(SHA, done.stdout)
        self.assertIn("--confirm-audio --confirm-midi", done.stdout)
        self.assertIn(str(self.root), done.stdout)
        self.assertNotIn("--null-audio", done.stdout)

    def test_dirty_checkout_fails_before_fetch_or_build(self):
        done = self.run_update("main", WS_DIRTY="1")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn("not clean", done.stderr)
        self.assertNotIn("fetch ", self.log.read_text())
        self.assertNotIn("dev.py build", self.log.read_text())

    def test_build_failure_never_runs_npm_or_claims_success(self):
        done = self.run_update(SHA, WS_FAIL_BUILD="1")
        self.assertNotEqual(done.returncode, 0)
        self.assertNotIn("npm ", self.log.read_text())
        self.assertNotIn("To launch", done.stdout)

    def test_invalid_ref_and_arguments_never_call_git(self):
        for args in ((), ("--help",), ("main..other",), ("main", "extra")):
            self.log.write_text("")
            done = self.run_update(*args)
            self.assertEqual(done.returncode, 2, done.stderr)
            self.assertEqual(self.log.read_text(), "")


if __name__ == "__main__":
    unittest.main()
