# SPDX-License-Identifier: GPL-3.0-only
"""Safety-contract regressions for the Windows setup entrypoint (host-independent).

These static tests do not execute PowerShell or establish Windows acceptance.
Windows CI must also parse/run -CheckOnly and execute the native and Electron tests.
"""
from pathlib import Path
import re
import unittest

SCRIPT = (Path(__file__).resolve().parents[2] / "windows" / "setup.ps1").read_text(
    encoding="utf-8"
)


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
        self.assertIn("--null-audio", SCRIPT)
        for token in ("--confirm-audio", "--confirm-midi", "winget install",
                      "SetEnvironmentVariable", "Remove-Item -Recurse",
                      "git reset --hard", "git clean -fd"):
            self.assertNotIn(token, SCRIPT)
        self.assertIn("app.setPath(key, dir)", SCRIPT)
        self.assertIn("['userData','user']", SCRIPT)
        self.assertIn("['sessionData','session']", SCRIPT)

    def test_node_and_windows_preflight(self):
        self.assertIn("v22", SCRIPT)
        self.assertIn("3.12 True", SCRIPT)
        self.assertIn("Microsoft.VisualStudio.Component.VC.Tools.x86.x64", SCRIPT)
        self.assertIn("WindowsSdkDir", SCRIPT)


if __name__ == "__main__":
    unittest.main()
