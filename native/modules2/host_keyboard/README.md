<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Keyboard

`host.keyboard` receives app keyboard notes through bounded D13 ingress. Only the
first instance in patch order receives host keys. Computer keys support chords;
auto-repeat is ignored and losing keyboard focus releases held keys.

Outputs: `pitch` (1/octave, 0=C4), `gate` (0/1), `velocity` (0..1), `notes`
(assigned-voice note events). Channels are voices, not stereo. Patch pitch into
VCO, gate into ADSR, and the resulting voice into Output to sum the voices.

| Parameter | Range | Default |
| --- | --- | --- |
| `voices` | 1..16 | 8 |
| `mode` | 0 rotate, 1 reuse, 2 reset, 3 mono | 0 |
| `octave` | -3..3 | 0 |

Changing voices or mode releases existing notes. Panic releases all voices.
Held notes are not serialized; saved parameters restore the keyboard setup.
