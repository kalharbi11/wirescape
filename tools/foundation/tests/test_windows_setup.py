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
LAUNCHER = (Path(__file__).resolve().parents[2] / "windows" / "Wirescape.cmd").read_text(
    encoding="utf-8"
)
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
        self.assertIn("--no-optional-locks -C $root status --porcelain", SCRIPT)
        self.assertIn("CMAKE_HOME_DIRECTORY:INTERNAL=", SCRIPT)
        self.assertIn("WIRESCAPE_BUILD_UI:BOOL=OFF", SCRIPT)
        self.assertIn("if ($Ref -notmatch", SCRIPT)

    def test_pinned_x64_portaudio_and_fail_closed_native_suite(self):
        self.assertIn("147dd722548358763a8b649b3e4b41dfffbcfbb6", SCRIPT)
        self.assertIn("0x8664", SCRIPT)
        self.assertIn("Get-FileHash", SCRIPT)
        self.assertIn("sidecar_golden", SCRIPT)
        self.assertIn("(Skipped|Not Run)", SCRIPT)

    def test_no_assignment_to_powershell_automatic_variables(self):
        # Owner report: `$home = ...` is the read-only $HOME (names are
        # case-insensitive) and aborted re-runs with an existing CMake cache.
        import re
        automatic = ("home", "host", "input", "args", "error", "pid", "pwd", "true",
                     "false", "null", "this", "matches", "psitem", "_", "shellid",
                     "executioncontext", "myinvocation", "pscmdlet", "psscriptroot",
                     "pscommandpath", "lastexitcode", "foreach", "switch", "profile")
        for script in (SCRIPT,):
            for match in re.finditer(r"\$([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=)", script):
                self.assertNotIn(match.group(1).lower(), automatic, match.group(0))

    def test_no_native_command_piped_into_select_first(self):
        # Owner report 2026-11-12: Windows PowerShell 5.1 stops the pipeline at -First, kills the
        # native process and sets $LASTEXITCODE to -1, so a present tool reads as "missing".
        import re
        for script in (SCRIPT,):
            for line in script.splitlines():
                if line.lstrip().startswith("#"):
                    continue
                self.assertIsNone(re.search(r"&\s*\S+[^|]*\|\s*Select-Object\s+-First", line, re.I), line)

    def test_pinned_cmake_is_preferred_without_files_changing(self):
        self.assertIn("function Use-PinnedCMake", SCRIPT)
        self.assertIn("'cmake version 4.2.3'", SCRIPT)
        self.assertIn("sysconfig.get_path('scripts','$kind')", SCRIPT)
        prefix = SCRIPT.split("if ($CheckOnly)", 1)[0]
        self.assertIn("Use-PinnedCMake", prefix.split("function Preflight", 1)[1])

    def test_launcher_starts_electron_exe_not_the_npm_shim(self):
        # Owner install report: start "" ".bin\electron.cmd" broke cmd quoting.
        self.assertIn("node_modules\\electron\\dist\\electron.exe", LAUNCHER)
        self.assertNotIn("electron.cmd", LAUNCHER)
        self.assertIn('if not exist "%ELECTRON%"', LAUNCHER)
        self.assertIn("--confirm-audio --confirm-midi %*", LAUNCHER)

    def test_asio_sdk_is_pinned_checked_and_optional(self):
        # Owner v1 request: ASIO drivers (FlexASIO, ASIO4ALL, interface drivers).
        self.assertIn("ASIO-SDK_2.3.4_2025-10-15.zip", SCRIPT)
        self.assertIn("D5EBF0C20DD2C5F43771FD0C1418F4B361BF52434EE670097CFA6B3A335E2ECA", SCRIPT)
        self.assertIn("'-DPA_USE_ASIO=ON'", SCRIPT)
        self.assertIn("[switch]$NoAsio", SCRIPT)
        self.assertIn("'.tools/windows-setup/asiosdk'", SCRIPT)
        download = SCRIPT.index("Invoke-WebRequest")
        self.assertLess(SCRIPT.index("if ($CheckOnly)"), download)
        self.assertLess(download, SCRIPT.index("$asioSha) {"))

    def test_no_device_consent_or_unsafe_install(self):
        self.assertIn("--null-audio", SMOKE)
        for token in ("--confirm-audio", "--confirm-midi", "winget install",
                      "SetEnvironmentVariable", "Remove-Item -Recurse",
                      "git reset --hard", "git clean -fd"):
            self.assertNotIn(token, SCRIPT + SMOKE)
        self.assertIn("app.setPath(key, dir)", SMOKE)
        self.assertIn("['userData','user']", SMOKE)
        self.assertIn("['sessionData','session']", SMOKE)

    def test_preflight_reports_all_tool_versions_before_failing(self):
        # One diagnostic table, not the old first-missing-tool exception.
        self.assertIn('Format-Table Tool, Found, Required, OK', SCRIPT)
        self.assertIn('Preflight failed ($($failures.Count) checks)', SCRIPT)
        for tool in ('Process', 'Visual Studio', 'cl.exe', 'Windows SDK', 'rc.exe',
                     'Python', 'CMake', 'Ninja', 'Node', 'npm'):
            self.assertIn("CheckRow '" + tool + "'", SCRIPT)
        self.assertIn("Node", SCRIPT)
        self.assertIn("22.x x64", SCRIPT)
        self.assertIn("4.2.3", SCRIPT)
        self.assertIn("1.13.0", SCRIPT)
        self.assertIn("elevated", SCRIPT.lower())

    def test_exact_pinned_ninja_and_kitware_wheel_suffix(self):
        import re
        source = re.search(r"function Test-PinnedNinja\(\[string\]\$v\)\s*\{\s*return \$v -match '([^']+)'\s*\}", SCRIPT)
        self.assertIsNotNone(source, "one shared PowerShell version predicate")
        pattern = source.group(1)
        self.assertEqual(pattern, r'^1\.13\.0(\.git\.kitware\.jobserver-pipe-\d+)?$')
        for value in ("1.13.0", "1.13.0.git.kitware.jobserver-pipe-1"):
            with self.subTest(positive=value):
                self.assertRegex(value, pattern)
        for value in ("missing", "1.12.1", "1.14.0", "1.13.01", "1.13.0-rc1", "1.13.0.git.other"):
            with self.subTest(negative=value):
                self.assertNotRegex(value, pattern)
        self.assertEqual(SCRIPT.count("Test-PinnedNinja $ninjaVersion"), 2)

    def test_process_local_x64_shell_and_pip_tool_recovery(self):
        self.assertIn("Microsoft.VisualStudio.DevShell.dll", SCRIPT)
        self.assertIn("Enter-VsDevShell", SCRIPT)
        self.assertIn("-arch=x64 -host_arch=x64", SCRIPT)
        self.assertIn("sysconfig.get_path('scripts','nt_user')", SCRIPT)
        self.assertIn('$env:Path = "$scripts;$env:Path"', SCRIPT)
        self.assertIn("$env:npm_config_update_notifier = 'false'", SCRIPT)
        # None of these may globally install tools or edit a user's PATH.
        self.assertNotIn("SetEnvironmentVariable", SCRIPT)
        self.assertNotIn("Install-Module", SCRIPT)

    def test_owner_launcher_smoke_is_device_off_and_bounded(self):
        self.assertIn('WIRESCAPE_SETUP_SMOKE', SCRIPT)
        self.assertIn('Wirescape.cmd', SCRIPT)
        self.assertIn('--null-audio', SCRIPT)
        self.assertIn('Start-Process -FilePath $env:ComSpec', SCRIPT)
        self.assertIn('Start-Sleep -Seconds 3', SCRIPT)
        self.assertIn('taskkill.exe /PID $launcherProcess.Id /T /F', SCRIPT)
        self.assertIn('if "%WIRESCAPE_SETUP_SMOKE%"=="1"', LAUNCHER)
        self.assertIn('exit /b %errorlevel%', LAUNCHER)
        self.assertIn('Write-Host "Summary: $summary"', SCRIPT)

    def test_launcher_smoke_isolated_and_existing_profile_preserved(self):
        # This is a source contract; the Windows job also runs the real smoke.
        self.assertIn("NewGuid().ToString('N')", SCRIPT)
        self.assertIn("$profileDir = Join-Path $smokeBase 'isolated-profile'", SCRIPT)
        self.assertIn("$existingProfile = Join-Path $smokeBase 'existing-profile'", SCRIPT)
        self.assertIn("Set-Content -LiteralPath $existingMarker", SCRIPT)
        self.assertIn(' --null-audio --profile-dir "{1}"', SCRIPT)
        self.assertIn("Wirescape.cmd --null-audio --profile-dir exited early", SCRIPT)
        for subdir in ("userData", "sessionData"):
            self.assertIn("'"+subdir+"'", SCRIPT)
        self.assertIn("[Convert]::ToBase64String($existingBefore)", SCRIPT)
        self.assertIn("[Convert]::ToBase64String($existingAfter)", SCRIPT)
        self.assertIn("Launcher smoke changed the existing profile sentinel", SCRIPT)
        self.assertIn("Remove-Item Env:WIRESCAPE_SETUP_SMOKE", SCRIPT)

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
                root.mkdir()
                old.mkdir()
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
