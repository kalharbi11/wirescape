# SPDX-License-Identifier: GPL-3.0-only
"""Safety-contract regressions for the Windows setup entrypoint (host-independent).

These static tests do not execute PowerShell or establish Windows acceptance.
Windows CI must also parse/run -CheckOnly and execute the native and Electron tests.
"""
from pathlib import Path
import base64
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = (Path(__file__).resolve().parents[2] / "windows" / "setup.ps1").read_text(
    encoding="utf-8"
)


SMOKE = (Path(__file__).resolve().parents[3] / "apps" / "desktop" / "tests" /
         "windows-setup.spec.cjs").read_text(encoding="utf-8")
SHELL = shutil.which("powershell.exe") or shutil.which("pwsh")


class WindowsSetupContract(unittest.TestCase):
    def test_check_only_is_before_any_mutating_stage(self):
        prefix = SCRIPT.split("if ($CheckOnly)", 1)[0]
        self.assertIn("Preflight", prefix)
        for action in ("New-Item", "Set-Content", "Out-File", "Copy-Item",
                       "Start-Process", "Run git @('clone'", "git fetch",
                       "git switch", "Remove-Item", "taskkill"):
            self.assertNotIn(action, prefix)

    def test_full_sha_and_clean_checkout_are_required(self):
        self.assertIn("^[0-9a-fA-F]{40}$", SCRIPT)
        self.assertIn("status --porcelain", SCRIPT)
        self.assertIn("CMAKE_HOME_DIRECTORY:INTERNAL=", SCRIPT)
        self.assertIn("WIRESCAPE_BUILD_UI:BOOL=OFF", SCRIPT)
        self.assertIn("if ($Ref -notmatch", SCRIPT)

    def test_pinned_x64_portaudio_and_fail_closed_native_suite(self):
        self.assertIn("147dd722548358763a8b649b3e4b41dfffbcfbb6", SCRIPT)
        self.assertIn("0x8664", SCRIPT)
        self.assertIn("Get-FileHash", SCRIPT)
        self.assertIn("sidecar_golden", SCRIPT)
        self.assertIn("(Skipped|Not Run)", SCRIPT)

    def test_no_device_consent_or_unsafe_install(self):
        self.assertIn("--null-audio", SMOKE)
        for token in ("--confirm-audio", "--confirm-midi", "winget install",
                      "SetEnvironmentVariable", "Remove-Item -Recurse",
                      "git reset --hard", "git clean -fd"):
            self.assertNotIn(token, SCRIPT + SMOKE)
        self.assertIn("app.setPath(key, dir)", SMOKE)
        self.assertIn("['userData','user']", SMOKE)
        self.assertIn("['sessionData','session']", SMOKE)

    def test_node_and_windows_preflight(self):
        self.assertIn("v22", SCRIPT)
        self.assertIn("3.12 True", SCRIPT)
        self.assertIn("Microsoft.VisualStudio.Component.VC.Tools.x86.x64", SCRIPT)
        self.assertIn("WindowsSdkDir", SCRIPT)


@unittest.skipUnless(SHELL, "Filesystem behavior requires PowerShell")
class WindowsOutputIsolation(unittest.TestCase):
    def run_guard(self, root, suffix=""):
        guard = SCRIPT.split("# BEGIN OUTPUT GUARD", 1)[1].split("# END OUTPUT GUARD", 1)[0]
        # The marker's explanatory parenthesis is a comment, not PowerShell code.
        guard = guard.split("\n", 1)[1]
        script = "$ErrorActionPreference='Stop'; $root='" + str(root).replace("'", "''") + "';\n" + guard + suffix
        encoded = base64.b64encode(script.encode("utf-16le")).decode("ascii")
        return subprocess.run([SHELL, "-NoProfile", "-EncodedCommand", encoded],
                              capture_output=True, text=True, timeout=20)

    def test_fresh_guard_writes_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = self.run_guard(root, "\nAssert-GeneratedOutputs")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(list(root.iterdir()), [])

    def test_redirected_outputs_and_ancestors_preserve_old_install(self):
        outputs = ["build", "build/native", "apps", "apps/studio/node_modules",
                   "apps/studio/dist", "apps/desktop/node_modules", "apps/desktop/build",
                   ".tools/windows-setup", "build_reports/windows-setup"]
        for relative in outputs:
            with self.subTest(relative=relative), tempfile.TemporaryDirectory() as directory:
                root, old = Path(directory) / "fresh", Path(directory) / "old-install"
                root.mkdir(); old.mkdir()
                sentinel = old / "saved-patch.json"
                sentinel.write_text("preserve", encoding="utf-8")
                redirected = root / relative
                redirected.parent.mkdir(parents=True, exist_ok=True)
                if sys.platform == "win32":
                    command = ["cmd.exe", "/c", "mklink", "/J", str(redirected), str(old)]
                    made = subprocess.run(command, capture_output=True, text=True, timeout=20)
                    self.assertEqual(made.returncode, 0, made.stderr)
                else:
                    redirected.symlink_to(old, target_is_directory=True)
                escaped = str(redirected / "must-not-write.txt").replace("'", "''")
                suffix = "\nAssert-GeneratedOutputs; Set-Content -LiteralPath '" + escaped + "' -Value 'bad'"
                result = self.run_guard(root, suffix)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Refusing redirected output path", result.stderr)
                self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")
                self.assertEqual([p.name for p in old.iterdir()], ["saved-patch.json"])
                # Remove only this test's link/junction, never its target.
                if sys.platform == "win32":
                    redirected.rmdir()
                else:
                    redirected.unlink()


if __name__ == "__main__":
    unittest.main()
