# WIRESCAPE 0.5 — replace the entire project folder on Windows

This source distribution includes the Wirescape 0.5 host, the latest integrated CHAOS source/front panel, the standalone CHORD / SEQUENCER module, example patches, the module SDK, and automated tests. It has one top-level `wirescape/` folder.

**Not bundled:** your Windows-compiled `surgepy*.pyd` / DLLs, your personal presets under `user_data/`, and patches you saved outside the project. Preserve those from the working installation. There is no need to recompile Surge if the old compatible binary works.

## Replace safely (PowerShell)

1. Stop the running application with Ctrl+C. If it is running in a different terminal, stop it there. Export important browser patches using EXPORT before changing anything.
2. In PowerShell, run:

```powershell
cd '${USERPROFILE}\Documents\<project-parent>'
Rename-Item -LiteralPath '.\wirescape' -NewName 'wirescape_backup'  # if you have not already made that backup
Expand-Archive -LiteralPath "$env:USERPROFILE\Downloads\WIRESCAPE_v0.5_COMPLETE_READY_TO_RUN.zip" -DestinationPath '.'
```

If the archive has another download name or location, replace only that name/path.

3. Transfer your locally compiled binary and user-created state:

```powershell
Copy-Item -LiteralPath '.\wirescape_backup\modules\chaos\surgepy_bin' -Destination '.\wirescape\modules\chaos\surgepy_bin' -Recurse -Force
if (Test-Path '.\wirescape_backup\user_data') { Copy-Item -LiteralPath '.\wirescape_backup\user_data' -Destination '.\wirescape\user_data' -Recurse -Force }
```

If `surgepy_bin` is missing from the backup but is available under your standalone `CHAOS` folder, use that instead:

```powershell
Copy-Item -LiteralPath '.\CHAOS\surgepy_bin' -Destination '.\wirescape\modules\chaos\surgepy_bin' -Recurse -Force
```

Optional: individually copy any third-party module directories you developed that are not already part of 0.5 (do not overwrite `chaos` or `sequencer` with older versions).

4. Start from the **new** folder:

```powershell
cd '.\wirescape'
.\setup_windows.bat
.\run_windows.bat
```

PowerShell needs the leading `.\` to run batch files from the current directory. Setup reuses installed Python dependencies and your compatible Surge binary and runs tests. It does not compile Surge. In the browser open `http://127.0.0.1:8765` if it does not open automatically. Refresh using Ctrl+Shift+R.

5. Check the SURGE / READY badge, open MODULES, scan and enable CHORD / SEQUENCER, and use `examples/03_chord_sequencer.wscape.json` if you want a test patch. Configure the audio device and start audio manually.

6. Optional native verification:

```powershell
py -3.12 tools\verify.py --real
```

### Notes

- The archive is source-only: your `.pyd` is tied to your Windows architecture/Python version and must be copied from the existing working installation.
- The older `wirescape_backup` remains a rollback option. Do not delete it until playback, presets and patches work as expected.
- The browser's localStorage auto-save may survive if the origin (`http://127.0.0.1:8765`) stays the same, but export JSON patches for a dependable backup.
- Third-party Python modules run trusted code. Only enable modules from sources you trust.
- Raspberry Pi requires an ARM64-compatible Surge binding. Windows `.pyd` will not work there.
