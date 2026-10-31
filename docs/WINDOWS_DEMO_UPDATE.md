<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Update the Windows demo to an accepted build

This guide rebuilds and packages one exact source revision, checks that the packaged app reaches its normal window with audio off, and points the existing Wirescape shortcuts at that package. It does not install or publish the app. A package check is not evidence of deployment or clean-machine compatibility.

## What is in the recorded source

At the audit on 2026-10-30, `origin/main` resolved to `e71ea75da7826bce4f6a73317a53396291eb78af`. This is the merge commit for PR #149. Use the exact SHA accepted for the demo; the value below is a placeholder, not a recommendation to use a different or newer revision.

| Feature | State at audit | Evidence |
|---|---|---|
| Fullscreen/minimize toolbar and `--fullscreen` | Merged in #149; present at recorded main SHA `e71ea75da7826bce4f6a73317a53396291eb78af` | PR #149 merged 2026-10-30; head `65877e3c4b507fb94f74871be0a945eb012287e3`, merge commit `e71ea75da7826bce4f6a73317a53396291eb78af` |
| MIDI input and MIDI-to-CV | Pending | PR #81 open; head `6ca0094cde2bde45829b244696b4396c535966c9` |
| Two-instance CHAOS runner fixture | Pending | PR #124 open; head `bd55cf54d2606a5e6b71d37433680e55860e6b2e` |
| Sidebar footer and cable curvature layout | Open, unmerged | PR #145 open; head `843980727953bb8a81b01b6b1725e54a050fb9a9` |
| Module control hierarchy and rotary UI | Open, unmerged | PR #147 open; head `92395e0400f8f78bdfbb344bcf7a7c1c1c0b176f` |
| Studio Clock Start/Stop toggle | Pending | PR #153 open; head `02860ff44748594a4611caaa39ae51b0fc55f263` |

The table records repository state, not a Windows/Pi deployment guarantee. Confirm the accepted SHA and its feature set before building. Open or pending PR branches are not included in a build from `main` unless their changes have been merged into the selected SHA. The Qt Quick interface is compiled into the executable resources, so changing QML requires a rebuild. `python tools/dev.py launch` only launches the existing build output; it does not rebuild it.

## Build, package and check one revision

Use PowerShell from the repository checkout. Set the accepted commit to the full SHA approved for the demo and set `WIRESCAPE_QT_DIR` to the Qt 6.10 MSVC x64 SDK already used by this checkout. No Qt location is assumed here.

```powershell
$AcceptedSha = '<full accepted commit SHA>'
$env:WIRESCAPE_QT_DIR = '<existing Qt 6.10.3 msvc2022_64 SDK directory>'

# Start in the repository root. Do not switch with uncommitted checkout changes.
if (git status --porcelain) { throw 'Save or commit checkout changes before switching revisions.' }
git fetch origin
if ($LASTEXITCODE -ne 0) { throw 'Fetch failed.' }
git switch --detach $AcceptedSha
if ($LASTEXITCODE -ne 0) { throw 'Checkout failed.' }
if ((git rev-parse HEAD).Trim() -ne $AcceptedSha) { throw 'Checkout does not match the accepted SHA.' }

python tools/dev.py doctor
if ($LASTEXITCODE -ne 0) { throw 'Resolve the reported prerequisites first.' }
python tools/dev.py build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed; do not package an older executable.' }

# Capture the package's actual reported path; do not guess or select a directory by timestamp.
$PackageResult = python tools/dev.py package --config Release --json | ConvertFrom-Json
if ($PackageResult.status -ne 'passed') { throw "Package failed; inspect report: $($PackageResult.summary)" }
$PackagePath = $PackageResult.package_directory
if (-not $PackagePath -or -not (Test-Path (Join-Path $PackagePath 'wirescape_studio.exe'))) {
    throw 'The reported package directory does not contain Wirescape Studio.'
}

# Self-closing normal-window readiness check. Audio remains off.
python tools/dev.py startup-test --config Release --portable "$PackagePath"
if ($LASTEXITCODE -ne 0) { throw 'Startup check failed; do not update shortcuts.' }
```

`build` configures and builds under `build/native`. `package` creates a new timestamped folder under `build/packages` and runs its isolated-path smoke, normal-window startup, and package inventory checks. The report's `package_directory` is the path used below. Keep the command output/report with the accepted SHA so the package can be identified later.

## Point the existing shortcuts at this package

After the package and startup checks pass, rerun the repository shortcut helper with the reported path:

```powershell
& .\tools\install_windows_shortcuts.ps1 -Package "$PackagePath"
```

The helper sets Desktop and Programs shortcuts to this package's `wirescape_studio.exe`, checks any existing shortcut targets before overwriting, and verifies the saved target paths. It accepts a package directory, not an executable path. Do not delete the previous package directory: to return to it, run the same helper with that previous package path. Saved patch files remain at the locations where you saved them; the package update does not migrate or delete those files.

## User-only playback check

The startup check confirms a normal window opens and audio has not started. It does not open devices or establish that sound works. At the Windows laptop, the user can then check audible USB output, keyboard/MIDI notes, Panic and device hotplug, CC control, and save/reopen of a patch. Record the actual device and result separately. Missing laptop access or screenshots is pending evidence, not a failed automated check.

## Explicit launcher runtime and update check (#188)

After building this revision, select existing local dependencies once:

```powershell
python tools/dev.py launch --qt-prefix '<Qt SDK>' --portaudio '<path to libportaudio64bit.dll>' --remember-runtime
python tools/dev.py updates --open-instructions
```

The launcher prints each validated path and its source. Saved selections live in
`%LOCALAPPDATA%/Wirescape/launcher.json`; explicit flags override them. An invalid
explicit or saved path produces an error instead of silently choosing another DLL.
The package command accepts the same options. Normal launch preserves the app's
interactive MIDI preference; it never adds probe or MIDI-confirmation flags.

`updates` explicitly reads latest `origin/main` with `git ls-remote`, displays the
installed build commit and checkout commit separately, and makes no local changes.
A different SHA does not prove the installed revision is behind. Use the accepted
revision procedure above; the command never pulls, switches or installs for you.
Older build metadata needs a fresh successful UI build to record its installed SHA.

Run the existing shortcut installer from the active checkout with the package
path returned by `dev.py package`. It rejects packages outside that checkout's
`build/packages`, missing/mismatched provenance, and changed executable digests.
The packaged Qt/PortAudio runtime is deployed by `package`; no SDK path is needed
when launching that verified package. Physical playback remains an owner check.
