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
| `class_demo_2_subtractive.json` | Cutoff, resonance, LFO rate, LFO depth, attack, release, fine pitch, level |
| `class_demo_3_macro.json` | Model CV, harmonics, timbre, morph, cutoff, resonance, release, level |

The current VCO has no shape parameter: CC 27 uses a small frequency modulation
instead (lead-approved). Macro model uses its dedicated `model_cv` input because
its stepped model parameter is not modulatable. No module IDs or DSP are changed.
