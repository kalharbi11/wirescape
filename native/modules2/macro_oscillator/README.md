<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Macro Oscillator

Module `ports.macro_oscillator`: 24 selectable synthesis models, main and auxiliary
outputs. **48 kHz only**; preparation rejects every other sample rate. Polyphony is
capped at eight active channels; channels above eight are silent. Main/aux are two
separate audio streams, not a left/right stereo pair.

| Input | Mapping |
|---|---|
| `pitch` | C4-relative 1/oct → upstream semitone offset ×12 |
| `model_cv` | −1..1 → upstream engine modulation |
| `fm` | −1..1 nominal → upstream frequency modulation ×6 |
| `harmonics_cv`, `timbre_cv`, `morph_cv` | −1..1 → upstream modulation fields |
| `trigger`, `level` | 0..1, preserving connected/unconnected semantics |

All inputs are poly signals. No note event input is advertised; patch a note-to-CV
source to pitch/trigger/level. Model selection is stepped 0..23, coarse −48..48 st;
harmonics/timbre/morph/decay/LPG colour are 0..1; three modulation amounts −1..1.
Stable IDs and parameter slot indices are in `module.json`.

Controls are sampled on an internal 12-frame quantum. A per-channel FIFO makes host
block divisions 1/12/17/128/256 equivalent; `latency_frames()` reports the fixed
12-frame delay. Trigger transitions shorter than a control quantum may be missed.
Outputs are normalized from upstream signed 16-bit values by division by 32768.

State v1 is patch state plus deterministic reset: parameters belong to the host;
upstream workspaces and raw C++ objects are never serialized. Restoring state
restarts the sound. Custom flash/UserData banks are not supported; all 24 models
use factory resources. No hardware flash or firmware code is run.

Macro Oscillator: DSP port of Mutable Instruments Plaits by Emilie Gillet,
`pichenettes/eurorack` commit `f4cfa7697095a28d292bff758beb51deb49b5a86`;
stmlib commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.
Original code under MIT; Wirescape adapter under GPL-3.0-only. Complete upstream
copyright/MIT notices stay in the fetched sources. This module does not use upstream
branding as its product name. No Audible Instruments code or graphics are used.

The cost probe is `abi2_macro_oscillator_tests --benchmark`; it opens no device.
