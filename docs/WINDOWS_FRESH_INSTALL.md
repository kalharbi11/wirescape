<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows: fresh Electron build, with no old Qt install involved

Owner guide for #436 (source build; not an installer). Result: the new
Electron + Studio + `wirescape-engine` app built in a **new folder**, with old
Qt builds, shortcuts and environment variables out of the way. Nothing is
deleted: old folders are renamed so your patches and settings stay recoverable.

Use **PowerShell** (not as Administrator unless winget asks). About 20–40 min.

## 1. Put the old Qt install out of the way (nothing deleted)
1. Close any running Wirescape.
2. Rename your old checkout folder, e.g. `wirescape` → `wirescape-qt-old`.
   Its `user_data\`, saved patches and `.tools\portaudio\` stay inside it.
3. Move old Wirescape shortcuts (Desktop, Start Menu
   `%APPDATA%\Microsoft\Windows\Start Menu\Programs`) into a folder such as
   `Desktop\old-wirescape-shortcuts`.
4. Clear old environment variables and Qt PATH entries for this user:
   ```powershell
   foreach ($v in 'WIRESCAPE_QT_DIR','WIRESCAPE_PORTAUDIO','CMAKE_PREFIX_PATH') {
     [Environment]::SetEnvironmentVariable($v, $null, 'User') }
   $p = [Environment]::GetEnvironmentVariable('Path','User') -split ';' |
        Where-Object { $_ -and $_ -notmatch '\\Qt\\|wirescape' }
   [Environment]::SetEnvironmentVariable('Path', ($p -join ';'), 'User')
   ```
   Then **close and reopen PowerShell**.
5. Optional, for a truly first-run app profile: rename
   `%APPDATA%\wirescape-desktop` (if it exists) to `wirescape-desktop-old`.

## 2. Install the tools (once)
```powershell
winget install --id Git.Git -e
winget install --id Python.Python.3.12 -e
winget install --id OpenJS.NodeJS.LTS -e
winget install --id Kitware.CMake -e
winget install --id Ninja-build.Ninja -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```
Close and reopen PowerShell. **No Qt is needed.**

## 3. Get a fresh copy and build
```powershell
cd $HOME
git clone https://github.com/kalharbi11/wirescape.git wirescape-new
cd wirescape-new
python tools/dev.py doctor
python tools/dev.py build --headless --config Release
if ($LASTEXITCODE -ne 0) { throw 'Engine build failed' }
cd apps/studio;  npm ci; npm run build; if ($LASTEXITCODE) { throw 'Studio build failed' }
cd ../desktop;   npm ci; npm run build; if ($LASTEXITCODE) { throw 'Desktop build failed' }
cd ../..
```
`--headless` means "no Qt UI". Ignore any Qt notes printed by `doctor`.

## 4. Add the audio library (one file)
The engine loads `libportaudio64bit.dll` from **its own folder**. Reuse your
existing, known-good copy from the old checkout:
```powershell
Copy-Item ..\wirescape-qt-old\.tools\portaudio\libportaudio64bit.dll build\native\Release\
```
(If it lived elsewhere before, copy it from there. Don't download a new one
for this test.)

## 5. Check it is clean, then launch
```powershell
Test-Path build\native\Release\wirescape-engine.exe          # True
Get-ChildItem build -Recurse -Filter 'Qt6*.dll' | Measure-Object | % Count   # 0
.\tools\windows\Wirescape.cmd
```
The launcher starts Electron with real audio/MIDI enabled (owner only).
Patches you save go to `%APPDATA%\wirescape-desktop\patches`.

## Updating later
```powershell
cd $HOME\wirescape-new
git pull
python tools/dev.py build --headless --config Release
cd apps/studio; npm ci; npm run build; cd ../desktop; npm ci; npm run build; cd ../..
```

## If something fails
- `doctor`/build says no compiler or generator: reopen PowerShell after the
  Build Tools install, or use "Developer PowerShell for VS 2022".
- App opens but no sound / "PortAudio unavailable": step 4's DLL is missing
  from `build\native\Release\`.
- "Build wirescape-engine before launching": step 3's engine build did not finish.
- Paste the last ~30 lines of the error into issue #436.

Old setup back: rename `wirescape-qt-old` back and restore shortcuts.
