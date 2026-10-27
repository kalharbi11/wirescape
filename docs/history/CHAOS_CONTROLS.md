# CHAOS UI mapping — beta 0.3

**Expanded performance panel:** 960 × 900 canvas pixels, 22 interactive front controls grouped into five functional sections. This is one node in the independent Wirescape patch graph, not a separate window or audio stream. Click **FOCUS** to center and zoom to the module; **OPEN ENGINE ↗** to open its full editor.

## Fix to beta 0.2 control path

The previous HTML template called `window.wirescapeBridge.control(...)` only when `button.dataset.ready === '1'`. That flag was never initialized, so front-panel gestures never reached Python. Beta 0.3 initializes `dataset.ready` after the control's visual state and issues WebSocket `control` messages on later user input. Knobs, keyboard controls, presets, modulation inputs, and deep edits remain in one module schema. Regression tests are in `tools/ui_smoke.py` and `tests/test_chaos_panel.py`.

## Curated parameter architecture

All performance values are 0–100 (normalized) in the patch file and UI. The host maps them to engineering units and the original CHAOS macro contract in `wirescape/mapping.py`. Python/Surge sources are unchanged.

| Section | Control | Macro(s) | Main behavior |
|---|---|---|---|
| CHARACTER | MORPH | A_LEVEL, B_LEVEL, A_MUTE, B_MUTE | Equal-power 2-generator crossfade; 0=A, 100=B |
| CHARACTER | CHAOS | JITTER, ROUGH_DEPTH, DRIFT | Grouped nonlinear instability (minimum values; see fine-tuning below) |
| CHARACTER | FOLD | BREAK, DRIVE_TYPE | Existing compound BREAK waveshaping; not isolated pure folding |
| CHARACTER | CUTOFF | CUTOFF | Surge filter 1 macro cutoff |
| GENERATORS | PITCH | OFFSET | Fine offset ±50 cents |
| GENERATORS | DETUNE | DETUNE, VOICES | 0–60 cents spread; 3-voice unison when on |
| GENERATORS | CLUSTER | CLUSTER | 1–5 adjacent notes per held key; lower-cost default 1 for Pi |
| GENERATORS | SUB | SUB | Classic oscillator subharmonic content |
| GENERATORS | FM | FM_RATIO | Offset oscillator 2 ratio, creating more inharmonic sidebands |
| INSTABILITY | ROUGH | ROUGH_DEPTH | Original host 5–150 Hz roughness AM depth; rate under MACROS |
| INSTABILITY | JITTER | JITTER | Pitch jitter + occasional frequency jumps |
| INSTABILITY | DRIFT | DRIFT | Native Surge oscillator drift |
| INSTABILITY | FALL | FALL | Continuous downwards pitch while holding notes |
| BODY | COUPLE | ROUTE_MODE, ROUTE_DEPTH | Ring/delay-bend interactions selected by scene |
| BODY | BODY | BODY_MIX | Wet amount of resonator body |
| BODY | TONE | BODY_TONE | Resonator damping tone |
| BODY | FEEDBACK | BODY_FEED | Resonator internal feedback up to 0.76 (original 0.85 limit) |
| MOTION | RATE | LFO_RATE | Logarithmic 0.05–8 Hz |
| MOTION | MOTION | LFO_DEPTH, LFO_SELF, LFO_TARGET | Internal LFO acts on cutoff only while active |
| MOTION | TAPE | TAPE | Tape age/saturation/wow |
| MOTION | SPACE | SPACE, SPACE_MIX | Reverb size + wet amount |
| MOTION | LOOP | LOOP_MIX, LOOP_FEEDBACK | Playback mix + loop memory feedback |

CHAOS acts as a global minimum for ROUGH/JITTER/DRIFT (those individual knobs can increase independently, not reduce below the CHAOS macro). COUPLE is independent to avoid routing an effect accidentally when the character scene changes. In the FM scene, CHAOS also raises FM_RATIO. If you want total independent control, use the advanced MACROS editor and/or raw Surge controls.

Modes **CLUSTER**, **FM**, **NOISE** are scenes of one engine, **not fundamentally new DSP algorithms**. The underlying voice, effects and saved presets remain from CHAOS beta 0.1.

## Access to all parameters

**OPEN ENGINE ↗ → MACROS** shows all original `SPEC` macros, including the four-slot internal modulation matrix, generator bus controls, tape/reverb, looper, resonant body, routing and master volume. **RAW A / RAW B** offer curated native Surge settings and the expandable full parameter index from the bundled snapshot. **PANEL** has sliders for each of the 30 exposed knobs and preset loading. **ROUTING** describes the topology.

Precedence is: RAW Surge overrides > touched front controls (for the macros they own) > untouched advanced macro overrides / loaded preset overrides > initial front control mapping. In practice:

1. Loading a preset saves its original macro settings as advanced overrides; the audible sound changes without forcibly reconstructing the front knobs.
2. Touching FOLD deletes only BREAK and DRIVE_TYPE advanced overrides, takes ownership of those macros and makes a live audible change. Existing unrelated preset parameters remain intact.
3. Changing an advanced macro in DEEP takes precedence until its matching front knob is touched. A raw Surge edit always takes precedence over matching macro-driven native Surge parameters until manually cleared.

Raw parameter names reflect the bundled `params.json` snapshot, not a live scan of installed Surge. A mismatch in Surge versions can make individual entries unavailable. Unedited RAW controls report NO OVERRIDE rather than a potentially incorrect value.

## Maintaining code correctness

`wirescape/schema.py:DEFINITIONS['chaos']['controls']` is the machine-readable set of exposed controls and defaults. `wirescape/mapping.py` owns the DSP macro mappings and `FRONT_OWNS` declaration; `web/index.html` declares their visual placement and knob gestures; `web/client.js:FRONT_OWNS` mirrors ownership for saved patch data. If you rename or add a control, change all of these and test with `python tools/verify.py`, plus `python tools/verify.py --ui` if a Playwright/Chromium environment is available.

The same Wirescape patch API and audio callback continues to support independent modulation/audio plugins and future data and visual subsystems. No per-module hardware stream was added.


### Beta 0.4 front envelope and preset additions

Both A and B now expose amplitude **ATTACK / DECAY / SUSTAIN / RELEASE** on the main instrument, in a dedicated sixth section. These map to the actual independent Surge engines through normalized RAW writes. RAW edits have precedence until cleared. The KEYS/SOLO KEYS buttons in CHAOS are *computer-keyboard targeting*, not audio muting. Sound presets are chosen and saved directly from the front; the complete patch save/load remains separate. Detailed operating and source contracts are in `docs/BETA_04_UPGRADE.md` and `docs/KEYBOARD_PRESETS_AND_TYPES.md`.
