<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Quantizer (`essentials.quantizer`)
Snaps a pitch to the nearest note of a scale and says when the note changes. Pitch is 1.0 per octave, 0 = C4.
## Ports
| Kind | ID | Name | Notes |
|---|---|---|---|
| in | `pitch` | Pitch | poly; sets the voice count |
| in | `trigger` | Trigger | poly (a mono cable broadcasts). Connected: each voice samples the pitch on its rising edge (Schmitt 0.5/0.25) and holds. Unconnected: continuous |
| out | `pitch` | Pitch | quantized, poly. An allowed note is exactly `(midi - 60) / 12` |
| out | `changed` | Changed | 1 ms trigger (`ws::Trigger`) on the frame a voice's output note changes |
## Parameters (all stepped)
| ID | Name | Range | Default |
|---|---|---|---|
| `scale` | Scale | 0..13: Chromatic, Major, Natural minor, Harmonic minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Major pentatonic, Minor pentatonic, Blues, Whole tone, Custom | 1 |
| `root` | Root | 0..11 (C..B) | 0 |
| `shift` | Shift | -7..+7 scale steps, applied after quantizing | 0 |
| `octave` | Octave | -3..+3 | 0 |
| `n0`..`n11` | C..B | 0/1, Custom scale notes relative to C (root is ignored); used only when `scale` = Custom | major |
## Behaviour
- Nearest allowed note, ties go down. The held note stays until the input is 0.1 semitone past the midpoint to the next allowed note.
- Custom with no notes on passes Chromatic.
- `shift` walks scale degrees and wraps into the next octave (+1 on the top degree gives the root an octave up).
- The first sample of a voice (and the first after reset) takes a note without firing `changed`. With a trigger connected, a voice holds its first note until the first edge. Changing `scale`, `root`, `shift` or `octave` changes the output at the next sample (or next trigger edge) and fires `changed` if the note differs.
- No saved state: held notes reset on reset and load (`save_state` reports unsupported).
- CONSTANT pitch output when the pitch (and a connected trigger) is constant; `changed` is constant 0 when no pulse is running.
Tests: `abi2_quantizer_tests`.
