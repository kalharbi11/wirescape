# Wirescape Beta 0.4 — CHAOS usability upgrade

This is a **drop-in upgrade for beta 0.3**. No native Surge XT source/build changes. The original `surgepy_bin/` is **not included** in the ZIP and is not overwritten.

## Install over the current Windows folder

1. Export valuable Wirescape **patches** using `EXPORT ↓`; keep a copy of your `user_data/` folder if it exists.
2. Stop the existing host in PowerShell with **Ctrl+C**.
3. In PowerShell from `<repo>`, run:

   ```powershell
   Expand-Archive -LiteralPath "$env:USERPROFILE\Downloads\WIRESCAPE_beta_0.4_DROP_IN_UPDATE.zip" -DestinationPath "." -Force
   .\run_windows.bat
   ```

4. In your browser press **Ctrl+Shift+R** to reload the updated JavaScript and CSS. All your existing saved patch coordinates, cables, macros and raw overrides remain represented in the patch document.
5. Check that `SURGE / READY` appears; start audio normally. On first start, test with volume low.

## Changes

### Computer keyboard while the deep editor is open

Keyboard input is no longer tied to whichever module you last clicked or to the canvas being in focus. A range slider in RAW or MACROS does **not** block A/W/S/E/D... note keys. Typing in a text field **does** block note keys, intentionally. A raw parameter dropdown still allows note keys; use the arrow keys to navigate its choices.

Each CHAOS instance has **KEYS** (keyboard-arm) and **SOLO KEYS** (temporarily targets this one synth). Multiple instances can have KEYS enabled at once. SOLO KEYS routes new computer-keyboard notes only to the soloed synth; it does not mute or disconnect other modules' audio, nor their external gate signals. Note-off messages always return to the instances that received the original note-on, including when keyboard routing changes mid-note.

### CHAOS front panel and deep editor

* The **FOCUS** button was removed. **Double-click a module's non-control surface/header** to focus it. **Fit Patch** still fits the workspace.
* There are **30 front controls**: 22 synthesis/FX/instability controls plus two independent 4-knob **AMP ADSR** envelopes (A and B). B only becomes audible when its generator is mixed in via MORPH or FM scene.
* The **CLUSTER/FM/NOISE** selector changes actual synthesis state even if the related depth knobs are set to zero: CLUSTER triggers multiple neighboring notes; FM establishes nonzero cross-modulation and ensures B is present; NOISE activates Surge's noise source. These are **parameter/routing scenes**, not independent custom DSP synthesis engines.
* Tooltips describe the less familiar concepts, especially MORPH, CHAOS, FOLD, CLUSTER, COUPLE, BODY and FEEDBACK, plus ADSR.
* RAW OSC/FILTER/LFO **Type/Mode/Configuration** parameters are **selectors**, not smooth sliders. Discrete booleans and small integer parameters are also selectors, including macros DRIVE TYPE, ROUTE MODE, and modulation source/destination. Other normalized parameters remain sliders. `NO OVERRIDE` means the editor is not forcing a value on Surge.

### Named options on the specific local Surge build

The bundled `params.json` knows numerical ranges and default labels but **not every label for every enum value**. So some selectors initially show labels such as `5 · Type 5` rather than inventing a filter name that might be wrong on your compiled Surge revision. To populate their actual names, with the local Surge module already present, **stop the Wirescape host** and run:

```powershell
py -3.12 tools\scan_surge_choices.py
.\run_windows.bat
```

The scanner starts a *temporary separate* Surge instance, queries its native parameter display names, and writes `user_data/surge_enum_labels.json`. It **does not rebuild Surge**. This file is local to your installation. Rerun it after changing the compiled Surge revision. If it fails for an unusual parameter, its numeric choice is still available.

### CHAOS presets

Select a factory/user sound in the CHAOS front-panel **PRESETS** menu, then **LOAD**. **SAVE AS** stores the current CHAOS controls, mode, deep macros, and raw overrides as a user preset. The files are in `user_data/chaos_presets/` (outside the replaceable plugin source). Three factory starter scenes were added: Cracked Choir, Steel Interference, and Dust Field, alongside the five original legacy presets.

A **preset** is a single CHAOS sound. A Wirescape **patch** stores the whole workspace, nodes and cables; these are separate files/actions.

## Regression testing

```powershell
py -3.12 tools\verify.py
py -3.12 tools\verify.py --real
```

The first command checks the graph, preset validation, parameter mapping, raw enums, and JavaScript syntax without requiring an audio interface. The second also renders through your **actual** native Surge build. It cannot prove a click-free live hardware session: audition several simultaneous notes and effects through your audio device before considering the changes stable.

## Raspberry Pi 5

No changes were made to the PortAudio callback device model, sample rate, or Surge C++ source. ADSR updates are delivered in the existing control queue and only on changes; not every sample. The 3 new scenes may increase native polyphony and CPU load. Pi 5 performance and real audio compatibility require an ARM64 Surge binding and on-device verification.
