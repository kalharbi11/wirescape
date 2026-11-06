# Six Essentials demo

Open Six_Essentials_Demo.json from Wirescape's Open button. Opening keeps audio off. Noise goes through VCF, Ladder, VCA and Output. Clock pulses drive Function Envelope, then Slew controls VCA gain. Clock starts running in the graph when it is rendered; it is not the host transport. Manual Hold Gate is not required for this patch. Adjust Noise Level, VCF/Ladder cutoff, Clock BPM and envelope Rise/Fall. Pan/zoom to explore the full graph. The VCF also exposes low12 if you want to replace the low24 connection.

This example intentionally uses all six imported modules plus the existing VCA and Output. It does not change user presets or default startup patches. The Function Envelope has a control-buffer gate input; the existing ADSR remains host-event gated. Slew is linear, not exponential lag. White noise is deterministic on reset, and the filters are original approximations rather than exact analog models.

## Class demos (native v2; devices off until the owner launches playback)

All use 48 kHz / 256 frames, four MIDI voices, and CC 21–28 on any channel
(`channel: 0` is omni). `devices: []` is filled locally by the Pi demo launcher.
Silence without MIDI is expected. The owner checks controller travel and levels.

| Patch | CC 21–28, in order |
|---|---|
| `class_demo_1_chaos.json` | A shape, B shape, body mix, roughness, LFO depth, cutoff, detune, mixer A level |
| `class_demo_2_subtractive.json` | Cutoff, resonance, LFO rate, LFO depth, attack, release, pitch, level |
| `class_demo_3_macro.json` | Model CV, harmonics, timbre, morph, cutoff, resonance, release, level |

The current VCO has no shape parameter: CC 27 sweeps its frequency multiplier
instead (lead-approved). Macro model uses its dedicated `model_cv` input because
its stepped model parameter is not modulatable. No module IDs or DSP are changed.

### Controller labels and full travel (CC value 0 → 127)

Other controllers start at zero. Positive amounts add normalized knob travel;
negative amounts sweep downward. Cutoff uses the VCF's logarithmic taper.
These patches leave all devices disabled; verify listening level before use.

CHAOS (`class_demo_1_chaos.json`):

| M8 | CC | Target | Range |
|---|---|---|---|
| CCA | 21 | First CHAOS A shape | 0.1 → 1.7 |
| CCB | 22 | Second CHAOS B shape | 1.8 → 0.2 |
| CCC | 23 | First CHAOS body mix | 0.1 → 0.9 |
| CCD | 24 | Second CHAOS roughness | 0.05 → 0.85 |
| CCE | 25 | First CHAOS pitch LFO depth | 0.05 → 0.75 |
| CCF | 26 | Second CHAOS cutoff | 13000 → 1012 Hz |
| CCG | 27 | First CHAOS B detune | −30 → +30 cents |
| CCH | 28 | Mixer first instrument level | 1.2 → 0.3 |

Subtractive (`class_demo_2_subtractive.json`):

| M8 | CC | Target | Range |
|---|---|---|---|
| CCA | 21 | VCF cutoff | 20 → 2518 Hz |
| CCB | 22 | VCF resonance | 0.3 → 0.87 |
| CCC | 23 | LFO rate | 0.6 → 10.595 Hz |
| CCD | 24 | LFO depth | 0.05 → 0.85 |
| CCE | 25 | ADSR attack | 0.01 → 1.51 s |
| CCF | 26 | ADSR release | 0.1 → 3.1 s |
| CCG | 27 | VCO frequency multiplier | 220 → 660 Hz (threefold pitch) |
| CCH | 28 | VCA gain | 0.1 → 1.0 |

The LFO also modulates cutoff, so its instantaneous cutoff moves around the
listed endpoints. CC27 remains active with the pitch cable connected: the VCO
multiplies the note frequency by its frequency parameter divided by 220.

Macro (`class_demo_3_macro.json`):

| M8 | CC | Target | Range |
|---|---|---|---|
| CCA | 21 | Model CV input | 0 → 1 (full model scan) |
| CCB | 22 | Harmonics | 0.1 → 0.9 |
| CCC | 23 | Timbre | 0.1 → 0.9 |
| CCD | 24 | Morph | 0.1 → 0.9 |
| CCE | 25 | VCF cutoff | 20 → 2518 Hz |
| CCF | 26 | VCF resonance | 0.3 → 0.87 |
| CCG | 27 | ADSR release | 0.1 → 3.1 s |
| CCH | 28 | VCA gain | 0.1 → 1.0 |

`class_demo_cc_depth` sends MIDI through `midi.in`, renders each endpoint for
0.5 s with a held note, and observes the effective DSP callback parameter (the
actual input for model CV). It requires at least 0.4 normalized travel. Musical
exceptions, approved by the task lead: linear 30 s envelope and 20 kHz frequency
ranges instead use the explicit endpoints above and a physical-value ratio ≥1.5;
0.4 travel would mean 12 s envelopes or an 8 kHz pitch increase. The regression
also requires ≥1.5 held-note RMS ratio for the subtractive and Macro cutoff
sweeps, renders 0.5 s after note-off, and requires changed audio, catching unused
controls and release behavior. Release is heard when a note ends; attack is
heard on a fresh note. Real M8 listening checks for all 24 mappings remain with
the owner in `docs/DEMO_CLASS.md`.
