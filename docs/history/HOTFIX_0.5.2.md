# Wirescape 0.5.2 — module manager and sequencer visibility fix

## Symptoms

- `MODULES ↻` appeared to do nothing.
- `CHORD / SEQUENCER` did not appear in the module browser even though `modules/sequencer/` was present.

## Cause

The manager shares a CSS slide-in panel, which is kept outside the viewport until the `visible` class is set. Version 0.5/0.5.1 set only `display:block` and never added that class. The sequencer was bundled but required enabling through the unreachable manager.

## Changes

1. `MODULES ↻` correctly toggles the panel's `visible` class; `CLOSE ×` hides it.
2. The trusted *bundled* sequencer ships enabled by default **only if** `user_data/enabled_modules.json` does not yet exist. Third-party modules remain opt-in, and existing enable/disable preferences are respected.
3. The module browser dynamically updates its count and scrolls when the list grows.
4. Failure loading one module's custom JS no longer blocks registration of the remaining modules' generic controls.
5. New automatic regression tests and a local module diagnostic script.

## Updating your Windows installation

Stop Wirescape with Ctrl+C in its PowerShell console. Back up your installation and exported patches.

From `<repo>` run:

```powershell
Expand-Archive -LiteralPath "$env:USERPROFILE\Downloads\WIRESCAPE_v0.5.2_MODULE_MANAGER_FIX.zip" -DestinationPath "." -Force
py -3.12 tools\check_modules.py
py -3.12 -m unittest discover -s tests -q
.\run_windows.bat
```

Refresh the browser with **Ctrl+Shift+R**. `CHORD / SEQUENCER` should appear in the left sidebar, and `MODULES ↻` should open a slide-in panel.

If the diagnostic says `NOT ACTIVE`, open `MODULES ↻`, choose `SCAN MODULES`, and select `ENABLE MODULE` on `CHORD / SEQUENCER`. An existing `user_data/enabled_modules.json` containing an empty list overrides the bundled default, deliberately.

Do **not** delete `modules/chaos/surgepy_bin`, `user_data`, or your saved patch/preset files. This update does not rebuild Surge, install Python packages, or alter the underlying synthesizer DSP.

## Verification

- 71 Python tests pass in the test environment, including live server/WebSocket discovery, enabled sequencer, and asset delivery using an emulated synth.
- The `tools/check_modules.py` diagnostic confirms discovery, trusted startup activation, and the UI class toggle.
- Node.js `--check` passes for the frontend script.
- Physical Windows audio, native Surge synth timing, Raspberry Pi performance, and an interactive Chromium walkthrough were **not verified** here.
