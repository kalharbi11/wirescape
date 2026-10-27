# WIRESCAPE beta 0.3 — verification and limitations

## Confirmed from code and automated checks (Linux sandbox)

- The beta 0.2 **knob interaction bug** was identified and fixed: the guard on the frontend control bridge was never set, so audio host parameter commands were not sent. It is now initialized after constructing the knob, and user movements send named values to Python.
- CHAOS has an expanded **960 × 900** performance face with **22 mapped controls**, five labeled sections, FOCUS and OPEN ENGINE actions, and the same two underlying Surge instances.
- Named performance controls map to real existing CHAOS macros and are clamped to their original bounds, including native Surge filtering, roughness, FM, resonant body, LFO, tape, loop and space processing.
- **Macro precedence repair**: after loading a preset or deep macro override, touching the associated front knob clears only conflicting advanced overrides in both browser-persisted patch state and graph DSP state.
- `PANEL`, `MACROS`, `RAW A`, `RAW B`, `ROUTING` navigation retained; all original macro and source-native raw parameters remain accessible via the existing editor.
- Browser patch migration shifts an adjacent Audio I/O node to the right if it would overlap the newly enlarged CHAOS module in a saved beta 0.2 patch.
- Existing graph, built-in GAIN/SHAPE processor, typed cables, input and output routes, save/load, and audio backend were preserved.
- Backend unit tests (including new mapping and priority tests), Python compileall and both JavaScript syntax checks passed.
- **A Chromium/Playwright UI smoke test passed using a mocked WebSocket**: 22 controls rendered, FOLD sent the correct host message, navigation worked, RAW B opened, migration worked and the browser emitted no runtime JavaScript errors. This is frontend verification, not native hardware audio verification.
- Mock DSP engine achieved low block rendering time; results are available in `build_reports/verify.json`.

## Not verified in this environment

- Real compiled Surge playback or **audible knob response** on the user's Windows machine (native `surgepy` missing here), despite correct control messages and mappings.
- Physical Windows audio device, ASIO/WASAPI drivers, sustained polyphonic load, note stealing and real-time underruns.
- Raspberry Pi 5 operation or CPU capacity at many voices; `.pyd` Windows binaries are unusable on ARM64.
- Surges compiled from a different source revision may have differently named parameters; the full 766-entry index is based on the bundled introspection snapshot, not fresh runtime enumeration.
- Sample-accurate modulation, audio-thread hard realtime allocation freedom, plugin unloading/reloading, data/video processing.

## User smoke test on Windows

1. Restart Wirescape from its existing `wirescape` folder: ` .\run_windows.bat `.
2. Ctrl+Shift+R refresh in browser; confirm **SURGE / READY**.
3. Connect CHAOS L/R into Audio I/O and start the output stream in AUDIO SETUP.
4. Play a held note. Change **CUTOFF**, **FOLD**, **MORPH** or **SPACE** using the new CHAOS face and listen for a clear effect. With FOLD especially, compare 0% and 100%.
5. Press **FOCUS** to center CHAOS; **OPEN ENGINE ↗** to view all macros and raw Surge controls.
6. Run `py -3.12 tools\verify.py --real` to render actual compiled Surge and report CPU budget.

**No C++ rebuild or Python package reinstallation is required for this beta 0.3 editor/control update.** Preserve `modules/chaos/surgepy_bin/` and your own patch/preset files when extracting the update.


## Beta 0.4 (keyboard, raw enums, dual ADSR, presets)

New live local-host features: selection-independent multi-CHAOS keyboard arming/solo targeting; keyboard audition while range sliders are focused in the deep editor; 30-knob front with actual native Surge A/B ADSR mappings; discrete selector editors for oscillator/filter/LFO types, bools, and macro enums; sidecar preset storage; and minimum-active CLUSTER/FM/NOISE scene behavior. Optional native type-name scanner works with the user's compiled Surge binding.

Automated verification expanded to Python DSP/graph/preset tests, strict websocket save/load handling, Node.js syntax checks, and Chromium interaction tests. Native Surge audio, Windows WASAPI/ASIO device behavior, Pi ARM64 builds, and realtime polyphony headroom are **not confirmed here**.

## Beta 0.5 module SDK status

- **Implemented:** JSON-only folder discovery; explicit Python plugin enable; persisted allowlist; dynamic schema/JS/CSS loading; generic state editing; typed note-event routing to CHAOS; sequencer and example integration tests; no per-module host source modifications.
- **Verified in sandbox:** mock graph tests, existing 0.4 regression suite, sequencer pattern/notes/state/disconnect tests, browser mock workflow for custom module editor, JS parse checks. See `build_reports/verify.json`.
- **Not verified:** actual Surge on user Windows (only their local compiled `.pyd`), audio hardware XRUN behavior with stacked voices/effects, real Pi 5 ARM64 playback, browser connecting to localhost in this sandbox (browser policy blocked that navigation), safe execution of arbitrary third-party plugins (Python plugins are trusted code).
- **Known limits:** callbacks use Python and allocate; sequencer events quantized to block boundaries; no hot-reload of already imported modules; only audio/mod/gate/clock/notes runtime APIs today; legacy built-ins still have host adapters.

## Beta 0.5.2 — module manager visibility repair

- Repaired invisible module manager (missing `visible` CSS state); updated close button and accessibility state.
- Bundled CHORD / SEQUENCER now appears in the module browser on a fresh installation; third-party modules are still explicit opt-in.
- Module library list count and overflow corrected. Module UI asset failures no longer abort all frontend schema registration.
- 71 Python automated tests (including manager regression and live aiohttp/WebSocket checks) pass; Node.js syntax check passes. Interactive browser verification blocked by environment policy, physical Windows/Pi hardware untested.
