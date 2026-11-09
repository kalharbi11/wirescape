<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows: isolated Electron source build

**Status: BLOCKED, not yet a verified install recipe.** PR #439 at
`46ab2c5d1a30475349d8460176341d47c9419b38` failed both Windows
`abi2_sidecar_protocol_tests` and `sidecar_golden` in
[run 42558134759](https://github.com/kalharbi11/wirescape/actions/runs/42558134759).
Both processes exited 3 after a pipe watchdog timeout. A successful build,
Linux pass, or green task-acceptance job does not resolve this Windows failure.
The commands below are a reviewable source-build procedure for #436. Do not
start the owner's clean-install experiment until a tested full source SHA is
recorded here after #439's Windows tests, required checks and Keel T2 review.
No accepted Windows SHA is currently supplied.

This is the Electron + Studio + Qt-free engine path. There is no certified
Windows binary installer in this procedure. The older `dev.py launch/package`
and Qt instructions are separate legacy flows. Do not use them here.

## Preserve the previous installation

Close Wirescape. Keep the old checkout, patches, settings and shortcuts as
backups. If you already moved the checkout, leave it there. Do not delete or
rename `%APPDATA%`, uninstall Qt, or alter User/Machine environment variables.
The fresh target is `$HOME\Documents\code\wirescape`. Use an empty target;
never build over the old checkout or copy its DLLs, `.tools`, cache or modules.

Use a new **Developer PowerShell for VS 2022**, as a normal user. The process
changes below disappear when this shell closes. They do not change your saved
PATH. Keep this shell open for subsequent sections.

## Prerequisites

Windows x64; Git with GitHub SSH access; Python 3.12 x64; CMake 4.2.3;
Visual Studio 2022 Build Tools with the C++ x64 workload and Windows SDK;
Node.js 22 LTS x64 and npm. This procedure uses the Visual Studio generator,
so Ninja is not required. Do not silently substitute a different architecture.
Allow several GB for the checkout, C++ outputs, npm dependencies and Electron.

Install missing tools from their official installers, or explicitly opt into
these example winget commands **only for the tools you need**:

```powershell
winget install --id Git.Git -e
winget install --id Python.Python.3.12 -e
winget install --id Kitware.CMake -e --version 4.2.3
winget install --id OpenJS.NodeJS.22 -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
```

Check each installer exit code before continuing. Some installers require
confirmation, elevation or a restart. If winget/the specified version is
unavailable, use the official vendor installer and verify the resulting version;
do not continue with a partially installed toolchain. Reopen Developer
PowerShell after installing tools. Prerequisite installation can change the
machine; it is an explicit owner action, separate from process-only isolation.

## Prepare the shell and clone

```powershell
$ErrorActionPreference = 'Stop'
function Check-Exit([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed (exit $LASTEXITCODE). Stop and inspect the output." }
}
foreach ($v in 'WIRESCAPE_QT_DIR','WIRESCAPE_PORTAUDIO','CMAKE_PREFIX_PATH',
               'QT_PLUGIN_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM_PLUGIN_PATH',
               'ELECTRON_RUN_AS_NODE','WIRESCAPE_TEST_STUB') {
    Remove-Item "Env:$v" -ErrorAction SilentlyContinue
}
# Only this process: quarantine known old checkout/Qt PATH entries.
$env:Path = (($env:Path -split ';') | Where-Object {
    $_ -and $_ -notmatch '(?i)(^|[\\/])Qt([\\/]|$)|wirescape'
}) -join ';'

if (-not [Environment]::Is64BitProcess -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
    throw 'Use x64 Developer PowerShell on Windows x64.'
}
git --version; Check-Exit 'Git'
py -3.12 -c 'import sys; assert sys.version_info[:2] == (3,12); assert sys.maxsize > 2**32; print(sys.version)'
Check-Exit 'Python 3.12 x64'
cmake --version; Check-Exit 'CMake'
$NodeVersion = node --version; Check-Exit 'Node version'
$NodeArch = node -p process.arch; Check-Exit 'Node architecture'
if ($NodeVersion -notmatch '^v22\.' -or $NodeArch -ne 'x64') { throw 'Install Node 22 LTS x64.' }
npm --version; Check-Exit 'npm'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'Install VS 2022 C++ Build Tools and Windows SDK.' }
$vs = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Check-Exit 'VS 2022 C++ detection'
if (-not $vs) { throw 'VS 2022 x64 C++ workload missing.' }
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue) -or -not $env:WindowsSdkDir) {
    throw 'Open Developer PowerShell for VS 2022 with x64 tools and Windows SDK.'
}
# GitHub returns exit 1 on successful SSH authentication (it provides no shell).
ssh -T git@github.com
# Continue only if this prints your expected GitHub account and successful authentication.
# If not, configure your GitHub SSH key before cloning; never paste private keys into chat.

$Root = Join-Path $HOME 'Documents\code\wirescape'
if ((Test-Path $Root) -and (Get-ChildItem -Force $Root | Select-Object -First 1)) {
    throw 'Fresh target is not empty. Preserve it and choose an empty directory.'
}
New-Item -ItemType Directory -Force (Split-Path $Root) | Out-Null
git clone git@github.com:kalharbi11/wirescape.git $Root
Check-Exit 'Clone'
Set-Location $Root
$AcceptedSha = '<pending: full 40-character SHA after Windows validation and review>'
if ($AcceptedSha -notmatch '^[0-9a-f]{40}$') { throw 'No accepted Windows revision yet. Stop here.' }
git fetch origin; Check-Exit 'Fetch'
git switch --detach $AcceptedSha; Check-Exit 'Checkout accepted revision'
$ActualSha = (git rev-parse HEAD).Trim(); Check-Exit 'Resolve source SHA'
if ($ActualSha -ne $AcceptedSha) { throw 'Wrong source revision.' }
$Dirty = git status --porcelain; Check-Exit 'Check source cleanliness'
if ($Dirty) { throw 'Save source changes before building.' }
New-Item -ItemType Directory -Force '.tools','build_reports\windows-fresh' | Out-Null
$Log = Join-Path $Root 'build_reports\windows-fresh'
Start-Transcript -Path (Join-Path $Log 'session.log') -Append
$ActualSha | Set-Content (Join-Path $Log 'source-sha.txt')
$env:npm_config_cache = Join-Path $Root '.tools\npm-cache'
$env:ELECTRON_CACHE = Join-Path $Root '.tools\electron-cache'
```

A clone/download failure stops the procedure. Retry a transient download only
after checking the error. Do not disable TLS, bypass source verification, or
fall back to the previous checkout. The transcript starts after SSH setup;
keep logs local and redact account paths before sharing them.

## Build fresh PortAudio, then the engine

PortAudio v19.7.0, MIT licence, exact source commit used by repository CI:
`147dd722548358763a8b649b3e4b41dfffbcfbb6`. Build it locally, with no ASIO SDK.
Do not download an arbitrary DLL or reuse the old installation's copy.

```powershell
$PaSha = '147dd722548358763a8b649b3e4b41dfffbcfbb6'
if (Test-Path '.tools\portaudio-src') { throw 'This first-run procedure expects a fresh dependency directory.' }
git clone https://github.com/PortAudio/portaudio.git .tools/portaudio-src
Check-Exit 'PortAudio source clone'
git -C .tools/portaudio-src checkout --detach $PaSha; Check-Exit 'Pin PortAudio'
$PaActual = (git -C .tools/portaudio-src rev-parse HEAD).Trim(); Check-Exit 'Resolve PortAudio SHA'
if ($PaActual -ne $PaSha) { throw 'Wrong PortAudio revision.' }
cmake -S .tools/portaudio-src -B .tools/portaudio-build -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' -DPA_BUILD_SHARED=ON -DPA_BUILD_STATIC=OFF -DPA_BUILD_TESTS=OFF -DPA_BUILD_EXAMPLES=OFF -DPA_USE_ASIO=OFF
Check-Exit 'PortAudio configure'
cmake --build .tools/portaudio-build --config Release --parallel 2
Check-Exit 'PortAudio build'
$PaDll = Join-Path $Root '.tools\portaudio-build\Release\portaudio_x64.dll'
if (-not (Test-Path $PaDll)) { throw 'Expected newly built x64 PortAudio DLL is missing.' }

if (Test-Path 'build\native\CMakeCache.txt') { throw 'Existing CMake cache: use a fresh checkout; do not reuse it.' }
$Python = (py -3.12 -c 'import sys; print(sys.executable)').Trim(); Check-Exit 'Find Python'
cmake -S native -B build/native -G 'Visual Studio 17 2022' -A x64 -DWIRESCAPE_BUILD_UI=OFF "-DPython3_EXECUTABLE=$Python"
Check-Exit 'Qt-free native configure'
cmake --build build/native --config Release --parallel 2
Check-Exit 'Native build'
$Engine = Join-Path $Root 'build\native\Release\wirescape-engine.exe'
if (-not (Test-Path $Engine)) { throw 'Engine missing from this build.' }
$RuntimeDll = Join-Path (Split-Path $Engine) 'libportaudio64bit.dll'
Copy-Item -LiteralPath $PaDll -Destination $RuntimeDll
if ((Get-FileHash $PaDll).Hash -ne (Get-FileHash $RuntimeDll).Hash) { throw 'DLL staging hash mismatch.' }
# PE machine check + loader check only; never call Pa_Initialize or open devices.
@'
import ctypes, struct, sys
with open(sys.argv[1], 'rb') as source:
    assert source.read(2) == b'MZ'
    source.seek(60)
    offset = struct.unpack('<I', source.read(4))[0]
    source.seek(offset)
    assert source.read(4) == b'PE\0\0'
    assert struct.unpack('<H', source.read(2))[0] == 0x8664
library = ctypes.WinDLL(sys.argv[1])
getattr(library, 'Pa_Initialize')
print('x64 PortAudio DLL load passed; no device initialization')
'@ | py -3.12 - $RuntimeDll
Check-Exit 'PortAudio architecture and load check'
@{ source=$ActualSha; portaudio_source=$PaSha; portaudio_license='MIT'; portaudio_sha256=(Get-FileHash $RuntimeDll).Hash; engine_sha256=(Get-FileHash $Engine).Hash } |
    ConvertTo-Json | Set-Content (Join-Path $Log 'provenance.json')
if (Get-ChildItem build -Recurse -Filter 'Qt6*.dll') { throw 'Unexpected Qt DLL in fresh build.' }
```

The loader check does not prove audio works. Preserve PortAudio's `LICENSE.txt`
in `.tools/portaudio-src` with its provenance. A distributable package must
include licences and all runtime dependencies separately under #436/W2.

## Test before opening the app

```powershell
ctest --test-dir build/native -C Release --output-on-failure --no-tests=error -R '^(abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests|sidecar_golden)$' --output-log "$Log\protocol-tests.log"
Check-Exit 'Windows sidecar suites'
# All three must execute and pass. Any skip, timeout or exit 3 blocks launch.
npm --prefix apps/studio ci; Check-Exit 'Studio dependency install'
npm --prefix apps/studio run build; Check-Exit 'Studio build'
npm --prefix apps/desktop ci; Check-Exit 'Desktop dependency install'
npm --prefix apps/desktop run build; Check-Exit 'Desktop build'
```

Do not count the existing `apps/desktop/tests/app-e2e.spec.cjs` as Windows
verification: it currently skips non-Linux hosts. A Windows Electron window,
UI assets, actual engine hello/command round trip and clean shutdown still need
an executed Windows result. The native suites above verify the actual engine
protocol separately, without devices. A green native test is insufficient to
certify the whole desktop app or installer.

## Isolated device-off launch (after the gates above)

This checkout-local wrapper explicitly isolates Electron `userData` (window
state and patches), `sessionData` (Chromium caches) and native settings.
`--settings` alone does not isolate Electron's profile. It never edits existing
app data. Keep the wrapper under `.tools`, outside tracked source.

```powershell
@'
// SPDX-License-Identifier: GPL-3.0-only
const {app} = require('electron');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
for (const [key, folder] of [['userData', 'fresh-profile'], ['sessionData', 'fresh-session']]) {
  const directory = path.join(root, '.tools', folder);
  fs.mkdirSync(directory, {recursive: true});
  app.setPath(key, directory);
  console.error(key + '=' + app.getPath(key));
}
require(path.join(root, 'apps', 'desktop', 'build', 'main.js'));
'@ | Set-Content -Encoding utf8 '.tools\fresh-launch.cjs'
$Electron = Join-Path $Root 'apps\desktop\node_modules\electron\dist\electron.exe'
& $Electron "$Root\.tools\fresh-launch.cjs" --engine $Engine --null-audio --settings "$Root\.tools\fresh-settings.json"
Check-Exit 'Device-off Electron launch and shutdown'
Stop-Transcript
```

Confirm the expected profile paths in the terminal, the normal Studio window,
`engine: real`, and `boot-to-first-hello-ms=`. Load a copy of a native example
patch, confirm a command is acknowledged with no refusal/stall, then quit and
confirm the engine terminates. Preserve the actual results and source SHA;
these steps are pending verification, not pre-recorded success.

Do not use `tools/windows/Wirescape.cmd` for this smoke: it uses `start` and
adds `--confirm-audio --confirm-midi`. Only after the device-off verification,
the owner can deliberately relaunch the same isolated wrapper, replace
`--null-audio` with `--confirm-audio --confirm-midi`, and test real sound/MIDI.
Agents never perform that device test. Groove's eventual two-track acceptance
is tracked in #432; S0 catalogue presence alone does not establish readiness.

## Failures, updates and recovery

- Missing compiler/SDK: install the missing VS component and reopen the correct
  developer shell. Wrong-architecture/missing DLL: rebuild from the pinned x64
  source and inspect the failing loader check; do not search old PATH entries.
- Exit 3 or a protocol timeout: stop, retain logs and report #266/#439. Never
  restore skips, raise the watchdog or continue to real devices.
- npm/FetchContent/network: record the failed URL and error, fix connectivity,
  then retry the failed phase. Keep lockfiles and verified source pins intact.
- Electron/GPU/Defender failure: retain the error; do not disable antivirus or
  the Electron sandbox as an install workaround.
- No global cleanup is needed. Closing PowerShell restores inherited process
  settings. Old shortcuts and the old app remain intact. Never delete user data
  or copy old binaries into the fresh directory as recovery.
- Do not `git pull` a detached accepted build. For a later update, select a new
  reviewed full SHA and a fresh directory/profile, then repeat build and test.
  This first-run runbook is not the idempotent automatic bootstrap requested
  in #436/W1, and it is not clean-machine installer certification (#436/W2).

Signed: Codex (Windows guide correction), 2026-11-08 America/New_York.
