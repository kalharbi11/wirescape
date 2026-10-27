> **Current standalone build / reviewer entry:** [START_HERE_CURRENT_BUILD.md](START_HERE_CURRENT_BUILD.md). The original README below describes the preserved legacy app; use the current handoff for build commands, architecture, evidence and plans.

> **0.5.2 module manager fix:** See [HOTFIX_0.5.2.md](HOTFIX_0.5.2.md). `CHORD / SEQUENCER` now appears on clean installs; `MODULES ↻` opens its management panel. Run `py -3.12 tools\check_modules.py` for a local diagnostic.

# WIRESCAPE — an open patching environment

**Beta 0.5 — independent module discovery, CHORD / SEQUENCER, and CHAOS Beta 0.4 controls**

For replacing an existing Windows installation with the complete source folder, read **`INSTALL_WINDOWS_REPLACE.md`** first.

Wirescape is a spatial graph editor and local Python execution host for audio modules. Its long-term remit is wider than audio: data, logic and visual processors can eventually share the same workspace. In beta 0.4, **only the AUDIO, MOD, GATE and CLOCK domains are executable**. The original Studio HTML file is the design foundation; no replacement skin was invented.

## Start on Windows (your existing CHAOS build)

Wirescape is the main folder. It can contain the compiled Surge binding under `modules/chaos/surgepy_bin/` or reuse the adjacent standalone CHAOS build:

```text
${USERPROFILE}\Documents\<project-parent>\
    CHAOS\
        surgepy_bin\surgepy*.pyd
    wirescape\
        setup_windows.bat
        run_windows.bat
        wirescape\
        modules\chaos\
        web\
```

1. Run **`setup_windows.bat` once**. This installs the small web/audio host dependencies into **Python 3.12** and runs offline tests. **It does not clone or compile Surge**. Existing pip packages are reused when present.
2. Run **`run_windows.bat`**. Wirescape opens in your browser at `http://127.0.0.1:8765`.
3. Check the badge: `SURGE / READY` means the existing compatible compiled Python binding was found. `SURGE / MISSING` means the editor works but CHAOS audio is silent. Hover the badge for the error.
4. Choose **AUDIO SETUP → output device → START / STOP AUDIO**. If you want physical sound-card inputs, check the duplex input option before starting.
5. Hold **HOLD TO PLAY C** or use the keyboard keys **A W S E D F T G Y H U J K** (chromatic octave). **PANIC** silences held notes. Select a CHAOS instance before playing to target it; otherwise the first CHAOS instance is used.
6. Drag modules by the header; drag a cable from an output to a compatible input. Drag empty canvas to pan, scroll to zoom, select a cable and press Delete to remove it.
7. CHAOS is now a **960 × 900** canvas module. Use **FOCUS** on its face to zoom to it; use the canvas **Fit Patch** button to return. Click **OPEN ENGINE ↗** directly on CHAOS (or select CHAOS and click **DEEP EDIT**) to open **PANEL**, **MACROS**, **RAW A**, **RAW B** and **ROUTING**. All original parameters remain available.
8. Use **EXPORT** to save a JSON patch and **IMPORT** to load it. Auto-save uses browser localStorage for convenience, but export JSON for portable backups.

### If Surge is installed elsewhere

In `run_windows.bat`, uncomment the line `set "SURGEPY_DIR=..."` and set it to the **directory** containing `surgepy*.pyd`. The Python version and bitness **must match** the compiled module. If you moved the compiled files without their runtime dependencies, rebuild or copy all required runtime files from the original working CHAOS setup. Do **not** point it at a `.dll` or at the Surge source directory.

The original `CHAOS` source is included under `modules/chaos`, but its heavy `surge/` checkout and binary build results **are intentionally excluded**. Wirescape does not depend on GUTTR, GUTTUR, MERGE, Node.js or a throat engine.

## Raspberry Pi 5 (future target)

Run `./setup_pi5.sh`, then compile the ARM64 Surge Python binding using the included `modules/chaos/build_pi5.sh` instructions, or set `SURGEPY_DIR` to a compatible Pi `.so`. Run `./run_pi5.sh` to start the host. **Windows `.pyd` binaries cannot run on the Pi.** Pi audio I/O and multi-voice performance were **not tested** in this build. Expect to benchmark and possibly port the callback-side DSP to compiled code for production-quality low latency. Raspberry Pi OS, 64-bit, is strongly recommended.

## Try the patches

- `examples/01_chaos_mod_io.wscape.json`: Modulator → CHAOS (two modulation cables); CHAOS L/R → Audio I/O. Matches the uploaded visual layout.
- `examples/02_chaos_gain_io.wscape.json`: Modulator → CHAOS → GAIN / SHAPE → Audio I/O. Demonstrates an installable stereo processor and real multihop audio routing.

Four built-in module kinds are available: **CHAOS** (dual-Surge synthesis), **MODULATOR** (block-rate evolving control source), **AUDIO I/O** (computer interface and stereo mix destination), and **GAIN / SHAPE** (real stereo insert processor and module-plugin example).

## CHAOS front panel: 22 controls, five sound-design sections

The beta 0.2 front-panel knobs did not send control messages because `button.dataset.ready` was never set. **Beta 0.3 fixes the real cause**: a knob updates its actual audio-graph control in response to a mouse drag or keyboard arrow/Home/End gesture. The native Surge engine and your already compiled `.pyd` are unchanged.

| Section | Performance controls | Macro meanings |
|---|---|---|
| **01 / Character** | MORPH, CHAOS, FOLD, CUTOFF | A/B blend, aggregate instability, BREAK-based waveshaping and filter cutoff. |
| **02 / Generators + tuning** | PITCH, DETUNE, CLUSTER, SUB, FM | Fine cents, unison spread, 1–5 adjacent notes, subharmonic and inharmonic FM-ratio mapping. |
| **03 / Instability** | ROUGH, JITTER, DRIFT, FALL | Rough AM, random pitch deviations, Surge oscillator drift and held-note pitch fall. |
| **04 / Coupling + resonance** | COUPLE, BODY, TONE, FEEDBACK | A/B bus interactions, resonant comb wet level, damping and stable feedback. |
| **05 / Motion + space** | RATE, MOTION, TAPE, SPACE, LOOP | Internal LFO to cutoff, tape degradation, reverb and time-loop mixing. |

The buttons **CLUSTER**, **FM**, **NOISE** are **routing/synthesis scenes** of the existing sound engine—not claimed equivalents of Forge TME algorithms. They change how shared processors and internal sources are used. Some controls are context-dependent: **COUPLE** determines bus interaction intensity and uses the selected mode; BODY/TONE/FEEDBACK are most apparent when BODY is up; MOTION changes the cutoff over time; LOOP blends the current loop with live sound.

### Depth and parameter precedence

- On the CHAOS face, **OPEN ENGINE ↗** opens **MACROS** immediately. Use the **PANEL** tab to edit the 22 exposed controls with sliders and load the original presets.
- **MACROS** exposes *every original parameter* in `modules/chaos/macros.py:SPEC`, including research macros, generator levels and pans, tape/reverb, loop speed/feedback, LFO settings, the four-slot internal modulation matrix and resonant routing.
- **RAW A / RAW B** contains the curated original Surge oscillator/filter/envelope/LFO controls **plus** expandable **766-entry raw parameter indexes** for each Surge engine based on `modules/chaos/params.json`.
- **ROUTING** explains the A/B → bus interaction → resonant body path and links to the original detailed routing controls.
- Advanced MACROS overrides and original presets retain priority until you **touch the related performance knob**. Touching, for example, FOLD automatically clears only the `BREAK`/`DRIVE_TYPE` macro overrides so **that knob works again**, without clearing other advanced edits.
- An explicit **RAW** Surge edit retains final priority over matching macro-controlled Surge parameters. Clear the raw override in RAW A/B to return that individual synth parameter to macro ownership.

The full 766-entry indexes are **editing controls, not measured live readbacks**. Unedited entries are labelled UNSET and their actual initial value may not be the slider midpoint. Parameter names depend on the built Surge revision. This beta does not expose a graphical per-oscillator wavetable editor; the raw parameters remain accessible by name. The original standalone CHAOS is left unchanged.

### Updating an existing beta 0.2 installation without recompiling Surge

Stop Wirescape with **Ctrl+C**. Keep your existing `modules/chaos/surgepy_bin/` directory, patches, and standalone CHAOS build. Download the **latest drop-in update** and extract its contents **into your existing `wirescape` project root**, allowing it to replace matching files (not into another nested folder). The update modifies only Wirescape host/editor files, docs and tests; it does **not** contain or overwrite `surgepy_bin`, Surge C++ sources, your personal patches or presets.

Then run `./run_windows.bat` in PowerShell as ` .\run_windows.bat ` and refresh the browser with **Ctrl+Shift+R**. Your locally stored patches should be restored; beta 0.2 patches whose Audio I/O module was positioned immediately to the right of the old smaller CHAOS panel are shifted right during migration to avoid panel overlap. Export your favorite patch JSON before upgrading as a portable backup.

## Verification and performance

```bat
py -3.12 tools\verify.py
py -3.12 tools\verify.py --real
REM Optional GUI test if Playwright/Chromium are installed:
py -3.12 tools\verify.py --ui
```

```bash
python tools/verify.py
python tools/verify.py --real
# Optional browser regression test: python tools/verify.py --ui
```

The command writes `build_reports/verify.json` with individual test and timing results. The regular tests do not need Surge or an audio interface and exercise typed ports, cycle rejection, routing, independent voices, polyphony events, the real GAIN plugin, modulation, advanced parameters, host WebSocket, and output safety. The `--real` option is an **actual compiled Surge four-note render performance test**; use it on both Windows and Pi 5 to establish a measured budget.

Audio currently uses **one PortAudio `sounddevice` stereo stream**, fixed at **48 kHz / 512 frames** (10.67 ms callback budget). An optional full-duplex stream adds two physical hardware inputs. All CHAOS/polyphonic engines and future effects are summed through the same graph into the one I/O output; gain smoothing and final soft limiting reduce clicks/overload. Control messages travel over WebSockets, **audio does not**. The UI polls metering and render load about four times a second.

**Performance caveat:** This is a Python audio host. The original CHAOS engine applies some block-level parameters from Python, and `ResonantBody` uses NumPy/SciPy. A passed mock benchmark does **not** demonstrate that many simultaneous Surge voices are glitch-free at low latency, particularly on a Raspberry Pi 5. Measure actual buffer underruns (`XRUN`) and run the `--real` benchmark on your physical systems. See `docs/AUDIO_AND_RT.md`.

## Architecture and contributors

- `docs/ARCHITECTURE.md`: graph lifetime, message boundaries and project layout.
- `docs/MODULE_API.md`: **exact contract** for a new module—manifest, typed ports, block processing, generic UI and tests.
- `docs/CHAOS_CONTROLS.md`: mapping and full-parameter escalation.
- `docs/AUDIO_AND_RT.md`: realtime implementation limitations, device selection, CPU budget and Pi roadmap.
- `BUILD_STATUS.md`: verified features versus unverified hardware behavior.

The source is GPL-3.0; consult `LICENSE`. **The ABC Areal font files supplied with the visual prototype are not bundled or redistributed**. The HTML requests a locally installed font and otherwise falls back to system sans-serif. Do not add font binaries to the repository without reviewing their separate license.

## Technical dependencies

Python 3.12 (Windows, for binary ABI compatibility), NumPy, SciPy, aiohttp, sounddevice, soundfile and your already compiled Surge Python binding (`surgepy`). The patch editor itself requires only a modern browser served from the local host. Neither Electron nor Qt is required for this beta.


## Beta 0.4: updated CHAOS workflow

See [`docs/BETA_04_UPGRADE.md`](docs/BETA_04_UPGRADE.md) for Windows drop-in instructions and [`docs/KEYBOARD_PRESETS_AND_TYPES.md`](docs/KEYBOARD_PRESETS_AND_TYPES.md) for implementation details.

The CHAOS plugin exposes 30 front controls (including **ADSR for both independent Surge generators**), a keyboard **KEYS / SOLO KEYS** targeting system, descriptive parameter tooltips, **LOAD / SAVE AS** sound presets, and discrete dropdown selectors for native Surge oscillator/filter types. Double-click any module's non-control surface to focus it. The deep editor remains bound to the same CHAOS instance while you play from the computer keyboard.

Existing five CHAOS presets remain available, plus three additional starting scenes. Your own presets are saved outside the replaceable source tree in `user_data/chaos_presets/`. Native Surge binaries are not shipped in this source ZIP; reuse the compatible compiled `surgepy` binding under `modules/chaos/surgepy_bin/`.

To discover the *exact* local Surge parameter type names, stop Wirescape and run `py -3.12 tools\scan_surge_choices.py`, then restart. Native enum names differ between Surge revisions and are intentionally not guessed. This command does not compile Surge.

Automated tests use `py -3.12 tools\verify.py` and `py -3.12 tools\verify.py --real`. Audio and Pi 5 realtime performance have not been verified by this build environment.

---

## Beta 0.5: drop-in folder modules

Wirescape 0.5 adds an opt-in **MODULES ↻ / SCAN / ENABLE** workflow for modules stored in `modules/<id>/` with `module.json` and `plugin.py`. New API v1 modules register without changing host Python or global frontend files. Explicit ENABLE executes locally installed Python and requires the audio engine to be stopped; already loaded code requires a restart to update. See `docs/MODULE_API.md` and the bundled `module_build_guide/` folder (also available as a separate ZIP).

The **CHORD / SEQUENCER** is the first full API v1 module. It connects typed polyphonic note events to CHAOS NOTES IN, uses a generic plugin state channel, and owns its own CSS/JS. The previous 0.4 CHAOS front panel, deep editor, keyboard arming/solo, and preset system remain in place. The sequencer provided by the other agent was based on an older host snapshot; do **not** overlay its modified host files onto v0.4.

Fresh install: first run `setup_windows.bat` or `setup_pi5.sh`; launch with `run_windows.bat` or `run_pi5.sh`. Existing v0.4 users install the 0.5 drop-in update by merging its folders, preserving `modules/chaos/surgepy_bin` and `user_data`. Open Wirescape, click MODULES ↻, SCAN, then ENABLE the sequencer. For a new module from another developer, copy only its folder under `modules/` and use the same workflow. Run `py -3.12 tools\\verify.py` on Windows or `python3 tools/verify.py` on Pi. Actual compiled Surge playback and Pi timing still require hardware validation.
